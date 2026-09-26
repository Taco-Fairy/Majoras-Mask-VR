package com.fulldivegames.majorasmaskvr;
import android.content.Context;
import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.util.zip.*;
import org.json.JSONObject;
public final class RomExtractor {
 static {System.loadLibrary("c++_shared");System.loadLibrary("SDL2");System.loadLibrary("openxr_loader");System.loadLibrary("mmvr_extract");}
 static native void extract(String work,String revision,String port)throws IOException;
 static native int progress();
 public interface Status {void show(String text);}
 public static void run(Context context,InputStream source,Status status)throws Exception{
  File root=context.getExternalFilesDir(null);if(root==null)throw new IOException("App storage unavailable");
  File work=Files.createTempDirectory(context.getCacheDir().toPath(),"rom-import-").toFile();
  File stage=new File(root,"mm.o2r.pending");
  try{
   status.show("Validating your ROM…");String revision=RomPolicy.normalize(source,new File(work,"rom.z64"));
   status.show("Preparing extraction…");File tools=new File(work,"extractor.zip");SetupActivity.copy(context.getAssets().open("extractor.zip"),tools,64L*1024*1024);
   String expected=new JSONObject(SetupActivity.read(context.getAssets().open("bundled-assets.json"))).getString("extractor.zip");
   if(!SetupActivity.sha(tools).equals(expected))throw new IOException("Extractor resources failed verification");
   try(ZipInputStream zip=new ZipInputStream(new FileInputStream(tools))){ZipEntry e;long size=0;byte[] b=new byte[65536];while((e=zip.getNextEntry())!=null){
    Path path=work.toPath().resolve(e.getName()).normalize();if(!path.startsWith(work.toPath().resolve("assets"))||e.getName().contains("\\"))throw new IOException("Invalid extractor resource path");
    if(e.isDirectory()){Files.createDirectories(path);continue;}Files.createDirectories(path.getParent());
    try(OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){int n;while((n=zip.read(b))!=-1){if((size+=n)>128L*1024*1024)throw new IOException("Extractor resources exceed limit");out.write(b,0,n);}}
   }}
   byte[] version;try(ZipFile zip=new ZipFile(new File(root,"2ship.o2r"));InputStream in=zip.getInputStream(zip.getEntry("portVersion"))){version=new byte[7];new DataInputStream(in).readFully(version);}
   if(version.length!=7)throw new IOException("Invalid support archive version");
   ByteBuffer data=ByteBuffer.wrap(version,1,6).order(version[0]==1?ByteOrder.BIG_ENDIAN:ByteOrder.LITTLE_ENDIAN);
   String port=(data.getShort()&65535)+"."+(data.getShort()&65535)+"."+(data.getShort()&65535);
   status.show("Extracting game assets on your headset…");extract(work.getAbsolutePath(),revision,port);
   File result=new File(work,"mm.o2r");ArchivePolicy.verify(result,new File(root,"2ship.o2r"));
   SetupActivity.copy(new FileInputStream(result),stage,2L*1024*1024*1024);ArchivePolicy.verify(stage,new File(root,"2ship.o2r"));
   Files.move(stage.toPath(),new File(root,"mm.o2r").toPath(),StandardCopyOption.REPLACE_EXISTING,StandardCopyOption.ATOMIC_MOVE);
   status.show("ROM imported successfully");
  }finally{
   ImportCleanup.run(work.toPath(),stage.toPath(),root.toPath(),
    (operation,error)->android.util.Log.w("MMVR-ROM-import","Cleanup: "+operation,error));
  }
 }
}
