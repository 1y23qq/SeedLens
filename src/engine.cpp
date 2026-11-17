#include "engine.h"
#include "biome_palette.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace seedlens {
using Clock = std::chrono::steady_clock;
const SLBackend& backend(int edition) {
    return edition == 1 ? sl_bedrock : sl_java;
}
World default_world(int edition) {
    World w;
    w.edition = edition;
    w.version = backend(edition).versions[0].id;
    return w;
}
bool dimension_available(const World& world, int dimension) {
    return dimension >= -1 && dimension <= 1 && !(dimension == 1 && world.edition == 0 && world.version < 3);
}
int floor_div(int x, int d) {
    int q = x / d;
    return q - (x % d < 0);
}
std::string signed_seed(uint64_t v) {
    return std::to_string(static_cast<int64_t>(v));
}

bool parse_seed(const std::string& input, uint64_t& value, std::string& error) {
    error.clear();
    size_t first = input.find_first_not_of(" \t\r\n"), last = input.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) {
        error = "シードを入力してください / Enter a seed.";
        return false;
    }
    std::string s = input.substr(first, last - first + 1);
    bool numeric = true;
    size_t start = s[0] == '-' || s[0] == '+' ? 1 : 0;
    if (start == s.size())
        numeric = false;
    for (size_t i = start; i < s.size(); ++i)
        numeric &= s[i] >= '0' && s[i] <= '9';
    if (numeric) {
        int64_t result = 0;
        const char* begin = s.data() + (s[0] == '+' ? 1 : 0);
        auto [end, ec] = std::from_chars(begin, s.data() + s.size(), result);
        if (ec != std::errc() || end != s.data() + s.size()) {
            error =
                "シードは符号付き64ビット整数の範囲で入力してください / Seed is outside signed 64-bit range.";
            return false;
        }
        value = static_cast<uint64_t>(result);
        return true;
    }
    uint32_t hash = 0;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = static_cast<unsigned char>(s[i++]);
        int more = 0;
        if (cp >= 0xf0) {
            cp &= 7;
            more = 3;
        } else if (cp >= 0xe0) {
            cp &= 15;
            more = 2;
        } else if (cp >= 0xc0) {
            cp &= 31;
            more = 1;
        } else if (cp >= 0x80) {
            error = "Invalid UTF-8 seed.";
            return false;
        }
        while (more--) {
            if (i >= s.size() || (static_cast<unsigned char>(s[i]) & 0xc0) != 0x80) {
                error = "Invalid UTF-8 seed.";
                return false;
            }
            cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 63);
        }
        if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
            error = "Invalid Unicode seed.";
            return false;
        }
        if (cp > 0xffff) {
            cp -= 0x10000;
            hash = hash * 31 + 0xd800 + (cp >> 10);
            hash = hash * 31 + 0xdc00 + (cp & 1023);
        } else
            hash = hash * 31 + cp;
    }
    value = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(hash)));
    return true;
}

int sample_scale(double bpp) {
    if (bpp < 2)
        return 1;
    if (bpp < 8)
        return 4;
    if (bpp < 32)
        return 16;
    if (bpp < 128)
        return 64;
    return 256;
}
static constexpr uint32_t rgb(int r, int g, int b) {
    return 0xff000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | r;
}
bool legacy_palette(const World& w) {
    const auto& api = backend(w.edition);
    for (int i = 0; i < api.version_count; ++i)
        if (api.versions[i].id == w.version) {
            std::string name = api.versions[i].name;
            if (name.starts_with("Beta"))
                return true;
            if (name.starts_with("1.")) {
                int minor = 0;
                std::from_chars(name.data() + 2, name.data() + name.size(), minor);
                return minor < 18;
            }
        }
    return false;
}
uint32_t biome_color(int b, bool legacy) {
    auto make = [](bool old) {
        unsigned char source[256][3]{};
        backend(0).palette(source);
        std::array<uint32_t, 256> values{};
        for (int i = 0; i < 256; ++i)
            values[i] = rgb(source[i][0], source[i][1], source[i][2]);
        for (auto entry : reference_palette)
            if (!entry.legacy)
                values[entry.id] = rgb(entry.r, entry.g, entry.b);
        if (old)
            for (auto entry : reference_palette)
                if (entry.legacy)
                    values[entry.id] = rgb(entry.r, entry.g, entry.b);
        return values;
    };
    static const auto modern = make(false), old = make(true);
    return b >= 0 && b < 256 ? (legacy ? old[b] : modern[b]) : rgb(34, 39, 45);
}

