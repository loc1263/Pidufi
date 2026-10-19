#include "pdf_document.h"
#include <algorithm>
#include <cmath>
#include "util.h"

// Importante: dentro de fz_try solo se usan variables simples; los objetos C++ (vector, string)
// se crean antes o despues, porque fz_try usa setjmp/longjmp.

namespace {
constexpr size_t kStoreBytes = 64u << 20;  // cache interna de MuPDF
constexpr size_t kMaxSlots = 16;           // paginas cargadas a la vez
constexpr int kMaxQuads = 4096;
constexpr float kFallbackW = 595, kFallbackH = 842;  // A4, si no se puede medir una pagina
}  // namespace

PdfDocument::PdfDocument() {
    m_ctx = fz_new_context(nullptr, nullptr, kStoreBytes);
    if (!m_ctx) return;
    fz_try(m_ctx) { fz_register_document_handlers(m_ctx); }
    fz_catch(m_ctx) {
        fz_drop_context(m_ctx);
        m_ctx = nullptr;
    }
    m_quads.resize(kMaxQuads);
}

PdfDocument::~PdfDocument() {
    dropSlots();
    if (m_doc) fz_drop_document(m_ctx, m_doc);
    if (m_ctx) fz_drop_context(m_ctx);
}

PdfDocument::OpenResult PdfDocument::open(const std::wstring& path, const std::wstring& password) {
    if (!m_ctx) return OpenResult::Failed;
    const std::string u8 = ToUtf8(path);
    const std::string pw = ToUtf8(password);
    const char* pwc = pw.c_str();
    fz_document* doc = nullptr;
    int pages = 0;
    int status = 0;  // 0 = bien, 1 = pide contrasena, 2 = contrasena incorrecta
    fz_var(doc);
    fz_var(pages);
    fz_var(status);
    fz_try(m_ctx) {
        doc = fz_open_document(m_ctx, u8.c_str());
        if (fz_needs_password(m_ctx, doc)) {
            if (!*pwc) status = 1;
            else if (!fz_authenticate_password(m_ctx, doc, pwc)) status = 2;
        }
        if (!status) pages = fz_count_pages(m_ctx, doc);
    }
    fz_catch(m_ctx) {
        fz_drop_document(m_ctx, doc);
        return OpenResult::Failed;
    }
    if (status) {
        fz_drop_document(m_ctx, doc);
        return status == 1 ? OpenResult::NeedsPassword : OpenResult::BadPassword;
    }
    if (pages <= 0) {
        fz_drop_document(m_ctx, doc);
        return OpenResult::Failed;
    }

    std::vector<PageSize> sizes;
    std::vector<PointF> origins;
    sizes.reserve(pages);
    origins.reserve(pages);
    for (int i = 0; i < pages; i++) {
        fz_rect b = {0, 0, kFallbackW, kFallbackH};
        fz_page* page = nullptr;
        fz_var(page);
        fz_var(b);
        fz_try(m_ctx) {
            page = fz_load_page(m_ctx, doc, i);
            b = fz_bound_page(m_ctx, page);
        }
        fz_catch(m_ctx) {}
        fz_drop_page(m_ctx, page);
        const float w = b.x1 - b.x0, h = b.y1 - b.y0;
        const bool ok = w > 0 && h > 0;
        sizes.push_back(ok ? PageSize{w, h} : PageSize{kFallbackW, kFallbackH});
        origins.push_back(ok ? PointF{b.x0, b.y0} : PointF{});
    }

    std::vector<TocItem> toc;
    fz_outline* outline = nullptr;
    fz_var(outline);
    fz_try(m_ctx) { outline = fz_load_outline(m_ctx, doc); }
    fz_catch(m_ctx) { outline = nullptr; }
    collectToc(doc, outline, 0, &toc);
    fz_drop_outline(m_ctx, outline);

    dropSlots();
    if (m_doc) fz_drop_document(m_ctx, m_doc);
    m_doc = doc;
    m_pageCount = pages;
    m_sizes = std::move(sizes);
    m_origins = std::move(origins);
    m_toc = std::move(toc);
    return OpenResult::Ok;
}

