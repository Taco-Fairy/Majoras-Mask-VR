package com.fulldivegames.majorasmaskvr;
import java.net.URI;
public final class UpdatePolicy {
 public static final long MAX_APK_BYTES=1024L*1024*1024;
 public static void url(String text) throws Exception {
  URI uri=new URI(text);
  if(!"https".equals(uri.getScheme())||uri.getHost()==null||uri.getUserInfo()!=null)throw new IllegalArgumentException("Updates require an HTTPS URL");
 }
 public static void digest(String text){if(text==null||!text.matches("[0-9a-fA-F]{64}"))throw new IllegalArgumentException("Invalid SHA-256");}
 public static void size(long bytes){if(bytes<1||bytes>MAX_APK_BYTES)throw new IllegalArgumentException("APK size outside allowed range");}
 public static void identity(String expected,String actual,long current,long candidate){
  if(!expected.equals(actual))throw new IllegalArgumentException("APK belongs to a different application");
  if(candidate<=current||candidate>Integer.MAX_VALUE)throw new IllegalArgumentException("APK is not a newer build");
 }
}
