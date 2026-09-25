package net.int13.shogun;

import android.util.Log;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * The online leaderboard, talking to a server of our own.
 *
 * The original game reached int13's "Hub Server" over NWT, their obfuscated
 * binary packet protocol, at a hardcoded IP. That has been dead for years. But
 * the engine hands its ranking screen whatever a registered callback gives it,
 * so the port can do the networking itself and feed that callback — which is
 * what this does, over ordinary HTTPS.
 *
 * The HTTP lives in Java rather than in the native runtime purely so that TLS
 * is the platform's problem rather than ours.
 *
 * Native owns everything that touches the emulated engine: this thread only
 * moves JSON.
 */
final class Leaderboard extends Thread {
    private static final String TAG = "shogun";

    /**
     * Your own server, from server/worker.js. Empty keeps the leaderboard
     * offline, which is the default: the original Hub Server has been gone for
     * over a decade and nothing answers at its address.
     */
    private static final String DEFAULT_ENDPOINT = "";

    /**
     * A file in the app's external directory overrides the built-in value, so
     * a test device can be pointed at a server without rebuilding:
     *
     *   adb shell "echo https://host/v1/score >      *     /sdcard/Android/data/dev.admiralgallade.shogun64/files/leaderboard.txt"
     */
    static String endpointFor(android.content.Context ctx) {
        try {
            java.io.File dir = ctx.getExternalFilesDir(null);
            if (dir != null) {
                java.io.File f = new java.io.File(dir, "leaderboard.txt");
                if (f.isFile() && f.length() > 0 && f.length() < 512) {
                    byte[] b = new byte[(int) f.length()];
                    try (java.io.FileInputStream in = new java.io.FileInputStream(f)) {
                        int n = in.read(b);
                        if (n > 0) {
                            String s = new String(b, 0, n, "UTF-8").trim();
                            if (s.startsWith("http")) {
                                Log.i(TAG, "leaderboard: endpoint from leaderboard.txt");
                                return s;
                            }
                        }
                    }
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "leaderboard: could not read override (" + e.getMessage() + ")");
        }
        return DEFAULT_ENDPOINT;
    }
    /** Empty disables the leaderboard entirely, which is the default. */
    private final String endpoint;
    private volatile boolean running = true;

    Leaderboard(String endpoint) {
        super("shogun-leaderboard");
        this.endpoint = endpoint == null ? "" : endpoint.trim();
        setDaemon(true);
    }

    boolean enabled() { return !endpoint.isEmpty(); }

    void quit() { running = false; }

    @Override public void run() {
        if (!enabled()) {
            Log.i(TAG, "leaderboard: no endpoint configured, staying offline");
            return;
        }
        Log.i(TAG, "leaderboard: endpoint " + endpoint);
        while (running) {
            String job = null;
            try {
                if (ShogunNative.nativeBooted()) job = ShogunNative.nativeLeaderboardPending();
            } catch (Throwable t) {
                // Never let the leaderboard take the game down with it.
                Log.w(TAG, "leaderboard: poll failed", t);
            }
            if (job == null) {
                try { Thread.sleep(500); } catch (InterruptedException ignored) {}
                continue;
            }
            try {
                submit(job);
            } catch (Exception e) {
                // A dead network is not an error worth retrying hard: the next
                // score will try again.
                Log.w(TAG, "leaderboard: submit failed (" + e.getMessage() + ")");
            }
        }
    }

    /** job is "board\nscore", as built by the native side. */
    private void submit(String job) throws Exception {
        final int nl = job.indexOf('\n');
        final String board = nl < 0 ? "total" : job.substring(0, nl);
        final int score = nl < 0 ? 0 : Integer.parseInt(job.substring(nl + 1).trim());

        String name = ShogunNative.nativeLeaderboardName();
        if (name == null || name.isEmpty()) name = "Player";

        final JSONObject out = new JSONObject();
        out.put("board", board);
        out.put("name", name);
        out.put("score", score);

        final JSONObject in = post(out);
        if (in == null) return;

        ShogunNative.nativeLeaderboardResult(
                in.optString("board", board),
                in.optInt("yourBest"),
                in.optInt("worldRank"),   in.optInt("worldBest"),
                in.optString("worldBestName", "--"),
                in.optInt("countryRank"), in.optInt("countryBest"),
                in.optString("countryBestName", "--"),
                in.optInt("cityRank"),    in.optInt("cityBest"),
                in.optString("cityBestName", "--"),
                in.optString("countryName", "--"),
                in.optString("cityName", "--"));
        Log.i(TAG, "leaderboard: '" + board + "' score " + score
                + " -> world #" + in.optInt("worldRank"));
    }

    private JSONObject post(JSONObject body) throws Exception {
        final HttpURLConnection c = (HttpURLConnection) new URL(endpoint).openConnection();
        try {
            c.setRequestMethod("POST");
            c.setConnectTimeout(8000);
            c.setReadTimeout(8000);
            c.setDoOutput(true);
            c.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            final byte[] payload = body.toString().getBytes("UTF-8");
            c.setFixedLengthStreamingMode(payload.length);
            try (OutputStream os = c.getOutputStream()) { os.write(payload); }

            final int code = c.getResponseCode();
            if (code != 200) {
                Log.w(TAG, "leaderboard: HTTP " + code);
                return null;
            }
            try (InputStream is = c.getInputStream()) {
                final ByteArrayOutputStream buf = new ByteArrayOutputStream();
                final byte[] b = new byte[4096];
                int n;
                while ((n = is.read(b)) > 0) buf.write(b, 0, n);
                return new JSONObject(buf.toString("UTF-8"));
            }
        } finally {
            c.disconnect();
        }
    }
}
