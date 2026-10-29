package net.int13.shogun;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Hands the bug report's log to the email app. The build has no AndroidX, so
 * this is the few lines of FileProvider it needs: read-only, one folder in the
 * cache, and only to an app the report intent granted access to.
 */
public final class BugReportProvider extends ContentProvider {
    static final String DIR = "bugreport";

    static String authority(Context c) { return c.getPackageName() + ".bugreport"; }

    static Uri uriFor(Context c, String name) {
        return new Uri.Builder().scheme("content").authority(authority(c))
                .appendPath(name).build();
    }

    /** Only plain names inside the folder; nothing that climbs out of it. */
    private File fileFor(Uri uri) throws FileNotFoundException {
        String name = uri.getLastPathSegment();
        if (name == null || name.contains("/") || name.contains("..")
                || uri.getPathSegments().size() != 1) {
            throw new FileNotFoundException(String.valueOf(uri));
        }
        File f = new File(new File(getContext().getCacheDir(), DIR), name);
        if (!f.isFile()) throw new FileNotFoundException(String.valueOf(uri));
        return f;
    }

    @Override public boolean onCreate() { return true; }

    @Override public ParcelFileDescriptor openFile(Uri uri, String mode)
            throws FileNotFoundException {
        if (!"r".equals(mode)) throw new SecurityException("read-only");
        return ParcelFileDescriptor.open(fileFor(uri), ParcelFileDescriptor.MODE_READ_ONLY);
    }

    /** Email apps ask for the name and size to show the attachment. */
    @Override public Cursor query(Uri uri, String[] projection, String sel,
                                  String[] args, String sort) {
        File f;
        try { f = fileFor(uri); } catch (FileNotFoundException e) { return null; }
        if (projection == null) {
            projection = new String[] { OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE };
        }
        MatrixCursor c = new MatrixCursor(projection, 1);
        Object[] row = new Object[projection.length];
        for (int i = 0; i < projection.length; i++) {
            if (OpenableColumns.DISPLAY_NAME.equals(projection[i])) row[i] = f.getName();
            else if (OpenableColumns.SIZE.equals(projection[i])) row[i] = f.length();
        }
        c.addRow(row);
        return c;
    }

    @Override public String getType(Uri uri) { return "text/plain"; }

    @Override public Uri insert(Uri uri, ContentValues v) {
        throw new UnsupportedOperationException();
    }
    @Override public int delete(Uri uri, String s, String[] a) {
        throw new UnsupportedOperationException();
    }
    @Override public int update(Uri uri, ContentValues v, String s, String[] a) {
        throw new UnsupportedOperationException();
    }
}
