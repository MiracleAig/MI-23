#include "core/companion/CompanionUtils.h"

#include <cctype>

namespace Companion {

namespace {

constexpr char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64Value(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A';
    }
    if (ch >= 'a' && ch <= 'z') {
        return 26 + (ch - 'a');
    }
    if (ch >= '0' && ch <= '9') {
        return 52 + (ch - '0');
    }
    if (ch == '+') {
        return 62;
    }
    if (ch == '/') {
        return 63;
    }
    return -1;
}

bool invalidPathCharacter(char ch) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    return uch < 0x20u || ch == '\\';
}

} // namespace

void appendJsonString(std::string& out, const std::string& value) {
    out.push_back('"');
    for (const char ch : value) {
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20u) {
                    out += "\\u00";
                    const char hex[] = "0123456789abcdef";
                    out.push_back(hex[(static_cast<unsigned char>(ch) >> 4u) & 0x0Fu]);
                    out.push_back(hex[static_cast<unsigned char>(ch) & 0x0Fu]);
                } else {
                    out.push_back(ch);
                }
                break;
        }
    }
    out.push_back('"');
}

std::string jsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    appendJsonString(escaped, value);
    return escaped;
}

std::string base64Encode(const uint8_t* data, std::size_t size) {
    if (!data && size != 0) {
        return {};
    }

    std::string out;
    out.reserve(((size + 2u) / 3u) * 4u);
    for (std::size_t i = 0; i < size; i += 3u) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = (i + 1u < size) ? data[i + 1u] : 0u;
        const uint32_t b2 = (i + 2u < size) ? data[i + 2u] : 0u;
        const uint32_t triple = (b0 << 16u) | (b1 << 8u) | b2;

        out.push_back(kBase64Alphabet[(triple >> 18u) & 0x3Fu]);
        out.push_back(kBase64Alphabet[(triple >> 12u) & 0x3Fu]);
        out.push_back(i + 1u < size ? kBase64Alphabet[(triple >> 6u) & 0x3Fu] : '=');
        out.push_back(i + 2u < size ? kBase64Alphabet[triple & 0x3Fu] : '=');
    }
    return out;
}

bool base64Decode(const std::string& text, std::vector<uint8_t>& out) {
    out.clear();
    if (text.empty()) {
        return true;
    }
    if ((text.size() % 4u) != 0u) {
        return false;
    }

    out.reserve((text.size() / 4u) * 3u);
    for (std::size_t i = 0; i < text.size(); i += 4u) {
        int values[4] = {};
        bool padded[4] = {};
        for (int j = 0; j < 4; ++j) {
            const char ch = text[i + static_cast<std::size_t>(j)];
            if (ch == '=') {
                padded[j] = true;
                values[j] = 0;
            } else {
                values[j] = base64Value(ch);
                if (values[j] < 0) {
                    return false;
                }
            }
        }

        if (padded[0] || padded[1] || (padded[2] && !padded[3])) {
            return false;
        }
        if ((padded[2] || padded[3]) && i + 4u != text.size()) {
            return false;
        }

        const uint32_t triple =
            (static_cast<uint32_t>(values[0]) << 18u) |
            (static_cast<uint32_t>(values[1]) << 12u) |
            (static_cast<uint32_t>(values[2]) << 6u) |
            static_cast<uint32_t>(values[3]);

        out.push_back(static_cast<uint8_t>((triple >> 16u) & 0xFFu));
        if (!padded[2]) {
            out.push_back(static_cast<uint8_t>((triple >> 8u) & 0xFFu));
        }
        if (!padded[3]) {
            out.push_back(static_cast<uint8_t>(triple & 0xFFu));
        }
    }

    return true;
}

bool validateVirtualPath(const std::string& path,
                         bool allowRoot,
                         std::string& outFilesystemPath,
                         std::string* errorMessage) {
    outFilesystemPath.clear();

    auto setError = [errorMessage](const char* message) {
        if (errorMessage) {
            *errorMessage = message ? message : "Invalid path";
        }
        return false;
    };

    if (path.empty()) {
        return setError("Path is required");
    }
    if (path.size() > kMaxPathLength) {
        return setError("Path is too long");
    }
    if (path[0] != '/') {
        return setError("Path must begin with /");
    }
    if (path == "/") {
        return allowRoot ? true : setError("Path must name a file or directory");
    }
    if (path.back() == '/') {
        return setError("Trailing slash is not allowed");
    }

    std::size_t start = 1;
    while (start < path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::size_t end = slash == std::string::npos ? path.size() : slash;
        const std::string component = path.substr(start, end - start);
        if (component.empty()) {
            return setError("Empty path component is not allowed");
        }
        if (component == "." || component == "..") {
            return setError("Path traversal is not allowed");
        }
        for (const char ch : component) {
            if (invalidPathCharacter(ch)) {
                return setError("Path contains an invalid character");
            }
        }

        if (!outFilesystemPath.empty()) {
            outFilesystemPath.push_back('/');
        }
        outFilesystemPath += component;

        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1u;
    }

    return true;
}

std::string virtualPathFromFilesystemPath(const std::string& filesystemPath) {
    if (filesystemPath.empty()) {
        return "/";
    }
    return "/" + filesystemPath;
}

} // namespace Companion
