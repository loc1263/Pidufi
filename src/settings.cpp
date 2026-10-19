#include "settings.h"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "util.h"

namespace {
constexpr size_t kMaxDocs = 200;
}

std::wstring Settings::keyFor(const std::wstring& path) {
    std::wstring k = path;
    CharLowerBuffW(k.data(), (DWORD)k.size());
    return k;
}

std::wstring Settings::filePath() {
    wchar_t dir[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, dir))) return {};
    std::wstring d = std::wstring(dir) + L"\\Pidufi";
    CreateDirectoryW(d.c_str(), nullptr);
    return d + L"\\settings.txt";
}

bool Settings::docPos(const std::wstring& path, DocPos* out) const {
    const std::wstring key = keyFor(path);
    for (const Entry& e : m_docs) {
        if (e.key == key) {
            *out = e.pos;
            return true;
        }
    }
    return false;
}

void Settings::setDocPos(const std::wstring& path, const DocPos& pos) {
    const std::wstring key = keyFor(path);
    m_docs.erase(std::remove_if(m_docs.begin(), m_docs.end(),
                                [&](const Entry& e) { return e.key == key; }),
                 m_docs.end());
    m_docs.insert(m_docs.begin(), Entry{key, pos});
    if (m_docs.size() > kMaxDocs) m_docs.resize(kMaxDocs);
}

// Formato, una linea por registro:
//   window <x> <y> <w> <h> <maximizada>
//   doc <pagina> <offX> <offY> <zoom> <ajustado><TAB><ruta>
void Settings::load() {
    const std::wstring path = filePath();
    if (path.empty()) return;
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return;
    std::string data;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) data.append(buf, n);
    fclose(f);

    size_t pos = 0;
    while (pos < data.size()) {
        size_t end = data.find('\n', pos);
        if (end == std::string::npos) end = data.size();
        std::string line = data.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.compare(0, 7, "window ") == 0) {
            int x, y, w, h, m;
            if (sscanf(line.c_str() + 7, "%d %d %d %d %d", &x, &y, &w, &h, &m) == 5 && w > 0 && h > 0)
                m_window = {x, y, w, h, m != 0, true};
        } else if (line.compare(0, 4, "doc ") == 0) {
            const size_t tab = line.find('\t');
            if (tab == std::string::npos) continue;
            DocPos p;
            int fit;
            if (sscanf(line.c_str() + 4, "%d %f %f %f %d", &p.page, &p.offX, &p.offY, &p.zoom, &fit) != 5)
                continue;
            p.fit = fit != 0;
            if (m_docs.size() < kMaxDocs) m_docs.push_back({FromUtf8(line.c_str() + tab + 1), p});
        }
    }
}

void Settings::save() const {
    const std::wstring path = filePath();
    if (path.empty()) return;
    std::string out;
    char line[128];
    if (m_window.valid) {
        snprintf(line, sizeof line, "window %d %d %d %d %d\n", m_window.x, m_window.y, m_window.w,
                 m_window.h, m_window.maximized ? 1 : 0);
        out += line;
    }
    for (const Entry& e : m_docs) {
        snprintf(line, sizeof line, "doc %d %.3f %.3f %.4f %d\t", e.pos.page, e.pos.offX,
                 e.pos.offY, e.pos.zoom, e.pos.fit ? 1 : 0);
        out += line;
        out += ToUtf8(e.key);
        out += '\n';
    }
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) return;
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
}
