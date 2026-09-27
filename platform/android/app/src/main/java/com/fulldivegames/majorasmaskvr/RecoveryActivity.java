package com.fulldivegames.majorasmaskvr;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.os.Bundle;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
public final class RecoveryActivity extends Activity {
 @Override public void onCreate(Bundle state) {
  super.onCreate(state);
  new AlertDialog.Builder(this).setTitle("Recover VR settings")
   .setMessage("Close the game first. Reset VR camera, height and controls? Your ordinary saves, save states and mods will be kept.")
   .setNegativeButton("Cancel",(dialog,which)->finish())
   .setPositiveButton("Reset and open",(dialog,which)->{
    try {
     File root=getExternalFilesDir(null);
     if(root==null)throw new java.io.IOException("Storage unavailable");
     try(FileOutputStream out=new FileOutputStream(new File(root,"reset-vr-settings.request"))) {out.write(1);out.getFD().sync();}
     startActivity(new Intent(this,SetupActivity.class));finish();
    }catch(Exception error){TextView message=new TextView(this);message.setText("Cannot request recovery. Check app storage and try again.");setContentView(message);}
   }).setOnCancelListener(dialog->finish()).show();
 }
}
