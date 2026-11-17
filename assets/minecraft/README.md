# Minecraft marker artwork

The embedded marker atlas uses selected Minecraft Java 26.3 texture fragments from Mojang's official client resources. These identify game features: villager and illager faces, soul lantern, portal frame, blaze, piglin, purpur, elytra, chest, rail, spawners, campfire and other game items/blocks.

Minecraft textures and their derivatives remain copyright Mojang AB / Microsoft and are not covered by the SeedLens source-code MIT license. See the [Minecraft Usage Guidelines](https://www.minecraft.net/en-us/usage-guidelines).

The exact source paths, original texture hashes and official client download hash are recorded in sources.json. The atlas is assets/minecraft-markers.png. It is embedded as resource 300 and decoded with Windows WIC; it does not require an external image file when running.

To regenerate with the official Java 26.3 client resources and Pillow:

    python scripts/extract_minecraft_icons.py path/to/client.jar

The small atlas contains 35 feature/variant markers and preserves pixel edges with nearest-neighbor sampling. UI controls use Lucide separately. The feature-to-texture choices were checked against the actual [Chunkbase Seed Map](https://www.chunkbase.com/apps/seed-map).

NOT AN OFFICIAL MINECRAFT PRODUCT. NOT APPROVED BY OR ASSOCIATED WITH MOJANG OR MICROSOFT.
