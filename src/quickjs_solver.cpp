#include <jni.h>
#include <chrono>
#include <string>
#include <vector>
extern "C" {
#include "quickjs.h"
extern const unsigned char bstheater_ejs_lib_start[], bstheater_ejs_lib_end[];
extern const unsigned char bstheater_ejs_core_start[], bstheater_ejs_core_end[];
}
namespace {
int Interrupt(JSRuntime*, void* opaque) {
    auto deadline = static_cast<std::chrono::steady_clock::time_point*>(opaque);
    return std::chrono::steady_clock::now() > *deadline;
}
std::string Utf8(JNIEnv* env, jstring value) {
    auto* chars = env->GetStringChars(value,nullptr);
    if (!chars) return {};
    std::string output;
    auto count=env->GetStringLength(value);
    for (int i=0;i<count;++i) {
        uint32_t cp=chars[i];
        if (cp>=0xd800 && cp<=0xdbff && i+1<count && chars[i+1]>=0xdc00 && chars[i+1]<=0xdfff) cp=0x10000+((cp-0xd800)<<10)+(chars[++i]-0xdc00);
        if(cp<0x80) output.push_back(cp);
        else if(cp<0x800) {output.push_back(0xc0|(cp>>6));output.push_back(0x80|(cp&63));}
        else if(cp<0x10000) {output.push_back(0xe0|(cp>>12));output.push_back(0x80|((cp>>6)&63));output.push_back(0x80|(cp&63));}
        else {output.push_back(0xf0|(cp>>18));output.push_back(0x80|((cp>>12)&63));output.push_back(0x80|((cp>>6)&63));output.push_back(0x80|(cp&63));}
    }
    env->ReleaseStringChars(value,chars); return output;
}
jstring JavaString(JNIEnv* env, const char* source, size_t length) {
    std::vector<jchar> result;
    for(size_t i=0;i<length;) {
        uint32_t cp=static_cast<unsigned char>(source[i++]); unsigned extra=0;
        if(cp>=0xf0) {cp&=7;extra=3;} else if(cp>=0xe0) {cp&=15;extra=2;} else if(cp>=0xc0) {cp&=31;extra=1;}
        while(extra-- && i<length) cp=(cp<<6)|(static_cast<unsigned char>(source[i++])&63);
        if(cp<0x10000) result.push_back(cp); else {cp-=0x10000;result.push_back(0xd800|(cp>>10));result.push_back(0xdc00|(cp&1023));}
    }
    return env->NewString(result.data(),static_cast<jsize>(result.size()));
}
void Fail(JNIEnv* env, const char* stage) {
    // Never return JS exception text: it may contain a signed media challenge.
    auto klass=env->FindClass("java/io/IOException");
    if(klass) env->ThrowNew(klass,stage);
}
bool Eval(JSContext* context,const char* source,size_t length,const char* filename) {
    JSValue value=JS_Eval(context,source,length,filename,JS_EVAL_TYPE_GLOBAL);
    bool ok=!JS_IsException(value);JS_FreeValue(context,value);return ok;
}
}
extern "C" JNIEXPORT jstring JNICALL Java_org_bstheater_quest_NativePlayerSolver_evaluate(JNIEnv* env,jclass,jstring input) {
    if(!input) {Fail(env,"Missing EJS input");return nullptr;}
    std::string task=Utf8(env,input);
    if(env->ExceptionCheck()) return nullptr;
    if(task.size()>16*1024*1024) {Fail(env,"EJS task exceeds size limit");return nullptr;}
    JSRuntime* runtime=JS_NewRuntime();
    if(!runtime) {Fail(env,"Cannot allocate EJS runtime");return nullptr;}
    JS_SetMemoryLimit(runtime,384*1024*1024);
    JS_SetMaxStackSize(runtime,4*1024*1024);
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);
    JS_SetInterruptHandler(runtime,Interrupt,&deadline);
    JSContext* context=JS_NewContext(runtime);
    if(!context) {JS_FreeRuntime(runtime);Fail(env,"Cannot allocate EJS context");return nullptr;}
    jstring output=nullptr;
    const char* error=nullptr;
    if(!Eval(context,reinterpret_cast<const char*>(bstheater_ejs_lib_start),bstheater_ejs_lib_end-bstheater_ejs_lib_start,"ejs-lib") ||
       !Eval(context,"var meriyah=lib.meriyah,astring=lib.astring;",sizeof("var meriyah=lib.meriyah,astring=lib.astring;")-1,"ejs-setup") ||
       !Eval(context,reinterpret_cast<const char*>(bstheater_ejs_core_start),bstheater_ejs_core_end-bstheater_ejs_core_start,"ejs-core")) error="EJS library initialization failed";
    else {
        JSValue argument=JS_ParseJSON(context,task.data(),task.size(),"ejs-task");
        JSValue global=JS_GetGlobalObject(context);
        JSValue solver=JS_GetPropertyStr(context,global,"jsc");
        JSValue result=JS_IsException(argument) ? JS_EXCEPTION : JS_Call(context,solver,global,1,&argument);
        JSValue json=JS_IsException(result) ? JS_EXCEPTION : JS_JSONStringify(context,result,JS_UNDEFINED,JS_UNDEFINED);
        if(JS_IsException(json)) error="EJS player execution failed or exceeded limits";
        else {
            size_t length=0;const char* bytes=JS_ToCStringLen(context,&length,json);
            if(bytes) {output=JavaString(env,bytes,length);JS_FreeCString(context,bytes);} else error="EJS result encoding failed";
        }
        JS_FreeValue(context,json);JS_FreeValue(context,result);JS_FreeValue(context,solver);JS_FreeValue(context,global);JS_FreeValue(context,argument);
    }
    JS_FreeContext(context);JS_FreeRuntime(runtime);
    if(error) Fail(env,error);
    return output;
}
