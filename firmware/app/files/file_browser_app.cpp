#include "app/files/file_browser_app.h"

#include "graphics/font.h"

#include <algorithm>
#include <cstdio>

namespace {

const Color COLOR_BG = Display::rgb(8, 10, 14);
const Color COLOR_PANEL = Display::rgb(18, 24, 32);
const Color COLOR_PANEL_SELECTED = Display::rgb(31, 42, 54);
const Color COLOR_TEXT = Display::WHITE;
const Color COLOR_MUTED = Display::rgb(150, 160, 172);
const Color COLOR_FOCUS = Display::rgb(255, 230, 95);
const Color COLOR_WARN = Display::rgb(255, 180, 80);

void drawTextFit(Display& display,
                 const char* text,
                 int x,
                 int y,
                 int maxWidth,
                 Color color) {
    if (!text || maxWidth <= 0) {
        return;
    }

    char buffer[64] = {};
    const int maxChars = std::min(maxWidth / FONT_CHAR_ADVANCE,
                                  static_cast<int>(sizeof(buffer)) - 1);
    int i = 0;
    for (; i < maxChars && text[i] != '\0'; ++i) {
        buffer[i] = text[i];
    }
    buffer[i] = '\0';
    display.drawText(buffer, x, y, color);
}

} // namespace

FileBrowserApp::FileBrowserApp(Display& display, AxiomFS::FileSystem* filesystem)
    : m_display(display)
    , m_filesystem(filesystem)
    , m_screen(Screen::List)
    , m_path()
    , m_listing()
    , m_selectedIndex(0)
    , m_needsRefresh(true)
    , m_needsRender(true)
    , m_bounds{0, 22, DISPLAY_WIDTH, DISPLAY_HEIGHT - 22} {}

void FileBrowserApp::enter() {
    m_screen = Screen::List;
    m_path.clear();
    m_selectedIndex = 0;
    m_needsRefresh = true;
    requestRender();
}

void FileBrowserApp::handleKey(Key key) {
    if (key == Key::NONE) {
        return;
    }

    if (m_needsRefresh) {
        refresh();
    }

    if (m_screen == Screen::DeleteConfirm) {
        if (key == Key::ENTER) {
            confirmDelete();
            m_screen = Screen::List;
        } else if (key == Key::CLEAR) {
            m_screen = Screen::List;
        }
        requestRender();
        return;
    }

    if (m_screen == Screen::Details) {
        if (key == Key::CLEAR || key == Key::ENTER) {
            m_screen = Screen::List;
        } else if (key == Key::DELETE_KEY) {
            requestDelete();
        }
        requestRender();
        return;
    }

    if (key == Key::CLEAR) {
        goUp();
    } else if (key == Key::CURSOR_UP) {
        if (!m_listing.entries.empty()) {
            m_selectedIndex = (m_selectedIndex + static_cast<int>(m_listing.entries.size()) - 1)
                % static_cast<int>(m_listing.entries.size());
        }
    } else if (key == Key::CURSOR_DOWN) {
        if (!m_listing.entries.empty()) {
            m_selectedIndex = (m_selectedIndex + 1) % static_cast<int>(m_listing.entries.size());
        }
    } else if (key == Key::ENTER) {
        openSelected();
    } else if (key == Key::DELETE_KEY) {
        requestDelete();
    }

    requestRender();
}

void FileBrowserApp::renderContent(int x, int y, int w, int h) {
    m_bounds = {x, y, w, h};
    if (m_needsRefresh) {
        refresh();
    }
    if (!m_needsRender) {
        return;
    }

    if (m_screen == Screen::Details) {
        renderDetails(x, y, w, h);
    } else if (m_screen == Screen::DeleteConfirm) {
        renderDeleteConfirm(x, y, w, h);
    } else {
        renderList(x, y, w, h);
    }
    m_needsRender = false;
}

bool FileBrowserApp::needsRender() const {
    return m_needsRender;
}

void FileBrowserApp::requestRender() {
    m_needsRender = true;
}

const std::string& FileBrowserApp::currentPathForTest() const {
    return m_path;
}

bool FileBrowserApp::canDeletePathForTest(const std::string& path) const {
    std::string normalized;
    if (AxiomFS::normalizePath(path, normalized) != AxiomFS::Status::Ok) {
        return false;
    }
    return !normalized.empty() && !AxiomFS::isProtectedSystemPath(normalized);
}

void FileBrowserApp::refresh() {
    if (!m_filesystem) {
        m_listing.status = AxiomFS::Status::NotMounted;
        m_listing.entries.clear();
    } else {
        m_listing = m_filesystem->listDir(m_path.empty() ? "/" : m_path);
    }

    if (m_selectedIndex >= static_cast<int>(m_listing.entries.size())) {
        m_selectedIndex = std::max(0, static_cast<int>(m_listing.entries.size()) - 1);
    }
    m_needsRefresh = false;
}

void FileBrowserApp::openSelected() {
    const AxiomFS::DirectoryEntry* entry = selectedEntry();
    if (!entry) {
        return;
    }

    if (entry->isDirectory) {
        m_path = childPath(entry->name);
        m_selectedIndex = 0;
        m_needsRefresh = true;
    } else {
        m_screen = Screen::Details;
    }
}

void FileBrowserApp::goUp() {
    if (m_screen != Screen::List) {
        m_screen = Screen::List;
        return;
    }
    if (m_path.empty()) {
        return;
    }
    m_path = parentPath(m_path);
    m_selectedIndex = 0;
    m_needsRefresh = true;
}

void FileBrowserApp::requestDelete() {
    const std::string path = selectedPath();
    if (path.empty() || !canDeletePathForTest(path)) {
        return;
    }
    m_screen = Screen::DeleteConfirm;
}

