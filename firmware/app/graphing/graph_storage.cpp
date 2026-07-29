#include "app/graphing/graph_storage.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

constexpr GraphWindow DEFAULT_GRAPH_WINDOW = {
    -10.0,
    10.0,
    -10.0,
    10.0,
    1.0,
    1.0,
};

std::string jsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        if (ch == '"' || ch == '\\') {
            escaped.push_back('\\');
            escaped.push_back(ch);
        } else if (static_cast<unsigned char>(ch) < 0x20u) {
            escaped.push_back(' ');
        } else {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

void skipWhitespace(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        pos++;
    }
}

bool parseStringAt(const std::string& text, std::size_t& pos, std::string& out) {
    skipWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != '"') {
        return false;
    }
    pos++;
    out.clear();
    while (pos < text.size()) {
        const char ch = text[pos++];
        if (ch == '"') {
            return true;
        }
        if (ch == '\\') {
            if (pos >= text.size()) {
                return false;
            }
            out.push_back(text[pos++]);
        } else {
            out.push_back(ch);
        }
    }
    return false;
}

bool findKeyValueStart(const std::string& text,
                       const char* key,
                       std::size_t start,
                       std::size_t end,
                       std::size_t& valueStart) {
    const std::string marker = std::string("\"") + key + "\"";
    const std::size_t keyPos = text.find(marker, start);
    if (keyPos == std::string::npos || keyPos >= end) {
        return false;
    }
    std::size_t colon = text.find(':', keyPos + marker.size());
    if (colon == std::string::npos || colon >= end) {
        return false;
    }
    valueStart = colon + 1;
    skipWhitespace(text, valueStart);
    return valueStart < end;
}

bool parseStringField(const std::string& text,
                      const char* key,
                      std::size_t start,
                      std::size_t end,
                      std::string& out) {
    std::size_t pos = 0;
    if (!findKeyValueStart(text, key, start, end, pos)) {
        return false;
    }
    return parseStringAt(text, pos, out) && pos <= end + 1;
}

bool parseBoolField(const std::string& text,
                    const char* key,
                    std::size_t start,
                    std::size_t end,
                    bool& out) {
    std::size_t pos = 0;
    if (!findKeyValueStart(text, key, start, end, pos)) {
        return false;
    }
    if (text.compare(pos, 4, "true") == 0) {
        out = true;
        return true;
    }
    if (text.compare(pos, 5, "false") == 0) {
        out = false;
        return true;
    }
    return false;
}

bool parseDoubleField(const std::string& text,
                      const char* key,
                      std::size_t start,
                      std::size_t end,
                      double& out) {
    std::size_t pos = 0;
    if (!findKeyValueStart(text, key, start, end, pos)) {
        return false;
    }

    char* parseEnd = nullptr;
    const double value = std::strtod(text.c_str() + pos, &parseEnd);
    if (parseEnd == text.c_str() + pos) {
        return false;
    }
    const std::size_t parsedEnd = static_cast<std::size_t>(parseEnd - text.c_str());
    if (parsedEnd > end + 1) {
        return false;
    }
    out = value;
    return true;
}

bool parseWindow(const std::string& text, GraphWindow& window) {
    std::size_t windowStart = 0;
    if (!findKeyValueStart(text, "window", 0, text.size(), windowStart) || text[windowStart] != '{') {
        return false;
    }
    const std::size_t windowEnd = text.find('}', windowStart + 1);
    if (windowEnd == std::string::npos) {
        return false;
    }

    return parseDoubleField(text, "xMin", windowStart, windowEnd, window.xMin) &&
           parseDoubleField(text, "xMax", windowStart, windowEnd, window.xMax) &&
           parseDoubleField(text, "yMin", windowStart, windowEnd, window.yMin) &&
           parseDoubleField(text, "yMax", windowStart, windowEnd, window.yMax) &&
           std::isfinite(window.xMin) && std::isfinite(window.xMax) &&
           std::isfinite(window.yMin) && std::isfinite(window.yMax) &&
           window.xMin < window.xMax && window.yMin < window.yMax;
}