std::string biome_label(const World& w, int id, bool ja) {
    static const std::pair<int, const char*> names[] = {{0, "海洋"},
                                                        {1, "平原"},
                                                        {2, "砂漠"},
                                                        {3, "吹きさらしの丘"},
                                                        {4, "森林"},
                                                        {5, "タイガ"},
                                                        {6, "沼地"},
                                                        {7, "河川"},
                                                        {8, "ネザーの荒地"},
                                                        {9, "ジ・エンド"},
                                                        {10, "凍った海"},
                                                        {11, "凍った川"},
                                                        {12, "雪原"},
                                                        {13, "雪山"},
                                                        {14, "キノコ島"},
                                                        {15, "キノコ島の海岸"},
                                                        {16, "砂浜"},
                                                        {17, "砂漠の丘陵"},
                                                        {18, "森のある丘陵"},
                                                        {19, "タイガの丘陵"},
                                                        {21, "ジャングル"},
                                                        {22, "ジャングルの丘陵"},
                                                        {23, "まばらなジャングル"},
                                                        {24, "深海"},
                                                        {25, "石だらけの海岸"},
                                                        {26, "雪の砂浜"},
                                                        {27, "シラカバの森"},
                                                        {28, "シラカバの丘陵"},
                                                        {29, "暗い森"},
                                                        {30, "雪のタイガ"},
                                                        {32, "マツの原生林"},
                                                        {33, "巨大樹のタイガの丘陵"},
                                                        {34, "吹きさらしの森"},
                                                        {35, "サバンナ"},
                                                        {36, "サバンナの高原"},
                                                        {37, "荒野"},
                                                        {38, "森のある荒野"},
                                                        {39, "荒野の高原"},
                                                        {40, "小さなエンドの島"},
                                                        {41, "エンドの内陸部"},
                                                        {42, "エンドの高地"},
                                                        {43, "エンドのやせ地"},
                                                        {44, "暖かい海"},
                                                        {45, "ぬるい海"},
                                                        {46, "冷たい海"},
                                                        {48, "ぬるい深海"},
                                                        {49, "冷たい深海"},
                                                        {50, "凍った深海"},
                                                        {129, "ヒマワリ平原"},
                                                        {132, "花の森"},
                                                        {140, "氷樹"},
                                                        {155, "シラカバの原生林"},
                                                        {160, "トウヒの原生林"},
                                                        {165, "侵食された荒野"},
                                                        {168, "竹林"},
                                                        {169, "竹林の丘陵"},
                                                        {170, "ソウルサンドの谷"},
                                                        {171, "真紅の森"},
                                                        {172, "歪んだ森"},
                                                        {173, "玄武岩の三角州"},
                                                        {174, "鍾乳洞"},
                                                        {175, "繁茂した洞窟"},
                                                        {177, "牧草地"},
                                                        {178, "林"},
                                                        {179, "雪の斜面"},
                                                        {180, "凍った山頂"},
                                                        {181, "尖った山頂"},
                                                        {182, "石だらけの山頂"},
                                                        {183, "ディープダーク"},
                                                        {184, "マングローブの沼地"},
                                                        {185, "サクラの林"},
                                                        {186, "ペールガーデン"}};
    if (ja)
        for (auto& entry : names)
            if (entry.first == id)
                return entry.second;
    const char* raw = backend(w.edition).biome_name(w.version, id);
    if (!raw || !*raw)
        return ja ? "不明" : "Unknown";
    std::string s(raw);
    bool upper = true;
    for (char& c : s) {
        if (c == '_') {
            c = ' ';
            upper = true;
        } else if (upper) {
            if (c >= 'a' && c <= 'z')
                c -= 'a' - 'A';
            upper = false;
        }
    }
    return s;
}

