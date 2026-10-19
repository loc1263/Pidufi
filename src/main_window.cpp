#include "main_window.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include "../res/resource.h"
#include "util.h"

namespace {

const wchar_t* const kClassName = L"PidufiWnd";
constexpr COLORREF kBgColor = RGB(70, 70, 74);
constexpr int kLineStep = 60;     // pixeles por pulsacion de flecha
constexpr float kFar = 1e6f;      // "hasta el principio / el final" de una pagina al seleccionar
constexpr COLORREF kHitColor = RGB(255, 240, 0), kCurHitColor = RGB(255, 140, 0);

// ctrl: 1 = requerida, 0 = no debe estar pulsada, -1 = indiferente.
struct KeyBinding {
    UINT vk;
    int ctrl;
    Command cmd;
};

const KeyBinding kKeys[] = {
    {'O', 1, Command::Open},
    {'F', 1, Command::Search},
    {'T', 1, Command::ToggleToc},
    {'G', 1, Command::GoToPage},
    {'C', 1, Command::Copy},
    {'A', 1, Command::SelectAll},
    {'0', 1, Command::ZoomReset},
    {VK_NUMPAD0, 1, Command::ZoomReset},
    {'9', 1, Command::FitWidth},
    {VK_NUMPAD9, 1, Command::FitWidth},
    {VK_ADD, -1, Command::ZoomIn},
    {VK_OEM_PLUS, -1, Command::ZoomIn},
    {VK_SUBTRACT, -1, Command::ZoomOut},
    {VK_OEM_MINUS, -1, Command::ZoomOut},
    {VK_PRIOR, -1, Command::PrevPage},
    {VK_NEXT, -1, Command::NextPage},
    {VK_HOME, -1, Command::FirstPage},
    {VK_END, -1, Command::LastPage},
    {VK_UP, -1, Command::ScrollUp},
    {VK_DOWN, -1, Command::ScrollDown},
    {VK_LEFT, -1, Command::ScrollLeft},
    {VK_RIGHT, -1, Command::ScrollRight},
    {VK_SPACE, -1, Command::ScrollPageDown},
};

bool IsDown(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }

bool Contains(const RECT& outer, const RECT& inner) {
    return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right &&
           inner.bottom <= outer.bottom;
}

struct GotoData {
    int page, count;
};

INT_PTR CALLBACK GotoProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG: {
            SetWindowLongPtrW(dlg, DWLP_USER, lp);
            const GotoData* d = (const GotoData*)lp;
            wchar_t label[64];
            swprintf(label, 64, L"Pagina (1 - %d):", d->count);
            SetDlgItemTextW(dlg, IDC_LABEL, label);
            SetDlgItemInt(dlg, IDC_EDIT, d->page, FALSE);
            SendDlgItemMessageW(dlg, IDC_EDIT, EM_SETSEL, 0, -1);
            SetFocus(GetDlgItem(dlg, IDC_EDIT));
            return FALSE;  // el foco ya se puso a mano
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                GotoData* d = (GotoData*)GetWindowLongPtrW(dlg, DWLP_USER);
                BOOL ok = FALSE;
                const UINT v = GetDlgItemInt(dlg, IDC_EDIT, &ok, FALSE);
                if (!ok || v < 1 || v > (UINT)d->count) {
                    MessageBeep(MB_ICONWARNING);
                    SetFocus(GetDlgItem(dlg, IDC_EDIT));
                    SendDlgItemMessageW(dlg, IDC_EDIT, EM_SETSEL, 0, -1);
                    return TRUE;
                }
                d->page = (int)v;
                EndDialog(dlg, IDOK);
                return TRUE;
            }
            if (LOWORD(wp) == IDCANCEL) {
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK PasswordProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG:
            SetWindowLongPtrW(dlg, DWLP_USER, lp);
            SetFocus(GetDlgItem(dlg, IDC_EDIT));
            return FALSE;
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                std::wstring* out = (std::wstring*)GetWindowLongPtrW(dlg, DWLP_USER);
                wchar_t buf[256];
                GetDlgItemTextW(dlg, IDC_EDIT, buf, 256);
                *out = buf;
                SecureZeroMemory(buf, sizeof buf);
                EndDialog(dlg, IDOK);
                return TRUE;
            }
            if (LOWORD(wp) == IDCANCEL) {
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

bool IsSafeExternalUri(const std::string& uri) {
    // Solo web y correo: un PDF no debe poder lanzar programas ni abrir archivos locales.
    auto starts = [&](const char* p) { return _strnicmp(uri.c_str(), p, strlen(p)) == 0; };
    return starts("http://") || starts("https://") || starts("mailto:");
}

}  // namespace

// ---------------------------------------------------------------------------------------------

BackBuffer::~BackBuffer() {
    if (m_dc) DeleteDC(m_dc);
    if (m_bmp) DeleteObject(m_bmp);
}

HDC BackBuffer::acquire(HDC like, int w, int h) {
    if (!m_dc) m_dc = CreateCompatibleDC(like);
    if (!m_bmp || w != m_w || h != m_h) {
        HBITMAP bmp = CreateCompatibleBitmap(like, w, h);
        SelectObject(m_dc, bmp);
        if (m_bmp) DeleteObject(m_bmp);
        m_bmp = bmp;
        m_w = w;
        m_h = h;
    }
    return m_dc;
}

// ---------------------------------------------------------------------------------------------

MainWindow::MainWindow() : m_bgBrush(CreateSolidBrush(kBgColor)) {}

MainWindow::~MainWindow() { DeleteObject(m_bgBrush); }

bool MainWindow::create(HINSTANCE inst, int show) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = kClassName;
    if (!RegisterClassW(&wc)) return false;
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_TREEVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    CreateWindowExW(WS_EX_ACCEPTFILES, kClassName, m_fileName.c_str(),
                    WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL, CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    900, 1000, nullptr, nullptr, inst, this);
    if (!m_hwnd) return false;

