# Vendored source

All required generation and UI source is vendored. Building or running does not fetch dependencies.

- Java: [xpple/cubiomes](https://github.com/xpple/cubiomes), commit 8c37ab26f2b47740839eb0045b34efc5141eb065, MIT.
- Bedrock: [FragrantResult186/cubiomes-bedrock](https://github.com/FragrantResult186/cubiomes-bedrock), commit 294c72ab0392dd818a91d293dbd5774d4874d64b, MIT.
- Dear ImGui: [ocornut/imgui](https://github.com/ocornut/imgui), commit 77fe4588e6af934b4167b91312174ca3fbb0d71a, version 1.93.1 WIP, MIT. Only Win32 / DX11 backends are built.
- Lucide: [lucide-icons/lucide](https://github.com/lucide-icons/lucide), commit f8feea7a02ea461c9f2a4b4d851512aae6ec933f, ISC and the retained Feather MIT notice. Original SVGs are in assets/lucide; generated geometry is in src/lucide_icons.h. The building-2, waves, and circle-help names use the current building, waves-horizontal, and circle-question-mark geometry.

## Local changes

The generation algorithms are retained. Two storage/linkage changes reduce the standalone EXE without compression or extraction:

1. Identical immutable tables in btree18.h, btree19.h, btree192.h, btree20.h, and btree21wd.h use Windows GNU selectany linkage. Before this change, matching Java and Bedrock files were byte-identical. The linker can share one copy. Nonidentical newer biome tables remain separate.
2. The Java xrms jump table uses the same linkage to avoid storing a 128 KiB copy in each translation unit.

Other platforms keep the original static linkage through the preprocessor fallback.

Bedrock globals are prefixed at compile time by src/bedrock_symbols.h. This allows both C libraries to coexist in the same process. The prefix header is generated with the --probe build option only when updating the vendored engine; normal builds use the checked-in header. Intentionally shared biome tables are excluded from prefixing.

The app's C adapters are src/backend_java.c, src/backend_bedrock.c, and src/backend_impl.h; they do not use Java placement rules for Bedrock.

Full license text is in the individual vendor directories and THIRD_PARTY_NOTICES.txt; the latter is embedded in the executable and accessible in Help → Licenses.