// Recorre el arbol del indice en orden (un nivel por profundidad).
void PdfDocument::collectToc(fz_document* doc, fz_outline* node, int level, std::vector<TocItem>* out) {
    for (; node; node = node->next) {
        int page = -1;
        const char* uri = node->uri;
        fz_location loc = node->page;
        fz_var(page);
        fz_try(m_ctx) {
            if (uri) loc = fz_resolve_link(m_ctx, doc, uri, nullptr, nullptr);
            page = fz_page_number_from_location(m_ctx, doc, loc);
        }
        fz_catch(m_ctx) { page = -1; }
        out->push_back({node->title ? FromUtf8(node->title) : std::wstring(), page, level});
        collectToc(doc, node->down, level + 1, out);
    }
}

std::vector<PdfDocument::Hit> PdfDocument::search(const std::wstring& needle) {
    std::vector<Hit> hits;
    if (!m_doc || needle.empty()) return hits;
    const std::string u8 = ToUtf8(needle);
    const char* text = u8.c_str();
    for (int p = 0; p < m_pageCount; p++) {
        int n = 0;
        fz_document* doc = m_doc;
        fz_try(m_ctx) { n = fz_search_page_number(m_ctx, doc, p, text, nullptr, m_quads.data(), kMaxQuads); }
        fz_catch(m_ctx) { n = 0; }
        const PointF o = m_origins[p];
        for (int i = 0; i < n; i++) {
            const fz_quad& q = m_quads[i];
            hits.push_back({p,
                            {std::min({q.ul.x, q.ur.x, q.ll.x, q.lr.x}) - o.x,
                             std::min({q.ul.y, q.ur.y, q.ll.y, q.lr.y}) - o.y,
                             std::max({q.ul.x, q.ur.x, q.ll.x, q.lr.x}) - o.x,
                             std::max({q.ul.y, q.ur.y, q.ll.y, q.lr.y}) - o.y}});
        }
    }
    return hits;
}

void PdfDocument::dropSlot(Slot& s) {
    if (s.links) fz_drop_link(m_ctx, s.links);
    if (s.text) fz_drop_stext_page(m_ctx, s.text);
    if (s.page) fz_drop_page(m_ctx, s.page);
}

void PdfDocument::dropSlots() {
    for (Slot& s : m_slots) dropSlot(s);
    m_slots.clear();
}

PdfDocument::Slot* PdfDocument::acquire(int index) {
    if (!m_doc || index < 0 || index >= m_pageCount) return nullptr;
    for (size_t i = 0; i < m_slots.size(); i++) {
        if (m_slots[i].index == index) {
            std::rotate(m_slots.begin(), m_slots.begin() + i, m_slots.begin() + i + 1);
            return &m_slots.front();
        }
    }
    fz_page* page = nullptr;
    fz_rect bounds = {};
    fz_var(page);
    fz_try(m_ctx) {
        page = fz_load_page(m_ctx, m_doc, index);
        bounds = fz_bound_page(m_ctx, page);
    }
    fz_catch(m_ctx) {
        fz_drop_page(m_ctx, page);
        return nullptr;
    }
    m_slots.insert(m_slots.begin(), Slot{index, page, nullptr, bounds});
    while (m_slots.size() > kMaxSlots) {
        dropSlot(m_slots.back());
        m_slots.pop_back();
    }
    return &m_slots.front();
}

PdfDocument::LinkTarget PdfDocument::linkAt(int index, PointF pt) {
    LinkTarget out;
    Slot* slot = acquire(index);
    if (!slot) return out;
    if (!slot->linksLoaded) {
        fz_page* page = slot->page;
        fz_link* links = nullptr;
        fz_try(m_ctx) { links = fz_load_links(m_ctx, page); }
        fz_catch(m_ctx) { links = nullptr; }
        slot->links = links;
        slot->linksLoaded = true;
    }
    const fz_point p = toNative(*slot, pt);
    for (fz_link* l = slot->links; l; l = l->next) {
        if (!l->uri || !fz_is_point_inside_rect(p, l->rect)) continue;
        const char* uri = l->uri;
        if (fz_is_external_link(m_ctx, uri)) {
            out.kind = LinkTarget::External;
            out.uri = uri;
            return out;
        }
        fz_document* doc = m_doc;
        float x = 0, y = 0;
        int dest = -1;
        fz_var(dest);
        fz_try(m_ctx) {
            const fz_location loc = fz_resolve_link(m_ctx, doc, uri, &x, &y);
            dest = fz_page_number_from_location(m_ctx, doc, loc);
        }
        fz_catch(m_ctx) { dest = -1; }
        if (dest < 0 || dest >= m_pageCount) return out;
        out.kind = LinkTarget::Page;
        out.page = dest;
        // Sin coordenada valida (o NaN) se va al principio de la pagina.
        out.y = std::isfinite(y) ? std::clamp(y - m_origins[dest].y, 0.0f, m_sizes[dest].h) : 0.0f;
        return out;
    }
    return out;
}

