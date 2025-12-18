#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
   RENDERIZADO RECURSIVO DE PORTALES
   ============================================================================ */

/* Stack para prevenir recursión infinita */
#define MAX_RECURSION_DEPTH 8
static int recursion_stack[MAX_RECURSION_DEPTH];
static int recursion_depth = 0;

/**
 * Encontrar el sector donde está la cámara
 */
int ray_find_camera_sector(RAY_Raycaster* rc, float cam_x, float cam_y)
{
    if (!rc || rc->num_sectors == 0) return -1;
    
    int tile_x = (int)(cam_x / rc->tile_size);
    int tile_y = (int)(cam_y / rc->tile_size);
    
    /* Buscar sector que contenga esta posición */
    for (int i = 0; i < rc->num_sectors; i++) {
        RAY_Sector* sector = &rc->sectors[i];
        if (tile_x >= sector->min_x && tile_x <= sector->max_x &&
            tile_y >= sector->min_y && tile_y <= sector->max_y) {
            return i;
        }
    }
    
    return -1;
}

/**
 * Verificar si un sector ya está en el stack de recursión
 */
static int is_sector_in_stack(int sector_id)
{
    for (int i = 0; i < recursion_depth; i++) {
        if (recursion_stack[i] == sector_id) {
            return 1;
        }
    }
    return 0;
}

/**
 * Renderizar un sector recursivamente con portales
 * 
 * Esta es una versión simplificada que se integra con el sistema existente.
 * Por ahora solo renderiza el sector actual y detecta portales visibles.
 */
void ray_render_sector_recursive(RAY_Raycaster* rc, GRAPH* render_buffer,
                                 int sector_id,
                                 float cam_x, float cam_y, float cam_z,
                                 float cam_angle, float cam_pitch, float fov,
                                 int screen_w, int screen_h,
                                 RAY_ClipWindow* clip_window)
{
    /* Prevenir recursión infinita */
    if (recursion_depth >= MAX_RECURSION_DEPTH) return;
    if (sector_id < 0 || sector_id >= rc->num_sectors) return;
    if (is_sector_in_stack(sector_id)) return;
    
    /* Agregar sector al stack */
    recursion_stack[recursion_depth++] = sector_id;
    
    RAY_Sector* sector = &rc->sectors[sector_id];
    
    /* TODO: Aquí iría el renderizado del sector actual
     * Por ahora, el renderizado principal se hace en ray_render_frame
     * Esta función solo maneja la recursión de portales
     */
    
    /* Calcular distancia al plano de proyección */
    float screen_distance = ray_screen_distance((float)screen_w, fov * 3.14159f / 180.0f);
    
    /* Renderizar portales de este sector */
    for (int i = 0; i < sector->num_portals; i++) {
        int portal_id = sector->portal_ids[i];
        if (portal_id < 0 || portal_id >= rc->num_portals) continue;
        
        RAY_Portal* portal = &rc->portals[portal_id];
        if (!portal->enabled) continue;
        
        /* Determinar sector destino */
        int dest_sector = (portal->from_sector == sector_id) ? 
                         portal->to_sector : portal->from_sector;
        
        /* Proyectar portal a pantalla */
        RAY_ClipWindow* portal_clip = ray_create_clip_window(screen_w, screen_h);
        if (!portal_clip) continue;
        
        int visible = ray_project_portal(portal, cam_x, cam_y, cam_angle,
                                        screen_distance, screen_w, screen_h,
                                        clip_window, portal_clip);
        
        if (visible) {
            /* Renderizar sector destino recursivamente */
            ray_render_sector_recursive(rc, render_buffer, dest_sector,
                                       cam_x, cam_y, cam_z,
                                       cam_angle, cam_pitch, fov,
                                       screen_w, screen_h,
                                       portal_clip);
        }
        
        ray_destroy_clip_window(portal_clip);
    }
    
    /* Remover sector del stack */
    recursion_depth--;
}

/**
 * Iniciar renderizado con portales
 * Wrapper que inicializa el sistema y llama al renderizado recursivo
 */
void ray_render_with_portals(RAY_Raycaster* rc, GRAPH* render_buffer,
                             float cam_x, float cam_y, float cam_z,
                             float cam_angle, float cam_pitch, float fov,
                             int screen_w, int screen_h)
{
    if (!rc || !render_buffer) return;
    
    /* Resetear stack de recursión */
    recursion_depth = 0;
    
    /* Encontrar sector inicial (donde está la cámara) */
    int start_sector = ray_find_camera_sector(rc, cam_x, cam_y);
    
    /* Si no hay sectores o no encontramos el sector, usar renderizado normal */
    if (start_sector < 0 || rc->num_sectors == 0) {
        /* Renderizado normal sin portales */
        return;
    }
    
    /* Crear clip window inicial (pantalla completa) */
    RAY_ClipWindow* initial_clip = ray_create_clip_window(screen_w, screen_h);
    if (!initial_clip) return;
    
    /* Renderizar recursivamente desde el sector inicial */
    ray_render_sector_recursive(rc, render_buffer, start_sector,
                                cam_x, cam_y, cam_z,
                                cam_angle, cam_pitch, fov,
                                screen_w, screen_h,
                                initial_clip);
    
    ray_destroy_clip_window(initial_clip);
}
