#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
   GESTIÓN DE PORTALES
   ============================================================================ */

/**
 * Agregar un portal entre dos sectores
 * 
 * @param rc Raycaster
 * @param from_sector ID del sector origen
 * @param to_sector ID del sector destino
 * @param x1, y1 Punto inicial del portal en coordenadas de mundo
 * @param x2, y2 Punto final del portal en coordenadas de mundo
 * @param bottom_z Altura inferior del portal
 * @param top_z Altura superior del portal
 * @param bidirectional 1 = se puede ver en ambas direcciones
 * @return ID del portal creado, o -1 si error
 */
int ray_add_portal(RAY_Raycaster* rc,
                   int from_sector, int to_sector,
                   float x1, float y1, float x2, float y2,
                   float bottom_z, float top_z,
                   int bidirectional)
{
    if (!rc) return -1;
    
    /* Validar sectores */
    if (from_sector < 0 || from_sector >= rc->num_sectors) return -1;
    if (to_sector < 0 || to_sector >= rc->num_sectors) return -1;
    if (from_sector == to_sector) return -1;
    
    /* Expandir array si es necesario */
    if (rc->num_portals >= rc->portals_capacity) {
        int new_capacity = rc->portals_capacity == 0 ? 16 : rc->portals_capacity * 2;
        RAY_Portal* new_portals = (RAY_Portal*)realloc(rc->portals, 
                                                        sizeof(RAY_Portal) * new_capacity);
        if (!new_portals) return -1;
        
        rc->portals = new_portals;
        rc->portals_capacity = new_capacity;
    }
    
    /* Crear portal */
    int portal_id = rc->num_portals;
    RAY_Portal* portal = &rc->portals[portal_id];
    
    portal->portal_id = portal_id;
    portal->from_sector = from_sector;
    portal->to_sector = to_sector;
    portal->x1 = x1;
    portal->y1 = y1;
    portal->x2 = x2;
    portal->y2 = y2;
    portal->bottom_z = bottom_z;
    portal->top_z = top_z;
    portal->bidirectional = bidirectional;
    portal->enabled = 1;
    
    rc->num_portals++;
    
    /* Agregar portal al sector origen */
    RAY_Sector* sector = &rc->sectors[from_sector];
    
    if (sector->num_portals >= sector->portals_capacity) {
        int new_cap = sector->portals_capacity == 0 ? 4 : sector->portals_capacity * 2;
        int32_t* new_ids = (int32_t*)realloc(sector->portal_ids, sizeof(int32_t) * new_cap);
        if (!new_ids) return -1;
        
        sector->portal_ids = new_ids;
        sector->portals_capacity = new_cap;
    }
    
    sector->portal_ids[sector->num_portals++] = portal_id;
    
    /* Si es bidireccional, agregar también al sector destino */
    if (bidirectional) {
        RAY_Sector* to_sec = &rc->sectors[to_sector];
        
        if (to_sec->num_portals >= to_sec->portals_capacity) {
            int new_cap = to_sec->portals_capacity == 0 ? 4 : to_sec->portals_capacity * 2;
            int32_t* new_ids = (int32_t*)realloc(to_sec->portal_ids, sizeof(int32_t) * new_cap);
            if (!new_ids) return -1;
            
            to_sec->portal_ids = new_ids;
            to_sec->portals_capacity = new_cap;
        }
        
        to_sec->portal_ids[to_sec->num_portals++] = portal_id;
    }
    
    return portal_id;
}

/**
 * Eliminar un portal
 */
void ray_remove_portal(RAY_Raycaster* rc, int portal_id)
{
    if (!rc || portal_id < 0 || portal_id >= rc->num_portals) return;
    
    RAY_Portal* portal = &rc->portals[portal_id];
    portal->enabled = 0;
    
    /* TODO: Eliminar de arrays de sectores */
}

/**
 * Habilitar/deshabilitar un portal
 */
void ray_enable_portal(RAY_Raycaster* rc, int portal_id, int enabled)
{
    if (!rc || portal_id < 0 || portal_id >= rc->num_portals) return;
    
    rc->portals[portal_id].enabled = enabled ? 1 : 0;
}

/**
 * Obtener un portal por ID
 */
RAY_Portal* ray_get_portal(RAY_Raycaster* rc, int portal_id)
{
    if (!rc || portal_id < 0 || portal_id >= rc->num_portals) return NULL;
    
    return &rc->portals[portal_id];
}

/* ============================================================================
   GESTIÓN DE CLIP WINDOWS
   ============================================================================ */

/**
 * Crear una ventana de clipping
 */
