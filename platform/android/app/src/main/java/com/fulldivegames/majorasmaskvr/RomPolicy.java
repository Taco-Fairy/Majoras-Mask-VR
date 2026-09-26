package com.fulldivegames.majorasmaskvr;
import java.io.*;
import java.util.zip.CRC32C;
/** Same supported revisions and whole-ROM CRC32C values as the desktop extractor. */
public final class RomPolicy {
 private static int block(InputStream in,byte[] b)throws IOException{int n=0,k;while(n<b.length&&(k=in.read(b,n,b.length-n))!=-1){if(k==0)continue;n+=k;}return n;}
 public static String normalize(InputStream source,File output)throws IOException{
  if(source==null)throw new IOException("Cannot open selected ROM");
  CRC32C crc=new CRC32C();long total=0;int mode=-1;long id=0;
  try(DataInputStream in=new DataInputStream(new BufferedInputStream(source));FileOutputStream out=new FileOutputStream(output)){
   byte[] b=new byte[65536];int n;
   while((n=block(in,b))!=0){
    total+=n;if(total>64L*1024*1024||n%4!=0)throw new IOException("ROM must be an uncompressed 32, 54 or 64 MiB file");
    if(mode<0){if(n<32)throw new IOException("Selected file is too short");int header=(b[0]&255)<<24|(b[1]&255)<<16|(b[2]&255)<<8|(b[3]&255);mode=header==0x80371240?0:header==0x37804012?1:header==0x40123780?2:-1;if(mode<0)throw new IOException("Select an uncompressed .z64, .v64 or .n64 Majora's Mask ROM");}
    if(mode==1)for(int i=0;i<n;i+=2){byte t=b[i];b[i]=b[i+1];b[i+1]=t;}
    if(mode==2)for(int i=0;i<n;i+=4){byte t=b[i];b[i]=b[i+3];b[i+3]=t;t=b[i+1];b[i+1]=b[i+2];b[i+2]=t;}
    if(total==n)id=((long)(b[16]&255)<<24)|((long)(b[17]&255)<<16)|((long)(b[18]&255)<<8)|(b[19]&255);
    crc.update(b,0,n);out.write(b,0,n);
   }
   out.getFD().sync();
  }
  if(total!=32L*1024*1024&&total!=54L*1024*1024&&total!=64L*1024*1024)throw new IOException("Unsupported ROM size");
  if(id==0x5354631cL&&crc.getValue()==0x96f49400L)return "N64_US";
  if(id==0xb443eb08L&&crc.getValue()==0xbb434787L)return "GC_US";
  throw new IOException("Unsupported or modified ROM. Use Majora's Mask US 1.0 or US GameCube; texture packs can be added separately");
 }
}
