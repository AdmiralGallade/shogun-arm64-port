package net.int13.shogun;

/** Native entry points into the ARM32 runtime. */
public final class ShogunNative {
    static { System.loadLibrary("shogun"); }

    private ShogunNative() {}

    /** Loads the 2012 library and boots the engine. Must run on the GL thread. */
    public static native boolean nativeInit(Object activity, byte[] lib,
                                            String pakPath, long pakLen,
                                            String filesDir, int w, int h);

    public static native void nativeTick();
    /** action: 0 = pressed, 1 = move, 2 = released. */
    public static native void nativeTouch(int action, int x, int y);
    public static native void nativeAudio(byte[] buf, int samples);
    public static native void nativePause();

    /** Rebuild every GL object after Android destroyed the EGL context. */
    public static native void nativeSurfaceRecreated();

    /** Flush the engine's persistent data to disk. */
    public static native void nativeSave();
    public static native boolean nativeBooted();
}
