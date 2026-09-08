package net.int13;

import android.content.Context;
import android.opengl.GLSurfaceView;

/**
 * The engine looks this class up by name and pulls the camera callbacks off
 * it. int13 shipped AR titles on the same engine; Shogun does not use the
 * camera, but the lookups happen during init and should resolve cleanly.
 */
public class GLJNIView extends GLSurfaceView {
    public GLJNIView(Context c) { super(c); }

    public void initCamera(int w, int h) {}
    public void startCamera() {}
    public void updateCamera() {}
    public void stopCamera() {}
}
