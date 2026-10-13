#include "video_processing.hpp"
#include "video_filter_glsl.hpp"
#include "main.hpp"
#include "modconfig.hpp"
#include "UnityEngine/GL.hpp"
#include "UnityEngine/Graphics.hpp"
#include "UnityEngine/SystemInfo.hpp"
#include "UnityEngine/Rendering/GraphicsDeviceType.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/RenderTextureFormat.hpp"
#include "UnityEngine/RenderTextureReadWrite.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "System/IntPtr.hpp"
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <atomic>
#include <array>
#include <mutex>
#include <unordered_map>
#include <algorithm>
namespace theater {
namespace {
using namespace UnityEngine;
struct RenderJob {
    GLuint source{}, target{},program{},framebuffer{},vao{};
    GLint sampler{},colorA{},colorB{},oval{};
    int width{},height{};
    VideoConfig::ColorCorrection color;
    VideoConfig::Vignette vignette;
    EGLContext context{};
    std::atomic<bool> failed{false},ready{false};
};
std::mutex jobsMutex;
std::unordered_map<int,std::shared_ptr<RenderJob>> jobs;
std::atomic<int> nextID{1};
struct GLState {
    GLint program{},vao{},draw{},read{},active{},texture0{},sampler0{},viewport[4]{};
    GLboolean colorMask[4]{};
    std::array<GLenum,6> flags{GL_BLEND,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_RASTERIZER_DISCARD};
    std::array<GLboolean,6> enabled{};
    GLState() {
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
        glGetIntegerv(GL_VIEWPORT,viewport);glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture0);glGetIntegerv(GL_SAMPLER_BINDING,&sampler0);
        for(size_t i=0;i<flags.size();i++){enabled[i]=glIsEnabled(flags[i]);glDisable(flags[i]);}
    }
    ~GLState() {
        glUseProgram(program);glBindVertexArray(vao);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture0);glBindSampler(0,sampler0);glActiveTexture(active);
        for(size_t i=0;i<flags.size();i++)if(enabled[i])glEnable(flags[i]);else glDisable(flags[i]);
    }
};
GLuint Compile(GLenum type,const char* text) {
    auto shader=glCreateShader(type);glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
    GLint okay{};glGetShaderiv(shader,GL_COMPILE_STATUS,&okay);
    if(!okay){char log[2048]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);glDeleteShader(shader);throw std::runtime_error(std::string("Video filter shader: ")+log);}
    return shader;
}
void Release(RenderJob& job) {
    if(job.context!=eglGetCurrentContext())return;
    if(job.program)glDeleteProgram(job.program);if(job.vao)glDeleteVertexArrays(1,&job.vao);if(job.framebuffer)glDeleteFramebuffers(1,&job.framebuffer);
    job.program=job.vao=job.framebuffer=0;
}
void RenderEvent(int eventID) {
    std::shared_ptr<RenderJob> job;
    {std::lock_guard guard(jobsMutex);auto found=jobs.find(std::abs(eventID));if(found==jobs.end())return;job=found->second;if(eventID<0)jobs.erase(found);}
    if(eventID<0){Release(*job);return;}
    if(job->failed)return;
    try {
        if(eglGetCurrentContext()==EGL_NO_CONTEXT)throw std::runtime_error("Video filter has no render context");
        if(job->context&&job->context!=eglGetCurrentContext())throw std::runtime_error("Video filter render context changed");
        GLState saved;
        if(!job->program) {
            job->context=eglGetCurrentContext();
            GLuint vertex=Compile(GL_VERTEX_SHADER,VideoFilterVertex),fragment{};
            try {fragment=Compile(GL_FRAGMENT_SHADER,VideoFilterFragment);}catch(...){glDeleteShader(vertex);throw;}
            job->program=glCreateProgram();glAttachShader(job->program,vertex);glAttachShader(job->program,fragment);glLinkProgram(job->program);
            glDeleteShader(vertex);glDeleteShader(fragment);
            GLint okay{};glGetProgramiv(job->program,GL_LINK_STATUS,&okay);if(!okay)throw std::runtime_error("Video filter shader link failed");
            job->sampler=glGetUniformLocation(job->program,"video");job->colorA=glGetUniformLocation(job->program,"colorA");
            job->colorB=glGetUniformLocation(job->program,"colorB");job->oval=glGetUniformLocation(job->program,"oval");
            glGenVertexArrays(1,&job->vao);glGenFramebuffers(1,&job->framebuffer);
        }
        if(!glIsTexture(job->source)||!glIsTexture(job->target))throw std::runtime_error("Video filter texture unavailable");
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,job->framebuffer);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,job->target,0);
        if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Video filter framebuffer incomplete");
        glViewport(0,0,job->width,job->height);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glUseProgram(job->program);glBindVertexArray(job->vao);glBindTexture(GL_TEXTURE_2D,job->source);glBindSampler(0,0);
        glUniform1i(job->sampler,0);auto c=job->color;auto v=job->vignette;
        glUniform4f(job->colorA,c.brightness,c.contrast,c.saturation,c.hue/360);
        glUniform4f(job->colorB,c.exposure,c.gamma,v.radius,v.softness);glUniform1i(job->oval,v.elliptical);
        glDrawArrays(GL_TRIANGLES,0,3);
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Video filter draw failed");
        if(!job->ready)PaperLogger.info("Video filter ready: {}x{}, GLSL ES 3.0",job->width,job->height);
        job->ready=true;
    }catch(const std::exception& error){job->failed=true;PaperLogger.error("{}",error.what());}
}
void Issue(int id){GL::IssuePluginEvent(System::IntPtr::op_Explicit___System__IntPtr(static_cast<int64_t>(reinterpret_cast<intptr_t>(&RenderEvent))),id);}
bool BlendProperties(Material* material,std::string& source,std::string& destination) {
    for(auto keys:{std::pair{"_SrcColor","_DestColor"},std::pair{"_SrcBlend","_DstBlend"},std::pair{"_BlendSrcFactor","_BlendDstFactor"}})
        if(material->HasProperty(keys.first)&&material->HasProperty(keys.second)){source=keys.first;destination=keys.second;return true;}
    return false;
}
}
UnityEngine::Material* CreateVideoMaterial(const VideoConfig& config,bool menu) {
    bool blend=menu||config.colorBlending.value_or(getModConfig().ColorBlendingEnabled.GetValue());
    std::vector<Shader*> candidates;
    // Load the stock particle material first: its unlit shader supports dynamic
    // source/destination blend factors and includes platform/stereo variants.
    for(auto name:{"Default-ParticleSystem.mat","Default-Particle.mat"})try {
        if(auto material=Resources::GetBuiltinResource<Material*>(name)) {
            auto shader=material->get_shader();if(shader)candidates.push_back(shader.ptr());
        }
    }catch(const std::exception&) {}
    for(auto shader:Resources::FindObjectsOfTypeAll<Shader*>()) {
        auto name=static_cast<std::string>(shader->get_name());
        if(name.find("Unlit")!=std::string::npos||name.find("Video")!=std::string::npos)candidates.push_back(shader);
    }
    if(auto shader=Shader::Find("Particles/Standard Unlit"))candidates.push_back(shader.ptr());
    for(auto shader:candidates) {
        auto material=Material::New_ctor(shader);std::string source,destination;
        if(!material->HasProperty("_MainTex")||!BlendProperties(material,source,destination)){Object::Destroy(material);continue;}
        material->SetInt(source,blend?4:1);material->SetInt(destination,blend?1:0);
        material->SetInt("_SrcAlpha",0);material->SetInt("_DestAlpha",1);material->SetInt("_BlendOp",0);material->SetInt("_ZWrite",0);material->SetInt("_Cull",0);
        material->SetColor("_Color",{1,1,1,1});material->SetColor("_BaseColor",{1,1,1,1});
        material->SetFloat("_SoftParticlesEnabled",0);material->SetFloat("_CameraFadingEnabled",0);material->SetFloat("_DistortionEnabled",0);material->SetFloat("_LightingEnabled",0);
        for(auto keyword:{"_ALPHATEST_ON","_ALPHAPREMULTIPLY_ON","_ALPHAMODULATE_ON","_FADING_ON","_DISTORTION_ON","SOFTPARTICLES_ON"})material->DisableKeyword(keyword);
        material->set_renderQueue(3001);PaperLogger.info("Video material: {}, soft additive {}",static_cast<std::string>(shader->get_name()),blend);return material;
    }
    auto shader=Shader::Find(config.hasVignette?"UI/Default":"Unlit/Texture");
    if(!shader)throw std::runtime_error("Video material unavailable");
    auto material=Material::New_ctor(shader);material->SetColor("_Color",{1,1,1,1});material->SetInt("unity_GUIZTestMode",4);
    if(blend)PaperLogger.warn("Soft additive video shader unavailable; using the standard screen material");
    return material;
}
struct VideoImageProcessor::Impl {
    VideoConfig config;
    RenderTexture* input{},*output{};
    Material* body{};
    Texture2D* black{};
    std::vector<uint32_t> handles;
    std::shared_ptr<RenderJob> job;
    int id{};
    bool backendWarning{};
    template<class T>T* Hold(T* value){handles.push_back(il2cpp_functions::gchandle_new(reinterpret_cast<Il2CppObject*>(value),false));return value;}
    explicit Impl(const VideoConfig& value):config(value){}
    ~Impl() {
        // Retire GL objects on the render thread before Unity queues texture release.
        if(id)Issue(-id);
        for(auto rt:{input,output})if(rt&&Object::op_Implicit_bool(rt)){rt->Release();Object::Destroy(rt);}
        if(body&&Object::op_Implicit_bool(body))Object::Destroy(body);
        if(black&&Object::op_Implicit_bool(black))Object::Destroy(black);
        for(auto handle:handles)il2cpp_functions::gchandle_free(handle);
    }
};
VideoImageProcessor::VideoImageProcessor(const VideoConfig& config):impl(std::make_unique<Impl>(config)){}
VideoImageProcessor::~VideoImageProcessor()=default;
UnityEngine::Texture* VideoImageProcessor::Update(UnityEngine::Texture* texture) {
    if(!texture||(!impl->config.hasColorCorrection&&!impl->config.hasVignette))return texture;
    if(SystemInfo::get_graphicsDeviceType()!=Rendering::GraphicsDeviceType::OpenGLES3) {
        if(!impl->backendWarning){PaperLogger.warn("Video color/vignette filter requires OpenGLES3; displaying the original video");impl->backendWarning=true;}
        return texture;
    }
    if(impl->job&&impl->job->failed)return texture;
    if(!impl->input) {
        int width=texture->get_width(),height=texture->get_height();
        if(width<=0||height<=0)return texture;
        auto create=[&] {
            auto rt=impl->Hold(RenderTexture::New_ctor(width,height,0,RenderTextureFormat::ARGB32,RenderTextureReadWrite::Linear));
            rt->set_filterMode(FilterMode::Bilinear);rt->set_wrapMode(TextureWrapMode::Clamp);rt->Create();return rt;
        };
        impl->input=create();impl->output=create();
        impl->job=std::make_shared<RenderJob>();auto& job=*impl->job;
        job.source=static_cast<GLuint>(impl->input->GetNativeTexturePtr().ToInt64());job.target=static_cast<GLuint>(impl->output->GetNativeTexturePtr().ToInt64());
        job.width=width;job.height=height;job.color=impl->config.color;job.vignette=impl->config.vignette;
        impl->id=nextID.fetch_add(1);{std::lock_guard guard(jobsMutex);jobs.emplace(impl->id,impl->job);}
    }
    Graphics::Blit(texture,impl->input);Issue(impl->id);
    return impl->job->ready?static_cast<Texture*>(impl->output):texture;
}
void VideoImageProcessor::AttachScreenBody(UnityEngine::GameObject* screen,UnityEngine::Mesh* mesh) {
    if(getModConfig().TransparencyEnabled.GetValue()&&!impl->config.transparency.value_or(false))return;
    if(!impl->body) {
        auto shader=Shader::Find("Unlit/Texture");if(!shader)return;
        impl->black=impl->Hold(Texture2D::New_ctor(1,1,TextureFormat::RGBA32,false,true));
        impl->black->SetPixel(0,0,{0,0,0,1});impl->black->Apply(false,false);
        impl->body=impl->Hold(Material::New_ctor(shader));impl->body->set_mainTexture(impl->black);impl->body->set_renderQueue(1999);
    }
    auto body=GameObject::New_ctor("TheaterOpaqueScreenBody");body->set_layer(screen->get_layer());body->get_transform()->SetParent(screen->get_transform(),false);
    body->get_transform()->set_localPosition({0,0,.02f});body->AddComponent<MeshFilter*>()->set_sharedMesh(mesh);body->AddComponent<MeshRenderer*>()->set_sharedMaterial(impl->body);
}
}
