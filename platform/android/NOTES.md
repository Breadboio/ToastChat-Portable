# Android port

`.apk` for phones and tablets, built on the same C99 core as the 3DS, Switch
and Wii. Unlike those, this target's hardware is one the project actually has
on hand, so it is the first realistic chance to run the shared core on a real
device.

    tools/build-android.sh                                   # wss://443, release-shaped
    TC_TLS=OFF TC_HOST=192.168.1.167 TC_PORT=3401 tools/build-android.sh

Output lands in `build/ToastChat-Android-debug.apk`. Install with
`adb install -r`, or copy it to the phone and open it.

## Status

Builds clean for arm64-v8a, armeabi-v7a and x86_64 under `-Wall -Wextra`.
**It has never been run.** This box has no `/dev/kvm` and no `/dev/binder`, so
neither the Android emulator nor redroid can boot here - the same limitation
[[project_reader_comfort_android]] hit. Everything below was verified by
rendering or by host-side tests, which is not the same thing as running.

What *was* verified without a device:

| Piece | How |
|---|---|
| the tall layout | `tools/test_ui.c` renders 640x1422 and 640x1024 to PNG |
| touch mapping + scroll gesture | `tools/test_gesture.c`, 9 groups, runs in gcc:13 |
| Switch/Wii/3DS layouts unmoved | same tests, geometry identical to HEAD |
| APK shape | `ANativeActivity_onCreate` exported, 3 ABIs, 2.78MB |

## No Java

`android:hasCode="false"`. The activity is the framework's own
`android.app.NativeActivity`, which loads `libtoastchat.so` and calls
`ANativeActivity_onCreate`. There is no Kotlin, no `MainActivity`, no
`libc++_shared.so` (`ANDROID_STL=none` - the whole program is C).

Two things bite here:

- **`-u ANativeActivity_onCreate` is mandatory.** The symbol lives in the NDK's
  `native_app_glue` and nothing in our own code references it, so without the
  undefined-symbol flag the linker drops it and the app dies at launch with
  "Unable to find native library entry point". The APK builds and installs
  perfectly either way.
- `native_app_glue` ships as **source**, not a prebuilt library. It is compiled
  from `${ANDROID_NDK}/sources/android/native_app_glue`.

## Screen: one logical size, scaled by the compositor

Every other target has a screen size fixed by the hardware. A phone does not,
and the core's layout thresholds are absolute pixel values (`W < 400` is a
dual-screen 3DS, `W < 900` is the compact toolbar), so handing it a raw
1080x2400 surface would land in the *wide* layout on a phone.

So the core renders at a fixed **640 logical width**, and
`ANativeWindow_setBuffersGeometry` hands that buffer to the compositor, which
scales it to the panel in hardware. Consequences:

- `tc_video_present()` is a row `memcpy`. There is no software scaler.
- 640 is not arbitrary: below ~572 the compact toolbar's room button runs off
  the right edge, and at 900 the layout switches to the wide one.
- The logical **height** is derived from the real surface aspect
  (`640 * surfH / surfW`, clamped to 480..1600) so the hardware scale stays
  square. Nothing is letterboxed.
- `WINDOW_FORMAT_RGBX_8888` is R,G,B,X in memory - the same byte order the core
  writes - so no channel swizzle is needed either.

### Touch arrives in a different coordinate space

Motion events are in **surface** pixels; the buffer is logical pixels. They
differ by exactly the scale factor above, and getting it wrong is invisible on
a 1:1 emulator and completely wrong on a real phone. `tc_gesture.c` does the
conversion, and it **rounds rather than truncates** - truncation biases every
coordinate down by up to a logical pixel, which at this downscale is ~1.7
surface px and was measured losing a whole row off the end of a drag
(`test_gesture.c` case 4 catches it).

## The `tall` layout branch

Added to `core/tc_ui.c` for this port. The compact layout gives the log a fixed
152px; on a 640x1422 phone that is 11% of the screen. In the tall branch the
log and canvas **split** the leftover height 40/60, and the toolbar is the
640x480 one unchanged.

It triggers on `H * 5 >= W * 6` (H >= 1.2W). The tallest console is the Wii at
H = 0.75W, so no existing target can reach it - confirmed by re-running the
layout and YUV tests against a clean HEAD checkout and getting identical
numbers.

## Scrolling is a drag, not a button

On the 3DS the log lives on the top screen, which has no touch panel, so
`tc_input_scroll()` is the D-pad. A phone has one screen and no buttons.

Here, a drag that **starts above the canvas** (in the bar/log band) scrolls, and
that gesture is withheld from the pointer entirely - which is also what stops a
flick from pressing a toolbar button when it crosses one on the way down. 48
logical px of drag is one row. Dragging down reveals older rows.

The state machine is in `tc_gesture.c`, which deliberately includes **no Android
headers** so `tools/test_gesture.c` can drive it on x86 with realistic input
(many small fractional moves, the way a digitiser actually reports, not two
endpoints). That is the only real logic in the port; the rest of
`plat_android.c` is bionic sockets and a memcpy.

## TLS

Same `core/tc_tls.c` and same bundled `core/tc_cacert.h` anchors as the 3DS and
Switch, against mbedtls 2.28.8 - the series devkitPro ships, so the file
compiles unchanged on all three. mbedtls is **not vendored** (27MB of upstream);
`tools/build-android.sh` fetches it into `third_party/mbedtls/` on the first TLS
build, and it is gitignored.

`usesCleartextTraffic="true"` is set for the `TC_TLS=OFF` builds (the LAN dev
server, and the plain `:80` path the Wii is stuck with). Release builds do not
rely on it.

## Known risks, to check on the first real device

- **Gesture navigation vs the SEND button.** The toolbar sits flush against the
  bottom edge, which on a gesture-nav phone is the home-swipe strip. This may
  need a bottom inset. Nothing here can test it.
- **Text size.** The 8x16 font at scale 1 on a 640-wide logical screen is ~27
  device px on a 1080-wide panel. Legible, but smaller in proportion than on a
  Wii. Bump the scale in the tall branch if it reads badly in the hand.
- Portrait is locked in the manifest. Landscape would give a ~1387x640 logical
  screen and take the *wide* layout, which is untested here.
- One finger only: extra pointers are ignored, matching the console ports.
