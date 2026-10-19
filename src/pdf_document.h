#pragma once
#include <windows.h>
#include <mupdf/fitz.h>
#include <string>
#include <utility>
#include <vector>
#include "geometry.h"

// Imagen BGRA (32 bits) renderizada por MuPDF. Debe destruirse antes que su PdfDocument.
class Pixmap {
public:
    Pixmap() = default;
    Pixmap(fz_context* ctx, fz_pixmap* pix) : m_ctx(ctx), m_pix(pix) {}
    Pixmap(Pixmap&& o) noexcept : m_ctx(o.m_ctx), m_pix(std::exchange(o.m_pix, nullptr)) {}
    Pixmap& operator=(Pixmap&& o) noexcept {
        if (this != &o) {
            reset();
            m_ctx = o.m_ctx;
            m_pix = std::exchange(o.m_pix, nullptr);
        }
        return *this;
    }
    Pixmap(const Pixmap&) = delete;
    Pixmap& operator=(const Pixmap&) = delete;
    ~Pixmap() { reset(); }

    void reset() {
        if (m_pix) fz_drop_pixmap(m_ctx, m_pix);
        m_pix = nullptr;
    }
    explicit operator bool() const { return m_pix != nullptr; }
    int stride() const { return fz_pixmap_stride(m_ctx, m_pix); }
    const unsigned char* samples() const { return fz_pixmap_samples(m_ctx, m_pix); }

private:
    fz_context* m_ctx = nullptr;
    fz_pixmap* m_pix = nullptr;
};

// Un documento PDF. Oculta MuPDF (y sus excepciones) al resto. Las paginas se cargan bajo demanda
// y se conservan las ultimas usadas.
class PdfDocument {
public:
    PdfDocument();
    ~PdfDocument();
    PdfDocument(const PdfDocument&) = delete;
    PdfDocument& operator=(const PdfDocument&) = delete;

    enum class OpenResult { Ok, Failed, NeedsPassword, BadPassword };
    // Mide todas las paginas. Si no devuelve Ok conserva el documento anterior. Con un PDF
    // protegido y `password` vacia devuelve NeedsPassword.
    OpenResult open(const std::wstring& path, const std::wstring& password = L"");
    bool isOpen() const { return m_doc != nullptr; }
    int pageCount() const { return m_pageCount; }
    const std::vector<PageSize>& pageSizes() const { return m_sizes; }

    // Entrada del indice (tabla de contenido). `page` es -1 si no se pudo resolver.
    struct TocItem {
        std::wstring title;
        int page;
        int level;
    };
    const std::vector<TocItem>& toc() const { return m_toc; }

    // Coincidencia de una busqueda: pagina y rectangulo dentro de ella.
    struct Hit {
        int page;
        RectF rect;
    };
    // Busca `needle` en todas las paginas, en orden de lectura.
    std::vector<Hit> search(const std::wstring& needle);

    // Destino de un enlace de la pagina. `y` es la posicion dentro de la pagina destino (puntos).
    struct LinkTarget {
        enum Kind { None, Page, External } kind = None;
        int page = 0;
        float y = 0;
        std::string uri;  // solo External
    };
    // Enlace que hay en el punto `pt` de la pagina `page` (kind == None si no hay ninguno).
    LinkTarget linkAt(int page, PointF pt);

    // Dibuja la region `region` de la pagina `page` (pixeles a `scale` px/punto, origen arriba-izquierda).
    Pixmap render(int page, float scale, const RECT& region);

    // Seleccion de texto en una pagina entre dos puntos. El texto se extrae la primera vez.
    std::vector<RectF> selectionRects(int page, PointF a, PointF b);
    std::wstring selectionText(int page, PointF a, PointF b);

private:
    struct Slot {
        int index;
        fz_page* page;
        fz_stext_page* text;
        fz_rect bounds;
        fz_link* links = nullptr;  // se cargan la primera vez que se consultan
        bool linksLoaded = false;
    };

    Slot* acquire(int index);  // valido hasta la siguiente llamada
    bool ensureText(Slot& slot);
    void dropSlot(Slot& slot);
    void dropSlots();
    void collectToc(fz_document* doc, fz_outline* node, int level, std::vector<TocItem>* out);
    static fz_point toNative(const Slot& s, PointF p) { return {p.x + s.bounds.x0, p.y + s.bounds.y0}; }

    fz_context* m_ctx = nullptr;
    fz_document* m_doc = nullptr;
    int m_pageCount = 0;
    std::vector<PageSize> m_sizes;
    std::vector<PointF> m_origins;  // esquina (x0, y0) de cada pagina en coordenadas nativas
    std::vector<TocItem> m_toc;
    std::vector<Slot> m_slots;     // la mas reciente primero
    std::vector<fz_quad> m_quads;  // buffer reutilizable para el resaltado
};
