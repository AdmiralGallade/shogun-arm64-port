package net.int13.shogun;

import android.content.res.AssetFileDescriptor;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.graphics.Color;
import android.graphics.Typeface;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

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

    /** Set by the audio thread once it starts, for the performance hint. */
    private volatile int audioTid;
    /** android.os.PerformanceHintManager.Session, API 31+. */
    private Object hintSession;
    /** Render-thread id the session was built for; 0 when there is none. */
    private int hintGlTid;
    /** Deadline the session currently believes in. */
    private long hintTargetNs;
    private boolean hintUnavailable;

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

        // The engine boots on the GL thread and draws nothing until it is up,
        // which on a first launch (unpacking the 19 MB pack and the worlds)
        // takes many seconds. A plain Android view covers that instead of a
        // black screen, and says what this app is while it waits.
        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.BLACK);
        root.addView(view);
        // The view's size on the screen (the GL buffer may be smaller: fitBuffer)
        view.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override public void onLayoutChange(View v, int l, int t, int r, int b,
                                                 int ol, int ot, int or, int ob) {
                if (r - l == screenW && b - t == screenH) return;
                screenW = r - l;
                screenH = b - t;
                computeViewport();
            }
        });
        loading = new LoadingView(this);
        root.addView(loading);
        root.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override public WindowInsets onApplyWindowInsets(View v, WindowInsets in) {
                readSafeInsets(in);
                return in;
            }
        });
        setContentView(root);
        startUnpack();
    }

    // ------------------------------------------------------- unpacking
    // The pack and the worlds are copied out of the APK on a thread of their
    // own, never on the GL thread. GLSurfaceView makes the UI thread wait for
    // the GL thread whenever the window changes (focus, insets, a dialog, a
    // pause), so the 30-odd seconds of copying after an update, done there,
    // stalled the UI thread past the 5 s limit: "Shogun 64 isn't responding",
    // again on every relaunch while the copy started over. One per process:
    // an activity recreated meanwhile waits for the same copy.
    private static Thread unpacker;
    private static volatile boolean unpacked;
    private static volatile String unpackError;
    private static volatile byte[] halLib;

    private void startUnpack() {
        synchronized (ShogunActivity.class) {
            if (unpacker != null) return;
            final android.content.Context app = getApplicationContext();
            unpacker = new Thread(new Runnable() {
                @Override public void run() {
                    android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_DEFAULT);
                    final long t0 = System.nanoTime();
                    try {
                        halLib = readAsset("libHAL.Android.so");
                        // The pack must be a standalone file. Inside the APK it sits
                        // at an offset, but the engine seeks absolutely -- it treats
                        // the archive as if it owned the whole file -- so an offset
                        // asset feeds the LZMA decoder bytes from the wrong place.
                        // Extract once to app storage and hand it over at offset 0.
                        java.io.File pak = new java.io.File(app.getFilesDir(), "data.mp3");
                        if (!pak.exists() || pak.length() == 0) {
                            Log.i(TAG, "extracting asset pack ...");
                            loadingStatus("Unpacking the game (first launch only)…");
                            try (InputStream in = getAssets().open("data.mp3",
                                         android.content.res.AssetManager.ACCESS_STREAMING);
                                 java.io.FileOutputStream out = new java.io.FileOutputStream(pak)) {
                                byte[] b = new byte[1 << 16];
                                int n;
                                while ((n = in.read(b)) > 0) out.write(b, 0, n);
                            }
                            Log.i(TAG, "extracted " + pak.length() + " bytes");
                        }
                        extractWorlds();
                    } catch (Exception e) {
                        Log.e(TAG, "unpack failed", e);
                        unpackError = String.valueOf(e.getMessage());
                    }
                    Log.i(TAG, "unpacked in " + (System.nanoTime() - t0) / 1_000_000L + " ms");
                    unpacked = true;
                }
            }, "shogun-unpack");
            unpacker.start();
        }
    }

    // ------------------------------------------------------- loading screen
    private LoadingView loading;
    /** Engine frames since boot; the cover comes off once the game is drawing. */
    private int ticksSinceBoot;
    /** Enough frames for the title screen to be up, even on a first launch. */
    private static final int TICKS_BEFORE_REVEAL = 45;

    /** Title, what is happening, and the line saying what this app is. */
    private static final class LoadingView extends LinearLayout {
        private final TextView status;

        LoadingView(android.content.Context c) {
            super(c);
            setOrientation(VERTICAL);
            setGravity(Gravity.CENTER);
            setBackgroundColor(Color.BLACK);
            final float dp = c.getResources().getDisplayMetrics().density;
            final int pad = (int) (24 * dp);
            setPadding(pad, pad, pad, pad);

            TextView title = new TextView(c);
            title.setText("SHOGUN 64");
            title.setTextColor(0xFF4FE0F0);
            title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 34);
            title.setTypeface(Typeface.DEFAULT_BOLD);
            title.setLetterSpacing(0.12f);
            title.setGravity(Gravity.CENTER);
            addView(title);

            ProgressBar spin = new ProgressBar(c);
            LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(
                    (int) (40 * dp), (int) (40 * dp));
            sp.topMargin = (int) (28 * dp);
            sp.bottomMargin = (int) (12 * dp);
            addView(spin, sp);

            status = new TextView(c);
            status.setTextColor(0xFFB0B8C0);
            status.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
            status.setGravity(Gravity.CENTER);
            status.setText("Starting…");
            addView(status);

            TextView note = new TextView(c);
            note.setTextColor(0xFF6A737C);
            note.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
            note.setGravity(Gravity.CENTER);
            note.setText(ABOUT_SHORT);
            LinearLayout.LayoutParams np = new LinearLayout.LayoutParams(
                    LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            np.topMargin = (int) (48 * dp);
            addView(note, np);
        }

        void setStatus(String s) { status.setText(s); }
    }

    /** From any thread: update the cover's status line. */
    private void loadingStatus(final String s) {
        runOnUiThread(new Runnable() { @Override public void run() {
            if (loading != null) loading.setStatus(s);
        }});
    }

    /** From any thread: fade the cover out and drop it. */
    private void reveal() {
        runOnUiThread(new Runnable() { @Override public void run() {
            final LoadingView l = loading;
            if (l == null) return;
            loading = null;
            l.animate().alpha(0f).setDuration(250).withEndAction(new Runnable() {
                @Override public void run() {
                    if (l.getParent() instanceof ViewGroup) ((ViewGroup) l.getParent()).removeView(l);
                }
            });
        }});
    }

    // ---------------------------------------------------- safe-area insets
    /**
     * Camera cutout and any visible system bars, in surface pixels. The game is
     * letterboxed inside what is left, so on a tall phone it never sits under
     * the camera or the gesture bar (Android 15+ always draws apps edge to
     * edge, whatever the theme asks for).
     */
    private volatile int safeL, safeT, safeR, safeB;
    private volatile boolean viewportDirty;
    private volatile int surfaceW, surfaceH;   // the GL buffer (smaller than the screen: RENDER_SCALE)
    private volatile int screenW, screenH;     // the view on the screen: insets, letterbox, touches

    /**
     * The GL buffer is at most this many times the game's 480 x 800, the
     * display scaling it the rest of the way to the screen. Drawn at the
     * phone's own resolution (~1280 x 2133 of game on a Pixel 10 Pro) every
     * sprite cost the GPU ~7x its native fill: big sprites and explosions
     * together (the Valhalla over Paris, Mission 15) ran it to 45 ms frames
     * (2026-10-27). Two is a whole-number scale of the art, so it stays as
     * crisp as the game draws it; the rest is the display's own scaler.
     */
    static final float RENDER_SCALE = 2.0f;
    private int fixedW, fixedH;                // the buffer size asked for (UI thread)

    private void readSafeInsets(WindowInsets in) {
        int l, t, r, b;
        if (android.os.Build.VERSION.SDK_INT >= 30) {
            android.graphics.Insets i = in.getInsets(WindowInsets.Type.displayCutout()
                    | WindowInsets.Type.systemBars());
            l = i.left; t = i.top; r = i.right; b = i.bottom;
        } else {
            l = in.getSystemWindowInsetLeft(); t = in.getSystemWindowInsetTop();
            r = in.getSystemWindowInsetRight(); b = in.getSystemWindowInsetBottom();
            if (android.os.Build.VERSION.SDK_INT >= 28 && in.getDisplayCutout() != null) {
                android.view.DisplayCutout dc = in.getDisplayCutout();
                l = Math.max(l, dc.getSafeInsetLeft()); t = Math.max(t, dc.getSafeInsetTop());
                r = Math.max(r, dc.getSafeInsetRight()); b = Math.max(b, dc.getSafeInsetBottom());
            }
        }
        if (l != safeL || t != safeT || r != safeR || b != safeB) {
            safeL = l; safeT = t; safeR = r; safeB = b;
            Log.i(TAG, "safe insets l" + l + " t" + t + " r" + r + " b" + b);
            computeViewport();
        }
    }

    /** Largest 480x800 box inside the safe area, centred on the whole screen (screen pixels). */
    private void computeViewport() {
        final int w = screenW > 0 ? screenW : surfaceW, h = screenW > 0 ? screenH : surfaceH;
        if (w <= 0 || h <= 0) return;
        // Keep the box centred on the screen (so the letterbox is symmetric)
        // but small enough to clear the larger inset on each axis.
        final int availW = Math.max(1, w - 2 * Math.max(safeL, safeR));
        final int availH = Math.max(1, h - 2 * Math.max(safeT, safeB));
        float scale = Math.min(availW / (float) GAME_W, availH / (float) GAME_H);
        vpW = (int) (GAME_W * scale);
        vpH = (int) (GAME_H * scale);
        vpX = (w - vpW) / 2;
        vpY = (h - vpH) / 2;
        viewportDirty = true;
        fitBuffer(w, h);
    }

    /**
     * Ask for a GL buffer of the screen's shape whose game box is RENDER_SCALE
     * times 480 x 800 (never larger than the screen). The view keeps filling
     * the screen; the display scales the buffer up for free.
     */
    private void fitBuffer(final int w, final int h) {
        final float k = Math.min(1f, RENDER_SCALE * GAME_W / (float) Math.max(1, vpW));
        final int bw = Math.max(GAME_W, Math.round(w * k)), bh = Math.max(GAME_H, Math.round(h * k));
        if (k >= 0.999f || (bw == fixedW && bh == fixedH) || view == null) return;
        runOnUiThread(new Runnable() { @Override public void run() {
            if (bw == fixedW && bh == fixedH) return;
            fixedW = bw;
            fixedH = bh;
            view.getHolder().setFixedSize(bw, bh);
            Log.i(TAG, "render buffer " + bw + "x" + bh + " for a " + w + "x" + h + " screen");
        }});
    }

    // --------------------------------------------------------- about screen
    /** One line for the loading screen. */
    static final String ABOUT_SHORT =
            "Unofficial 64-bit port of Shogun: Bullet Hell Shooter (int13, 2012),\n"
            + "published with the developer's permission. Not affiliated with int13.";

    private boolean aboutShowing;

    /**
     * What this app is, who made what, and the licences. Opened by the in-game
     * "About and source" button (Options > Cheats), which used to go straight
     * to the repository.
     */
    void showAbout() {
        runOnUiThread(new Runnable() { @Override public void run() {
            if (aboutShowing || isFinishing()) return;
            aboutShowing = true;
            String version = "";
            try {
                version = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
            } catch (Exception ignored) {}
            String text = "Shogun 64 " + version + "\n\n"
                    + "An unofficial 64-bit port of Shogun: Bullet Hell Shooter by int13 (2012), "
                    + "so it runs on phones that can no longer run 32-bit apps. Published free, "
                    + "with no ads or purchases, with the permission of its developer, "
                    + "Stéphane Cocquereaumont. Not affiliated with int13.\n\n"
                    + "Original game, engine, art, music and story © 2012 int13. "
                    + "All rights reserved.\n\n"
                    + "64-bit port, Missions 5–12, their bosses and weapons: AdmiralGallade.\n\n"
                    + "The game runs int13's original engine inside the Unicorn CPU emulator "
                    + "(© Nguyen Anh Quynh and contributors). The port and the new missions "
                    + "are free software under the GNU General Public License v2; the source "
                    + "code is linked below.";
            new android.app.AlertDialog.Builder(ShogunActivity.this,
                    android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle("About")
                    .setMessage(text)
                    .setPositiveButton("Close", null)
                    .setNeutralButton("Licences", new android.content.DialogInterface.OnClickListener() {
                        @Override public void onClick(android.content.DialogInterface d, int w) {
                            showLicences();
                        }
                    })
                    .setNegativeButton("Source code", new android.content.DialogInterface.OnClickListener() {
                        @Override public void onClick(android.content.DialogInterface d, int w) {
                            openRepoPage();
                        }
                    })
                    .setOnDismissListener(new android.content.DialogInterface.OnDismissListener() {
                        @Override public void onDismiss(android.content.DialogInterface d) {
                            aboutShowing = false;
                        }
                    })
                    .show();
        }});
    }

    /** NOTICE, then the full GPLv2, in a scrolling monospace view. */
    private void showLicences() {
        String text;
        try {
            text = new String(readAsset("licenses/NOTICE.txt"), "UTF-8") + "\n\n"
                    + new String(readAsset("licenses/GPL-2.0.txt"), "UTF-8");
        } catch (Exception e) {
            text = "Licence text missing from this build: " + e;
        }
        final float dp = getResources().getDisplayMetrics().density;
        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tv.setTypeface(Typeface.MONOSPACE);
        tv.setTextIsSelectable(true);
        tv.setPadding((int) (20 * dp), (int) (12 * dp), (int) (20 * dp), (int) (12 * dp));
        ScrollView sv = new ScrollView(this);
        sv.addView(tv);
        new android.app.AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Licences")
                .setView(sv)
                .setPositiveButton("Close", null)
                .show();
    }

    /**
     * Worlds added to the game (docs/NEW_LEVEL.md). Each lives in assets/NAME
     * and is read by the engine through its '*' paths, which resolve against
     * the files directory -- so it is copied there. A new world only needs
     * its folder name added here.
     */
    private static final String[] WORLD_ASSETS = { "beach", "seoul", "antarctica", "edinburgh", "andaman", "sunda", "abyss", "lab", "homeward", "amaterasu", "paris", "atlantic" };

    /**
     * Copy each world's folder to the files directory, only when the APK has
     * changed: a stamp records which install the copy came from. The old copy
     * is removed first, because the engine caches each enemy script it
     * assembles as a .bin beside it and prefers that cache when it is newer
     * than the script.
     */
    private void extractWorlds() {
        String want;
        try {
            want = String.valueOf(getPackageManager()
                    .getPackageInfo(getPackageName(), 0).lastUpdateTime);
        } catch (Exception e) {
            want = "unknown";
        }
        boolean said = false;
        for (String name : WORLD_ASSETS) {
            try {
                java.io.File dst = new java.io.File(getFilesDir(), name);
                java.io.File stamp = new java.io.File(getFilesDir(), name + ".stamp");
                if (dst.isDirectory() && stamp.exists()) {
                    try (InputStream in = new java.io.FileInputStream(stamp)) {
                        if (new String(in.readAllBytes(), "UTF-8").equals(want)) continue;
                    }
                }
                if (!said) { loadingStatus("Unpacking the levels (after an update only)…"); said = true; }
                deleteTree(dst);
                int n = copyAssetTree(name, dst);
                try (java.io.FileOutputStream out = new java.io.FileOutputStream(stamp)) {
                    out.write(want.getBytes("UTF-8"));
                }
                Log.i(TAG, name + ": extracted " + n + " files");
            } catch (Exception e) {
                // without it that world has nothing to load; the rest of the
                // game is unaffected
                Log.e(TAG, name + ": extract failed", e);
            }
        }
    }

    private static void deleteTree(java.io.File f) {
        java.io.File[] kids = f.listFiles();
        if (kids != null) for (java.io.File k : kids) deleteTree(k);
        f.delete();
    }

    private int copyAssetTree(String asset, java.io.File dst) throws Exception {
        String[] kids = getAssets().list(asset);
        if (kids == null || kids.length == 0) {
            dst.getParentFile().mkdirs();
            try (InputStream in = getAssets().open(asset);
                 java.io.FileOutputStream out = new java.io.FileOutputStream(dst)) {
                byte[] b = new byte[1 << 16];
                int n;
                while ((n = in.read(b)) > 0) out.write(b, 0, n);
            }
            return 1;
        }
        dst.mkdirs();
        int total = 0;
        for (String k : kids) total += copyAssetTree(asset + "/" + k, new java.io.File(dst, k));
        return total;
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
            // This thread drives the emulator and has a frame deadline, but ran
            // at default priority while the audio thread asked for
            // URGENT_AUDIO -- so it lost every scheduling contest between them.
            android.os.Process.setThreadPriority(
                    android.os.Process.THREAD_PRIORITY_URGENT_DISPLAY);
            // A second call means the context was not preserved after all.
            // Do NOT re-run nativeInit -- that would restart the engine and
            // throw the session away. Rebuild the GL objects underneath it.
            if (started) {
                Log.w(TAG, "surface recreated: restoring GL objects");
                ShogunNative.nativeSurfaceRecreated();
                return;
            }
            // The engine boots from onDrawFrame once the unpacking is done
            // (startUnpack): this thread must never sit in a long copy.
        }

        private boolean bootTried;

        /** On the GL thread, once the pack and the worlds are on disk. */
        private void boot() {
            bootTried = true;
            if (unpackError != null || halLib == null) {
                loadingStatus("The game could not start: " + unpackError);
                return;
            }
            try {
                loadingStatus("Starting the engine…");
                final java.io.File pak = new java.io.File(getFilesDir(), "data.mp3");
                final long t0 = System.nanoTime();
                boolean ok = ShogunNative.nativeInit(
                        ShogunActivity.this, halLib,
                        pak.getAbsolutePath(), pak.length(),
                        getFilesDir().getAbsolutePath(), GAME_W, GAME_H);
                Log.i(TAG, "engine boot: " + ok + " in " + (System.nanoTime() - t0) / 1_000_000L + " ms");
                started = ok;
                if (ok) {
                    halLib = null;                    // (the engine has its own copy)
                    loadingStatus("Loading…");
                    startAudio(); startLeaderboard();
                } else {
                    loadingStatus("The game could not start on this device.");
                }
            } catch (Exception e) {
                Log.e(TAG, "boot failed", e);
                loadingStatus("The game could not start: " + e.getMessage());
            }
        }

        @Override public void onSurfaceChanged(GL10 gl, int w, int h) {
            // The engine never calls glViewport (it is not among its imports),
            // so a viewport set once here survives and does the letterboxing
            // for us -- no offscreen target needed on device.
            surfaceW = w;
            surfaceH = h;
            computeViewport();
            applyViewport(gl);
            requestSixtyHz();
            pinSixtyHzMode();
            retunePacer();
        }

        /** Period the pacer is aiming at: always a whole number of vsyncs. */
        private long framePeriodNs = 16_666_667L;
        private long nextFrameNs;
        private long lastRetuneNs;

        /**
         * Pace to 60 fps, but only ever to a whole number of vsync intervals.
         *
         * Sleeping to a period the display cannot present on is worse than
         * choosing a slower one it can. On a 60 or 120 Hz panel 16.667 ms is a
         * whole number of vsyncs, so every frame lands on one. On the Pixel 7's
         * 90 Hz panel it is not: vsyncs are 11.111 ms apart, so a frame
         * finished at 16.7 ms cannot be shown until 22.2 ms. The pacer then
         * runs late, stops sleeping, free-runs at 90 fps until it is ahead
         * again, and starts over. The delivered rate oscillates between 45 and
         * 90 fps, and that beat is the stutter -- the frames are all being
         * drawn, just not shown evenly.
         *
         * So ask the panel what it is really doing and pick the multiple
         * closest to 60: one vsync at 60 Hz, two at 90 Hz (45 fps) or 120 Hz
         * (60 fps). A steady 45 fps looks smooth; 45 beating against 90 does
         * not.
         */
        private void retunePacer() {
            float hz = 60f;
            try {
                android.view.Display d = view.getDisplay();
                if (d != null && d.getRefreshRate() > 20f) hz = d.getRefreshRate();
            } catch (Throwable ignored) {
                // Never let a display query stop the game from drawing.
            }
            final int n = Math.max(1, Math.round(hz / 60f));
            final long period = (long) (n * 1_000_000_000.0 / hz);
            if (period != framePeriodNs) {
                Log.i(TAG, "pacing: panel " + hz + " Hz, present every " + n
                        + " vsync = " + Math.round(1e9 / period) + " fps");
                framePeriodNs = period;
                nextFrameNs = 0;              // resynchronise onto the new cadence
            }
        }

        /** Insets can arrive after the surface; GL calls must be on this thread. */
        private void applyViewport(GL10 gl) {
            viewportDirty = false;
            // The letterbox is in screen pixels; the buffer may be smaller
            // (fitBuffer): scaled into it. GL's coordinates are bottom-up.
            final int sw = screenW > 0 ? screenW : surfaceW, sh = screenW > 0 ? screenH : surfaceH;
            final float kx = surfaceW / (float) Math.max(1, sw), ky = surfaceH / (float) Math.max(1, sh);
            final int x = Math.round(vpX * kx), w = Math.round(vpW * kx);
            final int h = Math.round(vpH * ky), y = surfaceH - Math.round(vpY * ky) - h;
            gl.glViewport(x, y, w, h);
            Log.i(TAG, "viewport " + w + "x" + h + " at " + x + "," + y + " in a " + surfaceW + "x" + surfaceH
                    + " buffer (screen box " + vpW + "x" + vpH + ")");
        }

        @Override public void onDrawFrame(GL10 gl) {
            if (!started) {
                if (!bootTried && unpacked) boot();
                else {                                // waiting: don't spin the GPU
                    try { Thread.sleep(16); } catch (InterruptedException ignored) {}
                }
                return;
            }
            if (viewportDirty) applyViewport(gl);
            final long workStart = System.nanoTime();
            ShogunNative.nativeTick();
            if (++ticksSinceBoot == TICKS_BEFORE_REVEAL) reveal();
            if (ShogunNative.nativeTakeOpenGithub()) showAbout();
            if (ShogunNative.nativeTakeBugReport()) {
                final String ctx = ShogunNative.nativeBugContext();
                runOnUiThread(new Runnable() { @Override public void run() {
                    BugReport.ask(ShogunActivity.this, ctx);
                }});
            }

            long now = System.nanoTime();
            reportWork(now - workStart, framePeriodNs);
            // The panel can change mode underneath us: Smooth Display, battery
            // saver, or the 60 Hz request above being granted a moment late.
            // Re-read it about once a second rather than trusting boot time.
            if (now - lastRetuneNs > 1_000_000_000L) {
                lastRetuneNs = now;
                requestSixtyHz();
                retunePacer();
                now = System.nanoTime();
            }
            if (nextFrameNs == 0) nextFrameNs = now;
            nextFrameNs += framePeriodNs;
            // Wake a little before the target and let eglSwapBuffers do the
            // final alignment. Sleeping the whole period puts us in a photo
            // finish with the vsync we are aiming for, and losing it costs a
            // whole frame.
            long sleep = nextFrameNs - now - 2_000_000L;
            if (sleep > 0) {
                try { Thread.sleep(sleep / 1_000_000L, (int) (sleep % 1_000_000L)); }
                catch (InterruptedException ignored) {}
            } else if (sleep < -4 * framePeriodNs) {
                nextFrameNs = now;            // fell far behind; resynchronise
            }
        }
    }

    /**
     * Ask the display for 60 Hz.
     *
     * FRAME_RATE_COMPATIBILITY_FIXED_SOURCE is exactly the right signal: it
     * says this content has one true rate and the platform should pick a mode
     * that divides into it. The Pixel 7 panel has a real 60 Hz mode, so it can
     * grant this, and then nothing beats against anything. It is a request and
     * not a guarantee, which is why retunePacer still measures what we got.
     */
    /**
     * Pin the window to the panel's real 60 Hz mode.
     *
     * setFrameRate below is only a vote, and it showed: granted on one launch
     * and ignored on the next, which is the whole difference between a steady
     * 60 fps and the 45 the pacer falls back to. preferredDisplayModeId is the
     * stronger statement -- it names the mode outright -- so do both.
     */
    private void pinSixtyHzMode() {
        if (android.os.Build.VERSION.SDK_INT < 23) return;
        runOnUiThread(new Runnable() { @Override public void run() {
            try {
                android.view.Display d = getWindowManager().getDefaultDisplay();
                android.view.Display.Mode cur = d.getMode();
                android.view.Display.Mode best = null;
                for (android.view.Display.Mode m : d.getSupportedModes()) {
                    // Same resolution only: a mode switch that also changes
                    // size would re-letterbox the game mid-play.
                    if (m.getPhysicalWidth() != cur.getPhysicalWidth()
                            || m.getPhysicalHeight() != cur.getPhysicalHeight()) continue;
                    if (m.getRefreshRate() > 59f && m.getRefreshRate() < 61f) {
                        best = m;
                        break;
                    }
                }
                if (best == null) {
                    Log.i(TAG, "panel has no 60 Hz mode; leaving it alone");
                    return;
                }
                android.view.WindowManager.LayoutParams lp = getWindow().getAttributes();
                if (lp.preferredDisplayModeId == best.getModeId()) return;
                lp.preferredDisplayModeId = best.getModeId();
                getWindow().setAttributes(lp);
                Log.i(TAG, "pinned display mode " + best.getModeId() + " (60 Hz)");
            } catch (Throwable t) {
                Log.w(TAG, "could not pin 60 Hz: " + t);
            }
        }});
    }

    /**
     * Tell the scheduler this is frame-deadline work.
     *
     * Measured on a Pixel 7: the two big cores swing between 500 MHz and
     * 2.8 GHz under steady load, with no thermal throttling at all. That is
     * the governor, not heat -- both of our threads spend much of each frame
     * blocked (waiting their turn for the emulator, or for vsync), so per-core
     * utilisation looks modest and nothing ever commits to a high clock. A
     * 5.6x frequency swing is felt directly as the game speeding up and
     * slowing down.
     *
     * PerformanceHintManager is the first-party way to say otherwise: name the
     * threads that must hit a deadline, state the deadline, and report what
     * each frame actually cost. The scheduler then boosts those threads rather
     * than guessing from utilisation. API 31+, so older devices simply keep
     * the old behaviour.
     */
    private void reportWork(long actualNs, long targetNs) {
        if (android.os.Build.VERSION.SDK_INT < 31 || hintUnavailable) return;
        try {
            final int glTid = android.os.Process.myTid();
            // GLSurfaceView builds a fresh GL thread whenever the surface comes
            // back, so the id changes under us. A session still naming the old
            // thread boosts something that no longer exists, which is how this
            // silently did nothing at all the first time.
            if (hintSession == null || hintGlTid != glTid) {
                // The audio thread is half the work; a session naming only the
                // render thread leaves the emulator's other user unboosted.
                if (audioTid == 0) return;
                android.os.PerformanceHintManager phm =
                        getSystemService(android.os.PerformanceHintManager.class);
                if (phm == null) { hintUnavailable = true; return; }
                int[] tids = { glTid, audioTid };
                hintSession = phm.createHintSession(tids, targetNs);
                if (hintSession == null) { hintUnavailable = true;
                    Log.w(TAG, "performance hints unavailable on this device");
                    return; }
                hintGlTid = glTid;
                hintTargetNs = targetNs;
                Log.i(TAG, "performance hint session for tids "
                        + glTid + "," + audioTid + ", target "
                        + (targetNs / 1000000) + " ms");
            }
            final android.os.PerformanceHintManager.Session session =
                    (android.os.PerformanceHintManager.Session) hintSession;
            // The deadline has to be the one we are actually trying to hit.
            // The session was created before the 60 Hz request was granted, so
            // it held the old 90 Hz figure of 22 ms -- against which a 16 ms
            // frame looks comfortable, and nothing was ever boosted. That is
            // why the cores sat at 500 MHz while the game ran slowly.
            if (targetNs != hintTargetNs) {
                session.updateTargetWorkDuration(targetNs);
                hintTargetNs = targetNs;
                Log.i(TAG, "performance hint target now "
                        + (targetNs / 1000000) + " ms");
            }
            session.reportActualWorkDuration(actualNs > 0 ? actualNs : 1);
        } catch (Throwable t) {
            hintSession = null;
            hintUnavailable = true;
            Log.w(TAG, "performance hint failed: " + t);
        }
    }

    private boolean saidSixty;

    private void requestSixtyHz() {
        if (android.os.Build.VERSION.SDK_INT < 30) return;
        try {
            android.view.Surface s = view.getHolder().getSurface();
            if (s != null && s.isValid()) {
                s.setFrameRate(60.0f,
                        android.view.Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
                if (!saidSixty) { saidSixty = true; Log.i(TAG, "asked the display for 60 Hz"); }
            }
        } catch (Throwable t) {
            Log.w(TAG, "setFrameRate(60) unavailable: " + t);
        }
    }

    /**
     * The finger the engine follows (it knows only one: pressed, moved,
     * released). A MotionEvent's index 0 is whichever finger has been down
     * longest, so reading getX() blindly swapped fingers mid-drag: tap with a
     * second finger and lift the first, and the "same" finger jumped to the
     * other one -- with relative controls, the ship lurched across the screen.
     * So one finger is followed by its pointer id; others don't move the
     * ship, and if the followed one lifts while another is down, the engine
     * sees a release and a fresh press there (the controls re-anchor).
     */
    private int touchId = -1;

    /** Maps a screen touch back through the letterbox into game coordinates. */
    @Override public boolean onTouchEvent(MotionEvent e) {
        if (vpW == 0 || vpH == 0) return true;
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                touchId = e.getPointerId(0);
                sendTouch(0, e, 0);
                break;
            case MotionEvent.ACTION_MOVE: {
                final int i = e.findPointerIndex(touchId);
                if (i >= 0) sendTouch(1, e, i);
                break;
            }
            case MotionEvent.ACTION_POINTER_UP: {
                final int up = e.getActionIndex();
                if (e.getPointerId(up) != touchId) break;          // another finger: nothing to do
                sendTouch(2, e, up);
                final int next = up == 0 ? 1 : 0;                  // hand over to a finger still down
                touchId = e.getPointerId(next);
                sendTouch(0, e, next);
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                final int i = e.findPointerIndex(touchId);
                sendTouch(2, e, i >= 0 ? i : 0);
                touchId = -1;
                break;
            }
            default:                                               // ACTION_POINTER_DOWN: another finger
                break;
        }
        return true;
    }

    private void sendTouch(final int action, MotionEvent e, int index) {
        final int gx = (int) ((e.getX(index) - vpX) * GAME_W / vpW);
        final int gy = (int) ((e.getY(index) - vpY) * GAME_H / vpH);
        // Touches must reach the guest on the same thread that owns the CPU.
        view.queueEvent(new Runnable() {
            @Override public void run() { ShogunNative.nativeTouch(action, gx, gy); }
        });
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
        AudioThread() { super("shogun-audio"); }

        private volatile boolean running = true;
        /** How deep to make the AudioTrack buffer, in chunks. */
        private static final int BATCH = 4;
        /**
         * Chunks generated per pass -- deliberately one, not BATCH.
         *
         * Generating four in a row means four guest calls back to back, and
         * guest entry is serialised, so the render thread is locked out for
         * the whole burst: measured at about 5.5 ms of emulation per chunk,
         * that is a 22 ms block every fourth frame. It showed up as a hitch
         * roughly one frame in five. One chunk per pass spreads the same total
         * work evenly instead, and the buffer stays BATCH deep either way.
         */
        private static final int PER_PASS = 1;

        @Override public void run() {
            android.os.Process.setThreadPriority(
                    android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
            audioTid = android.os.Process.myTid();

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
                for (int i = 0; i < PER_PASS; i++) {
                    ShogunNative.nativeAudio(chunk, samples);
                    System.arraycopy(chunk, 0, batch, i * chunk.length, chunk.length);
                }
                final int bytes = PER_PASS * chunk.length;
                int wrote = track.write(batch, 0, bytes);
                if (wrote < 0) { Log.w(TAG, "AudioTrack.write -> " + wrote); break; }
                if (++silent == 400) {          // ~15 s health line
                    int nz = 0;
                    for (int i = 0; i < bytes; i++) if (batch[i] != 0) nz++;
                    Log.i(TAG, "audio alive, " + nz + "/" + bytes + " nonzero");
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

    /** Where the in-game "Source on GitHub" button goes. */
    static final String REPO_URL = "https://github.com/AdmiralGallade/shogun-arm64-port";

    /**
     * The button lives inside the emulated game, so the press arrives as a flag
     * on the GL thread. Opening a browser has to happen on the UI thread, and
     * must not block the engine, so it is handed over rather than done there.
     */
    void openRepoPage() {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                try {
                    android.content.Intent i = new android.content.Intent(
                            android.content.Intent.ACTION_VIEW,
                            android.net.Uri.parse(REPO_URL));
                    i.addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK);
                    startActivity(i);
                } catch (Exception e) {
                    Log.w(TAG, "could not open " + REPO_URL, e);
                }
            }
        });
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