bool parseFunctions(const std::string& text, std::vector<GraphSessionFunction>& functions) {
    std::size_t arrayStart = 0;
    if (!findKeyValueStart(text, "functions", 0, text.size(), arrayStart) || text[arrayStart] != '[') {
        return false;
    }
    const std::size_t arrayEnd = text.find(']', arrayStart + 1);
    if (arrayEnd == std::string::npos) {
        return false;
    }

    std::size_t pos = arrayStart + 1;
    while (pos < arrayEnd) {
        const std::size_t objectStart = text.find('{', pos);
        if (objectStart == std::string::npos || objectStart >= arrayEnd) {
            break;
        }
        const std::size_t objectEnd = text.find('}', objectStart + 1);
        if (objectEnd == std::string::npos || objectEnd > arrayEnd) {
            return false;
        }

        GraphSessionFunction function;
        if (!parseStringField(text, "expression", objectStart, objectEnd, function.expression) ||
            !parseBoolField(text, "enabled", objectStart, objectEnd, function.enabled) ||
            function.expression.size() > 256 || functions.size() >= 16) {
            return false;
        }
        functions.push_back(function);
        pos = objectEnd + 1;
    }
    return true;
}

std::string withoutExtension(const std::string& fileName) {
    const std::string extension = GraphSessionStorage::kExtension;
    if (fileName.size() >= extension.size() &&
        fileName.compare(fileName.size() - extension.size(), extension.size(), extension) == 0) {
        return fileName.substr(0, fileName.size() - extension.size());
    }
    return fileName;
}

bool graphFileExists(AxiomFS::FileSystem& fs, const std::string& fileName) {
    bool exists = false;
    return fs.exists(GraphSessionStorage::graphPath(fileName), exists) == AxiomFS::Status::Ok && exists;
}

std::string uniqueFileName(AxiomFS::FileSystem& fs, const std::string& preferredName) {
    const std::string sanitized = GraphSessionStorage::sanitizeFileName(preferredName);
    if (!graphFileExists(fs, sanitized)) {
        return sanitized;
    }

    const std::string base = withoutExtension(sanitized);
    char candidate[96] = {};
    for (int i = 1; i < 1000; ++i) {
        std::snprintf(candidate, sizeof(candidate), "%s_%03d%s",
                      base.c_str(),
                      i,
                      GraphSessionStorage::kExtension);
        if (!graphFileExists(fs, candidate)) {
            return candidate;
        }
    }
    return sanitized;
}

} // namespace

std::string GraphSessionStorage::sanitizeFileName(const std::string& name) {
    std::string cleaned;
    cleaned.reserve(name.size());
    for (char ch : name) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.') {
            cleaned.push_back(ch);
        } else if (std::isspace(uch)) {
            cleaned.push_back('_');
        }
    }

    if (cleaned.empty()) {
        cleaned = "Graph";
    }
    while (!cleaned.empty() && cleaned[0] == '.') {
        cleaned.erase(0, 1);
    }
    if (cleaned.empty()) {
        cleaned = "Graph";
    }

    const std::string extension = kExtension;
    if (cleaned.size() < extension.size() ||
        cleaned.compare(cleaned.size() - extension.size(), extension.size(), extension) != 0) {
        cleaned += extension;
    }
    return cleaned;
}

std::string GraphSessionStorage::graphPath(const std::string& fileName) {
    return "graphs/" + sanitizeFileName(fileName);
}

std::string GraphSessionStorage::defaultFileName(AxiomFS::FileSystem& fs) {
    char candidate[32] = {};
    for (int i = 1; i < 1000; ++i) {
        std::snprintf(candidate, sizeof(candidate), "Graph_%03d%s", i, kExtension);
        if (!graphFileExists(fs, candidate)) {
            return candidate;
        }
    }
    return std::string("Graph_999") + kExtension;
}

