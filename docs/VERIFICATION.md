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
