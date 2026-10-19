#pragma once

// Coordenadas de pagina: puntos PDF (1/72") con origen en la esquina superior izquierda de la pagina.
struct PointF { float x = 0, y = 0; };
struct RectF { float x0 = 0, y0 = 0, x1 = 0, y1 = 0; };
struct PageSize { float w = 0, h = 0; };
