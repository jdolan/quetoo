# Quetoo Android — ProGuard/R8 rules
#
# minifyEnabled is currently false (there is little Java to shrink), so these
# rules are mostly a safety net for when shrinking is turned on.
#
# SDL's Java glue is called from native code via JNI; keep it intact so R8 never
# strips/renames methods that libSDL3.so resolves by name.
-keep class org.libsdl.app.** { *; }
-keep class org.quetoo.android.** { *; }

# Native methods must never be renamed.
-keepclasseswithmembernames class * {
    native <methods>;
}
