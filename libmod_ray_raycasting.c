/*
 * libmod_ray_raycasting.c - Core Raycasting Functions
 * Port of raycasting.cpp from Andrew Lim's raycasting engine
 */

#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
   RAYCASTER - GRID MANAGEMENT
   ============================================================================ */

void ray_raycaster_create_grids(RAY_Raycaster *rc, int width, int height, int count, int tileSize)
{
    rc->gridWidth = width;
    rc->gridHeight = height;
    rc->gridCount = count;
    rc->tileSize = tileSize;
    
    rc->grids = (int**)malloc(count * sizeof(int*));
    for (int z = 0; z < count; z++) {
        rc->grids[z] = (int*)calloc(width * height, sizeof(int));
    }
}

/* ============================================================================
   RAYCASTER - UTILITY FUNCTIONS
   ============================================================================ */

/* Port of Raycaster::screenDistance() */
float ray_screen_distance(float screenWidth, float fovRadians)
{
    return (screenWidth / 2.0f) / tanf(fovRadians / 2.0f);
}

/* Port of Raycaster::stripAngle() */
float ray_strip_angle(float screenX, float screenDistance)
{
    return atanf(screenX / screenDistance);
}

/* Port of Raycaster::stripScreenHeight() */
float ray_strip_screen_height(float screenDistance, float correctDistance, float tileSize)
{
    return floorf(screenDistance / correctDistance * tileSize + 0.5f);
}

/* ============================================================================
   RAYCASTER - HELPER FUNCTIONS
   ============================================================================ */

static int any_space_below(RAY_Raycaster *rc, int x, int y, int z)
{
    if (z == 0) {
        int *grid = rc->grids[0];
        if (x >= 0 && y >= 0) {
            if (ray_is_door(grid[x + y * rc->gridWidth])) {
                return 1;
            }
        }
    }
    
    for (int level = z - 1; level >= 0; level--) {
        int *grid = rc->grids[level];
        int gridOffset = x + y * rc->gridWidth;
        if (grid[gridOffset] == 0 || ray_is_door(grid[gridOffset])) {
            return 1;
        }
    }
    return 0;
}

static int any_space_above(RAY_Raycaster *rc, int x, int y, int z)
{
    /* MODIFICADO: Siempre retornar 1 para permitir renderizado multi-nivel */
    /* Esto permite ver estructuras flotantes y torres en niveles superiores */
    /* incluso si hay paredes en niveles inferiores */
    return 1;
    
    /* Código original comentado - implementaba oclusión estricta
    if (z == 0) {
        int *grid = rc->grids[0];
        if (x >= 0 && y >= 0) {
            if (ray_is_door(grid[x + y * rc->gridWidth])) {
                return 1;
            }
        }
    }
    
    for (int level = z + 1; level < rc->gridCount; level++) {
        int *grid = rc->grids[level];
        int gridOffset = x + y * rc->gridWidth;
        if (grid[gridOffset] == 0 || ray_is_door(grid[gridOffset])) {
            return 1;
        }
    }
    return 0;
    */
}

static int needs_next_wall(RAY_Raycaster *rc, float playerZ, int x, int y, int z)
{
    /* Siempre permitir puertas */
    if (z == 0) {
        int *grid = rc->grids[0];
        if (x >= 0 && y >= 0) {
            if (ray_is_door(grid[x + y * rc->gridWidth])) {
                return 1;
            }
        }
    }
    
    /* Calcular altura del ojo y límites del nivel */
    float eyeHeight = rc->tileSize / 2.0f + playerZ;
    float wallBottom = z * rc->tileSize;
    float wallTop = wallBottom + rc->tileSize;
    
    /* Si el ojo está dentro del nivel actual, no buscar más niveles
     * Esto hace que desde dentro de una habitación solo veas ese nivel */
    if (eyeHeight >= wallBottom && eyeHeight <= wallTop) {
        return 0;  // Estamos dentro de este nivel, no buscar más
    }
    
    /* Si el ojo está por encima o por debajo, continuar buscando
     * Esto permite ver edificios completos desde fuera */
    int eyeAboveWall = eyeHeight > wallTop;
    int eyeBelowWall = eyeHeight < wallBottom;
    
    if (eyeAboveWall) {
        return any_space_above(rc, x, y, z);
    }
    if (eyeBelowWall) {
        return any_space_below(rc, x, y, z);
    }
    
    return 0;
}

