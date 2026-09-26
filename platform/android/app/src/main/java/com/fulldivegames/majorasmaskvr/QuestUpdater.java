package com.fulldivegames.majorasmaskvr;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import java.io.*;
import java.net.*;
import java.security.MessageDigest;
import java.util.Locale;
import org.json.JSONObject;
public final class QuestUpdater {
 private final Activity activity;
 private volatile String status="Select Check for updates.";
 private boolean busy;
 private volatile File pendingApk;
 private volatile boolean installing;
 private String pendingDigest;
 private long pendingVersion;
 QuestUpdater(Activity activity){this.activity=activity;}
 String status(){return status;}
 // Pack refresh messages share the status area, not the install lifecycle.
 void report(String text){status=text;}
 void installationResult(String text){installing=false;status=text;}
 private File root(){return activity.getExternalFilesDir(null);}
 synchronized void request(boolean install){
  if(busy||installing)return;
  if(pendingApk!=null){if(install)activity.runOnUiThread(this::installOrRequestPermission);return;}busy=true;
  new Thread(()->{try{run(install);}catch(Exception e){status="Update failed: "+e.getMessage();}finally{synchronized(this){busy=false;}}},"MMVR-update").start();
 }
 private void run(boolean install)throws Exception{
  status="Checking for updates...";
  File feedFile=new File(root(),"update-feed.json");
  String feed=feedFile.isFile()?new JSONObject(SetupActivity.read(new FileInputStream(feedFile))).optString("url",""):"";
  if(feed.isEmpty()){status="No release feed is configured.";return;}
  byte[] document=readManifest(feed);
  JSONObject manifest=new JSONObject(new String(document,java.nio.charset.StandardCharsets.UTF_8));
  if(manifest.getInt("schema")!=1||!"mmvr".equals(manifest.getString("product")))throw new IOException("Invalid MMVR release manifest");
  PackageInfo installed=activity.getPackageManager().getPackageInfo(activity.getPackageName(),0);
  long current=installed.getLongVersionCode();long version=manifest.getLong("build");
  if(version<=current){status="Already up to date ("+installed.versionName+").";return;}
  JSONObject asset=manifest.getJSONObject("quest");
  UpdatePolicy.identity(activity.getPackageName(),asset.getString("packageName"),current,asset.getLong("versionCode"));
  if(version!=asset.getLong("versionCode"))throw new IOException("Release/APK version mismatch");
  String url=asset.getString("url"),digest=asset.getString("sha256");long bytes=asset.getLong("size");
  UpdatePolicy.url(url);UpdatePolicy.digest(digest);UpdatePolicy.size(bytes);
  if(!install){status="Update "+manifest.getString("version")+" is available. Select Install update.";return;}
  File stage=new File(activity.getCacheDir(),"mmvr-update.pending.apk");
  download(url,stage,bytes);
  verify(stage,digest,version);
  // Publish the volatile readiness flag last: resumed() can run concurrently.
  pendingDigest=digest;pendingVersion=version;pendingApk=stage;
  activity.runOnUiThread(this::installOrRequestPermission);
 }
 private HttpURLConnection connect(String text)throws Exception{
  // Follow only bounded HTTPS redirects. Never relax TLS or accept credentials in URLs.
  for(int i=0;i<6;++i){UpdatePolicy.url(text);HttpURLConnection c=(HttpURLConnection)new URL(text).openConnection();
   c.setConnectTimeout(15000);c.setReadTimeout(30000);c.setInstanceFollowRedirects(false);c.setRequestProperty("User-Agent","FullDiveGames-MMVR-Updater/1");
   int code=c.getResponseCode();if(code>=300&&code<400){String location=c.getHeaderField("Location");c.disconnect();if(location==null)throw new IOException("Missing update redirect");text=new URL(new URL(text),location).toString();continue;}
   if(code!=200){c.disconnect();throw new IOException("Update server returned HTTP "+code);}return c;
  }throw new IOException("Too many update redirects");
 }
 private byte[] readManifest(String url)throws Exception{
  HttpURLConnection c=connect(url);
  try(InputStream in=c.getInputStream();ByteArrayOutputStream out=new ByteArrayOutputStream()){
   byte[] b=new byte[4096];int n;while((n=in.read(b))!=-1){if(out.size()+n>1048576)throw new IOException("Release manifest too large");out.write(b,0,n);}return out.toByteArray();
  }finally{c.disconnect();}
 }
 private void download(String url,File file,long expected)throws Exception{
  HttpURLConnection c=connect(url);long header=c.getContentLengthLong();
  try{if(header>=0&&header!=expected)throw new IOException("APK length mismatch");
   try(InputStream in=c.getInputStream();FileOutputStream out=new FileOutputStream(file)){
    byte[] b=new byte[65536];long total=0,last=0;int n;
    while((n=in.read(b))!=-1){total+=n;if(total>expected)throw new IOException("APK exceeds declared size");out.write(b,0,n);
     if(System.currentTimeMillis()-last>500){status="Downloading update: "+(total*100/expected)+"%";last=System.currentTimeMillis();}}
    out.getFD().sync();if(total!=expected)throw new IOException("APK download truncated");
   }
  }finally{c.disconnect();}
 }
 private void verify(File apk,String hash,long version)throws Exception{
  status="Verifying APK and signing identity...";
  if(!SetupActivity.sha(apk).equalsIgnoreCase(hash))throw new IOException("APK SHA-256 mismatch");
  PackageManager pm=activity.getPackageManager();PackageInfo candidate=pm.getPackageArchiveInfo(apk.getAbsolutePath(),PackageManager.GET_SIGNING_CERTIFICATES);
  PackageInfo current=pm.getPackageInfo(activity.getPackageName(),0);
  if(candidate==null||candidate.signingInfo==null)throw new IOException("Downloaded file is not a signed APK");
  UpdatePolicy.identity(activity.getPackageName(),candidate.packageName,current.getLongVersionCode(),candidate.getLongVersionCode());
  if(candidate.getLongVersionCode()!=version)throw new IOException("APK version does not match release manifest");
  android.content.pm.Signature[] signers=candidate.signingInfo.getApkContentsSigners();
  if(signers.length!=1)throw new IOException("Unexpected APK signing identity");
  byte[] cert=MessageDigest.getInstance("SHA-256").digest(signers[0].toByteArray());
  if(!pm.hasSigningCertificate(activity.getPackageName(),cert,PackageManager.CERT_INPUT_SHA256))throw new IOException("APK signing certificate differs from installed MMVR");
 }
 private void installOrRequestPermission(){
  if(pendingApk==null||activity.isFinishing())return;
  if(!activity.getPackageManager().canRequestPackageInstalls()){
   status="Allow MMVR updates in Android, then return to the game.";
   try{activity.startActivityForResult(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,Uri.parse("package:"+activity.getPackageName())),3301);}catch(Exception e){status="Cannot open Android install permission: "+e.getMessage();}
   return;
  }
  File apk=pendingApk;String digest=pendingDigest;long version=pendingVersion;installing=true;pendingApk=null;
  new Thread(()->{int id=-1;PackageInstaller installer=activity.getPackageManager().getPackageInstaller();
   try{verify(apk,digest,version);
    PackageInstaller.SessionParams params=new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
    params.setAppPackageName(activity.getPackageName());params.setSize(apk.length());
    if(Build.VERSION.SDK_INT>=31)params.setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_REQUIRED);
    id=installer.createSession(params);
    try(PackageInstaller.Session session=installer.openSession(id)){
     try(InputStream in=new FileInputStream(apk);OutputStream out=session.openWrite("base.apk",0,apk.length())){byte[] b=new byte[65536];int n;while((n=in.read(b))!=-1)out.write(b,0,n);session.fsync(out);}
     Intent callback=new Intent(activity,UpdateInstallReceiver.class).setAction(activity.getPackageName()+".UPDATE_RESULT");
     int flags=PendingIntent.FLAG_UPDATE_CURRENT;if(Build.VERSION.SDK_INT>=31)flags|=PendingIntent.FLAG_MUTABLE;
     PendingIntent pending=PendingIntent.getBroadcast(activity,id,callback,flags);
     status="Confirm the MMVR update in Android.";session.commit(pending.getIntentSender());
    }
   }catch(Exception e){if(id>=0)installer.abandonSession(id);installing=false;status="Install failed: "+e.getMessage();android.util.Log.e("MMVRUpdater",status,e);}
  },"MMVR-install").start();
 }
 void resumed(){if(pendingApk!=null&&activity.getPackageManager().canRequestPackageInstalls())installOrRequestPermission();}
}