const std::array<KindInfo, SL_KIND_COUNT>& kinds() {
    static const std::array<KindInfo, SL_KIND_COUNT> data = {
        {{SL_VILLAGE, 0, "Village", "村", "house", rgb(235, 189, 95), false},
         {SL_DESERT_TEMPLE, 0, "Desert pyramid", "砂漠の寺院", "pyramid", rgb(233, 176, 102), false},
         {SL_JUNGLE_TEMPLE, 0, "Jungle temple", "ジャングルの寺院", "landmark", rgb(150, 198, 119), false},
         {SL_HUT, 0, "Swamp hut", "沼地の小屋", "house", rgb(175, 186, 115), false},
         {SL_IGLOO, 0, "Igloo", "イグルー", "house", rgb(190, 218, 235), false},
         {SL_MONUMENT, 0, "Ocean monument", "海底神殿", "landmark", rgb(109, 206, 198), false},
         {SL_MANSION, 0, "Woodland mansion", "森の洋館", "castle", rgb(190, 145, 215), false},
         {SL_OUTPOST, 0, "Pillager outpost", "ピリジャーの前哨基地", "flag", rgb(224, 129, 107), false},
         {SL_PORTAL, 0, "Ruined portal", "荒廃したポータル", "door-open", rgb(183, 141, 230), false},
         {SL_PORTAL_N, -1, "Ruined portal", "荒廃したポータル", "door-open", rgb(183, 141, 230), false},
         {SL_ANCIENT_CITY, 0, "Ancient city", "古代都市", "building-2", rgb(93, 200, 202), false},
         {SL_TREASURE, 0, "Buried treasure", "埋もれた宝", "gem", rgb(240, 198, 108), true},
         {SL_MINESHAFT, 0, "Mineshaft", "廃坑", "pickaxe", rgb(199, 165, 132), true},
         {SL_STRONGHOLD, 0, "Stronghold", "要塞", "castle", rgb(136, 181, 226), false},
         {SL_SHIPWRECK, 0, "Shipwreck", "難破船", "ship", rgb(192, 156, 117), false},
         {SL_OCEAN_RUIN, 0, "Ocean ruins", "海底遺跡", "waves", rgb(133, 193, 200), false},
         {SL_TRAIL_RUINS, 0, "Trail ruins", "旅路の遺跡", "shovel", rgb(207, 158, 124), false},
         {SL_TRIAL_CHAMBERS, 0, "Trial chambers", "トライアルチャンバー", "vault", rgb(224, 159, 104), false},
         {SL_CAMP, 0, "Abandoned camp", "Abandoned Camp", "tent", rgb(235, 176, 93), false},
         {SL_FORTRESS, -1, "Nether fortress", "ネザー要塞", "castle", rgb(238, 145, 119), false},
         {SL_BASTION, -1, "Bastion remnant", "砦の遺跡", "landmark", rgb(230, 189, 103), false},
         {SL_NETHER_FOSSIL, -1, "Nether fossil", "ネザーの化石", "bone", rgb(218, 211, 179), true},
         {SL_END_CITY, 1, "End city", "エンドシティ", "building-2", rgb(204, 157, 228), false},
         {SL_GATEWAY, 1, "End gateway", "エンドゲートウェイ", "door-open", rgb(157, 212, 214), true},
         {SL_END_ISLAND, 1, "Small end islands", "小さなエンドの島", "mountain", rgb(204, 202, 148), true},
         {SL_DESERT_WELL, 0, "Desert well", "砂漠の井戸", "droplets", rgb(230, 209, 139), true},
         {SL_GEODE, 0, "Amethyst geode", "アメジストジオード", "gem", rgb(191, 156, 225), true},
         {SL_DUNGEON, 0, "Monster room", "モンスタールーム", "box", rgb(186, 195, 183), true},
         {SL_RAVINE, 0, "Ravine", "峡谷", "mountain", rgb(162, 170, 169), true},
         {SL_LAVA_LAKE, 0, "Lava lake", "溶岩湖", "flame", rgb(239, 140, 87), true},
         {SL_FOSSIL, 0, "Overworld fossil", "オーバーワールドの化石", "bone", rgb(214, 206, 177), true}}};
    return data;
}
uint64_t default_layers() {
    uint64_t mask = 0;
    for (auto& k : kinds())
        if (!k.dense && k.kind != SL_PORTAL && k.kind != SL_PORTAL_N && k.kind != SL_SHIPWRECK &&
            k.kind != SL_OCEAN_RUIN)
            mask |= uint64_t(1) << k.kind;
    return mask;
}