void FileBrowserApp::confirmDelete() {
    const std::string path = selectedPath();
    if (!m_filesystem || path.empty() || !canDeletePathForTest(path)) {
        return;
    }

    const AxiomFS::DirectoryEntry* entry = selectedEntry();
    if (!entry || entry->isDirectory) {
        return;
    }

    (void)m_filesystem->deleteFile(path);
    m_needsRefresh = true;
}

void FileBrowserApp::renderList(int x, int y, int w, int h) {
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Files", x + 8, y + 8, COLOR_TEXT);

    char pathLabel[80] = {};
    std::snprintf(pathLabel, sizeof(pathLabel), "/%s", m_path.c_str());
    drawTextFit(m_display, pathLabel, x + 64, y + 8, w - 72, COLOR_MUTED);

    if (!m_listing.ok()) {
        char status[64] = {};
        std::snprintf(status, sizeof(status), "Storage: %s", AxiomFS::statusToString(m_listing.status));
        m_display.drawText(status, x + 12, y + 42, COLOR_WARN);
        return;
    }

    if (m_listing.entries.empty()) {
        m_display.drawText("Empty folder", x + 12, y + 42, COLOR_MUTED);
    }

    const int rowHeight = 18;
    const int listY = y + 30;
    const int visibleRows = std::max(1, (h - 54) / rowHeight);
    int first = 0;
    if (m_selectedIndex >= visibleRows) {
        first = m_selectedIndex - visibleRows + 1;
    }

    for (int row = 0; row < visibleRows && first + row < static_cast<int>(m_listing.entries.size()); ++row) {
        const int index = first + row;
        const AxiomFS::DirectoryEntry& entry = m_listing.entries[index];
        const int rowY = listY + row * rowHeight;
        const bool selected = index == m_selectedIndex;
        m_display.fillRect(x + 6, rowY - 3, w - 12, rowHeight,
                           selected ? COLOR_PANEL_SELECTED : COLOR_PANEL);
        if (selected) {
            m_display.fillRect(x + 6, rowY - 3, 3, rowHeight, COLOR_FOCUS);
        }

        char label[80] = {};
        std::snprintf(label, sizeof(label), "%s%s",
                      entry.isDirectory ? "[DIR] " : "      ",
                      entry.name.c_str());
        drawTextFit(m_display, label, x + 14, rowY, w - 80,
                    selected ? COLOR_TEXT : COLOR_MUTED);
        if (!entry.isDirectory) {
            char size[24] = {};
            std::snprintf(size, sizeof(size), "%llu", static_cast<unsigned long long>(entry.size));
            m_display.drawText(size,
                               x + w - Display::textWidth(size) - 12,
                               rowY,
                               selected ? COLOR_TEXT : COLOR_MUTED);
        }
    }

    m_display.drawText("ENT open  CLR up  DEL delete", x + 8, y + h - 14, COLOR_MUTED);
}

void FileBrowserApp::renderDetails(int x, int y, int w, int h) {
    (void)w;
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("File Details", x + 8, y + 8, COLOR_TEXT);
    const AxiomFS::DirectoryEntry* entry = selectedEntry();
    if (!entry) {
        m_display.drawText("No file selected", x + 12, y + 38, COLOR_MUTED);
        return;
    }

    char line[96] = {};
    std::snprintf(line, sizeof(line), "Name: %s", entry->name.c_str());
    drawTextFit(m_display, line, x + 12, y + 38, w - 24, COLOR_TEXT);
    std::snprintf(line, sizeof(line), "Type: %s", entry->isDirectory ? "Folder" : "File");
    m_display.drawText(line, x + 12, y + 54, COLOR_MUTED);
    std::snprintf(line, sizeof(line), "Size: %llu B", static_cast<unsigned long long>(entry->size));
    m_display.drawText(line, x + 12, y + 70, COLOR_MUTED);
    std::snprintf(line, sizeof(line), "Path: /%s", selectedPath().c_str());
    drawTextFit(m_display, line, x + 12, y + 86, w - 24, COLOR_MUTED);
    if (entry->name.size() >= 10 &&
        entry->name.compare(entry->name.size() - 10, 10, ".mi23graph") == 0) {
        m_display.drawText("Graph session file", x + 12, y + 102, COLOR_TEXT);
        m_display.drawText("Open from Graphing app", x + 12, y + 116, COLOR_MUTED);
    }
    m_display.drawText("DEL delete  ENT/CLR back", x + 8, y + h - 14, COLOR_MUTED);
}

void FileBrowserApp::renderDeleteConfirm(int x, int y, int w, int h) {
    (void)w;
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Delete File", x + 8, y + 8, COLOR_TEXT);
    m_display.drawText("Delete selected file?", x + 20, y + 52, COLOR_WARN);
    m_display.drawText("ENT = Confirm", x + 20, y + 78, COLOR_TEXT);
    m_display.drawText("CLR = Cancel", x + 20, y + 92, COLOR_MUTED);
}

const AxiomFS::DirectoryEntry* FileBrowserApp::selectedEntry() const {
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_listing.entries.size())) {
        return nullptr;
    }
    return &m_listing.entries[m_selectedIndex];
}

std::string FileBrowserApp::selectedPath() const {
    const AxiomFS::DirectoryEntry* entry = selectedEntry();
    return entry ? childPath(entry->name) : std::string();
}

std::string FileBrowserApp::childPath(const std::string& name) const {
    return m_path.empty() ? name : m_path + "/" + name;
}

std::string FileBrowserApp::parentPath(const std::string& path) {
    const std::size_t slash = path.rfind('/');
    if (slash == std::string::npos) {
        return "";
    }
    return path.substr(0, slash);
}
