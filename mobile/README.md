# Mobile builds

The Android and iOS applications compile the same C++ generation engines, ImGui interface, biome palette, Minecraft marker atlas, Noto Sans JP fonts and Lucide controls as Windows. SDL3 supplies the native window, renderer, text input and clipboard. The app does not use a WebView or download map data.

Phone layouts use two compact toolbars and a floating feature panel. Larger tablets use the desktop layout. Search results support a single tap, the map supports one-finger panning, two-finger zooming and a long-press coordinate menu. Android saves PNG/CSV through its document picker. iOS saves copies in Files > SeedLens > Exports and opens the native share sheet for additional destinations. Existing exports receive numbered filenames. App-private storage retains settings and bookmarks.

## Android

Requires Android 8.0 / API 26 or later. The normal release APK targets ARM64 devices. The optional universal build also includes ARMv7 and x86-64. Internet and storage permissions are not requested; exports use the system document picker.

On Windows, install Python 3.10+ and a JDK 17+ (java, javac and keytool on PATH):

    python scripts/setup_mobile.py
    python scripts/build_android.py

Output: dist/SeedLens.apk. Tool downloads are pinned and verified. Subsequent builds can run offline after the tool cache has been populated. An existing Android SDK with platform 36 and build-tools 36.0.0 can be selected with ANDROID_HOME; use ANDROID_NDK_HOME to choose its NDK. SDL_ANDROID_AAR can point to the official SDL3 3.4.20 AAR.

For other architectures:

    python scripts/build_android.py --abi arm64-v8a armeabi-v7a x86_64 --output dist/SeedLens-universal.apk

The script creates a signing key in the user's .seedlens/signing directory, outside the repository and tool cache. Preserve this directory securely to sign future updates with the same identity. Older keys under .cache/seedlens-mobile/signing are copied automatically on the first build. SEEDLENS_SIGNING_DIR selects another private directory. Keys, passwords and provisioning profiles must not be committed. The release APK is signed; CI or third-party builds with a different key cannot update it in place.

## iOS / iPadOS

Requires iOS / iPadOS 15+, macOS, Xcode, CMake and Python 3. An unsigned ARM64 device application can be built with:

    cmake -S mobile -B build-mobile/ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
    cmake --build build-mobile/ios --config Release --target SeedLens --parallel 3
    mkdir -p dist/Payload
    cp -R build-mobile/ios/Release-iphoneos/SeedLens.app dist/Payload/
    (cd dist && zip -qr SeedLens-unsigned.ipa Payload)

The IPA contains a real ARM64 iPhone/iPad app bundle. It is **unsigned** and cannot be installed directly until it is signed with a valid Apple signing identity and a provisioning profile for the intended installation method. No Apple account or signing material is included. For an Xcode-signed build, set the target's signing team and enable code signing locally.

For the simulator, use iphonesimulator as CMAKE_OSX_SYSROOT and the host architecture as CMAKE_OSX_ARCHITECTURES. Launching with --self-test runs the same native core assertions as Windows and writes self-test.json under the application preference directory. The Mobile GitHub Actions workflow builds both a device IPA and simulator app, runs this check, and captures iPhone/iPad screenshots in all dimensions. The --export-smoke diagnostic flag captures a clean native map, writes a copy under Documents/Exports and opens the native share sheet after generation completes.

The generation coverage and accuracy limitations are identical to the Windows application; see the main README.

The final iOS Files/share-sheet flow is compiled but not fully runtime-verified. Hosted simulator installation and launch operations stalled during the final check. Earlier native engine checks, Retina rendering, all-dimension screenshots and PNG capture passed; physical installation and the updated share flow still need device verification.
