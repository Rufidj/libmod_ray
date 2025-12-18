#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
   FUNCIONES DE RAYCASTING
   Portadas desde raycasting.cpp de Andrew Lim
   Algoritmo DDA (Digital Differential Analysis)
   ============================================================================ */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define USE_FMOD 1

static float fmod2(float a, int b) {
#if USE_FMOD == 1
    return fmodf(a, (float)b);
#else
    return (int)a % b; /* Más rápido pero causa costuras en texturas */
#endif
}

/**
 * Crear un raycaster
 */
RAY_Raycaster* ray_create_raycaster(int grid_width, int grid_height, 
                                    int grid_count, int tile_size)
{
    RAY_Raycaster* rc = (RAY_Raycaster*)malloc(sizeof(RAY_Raycaster));
    if (!rc) return NULL;
    
    rc->grid_width = grid_width;
    rc->grid_height = grid_height;
    rc->grid_count = grid_count;
    rc->tile_size = tile_size;
    
    /* Inicializar array de sectores */
    rc->sectors = NULL;
    rc->num_sectors = 0;
    rc->sectors_capacity = 0;
    
    /* Crear array de grids */
    rc->grids = (GridCell**)malloc(sizeof(GridCell*) * grid_count);
    if (!rc->grids) {
        free(rc);
        return NULL;
    }
    
    /* Inicializar cada grid */
    int grid_size = grid_width * grid_height;
    for (int z = 0; z < grid_count; z++) {
        rc->grids[z] = (GridCell*)calloc(grid_size, sizeof(GridCell));
        if (!rc->grids[z]) {
            /* Limpiar grids ya creados */
            for (int i = 0; i < z; i++) {
                free(rc->grids[i]);
            }
            free(rc->grids);
            free(rc);
            return NULL;
        }
        
        /* Inicializar todas las celdas como vacías con forma sólida por defecto */
        for (int i = 0; i < grid_size; i++) {
            rc->grids[z][i].wall_type = 0;
            rc->grids[z][i].shape = TILE_SOLID;
            rc->grids[z][i].rotation = 0;
            rc->grids[z][i].flags = 0;
            rc->grids[z][i].reserved = 0;
        }
    }
    
    return rc;
}

/**
 * Destruir un raycaster
 */
void ray_destroy_raycaster(RAY_Raycaster* rc)
{
    if (!rc) return;
    
    if (rc->grids) {
        for (int z = 0; z < rc->grid_count; z++) {
            if (rc->grids[z]) {
                free(rc->grids[z]);
            }
        }
        free(rc->grids);
    }
    
    /* Liberar sectores */
    if (rc->sectors) {
        free(rc->sectors);
    }
    
    free(rc);
}

/**
 * Calcular distancia al plano de proyección
 * screenDistance = (screenWidth/2) / tan(fov/2)
 */
float ray_screen_distance(float screen_width, float fov_radians)
{
    return (screen_width / 2.0f) / tanf(fov_radians / 2.0f);
}

/**
 * Calcular ángulo de una columna de pantalla
 * a = atan(screenX / screenDistance)
 */
float ray_strip_angle(float screen_x, float screen_distance)
{
    return atanf(screen_x / screen_distance);
}

/**
 * Calcular altura en pantalla de una columna de pared
 * Principio de triángulos similares
 */
float ray_strip_screen_height(float screen_distance, float correct_distance, 
                              float tile_size)
{
    return floorf(screen_distance / correct_distance * tile_size + 0.5f);
}

/**
 * Verificar si hay espacio vacío debajo de un bloque
 */
