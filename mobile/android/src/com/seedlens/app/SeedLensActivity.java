package com.seedlens.app;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Build;
import android.graphics.Rect;
import android.view.View;
import android.view.WindowInsets;
import java.io.File;
import java.io.FileInputStream;
import java.io.OutputStream;
import org.libsdl.app.SDLActivity;

public final class SeedLensActivity extends SDLActivity {
    private File pendingExport;
    private static final int EXPORT_DOCUMENT=501;
    public static native void nativeExportResult(String message);
    public static native void nativeKeyboardHeight(int height);

    @Override protected String[] getLibraries(){return new String[]{"SDL3","main"};}
    @Override protected String[] getArguments(){
        if(getIntent().getBooleanExtra("self_test",false))return new String[]{"--self-test"};
        String scene=getIntent().getStringExtra("scene");
        return scene==null?new String[]{}:new String[]{"--scene",scene};
    }
    @Override protected void onCreate(Bundle state){
        super.onCreate(state);
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR|View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
        View decor=getWindow().getDecorView();
        decor.getViewTreeObserver().addOnGlobalLayoutListener(()->{
            int height=0;
            if(Build.VERSION.SDK_INT>=30){
                WindowInsets insets=decor.getRootWindowInsets();
                if(insets!=null)height=insets.getInsets(WindowInsets.Type.ime()).bottom;
            }else{
                Rect visible=new Rect();decor.getWindowVisibleDisplayFrame(visible);
                int covered=decor.getRootView().getHeight()-visible.bottom;
                if(covered>decor.getRootView().getHeight()/5)height=covered;
            }
            nativeKeyboardHeight(height);
        });
    }
    public void exportFile(String path,String mime){
        runOnUiThread(()->{
            pendingExport=new File(path);
            Intent intent=new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType(mime);
            intent.putExtra(Intent.EXTRA_TITLE,pendingExport.getName());
            startActivityForResult(intent,EXPORT_DOCUMENT);
        });
    }
    @Override protected void onActivityResult(int request,int result,Intent data){
        if(request!=EXPORT_DOCUMENT){super.onActivityResult(request,result,data);return;}
        File file=pendingExport;pendingExport=null;
        if(result!=RESULT_OK||data==null||data.getData()==null||file==null){nativeExportResult("保存をキャンセルしました / Cancelled");return;}
        Uri destination=data.getData();
        new Thread(()->{
            try(FileInputStream input=new FileInputStream(file);
                OutputStream output=getContentResolver().openOutputStream(destination,"wt")){
                if(output==null)throw new java.io.IOException("No output stream");
                byte[] bytes=new byte[65536];int length;
                while((length=input.read(bytes))>=0)output.write(bytes,0,length);
                nativeExportResult("保存しました / Saved");
            }catch(Exception error){nativeExportResult("保存できませんでした / Could not save");}
        },"SeedLens export").start();
    }
}
