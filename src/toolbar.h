#pragma once
#include <windows.h>
#include <array>
#include <string>
#include "commands.h"

// Barra superior con botones dibujados con GDI. Para agregar un boton: una funcion de icono y
// una entrada en kButtons (toolbar.cpp).
class Toolbar {
public:
    static constexpr int kHeight = 44;
    static constexpr int kButtonCount = 9;

    Toolbar();
    ~Toolbar();
    Toolbar(const Toolbar&) = delete;
    Toolbar& operator=(const Toolbar&) = delete;

    Command hitTest(int x, int y) const;
    // Devuelve true si cambio el boton resaltado (hay que repintar la barra).
    bool setHover(Command c);
    // Boton "encendido" (p. ej. el panel del indice esta abierto).
    void setActive(Command c, bool on);
    // Boton deshabilitado: se dibuja atenuado y no responde.
    void setEnabled(Command c, bool on);
    // Muestra u oculta los controles de busqueda (cuadro de texto, anterior / siguiente, contador).
    void setSearchOpen(bool open);
    void setSearchInfo(const std::wstring& info) { m_searchInfo = info; }
    RECT searchEditRect() const { return m_editRect; }
    // `zoomPercent` <= 0 oculta el porcentaje.
    void draw(HDC dc, int width, int zoomPercent) const;

private:
    int indexOf(Command c) const;
    bool visible(int i) const;

    std::array<RECT, kButtonCount> m_rects;
    std::array<bool, kButtonCount> m_active{};
    std::array<bool, kButtonCount> m_disabled{};
    RECT m_editRect = {};
    bool m_searchOpen = false;
    std::wstring m_searchInfo;
    Command m_hover = Command::None;
    HBRUSH m_bgBrush, m_hoverBrush, m_activeBrush;
    HPEN m_pen, m_dimPen;
};
