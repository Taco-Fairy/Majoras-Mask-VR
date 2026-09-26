#include <openxr/openxr.h>
#include "controller_profiles.h"
#include <iostream>
#include <vector>
#include <cstring>
int main(int argc,char** argv){
 const bool headsetOnly=argc==2&&std::strcmp(argv[1],"--headset-only")==0;
 XrInstance instance=XR_NULL_HANDLE;XrActionSet set=XR_NULL_HANDLE;
 try{
  auto check=[](XrResult r){if(XR_FAILED(r))throw int(r);};
  uint32_t count=0;check(xrEnumerateInstanceExtensionProperties(nullptr,0,&count,nullptr));
  std::vector<XrExtensionProperties> available(count,{XR_TYPE_EXTENSION_PROPERTIES});check(xrEnumerateInstanceExtensionProperties(nullptr,count,&count,available.data()));
  auto has=[&](const char* s){return std::any_of(available.begin(),available.end(),[&](const auto& e){return std::strcmp(s,e.extensionName)==0;});};
  std::vector<const char*> extensions;
  for(const auto& p:mmvr::compat::Profiles())if(*p.extension&&has(p.extension)&&std::none_of(extensions.begin(),extensions.end(),[&](auto e){return std::strcmp(e,p.extension)==0;}))extensions.push_back(p.extension);
  XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};std::strcpy(info.applicationInfo.applicationName,"MMVR compatibility probe");info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);info.enabledExtensionCount=uint32_t(extensions.size());info.enabledExtensionNames=extensions.data();check(xrCreateInstance(&info,&instance));
  XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};check(xrGetInstanceProperties(instance,&props));std::cout<<"Runtime: "<<props.runtimeName<<" version="<<props.runtimeVersion<<"\n";
  XrSystemId system=XR_NULL_SYSTEM_ID;XrSystemGetInfo get{XR_TYPE_SYSTEM_GET_INFO};get.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;auto result=xrGetSystem(instance,&get,&system);std::cout<<"Headset discovery result="<<result<<"\n";
  if(XR_SUCCEEDED(result)){XrSystemProperties sys{XR_TYPE_SYSTEM_PROPERTIES};check(xrGetSystemProperties(instance,system,&sys));std::cout<<"Headset: "<<sys.systemName<<"\n";}
  if(headsetOnly){xrDestroyInstance(instance);return XR_SUCCEEDED(result)?0:2;}
  XrActionSetCreateInfo asi{XR_TYPE_ACTION_SET_CREATE_INFO};std::strcpy(asi.actionSetName,"compatibility");std::strcpy(asi.localizedActionSetName,"MMVR compatibility");check(xrCreateActionSet(instance,&asi,&set));
  XrAction actions[29]{};for(int a=0;a<29;++a){XrActionCreateInfo ai{XR_TYPE_ACTION_CREATE_INFO};std::snprintf(ai.actionName,sizeof(ai.actionName),"action_%d",a);std::snprintf(ai.localizedActionName,sizeof(ai.localizedActionName),"Action %d",a);
   ai.actionType=((a<9&&a!=6&&a!=7)||(a>=19&&a<=20)||(a>=23&&a<=26))?XR_ACTION_TYPE_BOOLEAN_INPUT:(a==6||a==7||a==11||a==12||a>=27)?XR_ACTION_TYPE_FLOAT_INPUT:(a==9||a==10||a==21||a==22)?XR_ACTION_TYPE_VECTOR2F_INPUT:(a>=13&&a<=16)?XR_ACTION_TYPE_POSE_INPUT:XR_ACTION_TYPE_VIBRATION_OUTPUT;check(xrCreateAction(set,&ai,&actions[a]));}
  unsigned accepted=0;for(const auto& p:mmvr::compat::Profiles()){
   if(*p.extension&&!has(p.extension))continue;
   std::vector<XrActionSuggestedBinding> bindings;for(const auto& b:p.bindings){XrPath path;check(xrStringToPath(instance,b.path,&path));bindings.push_back({actions[b.action],path});}
   XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};check(xrStringToPath(instance,p.path,&suggested.interactionProfile));suggested.countSuggestedBindings=uint32_t(bindings.size());suggested.suggestedBindings=bindings.data();auto r=xrSuggestInteractionProfileBindings(instance,&suggested);
   std::cout<<p.path<<" result="<<r<<"\n";if(XR_SUCCEEDED(r))++accepted;
  }
  std::cout<<"Accepted profiles="<<accepted<<"; binding probe only, no session or stereo validation\n";
  xrDestroyActionSet(set);xrDestroyInstance(instance);return accepted?0:2;
 }catch(int r){std::cerr<<"Probe failed OpenXR="<<r<<"\n";if(set)xrDestroyActionSet(set);if(instance)xrDestroyInstance(instance);return 1;}
}