/* Usar fmod para texturas (compatible con motor original) */
static float fmod2(float a, int b)
{
    return fmodf(a, (float)b);
}

/* ============================================================================
   RAYCASTER - MAIN RAYCAST FUNCTION
   ============================================================================ */

void ray_raycaster_raycast(RAY_Raycaster *rc, RAY_RayHit *hits, int *num_hits,
                           int playerX, int playerY, float playerZ,
                           float playerRot, float stripAngle, int stripIdx,
                           RAY_Sprite *sprites, int num_sprites)
{
    if (!rc->grids || rc->gridCount == 0) {
        *num_hits = 0;
        return;
    }
    
    int hit_count = 0;
    const int max_hits = RAY_MAX_RAYHITS;
    
    /* Calcular ángulo del rayo */
    float rayAngle = stripAngle + playerRot;
    while (rayAngle < 0) rayAngle += RAY_TWO_PI;
    while (rayAngle >= RAY_TWO_PI) rayAngle -= RAY_TWO_PI;
    
    /* Determinar cuadrante */
    int right = (rayAngle < RAY_TWO_PI * 0.25f && rayAngle >= 0) ||
                (rayAngle > RAY_TWO_PI * 0.75f);
    int up = rayAngle < RAY_TWO_PI * 0.5f && rayAngle >= 0;
    
    int *groundGrid = rc->grids[0];
    int currentTileX = playerX / rc->tileSize;
    int currentTileY = playerY / rc->tileSize;
    
    /* Iterar por cada nivel del grid */
    for (int level = 0; level < rc->gridCount; level++) {
        int *grid = rc->grids[level];
        
        /* ========================================
           VERTICAL LINES CHECKING
           ======================================== */
        
        /* Find x coordinate of vertical lines on the right and left */
        float vx = 0;
        if (right) {
            vx = floorf(playerX / rc->tileSize) * rc->tileSize + rc->tileSize;
        } else {
            vx = floorf(playerX / rc->tileSize) * rc->tileSize - 1;
        }
        
        /* Calculate y coordinate of those lines */
        float vy = playerY + (playerX - vx) * tanf(rayAngle);
        
        /* Calculate stepping vector for each line */
        float stepx = right ? rc->tileSize : -rc->tileSize;
        float stepy = rc->tileSize * tanf(rayAngle);
        
        /* tan() returns positive values in Quadrant 1 and Quadrant 4 but window
         * coordinates need negative coordinates for Y-axis so we reverse */
        if (right) {
            stepy = -stepy;
        }
        
        /* Recorrer líneas verticales */
        /* NOTA: No verificamos límites en el while para permitir detectar muros en los bordes */
        for (int step = 0; step < 100; step++) /* Máximo 100 pasos para evitar loops infinitos */
        {
            int wallY = (int)floorf(vy / rc->tileSize);
            int wallX = (int)floorf(vx / rc->tileSize);
            
            /* Verificar que wallX y wallY estén dentro de los límites del grid */
            if (wallX < 0 || wallX >= rc->gridWidth || 
                wallY < 0 || wallY >= rc->gridHeight) {
                break; /* Salir del loop si estamos fuera del mapa */
            }
            
            int wallOffset = wallX + wallY * rc->gridWidth;
            
            
            /* Check if current cell is a wall (treat doors like normal walls) */
            if (grid[wallOffset] > 0) {
                
                float distX = playerX - vx;
                float distY = playerY - vy;
                float blockDist = distX * distX + distY * distY;
                
                if (blockDist > 0 && hit_count < max_hits) {
                    float texX = fmod2(vy, rc->tileSize);
                    texX = right ? texX : rc->tileSize - texX;
                    
                    RAY_RayHit *rayHit = &hits[hit_count];
                    rayHit->x = vx;
                    rayHit->y = vy;
                    rayHit->rayAngle = rayAngle;
                    rayHit->strip = stripIdx;
                    rayHit->wallType = grid[wallOffset];
                    rayHit->wallX = wallX;
                    rayHit->wallY = wallY;
                    rayHit->level = level;
                    rayHit->up = up;
                    rayHit->right = right;
                    rayHit->distance = sqrtf(blockDist);
                    rayHit->sortdistance = rayHit->distance;
                    rayHit->sprite = NULL;
                    rayHit->thinWall = NULL;
                    rayHit->wallHeight = rc->tileSize;
                    
                    rayHit->correctDistance = rayHit->distance * cosf(stripAngle);
                    rayHit->horizontal = 0;
                    rayHit->tileX = texX;
                    
                    hit_count++;
                    
                    int gaps = needs_next_wall(rc, playerZ, wallX, wallY, level);
                    if (!gaps) {
                        break;
                    }
                }
            }
            
            vx += stepx;
            vy += stepy;
        }
        
        /* ========================================
           HORIZONTAL LINES CHECKING
           ======================================== */
        
        /* Encontrar coordenada Y de líneas horizontales */
        float hy = 0;
        if (up) {
            hy = floorf(playerY / rc->tileSize) * rc->tileSize - 1;
        } else {
            hy = floorf(playerY / rc->tileSize) * rc->tileSize + rc->tileSize;
        }
        
        /* Calcular coordenada X */
        float hx = playerX + (playerY - hy) / tanf(rayAngle);
        stepy = up ? -rc->tileSize : rc->tileSize;
        stepx = rc->tileSize / tanf(rayAngle);
        
        if (!up) {
            stepx = -stepx;
        }
        
        /* Recorrer líneas horizontales */
        /* NOTA: No verificamos límites en el while para permitir detectar muros en los bordes */
        for (int step = 0; step < 100; step++) /* Máximo 100 pasos para evitar loops infinitos */
        {
            int wallY = (int)floorf(hy / rc->tileSize);
            int wallX = (int)floorf(hx / rc->tileSize);
            
            /* Verificar que wallX y wallY estén dentro de los límites del grid */
            if (wallX < 0 || wallX >= rc->gridWidth || 
                wallY < 0 || wallY >= rc->gridHeight) {
                break; /* Salir del loop si estamos fuera del mapa */
            }
            
            int wallOffset = wallX + wallY * rc->gridWidth;
            
            
            /* Check if current cell is a wall (treat doors like normal walls) */
            if (grid[wallOffset] > 0) {
                
                float distX = playerX - hx;
                float distY = playerY - hy;
                float blockDist = distX * distX + distY * distY;
                
                if (blockDist > 0 && hit_count < max_hits) {
                    float texX = fmod2(hx, rc->tileSize);
                    texX = up ? rc->tileSize - texX : texX;
                    
                    RAY_RayHit *rayHit = &hits[hit_count];
                    rayHit->x = hx;
                    rayHit->y = hy;
                    rayHit->rayAngle = rayAngle;
                    rayHit->strip = stripIdx;
                    rayHit->wallType = grid[wallOffset];
                    rayHit->wallX = wallX;
                    rayHit->wallY = wallY;
                    rayHit->level = level;
                    rayHit->up = up;
                    rayHit->right = right;
                    rayHit->distance = sqrtf(blockDist);
                    rayHit->sortdistance = rayHit->distance;
                    rayHit->sprite = NULL;
                    rayHit->thinWall = NULL;
                    rayHit->wallHeight = rc->tileSize;
                    
                    rayHit->correctDistance = rayHit->distance * cosf(stripAngle);
                    rayHit->horizontal = 1;
                    rayHit->tileX = texX;
                    
                    hit_count++;
                    
                    int gaps = needs_next_wall(rc, playerZ, wallX, wallY, level);
                    if (!gaps) {
                        break;
                    }
                }
            }
            
            hx += stepx;
            hy += stepy;
        }
    }
    
    
    
    /* ========================================
       DEDUPLICACIÓN DE HITS
       Si vertical y horizontal encontraron la misma pared,
       solo quedarnos con el más cercano
       ======================================== */
    
    /* Eliminar hits duplicados que están en la misma posición de grid */
    for (int i = 0; i < hit_count; i++) {
        if (hits[i].wallType == 0) continue; /* Ya eliminado */
        
        for (int j = i + 1; j < hit_count; j++) {
            if (hits[j].wallType == 0) continue; /* Ya eliminado */
            
            /* Si ambos hits están en el mismo tile y nivel */
            if (hits[i].wallX == hits[j].wallX && 
                hits[i].wallY == hits[j].wallY &&
                hits[i].level == hits[j].level) {
                
                /* Eliminar el más lejano */
                if (hits[i].distance < hits[j].distance) {
                    hits[j].wallType = 0; /* Marcar como eliminado */
                } else {
                    hits[i].wallType = 0; /* Marcar como eliminado */
                    break; /* Este hit ya fue eliminado, pasar al siguiente */
                }
            }
        }
    }
    
    /* Compactar array eliminando hits marcados como 0 */
    int write_idx = 0;
    for (int read_idx = 0; read_idx < hit_count; read_idx++) {
        if (hits[read_idx].wallType != 0) {
            if (write_idx != read_idx) {
                hits[write_idx] = hits[read_idx];
            }
            write_idx++;
        }
    }
    hit_count = write_idx;
    
    *num_hits = hit_count;
}

