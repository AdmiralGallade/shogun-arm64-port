package net.int13.shogun;

import android.content.res.AssetFileDescriptor;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.MotionEvent;
import android.view.WindowManager;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Mirrors the original net.int13.HalActivity closely enough for the engine to
 * feel at home: a GLES 1.x surface, touch forwarding, and an AudioTrack thread
 * that pulls PCM out of the software mixer.
 */
public class ShogunActivity extends net.int13.HalActivity {
    private static final String TAG = "shogun";

    /** The game is authored for this; everything else is letterboxing. */
    static final int GAME_W = 480, GAME_H = 800;

    private GLSurfaceView view;
    private AudioThread audio;
    private Leaderboard leaderboard;

    // letterbox rect inside the surface, filled in on surfaceChanged
    private volatile int vpX, vpY, vpW = GAME_W, vpH = GAME_H;

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        view = new GLSurfaceView(this);
        // No setEGLContextClientVersion: the default is a GLES 1.x context,
        // which is exactly what this engine wants.
        //
        // Ask to keep that context across a minimise. By default GLSurfaceView
        // destroys it in onPause, and the engine is never told: it comes back
        // still holding texture and buffer names that now refer to nothing, and
        // draws the whole game untextured. This is a request the driver may
        // refuse, so Renderer.onSurfaceCreated also handles being called twice.
        view.setPreserveEGLContextOnPause(true);
        view.setRenderer(new Renderer());
        view.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        setContentView(view);
    }

    private byte[] readAsset(String name) throws Exception {
        try (InputStream in = getAssets().open(name)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream(1 << 20);
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            return out.toByteArray();
        }
    }

    private final class Renderer implements GLSurfaceView.Renderer {
        private boolean started;

        @Override public void onSurfaceCreated(GL10 gl, EGLConfig cfg) {
            // A second call means the context was not preserved after all.
            // Do NOT re-run nativeInit -- that would restart the engine and
            // throw the session away. Rebuild the GL objects underneath it.
            if (started) {
                Log.w(TAG, "surface recreated: restoring GL objects");
                ShogunNative.nativeSurfaceRecreated();
                return;
            }
            try {
                byte[] lib = readAsset("libHAL.Android.so");
                // The pack must be a standalone file. Inside the APK it sits at
                // an offset, but the engine seeks absolutely -- it treats the
                // archive as if it owned the whole file -- so an offset asset
                // feeds the LZMA decoder bytes from the wrong place. Extract
                // once to app storage and hand it over at offset 0.
                java.io.File pak = new java.io.File(getFilesDir(), "data.mp3");
                if (!pak.exists() || pak.length() == 0) {
                    Log.i(TAG, "extracting asset pack ...");
                    try (InputStream in = getAssets().open("data.mp3",
                                 android.content.res.AssetManager.ACCESS_STREAMING);
                         java.io.FileOutputStream out = new java.io.FileOutputStream(pak)) {
                        byte[] b = new byte[1 << 16];
                        int n;
                        while ((n = in.read(b)) > 0) out.write(b, 0, n);
                    }
                    Log.i(TAG, "extracted " + pak.length() + " bytes");
                }
                boolean ok = ShogunNative.nativeInit(
                        ShogunActivity.this, lib,
                        pak.getAbsolutePath(), pak.length(),
                        getFilesDir().getAbsolutePath(), GAME_W, GAME_H);
                Log.i(TAG, "engine boot: " + ok);
                started = ok;
                if (ok) { startAudio(); startLeaderboard(); }
            } catch (Exception e) {
                Log.e(TAG, "boot failed", e);
            }
        }

        @Override public void onSurfaceChanged(GL10 gl, int w, int h) {
            // The engine never calls glViewport (it is not among its imports),
            // so a viewport set once here survives and does the letterboxing
            // for us -- no offscreen target needed on device.
            float scale = Math.min(w / (float) GAME_W, h / (float) GAME_H);
            vpW = (int) (GAME_W * scale);
            vpH = (int) (GAME_H * scale);
            vpX = (w - vpW) / 2;
            vpY = (h - vpH) / 2;
            gl.glViewport(vpX, vpY, vpW, vpH);
            Log.i(TAG, "viewport " + vpW + "x" + vpH + " at " + vpX + "," + vpY);
        }

        private long nextFrameNs;

        @Override public void onDrawFrame(GL10 gl) {
            if (!started) return;
            ShogunNative.nativeTick();
            // Pace to 60 fps. The engine was written for it, and on a 120 Hz
            // panel an unthrottled loop burns twice the CPU for no benefit --
            // CPU the software mixer needs to keep the audio track fed.
            final long FRAME_NS = 16_666_667L;
            long now = System.nanoTime();
            if (nextFrameNs == 0) nextFrameNs = now;
            nextFrameNs += FRAME_NS;
            long sleep = nextFrameNs - now;
            if (sleep > 0) {
                try { Thread.sleep(sleep / 1_000_000L, (int) (sleep % 1_000_000L)); }
                catch (InterruptedException ignored) {}
            } else if (sleep < -4 * FRAME_NS) {
                nextFrameNs = now;            // fell far behind; resynchronise
            }
        }
    }

    /** Maps a screen touch back through the letterbox into game coordinates. */
    @Override public boolean onTouchEvent(MotionEvent e) {
        if (vpW == 0 || vpH == 0) return true;
        final int gx = (int) ((e.getX() - vpX) * GAME_W / vpW);
        final int gy = (int) ((e.getY() - vpY) * GAME_H / vpH);
        final int action;
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: action = 0; break;
            case MotionEvent.ACTION_MOVE: action = 1; break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: action = 2; break;
            default: return true;
        }
        // Touches must reach the guest on the same thread that owns the CPU.
        view.queueEvent(new Runnable() {
            @Override public void run() { ShogunNative.nativeTouch(action, gx, gy); }
        });
        return true;
    }

    // ---------------------------------------------------------------- audio
    /** The engine calls this on us when it wants sound; we also call it once
     *  after boot in case playback is already armed. */
    @Override public void startAudio() {
        super.startAudio();          // keeps the callback visible in the log
        if (audio == null) {
            audio = new AudioThread();
            audio.start();
        }
    }

    /**
     * The engine mixes in software and hands back finished PCM, so there is no
     * audio API to emulate -- only a buffer to move. It asks for 512-sample
     * buffers at 22050 Hz; note that initAudio(II)Z is (bufferSize, rateHz),
     * in that order.
     */
    private final class AudioThread extends Thread {
        private volatile boolean running = true;
        private static final int BATCH = 4;      // chunks handed over per pass

        @Override public void run() {
            android.os.Process.setThreadPriority(
                    android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);

            // The engine calls startAudio() itself, from inside onInit -- so
            // this thread can start before nativeInit has finished booting.
            // Waiting here rather than bailing out is the whole difference
            // between music and silence: an early exit never comes back.
            while (running && !ShogunNative.nativeBooted()) {
                try { Thread.sleep(10); } catch (InterruptedException ignored) {}
            }
            if (!running) return;

            // Read the negotiated parameters now, not at construction time.
            final int rate = audioHz, samples = audioSamples;
            Log.i(TAG, "audio thread: " + rate + " Hz, " + samples + "-sample chunks");

            int min = AudioTrack.getMinBufferSize(rate,
                    AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT);
            // One chunk is only ~23 ms. Guest execution is serialised behind
            // the render thread, so give the track enough depth to ride out a
            // slow frame instead of underrunning into noise.
            int size = Math.max(min, samples * 2 * BATCH * 4);
            AudioTrack track = new AudioTrack.Builder()
                    .setAudioAttributes(new AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_GAME)
                            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
                    .setAudioFormat(new AudioFormat.Builder()
                            .setSampleRate(rate)
                            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                            .setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build())
                    .setBufferSizeInBytes(size)
                    .setTransferMode(AudioTrack.MODE_STREAM)
                    .build();

            byte[] chunk = new byte[samples * 2];
            byte[] batch = new byte[samples * 2 * BATCH];

            // Fill the track before starting playback so the first sound is
            // not already chasing an empty buffer.
            for (int i = 0; i < BATCH && running; i++) {
                ShogunNative.nativeAudio(chunk, samples);
                track.write(chunk, 0, chunk.length);
            }
            track.play();
            Log.i(TAG, "audio playing, buffer " + size + " bytes");

            long silent = 0;
            while (running) {
                for (int i = 0; i < BATCH; i++) {
                    ShogunNative.nativeAudio(chunk, samples);
                    System.arraycopy(chunk, 0, batch, i * chunk.length, chunk.length);
                }
                int wrote = track.write(batch, 0, batch.length);
                if (wrote < 0) { Log.w(TAG, "AudioTrack.write -> " + wrote); break; }
                if (++silent == 400) {          // ~15 s health line
                    int nz = 0;
                    for (byte b : batch) if (b != 0) nz++;
                    Log.i(TAG, "audio alive, " + nz + "/" + batch.length + " nonzero");
                    silent = 0;
                }
            }
            track.stop();
            track.release();
        }

        void quit() { running = false; }
    }

    /**
     * Hand work to the GL thread and wait for it, because that thread owns the
     * emulated CPU. Returns once it has run, or after the timeout if the
     * thread is already gone.
     */
    private void runOnGlThread(Runnable r, long timeoutMs) {
        if (view == null) return;
        final java.util.concurrent.CountDownLatch done =
                new java.util.concurrent.CountDownLatch(1);
        view.queueEvent(new Runnable() {
            @Override public void run() {
                try { r.run(); } finally { done.countDown(); }
            }
        });
        try { done.await(timeoutMs, java.util.concurrent.TimeUnit.MILLISECONDS); }
        catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
    }

    /** Started once the engine is up; harmless when no endpoint is set. */
    private void startLeaderboard() {
        if (leaderboard != null) return;
        leaderboard = new Leaderboard(Leaderboard.endpointFor(this));
        if (leaderboard.enabled()) leaderboard.start();
    }

    @Override protected void onPause() {
        super.onPause();
        // The engine has exactly one lifecycle hook -- onApplicationPause --
        // and nothing else in it ever writes the save file. Leaving without
        // calling it is why progress and the tutorial flag never survived a
        // close. It must run before view.onPause() stops the GL thread, and on
        // that thread, because that is where the guest CPU lives.
        runOnGlThread(new Runnable() {
            @Override public void run() {
                ShogunNative.nativePause();
                ShogunNative.nativeSave();
            }
        }, 2000);
        if (view != null) view.onPause();
        if (audio != null) { audio.quit(); audio = null; }
    }

    @Override protected void onResume() {
        super.onResume();
        if (view != null) view.onResume();
        // onPause tore the audio thread down; the engine has no resume hook to
        // rebuild it, so without this the game comes back silent.
        if (audio == null && ShogunNative.nativeBooted()) startAudio();
    }

}
