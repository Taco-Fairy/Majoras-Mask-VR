package com.fulldivegames.majorasmaskvr;
import java.io.*;
import java.nio.*;
import java.util.zip.*;
/** Matches the supported native game IDs and major-version rule before SDL starts. */
public final class ArchivePolicy {
 private static byte[] entry(ZipFile zip,String name,int size)throws IOException{
  ZipEntry e=zip.getEntry(name);if(e==null||e.getSize()!=size)throw new IOException("Missing or invalid "+name+" in game archive");
  try(InputStream in=zip.getInputStream(e)){byte[] b=new byte[size];new DataInputStream(in).readFully(b);if(in.read()!=-1)throw new IOException("Invalid archive metadata");return b;}
 }
 private static ByteBuffer numbers(byte[] b)throws IOException{
  if(b[0]!=0&&b[0]!=1)throw new IOException("Invalid archive byte order");
  return ByteBuffer.wrap(b,1,b.length-1).order(b[0]==1?ByteOrder.BIG_ENDIAN:ByteOrder.LITTLE_ENDIAN);
 }
 public static void verify(File game,File support)throws IOException{
  try(ZipFile archive=new ZipFile(game);ZipFile bundled=new ZipFile(support)){
   int id=numbers(entry(archive,"version",5)).getInt();
   if(id!=0x5354631c&&id!=0xb443eb08)throw new IOException("Select a supported Majora's Mask mm.o2r archive");
   if(numbers(entry(archive,"portVersion",7)).getShort()!=numbers(entry(bundled,"portVersion",7)).getShort())throw new IOException("Import your ROM again to regenerate the game archive");
   if(archive.getEntry("audio/audio")==null||archive.size()<100)throw new IOException("Incomplete mm.o2r game archive");
  }
 }
}