Pixmap PdfDocument::render(int index, float scale, const RECT& region) {
    Slot* slot = acquire(index);
    if (!slot) return {};
    const fz_matrix ctm =
        fz_concat(fz_translate(-slot->bounds.x0, -slot->bounds.y0), fz_scale(scale, scale));
    const fz_irect box = {region.left, region.top, region.right, region.bottom};
    fz_page* page = slot->page;
    fz_pixmap* pix = nullptr;
    fz_device* dev = nullptr;
    fz_var(pix);
    fz_var(dev);
    fz_try(m_ctx) {
        pix = fz_new_pixmap_with_bbox(m_ctx, fz_device_bgr(m_ctx), box, nullptr, 1);
        fz_clear_pixmap_with_value(m_ctx, pix, 255);
        dev = fz_new_draw_device(m_ctx, fz_identity, pix);
        fz_run_page(m_ctx, page, dev, ctm, nullptr);
        fz_close_device(m_ctx, dev);
    }
    fz_always(m_ctx) { fz_drop_device(m_ctx, dev); }
    fz_catch(m_ctx) {
        fz_drop_pixmap(m_ctx, pix);
        pix = nullptr;
    }
    return Pixmap(m_ctx, pix);
}

bool PdfDocument::ensureText(Slot& slot) {
    if (slot.text) return true;
    fz_page* page = slot.page;
    fz_stext_page* text = nullptr;
    fz_try(m_ctx) { text = fz_new_stext_page_from_page(m_ctx, page, nullptr); }
    fz_catch(m_ctx) { text = nullptr; }
    slot.text = text;
    return text != nullptr;
}

std::vector<RectF> PdfDocument::selectionRects(int index, PointF a, PointF b) {
    std::vector<RectF> out;
    Slot* slot = acquire(index);
    if (!slot || !ensureText(*slot)) return out;
    const fz_point pa = toNative(*slot, a), pb = toNative(*slot, b);
    fz_stext_page* text = slot->text;
    const fz_rect bounds = slot->bounds;
    int n = 0;
    fz_try(m_ctx) { n = fz_highlight_selection(m_ctx, text, pa, pb, m_quads.data(), kMaxQuads); }
    fz_catch(m_ctx) { n = 0; }
    out.reserve(n);
    for (int i = 0; i < n; i++) {
        const fz_quad& q = m_quads[i];
        out.push_back({std::min({q.ul.x, q.ur.x, q.ll.x, q.lr.x}) - bounds.x0,
                       std::min({q.ul.y, q.ur.y, q.ll.y, q.lr.y}) - bounds.y0,
                       std::max({q.ul.x, q.ur.x, q.ll.x, q.lr.x}) - bounds.x0,
                       std::max({q.ul.y, q.ur.y, q.ll.y, q.lr.y}) - bounds.y0});
    }
    return out;
}

std::wstring PdfDocument::selectionText(int index, PointF a, PointF b) {
    Slot* slot = acquire(index);
    if (!slot || !ensureText(*slot)) return {};
    const fz_point pa = toNative(*slot, a), pb = toNative(*slot, b);
    fz_stext_page* page = slot->text;
    char* text = nullptr;
    fz_try(m_ctx) { text = fz_copy_selection(m_ctx, page, pa, pb, 1); }
    fz_catch(m_ctx) { text = nullptr; }
    if (!text) return {};
    std::wstring w = FromUtf8(text);
    fz_free(m_ctx, text);
    return w;
}
