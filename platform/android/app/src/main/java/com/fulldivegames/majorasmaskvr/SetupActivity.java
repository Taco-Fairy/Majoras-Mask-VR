package com.fulldivegames.majorasmaskvr;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.net.Uri;
import android.widget.*;
import java.io.*;
import java.security.MessageDigest;
import java.util.Locale;
import org.json.JSONObject;
public final class SetupActivity extends Activity {
 private TextView status;private Button choose,archive;private volatile boolean busy;
 private File root(){return getExternalFilesDir(null);}
 private static boolean keepSelectedFeed(File file){
  if(!file.isFile())return false;
  try{return !new JSONObject(read(new FileInputStream(file))).optString("url","").isEmpty();}
  catch(Exception invalid){return true;} // Updater reports a malformed custom feed; game startup remains usable.
 }
 @Override public void onCreate(Bundle saved){super.onCreate(saved);
  LinearLayout layout=new LinearLayout(this);layout.setOrientation(LinearLayout.VERTICAL);layout.setPadding(48,32,48,32);
  status=new TextView(this);status.setTextSize(22);layout.addView(status);
  choose=new Button(this);choose.setText("Choose Majora's Mask ROM");choose.setOnClickListener(v->pick(1));layout.addView(choose);
  archive=new Button(this);archive.setText("Import existing mm.o2r instead");archive.setOnClickListener(v->pick(2));layout.addView(archive);setContentView(layout);
  prepare();
 }
 private void display(String text){runOnUiThread(()->{status.setText(text);choose.setEnabled(!busy);archive.setEnabled(!busy);});}
 private void pick(int request){
  Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE);
  intent.putExtra(android.provider.DocumentsContract.EXTRA_INITIAL_URI,android.provider.DocumentsContract.buildDocumentUri("com.android.externalstorage.documents","primary:Download"));
  try{startActivityForResult(intent,request);}catch(android.content.ActivityNotFoundException e){display("No document picker is installed. Install a file picker or copy mm.o2r to this app's files folder.");}
 }
 private Intent gameIntent(){
  Intent game=new Intent(this,GameActivity.class);
  // A local development request can run the native renderer in a normal panel
  // when headset room tracking is unavailable. Native code still gates the
  // diagnostic mode and blocks save writes; this is not a playable VR mode.
  boolean flat=false;
  try{if(BuildConfig.MMVR_PRIVATE_TOOLS){String request=read(new FileInputStream(new File(root(),"mmvr-test-request.txt"))).trim();flat=request.equals("performance-flat")||request.equals("performance-flat-detailed")||request.equals("performance-flat-legacy")||request.equals("shader-flat")||request.equals("arena-flat")||request.equals("room-flat")||request.equals("state-capture-flat")||request.equals("state-reload-flat")||request.equals("state-import-flat");}}catch(Exception ignored){}
  if(!flat)game.addCategory("org.khronos.openxr.intent.category.IMMERSIVE_HMD");return game;
 }
 private void prepare(){busy=true;display("Preparing game resources…");new Thread(()->{try{
   if(root()==null)throw new IOException("App storage unavailable");
   JSONObject bundled=new JSONObject(read(getAssets().open("bundled-assets.json")));
   for(String name:new String[]{"2ship.o2r","gamecontrollerdb.txt","version.json","update-feed.json"}){
    File target=new File(root(),name);String wanted=bundled.getString(name);
    if("update-feed.json".equals(name)&&keepSelectedFeed(target))continue; // Preserve a selected feed; an unconfigured preview may adopt the bundled release feed.
    if(!target.isFile()||!sha(target).equals(wanted)){
     File stage=new File(root(),name+".pending");copy(getAssets().open(name),stage,256L*1024*1024);
     if(!sha(stage).equals(wanted))throw new IOException("Bundled asset verification failed: "+name);
     java.nio.file.Files.move(stage.toPath(),target.toPath(),java.nio.file.StandardCopyOption.REPLACE_EXISTING,java.nio.file.StandardCopyOption.ATOMIC_MOVE);
    }
   }
   new File(root(),"mods").mkdirs();new File(root(),"saves").mkdirs();
   try{SharedStorage.sync(this);}catch(Exception storage){android.util.Log.w("MMVR-Storage","Shared packs could not refresh; keeping last cached set",storage);}
   if(new File(root(),"mm.o2r").isFile()){ArchivePolicy.verify(new File(root(),"mm.o2r"),new File(root(),"2ship.o2r"));runOnUiThread(()->{startActivity(gameIntent());finish();});return;}
   busy=false;display("Choose your own uncompressed Majora's Mask ROM (.z64, .v64 or .n64). Browse Downloads or another folder. Extraction happens here on your headset. US 1.0 and US GameCube are supported.");
  }catch(Exception e){busy=false;display("Setup failed: "+e.getMessage());}},"MMVR-setup").start();}
 @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);
  if(request==3){if(result==RESULT_OK)prepare();else{busy=false;display("Choose a ROM to retry import.");}return;}
  if(result!=RESULT_OK||data==null||data.getData()==null)return;Uri uri=data.getData();
  if(request==1){busy=true;display("Importing selected ROM...");startActivityForResult(new Intent(this,RomImportActivity.class).setData(uri).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION),3);return;}
  if(request!=2)return;busy=true;display("Importing game archive...");
  new Thread(()->{File stage=new File(root(),"mm.o2r.pending");try{
   copy(getContentResolver().openInputStream(uri),stage,2L*1024*1024*1024);
   ArchivePolicy.verify(stage,new File(root(),"2ship.o2r"));
   java.nio.file.Files.move(stage.toPath(),new File(root(),"mm.o2r").toPath(),java.nio.file.StandardCopyOption.REPLACE_EXISTING,java.nio.file.StandardCopyOption.ATOMIC_MOVE);
   prepare();
  }catch(Exception e){stage.delete();busy=false;display("Import failed: "+e.getMessage());}},"MMVR-import").start();
 }
 static void copy(InputStream input,File to,long limit)throws Exception{
  if(input==null)throw new IOException("Cannot open selected file");
  try(InputStream in=input;FileOutputStream out=new FileOutputStream(to)){byte[] bytes=new byte[65536];long total=0;int n;while((n=in.read(bytes))!=-1){if((total+=n)>limit)throw new IOException("File exceeds allowed size");out.write(bytes,0,n);}out.getFD().sync();}
 }
 static String read(InputStream input)throws Exception{try(InputStream in=input;ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] b=new byte[4096];int n;while((n=in.read(b))!=-1)out.write(b,0,n);return out.toString("UTF-8");}}
 static String sha(File file)throws Exception{MessageDigest digest=MessageDigest.getInstance("SHA-256");try(InputStream in=new FileInputStream(file)){byte[] b=new byte[65536];int n;while((n=in.read(b))!=-1)digest.update(b,0,n);}StringBuilder s=new StringBuilder();for(byte b:digest.digest())s.append(String.format(Locale.ROOT,"%02x",b&255));return s.toString();}
}
