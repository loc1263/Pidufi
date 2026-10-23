#include "toolbar.h"
#include <string>

namespace {

constexpr int kBtnW = 36, kBtnH = 32, kGap = 4, kMargin = 8;
constexpr COLORREF kBg = RGB(45, 45, 48), kHover = RGB(75, 75, 80), kFg = RGB(235, 235, 235);

// Los iconos se dibujan centrados en (cx, cy) con el lapiz y el pincel nulo ya seleccionados.
using IconFn = void (*)(HDC dc, int cx, int cy);

void Line(HDC dc, int x1, int y1, int x2, int y2) {
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
}

void IconOpen(HDC dc, int cx, int cy) {
    const POINT pts[] = {{cx - 10, cy - 7}, {cx - 3, cy - 7}, {cx, cy - 4},
                         {cx + 10, cy - 4}, {cx + 10, cy + 8}, {cx - 10, cy + 8}};
    Polygon(dc, pts, 6);
}

void IconPrint(HDC dc, int cx, int cy) {  // impresora con la hoja saliendo
    Rectangle(dc, cx - 6, cy - 9, cx + 6, cy - 2);
    Rectangle(dc, cx - 10, cy - 2, cx + 10, cy + 5);
    Rectangle(dc, cx - 6, cy + 2, cx + 6, cy + 9);
}

void IconMagnifier(HDC dc, int cx, int cy, bool plus) {
    Ellipse(dc, cx - 10, cy - 10, cx + 4, cy + 4);
    Line(dc, cx + 4, cy + 4, cx + 10, cy + 10);
    Line(dc, cx - 7, cy - 3, cx + 1, cy - 3);
    if (plus) Line(dc, cx - 3, cy - 7, cx - 3, cy + 1);
}
void IconZoomOut(HDC dc, int cx, int cy) { IconMagnifier(dc, cx, cy, false); }
void IconZoomIn(HDC dc, int cx, int cy) { IconMagnifier(dc, cx, cy, true); }

void IconFitWidth(HDC dc, int cx, int cy) {  // |<->|
    Line(dc, cx - 11, cy - 8, cx - 11, cy + 9);
    Line(dc, cx + 11, cy - 8, cx + 11, cy + 9);
    Line(dc, cx - 9, cy, cx + 9, cy);
    Line(dc, cx - 9, cy, cx - 5, cy - 4);
    Line(dc, cx - 9, cy, cx - 5, cy + 4);
    Line(dc, cx + 9, cy, cx + 5, cy - 4);
    Line(dc, cx + 9, cy, cx + 5, cy + 4);
}

void IconSearch(HDC dc, int cx, int cy) {  // lupa sin signo
    Ellipse(dc, cx - 10, cy - 10, cx + 4, cy + 4);
    Line(dc, cx + 4, cy + 4, cx + 10, cy + 10);
}

void IconToc(HDC dc, int cx, int cy) {  // tres entradas con vineta
    for (int i = -1; i <= 1; i++) {
        const int y = cy + i * 6;
        Line(dc, cx - 10, y, cx - 8, y);
        Line(dc, cx - 4, y, cx + 10, y);
    }
}

void IconUp(HDC dc, int cx, int cy) {
    Line(dc, cx - 6, cy + 3, cx, cy - 3);
    Line(dc, cx, cy - 3, cx + 6, cy + 3);
}
void IconDown(HDC dc, int cx, int cy) {
    Line(dc, cx - 6, cy - 3, cx, cy + 3);
    Line(dc, cx, cy + 3, cx + 6, cy - 3);
}

// Zona entre el porcentaje de zoom y los botones de busqueda.
constexpr int kZoomTextW = 80, kEditW = 200, kEditH = 24;

struct Button {
    Command cmd;
    IconFn icon;
    int gapBefore;  // espacio extra a la izquierda (separador de grupo)
};

constexpr Button kButtons[Toolbar::kButtonCount] = {
    {Command::Open, IconOpen, 0},
    {Command::Print, IconPrint, 0},
    {Command::ZoomOut, IconZoomOut, 10},
    {Command::ZoomIn, IconZoomIn, 0},
    {Command::FitWidth, IconFitWidth, 0},
    {Command::Search, IconSearch, 10},
    {Command::ToggleToc, IconToc, 0},
    // Los dos siguientes solo se ven con la busqueda abierta; van tras el porcentaje y el cuadro.
    {Command::SearchPrev, IconUp, kZoomTextW + kEditW + 6 - kGap},
    {Command::SearchNext, IconDown, 0},
};

// Botones que dependen de la busqueda.
bool IsSearchOnly(Command c) { return c == Command::SearchPrev || c == Command::SearchNext; }

}  // namespace

