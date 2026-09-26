package com.fulldivegames.majorasmaskvr;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import org.json.*;

/** A persisted grant for MMVR only. No broad storage permissions are requested. */
final class SharedStorage {
 static final int REQUEST=47;
 static final String AUTHORITY="com.android.externalstorage.documents", ID="primary:MMVR";
 static final String README="Majora's Mask VR / 2Ship2Harkinian shared files\n\n"
  +"mods and texturepacks: unpack the downloaded ZIP; place .o2r or .otr packs here. Subfolders work.\n"
  +"VR Settings > System > Mods and texture packs: select this MMVR folder once. Restart to reload packs.\n"
  +"Private app mods load first, then shared mods, then texture packs in alphabetical path order.\n"
  +"Use the VR menu checkboxes to enable or disable packs, then restart. VR item icons use pack textures.\n"
  +"cache/shaders is reserved; this preview does not persist compiled shader binaries there.\n"
  +"exports is for your manual backups. Active saves, settings and the imported game archive stay in\n"
  +"Android/data/com.fulldivegames.majorasmaskvr/files. Keep your own backup before uninstalling.\n"
  +"Android's folder picker grants access to MMVR only. Packs are cached privately for the native loader.\n";
 static void pick(Activity a){
  Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION|Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
  intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI,DocumentsContract.buildDocumentUri(AUTHORITY,ID));
  a.startActivityForResult(intent,REQUEST);
 }
 static boolean allowed(String id){return ID.equals(id)||id.startsWith(ID+"/mods/")||id.equals(ID+"/mods")||id.equals(ID+"/texturepacks")||id.startsWith(ID+"/texturepacks/");}
 static synchronized void accept(Context c,Intent data)throws Exception{
  Uri tree=data.getData();
  if(tree==null||!AUTHORITY.equals(tree.getAuthority())||!allowed(DocumentsContract.getTreeDocumentId(tree)))throw new IOException("Select the MMVR folder at the root of headset storage. Create it in the picker if needed.");
  int flags=data.getFlags()&(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
  if((flags&3)!=3)throw new IOException("Read and write access to MMVR is needed.");
  c.getContentResolver().takePersistableUriPermission(tree,flags);
  c.getSharedPreferences("mmvr-shared-files",0).edit().putString("tree",tree.toString()).apply();
  if(ID.equals(DocumentsContract.getTreeDocumentId(tree)))layout(c,tree);
 }
 static class Entry {String id,name,mime;long size,modified;Entry(Cursor c){id=c.getString(0);name=c.getString(1);mime=c.getString(2);size=c.isNull(3)?-1:c.getLong(3);modified=c.isNull(4)?0:c.getLong(4);}}
 static List<Entry> children(Context c,Uri tree,String parent)throws Exception{
  List<Entry> result=new ArrayList<>();Uri query=DocumentsContract.buildChildDocumentsUriUsingTree(tree,parent);
  String[] cols={DocumentsContract.Document.COLUMN_DOCUMENT_ID,DocumentsContract.Document.COLUMN_DISPLAY_NAME,DocumentsContract.Document.COLUMN_MIME_TYPE,DocumentsContract.Document.COLUMN_SIZE,DocumentsContract.Document.COLUMN_LAST_MODIFIED};
  try(Cursor cursor=c.getContentResolver().query(query,cols,null,null,null)){
   if(cursor==null)throw new IOException("MMVR folder is unavailable");while(cursor.moveToNext()){
    Entry e=new Entry(cursor);if(e.id.startsWith(ID+"/")&&e.name!=null&&!e.name.contains("/")&&!e.name.equals(".."))result.add(e);
   }
  }
  result.sort(Comparator.comparing((Entry e)->e.name.toLowerCase(Locale.ROOT)).thenComparing(e->e.name));return result;
 }
 static Uri ensure(Context c,Uri tree,String parent,String name,String mime)throws Exception{
  for(Entry e:children(c,tree,parent))if(name.equals(e.name)){if(!mime.equals(e.mime)&&DocumentsContract.Document.MIME_TYPE_DIR.equals(mime))throw new IOException(name+" must be a folder");return DocumentsContract.buildDocumentUriUsingTree(tree,e.id);}
  Uri result=DocumentsContract.createDocument(c.getContentResolver(),DocumentsContract.buildDocumentUriUsingTree(tree,parent),mime,name);
  if(result==null)throw new IOException("Cannot create "+name);return result;
 }
 static void layout(Context c,Uri tree)throws Exception{
  for(String name:new String[]{"mods","texturepacks","exports"})ensure(c,tree,ID,name,DocumentsContract.Document.MIME_TYPE_DIR);
  Uri cache=ensure(c,tree,ID,"cache",DocumentsContract.Document.MIME_TYPE_DIR);ensure(c,tree,DocumentsContract.getDocumentId(cache),"shaders",DocumentsContract.Document.MIME_TYPE_DIR);
  Uri readme=ensure(c,tree,ID,"README.txt","text/plain");try(OutputStream out=c.getContentResolver().openOutputStream(readme,"wt")){if(out==null)throw new IOException("Cannot write MMVR README");out.write(README.getBytes(StandardCharsets.UTF_8));}
 }
 static void scan(Context c,Uri tree,String dir,String relative,File cache,JSONObject previous,JSONArray current,int depth,Set<String> visited,boolean force)throws Exception{
  if(depth>12||!visited.add(dir))throw new IOException("Pack folder nesting is too deep or repeated");
  for(Entry e:children(c,tree,dir)){
   String name=relative+e.name;
   if(DocumentsContract.Document.MIME_TYPE_DIR.equals(e.mime)){scan(c,tree,e.id,name+"/",cache,previous,current,depth+1,visited,force);continue;}
   String lower=e.name.toLowerCase(Locale.ROOT);if(!lower.endsWith(".o2r")&&!lower.endsWith(".otr"))continue;
   String ext=lower.endsWith(".o2r")?".o2r":".otr";MessageDigest digest=MessageDigest.getInstance("SHA-256");
   byte[] bytes=digest.digest(e.id.getBytes(StandardCharsets.UTF_8));StringBuilder key=new StringBuilder();for(byte b:bytes)key.append(String.format(Locale.ROOT,"%02x",b&255));
   String filename=key+ext;File local=new File(cache,filename);JSONObject old=previous.optJSONObject(filename);
   if(force||!local.isFile()||old==null||e.modified<=0||old.optLong("modified")!=e.modified||old.optLong("size")!=e.size||(e.size>=0&&local.length()!=e.size)){
    File stage=new File(cache,filename+".pending");
    SetupActivity.copy(c.getContentResolver().openInputStream(DocumentsContract.buildDocumentUriUsingTree(tree,e.id)),stage,8L*1024*1024*1024);
    if(e.size>=0&&stage.length()!=e.size)throw new IOException("Incomplete pack: "+name);
    Files.move(stage.toPath(),local.toPath(),StandardCopyOption.REPLACE_EXISTING,StandardCopyOption.ATOMIC_MOVE);
   }
   current.put(new JSONObject().put("file",filename).put("name",name).put("size",e.size).put("modified",e.modified));
  }
 }
 // Refresh and picker callbacks run on separate workers. One transaction owns
 // the shared staging filenames and manifest until its atomic publish finishes.
 static synchronized void sync(Context c)throws Exception{sync(c,false);}
 // An explicit refresh rereads pack contents even when a document provider
 // reports an unchanged or coarse modification time and identical file size.
 static synchronized void sync(Context c,boolean force)throws Exception{
  String selected=c.getSharedPreferences("mmvr-shared-files",0).getString("tree","");if(selected.isEmpty())return;
  Uri tree=Uri.parse(selected);if(!AUTHORITY.equals(tree.getAuthority())||!allowed(DocumentsContract.getTreeDocumentId(tree)))throw new IOException("Invalid shared folder selection");
  if(ID.equals(DocumentsContract.getTreeDocumentId(tree)))layout(c,tree);File root=c.getExternalFilesDir(null);if(root==null)throw new IOException("App storage is unavailable");File cache=new File(root,"shared-pack-cache");if(!cache.isDirectory()&&!cache.mkdirs())throw new IOException("Cannot prepare pack cache");
  File manifest=new File(cache,"packs.json");JSONObject previous=new JSONObject();
  if(manifest.isFile())try{JSONArray old=new JSONArray(SetupActivity.read(new FileInputStream(manifest)));for(int i=0;i<old.length();++i){JSONObject entry=old.getJSONObject(i);previous.put(entry.getString("file"),entry);}}catch(JSONException invalid){android.util.Log.w("MMVR-Storage","Rebuilding pack cache index");}
  JSONArray current=new JSONArray();Set<String> visited=new HashSet<>();
  String selectedId=DocumentsContract.getTreeDocumentId(tree);
  if(ID.equals(selectedId))for(String folder:new String[]{"mods","texturepacks"})scan(c,tree,ID+"/"+folder,folder+"/",cache,previous,current,0,visited,force);
  else scan(c,tree,selectedId,selectedId.substring(ID.length()+1)+"/",cache,previous,current,0,visited,force);
  File stage=new File(cache,"packs.json.pending");Files.write(stage.toPath(),current.toString(2).getBytes(StandardCharsets.UTF_8));Files.move(stage.toPath(),manifest.toPath(),StandardCopyOption.REPLACE_EXISTING,StandardCopyOption.ATOMIC_MOVE);
  android.util.Log.i("MMVR-Storage","Prepared "+current.length()+" shared packs");
 }
}
