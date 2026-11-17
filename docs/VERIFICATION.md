# Verification

Checked on Windows x64 on 2026-11-17. Build: w64devkit 2.11.0, GCC 16.2.0, C++20 / GNU C11. Generation uses signed wraparound and precise floating-point behavior; fast-math and CPU-specific instruction tuning are not enabled.

## Automated checks

- 2,622 core assertions passed. These cover seed boundaries and UTF-16 string hashing, negative coordinates, biome generation through all 35 Java and 28 Bedrock profiles in all dimensions and five map sampling scales available to each version, current Java reference coordinates, structure and terrain checks, Bedrock slime seed independence, duplicate tile suppression, background searches, cancellation, and Unicode settings/bookmark persistence.
- All five vendored Java upstream suites passed: biomes, structures, pieces, loot, and carvers. They link the same native objects used in the app.
- 25 executable integration checks passed. They invoke the actual standalone EXE and check JSON queries, edition / option handling, rejected invalid inputs, PNG encoding for both editions and all three dimensions, CSV structure scans, End ships, Java Nether fossil terrain checks, version gates, Unicode paths, and decoding of the embedded Minecraft marker atlas.
- The Java 26.3 Dappled Forest fixture is seed -3829811542736183482 at X=68148, Y=77, Z=80990, from the vendored upstream test.
- The Java End ring fixture is seed 856461443495910397 at X=-371136, Z=-64. The 26.3 generator reports End Highlands where the older 26.2 generator reports End Barrens.

Run the checks with:

    python scripts/build.py --upstream-tests
    python tests/integration.py

The executable integration report is written to build-native/verification/report.json. GitHub Actions repeats the build and headless checks.

## Runtime and packaging

The EXE statically links its C++ runtime, UI and both generation libraries. PE imports were checked: only Windows system DLLs are required. No browser, Java VM, Python runtime, external data files, network requests, local server, or extraction step is used at runtime.

The application icon, Lucide control geometry, Minecraft feature-texture atlas, and complete third-party notices are embedded. Noto Sans JP is embedded with regular and semibold weights. Windows fonts supply optional fallback for other user text. The palette includes 93 colours directly verified from Chunkbase biome legend swatches; PNG pixel tests cover all three dimensions. Settings and bookmarks are the app's own saved user data.

Matching immutable biome tables are shared between the two generators. The match was checked on original bytes before changing storage linkage. Algorithm code and nonidentical biome trees remain separate. See third_party/UPSTREAM.md.

## Interactive checks

The native application was opened and operated with Windows UI automation. Edition switching, all dimensions, native structure searches, the final exported map, the light theme, Noto Sans JP glyphs and map controls were checked visually. Screenshots and a short walkthrough accompany the delivered build.

## Accuracy limits

These checks do not establish exhaustive parity with an actual Minecraft instance. No game client or server was launched to independently verify every structure or biome. Library reference fixtures, API agreement, and smoke checks provide narrower evidence.

Bedrock 26.30 / 26.40 / 26.60 implementations derive from preview versions. Exact agreement with final 26.60–26.63 is unverified. Earlier profiles represent generation families and do not model every patch separately.

The app displays biome slices, not a full block or height map. Temples, mansions, spawn and decorations can depend on terrain conditions that are approximated by the libraries. Existing modified chunks, mods and datapacks are not simulated.

Unsupported categories are exposed in the in-app coverage list: Overworld fossils, Java monster rooms and Bedrock Nether fossils. Java ravines and lava lakes are not exposed as layers. Bedrock monster room terrain searches are intentionally restricted to radius 128.

## Mobile verification

Android was checked in an isolated AOSP API 35 x86-64 emulator. The signed release application passed the same 2,622 native assertions (35 Java and 28 Bedrock profiles, all dimensions). The manifest does not request Internet permission. Phone portrait, phone landscape and a 1280-dp tablet layout were operated and captured. Native on-screen text input resizes the usable viewport so that coordinate fields stay visible. Structure search returned results and a single result tap centered the map. One-finger panning, long-press point inspection and PNG export through the Android system document picker were checked; the saved PNG was pulled back and decoded.

A separate test-only instrumentation APK injects a real two-finger MotionEvent sequence into the native map. It then backgrounds the application and checks the saved scale. The tested pinch reduced blocks per pixel from 2.259624 to 1.027101. The test harness must be signed with the same local key as the installed application:

    python tests/android_touch.py --serial <emulator-serial> --server-port <adb-server-port>

This does not establish performance or compatibility on every physical Android device. The ARM64 and ARMv7 libraries are cross-compiled with the Android NDK; interactive emulator checks use the x86-64 build of the same C++ source. ARM64 and x86-64 ELF load segments were checked at 16-KiB alignment, including the official SDL3 library. The ARMv7 SDL3 binary uses 4-KiB alignment.

The final Android package was rechecked from a fresh install with Wi-Fi and mobile data disabled. Unicode bookmark names persisted across process restarts, numeric seed replacement and range errors were checked through native text input, and the saved structure CSV decoded as UTF-8 with the expected columns. Clean PNG exports were pulled back and visually checked. The Windows GUI also saved a clean map through its native file dialog after the common export change.

iOS builds were checked with Xcode 16.4 and the iPhoneOS 18.5 SDK, targeting ARM64 devices on iOS 15+. The unsigned IPA was inspected for a real ARM64 Mach-O executable, a valid iPhone/iPad Info.plist and compiled app icons. Apple Silicon and Intel iOS simulators passed all 2,622 native assertions. iPhone and iPad screenshots were checked visually, including Retina framebuffer scaling, the Overworld, Nether and End. CI also launches the native PNG export path, checks the copy in Documents/Exports, and retains the exported image and native sharing screenshots. Files > SeedLens > Exports provides direct access independently of the sharing destination.

Physical Android/iOS installations have not been tested. The IPA needs an Apple signing identity and provisioning profile or a compatible re-signing workflow before installation on an iPhone or iPad. Simulator results do not establish compatibility or performance on every physical device.
