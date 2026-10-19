# Pidufi

Lector de PDF simple y liviano para Windows, escrito en C++17 con Win32 y [MuPDF](https://mupdf.com/). Sin dependencias en tiempo de ejecucion salvo una DLL.

## Caracteristicas

- Abrir PDF desde el boton, con `Ctrl+O`, arrastrando el archivo a la ventana o desde la linea de comandos (`pidufi.exe archivo.pdf`).
- Todas las paginas se ven seguidas en una columna (desplazamiento continuo), con el numero de pagina actual en el titulo.
- Zoom del 10% al 800% y ajuste al ancho de la hoja.
- Busqueda de texto (`Ctrl+F`) e indice del documento (`Ctrl+T`), cuando el PDF lo trae.
- Enlaces del PDF: los internos (indice, referencias) saltan a su destino y los de web o correo se abren con el programa predeterminado.
- PDF protegidos con contrasena: la pide al abrir.
- Recuerda el tamano y la posicion de la ventana, y la pagina y el zoom de cada PDF (en `%APPDATA%\Pidufi\settings.txt`).
- Seleccion de texto con el mouse (tambien entre varias paginas) y copia al portapapeles.
- Solo se renderiza la zona visible de las paginas visibles, por lo que el consumo de memoria no crece con el zoom ni con el largo del documento (un PDF de 3000 paginas abre en ~0,2 s y usa ~25 MB).

## Atajos

| Accion | Teclas |
|---|---|
| Abrir archivo | `Ctrl+O` |
| Acercar / alejar | `+` / `-`, o `Ctrl` + rueda |
| Zoom 100% | `Ctrl+0` |
| Ajustar al ancho | `Ctrl+9` |
| Ir a la pagina anterior / siguiente | `RePag` / `AvPag`, o `Alt` + rueda |
| Primera / ultima pagina | `Inicio` / `Fin` |
| Ir a una pagina | `Ctrl+G` |
| Buscar / siguiente / anterior | `Ctrl+F` / `F3` / `Mayus+F3` |
| Mostrar u ocultar el indice | `Ctrl+T` |
| Desplazar | Flechas, rueda, `Espacio`, barras de desplazamiento |
| Seleccionar todo el texto del documento | `Ctrl+A` |
| Copiar la seleccion | `Ctrl+C` |

La seleccion solo funciona en PDF con texto (no en escaneos sin OCR).

## Compilar

Requisitos:

- [MSYS2](https://www.msys2.org/) con el entorno UCRT64.
- Paquete de MuPDF: `pacman -S mingw-w64-ucrt-x86_64-libmupdf`

Ejecuta:

```
build.bat
```

Genera `pidufi.exe`, que necesita `libopenjp2-7.dll` en la misma carpeta (el script la copia). Los scripts buscan MSYS2 solos (`msys.bat`): usan la variable `MSYS` si ya esta definida, luego `C:` a `F:` en `\msys64\ucrt64`, y por ultimo el `g++` del `PATH`. Si lo tienes en otro lugar, define antes la ruta: `set MSYS=X:\ruta\msys64\ucrt64`.

## Instalador

```
build_installer.bat
```

Compila Pidufi y genera `Pidufi-Setup-<version>.exe` con [NSIS](https://nsis.sourceforge.io/) (`pacman -S mingw-w64-ucrt-x86_64-nsis`).

- Instala por usuario en `%LOCALAPPDATA%\Programs\Pidufi` (no requiere administrador).
- Crea accesos directos en el Menu Inicio y el Escritorio, y una entrada en "Aplicaciones instaladas".
- Registra Pidufi en "Abrir con" para `.pdf`, sin cambiar la aplicacion predeterminada.
- Instalacion silenciosa: `Pidufi-Setup-<version>.exe /S`.

Para cambiar la version, edita `VERSION` en `version.bat`; el instalador y el zip portable la toman de ahi.

## Estructura

```
src/
  main.cpp          Punto de entrada
  main_window.*     Ventana, mensajes Win32, comandos, atajos (kKeys), seleccion y pintado
  pdf_document.*    Unico modulo que usa MuPDF: abrir, medir paginas, renderizar, texto
  viewport.h        Columna de paginas, zoom y desplazamiento (solo geometria, sin Win32 ni MuPDF)
  geometry.h        Tipos compartidos: PointF, RectF, PageSize
  toolbar.*         Barra superior; botones definidos en la tabla kButtons
  commands.h        Enum Command, compartido por la barra y el teclado
  settings.*        Preferencias guardadas (ventana, pagina y zoom por PDF)
  util.*            Conversion UTF-8 / UTF-16
res/
  resource.h        Identificadores de los dialogos (ir a pagina, contrasena)
  pidufi.rc         Icono y dialogos
installer/
  pidufi.nsi        Script del instalador (NSIS)
build.bat           Compila pidufi.exe
build_installer.bat Compila y genera el instalador
```

### Agregar una funcion

1. Agrega un valor a `Command` (`src/commands.h`) y su caso en `MainWindow::execute`.
2. Atajo de teclado: una linea en `kKeys` (`main_window.cpp`). Boton: una funcion de icono y una linea en `kButtons` (`toolbar.cpp`).

### Notas de diseno

- Al abrir se mide el tamano de todas las paginas para poder calcular la columna; las paginas se cargan despues bajo demanda y `PdfDocument` conserva las ultimas 16.
- Cada pagina visible tiene su propio cache de imagen (zona visible + margen); las que quedan lejos se descartan.
- El render ocurre al pintar, de modo que varias acciones seguidas (por ejemplo girar la rueda con `Ctrl`) se juntan en un solo dibujo.
- El texto de una pagina (para seleccionar) se extrae la primera vez que se usa, y los rectangulos de resaltado se guardan por pagina hasta que cambia la seleccion.
- Dentro de `fz_try` no deben crearse objetos C++ con destructor, porque MuPDF usa `setjmp/longjmp` para sus excepciones.
- En `StretchDIBits` con imagenes de arriba hacia abajo, `ySrc` es poco fiable; por eso el codigo apunta directamente a la primera fila visible.

## Licencia

MuPDF se distribuye bajo AGPL-3.0 (o licencia comercial de Artifex). Como Pidufi se enlaza estaticamente con MuPDF, al publicar o distribuir el programa debes cumplir esa licencia, por ejemplo publicando el codigo fuente bajo AGPL-3.0. El archivo `LICENSE` contiene el texto de la AGPL-3.0.
