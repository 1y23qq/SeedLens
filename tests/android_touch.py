import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_android as build

JAVA=r'''
package com.seedlens.smoke;
import android.app.*;
import android.content.*;
import android.os.*;
import android.view.*;
import android.util.DisplayMetrics;
import android.accessibilityservice.AccessibilityService;
import java.io.*;
import java.nio.file.Files;

public final class Smoke extends Instrumentation {
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    private void pointer(UiAutomation ui,long down,int action,float[] x,float[] y){
        MotionEvent.PointerProperties[] p=new MotionEvent.PointerProperties[x.length];
        MotionEvent.PointerCoords[] c=new MotionEvent.PointerCoords[x.length];
        for(int i=0;i<x.length;i++){
            p[i]=new MotionEvent.PointerProperties();p[i].id=i;p[i].toolType=MotionEvent.TOOL_TYPE_FINGER;
            c[i]=new MotionEvent.PointerCoords();c[i].x=x[i];c[i].y=y[i];c[i].pressure=1;c[i].size=1;
        }
        MotionEvent e=MotionEvent.obtain(down,SystemClock.uptimeMillis(),action,x.length,p,c,0,0,1,1,0,0,InputDevice.SOURCE_TOUCHSCREEN,0);
        if(!ui.injectInputEvent(e,true))throw new RuntimeException("Touch injection failed");
        e.recycle();
    }
    private double scale()throws Exception{
        File f=new File(getTargetContext().getFilesDir(),"session.ini");
        for(String line:Files.readAllLines(f.toPath()))if(line.startsWith("view "))return Double.parseDouble(line.split(" ")[3]);
        throw new IOException("Missing session view");
    }
    @Override public void onStart(){
        Bundle result=new Bundle();
        try{
            Intent intent=new Intent().setClassName("com.seedlens.app","com.seedlens.app.SeedLensActivity").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            Activity activity=startActivitySync(intent);
            Thread.sleep(1800);
            double before=scale();
            DisplayMetrics metrics=new DisplayMetrics();activity.getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
            float w=metrics.widthPixels,h=metrics.heightPixels,y=h*.45f;
            boolean zoomIn=before>=.6;
            float start=zoomIn?.10f:.22f,end=zoomIn?.22f:.10f;
            UiAutomation ui=getUiAutomation();long down=SystemClock.uptimeMillis();
            pointer(ui,down,MotionEvent.ACTION_DOWN,new float[]{w*(.5f-start)},new float[]{y});
            pointer(ui,down,MotionEvent.ACTION_POINTER_DOWN|(1<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),new float[]{w*(.5f-start),w*(.5f+start)},new float[]{y,y});
            for(int i=1;i<=24;i++){
                float d=start+(end-start)*i/24;
                pointer(ui,down,MotionEvent.ACTION_MOVE,new float[]{w*(.5f-d),w*(.5f+d)},new float[]{y,y});
                Thread.sleep(16);
            }
            pointer(ui,down,MotionEvent.ACTION_POINTER_UP|(1<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),new float[]{w*(.5f-end),w*(.5f+end)},new float[]{y,y});
            pointer(ui,down,MotionEvent.ACTION_UP,new float[]{w*(.5f-end)},new float[]{y});
            Thread.sleep(1000);
            File image=new File(getTargetContext().getFilesDir(),"touch-test.png");
            try(FileOutputStream output=new FileOutputStream(image)){ui.takeScreenshot().compress(android.graphics.Bitmap.CompressFormat.PNG,100,output);}
            ui.performGlobalAction(AccessibilityService.GLOBAL_ACTION_HOME);Thread.sleep(800);
            double after=scale();
            result.putString("before_bpp",Double.toString(before));result.putString("after_bpp",Double.toString(after));
            if(!(zoomIn?after<before*.8:after>before*1.4))throw new AssertionError("Pinch did not zoom the native map");
            File core=new File(getTargetContext().getFilesDir(),"self-test.json");
            if(core.exists())result.putString("core_report",new String(Files.readAllBytes(core.toPath()),"UTF-8").trim());
            result.putString("result","PASS");finish(Activity.RESULT_OK,result);
        }catch(Throwable error){
            result.putString("result","FAIL: "+error);finish(Activity.RESULT_CANCELED,result);
        }
    }
}
'''

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--serial',required=True)
    parser.add_argument('--server-port',default='5037')
    args=parser.parse_args()
    tools,android_jar,_,_,_=build.find_tools()
    adb=next((build.CACHE/'sdk-platform-tools').rglob('adb.exe'))
    base=[str(adb),'-P',args.server_port,'-s',args.serial]
    folder=ROOT/'build-mobile/smoke';folder.mkdir(parents=True,exist_ok=True)
    source=folder/'Smoke.java';source.write_text(JAVA,encoding='utf-8')
    manifest=folder/'AndroidManifest.xml'
    manifest.write_text('''<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.seedlens.smoke">
<uses-sdk android:minSdkVersion="26" android:targetSdkVersion="36"/>
<application android:label="SeedLens touch test" android:testOnly="true"/>
<instrumentation android:name="com.seedlens.smoke.Smoke" android:targetPackage="com.seedlens.app"/>
</manifest>''',encoding='utf-8')
    unsigned=folder/'test-unsigned.apk'
    build.run([tools/('aapt2'+build.SUFFIX),'link','-o',build.relative(unsigned),'-I',android_jar,'--manifest',build.relative(manifest)])
    classes=folder/'classes';classes.mkdir(exist_ok=True)
    build.run(['javac','-encoding','UTF-8','-source','8','-target','8','-cp',android_jar,'-d',classes,source])
    dex=folder/'dex';dex.mkdir(exist_ok=True)
    build.run(['java','-cp',tools/'lib/d8.jar','com.android.tools.r8.D8','--release','--min-api','26','--lib',android_jar,'--output',dex,*classes.rglob('*.class')])
    import zipfile
    with zipfile.ZipFile(unsigned,'a',zipfile.ZIP_DEFLATED) as z:z.write(dex/'classes.dex','classes.dex')
    aligned=folder/'test-aligned.apk'
    build.run([tools/('zipalign'+build.SUFFIX),'-f','4',build.relative(unsigned),build.relative(aligned)])
    apk=build.sign(aligned,tools,folder/'SeedLens-touch-test.apk')
    def adb_run(*command):
        r=subprocess.run(base+list(command),capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=45)
        if r.returncode:raise RuntimeError(r.stdout+r.stderr)
        return r.stdout
    print(adb_run('install','-r','-t',str(apk)).strip())
    adb_run('shell','am','force-stop','com.seedlens.app')
    output=adb_run('shell','am','instrument','-w','com.seedlens.smoke/.Smoke')
    print(output)
    artifact=ROOT/'artifacts/mobile/android-touch.json';artifact.parent.mkdir(parents=True,exist_ok=True)
    artifact.write_text(json.dumps({'output':output},indent=2)+'\n',encoding='utf-8')
    if 'INSTRUMENTATION_RESULT: result=PASS' not in output:raise RuntimeError('Native pinch check failed')

if __name__=='__main__':main()
