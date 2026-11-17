#pragma once
#include "backend.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace seedlens {
constexpr int tile_size = 128;
constexpr int world_limit = 29999984;
struct World {
    int edition = 0, version = 0, dimension = 0, y = 320;
    uint64_t seed = 8675309;
    bool large = false;
    bool operator==(const World&) const = default;
};
const SLBackend& backend(int edition);
World default_world(int edition = 0);
bool dimension_available(const World&, int dimension);
bool parse_seed(const std::string&, uint64_t&, std::string& error);
std::string signed_seed(uint64_t);
int floor_div(int x, int d);
int sample_scale(double blocks_per_pixel);
uint32_t biome_color(int id, bool legacy = false);
bool legacy_palette(const World&);
std::string biome_label(const World&, int id, bool japanese);
struct KindInfo {
    int kind, dimension;
    const char *en, *ja, *icon;
    uint32_t color;
    bool dense;
};
const std::array<KindInfo, SL_KIND_COUNT>& kinds();
uint64_t default_layers();

class Context {
  public:
    explicit Context(World);
    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    int biome(int x, int y, int z) const;
    bool supports(int kind) const;
    bool generate(int scale, int x, int z, int w, int h, int y, int* out) const;
    int scan(int kind, int x0, int z0, int x1, int z1, SLMarker* out, int cap, SLCancel, void*) const;
    std::pair<int, int> spawn() const;
    bool slime(int cx, int cz) const;
    const World world;

  private:
    const SLBackend& api_;
    void* ctx_;
};

struct TileKey {
    uint64_t epoch = 0;
    int scale = 4, x = 0, z = 0;
    bool operator==(const TileKey&) const = default;
};
struct TileHash {
    size_t operator()(const TileKey&) const;
};
struct Tile {
    TileKey key;
    std::vector<uint32_t> pixels;
    std::vector<int> biomes;
    double milliseconds = 0;
    bool ok = false;
};
class TilePool {
  public:
    explicit TilePool(std::function<void()> wake = {});
    ~TilePool();
    uint64_t reset();
    void request(TileKey, World);
    std::vector<Tile> collect();
    size_t pending() const;
    uint64_t epoch() const {
        return epoch_;
    }

  private:
    struct Job {
        TileKey key;
        World world;
    };
    void worker();
    std::atomic<uint64_t> epoch_{1};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    std::deque<Job> queue_;
    std::deque<Tile> done_;
    std::unordered_set<TileKey, TileHash> pending_;
    std::vector<std::thread> threads_;
    std::function<void()> wake_;
};
struct SearchRequest {
    World world;
    int x0 = 0, z0 = 0, x1 = 0, z1 = 0;
    uint64_t layers = 0;
    int biome = -1;
    bool spawn = false;
    int cap = 10000;
};
struct SearchResult {
    uint64_t id = 0;
    SearchRequest request;
    std::vector<SLMarker> markers;
    uint64_t omitted = 0;
    bool truncated = false;
    double milliseconds = 0;
    std::string error;
};
class SearchWorker {
  public:
    explicit SearchWorker(std::function<void()> wake = {});
    ~SearchWorker();
    uint64_t submit(SearchRequest);
    void cancel();
    std::optional<SearchResult> collect();
    bool busy() const {
        return busy_;
    }
    float progress() const {
        return progress_;
    }

  private:
    void worker();
    std::function<void()> wake_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    std::optional<std::pair<uint64_t, SearchRequest>> request_;
    std::optional<SearchResult> result_;
    std::atomic<uint64_t> serial_{0};
    std::atomic<bool> busy_{false};
    std::atomic<float> progress_{0};
};
struct Bookmark {
    World world;
    int x = 0, z = 0;
    std::string name;
    bool operator==(const Bookmark&) const = default;
};
struct Session {
    World world = default_world();
    int java_version = backend(0).versions[0].id, bedrock_version = backend(1).versions[0].id;
    std::string seed_text = "8675309";
    double x = 0, z = 0, blocks_per_pixel = 4;
    uint64_t layers = default_layers();
    bool japanese = true, grid = false, slime = false;
    std::vector<Bookmark> bookmarks;
    bool operator==(const Session&) const = default;
};
std::filesystem::path settings_path(bool portable);
bool load_session(const std::filesystem::path&, Session&);
bool save_session(const std::filesystem::path&, const Session&);
} // namespace seedlens
