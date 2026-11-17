#include "engine.h"
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace seedlens {
std::filesystem::path settings_path(bool portable) {
#ifdef _WIN32
    if (portable) {
        wchar_t path[32768];
        DWORD n = GetModuleFileNameW(nullptr, path, 32768);
        return std::filesystem::path(std::wstring(path, n)).parent_path() / L"SeedLens.ini";
    }
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        std::filesystem::path p(path);
        CoTaskMemFree(path);
        return p / L"SeedLens" / L"session.ini";
    }
    return {};
#else
    (void)portable;
    const char* home=std::getenv("HOME");
    return home?std::filesystem::path(home)/".local/share/SeedLens/session.ini":std::filesystem::path("session.ini");
#endif
}
static bool valid_world(World& w) {
    if (w.edition < 0 || w.edition > 1 || (w.dimension != -1 && w.dimension != 0 && w.dimension != 1))
        return false;
    const auto& api = backend(w.edition);
    bool valid = false;
    for (int i = 0; i < api.version_count; ++i)
        if (api.versions[i].id == w.version)
            valid = true;
    w.y = std::clamp(w.y, -64, 320);
    if (w.edition)
        w.large = false;
    return valid;
}
bool load_session(const std::filesystem::path& path, Session& s) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    Session loaded;
    std::string line;
    while (std::getline(file, line)) {
        if (line.size() > 8192)
            continue;
        std::istringstream in(line);
        in.imbue(std::locale::classic());
        std::string key;
        in >> key;
        if (key == "world")
            in >> loaded.world.edition >> loaded.world.version >> loaded.world.dimension >> loaded.world.y >>
                loaded.world.seed >> loaded.world.large;
        else if (key == "seed")
            in >> std::quoted(loaded.seed_text);
        else if (key == "versions")
            in >> loaded.java_version >> loaded.bedrock_version;
        else if (key == "view")
            in >> loaded.x >> loaded.z >> loaded.blocks_per_pixel;
        else if (key == "ui")
            in >> loaded.japanese >> loaded.grid >> loaded.slime >> loaded.layers;
        else if (key == "bookmark" && loaded.bookmarks.size() < 1000) {
            Bookmark b;
            in >> b.world.edition >> b.world.version >> b.world.dimension >> b.world.y >> b.world.seed >>
                b.world.large >> b.x >> b.z >> std::quoted(b.name);
            if (in && valid_world(b.world) && b.name.size() <= 512 && std::abs(double(b.x)) <= world_limit &&
                std::abs(double(b.z)) <= world_limit)
                loaded.bookmarks.push_back(std::move(b));
        }
    }
    if (!valid_world(loaded.world) || !std::isfinite(loaded.x) || !std::isfinite(loaded.z) ||
        !std::isfinite(loaded.blocks_per_pixel))
        return false;
    loaded.x = std::clamp(loaded.x, -double(world_limit), double(world_limit));
    loaded.z = std::clamp(loaded.z, -double(world_limit), double(world_limit));
    loaded.blocks_per_pixel = std::clamp(loaded.blocks_per_pixel, 0.25, 512.0);
    loaded.layers &= (uint64_t(1) << SL_KIND_COUNT) - 1;
    uint64_t seed;
    std::string error;
    if (!parse_seed(loaded.seed_text, seed, error) || seed != loaded.world.seed)
        loaded.seed_text = signed_seed(loaded.world.seed);
    s = std::move(loaded);
    return true;
}
bool save_session(const std::filesystem::path& path, const Session& s) {
    if (path.empty())
        return false;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    auto temp = path;
    temp += ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        file.imbue(std::locale::classic());
        file << std::setprecision(17);
        file << "SeedLens 1\nworld " << s.world.edition << ' ' << s.world.version << ' ' << s.world.dimension
             << ' ' << s.world.y << ' ' << s.world.seed << ' ' << s.world.large << "\nseed "
             << std::quoted(s.seed_text) << "\nversions " << s.java_version << ' ' << s.bedrock_version
             << "\nview " << s.x << ' ' << s.z << ' ' << s.blocks_per_pixel << "\nui " << s.japanese << ' '
             << s.grid << ' ' << s.slime << ' ' << s.layers << '\n';
        for (auto& b : s.bookmarks)
            file << "bookmark " << b.world.edition << ' ' << b.world.version << ' ' << b.world.dimension
                 << ' ' << b.world.y << ' ' << b.world.seed << ' ' << b.world.large << ' ' << b.x << ' '
                 << b.z << ' ' << std::quoted(b.name) << '\n';
        file.flush();
        if (!file)
            return false;
    }
#ifdef _WIN32
    return MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temp,path,ec);
    return !ec;
#endif
}
} // namespace seedlens