    const HINSTANCE hi = inst;
    m_tree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                             WS_CHILD | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
                                 TVS_SHOWSELALWAYS | TVS_FULLROWSELECT,
                             0, 0, 0, 0, m_hwnd, nullptr, hi, nullptr);
    m_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, m_hwnd,
                             nullptr, hi, nullptr);
    const HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessageW(m_tree, WM_SETFONT, (WPARAM)font, FALSE);
    SendMessageW(m_edit, WM_SETFONT, (WPARAM)font, FALSE);
    SetWindowLongPtrW(m_edit, GWLP_USERDATA, (LONG_PTR)this);
    m_editOldProc = (WNDPROC)SetWindowLongPtrW(m_edit, GWLP_WNDPROC, (LONG_PTR)editProc);
    const RECT er = m_toolbar.searchEditRect();
    MoveWindow(m_edit, er.left, er.top, er.right - er.left, er.bottom - er.top, FALSE);
    m_toolbar.setEnabled(Command::ToggleToc, false);  // hasta que haya un PDF con indice

    m_settings.load();
    const Settings::Window& sw = m_settings.window();
    const RECT saved = {sw.x, sw.y, sw.x + sw.w, sw.y + sw.h};
    if (sw.valid && MonitorFromRect(&saved, MONITOR_DEFAULTTONULL)) {
        // Solo si el rectangulo guardado sigue cayendo en algun monitor (puede haberse desconectado).
        WINDOWPLACEMENT wp = {sizeof(wp)};
        wp.rcNormalPosition = saved;
        wp.showCmd = sw.maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
        SetWindowPlacement(m_hwnd, &wp);
    } else {
        ShowWindow(m_hwnd, show);
    }
    return true;
}

