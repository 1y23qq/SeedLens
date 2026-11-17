#include "export.h"
#include <fstream>
#include <limits>
namespace seedlens {
static std::string csv_field(std::string s) {
    size_t i = 0;
    while ((i = s.find('"', i)) != std::string::npos) {
        s.insert(i, 1, '"');
        i += 2;
    }
    return '"' + s + '"';
}
bool write_csv(const std::filesystem::path& path, const World& w, const std::vector<SLMarker>& markers,
               std::string& error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "Cannot create CSV file.";
        return false;
    }
    out << "\xef\xbb\xbf" << "edition,version,seed,dimension,type,x,y,z,biome,flags,variant\n";
    const auto& api = backend(w.edition);
    std::string version;
    for (int i = 0; i < api.version_count; ++i)
        if (api.versions[i].id == w.version)
            version = api.versions[i].name;
    for (auto& m : markers) {
        std::string name = m.kind == -1 ? "Estimated spawn" : m.kind == -2 ? "Biome" : kinds()[m.kind].en;
        out << (w.edition ? "Bedrock" : "Java") << ',' << csv_field(version) << ',' << signed_seed(w.seed)
            << ',' << w.dimension << ',' << csv_field(name) << ',' << m.x << ',';
        if (m.y != std::numeric_limits<int>::min())
            out << m.y;
        out << ',' << m.z << ',' << csv_field(biome_label(w, m.biome, false)) << ',' << m.flags << ','
            << m.variant << '\n';
    }
    out.flush();
    if (!out) {
        error = "CSV write failed.";
        return false;
    }
    error.clear();
    return true;
}
}