/* ============================================================================
   THINWALLS RAYCASTING
   ============================================================================ */

/* Find all ThinWalls that intersect with the ray */
void ray_find_intersecting_thin_walls(RAY_RayHit *hits, int *num_hits,
                                      RAY_ThickWall **thickWalls, int num_thick_walls,
                                      float playerX, float playerY,
                                      float rayEndX, float rayEndY)
{
    int hit_count = *num_hits;
    const int max_hits = RAY_MAX_RAYHITS;
    
    /* Iterar sobre todos los ThickWalls */
    for (int tw_idx = 0; tw_idx < num_thick_walls; tw_idx++) {
        RAY_ThickWall *thickWall = thickWalls[tw_idx];
        if (!thickWall) continue;
        
        /* Iterar sobre todos los ThinWalls de este ThickWall */
        for (int thin_idx = 0; thin_idx < thickWall->num_thin_walls; thin_idx++) {
            RAY_ThinWall *thinWall = &thickWall->thinWalls[thin_idx];
            if (thinWall->hidden) continue;
            
            float ix = 0, iy = 0;
            int hitFound = ray_lines_intersect(thinWall->x1, thinWall->y1,
                                               thinWall->x2, thinWall->y2,
                                               playerX, playerY,
                                               rayEndX, rayEndY,
                                               &ix, &iy);
            
            if (hitFound && hit_count < max_hits) {
                float distX = playerX - ix;
                float distY = playerY - iy;
                float squaredDistance = distX * distX + distY * distY;
                float distance = sqrtf(squaredDistance);
                
                if (distance > 0.1f) {  /* Evitar hits muy cercanos al jugador */
                    RAY_RayHit *rayHit = &hits[hit_count];
                    rayHit->thinWall = thinWall;
                    rayHit->x = ix;
                    rayHit->y = iy;
                    rayHit->squaredDistance = squaredDistance;
                    rayHit->distance = distance;
                    rayHit->sprite = NULL;
                    rayHit->wallHeight = thinWall->height;
                    rayHit->wallType = thinWall->wallType;
                    rayHit->horizontal = thinWall->horizontal;
                    rayHit->invertedZ = 0;
                    rayHit->siblingWallHeight = 0;
                    rayHit->siblingDistance = 0;
                    rayHit->siblingCorrectDistance = 0;
                    rayHit->siblingThinWallZ = 0;
                    rayHit->siblingInvertedZ = 0;
                    
                    hit_count++;
                }
            }
        }
    }
    
    *num_hits = hit_count;
}

