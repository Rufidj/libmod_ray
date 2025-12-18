#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
   PROYECCIÓN DE PORTALES
   ============================================================================ */

/**
 * Proyectar un portal a coordenadas de pantalla
 * Calcula qué columnas de pantalla son visibles a través del portal
 */
int ray_project_portal(RAY_Portal* portal, 
                       float cam_x, float cam_y, float cam_angle,
                       float screen_distance, int screen_w, int screen_h,
                       RAY_ClipWindow* parent_clip,
                       RAY_ClipWindow* out_clip)
{
    if (!portal || !out_clip || !parent_clip) return 0;
    if (!portal->enabled) return 0;
    
    /* Transformar puntos del portal a espacio de cámara */
    float cos_angle = cosf(-cam_angle);
    float sin_angle = sinf(-cam_angle);
    
    /* Punto 1 del portal */
    float dx1 = portal->x1 - cam_x;
    float dy1 = portal->y1 - cam_y;
    float cam_x1 = dx1 * cos_angle - dy1 * sin_angle;
    float cam_y1 = dx1 * sin_angle + dy1 * cos_angle;
    
    /* Punto 2 del portal */
    float dx2 = portal->x2 - cam_x;
    float dy2 = portal->y2 - cam_y;
    float cam_x2 = dx2 * cos_angle - dy2 * sin_angle;
    float cam_y2 = dx2 * sin_angle + dy2 * cos_angle;
    
    /* Si ambos puntos están detrás de la cámara, portal no visible */
    if (cam_y1 <= 0.1f && cam_y2 <= 0.1f) return 0;
    
    /* Clip contra near plane si es necesario */
    float near_plane = 0.1f;
    if (cam_y1 < near_plane || cam_y2 < near_plane) {
        /* TODO: Implementar clipping contra near plane */
        /* Por ahora, si algún punto está muy cerca, no renderizar */
        if (cam_y1 < near_plane) cam_y1 = near_plane;
        if (cam_y2 < near_plane) cam_y2 = near_plane;
    }
    
    /* Proyectar a pantalla */
    float screen_x1 = (cam_x1 * screen_distance / cam_y1) + screen_w / 2.0f;
    float screen_x2 = (cam_x2 * screen_distance / cam_y2) + screen_w / 2.0f;
    
    /* Asegurar que x1 < x2 */
    if (screen_x1 > screen_x2) {
        float temp = screen_x1;
        screen_x1 = screen_x2;
        screen_x2 = temp;
    }
    
    /* Calcular rango de columnas */
    int x_min = (int)floorf(screen_x1);
    int x_max = (int)ceilf(screen_x2);
    
    /* Clip contra bordes de pantalla */
    if (x_min < 0) x_min = 0;
    if (x_max >= screen_w) x_max = screen_w - 1;
    
    /* Intersectar con clip window padre */
    if (x_min < parent_clip->x_min) x_min = parent_clip->x_min;
    if (x_max > parent_clip->x_max) x_max = parent_clip->x_max;
    
    /* Si no hay overlap, portal no visible */
    if (x_min > x_max) return 0;
    
    /* Configurar clip window de salida */
    out_clip->x_min = x_min;
    out_clip->x_max = x_max;
    out_clip->screen_w = screen_w;
    out_clip->screen_h = screen_h;
    
    /* Copiar y_top/y_bottom del padre (por ahora pantalla completa) */
    /* TODO: Calcular y_top/y_bottom basado en alturas del portal */
    for (int x = x_min; x <= x_max; x++) {
        out_clip->y_top[x] = parent_clip->y_top[x];
        out_clip->y_bottom[x] = parent_clip->y_bottom[x];
    }
    
    return 1;
}

/**
 * Intersectar dos clip windows
 */
void ray_clip_window_intersect(RAY_ClipWindow* a, RAY_ClipWindow* b, RAY_ClipWindow* out)
{
    if (!a || !b || !out) return;
    
    /* Intersectar rangos horizontales */
    out->x_min = (a->x_min > b->x_min) ? a->x_min : b->x_min;
    out->x_max = (a->x_max < b->x_max) ? a->x_max : b->x_max;
    
    /* Intersectar límites verticales por columna */
    for (int x = out->x_min; x <= out->x_max; x++) {
        out->y_top[x] = (a->y_top[x] > b->y_top[x]) ? a->y_top[x] : b->y_top[x];
        out->y_bottom[x] = (a->y_bottom[x] < b->y_bottom[x]) ? a->y_bottom[x] : b->y_bottom[x];
    }
    
    out->screen_w = a->screen_w;
    out->screen_h = a->screen_h;
}
