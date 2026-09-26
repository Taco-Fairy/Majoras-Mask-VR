#include <jni.h>
#include <atomic>
#include <array>
#include <filesystem>
#include <string>
#include <mutex>
#include <cstdio>
extern "C" int zapd_report(int,char**,std::atomic<size_t>*,std::atomic<size_t>*);
static std::atomic<size_t> done{0},total{0};
static std::mutex extraction;
static std::string text(JNIEnv* env,jstring value){const char* chars=env->GetStringUTFChars(value,nullptr);if(!chars)throw std::runtime_error("Cannot read extraction path");std::string s(chars);env->ReleaseStringUTFChars(value,chars);return s;}
extern "C" JNIEXPORT jint JNICALL Java_com_fulldivegames_majorasmaskvr_RomExtractor_progress(JNIEnv*,jclass){auto n=total.load();return n?static_cast<jint>(std::min(size_t(99),done.load()*100/n)):0;}
extern "C" JNIEXPORT void JNICALL Java_com_fulldivegames_majorasmaskvr_RomExtractor_extract(JNIEnv* env,jclass,jstring work,jstring version,jstring port){
 std::lock_guard<std::mutex> guard(extraction);
 const auto previous=std::filesystem::current_path();
 try{
  const auto folder=text(env,work),v=text(env,version),p=text(env,port);
  if(v!="N64_US"&&v!="GC_US")throw std::runtime_error("Unsupported ROM revision");
  std::filesystem::current_path(folder);
  // This library runs only in the dedicated ROM-import process, before game startup.
  freopen("extract.log","a",stdout);freopen("extract.log","a",stderr);setvbuf(stdout,nullptr,_IOLBF,0);setvbuf(stderr,nullptr,_IOLBF,0);
  done=0;total=0;
  std::array<std::string,22> args={"ZAPD","ed","-i","assets/xml/"+v,"-b","rom.z64","-fl","assets/filelists","-gsf","0","-rconf","assets/Config_"+v+".xml","-se","OTR","--otrfile","mm.o2r","--portVer",p,"-o","placeholder","-osf","placeholder"};
  std::array<char*,22> argv;for(size_t i=0;i<args.size();++i)argv[i]=args[i].data();
  if(zapd_report(argv.size(),argv.data(),&done,&total)!=0||!std::filesystem::is_regular_file("mm.o2r"))throw std::runtime_error("ROM extraction failed; see rom-import.log");
  std::filesystem::current_path(previous);
 }catch(const std::exception& e){std::error_code ignored;std::filesystem::current_path(previous,ignored);if(!env->ExceptionCheck())env->ThrowNew(env->FindClass("java/io/IOException"),e.what());}
}