static int ray_any_space_below(RAY_Raycaster* rc, int x, int y, int z)
{
    if (z == 0) {
        return 0;
    }
    
    for (int level = z - 1; level >= 0; level--) {
        int grid_offset = x + y * rc->grid_width;
        if (rc->grids[level][grid_offset].wall_type == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * Verificar si hay espacio vacío encima de un bloque
 */
static int ray_any_space_above(RAY_Raycaster* rc, int x, int y, int z)
{
    for (int level = z + 1; level < rc->grid_count; level++) {
        int grid_offset = x + y * rc->grid_width;
        if (rc->grids[level][grid_offset].wall_type == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * Verificar si se necesita renderizar la siguiente pared
 */
static int ray_needs_next_wall(RAY_Raycaster* rc, float player_z, 
                               int x, int y, int z)
{
    float eye_height = rc->tile_size / 2.0f + player_z;
    float wall_bottom = z * rc->tile_size;
    float wall_top = wall_bottom + rc->tile_size;
    int eye_above_wall = eye_height > wall_top;
    int eye_below_wall = eye_height < wall_bottom;
    
    if (eye_above_wall) {
        return ray_any_space_above(rc, x, y, z);
    }
    if (eye_below_wall) {
        return ray_any_space_below(rc, x, y, z);
    }
    return 0;
}

/**
 * Algoritmo principal de raycasting DDA
 */
void ray_raycast(RAY_Raycaster* rc, RAY_RayHit* hits, int* num_hits,
                float player_x, float player_y, float player_z,
                float player_rot, float strip_angle, int strip_idx,
                RAY_Sprite* sprites, int num_sprites)
{
    if (!rc || !rc->grids || rc->grid_count == 0) {
        *num_hits = 0;
        return;
    }
    
    *num_hits = 0;
    int tile_size = rc->tile_size;
    int grid_width = rc->grid_width;
    int grid_height = rc->grid_height;
    
    float ray_angle = strip_angle + player_rot;
    const float TWO_PI = M_PI * 2.0f;
    
    /* Normalizar ángulo */
    while (ray_angle < 0) ray_angle += TWO_PI;
    while (ray_angle >= TWO_PI) ray_angle -= TWO_PI;
    
    /* Determinar cuadrante */
    int right = (ray_angle < TWO_PI * 0.25f && ray_angle >= 0) || 
                (ray_angle > TWO_PI * 0.75f);
    int up = ray_angle < TWO_PI * 0.5f && ray_angle >= 0;
    
    int current_tile_x = (int)(player_x / tile_size);
    int current_tile_y = (int)(player_y / tile_size);
    
    /* Iterar por cada nivel del mapa */
    for (int level = 0; level < rc->grid_count; level++) {
        GridCell* grid = rc->grids[level];
        
        /* ====================================================================
           VERIFICACIÓN DE LÍNEAS VERTICALES (paredes este-oeste)
           ==================================================================== */
        float vertical_line_distance = 0;
        RAY_RayHit vertical_wall_hit;
        int found_vertical = 0;
        
        /* Encontrar coordenada X de líneas verticales */
        float vx = 0;
        if (right) {
            vx = floorf(player_x / tile_size) * tile_size + tile_size;
        } else {
            vx = floorf(player_x / tile_size) * tile_size - 1;
        }
        
        /* Calcular coordenada Y */
        float vy = player_y + (player_x - vx) * tanf(ray_angle);
        
        /* Calcular vector de paso */
        float stepx = right ? tile_size : -tile_size;
        float stepy = tile_size * tanf(ray_angle);
        
        if (right) {
            stepy = -stepy;
        }
        
        /* Recorrer líneas verticales */
        while (vx >= 0 && vx < grid_width * tile_size && 
               vy >= 0 && vy < grid_height * tile_size) {
            int wall_y = (int)(vy / tile_size);
            int wall_x = (int)(vx / tile_size);
            int wall_offset = wall_x + wall_y * grid_width;
            
            /* Verificar si hay pared */
            if (wall_x >= 0 && wall_x < rc->grid_width &&
                wall_y >= 0 && wall_y < rc->grid_height) {
                
                int wall_type = grid[wall_offset].wall_type;
                
                if (wall_type > 0) {
                    float dist_x = player_x - vx;
                    float dist_y = player_y - vy;
                    float block_dist = dist_x * dist_x + dist_y * dist_y;
                    
                    /* Si la distancia horizontal es menor, parar */
                    if (block_dist > 0) {
                        float tex_x = fmod2(vy, tile_size);
                        tex_x = right ? tex_x : tile_size - tex_x;
                        
                        RAY_RayHit ray_hit;
                        memset(&ray_hit, 0, sizeof(RAY_RayHit));
                        ray_hit.x = vx;
                        ray_hit.y = vy;
                        ray_hit.ray_angle = ray_angle;
                        ray_hit.strip = strip_idx;
                        ray_hit.wall_type = wall_type;
                        ray_hit.wall_x = wall_x;
                        ray_hit.wall_y = wall_y;
                        ray_hit.level = level;
                        ray_hit.up = up;
                        ray_hit.right = right;
                        ray_hit.distance = sqrtf(block_dist);
                        ray_hit.correct_distance = ray_hit.distance * cosf(strip_angle);
                        ray_hit.horizontal = 0;
                        ray_hit.tile_x = tex_x;
                        ray_hit.wall_height = tile_size;
                        
                        int gaps = ray_needs_next_wall(rc, player_z, wall_x, wall_y, level);
                        
                        if (!gaps) {
                            vertical_wall_hit = ray_hit;
                            vertical_line_distance = block_dist;
                            found_vertical = 1;
                            break;
                        }
                        
                        if (*num_hits < 256) {
                            hits[(*num_hits)++] = ray_hit;
                        }
                    }
                }
            }
            
            vx += stepx;
            vy += stepy;
        }
        
        /* ====================================================================
           VERIFICACIÓN DE LÍNEAS HORIZONTALES (paredes norte-sur)
           ==================================================================== */
        
        /* Encontrar coordenada Y de líneas horizontales */
        float hy = 0;
        if (up) {
            hy = floorf(player_y / tile_size) * tile_size - 1;
        } else {
            hy = floorf(player_y / tile_size) * tile_size + tile_size;
        }
        
        /* Calcular coordenada X */
        float hx = player_x + (player_y - hy) / tanf(ray_angle);
        stepy = up ? -tile_size : tile_size;
        stepx = tile_size / tanf(ray_angle);
        
        if (!up) {
            stepx = -stepx;
        }
        
        /* Recorrer líneas horizontales */
        while (hx >= 0 && hx < grid_width * tile_size && 
               hy >= 0 && hy < grid_height * tile_size) {
            int wall_y = (int)(hy / tile_size);
            int wall_x = (int)(hx / tile_size);
            int wall_offset = wall_x + wall_y * grid_width;
            
            /* Verificar si la celda actual es una pared */
            if (grid[wall_offset].wall_type > 0) {
                float dist_x = player_x - hx;
                float dist_y = player_y - hy;
                float block_dist = dist_x * dist_x + dist_y * dist_y;
                
                /* Si la distancia vertical es menor, parar */
                if (vertical_line_distance > 0 && vertical_line_distance < block_dist) {
                    break;
                }
                
                if (block_dist > 0) {
                    float tex_x = fmod2(hx, tile_size);
                    tex_x = up ? tex_x : tile_size - tex_x;
                    
                    RAY_RayHit ray_hit;
                    memset(&ray_hit, 0, sizeof(RAY_RayHit));
                    ray_hit.x = hx;
                    ray_hit.y = hy;
                    ray_hit.ray_angle = ray_angle;
                    ray_hit.strip = strip_idx;
                    ray_hit.wall_type = grid[wall_offset].wall_type;
                    ray_hit.wall_x = wall_x;
                    ray_hit.wall_y = wall_y;
                    ray_hit.level = level;
                    ray_hit.up = up;
                    ray_hit.right = right;
                    ray_hit.distance = sqrtf(block_dist);
                    ray_hit.correct_distance = ray_hit.distance * cosf(strip_angle);
                    ray_hit.horizontal = 1;
                    ray_hit.tile_x = tex_x;
                    ray_hit.wall_height = tile_size;
                    
                    int gaps = ray_needs_next_wall(rc, player_z, wall_x, wall_y, level);
                    
                    if (!gaps) {
                        if (*num_hits < 256) {
                            hits[(*num_hits)++] = ray_hit;
                        }
                        break;
                    }
                    
                    if (*num_hits < 256) {
                        hits[(*num_hits)++] = ray_hit;
                    }
                }
            }
            
            hx += stepx;
            hy += stepy;
        }
        
        /* Agregar hit vertical si se encontró */
        if (found_vertical && *num_hits < 256) {
            hits[(*num_hits)++] = vertical_wall_hit;
        }
    }
}

/**
 * Raycasting para ThinWalls
 */
void ray_find_intersecting_thin_walls(RAY_RayHit* hits, int* num_hits,
                                     RAY_ThinWall* thin_walls, int num_thin_walls,
                                     float player_x, float player_y,
                                     float ray_end_x, float ray_end_y)
{
    for (int i = 0; i < num_thin_walls; i++) {
        RAY_ThinWall* tw = &thin_walls[i];
        if (tw->hidden) continue;
        
        float ix, iy;
        if (ray_lines_intersect(player_x, player_y, ray_end_x, ray_end_y,
                               tw->x1, tw->y1, tw->x2, tw->y2,
                               &ix, &iy)) {
            float dist_x = player_x - ix;
            float dist_y = player_y - iy;
            float distance = sqrtf(dist_x * dist_x + dist_y * dist_y);
            
            if (*num_hits < 256) {
                RAY_RayHit ray_hit;
                memset(&ray_hit, 0, sizeof(RAY_RayHit));
                ray_hit.x = ix;
                ray_hit.y = iy;
                ray_hit.distance = distance;
                ray_hit.thin_wall = tw;
                ray_hit.wall_type = tw->wall_type;
                ray_hit.wall_height = tw->height;
                hits[(*num_hits)++] = ray_hit;
            }
        }
    }
}

void ray_raycast_thin_walls(RAY_RayHit* hits, int* num_hits,
                            RAY_ThinWall* thin_walls, int num_thin_walls,
                            float player_x, float player_y, float player_z,
                            float player_rot, float strip_angle, int strip_idx)
{
    float ray_angle = strip_angle + player_rot;
    
    /* Calcular punto final del rayo (muy lejos) */
    float ray_length = 10000.0f;
    float ray_end_x = player_x + cosf(ray_angle) * ray_length;
    float ray_end_y = player_y + sinf(ray_angle) * ray_length;
    
    ray_find_intersecting_thin_walls(hits, num_hits, thin_walls, num_thin_walls,
                                    player_x, player_y, ray_end_x, ray_end_y);
}
