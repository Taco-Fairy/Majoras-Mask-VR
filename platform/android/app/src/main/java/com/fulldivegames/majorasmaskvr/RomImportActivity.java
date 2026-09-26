package com.fulldivegames.majorasmaskvr;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.view.WindowManager;
import android.widget.*;
/** Separate process isolates the native exporter's global state from the running game. */
public final class RomImportActivity extends Activity {
 private volatile boolean busy=true;private TextView status;private Button close;
 @Override public void onCreate(Bundle state){super.onCreate(state);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
  LinearLayout layout=new LinearLayout(this);layout.setPadding(48,32,48,32);layout.setOrientation(LinearLayout.VERTICAL);
  status=new TextView(this);status.setTextSize(22);status.setText("Opening selected ROM…");layout.addView(status);
  close=new Button(this);close.setText("Back to setup");close.setEnabled(false);close.setOnClickListener(v->finish());layout.addView(close);setContentView(layout);
  new Thread(()->{try{RomExtractor.run(this,getContentResolver().openInputStream(getIntent().getData()),this::show);runOnUiThread(()->{setResult(RESULT_OK);finish();});}
   catch(Throwable e){show("ROM import failed: "+(e.getMessage()==null?e.getClass().getSimpleName():e.getMessage()));}
   finally{busy=false;runOnUiThread(()->close.setEnabled(true));}},"MMVR-ROM-import").start();
 }
 private void show(String text){runOnUiThread(()->status.setText(text));}
 @Override public void onBackPressed(){if(!busy)super.onBackPressed();}
}
