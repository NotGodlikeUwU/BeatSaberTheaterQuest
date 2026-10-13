// Video glow is world geometry; environment lighting follows PC Theater (GPL-3.0).
#include "screen_effects.hpp"
#include "main.hpp"

#include "GlobalNamespace/DirectionalLight.hpp"
#include "GlobalNamespace/MovementBeatmapEventEffect.hpp"
#include "GlobalNamespace/CoreGameHUDController.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Graphics.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/RenderTextureFormat.hpp"
#include "UnityEngine/RenderTextureReadWrite.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Bounds.hpp"
#include "UnityEngine/Shader.hpp"
#include <algorithm>
#include <cmath>
#include <set>
using namespace UnityEngine;
namespace theater {
namespace {
std::vector<GameObject*> effectScreens;
Mesh* effectMesh{};
GlobalNamespace::DirectionalLight* screenLight{};
Material* glowMaterial{};
Mesh* glowMesh{};
Texture2D* glowTexture{}, *glowSample{};
std::vector<GameObject*> glowObjects;
std::vector<float> glowAlpha;
float glowAmount{};
Texture2D* pixel{};
std::vector<RenderTexture*> downscale;
float nextLightUpdate{};
struct SavedObject { GameObject* object; bool active; Vector3 position, scale; Quaternion rotation; };
std::vector<SavedObject> originals;
std::vector<GameObject*> clones;
std::set<GameObject*> saved;
GlobalNamespace::MovementBeatmapEventEffect* movement{};
std::vector<Vector3> movementOriginals;
Transform* softParent{}, *softRoot{};
Vector3 softOffset{};
Quaternion softRotation{};
std::vector<uint32_t> handles;
template<typename T> T* Hold(T* object) {
    if(object)handles.push_back(il2cpp_functions::gchandle_new(reinterpret_cast<Il2CppObject*>(object),false));
    return object;
}
struct Occluder { Renderer* renderer; bool enabled, hidden, ring; };
std::vector<Occluder> occluders;
float nextOcclusionCheck{};
bool CanCoverScreen(Renderer* renderer) {
    auto bounds=renderer->get_bounds();auto minimum=bounds.get_min(),maximum=bounds.get_max();
    Vector3 eye(0,1.6f,0);auto camera=Camera::get_main();if(camera)eye=camera->get_transform()->get_position();
    for(auto screen:effectScreens) {
        auto transform=screen->get_transform();auto observer=transform->InverseTransformPoint(eye);
        if(std::abs(observer.z)<.1f)continue;
        auto center=transform->InverseTransformPoint(bounds.get_center());
        float depth=(center.z-observer.z)/-observer.z;
        if(depth<=.02f || depth>=.98f)continue;
        float left=1e9f,right=-1e9f,bottom=1e9f,top=-1e9f;
        for(int i=0;i<8;i++) {
            auto corner=transform->InverseTransformPoint({(i&1)?maximum.x:minimum.x,(i&2)?maximum.y:minimum.y,(i&4)?maximum.z:minimum.z});
            float distance=(corner.z-observer.z)/-observer.z;if(distance<=.02f)continue;
            float x=observer.x+(corner.x-observer.x)/distance,y=observer.y+(corner.y-observer.y)/distance;
            left=std::min(left,x);right=std::max(right,x);bottom=std::min(bottom,y);top=std::max(top,y);
        }
        auto extent=effectMesh->get_bounds().get_extents();
        if(left<extent.x*.95f && right>-extent.x*.95f && bottom<extent.y*.95f && top>-extent.y*.95f)return true;
    }
    return false;
}
void FindOccluders() {
    for(auto renderer:Resources::FindObjectsOfTypeAll<Renderer*>()) {
        auto object=renderer->get_gameObject();auto scene=object->get_scene();
        if(!scene.IsValid() || !scene.get_isLoaded() || !std::string(scene.get_name()).ends_with("Environment"))continue;
        if(!renderer->get_enabled() || !object->get_activeInHierarchy())continue;
        bool ring=false,excluded=false;
        for(auto t=object->get_transform();t;t=t->get_parent()) {
            std::string name=t->get_name();
            ring|=name.find("TrackLaneRing")!=std::string::npos;
            excluded|=name=="PlayersPlace" || name.find("TrackMirror")!=std::string::npos || name.find("Floor")!=std::string::npos || name.find("CinemaScreen")!=std::string::npos || name.find("Theater")!=std::string::npos;
        }
        if(excluded)continue;
        auto material=renderer->get_sharedMaterial();if(!material || (!ring && material->get_renderQueue()>=2500))continue;
        occluders.push_back({Hold(renderer),true,false,ring});
    }
    nextOcclusionCheck=0;
}
void PreventOcclusion(bool visible) {
    float now=Time::get_realtimeSinceStartup();
    if(visible && now<nextOcclusionCheck)return;
    nextOcclusionCheck=now+.5f;
    for(auto& item:occluders) {
        if(!Object::op_Implicit_bool(item.renderer))continue;
        if(!visible) {if(item.hidden)item.renderer->set_enabled(item.enabled);item.hidden=false;continue;}
        if(item.hidden || !CanCoverScreen(item.renderer))continue;
        item.renderer->set_enabled(false);item.hidden=true;
        PaperLogger.info("Hidden environment geometry covering video: {}",static_cast<std::string>(item.renderer->get_gameObject()->get_name()));
    }
}
int ObjectIndex(const std::string& name) {
    auto index=name.find('(');
    return index!=std::string::npos && index+1<name.size() && std::isdigit(name[index+1]) ? name[index+1]-'0' : 0;
}
void Save(GameObject* object) {
    if (!saved.insert(object).second) return;
    auto t=object->get_transform();
    Hold(object);
    originals.push_back({object,object->get_activeSelf(),t->get_localPosition(),t->get_localScale(),t->get_localRotation()});
}
std::string Name(Object* object) { return object ? static_cast<std::string>(object->get_name()) : ""; }
bool Environment(GameObject* object) {
    auto scene=object->get_scene();
    if (!scene.IsValid() || !scene.get_isLoaded()) return false;
    std::string name=scene.get_name();
    return name.ends_with("Environment") || name=="GameCore";
}
void ModifyEnvironment(const VideoConfig& config) {
    auto objects=Resources::FindObjectsOfTypeAll<GameObject*>();
    std::vector<GameObject*> environment;
    GameObject* front{}, *coreLight{};
    for (auto object:objects) {
        if (!Environment(object)) continue;
        environment.push_back(object);
        auto name=Name(object), parent=Name(object->get_transform()->get_parent());
        if (object->get_activeInHierarchy() && (name=="FrontLights" || name=="FrontLight")) front=object;
        if (name=="DirectionalLight" && parent=="CoreLighting" && object->get_activeInHierarchy()) coreLight=object;
    }
    // PC Theater frees a directional-light slot and removes front lights washing out the video.
    if (front) { Save(front); front->SetActive(false); }
    if (coreLight) { Save(coreLight); coreLight->SetActive(false); }
    if (!config.disableDefaultModifications) for (auto object:environment) {
        auto t=object->get_transform(); auto name=Name(object), parent=Name(t->get_parent());
        std::string scene=object->get_scene().get_name();
        auto p=t->get_position(); auto scale=t->get_localScale();
        bool move=false, resize=false, hide=false;
        auto side=[&] { return p.x < 0 ? -1.f : 1.f; };
        if (scene=="NiceEnvironment" || scene=="BigMirrorEnvironment") {
            if (name.starts_with("DoubleColorLaser")) { p.x+=18*(ObjectIndex(name)%2==0?-1:1);p.z-=28.5f;move=true; }
            if (parent.starts_with("RotatingLasersPair")) { p.x=20*side();move=true; }
        } else if (scene=="OriginsEnvironment") {
            if (name.starts_with("Spectrogram")) { p.x=12*side();move=true; }
        } else if (scene=="KDAEnvironment") {
            if (name=="Construction" && parent!="PlayersPlace") { scale={1,2,1};resize=true; }
            if (name.starts_with("Tentacle")) { p.x=15*side();move=true;Save(object);t->set_eulerAngles({0,-135*side(),0}); }
            if (name.starts_with("Laser") && !parent.starts_with("RotatingLasersPair")) { p.x=10*side();move=true; }
            if (name=="GlowTopLine") { p.y=20;move=true; }
            if (parent.starts_with("RotatingLasersPair")) { p.x=18*side();move=true; }
        } else if (scene=="RocketEnvironment") {
            if (name.starts_with("RocketCar")) { p.x=16*side();move=true; }
            if (name=="RocketArena") { scale={2.38f,2,1};resize=true; }
            if (name=="RocketArenaLight") { p={0,5.8f,42.4f};scale={2.38f,1,1};move=resize=true;Save(object);t->set_eulerAngles({90,180,0}); }
            if (name=="RocketGateLight") { p={0,-3,64};scale={2.6f,1,4.5f};move=resize=true; }
        } else if (scene=="DragonsEnvironment" || scene=="Dragons2Environment") {
            bool second=scene=="Dragons2Environment";
            if (name.starts_with("Spectrogram")) { if(second){Save(object);t->set_eulerAngles({0,p.x<0?170.f:10.f,0});}p.x=(second?18:16)*side();move=true; }
            if (parent.starts_with("RotatingLasersPair")) { p.x=20*side();move=true; }
            if (name=="TopConstruction") { p.y=27;move=true; }
            if (name=="HallConstruction") { p.y=second?17.2f:22;move=true;if(second){scale={1,.7f,1};resize=true;} }
            hide=name.starts_with("PanelsTrackLaneRing") || (second && parent=="CoreLighting" && (name=="Left" || name=="Right"));
        } else if (scene=="BTSEnvironment") {
            hide=name=="MagicDoorSprite";
            if (name=="PillarL" || name=="PillarR") { p.x=(16+ObjectIndex(parent)*2.3f)*(name=="PillarL"?-1:1);move=true; }
            if(name.find("PillarPair")!=std::string::npos) {p.y-=2;move=true;}
        } else if (scene=="LinkinParkEnvironment") {
            hide=name=="Logo" && parent=="Environment";
            if(name=="Environment") {scale={4,3,3};resize=true;}
            if(name=="TrackConstruction") {p={.9f,0,106.5f};scale={.25f,1.f/3,1.f/3};move=resize=true;}
            if(name=="TrackMirror") {p={.3f,0,6.55f};scale={.25f,1.f/3,1.f/3};move=resize=true;}
            if(name=="TrackShadow") {p={0,-.3f,126.1f};scale={.25f,1.f/3,1.f/3};move=resize=true;}
            if(name=="PlayersPlace" || name=="PlayersPlaceShadow" || name=="NarrowGameHUD") {scale={.25f,1.f/3,1.f/3};resize=true;}
        } else if (scene=="CrabRaveEnvironment" || scene=="MonstercatEnvironment") {
            if(name=="SmallTrackLaneRing(Clone)"){scale={3,3,1};resize=true;}
            if(name=="GlowLineL" || name=="GlowLineR"){p={name=="GlowLineL"?-10.f:10.f,-1.5f,9.5f};move=true;}
            if(name=="GlowLineL (1)" || name=="GlowLineR (1)"){p={name=="GlowLineL (1)"?-12.f:12.f,1.5f,-20};move=true;}
            hide=name.find("MonstercatLogo")!=std::string::npos;
            if(name.find("GlowTopLine")!=std::string::npos){scale={2,2,2};resize=true;}
        } else if (scene=="KaleidoscopeEnvironment") {
            hide=(name=="Construction" && parent!="PlayersPlace") || name=="TrackMirror";
            if (parent.starts_with("ConeRing") && (name=="Cone" || name=="Cone (1)")) { Save(object);auto local=t->get_localPosition();local.y+=name=="Cone"?2.5f:-2.5f;t->set_localPosition(local); }
        } else if (scene=="InterscopeEnvironment") {
            hide=name=="Plane (1)" || name=="Plane (4)" || name=="NeonTop";
        } else if (scene=="SkrillexEnvironment") {
            if (name=="SkrillexLogo" || name=="SkrillexLogo (1)") { p={-.23f,name=="SkrillexLogo"?15.5f:-15.5f,60};move=true; }
        } else if (scene=="PyroEnvironment") {
            hide=name=="PyroLogo" || name=="Video" || name.starts_with("ScreenSetup");
            if (name=="LightGroupLeft" || name=="LightGroupRight") { p={11.1f*side(),.99f,59};scale={.7f,.7f,.7f};move=resize=true; }
            if(name=="LightBoxesScaffoldingLeft" || name=="LightBoxesScaffoldingRight") {p={name.ends_with("Left")?-37.65f:37.65f,-2.92f,50.54f};move=true;}
            if(name=="LightGroupCloserLeft" || name=="LightGroupCloserRight") {p={name.ends_with("Left")?-35.13f:35.13f,-.02f,66.74f};move=true;}
            if(name=="CrowdFlipbookGroup") {p={-4.83f,-.4f,-1.8f};move=true;}
            if (name=="Stairs") {p={0,-1,51.31f};move=true;}
        } else if (scene=="LizzoEnvironment") {
            if (name=="Rainbow") {p={0,2.1f,65.51f};scale={2,2,2};move=resize=true;}
            if (name=="BehindRainbowSpotlights") {p={0,2.1f,62.24f};scale={2,2,2};move=resize=true;}
        } else if (scene=="WeaveEnvironment") {
            if (name=="LightGroup14" || name=="LightGroup15") {p={name=="LightGroup14"?-3.85f:3.85f,1.5f,20.9f};move=true;}
        } else if (scene=="EDMEnvironment" && name=="Spectrograms") {p={0,-2,1.2f};move=true;}
        if (move || resize || hide) {
            Save(object);if(move)t->set_position(p);if(resize)t->set_localScale(scale);if(hide)object->SetActive(false);
        }
    }
    for (auto& change:config.environment) for (auto object:environment) {
        if (Name(object)!=(change.cloneFrom.empty()?change.name:change.cloneFrom)) continue;
        if (!change.parentName.empty() && Name(object->get_transform()->get_parent())!=change.parentName) continue;
        auto target=object;
        if (!change.cloneFrom.empty()) { target=Object::Instantiate<GameObject*>(object,object->get_transform()->get_parent());target->set_name(change.name);clones.push_back(target); }
        else Save(target);
        auto t=target->get_transform();
        if(change.position)t->set_position({change.position->x,change.position->y,change.position->z});
        if(change.rotation)t->set_eulerAngles({change.rotation->x,change.rotation->y,change.rotation->z});
        if(change.scale)t->set_localScale({change.scale->x,change.scale->y,change.scale->z});
        if(change.active)target->SetActive(*change.active);
    }
    if(!config.disableDefaultModifications) {
        auto effects=Resources::FindObjectsOfTypeAll<GlobalNamespace::MovementBeatmapEventEffect*>();
        for(auto effect:effects) if(Name(effect)=="PillarsMovementEffect" && Environment(effect->get_gameObject())) {
            movement=effect;
            for(auto value:effect->_startLocalPositions)movementOriginals.push_back(value);
            ArrayW<Vector3> positions(effect->_transforms.size());
            for(int i=0;i<positions.size();i++)positions[i]=effect->_transforms[i]->get_localPosition();
            effect->_startLocalPositions=positions;
            break;
        }
        bool desert=std::any_of(environment.begin(),environment.end(),[](auto object){return std::string(object->get_scene().get_name())=="GlassDesertEnvironment";});
        if(desert)for(auto hud:Resources::FindObjectsOfTypeAll<GlobalNamespace::CoreGameHUDController*>()) if(hud->get_isActiveAndEnabled()) {
            softRoot=effectScreens[0]->get_transform()->get_parent();softParent=hud->get_transform();
            softOffset=Vector3::op_Subtraction(softParent->get_position(),softRoot->get_position());
            softRotation=Quaternion::op_Multiply(softParent->get_rotation(),Quaternion::Inverse(softRoot->get_rotation()));break;
        }
    }
    PaperLogger.info("PC environment modifications applied to {} objects",originals.size());
}

void CreateWorldGlow(const VideoConfig& config) {
    glowAmount=std::clamp(config.bloom,0.f,2.f);
    if(glowAmount==0)return;
    auto shader=Shader::Find("UI/Default");
    if(!shader)shader=Shader::Find("Unlit/Transparent");
    if(!shader)throw std::runtime_error("No transparent shader for world-space video glow");
    glowTexture=Hold(Texture2D::New_ctor(64,64,TextureFormat::RGBA32,false,true));
    glowTexture->set_wrapMode(TextureWrapMode::Clamp);glowTexture->set_filterMode(FilterMode::Bilinear);
    ArrayW<Color> clear(64*64);for(auto& color:clear)color={0,0,0,0};
    glowTexture->SetPixels(clear);glowTexture->Apply(false,false);
    glowSample=Hold(Texture2D::New_ctor(64,64,TextureFormat::RGBA32,false,true));
    glowMaterial=Hold(Material::New_ctor(shader));glowMaterial->set_mainTexture(glowTexture);
    glowMaterial->SetColor("_Color",{1,1,1,1});glowMaterial->SetInt("unity_GUIZTestMode",4);
    glowMaterial->set_renderQueue(3000);
    auto bounds=effectMesh->get_bounds();auto extent=bounds.get_extents();
    float width=extent.x*2,height=extent.y*2,offset=height*.05f;
    glowMesh=Hold(Mesh::New_ctor());
    ArrayW<Vector3> vertices(4);vertices[0]={-extent.x*1.6f,-extent.y*1.6f,0};vertices[1]={-extent.x*1.6f,extent.y*1.6f,0};vertices[2]={extent.x*1.6f,-extent.y*1.6f,0};vertices[3]={extent.x*1.6f,extent.y*1.6f,0};
    ArrayW<Vector2> uv(4);uv[0]={0,0};uv[1]={0,1};uv[2]={1,0};uv[3]={1,1};
    ArrayW<int> triangles(6);int indices[]{0,1,2,2,1,3};for(int i=0;i<6;i++)triangles[i]=indices[i];
    ArrayW<Color> colors(4);for(auto& color:colors)color={1,1,1,1};
    glowMesh->set_vertices(vertices);glowMesh->set_uv(uv);glowMesh->set_triangles(triangles);glowMesh->set_colors(colors);glowMesh->RecalculateBounds();
    glowAlpha.resize(64*64);
    for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
        float px=((x+.5f)/64-.5f)*1.6f*width,py=((y+.5f)/64-.5f)*1.6f*height-offset;
        float dx=std::max(0.f,std::abs(px)/width-.5f),dy=std::max(0.f,std::abs(py)/height-.5f);
        // Keep the actual video rectangle transparent; the halo cannot bleach it.
        float alpha=(dx==0 && dy==0)?0.f:std::exp(-(dx*dx+dy*dy)*70.f);
        if(py>0)alpha*=.55f;
        float edge=std::min({(x+.5f)/64,(y+.5f)/64,1-(x+.5f)/64,1-(y+.5f)/64});
        glowAlpha[y*64+x]=alpha*std::clamp(edge*16.f,0.f,1.f)*.55f;
    }
    for(auto screen:effectScreens) {
        auto object=Hold(GameObject::New_ctor("TheaterWorldVideoGlow"));object->set_layer(screen->get_layer());
        object->get_transform()->SetParent(screen->get_transform(),false);
        object->get_transform()->set_localPosition({0,-offset,.08f});
        object->AddComponent<MeshFilter*>()->set_sharedMesh(glowMesh);object->AddComponent<MeshRenderer*>()->set_sharedMaterial(glowMaterial);
        object->SetActive(false);glowObjects.push_back(object);
    }
    PaperLogger.info("World video glow: shader {}, {} screen children, local downward offset {}",std::string(shader->get_name()),glowObjects.size(),offset);
}

void UpdateWorldGlow() {
    if(!glowTexture || !glowSample || downscale.size()<2)return;
    RenderTexture::set_active(downscale[1]);glowSample->ReadPixels({0,0,64,64},0,0,false);
    auto input=glowSample->GetPixels();std::vector<Color> horizontal(4096),blurred(4096);
    constexpr int radius=8;
    for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
        Color sum{0,0,0,0};for(int k=-radius;k<=radius;k++){auto c=input[y*64+std::clamp(x+k,0,63)];sum.r+=c.r;sum.g+=c.g;sum.b+=c.b;}
        horizontal[y*64+x]={sum.r/17,sum.g/17,sum.b/17,1};
    }
    for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
        Color sum{0,0,0,0};for(int k=-radius;k<=radius;k++){auto c=horizontal[std::clamp(y+k,0,63)*64+x];sum.r+=c.r;sum.g+=c.g;sum.b+=c.b;}
        blurred[y*64+x]={sum.r/17,sum.g/17,sum.b/17,1};
    }
    ArrayW<Color> output(4096);
    for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
        int sx=std::clamp(static_cast<int>(((x+.5f)/64-.5f)*1.6f*64+32),0,63);
        int sy=std::clamp(static_cast<int>(((y+.5f)/64-.5f)*1.6f*64+32-3.2f),0,63);
        auto c=blurred[sy*64+sx];float alpha=glowAlpha[y*64+x]*std::min(glowAmount,1.f);
        output[y*64+x]={std::clamp(c.r,0.f,.7f),std::clamp(c.g,0.f,.7f),std::clamp(c.b,0.f,.7f),alpha};
    }
    glowTexture->SetPixels(output);glowTexture->Apply(false,false);
}

}
void StartScreenEffects(const VideoConfig& config,const std::vector<GameObject*>& screens,Mesh* mesh,Material* material,float aspect) {
    effectScreens=screens;effectMesh=mesh;
    Hold(mesh);Hold(material);for(auto screen:screens)Hold(screen);
    ModifyEnvironment(config);
    FindOccluders();
    CreateWorldGlow(config);
    auto light=GameObject::New_ctor("TheaterVideoLight");light->get_transform()->SetParent(screens[0]->get_transform(),false);
    light->get_transform()->set_forward(Vector3::op_UnaryNegation(screens[0]->get_transform()->get_forward()));
    auto angle=light->get_transform()->get_eulerAngles();angle.x=15;
    screenLight=Hold(light->AddComponent<GlobalNamespace::DirectionalLight*>());screenLight->intensity=2;screenLight->radius=250;screenLight->color={0,0,0,1};
    for(auto& original:originals) {
        std::string env=original.object->get_scene().get_name();
        if(env=="BillieEnvironment"){screenLight->intensity=1.2f;angle.x=42;}
        if(env=="BTSEnvironment"){screenLight->intensity=1.6f;angle.x=55;}
        if(env=="LizzoEnvironment"){screenLight->intensity=1;angle.x=42;}
    }
    light->get_transform()->set_eulerAngles(angle);
    pixel=Hold(Texture2D::New_ctor(1,1,TextureFormat::RGBA32,false,true));
    for(int size=128;size>=1;size>>=1) {auto texture=Hold(RenderTexture::New_ctor(size,size,0,RenderTextureFormat::ARGB32,RenderTextureReadWrite::Linear));texture->Create();downscale.push_back(texture);}
    PaperLogger.info("World-space video glow and video directional light initialized");
}
void UpdateScreenLight(Texture* texture,bool visible) {
    if(softParent && Object::op_Implicit_bool(softParent) && softRoot) {
        auto side=Vector3::op_Multiply(softParent->get_right(),softOffset.x),forward=Vector3::op_Multiply(softParent->get_forward(),softOffset.z);
        auto offset=Vector3::op_Addition(side,forward);offset.y=softOffset.y;
        softRoot->set_position(Vector3::op_Subtraction(softParent->get_position(),offset));
        softRoot->set_rotation(Quaternion::op_Multiply(softParent->get_rotation(),Quaternion::Inverse(softRotation)));
    }
    for(auto object:glowObjects)object->SetActive(visible);
    PreventOcclusion(visible);
    if(!screenLight)return;
    if(!visible || !texture){screenLight->color={0,0,0,1};return;}
    float now=Time::get_realtimeSinceStartup();if(now<nextLightUpdate)return;nextLightUpdate=now+.1f;
    auto previous=RenderTexture::get_active();
    try {
        Texture* input=texture;for(auto output:downscale){Graphics::Blit(input,output);input=output;}
        RenderTexture::set_active(downscale.back());pixel->ReadPixels(Rect(0,0,1,1),0,0,false);
        screenLight->color=pixel->GetPixel(0,0);
        UpdateWorldGlow();
    } catch (...) {RenderTexture::set_active(previous);throw;}
    RenderTexture::set_active(previous);
}
void StopScreenEffects() {
    PreventOcclusion(false);occluders.clear();nextOcclusionCheck=0;
    softParent=nullptr;softRoot=nullptr;
    if(movement && Object::op_Implicit_bool(movement)) {
        ArrayW<Vector3> previous(movementOriginals.size());for(int i=0;i<previous.size();i++)previous[i]=movementOriginals[i];movement->_startLocalPositions=previous;
    }
    movement=nullptr;movementOriginals.clear();
    effectScreens.clear();effectMesh=nullptr;screenLight=nullptr;nextLightUpdate=0;
    for(auto object:glowObjects)if(Object::op_Implicit_bool(object))Object::Destroy(object);glowObjects.clear();glowAlpha.clear();
    if(glowMaterial && Object::op_Implicit_bool(glowMaterial))Object::Destroy(glowMaterial);glowMaterial=nullptr;
    if(glowMesh && Object::op_Implicit_bool(glowMesh))Object::Destroy(glowMesh);glowMesh=nullptr;
    if(glowTexture && Object::op_Implicit_bool(glowTexture))Object::Destroy(glowTexture);glowTexture=nullptr;
    if(glowSample && Object::op_Implicit_bool(glowSample))Object::Destroy(glowSample);glowSample=nullptr;
    if(pixel && Object::op_Implicit_bool(pixel))Object::Destroy(pixel);pixel=nullptr;
    for(auto rt:downscale) if(Object::op_Implicit_bool(rt)){rt->Release();Object::Destroy(rt);}downscale.clear();
    for(auto object:clones)if(Object::op_Implicit_bool(object))Object::Destroy(object);clones.clear();
    for(auto& saved:originals) if(Object::op_Implicit_bool(saved.object)) {
        auto t=saved.object->get_transform();t->set_localPosition(saved.position);t->set_localRotation(saved.rotation);t->set_localScale(saved.scale);saved.object->SetActive(saved.active);
    }
    originals.clear();saved.clear();
    for(auto handle:handles)il2cpp_functions::gchandle_free(handle);handles.clear();
}

}



