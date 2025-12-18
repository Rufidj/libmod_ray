#include "libmod_ray.h"
#include <math.h>

/* ============================================================================
   FUNCIONES DE GEOMETRÍA
   Portadas desde shape.cpp de Andrew Lim
   ============================================================================ */

/**
 * Verifica si dos líneas se intersectan
 * Basado en: http://paulbourke.net/geometry/pointlineplane/javascript.txt
 */
int ray_lines_intersect(float x1, float y1, float x2, float y2,
                        float x3, float y3, float x4, float y4,
                        float* ix, float* iy)
{
    /* Verificar que ninguna línea tenga longitud 0 */
    if ((x1 == x2 && y1 == y2) || (x3 == x4 && y3 == y4)) {
        return 0;
    }

    float denominator = ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1));

    /* Líneas paralelas */
    if (denominator == 0) {
        return 0;
    }

    float ua = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / denominator;
    float ub = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / denominator;

    /* Verificar si la intersección está dentro de los segmentos */
    if (ua < 0 || ua > 1 || ub < 0 || ub > 1) {
        return 0;
    }

    /* Calcular punto de intersección */
    float x = x1 + ua * (x2 - x1);
    float y = y1 + ua * (y2 - y1);

    if (ix) {
        *ix = x;
    }
    if (iy) {
        *iy = y;
    }
    
    return 1;
}

/**
 * Verifica si un punto está dentro de un rectángulo
 */
int ray_point_in_rect(float ptx, float pty, float x, float y, float w, float h)
{
    return x <= ptx && ptx <= (x + w) &&
           y <= pty && pty <= (y + h);
}

/**
 * Función auxiliar para cálculo de punto en triángulo
 */
float ray_sign(float p1x, float p1y, float p2x, float p2y, float p3x, float p3y)
{
    return (p1x - p3x) * (p2y - p3y) - (p2x - p3x) * (p1y - p3y);
}

/**
 * Verifica si un punto está dentro de un triángulo
 * Basado en: https://stackoverflow.com/a/2049593/1645045
 */
int ray_point_in_triangle(float ptx, float pty,
                          float v1x, float v1y,
                          float v2x, float v2y,
                          float v3x, float v3y)
{
    float d1, d2, d3;
    int has_neg, has_pos;

    d1 = ray_sign(ptx, pty, v1x, v1y, v2x, v2y);
    d2 = ray_sign(ptx, pty, v2x, v2y, v3x, v3y);
    d3 = ray_sign(ptx, pty, v3x, v3y, v1x, v1y);

    has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

/**
 * Verifica si un punto está dentro de un cuadrilátero
 */
int ray_point_in_quad(float ptx, float pty,
                      float v1x, float v1y,
                      float v2x, float v2y,
                      float v3x, float v3y,
                      float v4x, float v4y)
{
    return ray_point_in_triangle(ptx, pty, v1x, v1y, v2x, v2y, v3x, v3y) ||
           ray_point_in_triangle(ptx, pty, v3x, v3y, v4x, v4y, v1x, v1y);
}