LRESULT CALLBACK MainWindow::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self;
    if (msg == WM_NCCREATE) {
        self = (MainWindow*)((CREATESTRUCTW*)lp)->lpCreateParams;
        self->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = (MainWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }
    return self ? self->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT MainWindow::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: onPaint(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) layout();
            return 0;
        case WM_NOTIFY: onNotify((NMHDR*)lp); return 0;
        case WM_HSCROLL: onScrollBar(SB_HORZ, wp); return 0;
        case WM_VSCROLL: onScrollBar(SB_VERT, wp); return 0;
        case WM_MOUSEWHEEL: onMouseWheel(GET_WHEEL_DELTA_WPARAM(wp)); return 0;
        case WM_KEYDOWN: onKeyDown(wp); return 0;
        case WM_LBUTTONDOWN: onMouseDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
        case WM_MOUSEMOVE: onMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
        case WM_LBUTTONUP: onMouseUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
        case WM_CAPTURECHANGED: m_dragging = false; return 0;
        case WM_MOUSELEAVE:
            if (m_toolbar.setHover(Command::None)) InvalidateRect(m_hwnd, nullptr, FALSE);
            return 0;
        case WM_SETCURSOR: {
            if (LOWORD(lp) != HTCLIENT) break;
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(m_hwnd, &p);
            const bool overPage = m_doc.isOpen() && p.y >= Toolbar::kHeight && p.x >= viewLeft();
            const bool overLink =
                overPage && !m_dragging && linkAtClient(p.x, p.y).kind != PdfDocument::LinkTarget::None;
            SetCursor(LoadCursor(nullptr, overLink ? IDC_HAND : overPage ? IDC_IBEAM : IDC_ARROW));
            return TRUE;
        }
        case WM_DROPFILES: {
            wchar_t path[MAX_PATH];
            if (DragQueryFileW((HDROP)wp, 0, path, MAX_PATH)) openFile(path);
            DragFinish((HDROP)wp);
            return 0;
        }
        case WM_CLOSE:
            saveDocPos();
            saveWindow();
            m_settings.save();
            break;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

// --- Comandos y entrada ----------------------------------------------------------------------

void MainWindow::execute(Command cmd) {
    const bool hasDoc = m_doc.isOpen();
    switch (cmd) {
        case Command::Open: chooseFile(); break;
        case Command::Copy: copySelection(); break;
        case Command::SelectAll: selectAll(); break;
        case Command::ZoomIn:
            if (hasDoc && m_vp.zoomBy(Viewport::kStep)) {
                m_fit = false;
                refresh();
            }
            break;
        case Command::ZoomOut:
            if (hasDoc && m_vp.zoomBy(1 / Viewport::kStep)) {
                m_fit = false;
                refresh();
            }
            break;
        case Command::ZoomReset:
            if (hasDoc && m_vp.zoomTo(1.0f)) {
                m_fit = false;
                refresh();
            }
            break;
        case Command::FitWidth:
            if (hasDoc) {
                m_fit = true;
                // Mostrar u ocultar la barra vertical cambia el ancho disponible: se ajusta
                // otra vez con las barras ya definitivas.
                for (int pass = 0; pass < 2; pass++) {
                    m_vp.fitWidth();
                    refresh();
                }
            }
            break;
        case Command::Search: toggleSearch(); break;
        case Command::ToggleToc: toggleToc(); break;
        case Command::SearchPrev: runSearch(true); break;
        case Command::SearchNext: runSearch(false); break;
        case Command::PrevPage: goToPage(m_vp.currentPage() - 1); break;
        case Command::NextPage: goToPage(m_vp.currentPage() + 1); break;
        case Command::FirstPage: goToPage(0); break;
        case Command::LastPage: goToPage(m_doc.pageCount() - 1); break;
        case Command::GoToPage: promptGoToPage(); break;
        case Command::ScrollUp: m_vp.scrollBy(0, -kLineStep); refresh(); break;
        case Command::ScrollDown: m_vp.scrollBy(0, kLineStep); refresh(); break;
        case Command::ScrollLeft: m_vp.scrollBy(-kLineStep, 0); refresh(); break;
        case Command::ScrollRight: m_vp.scrollBy(kLineStep, 0); refresh(); break;
        case Command::ScrollPageDown: m_vp.scrollBy(0, m_vp.viewH() - 40); refresh(); break;
        case Command::None: break;
    }
}

void MainWindow::onKeyDown(WPARAM vk) {
    const bool ctrl = IsDown(VK_CONTROL);
    if (vk == VK_F3) {
        execute(IsDown(VK_SHIFT) ? Command::SearchPrev : Command::SearchNext);
        return;
    }
    if (vk == VK_ESCAPE && m_searchOpen) {
        toggleSearch();
        return;
    }
    for (const KeyBinding& k : kKeys) {
        if (k.vk == vk && (k.ctrl < 0 || (k.ctrl == 1) == ctrl)) {
            execute(k.cmd);
            return;
        }
    }
}

void MainWindow::onMouseWheel(int delta) {
    if (!m_doc.isOpen()) return;
    if (IsDown(VK_CONTROL)) {
        execute(delta > 0 ? Command::ZoomIn : Command::ZoomOut);
    } else if (IsDown(VK_MENU)) {
        execute(delta > 0 ? Command::PrevPage : Command::NextPage);
    } else {
        m_vp.scrollBy(0, -delta / 2);
        refresh();
    }
}

void MainWindow::onScrollBar(int bar, WPARAM wp) {
    SCROLLINFO si = {sizeof(si), SIF_ALL};
    GetScrollInfo(m_hwnd, bar, &si);
    int pos = si.nPos;
    switch (LOWORD(wp)) {
        case SB_LINEUP: pos -= 40; break;
        case SB_LINEDOWN: pos += 40; break;
        case SB_PAGEUP: pos -= (int)si.nPage; break;
        case SB_PAGEDOWN: pos += (int)si.nPage; break;
        case SB_THUMBTRACK: pos = si.nTrackPos; break;
    }
    if (bar == SB_HORZ) m_vp.scrollTo(pos, m_vp.scrollY());
    else m_vp.scrollTo(m_vp.scrollX(), pos);
    refresh();
}

void MainWindow::onMouseDown(int x, int y) {
    SetFocus(m_hwnd);  // saca el foco del cuadro de busqueda / indice para que funcionen los atajos
    if (y < Toolbar::kHeight) {
        execute(m_toolbar.hitTest(x, y));
    } else if (m_doc.isOpen()) {
        m_downX = x;
        m_downY = y;
        m_selA = m_selB = clientToSel(x, y);
        m_selCache.clear();
        m_hasSel = false;
        m_dragging = true;
        SetCapture(m_hwnd);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::onMouseMove(int x, int y) {
    if (m_dragging) {
        updateSelection(clientToSel(x, y));
        return;
    }
    if (m_toolbar.setHover(m_toolbar.hitTest(x, y))) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
        TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, m_hwnd, 0};
        TrackMouseEvent(&t);
    }
}

void MainWindow::onMouseUp(int x, int y) {
    if (!m_dragging) return;
    ReleaseCapture();
    // Un clic (sin arrastrar) sobre un enlace lo sigue.
    if (abs(x - m_downX) <= 3 && abs(y - m_downY) <= 3 && y >= Toolbar::kHeight) followLink(x, y);
}

// --- Documento y seleccion -------------------------------------------------------------------

void MainWindow::openFile(const std::wstring& rawPath) {
    wchar_t full[4 * MAX_PATH];
    const DWORD n = GetFullPathNameW(rawPath.c_str(), (DWORD)std::size(full), full, nullptr);
    const std::wstring path = (n > 0 && n < std::size(full)) ? std::wstring(full) : rawPath;
    const size_t slash = path.find_last_of(L"\\/");
    const std::wstring name = path.substr(slash == std::wstring::npos ? 0 : slash + 1);

    const DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        MessageBoxW(m_hwnd, (L"No se encontro el archivo:\n" + path).c_str(), L"Pidufi", MB_ICONERROR);
        return;
    }

    std::wstring password;
    for (;;) {
        const PdfDocument::OpenResult r = m_doc.open(path, password);
        if (r == PdfDocument::OpenResult::Ok) break;
        if (r == PdfDocument::OpenResult::Failed) {
            MessageBoxW(m_hwnd,
                        (L"No se pudo abrir \"" + name +
                         L"\".\nEl archivo esta danado, esta en uso o no es un PDF valido.")
                            .c_str(),
                        L"Pidufi", MB_ICONERROR);
            return;
        }
        if (r == PdfDocument::OpenResult::BadPassword)
            MessageBoxW(m_hwnd, L"La contrasena es incorrecta.", L"Pidufi", MB_ICONWARNING);
        if (!askPassword(&password)) return;
    }
    SecureZeroMemory(password.data(), password.size() * sizeof(wchar_t));

    saveDocPos();  // el documento anterior (m_vp todavia lo describe)
    m_filePath = path;
    m_fileName = name;
    m_cache.clear();
    clearSelection();
    clearSearch();
    m_vp.setPages(m_doc.pageSizes());
    fillToc();
    setTocVisible(false);  // el indice arranca desactivado en cada documento
    restoreDocPos();
}

