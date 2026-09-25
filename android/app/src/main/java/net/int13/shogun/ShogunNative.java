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

    // ---- leaderboard: Java does the HTTPS, native does the engine ----------
    /** "board
score" when the game has a score to send, else null. */
    public static native String nativeLeaderboardPending();
    /** The nickname the player entered, or null before one is set. */
    public static native String nativeLeaderboardName();
    public static native void nativeLeaderboardResult(
            String board, int yourBest,
            int worldRank, int worldBest, String worldBestName,
            int countryRank, int countryBest, String countryBestName,
            int cityRank, int cityBest, String cityBestName,
            String countryName, String cityName);
    public static native boolean nativeBooted();
}