Context::Context(World w)
    : world(w), api_(backend(w.edition)), ctx_(api_.create(w.version, w.seed, w.dimension, w.large)) {
    if (!ctx_)
        throw std::runtime_error("Cannot initialize the world generator.");
}
Context::~Context() {
    api_.destroy(ctx_);
}
int Context::biome(int x, int y, int z) const {
    return api_.biome(ctx_, x, y, z);
}
bool Context::supports(int k) const {
    return api_.supports(ctx_, k) != 0;
}
bool Context::generate(int scale, int x, int z, int w, int h, int y, int* out) const {
    return api_.generate(ctx_, scale, x, z, w, h, y, out) == 0;
}
int Context::scan(int k, int x0, int z0, int x1, int z1, SLMarker* out, int cap, SLCancel cancel,
                  void* user) const {
    return api_.scan(ctx_, k, x0, z0, x1, z1, out, cap, cancel, user);
}
std::pair<int, int> Context::spawn() const {
    int x = 0, z = 0;
    api_.spawn(ctx_, &x, &z);
    return {x, z};
}
bool Context::slime(int x, int z) const {
    return api_.slime(world.seed, x, z) != 0;
}

size_t TileHash::operator()(const TileKey& k) const {
    uint64_t h = uint32_t(k.x) * 0x9e3779b185ebca87ULL;
    h ^= uint32_t(k.z) * 0xc2b2ae3d27d4eb4fULL;
    return size_t(h ^ (k.epoch * 107ULL) ^ (uint64_t(k.scale) << 41));
}
TilePool::TilePool(std::function<void()> wake) : wake_(std::move(wake)) {
    unsigned count = std::clamp(std::thread::hardware_concurrency() / 2, 2u, 4u);
    for (unsigned i = 0; i < count; ++i)
        threads_.emplace_back([this] { worker(); });
}
TilePool::~TilePool() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
        queue_.clear();
    }
    cv_.notify_all();
    for (auto& t : threads_)
        t.join();
}
uint64_t TilePool::reset() {
    std::lock_guard lock(mutex_);
    ++epoch_;
    queue_.clear();
    done_.clear();
    pending_.clear();
    return epoch_;
}
void TilePool::request(TileKey key, World w) {
    std::lock_guard lock(mutex_);
    if (key.epoch != epoch_ || pending_.contains(key) || queue_.size() > 160)
        return;
    pending_.insert(key);
    queue_.push_back({key, w});
    cv_.notify_one();
}
std::vector<Tile> TilePool::collect() {
    std::lock_guard lock(mutex_);
    std::vector<Tile> result;
    while (!done_.empty()) {
        pending_.erase(done_.front().key);
        result.push_back(std::move(done_.front()));
        done_.pop_front();
    }
    return result;
}
size_t TilePool::pending() const {
    std::lock_guard lock(mutex_);
    return pending_.size();
}
void TilePool::worker() {
    std::unique_ptr<Context> ctx;
    for (;;) {
        Job job;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_)
                return;
            job = queue_.front();
            queue_.pop_front();
        }
        if (job.key.epoch != epoch_)
            continue;
        Tile tile;
        tile.key = job.key;
        auto start = Clock::now();
        try {
            if (!ctx || ctx->world != job.world)
                ctx = std::make_unique<Context>(job.world);
            tile.biomes.resize(tile_size * tile_size);
            tile.ok = ctx->generate(job.key.scale, job.key.x * tile_size, job.key.z * tile_size, tile_size,
                                    tile_size, job.world.y, tile.biomes.data());
            tile.pixels.resize(tile_size * tile_size);
            for (size_t i = 0; i < tile.pixels.size(); ++i)
                tile.pixels[i] = biome_color(tile.biomes[i], legacy_palette(job.world));
        } catch (const std::exception&) {
            tile.ok = false;
        }
        tile.milliseconds = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        {
            std::lock_guard lock(mutex_);
            if (job.key.epoch == epoch_)
                done_.push_back(std::move(tile));
        }
        if (wake_)
            wake_();
    }
}

