#pragma once

#include "app/graphing/graph_app.h"
#include "hal/fs/axiom_fs.h"

#include <string>
#include <vector>

struct GraphSessionFunction {
    std::string expression;
    bool enabled = false;
};

struct GraphSessionData {
    std::string name;
    std::vector<GraphSessionFunction> functions;
    GraphWindow window{};
    bool angleRadians = true;
};

class GraphSessionStorage {
public:
    static constexpr const char* kExtension = ".mi23graph";

    static std::string sanitizeFileName(const std::string& name);
    static std::string graphPath(const std::string& fileName);
    static std::string defaultFileName(AxiomFS::FileSystem& fs);

    static bool save(AxiomFS::FileSystem& fs,
                     const std::string& preferredName,
                     const GraphSessionData& session,
                     std::string* savedFileName = nullptr);
    static bool load(AxiomFS::FileSystem& fs,
                     const std::string& fileName,
                     GraphSessionData& session);
    static bool deleteFile(AxiomFS::FileSystem& fs, const std::string& fileName);
    static AxiomFS::ListResult list(AxiomFS::FileSystem& fs);
};