/* Raycast ThinWalls (slopes/ramps) */
void ray_raycast_thin_walls(RAY_RayHit *hits, int *num_hits,
                            RAY_ThickWall **thickWalls, int num_thick_walls,
                            float playerX, float playerY, float playerZ,
                            float playerRot, float stripAngle, int stripIdx,
                            int gridWidth, int tileSize)
{
    if (num_thick_walls == 0) return;
    
    /* Calcular ángulo del rayo */
    float rayAngle = stripAngle + playerRot;
    while (rayAngle < 0) rayAngle += RAY_TWO_PI;
    while (rayAngle >= RAY_TWO_PI) rayAngle -= RAY_TWO_PI;
    
    /* Determinar dirección */
    int right = (rayAngle < RAY_TWO_PI * 0.25f && rayAngle >= 0) ||
                (rayAngle > RAY_TWO_PI * 0.75f);
    
    /* Calcular punto final del rayo (en el borde del mapa) */
    float vx = 0;
    if (right) {
        vx = gridWidth * tileSize;
    } else {
        vx = 0;
    }
    
    float vy = playerY + (playerX - vx) * tanf(rayAngle);
    
    /* Encontrar ThinWalls que intersectan con este rayo */
    int initial_hits = *num_hits;
    ray_find_intersecting_thin_walls(hits, num_hits, thickWalls, num_thick_walls,
                                     playerX, playerY, vx, vy);
    
    /* Procesar los nuevos hits */
    for (int i = initial_hits; i < *num_hits; i++) {
        RAY_RayHit *rayHit = &hits[i];
        RAY_ThinWall *thinWall = rayHit->thinWall;
        RAY_ThickWall *thickWall = thinWall->thickWall;
        
        rayHit->wallHeight = thinWall->height;
        rayHit->strip = stripIdx;
        
        /* Calcular coordenada de textura basada en distancia al origen del ThinWall */
        float dto = roundf(ray_thin_wall_distance_to_origin(thinWall, rayHit->x, rayHit->y));
        rayHit->tileX = ((int)dto) % tileSize;
        rayHit->horizontal = thinWall->horizontal;
        rayHit->wallType = thinWall->wallType;
        rayHit->rayAngle = rayAngle;
        
        if (rayHit->distance > 0) {
            rayHit->correctDistance = rayHit->distance * cosf(playerRot - rayAngle);
            
            if (rayHit->correctDistance < 1.0f) {
                /* Hit muy cercano, marcar como inválido */
                rayHit->wallType = 0;
                continue;
            }
            
            /* Slope - calcular altura variable */
            static int slope_check_debug = 0;
            if (slope_check_debug < 5 && thickWall && thickWall->slope != 0) {
                printf("SLOPE_CHECK: thinWall->slope=%.3f thickWall->slope=%.3f ptr=%p\n",
                       thinWall->slope, thickWall->slope, (void*)thinWall);
                slope_check_debug++;
            }
            
            if (thinWall->slope != 0) {
                float dto = ray_thin_wall_distance_to_origin(thinWall, rayHit->x, rayHit->y);
                rayHit->wallHeight = thickWall->startHeight + thinWall->slope * dto;
                
                static int slope_calc_debug = 0;
                if (slope_calc_debug < 5) {
                    printf("SLOPE_CALC: startHeight=%.1f slope=%.3f dto=%.1f -> wallHeight=%.1f\n",
                           thickWall->startHeight, thinWall->slope, dto, rayHit->wallHeight);
                    printf("            thinWall: (%.1f,%.1f)-(%.1f,%.1f) hit: (%.1f,%.1f)\n",
                           thinWall->x1, thinWall->y1, thinWall->x2, thinWall->y2,
                           rayHit->x, rayHit->y);
                    slope_calc_debug++;
                }
                
                if (thickWall->invertedSlope) {
                    rayHit->invertedZ = rayHit->wallHeight;
                    rayHit->wallHeight = thickWall->tallerHeight - rayHit->wallHeight;
                }
            }
        }
    }
    
    /* ========================================
       SIBLING SYSTEM - Find opposite wall for slopes
       Port of raycasting.cpp lines 1101-1111
       ======================================== */
    for (int i = initial_hits; i < *num_hits; i++) {
        RAY_RayHit *rayHit = &hits[i];
        RAY_ThinWall *thinWall = rayHit->thinWall;
        if (!thinWall) continue;
        
        RAY_ThickWall *thickWall = thinWall->thickWall;
        if (!thickWall || thickWall->slope == 0) continue;
        
        /* Buscar siblings - otras paredes del mismo ThickWall */
        for (int j = initial_hits; j < *num_hits; j++) {
            if (i == j) continue;
            
            RAY_RayHit *otherHit = &hits[j];
            if (!otherHit->thinWall) continue;
            
            /* Si ambos hits pertenecen al mismo ThickWall, son siblings */
            if (otherHit->thinWall->thickWall == thickWall) {
                /* Copy sibling data (port of RayHit::copySibling) */
                rayHit->siblingWallHeight = otherHit->wallHeight;
                rayHit->siblingDistance = otherHit->distance;
                rayHit->siblingCorrectDistance = otherHit->correctDistance;
                rayHit->siblingThinWallZ = otherHit->thinWall->z;
                rayHit->siblingInvertedZ = otherHit->invertedZ;
                
                /* También copiar en la otra dirección */
                otherHit->siblingWallHeight = rayHit->wallHeight;
                otherHit->siblingDistance = rayHit->distance;
                otherHit->siblingCorrectDistance = rayHit->correctDistance;
                otherHit->siblingThinWallZ = rayHit->thinWall->z;
                otherHit->siblingInvertedZ = rayHit->invertedZ;
            }
        }
    }
    
    /* Compactar array eliminando hits inválidos */
    int write_idx = 0;
    for (int read_idx = 0; read_idx < *num_hits; read_idx++) {
        if (hits[read_idx].wallType != 0 || hits[read_idx].sprite != NULL) {
            if (write_idx != read_idx) {
                hits[write_idx] = hits[read_idx];
            }
            write_idx++;
        }
    }
    *num_hits = write_idx;
}