bool GraphSessionStorage::save(AxiomFS::FileSystem& fs,
                               const std::string& preferredName,
                               const GraphSessionData& session,
                               std::string* savedFileName) {
    if (fs.createDir("graphs") != AxiomFS::Status::Ok) {
        return false;
    }

    const std::string fileName = uniqueFileName(fs, preferredName.empty()
        ? defaultFileName(fs)
        : preferredName);

    std::string text;
    text += "{\n";
    text += "  \"version\": 1,\n";
    text += "  \"name\": \"" + jsonEscape(session.name.empty() ? withoutExtension(fileName) : session.name) + "\",\n";
    text += "  \"functions\": [\n";
    for (std::size_t i = 0; i < session.functions.size(); ++i) {
        text += "    { \"expression\": \"" + jsonEscape(session.functions[i].expression) + "\", ";
        text += "\"enabled\": ";
        text += session.functions[i].enabled ? "true" : "false";
        text += " }";
        if (i + 1 < session.functions.size()) {
            text += ",";
        }
        text += "\n";
    }
    text += "  ],\n";
    text += "  \"window\": {\n";

    char number[64] = {};
    std::snprintf(number, sizeof(number), "    \"xMin\": %.17g,\n", session.window.xMin);
    text += number;
    std::snprintf(number, sizeof(number), "    \"xMax\": %.17g,\n", session.window.xMax);
    text += number;
    std::snprintf(number, sizeof(number), "    \"yMin\": %.17g,\n", session.window.yMin);
    text += number;
    std::snprintf(number, sizeof(number), "    \"yMax\": %.17g\n", session.window.yMax);
    text += number;
    text += "  },\n";
    text += "  \"mode\": { \"angle\": \"";
    text += session.angleRadians ? "radian" : "degree";
    text += "\" }\n";
    text += "}\n";

    const AxiomFS::Status status = fs.writeFile(graphPath(fileName), text);
    if (status == AxiomFS::Status::Ok && savedFileName) {
        *savedFileName = fileName;
    }
    return status == AxiomFS::Status::Ok;
}

bool GraphSessionStorage::load(AxiomFS::FileSystem& fs,
                               const std::string& fileName,
                               GraphSessionData& session) {
    const AxiomFS::ReadResult read = fs.readFile(graphPath(fileName));
    if (!read.ok()) {
        return false;
    }

    const std::string text(read.data.begin(), read.data.end());
    double version = 0;
    if (text.find('{') == std::string::npos ||
        !parseDoubleField(text, "version", 0, text.size(), version) ||
        version != 1.0) {
        return false;
    }

    GraphSessionData parsed;
    parsed.window = DEFAULT_GRAPH_WINDOW;
    parsed.angleRadians = true;
    if (!parseStringField(text, "name", 0, text.size(), parsed.name) ||
        parsed.name.size() > 96) {
        return false;
    }
    if (!parseFunctions(text, parsed.functions)) {
        return false;
    }
    if (!parseWindow(text, parsed.window)) {
        return false;
    }

    std::string angle;
    if (!parseStringField(text, "angle", 0, text.size(), angle) ||
        (angle != "radian" && angle != "degree")) return false;
    parsed.angleRadians = angle == "radian";

    session = parsed;
    return true;
}

bool GraphSessionStorage::deleteFile(AxiomFS::FileSystem& fs, const std::string& fileName) {
    return fs.deleteFile(graphPath(fileName)) == AxiomFS::Status::Ok;
}

AxiomFS::ListResult GraphSessionStorage::list(AxiomFS::FileSystem& fs) {
    if (fs.createDir("graphs") != AxiomFS::Status::Ok) {
        AxiomFS::ListResult result;
        result.status = AxiomFS::Status::IoError;
        return result;
    }

    AxiomFS::ListResult listing = fs.listDir("graphs");
    if (!listing.ok()) {
        return listing;
    }

    std::vector<AxiomFS::DirectoryEntry> graphFiles;
    const std::string extension = kExtension;
    for (const AxiomFS::DirectoryEntry& entry : listing.entries) {
        if (!entry.isDirectory &&
            entry.name.size() >= extension.size() &&
            entry.name.compare(entry.name.size() - extension.size(), extension.size(), extension) == 0) {
            graphFiles.push_back(entry);
        }
    }
    listing.entries = graphFiles;
    return listing;
}
