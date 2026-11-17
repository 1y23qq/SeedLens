#include "engine.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace seedlens;
static int checks = 0;
static void check(bool condition, const char* message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    try {
        uint64_t value = 0;
        std::string error;
        check(parse_seed("-9223372036854775808", value, error) && value == 0x8000000000000000ULL,
              "Minimum signed seed");
        check(parse_seed("9223372036854775807", value, error) && value == 0x7fffffffffffffffULL,
              "Maximum signed seed");
        check(!parse_seed("9223372036854775808", value, error), "Reject overflowing numeric seed");
        check(parse_seed("hello", value, error) && value == 99162322, "Java string seed hash");
        check(parse_seed("\xf0\x9f\x98\x80", value, error) && value == 1772899, "UTF-16 surrogate seed hash");
        check(!parse_seed(" ", value, error), "Reject blank seed");
        check(floor_div(-1, 16) == -1 && floor_div(-16, 16) == -1 && floor_div(-17, 16) == -2,
              "Negative chunk boundaries");
        check(biome_color(188) == 0xff353f9au, "Reference Dappled Forest palette");
        check(biome_color(34, false) == 0xff1c5522u, "Reference modern Windswept Forest palette");
        check(biome_color(34, true) == 0xff507050u, "Reference legacy Wooded Mountains palette");
        World java = default_world();
        java.seed = static_cast<uint64_t>(INT64_C(-3829811542736183482));
        Context jc(java);
        check(jc.biome(68148, 77, 80990) == 188, "Java 26.3 upstream reference: Dappled Forest");
        World end = java;
        end.dimension = 1;
        end.seed = 856461443495910397ULL;
        Context ec(end);
        check(ec.biome(-371136, 64, -64) == 42, "Java 26.3 corrected End ring fixture");
        for (int edition = 0; edition < 2; ++edition) {
            const auto& api = backend(edition);
            for (int i = 0; i < api.version_count; ++i) {
                World world = default_world(edition);
                world.version = api.versions[i].id;
                for (int dim : {0, -1, 1}) {
                    if (!dimension_available(world, dim))
                        continue;
                    world.dimension = dim;
                    Context ctx(world);
                    std::vector<int> ids(16 * 16);
                    if (!ctx.generate(4, -8, -8, 16, 16, 64, ids.data()))
                        throw std::runtime_error(std::string("Biome generation: ") +
                                                 (edition ? "Bedrock " : "Java ") + api.versions[i].name +
                                                 " dim " + std::to_string(dim));
                    check(true, "Biome generation for edition/version/dimension");
                    check(std::all_of(ids.begin(), ids.end(), [](int id) { return id >= 0 && id < 256; }),
                          "All generated biome IDs are valid");
                    check(ctx.biome(-1, 64, -1) >= 0, "Negative-coordinate biome query");
                    for (int scale : {1, 4, 16, 64, 256}) {
                        std::vector<int> scaled(16);
                        check(ctx.generate(scale, -2, -2, 4, 4, 64, scaled.data()),
                              "Every map zoom scale generates");
                        check(std::all_of(scaled.begin(), scaled.end(),
                                          [](int id) { return id >= 0 && id < 256; }),
                              "Scaled biome IDs are valid");
                    }
                }
            }
        }
        World bed = default_world(1);
        Context bc(bed);
        World other = bed;
        other.seed = 123456789;
        Context bo(other);
        for (int x = -3; x <= 3; ++x)
            for (int z = -3; z <= 3; ++z)
                check(bc.slime(x, z) == bo.slime(x, z), "Bedrock slime chunks are seed-independent");
        for (int edition = 0; edition < 2; ++edition) {
            World world = default_world(edition);
            Context ctx(world);
            std::vector<SLMarker> markers(500);
            int n = ctx.scan(SL_VILLAGE, -4096, -4096, 4096, 4096, markers.data(), 500, nullptr, nullptr);
            check(n > 0 && n < 500, "Real village scan returns results");
            for (int i = 0; i < n; ++i)
                check(markers[i].kind == SL_VILLAGE && std::abs(markers[i].x) <= 4096 &&
                          std::abs(markers[i].z) <= 4096,
                      "Structure bounds and kind");
            world.dimension = -1;
            Context nether(world);
            check(nether.supports(SL_FORTRESS) && !nether.supports(SL_VILLAGE), "Dimension capability gates");
            n = nether.scan(SL_FORTRESS, -2048, -2048, 2048, 2048, markers.data(), 500, nullptr, nullptr);
            check(n > 0, "Nether fortress scan");
            world.dimension = 1;
            Context the_end(world);
            n = the_end.scan(SL_END_CITY, -4096, -4096, 4096, 4096, markers.data(), 500, nullptr, nullptr);
            check(n > 0, "End city scan with terrain validation");
        }
        auto start = std::chrono::steady_clock::now();
        {
            TilePool pool;
            World w = default_world();
            TileKey key{pool.epoch(), 4, -1, -1};
            pool.request(key, w);
            pool.request(key, w);
            check(pool.pending() == 1, "Duplicate tile work is coalesced");
            std::vector<Tile> result;
            while (result.empty() && std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                result = pool.collect();
            }
            check(result.size() == 1 && result[0].ok && result[0].pixels.size() == 16384,
                  "Background tile generation");
            pool.reset();
            check(pool.pending() == 0, "World changes clear pending work");
        }
        {
            SearchWorker worker;
            SearchRequest req;
            req.world = default_world();
            req.x0 = req.z0 = -2048;
            req.x1 = req.z1 = 2048;
            req.layers = uint64_t(1) << SL_VILLAGE;
            uint64_t id = worker.submit(req);
            std::optional<SearchResult> result;
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (!result && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                result = worker.collect();
            }
            check(result && result->id == id && !result->markers.empty(), "Background structure search");
            req.layers = uint64_t(1) << SL_GEODE;
            req.x0 = req.z0 = -32768;
            req.x1 = req.z1 = 32768;
            worker.submit(req);
            worker.cancel();
            check(!worker.busy(), "Search cancellation");
        }
        auto path = std::filesystem::temp_directory_path() /
                    ("SeedLens-test-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".ini");
        Session session;
        session.seed_text = "hello";
        parse_seed(session.seed_text, session.world.seed, error);
        session.x = -1234;
        session.bookmarks.push_back({session.world, -32, 48, "日本語 \"bookmark\""});
        check(save_session(path, session), "Atomic settings save");
        Session loaded;
        check(load_session(path, loaded) && loaded.world == session.world && loaded.bookmarks.size() == 1 &&
                  loaded.bookmarks[0].name == session.bookmarks[0].name,
              "Unicode settings and bookmark round trip");
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::printf("PASS: %d checks; %d Java and %d Bedrock profiles, three dimensions, native scans, async "
                    "workers, persistence.\n",
                    checks, backend(0).version_count, backend(1).version_count);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %d checks: %s\n", checks, error.what());
        return 1;
    }
}
