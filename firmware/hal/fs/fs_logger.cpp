#include "hal/fs/fs_logger.h"

#include <cstdio>
#include <string>

namespace AxiomFS {

namespace {

constexpr std::size_t kMaxLogBytes = 4096;

} // namespace

void appendLog(FileSystem* fs, const char* path, const char* message) {
    if (!fs || !path || !message) {
        return;
    }

    std::string content;
    const ReadResult existing = fs->readFile(path);
    if (existing.ok()) {
        content.assign(existing.data.begin(), existing.data.end());
        if (content.size() > kMaxLogBytes) {
            content.erase(0, content.size() - kMaxLogBytes);
        }
    }

    content += message;
    content += "\n";
    (void)fs->writeFile(path, content);
}

void appendBootLog(FileSystem* fs, const char* message) {
    appendLog(fs, "logs/boot.log", message);
}

void appendFsLog(FileSystem* fs, const char* message) {
    appendLog(fs, "logs/fs.log", message);
}

} // namespace AxiomFS
