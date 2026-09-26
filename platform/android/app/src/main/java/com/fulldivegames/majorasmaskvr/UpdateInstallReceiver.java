package com.fulldivegames.majorasmaskvr;
import android.content.*;
import android.content.pm.PackageInstaller;
public final class UpdateInstallReceiver extends BroadcastReceiver {
 @Override public void onReceive(Context context,Intent intent){
  int status=intent.getIntExtra(PackageInstaller.EXTRA_STATUS,PackageInstaller.STATUS_FAILURE);
  if(status==PackageInstaller.STATUS_PENDING_USER_ACTION){
   Intent confirm=intent.getParcelableExtra(Intent.EXTRA_INTENT);
   if(confirm==null){GameActivity.updateResult("Android did not provide an update confirmation. Try Install update again.");return;}
   try{confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);context.startActivity(confirm);}
   catch(RuntimeException error){GameActivity.updateResult("Cannot open Android update confirmation: "+error.getMessage());}
  }else GameActivity.updateResult(status==PackageInstaller.STATUS_SUCCESS?"Update installed.":"Android update failed: "+intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE));
 }
}
