#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "geometry.h"

// Todas las paginas apiladas en una columna, con zoom y desplazamiento. Solo geometria, sin Win32
// (salvo tipos basicos) ni MuPDF.
// "Contenido" = la columna completa en pixeles; "vista" = la ventana sobre ella.
// Pixeles a 96 dpi * zoom; los puntos son los del PDF (1/72").
class Viewport {
public:
    static constexpr float kMinZoom = 0.1f, kMaxZoom = 8.0f, kStep = 1.15f;
    static constexpr int kGap = 12;  // espacio entre paginas (y arriba / abajo), en pixeles

    float zoom() const { return m_zoom; }
    float scale() const { return m_zoom * 96.0f / 72.0f; }  // pixeles por punto
    int viewW() const { return m_viewW; }
    int viewH() const { return m_viewH; }
    int scrollX() const { return m_x; }
    int scrollY() const { return m_y; }
    int contentW() const { return m_contentW; }
    int contentH() const { return m_contentH; }
    int maxScrollX() const { return std::max(0, m_contentW - m_viewW); }
    int maxScrollY() const { return std::max(0, m_contentH - m_viewH); }

    int pageCount() const { return (int)m_pages.size(); }
    int pageW(int i) const { return m_pw[i]; }
    int pageH(int i) const { return m_ph[i]; }
    int pageTop(int i) const { return m_top[i]; }  // en coordenadas del contenido

    // Posicion de la pagina `i` dentro de la vista (paginas centradas en la columna).
    int pageX(int i) const {
        const int base = m_contentW < m_viewW ? (m_viewW - m_contentW) / 2 : -m_x;
        return base + (m_contentW - m_pw[i]) / 2;
    }
    int pageY(int i) const { return m_top[i] - m_y; }

    // Pagina que contiene la fila `contentY` del contenido (la mas cercana si cae en un hueco).
    int pageAtY(int contentY) const {
        if (m_top.empty()) return 0;
        const int i = (int)(std::upper_bound(m_top.begin(), m_top.end(), contentY) - m_top.begin()) - 1;
        return std::max(0, i);
    }
    int currentPage() const { return pageAtY(m_y + m_viewH / 2); }
    void visibleRange(int* first, int* last) const {
        *first = pageAtY(m_y);
        *last = pageAtY(m_y + std::max(0, m_viewH - 1));
    }

    void setView(int w, int h) {
        m_viewW = std::max(0, w);
        m_viewH = std::max(0, h);
        clamp();
    }

    void setPages(std::vector<PageSize> pages) {
        m_pages = std::move(pages);
        m_maxW = 0;
        for (const PageSize& p : m_pages) m_maxW = std::max(m_maxW, p.w);
        m_x = m_y = 0;
        recompute();
    }

    // Cambia el zoom manteniendo el centro de la vista. Devuelve false si no cambio nada.
    bool zoomTo(float z) { return zoomAt(z, m_viewH / 2); }
    bool zoomBy(float factor) { return zoomTo(m_zoom * factor); }

    // Ajusta el ancho de la pagina actual a la vista, conservando la fila superior visible.
    bool fitWidth() {
        if (m_pages.empty() || m_viewW <= 0) return false;
        const float w = m_pages[currentPage()].w;
        if (w <= 0) return false;
        // El 0.25 evita que el redondeo del ancho de pagina supere el de la vista (barra sobrante).
        const bool changed = zoomAt((m_viewW - 0.25f) / (w * 96.0f / 72.0f), 0);
        scrollTo(0, m_y);
        return changed;
    }

    void scrollTo(int x, int y) { m_x = x; m_y = y; clamp(); }
    void scrollBy(int dx, int dy) { scrollTo(m_x + dx, m_y + dy); }

private:
    // Zoom manteniendo fijo el punto de la pagina que esta en la fila `anchorY` de la vista.
    bool zoomAt(float z, int anchorY) {
        z = std::clamp(z, kMinZoom, kMaxZoom);
        if (z == m_zoom) return false;
        const float k = z / m_zoom;
        const float cx = (m_x + m_viewW / 2.0f) * k;
        int page = 0;
        float inPagePts = 0;
        if (!m_pages.empty()) {
            const int cy = m_y + anchorY;
            page = pageAtY(cy);
            inPagePts = (cy - m_top[page]) / scale();
        }
        m_zoom = z;
        recompute();
        m_x = (int)(cx - m_viewW / 2.0f);
        if (!m_pages.empty()) m_y = (int)(m_top[page] + inPagePts * scale()) - anchorY;
        clamp();
        return true;
    }

    void recompute() {
        const float s = scale();
        const size_t n = m_pages.size();
        m_pw.resize(n);
        m_ph.resize(n);
        m_top.resize(n);
        int y = kGap;
        for (size_t i = 0; i < n; i++) {
            m_pw[i] = (int)std::lround(m_pages[i].w * s);
            m_ph[i] = (int)std::lround(m_pages[i].h * s);
            m_top[i] = y;
            y += m_ph[i] + kGap;
        }
        m_contentW = (int)std::lround(m_maxW * s);
        m_contentH = n ? y : 0;
        clamp();
    }

    void clamp() {
        m_x = std::clamp(m_x, 0, maxScrollX());
        m_y = std::clamp(m_y, 0, maxScrollY());
    }

    float m_zoom = 1.0f;
    float m_maxW = 0;
    std::vector<PageSize> m_pages;  // en puntos
    std::vector<int> m_pw, m_ph, m_top;  // en pixeles
    int m_contentW = 0, m_contentH = 0;
    int m_viewW = 0, m_viewH = 0, m_x = 0, m_y = 0;
};