Toolbar::Toolbar()
    : m_bgBrush(CreateSolidBrush(kBg)),
      m_hoverBrush(CreateSolidBrush(kHover)),
      m_activeBrush(CreateSolidBrush(RGB(0, 95, 150))),
      m_pen(CreatePen(PS_SOLID, 2, kFg)),
      m_dimPen(CreatePen(PS_SOLID, 2, RGB(110, 110, 112))) {
    int x = kMargin;
    const int y = (kHeight - kBtnH) / 2;
    for (int i = 0; i < kButtonCount; i++) {
        x += kButtons[i].gapBefore;
        m_rects[i] = {x, y, x + kBtnW, y + kBtnH};
        if (kButtons[i].cmd == Command::SearchPrev) {
            const int ex = x - 6 - kEditW, ey = (kHeight - kEditH) / 2;
            m_editRect = {ex, ey, ex + kEditW, ey + kEditH};
        }
        x += kBtnW + kGap;
    }
}

Toolbar::~Toolbar() {
    DeleteObject(m_bgBrush);
    DeleteObject(m_hoverBrush);
    DeleteObject(m_activeBrush);
    DeleteObject(m_pen);
    DeleteObject(m_dimPen);
}

int Toolbar::indexOf(Command c) const {
    for (int i = 0; i < kButtonCount; i++)
        if (kButtons[i].cmd == c) return i;
    return -1;
}

bool Toolbar::visible(int i) const { return m_searchOpen || !IsSearchOnly(kButtons[i].cmd); }

Command Toolbar::hitTest(int x, int y) const {
    const POINT p = {x, y};
    for (int i = 0; i < kButtonCount; i++)
        if (visible(i) && !m_disabled[i] && PtInRect(&m_rects[i], p)) return kButtons[i].cmd;
    return Command::None;
}

bool Toolbar::setHover(Command c) {
    if (c == m_hover) return false;
    m_hover = c;
    return true;
}

void Toolbar::setActive(Command c, bool on) {
    const int i = indexOf(c);
    if (i >= 0) m_active[i] = on;
}

void Toolbar::setEnabled(Command c, bool on) {
    const int i = indexOf(c);
    if (i >= 0) m_disabled[i] = !on;
}

void Toolbar::setSearchOpen(bool open) {
    m_searchOpen = open;
    setActive(Command::Search, open);
}

void Toolbar::draw(HDC dc, int width, int zoomPercent) const {
    const RECT bar = {0, 0, width, kHeight};
    FillRect(dc, &bar, m_bgBrush);

    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HGDIOBJ oldPen = SelectObject(dc, m_pen);
    for (int i = 0; i < kButtonCount; i++) {
        if (!visible(i)) continue;
        const RECT& r = m_rects[i];
        if (m_active[i]) FillRect(dc, &r, m_activeBrush);
        else if (!m_disabled[i] && kButtons[i].cmd == m_hover) FillRect(dc, &r, m_hoverBrush);
        SelectObject(dc, m_disabled[i] ? m_dimPen : m_pen);
        kButtons[i].icon(dc, (r.left + r.right) / 2, (r.top + r.bottom) / 2);
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(220, 220, 220));
    const int tocRight = m_rects[indexOf(Command::ToggleToc)].right;
    if (zoomPercent > 0) {
        const std::wstring text = std::to_wstring(zoomPercent) + L"%";
        RECT tr = {tocRight + 12, 0, tocRight + kZoomTextW, kHeight};
        DrawTextW(dc, text.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    if (m_searchOpen && !m_searchInfo.empty()) {
        const int nextRight = m_rects[indexOf(Command::SearchNext)].right;
        RECT tr = {nextRight + 10, 0, nextRight + 150, kHeight};
        DrawTextW(dc, m_searchInfo.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}