/* ============================================================================
   FIND SIBLING AT ANGLE
   Exact 1:1 port of RayHit::findSiblingAtAngle() from raycasting.cpp lines 48-111
   ============================================================================ */
int ray_find_sibling_at_angle(RAY_RayHit *rayHit, float originAngle, float playerRot,
                               float playerX, float playerY,
                               int gridWidth, int tileSize)
{
    if (!rayHit->thinWall || !rayHit->thinWall->thickWall) {
        return 0;
    }
    
    RAY_ThickWall *thickWall = rayHit->thinWall->thickWall;
    static int debug_find = 0;
    if (debug_find < 3) {
        printf("FIND_SIBLING: Searching... originAngle=%.3f ThickWall has %d ThinWalls\n", originAngle, thickWall->num_thin_walls);
        debug_find++;
    }
    
    float rayAngle = originAngle; /* Note: we don't add player angle */
    while (rayAngle < 0) rayAngle += RAY_TWO_PI;
    while (rayAngle >= RAY_TWO_PI) rayAngle -= RAY_TWO_PI;
    
    int right = (rayAngle < RAY_TWO_PI * 0.25f && rayAngle >= 0) ||  /* Quadrant 1 */
                (rayAngle > RAY_TWO_PI * 0.75f);                      /* Quadrant 4 */
    
    float vx = 0;
    if (right) {
        vx = gridWidth * tileSize;
    } else {
        vx = 0;
    }
    
    float vy = playerY + (playerX - vx) * tanf(rayAngle);
    
    /* Para ThickWalls rectangulares (4 ThinWalls), encontrar el opuesto correcto:
       [0]=west ↔ [1]=east
       [2]=north ↔ [3]=south */
    int currentIndex = -1;
    for (int i = 0; i < thickWall->num_thin_walls; i++) {
        if (&thickWall->thinWalls[i] == rayHit->thinWall) {
            currentIndex = i;
            break;
        }
    }
    
    if (currentIndex == -1) {
        if (debug_find < 3) {
            printf("  ERROR: Current ThinWall not found in ThickWall!\n");
            debug_find++;
        }
        return 0;
    }
    
    /* Determinar el índice del sibling opuesto */
    int siblingIndex = -1;
    if (thickWall->num_thin_walls == 4) {
        /* Rectangular: 0↔1, 2↔3 */
        if (currentIndex == 0) siblingIndex = 1;
        else if (currentIndex == 1) siblingIndex = 0;
        else if (currentIndex == 2) siblingIndex = 3;
        else if (currentIndex == 3) siblingIndex = 2;
    }
    
    if (siblingIndex == -1 || siblingIndex >= thickWall->num_thin_walls) {
        if (debug_find < 3) {
            printf("  No opposite sibling for ThinWall[%d]\n", currentIndex);
            debug_find++;
        }
        return 0;
    }
    
    RAY_ThinWall *siblingThinWall = &thickWall->thinWalls[siblingIndex];
    
    if (debug_find < 3) {
        printf("  Using opposite sibling: ThinWall[%d] -> ThinWall[%d]\n", 
               currentIndex, siblingIndex);
        debug_find++;
    }
    
    /* Calcular intersección con el sibling */
    float x = 0, y = 0;
    if (ray_lines_intersect(siblingThinWall->x1, siblingThinWall->y1,
                           siblingThinWall->x2, siblingThinWall->y2,
                           playerX, playerY,
                           vx, vy, &x, &y))
    {
        float distX = playerX - x;
        float distY = playerY - y;
        float squaredDistance = distX * distX + distY * distY;
        float distance = sqrtf(squaredDistance);
        
        rayHit->siblingDistance = distance;
        rayHit->siblingThinWallZ = siblingThinWall->z;
        rayHit->siblingWallHeight = siblingThinWall->height;
        
        if (distance > 0) {
            rayHit->siblingCorrectDistance = distance * cosf(playerRot - rayAngle);
        }
        
        /* Slope */
        if (siblingThinWall->slope != 0) {
            rayHit->siblingWallHeight = thickWall->startHeight +
                                      siblingThinWall->slope *
                                      ray_thin_wall_distance_to_origin(siblingThinWall, x, y);
            if (thickWall->invertedSlope) {
                rayHit->siblingInvertedZ = rayHit->siblingWallHeight;
                rayHit->siblingWallHeight = thickWall->tallerHeight - rayHit->siblingWallHeight;
            }
        }
        return 1;
    }
    
    return 0;
}

