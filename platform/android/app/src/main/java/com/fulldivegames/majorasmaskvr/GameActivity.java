package com.fulldivegames.majorasmaskvr;
import org.libsdl.app.SDLActivity;
public final class GameActivity extends SDLActivity {
 private QuestUpdater updater;
 private static java.lang.ref.WeakReference<GameActivity> active=new java.lang.ref.WeakReference<>(null);
 @Override protected void onCreate(android.os.Bundle saved){updater=new QuestUpdater(this);active=new java.lang.ref.WeakReference<>(this);super.onCreate(saved);}
 // Android's 2D window focus is not OpenXR session focus. Start SDL while the
 // Activity is resumed; OpenXR FOCUSED/STOPPING still gates gameplay and actions.
 @Override public void onWindowFocusChanged(boolean hasFocus){super.onWindowFocusChanged(true);}
 // The native engine has process-lifetime registries and cannot run main twice.
 // SDL first joins its game thread (including save/resource cleanup), then a full
 // activity close exits this app process. Headset sleep/onPause never takes this path.
 @Override protected void onDestroy(){
  boolean finished=isFinishing()&&!isChangingConfigurations();
  super.onDestroy();
  if(finished){android.util.Log.i("MMVR-Lifecycle","Native game closed; ending process for a clean relaunch");android.os.Process.killProcess(android.os.Process.myPid());}
 }
 @Override protected void onResume(){super.onResume();if(updater!=null)updater.resumed();}
 private final java.util.concurrent.atomic.AtomicBoolean packsRefreshing=new java.util.concurrent.atomic.AtomicBoolean();
 public void refreshMMVRPacks(){
  if(!packsRefreshing.compareAndSet(false,true))return;
  updater.report("Refreshing packs...");
  new Thread(()->{try{
   if(getSharedPreferences("mmvr-shared-files",0).getString("tree","").isEmpty())
    updater.report("Connect the MMVR shared folder once, then refresh. Unpack ZIPs into mods or texturepacks.");
   else {SharedStorage.sync(this,true);updater.report("Pack list refreshed. Restart to apply selections. Unpack ZIPs first.");}
  }catch(Exception e){updater.report("Pack refresh: "+e.getMessage());}finally{packsRefreshing.set(false);}},"MMVR-packs").start();
 }
 public void requestMMVRSharedFolder(){runOnUiThread(()->{try{SharedStorage.pick(this);}catch(Exception e){updater.report("Folder picker unavailable: "+e.getMessage());}});}
 @Override protected void onActivityResult(int request,int result,android.content.Intent data){super.onActivityResult(request,result,data);
  if(request!=SharedStorage.REQUEST||result!=RESULT_OK||data==null)return;
  new Thread(()->{try{SharedStorage.accept(this,data);SharedStorage.sync(this,true);updater.report("MMVR folder connected. Restart the game to load changed packs.");}catch(Exception e){updater.report("Shared folder: "+e.getMessage());}},"MMVR-folder").start();
 }
 public void requestMMVRUpdate(boolean install){updater.request(install);}
 public String getMMVRUpdateStatus(){return updater.status();}
 static void updateResult(String text){GameActivity activity=active.get();if(activity!=null&&activity.updater!=null)activity.updater.installationResult(text);}

 @Override protected String[] getLibraries(){return new String[]{"c++_shared","SDL2","openxr_loader","2ship"};}
 @Override protected String getMainSharedObject(){return getApplicationInfo().nativeLibraryDir+"/lib2ship.so";}
}