RAY_ClipWindow* ray_create_clip_window(int screen_w, int screen_h)
{
    RAY_ClipWindow* window = (RAY_ClipWindow*)malloc(sizeof(RAY_ClipWindow));
    if (!window) return NULL;
    
    window->screen_w = screen_w;
    window->screen_h = screen_h;
    window->x_min = 0;
    window->x_max = screen_w - 1;
    
    window->y_top = (float*)malloc(sizeof(float) * screen_w);
    window->y_bottom = (float*)malloc(sizeof(float) * screen_w);
    
    if (!window->y_top || !window->y_bottom) {
        ray_destroy_clip_window(window);
        return NULL;
    }
    
    /* Inicializar con pantalla completa */
    for (int i = 0; i < screen_w; i++) {
        window->y_top[i] = 0.0f;
        window->y_bottom[i] = (float)screen_h;
    }
    
    return window;
}

/**
 * Destruir una ventana de clipping
 */
void ray_destroy_clip_window(RAY_ClipWindow* window)
{
    if (!window) return;
    
    if (window->y_top) free(window->y_top);
    if (window->y_bottom) free(window->y_bottom);
    free(window);
}

/**
 * Resetear ventana de clipping a pantalla completa
 */
void ray_reset_clip_window(RAY_ClipWindow* window)
{
    if (!window) return;
    
    window->x_min = 0;
    window->x_max = window->screen_w - 1;
    
    for (int i = 0; i < window->screen_w; i++) {
        window->y_top[i] = 0.0f;
        window->y_bottom[i] = (float)window->screen_h;
    }
}

/* ============================================================================
   DETECCIÓN AUTOMÁTICA DE PORTALES
   ============================================================================ */

/**
 * Detectar y crear portales automáticamente entre sectores adyacentes
 * Se llama automáticamente al cargar un mapa
 */
void ray_detect_portals_automatic(RAY_Raycaster* rc)
{
    if (!rc || rc->num_sectors < 2) return;
    
    int tile_size = rc->tile_size;
    
    /* Para cada par de sectores */
    for (int i = 0; i < rc->num_sectors; i++) {
        for (int j = i + 1; j < rc->num_sectors; j++) {
            RAY_Sector* s1 = &rc->sectors[i];
            RAY_Sector* s2 = &rc->sectors[j];
            
            /* Verificar borde derecho de s1 con borde izquierdo de s2 */
            if (s1->max_x + 1 == s2->min_x) {
                /* Calcular overlap en Y */
                int y_start = (s1->min_y > s2->min_y) ? s1->min_y : s2->min_y;
                int y_end = (s1->max_y < s2->max_y) ? s1->max_y : s2->max_y;
                
                if (y_end >= y_start) {
                    /* Hay overlap → crear portal vertical */
                    float x = (float)(s1->max_x + 1) * tile_size;
                    float y1 = (float)y_start * tile_size;
                    float y2 = (float)(y_end + 1) * tile_size;
                    
                    ray_add_portal(rc, i, j,
                                  x, y1, x, y2,
                                  0.0f, (float)tile_size, 1);
                }
            }
            
            /* Verificar borde izquierdo de s1 con borde derecho de s2 */
            if (s1->min_x == s2->max_x + 1) {
                int y_start = (s1->min_y > s2->min_y) ? s1->min_y : s2->min_y;
                int y_end = (s1->max_y < s2->max_y) ? s1->max_y : s2->max_y;
                
                if (y_end >= y_start) {
                    float x = (float)s1->min_x * tile_size;
                    float y1 = (float)y_start * tile_size;
                    float y2 = (float)(y_end + 1) * tile_size;
                    
                    ray_add_portal(rc, i, j,
                                  x, y1, x, y2,
                                  0.0f, (float)tile_size, 1);
                }
            }
            
            /* Verificar borde inferior de s1 con borde superior de s2 */
            if (s1->max_y + 1 == s2->min_y) {
                int x_start = (s1->min_x > s2->min_x) ? s1->min_x : s2->min_x;
                int x_end = (s1->max_x < s2->max_x) ? s1->max_x : s2->max_x;
                
                if (x_end >= x_start) {
                    /* Hay overlap → crear portal horizontal */
                    float y = (float)(s1->max_y + 1) * tile_size;
                    float x1 = (float)x_start * tile_size;
                    float x2 = (float)(x_end + 1) * tile_size;
                    
                    ray_add_portal(rc, i, j,
                                  x1, y, x2, y,
                                  0.0f, (float)tile_size, 1);
                }
            }
            
            /* Verificar borde superior de s1 con borde inferior de s2 */
            if (s1->min_y == s2->max_y + 1) {
                int x_start = (s1->min_x > s2->min_x) ? s1->min_x : s2->min_x;
                int x_end = (s1->max_x < s2->max_x) ? s1->max_x : s2->max_x;
                
                if (x_end >= x_start) {
                    float y = (float)s1->min_y * tile_size;
                    float x1 = (float)x_start * tile_size;
                    float x2 = (float)(x_end + 1) * tile_size;
                    
                    ray_add_portal(rc, i, j,
                                  x1, y, x2, y,
                                  0.0f, (float)tile_size, 1);
                }
            }
        }
    }
}