SearchWorker::SearchWorker(std::function<void()> wake) : wake_(std::move(wake)) {
    thread_ = std::thread([this] { worker(); });
}
SearchWorker::~SearchWorker() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
        ++serial_;
    }
    cv_.notify_all();
    thread_.join();
}
uint64_t SearchWorker::submit(SearchRequest request) {
    std::lock_guard lock(mutex_);
    uint64_t id = ++serial_;
    request_ = std::make_pair(id, request);
    result_.reset();
    busy_ = true;
    progress_ = 0;
    cv_.notify_one();
    return id;
}
void SearchWorker::cancel() {
    std::lock_guard lock(mutex_);
    ++serial_;
    request_.reset();
    result_.reset();
    busy_ = false;
    progress_ = 0;
}
std::optional<SearchResult> SearchWorker::collect() {
    std::lock_guard lock(mutex_);
    auto r = std::move(result_);
    result_.reset();
    return r;
}
void SearchWorker::worker() {
    std::unique_ptr<Context> ctx;
    for (;;) {
        uint64_t id;
        SearchRequest req;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || request_.has_value(); });
            if (stop_)
                return;
            id = request_->first;
            req = request_->second;
            request_.reset();
        }
        SearchResult result;
        result.id = id;
        result.request = req;
        auto start = Clock::now();
        struct Cancel {
            std::atomic<uint64_t>* serial;
            uint64_t id;
        } state{&serial_, id};
        auto cancel = [](void* p) -> int {
            auto& s = *static_cast<Cancel*>(p);
            return s.serial->load() != s.id;
        };
        try {
            if (!ctx || ctx->world != req.world)
                ctx = std::make_unique<Context>(req.world);
            if (req.spawn) {
                auto [x, z] = ctx->spawn();
                SLMarker m{};
                m.kind = -1;
                m.x = x;
                m.z = z;
                m.y = std::numeric_limits<int>::min();
                m.flags = SL_APPROXIMATE;
                result.markers.push_back(m);
            } else if (req.biome >= 0) {
                int cx = (req.x0 + req.x1) / 2, cz = (req.z0 + req.z1) / 2;
                int radius = std::max(req.x1 - req.x0, req.z1 - req.z0) / 2;
                constexpr int step = 32;
                for (int r = 0; r <= radius && !cancel(&state) && result.markers.size() < 64; r += step) {
                    progress_ = float(r) / std::max(radius, 1);
                    for (int dz = -r; dz <= r && !cancel(&state); dz += step) {
                        for (int dx = -r; dx <= r; dx += step) {
                            if (r > 0 && std::abs(dx) != r && std::abs(dz) != r)
                                continue;
                            int x = cx + dx, z = cz + dz;
                            if (ctx->biome(x, req.world.y, z) != req.biome)
                                continue;
                            bool near = false;
                            for (auto& m : result.markers)
                                if (std::hypot(double(m.x - x), double(m.z - z)) < 256)
                                    near = true;
                            if (!near) {
                                SLMarker m{};
                                m.kind = -2;
                                m.x = x;
                                m.z = z;
                                m.y = req.world.y;
                                m.biome = req.biome;
                                result.markers.push_back(m);
                            }
                        }
                    }
                }
            } else {
                int total = 0, done = 0;
                for (auto& k : kinds())
                    if (req.layers & (uint64_t(1) << k.kind))
                        ++total;
                for (auto& k : kinds()) {
                    if (!(req.layers & (uint64_t(1) << k.kind)) || !ctx->supports(k.kind))
                        continue;
                    if (cancel(&state))
                        break;
                    int width = req.x1 - req.x0, height = req.z1 - req.z0;
                    if ((k.dense && std::max(width, height) > 8192) ||
                        (k.kind == SL_DUNGEON && std::max(width, height) > 256)) {
                        result.omitted |= uint64_t(1) << k.kind;
                        continue;
                    }
                    int cap = std::min(2500, req.cap - int(result.markers.size()));
                    if (cap <= 0) {
                        result.truncated = true;
                        break;
                    }
                    std::vector<SLMarker> markers(cap);
                    int count = ctx->scan(k.kind, req.x0, req.z0, req.x1, req.z1, markers.data(), cap, cancel,
                                          &state);
                    if (count < 0) {
                        result.error = "The generator could not complete this search.";
                        continue;
                    }
                    if (count == cap)
                        result.truncated = true;
                    markers.resize(size_t(count));
                    result.markers.insert(result.markers.end(), markers.begin(), markers.end());
                    progress_ = float(++done) / std::max(total, 1);
                }
            }
            double cx = (req.x0 + req.x1) / 2.0, cz = (req.z0 + req.z1) / 2.0;
            std::sort(result.markers.begin(), result.markers.end(), [=](const auto& a, const auto& b) {
                return std::hypot(a.x - cx, a.z - cz) < std::hypot(b.x - cx, b.z - cz);
            });
        } catch (const std::exception& e) {
            result.error = e.what();
        }
        result.milliseconds = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        {
            std::lock_guard lock(mutex_);
            if (id == serial_) {
                result_ = std::move(result);
                busy_ = false;
                progress_ = 1;
            }
        }
        if (wake_)
            wake_();
    }
}
} // namespace seedlens
