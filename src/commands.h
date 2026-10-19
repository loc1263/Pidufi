#pragma once

// Acciones del visor. Las usan la barra de herramientas y los atajos de teclado.
enum class Command {
    None,
    Open,
    ZoomIn,
    ZoomOut,
    ZoomReset,
    FitWidth,
    Search,
    ToggleToc,
    SearchPrev,
    SearchNext,
    PrevPage,
    NextPage,
    FirstPage,
    LastPage,
    GoToPage,
    Copy,
    SelectAll,
    ScrollUp,
    ScrollDown,
    ScrollLeft,
    ScrollRight,
    ScrollPageDown,
};
