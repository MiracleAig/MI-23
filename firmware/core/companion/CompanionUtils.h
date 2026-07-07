#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Companion {

constexpr std::size_t kMaxMessageLength = 2048;
constexpr std::size_t kMaxResponseLength = 4096;
constexpr std::size_t kMaxPathLength = 160;
constexpr std::size_t kMaxFileChunkSize = 1024;
constexpr std::size_t kMaxCompanionFileSize = 64 * 1024;

void appendJsonString(std::string& out, const std::string& value);
std::string jsonEscape(const std::string& value);

std::string base64Encode(const uint8_t* data, std::size_t size);
bool base64Decode(const std::string& text, std::vector<uint8_t>& out);

bool validateVirtualPath(const std::string& path,
                         bool allowRoot,
                         std::string& outFilesystemPath,
                         std::string* errorMessage = nullptr);
std::string virtualPathFromFilesystemPath(const std::string& filesystemPath);

} // namespace Companion
