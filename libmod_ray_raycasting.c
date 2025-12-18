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
}

static int needs_next_wall(RAY_Raycaster *rc, float playerZ, int x, int y, int z)
{
    if (z == 0) {
        int *grid = rc->grids[0];
        if (x >= 0 && y >= 0) {
            if (ray_is_door(grid[x + y * rc->gridWidth])) {
                return 1;
            }
        }
    }
    
    float eyeHeight = rc->tileSize / 2.0f + playerZ;
    float wallBottom = z * rc->tileSize;
    float wallTop = wallBottom + rc->tileSize;
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
        
        /* Encontrar coordenada X de líneas verticales */
        float vx = 0;
        if (right) {
            vx = floorf(playerX / rc->tileSize) * rc->tileSize + rc->tileSize;
        } else {
            vx = floorf(playerX / rc->tileSize) * rc->tileSize - 1;
        }
        
        /* Calcular coordenada Y */
        float vy = playerY + (playerX - vx) * tanf(rayAngle);
        
        /* Calcular vector de paso */
        float stepx = right ? rc->tileSize : -rc->tileSize;
        float stepy = rc->tileSize * tanf(rayAngle);
        
        if (right) {
            stepy = -stepy;
        }
        
        /* Recorrer líneas verticales */
        while (vx >= 0 && vx < rc->gridWidth * rc->tileSize &&
               vy >= 0 && vy < rc->gridHeight * rc->tileSize)
        {
            int wallY = (int)floorf(vy / rc->tileSize);
            int wallX = (int)floorf(vx / rc->tileSize);
            int wallOffset = wallX + wallY * rc->gridWidth;
            
            /* Verificar si es una pared */
            if (grid[wallOffset] > 0 && !ray_is_horizontal_door(grid[wallOffset])) {
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
                    rayHit->correctDistance = rayHit->distance * cosf(stripAngle);
                    rayHit->horizontal = 0;
                    rayHit->tileX = texX;
                    rayHit->sprite = NULL;
                    rayHit->thinWall = NULL;
                    rayHit->wallHeight = rc->tileSize;
                    
                    /* Manejo especial para puertas verticales */
                    if (ray_is_vertical_door(grid[wallOffset])) {
                        int newWallY = (int)floorf((vy + stepy / 2) / rc->tileSize);
                        int newWallX = (int)floorf((vx + stepx / 2) / rc->tileSize);
                        if (newWallY == wallY && newWallX == wallX) {
                            float halfDistance = stepx / 2 * stepx / 2 + stepy / 2 * stepy / 2;
                            float halfDistanceSquared = sqrtf(halfDistance);
                            rayHit->distance += halfDistanceSquared;
                            texX = fmod2(vy + stepy / 2, rc->tileSize);
                            rayHit->sortdistance -= 1; /* Prioridad menor para puertas */
                            rayHit->correctDistance = rayHit->distance * cosf(stripAngle);
                            rayHit->tileX = texX;
                        }
                    }
                    
                    hit_count++;
                    
                    /* Si no hay espacios, terminar búsqueda */
                    if (!needs_next_wall(rc, playerZ, wallX, wallY, level)) {
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
        while (hx >= 0 && hx < rc->gridWidth * rc->tileSize &&
               hy >= 0 && hy < rc->gridHeight * rc->tileSize)
        {
            int wallY = (int)floorf(hy / rc->tileSize);
            int wallX = (int)floorf(hx / rc->tileSize);
            int wallOffset = wallX + wallY * rc->gridWidth;
            
            /* Verificar si es una pared */
            if (grid[wallOffset] > 0 && !ray_is_vertical_door(grid[wallOffset])) {
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
                    rayHit->correctDistance = rayHit->distance * cosf(stripAngle);
                    rayHit->horizontal = 1;
                    rayHit->tileX = texX;
                    rayHit->sprite = NULL;
                    rayHit->thinWall = NULL;
                    rayHit->wallHeight = rc->tileSize;
                    
                    /* Manejo especial para puertas horizontales */
                    if (ray_is_horizontal_door(grid[wallOffset])) {
                        int newWallY = (int)floorf((hy + stepy / 2) / rc->tileSize);
                        int newWallX = (int)floorf((hx + stepx / 2) / rc->tileSize);
                        if (newWallY == wallY && newWallX == wallX) {
                            float halfDistance = stepx / 2 * stepx / 2 + stepy / 2 * stepy / 2;
                            float halfDistanceSquared = sqrtf(halfDistance);
                            rayHit->distance += halfDistanceSquared;
                            texX = fmod2(hx + stepx / 2, rc->tileSize);
                            rayHit->sortdistance -= 1;
                            rayHit->correctDistance = rayHit->distance * cosf(stripAngle);
                            rayHit->tileX = texX;
                        }
                    }
                    
                    hit_count++;
                    
                    /* Si no hay espacios, terminar búsqueda */
                    if (!needs_next_wall(rc, playerZ, wallX, wallY, level)) {
                        break;
                    }
                }
            }
            
            hx += stepx;
            hy += stepy;
        }
    }
    
    /* TODO: Implementar raycast de sprites */
    /* TODO: Implementar raycast de thin walls */
    
    *num_hits = hit_count;
}
