#pragma once
#include <string>
#include <vector>

// Preferencias guardadas entre sesiones: tamano de la ventana y ultima posicion de cada PDF.
// Se escriben en %APPDATA%\Pidufi\settings.txt (texto plano).
class Settings {
public:
    struct Window {
        int x = 0, y = 0, w = 0, h = 0;  // rectangulo normal (no maximizado)
        bool maximized = false;
        bool valid = false;
    };
    // Donde se dejo un documento: pagina de la fila superior visible y desplazamiento dentro de
    // ella (en puntos PDF), mas el zoom.
    struct DocPos {
        int page = 0;
        float offX = 0, offY = 0;
        float zoom = 1.0f;
        bool fit = true;  // ajustado al ancho
    };

    void load();
    void save() const;

    const Window& window() const { return m_window; }
    void setWindow(const Window& w) { m_window = w; }

    bool docPos(const std::wstring& path, DocPos* out) const;
    void setDocPos(const std::wstring& path, const DocPos& pos);

private:
    struct Entry {
        std::wstring key;
        DocPos pos;
    };
    static std::wstring keyFor(const std::wstring& path);
    static std::wstring filePath();

    Window m_window;
    std::vector<Entry> m_docs;  // el mas reciente primero
};
