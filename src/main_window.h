#pragma once
#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "commands.h"
#include "pdf_document.h"
#include "settings.h"
#include "toolbar.h"
#include "viewport.h"

// Doble buffer reutilizable para pintar sin parpadeo.
class BackBuffer {
public:
    ~BackBuffer();
    HDC acquire(HDC like, int w, int h);

private:
    HDC m_dc = nullptr;
    HBITMAP m_bmp = nullptr;
    int m_w = 0, m_h = 0;
};

class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    bool create(HINSTANCE inst, int show);
    void openFile(const std::wstring& path);

private:
    // Punto de una seleccion: pagina + posicion dentro de ella.
    struct SelPoint {
        int page = 0;
        PointF pt;
    };
    // Imagen ya renderizada de (parte de) una pagina.
    struct PageCache {
        Pixmap pix;
        RECT rect = {};  // zona cubierta, en pixeles de la pagina
        float scale = 0;
    };

    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);

    // Entrada
    void execute(Command cmd);
    void onKeyDown(WPARAM vk);
    void onMouseWheel(int delta);
    void onScrollBar(int bar, WPARAM wp);
    void onMouseDown(int x, int y);
    void onMouseMove(int x, int y);
    void onMouseUp(int x, int y);

    // Documento y seleccion
    void goToPage(int index);
    void jumpTo(int page, float yPts);  // pagina + posicion dentro de ella (puntos)
    void promptGoToPage();
    bool askPassword(std::wstring* password);
    void followLink(int x, int y);
    PdfDocument::LinkTarget linkAtClient(int x, int y);
    void saveDocPos();
    void restoreDocPos();
    void saveWindow();
    void chooseFile();
    void copySelection();
    void selectAll();
    void clearSelection();
    void updateSelection(SelPoint b);
    SelPoint clientToSel(int x, int y) const;
    void selSpan(SelPoint* lo, SelPoint* hi) const;
    void pageSelection(int page, PointF* a, PointF* b) const;
    const std::vector<RectF>& selRectsFor(int page);
    int zoomPercent() const;

    // Indice y busqueda
    void fillToc();
    void setTocVisible(bool on);
    void toggleToc();
    void toggleSearch();
    void runSearch(bool backwards);
    void showHit();
    void clearSearch();
    void onNotify(NMHDR* nh);
    static LRESULT CALLBACK editProc(HWND, UINT, WPARAM, LPARAM);

    // Pantalla
    int viewLeft() const { return m_tocVisible ? kTocWidth : 0; }  // x de la vista en la ventana
    void layout();  // reparte el area cliente entre indice y vista
    void refresh();  // titulo, barras de desplazamiento y repintado
    void syncScrollbars();
    void ensureRendered();
    void onPaint();
    void paintView(HDC dc, int w, int h);

    HWND m_hwnd = nullptr;
    HBRUSH m_bgBrush;
    // Orden importante: los Pixmap (m_cache) deben destruirse antes que m_doc (usa su contexto MuPDF).
    PdfDocument m_doc;
    Toolbar m_toolbar;
    Viewport m_vp;
    BackBuffer m_back;
    std::wstring m_fileName = L"Pidufi";
    std::wstring m_filePath;  // ruta completa del PDF abierto (vacia si no hay)
    Settings m_settings;
    bool m_fit = false;       // el zoom actual viene de "ajustar al ancho"
    int m_downX = 0, m_downY = 0;  // donde se pulso el boton, para distinguir clic de arrastre

    // Cache de render por pagina: solo la zona visible (mas un margen), no la pagina entera.
    std::unordered_map<int, PageCache> m_cache;

    // Seleccion de texto; puede abarcar varias paginas.
    SelPoint m_selA, m_selB;
    std::unordered_map<int, std::vector<RectF>> m_selCache;  // rectangulos por pagina
    bool m_hasSel = false, m_dragging = false;

    static constexpr int kTocWidth = 280;
    HWND m_tree = nullptr;  // indice (TreeView); oculto por defecto
    bool m_tocVisible = false;

    HWND m_edit = nullptr;  // cuadro de busqueda; oculto por defecto
    WNDPROC m_editOldProc = nullptr;
    bool m_searchOpen = false;
    std::wstring m_term;                       // texto de la ultima busqueda
    std::vector<PdfDocument::Hit> m_hits;      // coincidencias de m_term
    int m_hitIdx = -1;
    std::unordered_map<int, std::vector<int>> m_hitsByPage;  // indices en m_hits, para pintar
};
