package org.bstheater.quest;
/** Registered by the native mod; the explicit library property is only used by the ADB device check. */
public final class NativePlayerSolver {
    static {
        String library = System.getProperty("bstheater.solver.library");
        if (library != null) System.load(library);
    }
    static native String evaluate(String input);
}