void MainWindow::saveDocPos() {
    if (m_filePath.empty() || m_vp.pageCount() == 0) return;
    const float s = m_vp.scale();
    const int page = m_vp.pageAtY(m_vp.scrollY());
    Settings::DocPos pos;
    pos.page = page;
    pos.offX = m_vp.scrollX() / s;
    pos.offY = (m_vp.scrollY() - m_vp.pageTop(page)) / s;
    pos.zoom = m_vp.zoom();
    pos.fit = m_fit;
    m_settings.setDocPos(m_filePath, pos);
}

// Vuelve a donde se dejo el documento; si es la primera vez, lo abre al 100%.
void MainWindow::restoreDocPos() {
    Settings::DocPos pos;
    if (!m_settings.docPos(m_filePath, &pos) || pos.page < 0 || pos.page >= m_doc.pageCount()) {
        execute(Command::ZoomReset);
        m_fit = false;
        m_vp.scrollTo(0, 0);
        refresh();
        return;
    }
    if (pos.fit) {
        // Primero a la fila guardada y luego se ajusta: el ajuste conserva la fila superior.
        m_vp.scrollTo(0, m_vp.pageTop(pos.page) + (int)(pos.offY * m_vp.scale()));
        execute(Command::FitWidth);
        return;
    }
    m_vp.zoomTo(pos.zoom);
    m_fit = false;
    const float s = m_vp.scale();
    m_vp.scrollTo((int)(pos.offX * s), m_vp.pageTop(pos.page) + (int)(pos.offY * s));
    refresh();
}

