package net.int13.shogun;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.util.Log;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * The in-game "Report a bug" button (Options > Cheats). Opens the player's
 * email app with a message to the developer, the device and mission filled
 * in, and this session's log attached. Nothing leaves the phone unless the
 * player presses Send in their own email app.
 */
final class BugReport {
    static final String TAG = "shogun";
    static final String EMAIL = "drcybal@gmail.com";
    static final String LOG_NAME = "shogun-log.txt";
    /** Enough to cover a mission or two of HITCH/JOLT lines. */
    static final int LOG_LINES = 4000;

    private BugReport() {}

    private static boolean showing;

    /** Say what will happen first: the log is attached, and they can read it. */
    static void ask(final Activity a, final String context) {
        if (showing || a.isFinishing()) return;
        showing = true;
        new AlertDialog.Builder(a, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Report a bug")
                .setMessage("This opens your email app with a message to " + EMAIL + ".\n\n"
                        + "Your phone model, Android version, the game version and the mission "
                        + "you were on are filled in, and a log of this session is attached "
                        + "to help find the problem. You can read or remove both before sending.")
                .setPositiveButton("Write email", (d, w) -> collectAndSend(a, context))
                .setNegativeButton("Cancel", null)
                .setOnDismissListener(d -> showing = false)
                .show();
    }

    /** logcat can take a moment; keep it off the UI thread. */
    private static void collectAndSend(final Activity a, final String context) {
        new Thread(() -> {
            final Uri log = writeLog(a);
            a.runOnUiThread(() -> send(a, context, log));
        }, "bugreport").start();
    }

    private static void send(Activity a, String context, Uri log) {
        final String subject = "Shogun 64 " + version(a) + " bug report";
        final String body = body(a, context, log != null);

        Intent mail = new Intent(Intent.ACTION_SEND);
        mail.putExtra(Intent.EXTRA_EMAIL, new String[] { EMAIL });
        mail.putExtra(Intent.EXTRA_SUBJECT, subject);
        mail.putExtra(Intent.EXTRA_TEXT, body);
        if (log != null) {
            mail.setType("text/plain");
            mail.putExtra(Intent.EXTRA_STREAM, log);
            // The grant only travels with the intent when the URI is in its ClipData.
            mail.setClipData(ClipData.newRawUri(LOG_NAME, log));
            mail.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        // Only email apps answer mailto:, so ask which those are and offer SEND
        // to just them. (A mailto: selector on the SEND would be shorter, but
        // Android 17's stricter intent matching refuses it: result -91.)
        List<Intent> targets = new ArrayList<>();
        PackageManager pm = a.getPackageManager();
        Set<String> seen = new HashSet<>();
        for (ResolveInfo ri : pm.queryIntentActivities(
                new Intent(Intent.ACTION_SENDTO, Uri.parse("mailto:")), 0)) {
            String pkg = ri.activityInfo.packageName;
            if (!seen.add(pkg)) continue;
            Intent t = new Intent(mail).setPackage(pkg);
            if (t.resolveActivity(pm) == null) continue;   // email app, but takes no SEND
            if (log != null) a.grantUriPermission(pkg, log, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            targets.add(t);
        }
        try {
            if (targets.size() == 1) {
                a.startActivity(targets.get(0));
                return;
            } else if (targets.size() > 1) {
                Intent chooser = Intent.createChooser(targets.remove(0), "Send bug report with");
                chooser.putExtra(Intent.EXTRA_INITIAL_INTENTS, targets.toArray(new Intent[0]));
                a.startActivity(chooser);
                return;
            }
            Log.w(TAG, "bugreport: no email app takes an attachment");
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "bugreport: email app refused the report", e);
        }
        // Some email apps answer only a plain mailto:. No attachment that way.
        try {
            Uri uri = Uri.parse("mailto:" + EMAIL
                    + "?subject=" + Uri.encode(subject) + "&body=" + Uri.encode(body));
            a.startActivity(new Intent(Intent.ACTION_SENDTO, uri));
            return;
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "bugreport: no email app", e);
        }
        noEmailApp(a);
    }

    /** No email app at all: show the address so they can write from elsewhere. */
    private static void noEmailApp(Activity a) {
        TextView tv = new TextView(a);
        tv.setText("No email app was found on this phone.\n\nPlease write to:\n" + EMAIL);
        tv.setTextIsSelectable(true);
        int pad = Math.round(20 * a.getResources().getDisplayMetrics().density);
        tv.setPadding(pad, pad, pad, 0);
        new AlertDialog.Builder(a, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Report a bug")
                .setView(tv)
                .setPositiveButton("Close", null)
                .show();
    }

    private static String body(Activity a, String context, boolean attached) {
        String world = "", hard = "0";
        if (context != null) {
            int bar = context.lastIndexOf('|');
            world = bar >= 0 ? context.substring(0, bar) : context;
            hard = bar >= 0 ? context.substring(bar + 1) : "0";
        }
        android.util.DisplayMetrics dm = a.getResources().getDisplayMetrics();
        return "What happened?\n\n\n"
                + "What were you doing just before?\n\n\n"
                + "Does it happen every time?\n\n\n"
                + "---- please keep the lines below ----\n"
                + "Game: Shogun 64 " + version(a) + "\n"
                + "Phone: " + Build.MANUFACTURER + " " + Build.MODEL + "\n"
                + "Android: " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")\n"
                + "Screen: " + dm.widthPixels + "x" + dm.heightPixels + "\n"
                + "Last level: " + (world.isEmpty() ? "none yet" : world) + "\n"
                + "Hard mode: " + ("1".equals(hard) ? "on" : "off") + "\n"
                + (attached ? "Log: attached (" + LOG_NAME + ")\n" : "Log: could not be attached\n");
    }

    private static String version(Activity a) {
        try {
            return a.getPackageManager().getPackageInfo(a.getPackageName(), 0).versionName;
        } catch (Exception e) {
            return "?";
        }
    }

    /**
     * This process's own log, newest LOG_LINES lines. An app may read its own
     * logcat without any permission. Leaderboard lines name the player's
     * country and city, so they are left out.
     */
    private static Uri writeLog(Activity a) {
        File dir = new File(a.getCacheDir(), BugReportProvider.DIR);
        File out = new File(dir, LOG_NAME);
        Process p = null;
        try {
            if (!dir.isDirectory() && !dir.mkdirs()) throw new java.io.IOException("mkdir " + dir);
            p = new ProcessBuilder("logcat", "-d", "-v", "threadtime",
                    "-t", String.valueOf(LOG_LINES),
                    "--pid=" + android.os.Process.myPid())
                    .redirectErrorStream(true).start();
            try (BufferedReader in = new BufferedReader(
                         new InputStreamReader(p.getInputStream(), "UTF-8"));
                 Writer w = new OutputStreamWriter(new FileOutputStream(out), "UTF-8")) {
                w.write("Shogun 64 " + version(a) + " on " + Build.MANUFACTURER + " "
                        + Build.MODEL + ", Android " + Build.VERSION.RELEASE + "\n\n");
                String line;
                while ((line = in.readLine()) != null) {
                    if (line.contains("leaderboard")) continue;
                    w.write(line);
                    w.write('\n');
                }
            }
            return BugReportProvider.uriFor(a, LOG_NAME);
        } catch (Exception e) {
            Log.w(TAG, "bugreport: could not write the log", e);
            return null;
        } finally {
            if (p != null) p.destroy();
        }
    }
}
