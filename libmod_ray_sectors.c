#include "libmod_ray.h"
#include <stdlib.h>
#include <stdio.h>

/* ============================================================================
   FUNCIONES DE GESTIÓN DE SECTORES
   ============================================================================ */

/**
 * Buscar sector en una posición del grid
 */
RAY_Sector* ray_find_sector_at(RAY_Raycaster* rc, int grid_x, int grid_y)
{
    if (!rc || !rc->sectors) return NULL;
    
    /* Buscar desde el final (mayor prioridad) hacia el principio */
    for (int i = rc->num_sectors - 1; i >= 0; i--) {
        RAY_Sector* sector = &rc->sectors[i];
        
        if (grid_x >= sector->min_x && grid_x <= sector->max_x &&
            grid_y >= sector->min_y && grid_y <= sector->max_y) {
            return sector;
        }
    }
    
    return NULL;
}

/**
 * RAY_ADD_SECTOR(min_x, min_y, max_x, max_y)
 * Retorna el ID del sector creado
 */
int64_t libmod_ray_add_sector(INSTANCE *my, int64_t *params)
{
    if (!global_raycaster) return -1;
    
    int min_x = (int)params[0];
    int min_y = (int)params[1];
    int max_x = (int)params[2];
    int max_y = (int)params[3];
    
    /* Validar bounds */
    if (min_x < 0 || min_y < 0 || 
        max_x >= global_raycaster->grid_width || 
        max_y >= global_raycaster->grid_height ||
        min_x > max_x || min_y > max_y) {
        fprintf(stderr, "ERROR: Bounds de sector inválidos\n");
        return -1;
    }
    
    /* Expandir array si es necesario */
    if (global_raycaster->num_sectors >= global_raycaster->sectors_capacity) {
        int new_capacity = global_raycaster->sectors_capacity == 0 ? 
                          16 : global_raycaster->sectors_capacity * 2;
        
        RAY_Sector* new_sectors = (RAY_Sector*)realloc(
            global_raycaster->sectors, 
            sizeof(RAY_Sector) * new_capacity
        );
        
        if (!new_sectors) {
            fprintf(stderr, "ERROR: No se pudo expandir array de sectores\n");
            return -1;
        }
        
        global_raycaster->sectors = new_sectors;
        global_raycaster->sectors_capacity = new_capacity;
    }
    
    /* Crear nuevo sector con valores por defecto */
    RAY_Sector* sector = &global_raycaster->sectors[global_raycaster->num_sectors];
    sector->sector_id = global_raycaster->num_sectors;
    sector->min_x = min_x;
    sector->min_y = min_y;
    sector->max_x = max_x;
    sector->max_y = max_y;
    sector->floor_height = 0.0f;
    sector->ceiling_height = (float)global_raycaster->tile_size;
    sector->has_ceiling = 1;
    sector->floor_texture = 0;
    sector->ceiling_texture = 0;
    sector->light_level = 1.0f;
    
    global_raycaster->num_sectors++;
    
    return sector->sector_id;
}

/**
 * RAY_SET_SECTOR_FLOOR(sector_id, height, texture)
 */
int64_t libmod_ray_set_sector_floor(INSTANCE *my, int64_t *params)
{
    if (!global_raycaster) return 0;
    
    int sector_id = (int)params[0];
    float height = *(float*)&params[1];  /* CORREGIDO: leer como float, no double */
    int texture = (int)params[2];
    
    if (sector_id < 0 || sector_id >= global_raycaster->num_sectors) {
        return 0;
    }
    
    RAY_Sector* sector = &global_raycaster->sectors[sector_id];
    sector->floor_height = height;
    sector->floor_texture = texture;
    
    return 1;
}

/**
 * RAY_SET_SECTOR_CEILING(sector_id, height, texture, has_ceiling)
 */
int64_t libmod_ray_set_sector_ceiling(INSTANCE *my, int64_t *params)
{
    if (!global_raycaster) return 0;
    
    int sector_id = (int)params[0];
    float height = *(float*)&params[1];
    int texture = (int)params[2];
    int has_ceiling = (int)params[3];
    
    if (sector_id < 0 || sector_id >= global_raycaster->num_sectors) {
        return 0;
    }
    
    RAY_Sector* sector = &global_raycaster->sectors[sector_id];
    sector->ceiling_height = height;
    sector->ceiling_texture = texture;
    sector->has_ceiling = has_ceiling;
    
    return 1;
}

/**
 * RAY_GET_SECTOR_AT(x, y)
 * Retorna el ID del sector en esa posición, o -1 si no hay
 */
int64_t libmod_ray_get_sector_at(INSTANCE *my, int64_t *params)
{
    if (!global_raycaster) return -1;
    
    int x = (int)params[0];
    int y = (int)params[1];
    
    RAY_Sector* sector = ray_find_sector_at(global_raycaster, x, y);
    
    return sector ? sector->sector_id : -1;
}