void MainWindow::saveWindow() {
    WINDOWPLACEMENT wp = {sizeof(wp)};
    if (!GetWindowPlacement(m_hwnd, &wp)) return;
    const RECT& r = wp.rcNormalPosition;
    m_settings.setWindow({r.left, r.top, r.right - r.left, r.bottom - r.top,
                          wp.showCmd == SW_SHOWMAXIMIZED, true});
}

bool MainWindow::askPassword(std::wstring* password) {
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_PASSWORD), m_hwnd,
                           PasswordProc, (LPARAM)password) == IDOK;
}

void MainWindow::promptGoToPage() {
    if (!m_doc.isOpen()) return;
    GotoData d = {m_vp.currentPage() + 1, m_doc.pageCount()};
    if (DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_GOTO), m_hwnd, GotoProc,
                        (LPARAM)&d) == IDOK)
        goToPage(d.page - 1);
}

PdfDocument::LinkTarget MainWindow::linkAtClient(int x, int y) {
    const SelPoint sp = clientToSel(x, y);
    return m_doc.linkAt(sp.page, sp.pt);
}

void MainWindow::followLink(int x, int y) {
    const PdfDocument::LinkTarget t = linkAtClient(x, y);
    if (t.kind == PdfDocument::LinkTarget::Page) {
        jumpTo(t.page, t.y);
    } else if (t.kind == PdfDocument::LinkTarget::External && IsSafeExternalUri(t.uri)) {
        ShellExecuteW(m_hwnd, L"open", FromUtf8(t.uri.c_str()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void MainWindow::goToPage(int index) { jumpTo(index, 0); }

void MainWindow::jumpTo(int index, float yPts) {
    if (!m_doc.isOpen()) return;
    index = std::clamp(index, 0, m_doc.pageCount() - 1);
    m_vp.scrollTo(m_vp.scrollX(), m_vp.pageTop(index) + (int)(yPts * m_vp.scale()) - Viewport::kGap);
    refresh();
}

void MainWindow::chooseFile() {
    wchar_t buf[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"PDF (*.pdf)\0*.pdf\0Todos\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) openFile(buf);
}

MainWindow::SelPoint MainWindow::clientToSel(int x, int y) const {
    const int contentY = m_vp.scrollY() + (y - Toolbar::kHeight);
    const int page = m_vp.pageAtY(contentY);
    const float s = m_vp.scale();
    return {page, {(x - viewLeft() - m_vp.pageX(page)) / s, (contentY - m_vp.pageTop(page)) / s}};
}

void MainWindow::clearSelection() {
    m_selCache.clear();
    m_hasSel = m_dragging = false;
}

void MainWindow::updateSelection(SelPoint b) {
    m_selB = b;
    m_hasSel = true;
    m_selCache.clear();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::selectAll() {
    if (!m_doc.isOpen()) return;
    m_selA = {0, {-kFar, -kFar}};
    updateSelection({m_doc.pageCount() - 1, {kFar, kFar}});
}

// Extremos de la seleccion en orden de lectura (por pagina; dentro de una pagina lo decide MuPDF).
void MainWindow::selSpan(SelPoint* lo, SelPoint* hi) const {
    *lo = m_selA;
    *hi = m_selB;
    if (lo->page > hi->page) std::swap(*lo, *hi);
}

// Puntos de inicio y fin de la seleccion dentro de `page`: las paginas intermedias van completas.
void MainWindow::pageSelection(int page, PointF* a, PointF* b) const {
    SelPoint lo, hi;
    selSpan(&lo, &hi);
    *a = page == lo.page ? lo.pt : PointF{-kFar, -kFar};
    *b = page == hi.page ? hi.pt : PointF{kFar, kFar};
}

const std::vector<RectF>& MainWindow::selRectsFor(int page) {
    auto it = m_selCache.find(page);
    if (it != m_selCache.end()) return it->second;
    std::vector<RectF> rects;
    SelPoint lo, hi;
    selSpan(&lo, &hi);
    if (page >= lo.page && page <= hi.page) {
        PointF a, b;
        pageSelection(page, &a, &b);
        rects = m_doc.selectionRects(page, a, b);
    }
    return m_selCache.emplace(page, std::move(rects)).first->second;
}

void MainWindow::copySelection() {
    if (!m_hasSel) return;
    SelPoint lo, hi;
    selSpan(&lo, &hi);
    std::wstring text;
    for (int page = lo.page; page <= hi.page; page++) {
        PointF a, b;
        pageSelection(page, &a, &b);
        const std::wstring t = m_doc.selectionText(page, a, b);
        if (t.empty()) continue;
        if (!text.empty()) text += L"\r\n";
        text += t;
    }
    if (text.empty()) return;
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!mem) return;
    memcpy(GlobalLock(mem), text.c_str(), bytes);
    GlobalUnlock(mem);
    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        if (SetClipboardData(CF_UNICODETEXT, mem)) mem = nullptr;  // el portapapeles es el dueno
        CloseClipboard();
    }
    if (mem) GlobalFree(mem);
}

int MainWindow::zoomPercent() const {
    return m_doc.isOpen() ? (int)(m_vp.zoom() * 100 + 0.5f) : 0;
}

// --- Indice y busqueda -----------------------------------------------------------------------

void MainWindow::fillToc() {
    TreeView_DeleteAllItems(m_tree);
    const std::vector<PdfDocument::TocItem>& toc = m_doc.toc();
    std::vector<HTREEITEM> parents;  // ultimo item de cada nivel
    std::vector<HTREEITEM> roots;
    for (const PdfDocument::TocItem& it : toc) {
        const size_t level = std::min<size_t>(it.level, parents.size());
        TVINSERTSTRUCTW is = {};
        is.hParent = level == 0 ? TVI_ROOT : parents[level - 1];
        is.hInsertAfter = TVI_LAST;
        is.item.mask = TVIF_TEXT | TVIF_PARAM;
        is.item.pszText = const_cast<wchar_t*>(it.title.c_str());
        is.item.lParam = it.page;
        HTREEITEM h = TreeView_InsertItem(m_tree, &is);
        parents.resize(level + 1);
        parents[level] = h;
        if (level == 0) roots.push_back(h);
    }
    for (HTREEITEM h : roots) TreeView_Expand(m_tree, h, TVE_EXPAND);
    if (!roots.empty()) TreeView_EnsureVisible(m_tree, roots.front());
    m_toolbar.setEnabled(Command::ToggleToc, !toc.empty());
}

void MainWindow::setTocVisible(bool on) {
    m_tocVisible = on;
    ShowWindow(m_tree, on ? SW_SHOW : SW_HIDE);
    m_toolbar.setActive(Command::ToggleToc, on);
    layout();
}

void MainWindow::toggleToc() {
    if (m_doc.toc().empty()) return;  // el PDF no trae indice
    setTocVisible(!m_tocVisible);
}

void MainWindow::onNotify(NMHDR* nh) {
    if (nh->hwndFrom != m_tree || nh->code != TVN_SELCHANGEDW) return;
    const NMTREEVIEWW* tv = (const NMTREEVIEWW*)nh;
    // Solo cuando el usuario elige un item (no al llenar el arbol por codigo).
    if (tv->action != TVC_BYMOUSE && tv->action != TVC_BYKEYBOARD) return;
    const int page = (int)tv->itemNew.lParam;
    if (page >= 0) goToPage(page);
}

void MainWindow::toggleSearch() {
    m_searchOpen = !m_searchOpen;
    m_toolbar.setSearchOpen(m_searchOpen);
    ShowWindow(m_edit, m_searchOpen ? SW_SHOW : SW_HIDE);
    if (m_searchOpen) {
        SetFocus(m_edit);
        SendMessageW(m_edit, EM_SETSEL, 0, -1);
    } else {
        SetFocus(m_hwnd);
        clearSearch();
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::clearSearch() {
    m_term.clear();
    m_hits.clear();
    m_hitsByPage.clear();
    m_hitIdx = -1;
    m_toolbar.setSearchInfo(L"");
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::runSearch(bool backwards) {
    if (!m_doc.isOpen()) return;
    if (!m_searchOpen) toggleSearch();
    wchar_t buf[256];
    GetWindowTextW(m_edit, buf, 256);
    const std::wstring term = buf;
    if (term.empty()) return;

    if (term != m_term) {
        HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
        m_hits = m_doc.search(term);
        SetCursor(old);
        m_term = term;
        m_hitsByPage.clear();
        for (int i = 0; i < (int)m_hits.size(); i++) m_hitsByPage[m_hits[i].page].push_back(i);
        // Primera coincidencia desde la pagina actual (la ultima, si se busca hacia atras).
        const int cur = m_vp.currentPage();
        m_hitIdx = -1;
        if (backwards) {
            for (int i = (int)m_hits.size() - 1; i >= 0 && m_hitIdx < 0; i--)
                if (m_hits[i].page <= cur) m_hitIdx = i;
            if (m_hitIdx < 0 && !m_hits.empty()) m_hitIdx = (int)m_hits.size() - 1;
        } else {
            for (int i = 0; i < (int)m_hits.size() && m_hitIdx < 0; i++)
                if (m_hits[i].page >= cur) m_hitIdx = i;
            if (m_hitIdx < 0 && !m_hits.empty()) m_hitIdx = 0;
        }
    } else if (!m_hits.empty()) {
        const int n = (int)m_hits.size();
        m_hitIdx = (m_hitIdx + (backwards ? -1 : 1) + n) % n;
    }

    m_toolbar.setSearchInfo(m_hits.empty() ? L"Sin resultados"
                                           : std::to_wstring(m_hitIdx + 1) + L" / " +
                                                 std::to_wstring(m_hits.size()));
    showHit();
}

// Desplaza la vista hasta la coincidencia actual.
void MainWindow::showHit() {
    if (m_hitIdx >= 0 && m_hitIdx < (int)m_hits.size()) {
        const PdfDocument::Hit& h = m_hits[m_hitIdx];
        const float s = m_vp.scale();
        int x = m_vp.scrollX();
        const int hitX = (m_vp.contentW() - m_vp.pageW(h.page)) / 2 + (int)(h.rect.x0 * s);
        if (hitX < x || hitX > x + m_vp.viewW() - 20) x = hitX - m_vp.viewW() / 2;
        m_vp.scrollTo(x, m_vp.pageTop(h.page) + (int)(h.rect.y0 * s) - m_vp.viewH() / 3);
    }
    refresh();
}

LRESULT CALLBACK MainWindow::editProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = (MainWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_KEYDOWN:
            if (wp == VK_RETURN) {
                self->runSearch(IsDown(VK_SHIFT));
                return 0;
            }
            if (wp == VK_ESCAPE) {
                self->toggleSearch();
                return 0;
            }
            if (wp == 'A' && IsDown(VK_CONTROL)) {
                SendMessageW(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
            break;
        case WM_CHAR:  // evita el pitido de Enter / Esc / Ctrl+A en un cuadro de una linea
            if (wp == VK_RETURN || wp == VK_ESCAPE || wp == 1) return 0;
            break;
    }
    return CallWindowProcW(self->m_editOldProc, hwnd, msg, wp, lp);
}

// --- Pantalla --------------------------------------------------------------------------------

// Reparte el area cliente: indice a la izquierda (si esta abierto) y la vista del documento.
void MainWindow::layout() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int h = std::max(0, (int)rc.bottom - Toolbar::kHeight);
    if (m_tree) MoveWindow(m_tree, 0, Toolbar::kHeight, kTocWidth, h, TRUE);
    m_vp.setView(std::max(0, (int)rc.right - viewLeft()), h);
    syncScrollbars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::refresh() {
    std::wstring title = m_fileName;
    if (m_doc.isOpen()) {
        title += L"  -  pag " + std::to_wstring(m_vp.currentPage() + 1) + L"/" +
                 std::to_wstring(m_doc.pageCount()) + L"  -  " + std::to_wstring(zoomPercent()) +
                 L"%";
    }
    SetWindowTextW(m_hwnd, title.c_str());
    syncScrollbars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::syncScrollbars() {
    SCROLLINFO si = {sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS};
    // El rango de una barra es [0, nMax]: la ultima posicion es tamano - 1, no tamano.
    si.nMax = std::max(0, m_vp.contentW() - 1);
    si.nPage = m_vp.viewW();
    si.nPos = m_vp.scrollX();
    SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
    si.nMax = std::max(0, m_vp.contentH() - 1);
    si.nPage = m_vp.viewH();
    si.nPos = m_vp.scrollY();
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

// Mantiene en cache, por pagina, solo la zona visible mas un margen; vuelve a dibujar cuando la
// vista sale de ella. Las paginas lejanas se descartan.
void MainWindow::ensureRendered() {
    if (!m_doc.isOpen()) return;
    int first, last;
    m_vp.visibleRange(&first, &last);
    const float scale = m_vp.scale();
    const int vw = m_vp.viewW(), vh = m_vp.viewH();

    for (int p = first; p <= last; p++) {
        const RECT page = {0, 0, m_vp.pageW(p), m_vp.pageH(p)};
        const int px = m_vp.pageX(p), py = m_vp.pageY(p);  // esquina de la pagina en la vista
        const RECT want = {-px, -py, -px + vw, -py + vh};   // la vista, en pixeles de la pagina
        RECT need;
        if (!IntersectRect(&need, &want, &page)) continue;
        PageCache& c = m_cache[p];
        if (c.pix && c.scale == scale && Contains(c.rect, need)) continue;

        RECT region = {need.left - vw / 4, need.top - vh / 2, need.right + vw / 4,
                       need.bottom + vh / 2};
        IntersectRect(&region, &region, &page);
        c.pix = m_doc.render(p, scale, region);
        c.rect = region;
        c.scale = scale;
    }
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        it = (it->first < first - 1 || it->first > last + 1) ? m_cache.erase(it) : std::next(it);
    }
}

void MainWindow::onPaint() {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(m_hwnd, &ps);
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    if (ps.rcPaint.top < Toolbar::kHeight) m_toolbar.draw(dc, rc.right, zoomPercent());
    const int w = rc.right - viewLeft(), h = rc.bottom - Toolbar::kHeight;
    if (w > 0 && h > 0 && ps.rcPaint.bottom > Toolbar::kHeight) {
        ensureRendered();
        HDC mem = m_back.acquire(dc, w, h);
        paintView(mem, w, h);
        BitBlt(dc, viewLeft(), Toolbar::kHeight, w, h, mem, 0, 0, SRCCOPY);
    }
    EndPaint(m_hwnd, &ps);
}

void MainWindow::paintView(HDC dc, int w, int h) {
    const RECT view = {0, 0, w, h};
    FillRect(dc, &view, m_bgBrush);

    if (!m_doc.isOpen()) {
        RECT r = view;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(220, 220, 220));
        DrawTextW(dc, L"Ctrl+O para abrir un PDF", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    int first, last;
    m_vp.visibleRange(&first, &last);
    const float s = m_vp.scale();
    HBRUSH hitBrush = m_hits.empty() ? nullptr : CreateSolidBrush(kHitColor);
    HBRUSH curBrush = m_hits.empty() ? nullptr : CreateSolidBrush(kCurHitColor);
    for (int p = first; p <= last; p++) {
        const int px = m_vp.pageX(p), py = m_vp.pageY(p);
        const RECT pageInView = {px, py, px + m_vp.pageW(p), py + m_vp.pageH(p)};
        RECT visible;
        if (!IntersectRect(&visible, &pageInView, &view)) continue;
        FillRect(dc, &visible, (HBRUSH)GetStockObject(WHITE_BRUSH));  // hasta que haya imagen

        auto it = m_cache.find(p);
        if (it != m_cache.end() && it->second.pix) {
            const PageCache& c = it->second;
            const RECT cacheInView = {c.rect.left + px, c.rect.top + py, c.rect.right + px,
                                      c.rect.bottom + py};
            RECT vis;
            if (IntersectRect(&vis, &cacheInView, &view)) {
                const int vw = vis.right - vis.left, vh = vis.bottom - vis.top;
                const int stride = c.pix.stride();
                // ySrc es poco fiable en DIB de arriba a abajo (con 0 devuelve las filas
                // superiores y con otros valores cuenta desde abajo). Se evita apuntando a la
                // primera fila visible.
                const unsigned char* bits =
                    c.pix.samples() + (size_t)(vis.top - cacheInView.top) * stride;
                BITMAPINFO bi = {};
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = stride / 4;
                bi.bmiHeader.biHeight = -vh;  // de arriba a abajo, solo las filas visibles
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;
                StretchDIBits(dc, vis.left, vis.top, vw, vh, vis.left - cacheInView.left, 0, vw,
                              vh, bits, &bi, DIB_RGB_COLORS, SRCCOPY);
            }
        }

        // Coincidencias de la busqueda: se multiplica el color de fondo por el amarillo (el texto
        // negro se conserva). La actual va en naranja.
        auto hitsIt = m_hitsByPage.find(p);
        if (hitsIt != m_hitsByPage.end()) {
            for (int idx : hitsIt->second) {
                const RectF& r = m_hits[idx].rect;
                const int x = px + (int)(r.x0 * s), y = py + (int)(r.y0 * s);
                const int bw = (int)((r.x1 - r.x0) * s) + 1, bh = (int)((r.y1 - r.y0) * s) + 1;
                HGDIOBJ old = SelectObject(dc, idx == m_hitIdx ? curBrush : hitBrush);
                BitBlt(dc, x, y, bw, bh, dc, x, y, MERGECOPY);
                SelectObject(dc, old);
            }
        }

        // Texto seleccionado: se invierten los colores bajo cada linea.
        if (m_hasSel) {
            for (const RectF& r : selRectsFor(p)) {
                PatBlt(dc, px + (int)(r.x0 * s), py + (int)(r.y0 * s),
                       (int)((r.x1 - r.x0) * s) + 1, (int)((r.y1 - r.y0) * s) + 1, DSTINVERT);
            }
        }
    }
    if (hitBrush) DeleteObject(hitBrush);
    if (curBrush) DeleteObject(curBrush);
}
