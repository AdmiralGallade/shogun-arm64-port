package net.int13;

import android.app.Activity;
import android.util.Log;

/**
 * The class the engine actually looks for.
 *
 * libHAL.Android.so does FindClass("net/int13/HalActivity") and then
 * GetMethodID for each callback below, by exact name and signature. Those
 * signatures were read out of the original classes.dex, so this is the 2012
 * interface reproduced rather than invented. ShogunActivity extends this, so
 * the object handed to the guest genuinely is a HalActivity and the method IDs
 * resolve against it.
 */
public class HalActivity extends Activity {
    private static final String T = "shogun";

    /** Buffer size in samples and sample rate in Hz -- in that order. */
    protected volatile int audioSamples = 512;
    protected volatile int audioHz = 22050;

    // ---- audio -------------------------------------------------------------
    /** initAudio(II)Z is (bufferSize, sampleRateHz). Order matters: reversed,
     *  the mixer gets a buffer of the wrong length and walks off its stream
     *  table into a null codec pointer. */
    public boolean initAudio(int bufferSize, int rateHz) {
        Log.i(T, "cb initAudio(bufferSize=" + bufferSize + ", rateHz=" + rateHz + ")");
        audioSamples = bufferSize;
        audioHz = rateHz;
        return true;
    }

    public void startAudio() { Log.i(T, "cb startAudio"); }
    public void pauseAudio() { Log.i(T, "cb pauseAudio"); }
    public void closeAudio() { Log.i(T, "cb closeAudio"); }
    public void fillAudio(byte[] buffer) {}

    // ---- lifecycle / misc --------------------------------------------------
    /** The engine asks the app to close. It called this spuriously while the
     *  archive was failing to open; with the pack loading it means what it
     *  says, so honour it again. */
    public void finishActivity() { Log.i(T, "cb finishActivity"); finish(); }
    public void initArchive()    { Log.i(T, "cb initArchive"); }
    public String geLang()       { Log.i(T, "cb geLang"); return "en"; }
    public void openURL(String url) { Log.i(T, "cb openURL " + url); }

    // ---- camera: int13 shipped AR titles on this engine; Shogun does not
    //      use these, but the lookups happen at init and must resolve.
    public void initCamera(int w, int h) {}
    public void startCamera() {}
    public void updateCamera() {}
    public void stopCamera() {}

    // ---- billing: the Play Billing v1 service this called is long gone -----
    public boolean canMakePurchase() { Log.i(T, "cb canMakePurchase"); return false; }
    public void purchaseItem(String sku) { Log.i(T, "cb purchaseItem " + sku); }
    public void restoreItems() { Log.i(T, "cb restoreItems"); }
}
