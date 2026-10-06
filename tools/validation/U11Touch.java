// Device acceptance helper. Uses the same shell event injection path as AOSP
// InputShellCommand; no engine input bypass or authored scene mutation.
import android.os.SystemClock;
import android.view.InputEvent;
import android.view.InputDevice;
import android.view.MotionEvent;
import java.lang.reflect.Method;

public final class U11Touch {
    static Object manager;
    static Method inject;
    static long down;
    static void send(int action, float x, float y, Float secondX, Float secondY) throws Exception {
        int count = secondX == null ? 1 : 2;
        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[count];
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[count];
        for (int i = 0; i < count; ++i) {
            properties[i] = new MotionEvent.PointerProperties();
            properties[i].id = i;
            properties[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            coords[i] = new MotionEvent.PointerCoords();
            coords[i].x = i == 0 ? x : secondX;
            coords[i].y = i == 0 ? y : secondY;
            coords[i].pressure = 1;
            coords[i].size = 1;
        }
        MotionEvent event = MotionEvent.obtain(down, SystemClock.uptimeMillis(), action,
            count, properties, coords, 0, 0, 1, 1, 0, 0, InputDevice.SOURCE_TOUCHSCREEN, 0);
        try {
            if (!Boolean.TRUE.equals(inject.invoke(manager, event, 2)))
                throw new IllegalStateException("Input injection refused");
        } finally { event.recycle(); }
    }
    public static void main(String[] args) throws Exception {
        if (args.length != 7) throw new IllegalArgumentException("x y endX endY secondX secondY cancel");
        Class<?> cls = Class.forName("android.hardware.input.InputManagerGlobal");
        manager = cls.getMethod("getInstance").invoke(null);
        inject = cls.getMethod("injectInputEvent", InputEvent.class, int.class);
        float x = Float.parseFloat(args[0]), y = Float.parseFloat(args[1]);
        float endX = Float.parseFloat(args[2]), endY = Float.parseFloat(args[3]);
        float sx = Float.parseFloat(args[4]), sy = Float.parseFloat(args[5]);
        down = SystemClock.uptimeMillis();
        send(MotionEvent.ACTION_DOWN, x, y, null, null);
        SystemClock.sleep(120);
        send(MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), x, y, sx, sy);
        for (int n = 1; n <= 30; ++n) {
            float t = n / 30f;
            send(MotionEvent.ACTION_MOVE, x + (endX-x)*t, y + (endY-y)*t, sx, sy);
            SystemClock.sleep(20);
        }
        send(MotionEvent.ACTION_POINTER_UP | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), endX, endY, sx, sy);
        SystemClock.sleep(120);
        send(Boolean.parseBoolean(args[6]) ? MotionEvent.ACTION_CANCEL : MotionEvent.ACTION_UP,
            endX, endY, null, null);
        System.out.println("Injected physical two-pointer stream including " + (args[6].equals("true") ? "CANCEL" : "UP"));
    }
}
