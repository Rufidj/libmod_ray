/*
 * libmod_ray_render.c - Complete Rendering System
 * Port of rendering functions from main.cpp
 */

#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <SDL2/SDL.h>

/* Forward declarations */
extern RAY_Engine g_engine;
extern SDL_PixelFormat *gPixelFormat;

/* ============================================================================
   FOG SYSTEM
   ============================================================================ */

#define FOG_R 150.0f
#define FOG_G 150.0f
#define FOG_B 150.0f
#define FOG_START_DISTANCE (RAY_TILE_SIZE * 8)

static uint32_t ray_fog_pixel(uint32_t pixel, float distance)
{
    if (distance < FOG_START_DISTANCE) {
        return pixel;
    }
    
    /* Extraer componentes RGBA */
    uint8_t r = (pixel >> 16) & 0xFF;
    uint8_t g = (pixel >> 8) & 0xFF;
    uint8_t b = pixel & 0xFF;
    uint8_t a = (pixel >> 24) & 0xFF;
    
    /* Calcular factor de fog */
    float fog_factor = (distance - FOG_START_DISTANCE) / (FOG_START_DISTANCE * 2.0f);
    if (fog_factor > 1.0f) fog_factor = 1.0f;
    
    /* Interpolar entre color original y color de fog */
    r = (uint8_t)(r * (1.0f - fog_factor) + FOG_R * fog_factor);
    g = (uint8_t)(g * (1.0f - fog_factor) + FOG_G * fog_factor);
    b = (uint8_t)(b * (1.0f - fog_factor) + FOG_B * fog_factor);
    
    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* ============================================================================
   TEXTURE SAMPLING
   ============================================================================ */

static uint32_t ray_sample_texture(GRAPH *texture, int tex_x, int tex_y)
{
    if (!texture || tex_x < 0 || tex_y < 0 || 
        tex_x >= texture->width || tex_y >= texture->height) {
        return 0xFF000000; /* Negro opaco */
    }
    
    uint32_t pixel = gr_get_pixel(texture, tex_x, tex_y);
    
    /* Extraer componentes RGB usando el formato de pixel */
    extern SDL_PixelFormat *gPixelFormat;
    uint8_t r = (pixel >> gPixelFormat->Rshift) & 0xFF;
    uint8_t g = (pixel >> gPixelFormat->Gshift) & 0xFF;
    uint8_t b = (pixel >> gPixelFormat->Bshift) & 0xFF;
    
    /* Retornar color opaco usando SDL_MapRGB */
    return SDL_MapRGB(gPixelFormat, r, g, b);
}


/* ============================================================================
   WALL RENDERING
   ============================================================================ */

static void ray_draw_wall_strip(GRAPH *dest, RAY_RayHit *rayHit, 
                                int wall_screen_height, float player_screen_z,
                                GRAPH *texture, int texture_dark)
{
    if (!dest || !rayHit || !texture) return;
    
    int strip = rayHit->strip;
    int strip_width = g_engine.stripWidth;
    int screen_x = strip * strip_width;
    
    /* Calcular posición Y en pantalla */
    int screen_y = (g_engine.displayHeight - wall_screen_height) / 2;
    screen_y += (int)player_screen_z;
    screen_y += (int)g_engine.camera.pitch;
    
    /* IMPORTANTE: Añadir offset de altura según el nivel */
    /* Cada nivel está a RAY_TILE_SIZE de altura del anterior */
    float level_z_offset = rayHit->level * RAY_TILE_SIZE;
    float level_screen_offset = ray_strip_screen_height(g_engine.viewDist,
                                                        rayHit->correctDistance,
                                                        level_z_offset);
    screen_y -= (int)level_screen_offset;
    
    /* Calcular coordenada de textura X */
    int texture_x = (int)rayHit->tileX;
    if (texture_x < 0) texture_x = 0;
    if (texture_x >= RAY_TEXTURE_SIZE) texture_x = RAY_TEXTURE_SIZE - 1;
    
    /* Renderizar columna de pared */
    for (int y = 0; y < wall_screen_height; y++) {
        int dst_y = screen_y + y;
        if (dst_y < 0 || dst_y >= g_engine.displayHeight) continue;
        
        /* Calcular coordenada de textura Y */
        float texture_y_f = ((float)y / wall_screen_height) * RAY_TEXTURE_SIZE;
        int texture_y = (int)texture_y_f;
        if (texture_y >= RAY_TEXTURE_SIZE) texture_y = RAY_TEXTURE_SIZE - 1;
        
        /* Obtener pixel de textura */
        uint32_t pixel = ray_sample_texture(texture, texture_x, texture_y);
        
        /* Aplicar fog si está activado */
        if (g_engine.fogOn) {
            pixel = ray_fog_pixel(pixel, rayHit->distance);
        }
        
        /* Dibujar pixel(s) según stripWidth */
        for (int sx = 0; sx < strip_width && screen_x + sx < g_engine.displayWidth; sx++) {
            gr_put_pixel(dest, screen_x + sx, dst_y, pixel);
        }
    }
}

/* ============================================================================
   FLOOR AND CEILING RENDERING
   ============================================================================ */

static void ray_draw_floor_ceiling_strip(GRAPH *dest, RAY_RayHit *rayHit,
                                         int wall_screen_height, float player_screen_z)
{
    if (!dest || !rayHit) return;
    
    int strip = rayHit->strip;
    int strip_width = g_engine.stripWidth;
    int screen_x = strip * strip_width;
    
    float eye_height = RAY_TILE_SIZE / 2.0f + g_engine.camera.z;
    float center_plane = g_engine.displayHeight / 2.0f;
    float cos_factor = 1.0f / cosf(g_engine.camera.rot - rayHit->rayAngle);
    
    int texture_repeat = 2; /* Repetición de textura */
    
    
    /* ========================================
       FLOOR RENDERING
       ======================================== */
    
    if (!g_engine.floorGrid) {
        return; // No floor data
    }
    
    int floor_start_y = (g_engine.displayHeight - wall_screen_height) / 2 + wall_screen_height;
    floor_start_y += (int)player_screen_z;
    if (floor_start_y < center_plane) {
        floor_start_y = (int)center_plane;
    }
    
    for (int screen_y = floor_start_y; screen_y < g_engine.displayHeight; screen_y++) {
        if (screen_y - center_plane <= 0) continue;
        
        float ratio = eye_height / (screen_y - center_plane);
        float straight_distance = g_engine.viewDist * ratio;
        float diagonal_distance = straight_distance * cos_factor;
        
        float x_end = g_engine.camera.x + diagonal_distance * cosf(rayHit->rayAngle);
        float y_end = g_engine.camera.y + diagonal_distance * -sinf(rayHit->rayAngle);
        
        int tile_x = (int)(x_end / RAY_TILE_SIZE);
        int tile_y = (int)(y_end / RAY_TILE_SIZE);
        
        if (tile_x < 0 || tile_x >= g_engine.raycaster.gridWidth ||
            tile_y < 0 || tile_y >= g_engine.raycaster.gridHeight) {
            continue;
        }
        
        /* Obtener tipo de tile de suelo */
        int floor_tile_type = g_engine.floorGrid[tile_x + tile_y * g_engine.raycaster.gridWidth];
        
        if (floor_tile_type <= 0) continue;
        
        /* Obtener textura del FPG */
        GRAPH *floor_texture = bitmap_get(g_engine.fpg_id, floor_tile_type);
        if (!floor_texture) continue;
        
        /* Calcular coordenadas de textura */
        int x = ((int)x_end) % RAY_TILE_SIZE;
        int y = ((int)y_end) % RAY_TILE_SIZE;
        if (x < 0) x += RAY_TILE_SIZE;
        if (y < 0) y += RAY_TILE_SIZE;
        
        int texture_x = (x * floor_texture->width) / RAY_TILE_SIZE;
        int texture_y = (y * floor_texture->height) / RAY_TILE_SIZE;
        
        uint32_t pixel = ray_sample_texture(floor_texture, texture_x, texture_y);
        
        /* Aplicar fog */
        if (g_engine.fogOn) {
            pixel = ray_fog_pixel(pixel, diagonal_distance);
        }
        
        /* Dibujar pixel(s) */
        for (int sx = 0; sx < strip_width && screen_x + sx < g_engine.displayWidth; sx++) {
            gr_put_pixel(dest, screen_x + sx, screen_y, pixel);
        }
    }
    
    /* ========================================
       CEILING RENDERING
       ======================================== */
    
    if (!g_engine.ceilingGrid) {
        return; // No ceiling data
    }
    
    int ceiling_end_y = (g_engine.displayHeight - wall_screen_height) / 2;
    ceiling_end_y += (int)player_screen_z;
    
    for (int screen_y = 0; screen_y < ceiling_end_y && screen_y < g_engine.displayHeight; screen_y++) {
        if (center_plane - screen_y <= 0) continue;
        
        float ratio = (g_engine.highestCeilingLevel * RAY_TILE_SIZE - eye_height) / 
                     (center_plane - screen_y);
        float straight_distance = g_engine.viewDist * ratio;
        float diagonal_distance = straight_distance * cos_factor;
        
        float x_end = g_engine.camera.x + diagonal_distance * cosf(rayHit->rayAngle);
        float y_end = g_engine.camera.y + diagonal_distance * -sinf(rayHit->rayAngle);
        
        int tile_x = (int)(x_end / RAY_TILE_SIZE);
        int tile_y = (int)(y_end / RAY_TILE_SIZE);
        
        if (tile_x < 0 || tile_x >= g_engine.raycaster.gridWidth ||
            tile_y < 0 || tile_y >= g_engine.raycaster.gridHeight) {
            continue;
        }
        
        /* Obtener tipo de tile de techo */
        int ceiling_tile_type = g_engine.ceilingGrid[tile_x + tile_y * g_engine.raycaster.gridWidth];
        
        if (ceiling_tile_type <= 0) continue;
        
        /* Obtener textura del FPG */
        GRAPH *ceiling_texture = bitmap_get(g_engine.fpg_id, ceiling_tile_type);
        if (!ceiling_texture) continue;
        
        /* Calcular coordenadas de textura */
        int x = ((int)x_end) % RAY_TILE_SIZE;
        int y = ((int)y_end) % RAY_TILE_SIZE;
        if (x < 0) x += RAY_TILE_SIZE;
        if (y < 0) y += RAY_TILE_SIZE;
        
        int texture_x = (x * ceiling_texture->width) / RAY_TILE_SIZE;
        int texture_y = (y * ceiling_texture->height) / RAY_TILE_SIZE;
        
        uint32_t pixel = ray_sample_texture(ceiling_texture, texture_x, texture_y);
        
        /* Aplicar fog */
        if (g_engine.fogOn) {
            pixel = ray_fog_pixel(pixel, diagonal_distance);
        }
        
        /* Dibujar pixel(s) */
        for (int sx = 0; sx < strip_width && screen_x + sx < g_engine.displayWidth; sx++) {
            gr_put_pixel(dest, screen_x + sx, screen_y, pixel);
        }
    }
}

/* ============================================================================
   SPRITE RENDERING
   ============================================================================ */

static int ray_sprite_sorter(const void *a, const void *b)
{
    const RAY_Sprite *sa = (const RAY_Sprite*)a;
    const RAY_Sprite *sb = (const RAY_Sprite*)b;
    
    /* Ordenar de más lejano a más cercano */
    if (sa->distance > sb->distance) return -1;
    if (sa->distance < sb->distance) return 1;
    return 0;
}

static void ray_draw_sprites(GRAPH *dest, float *z_buffer)
{
    if (!dest || !z_buffer) return;
    
    /* Calcular distancias de sprites */
    for (int i = 0; i < g_engine.num_sprites; i++) {
        RAY_Sprite *sprite = &g_engine.sprites[i];
        if (sprite->hidden || sprite->cleanup) continue;
        
        float dx = sprite->x - g_engine.camera.x;
        float dy = sprite->y - g_engine.camera.y;
        sprite->distance = sqrtf(dx * dx + dy * dy);
    }
    
    /* Ordenar sprites por distancia */
    qsort(g_engine.sprites, g_engine.num_sprites, sizeof(RAY_Sprite), ray_sprite_sorter);
    
    /* Renderizar sprites */
    for (int i = 0; i < g_engine.num_sprites; i++) {
        RAY_Sprite *sprite = &g_engine.sprites[i];
        if (sprite->hidden || sprite->cleanup || sprite->distance == 0) continue;
        
        /* Calcular ángulo del sprite relativo a la cámara */
        float dx = sprite->x - g_engine.camera.x;
        float dy = sprite->y - g_engine.camera.y;
        float sprite_angle = atan2f(dy, dx);
        
        /* Normalizar ángulo */
        while (sprite_angle - g_engine.camera.rot > M_PI) sprite_angle -= RAY_TWO_PI;
        while (sprite_angle - g_engine.camera.rot < -M_PI) sprite_angle += RAY_TWO_PI;
        
        float angle_diff = sprite_angle - g_engine.camera.rot;
        
        /* Verificar si el sprite está en el FOV */
        if (fabsf(angle_diff) > g_engine.fovRadians / 2.0f + 0.5f) continue;
        
        /* Calcular posición en pantalla */
        float sprite_screen_x = tanf(angle_diff) * g_engine.viewDist;
        int screen_x = g_engine.displayWidth / 2 - (int)sprite_screen_x;
        
        /* Calcular tamaño en pantalla */
        float sprite_screen_height = (g_engine.viewDist / sprite->distance) * sprite->h;
        float sprite_screen_width = (g_engine.viewDist / sprite->distance) * sprite->w;
        
        /* Calcular posición Z en pantalla */
        float sprite_z_offset = sprite->z - g_engine.camera.z;
        float sprite_screen_z = (g_engine.viewDist / sprite->distance) * sprite_z_offset;
        
        int screen_y = g_engine.displayHeight / 2 - (int)(sprite_screen_height / 2) + (int)sprite_screen_z;
        
        /* Obtener textura del sprite */
        GRAPH *sprite_texture = bitmap_get(g_engine.fpg_id, sprite->textureID);
        if (!sprite_texture) continue;
        
        /* Renderizar sprite */
        int start_x = screen_x - (int)(sprite_screen_width / 2);
        int end_x = screen_x + (int)(sprite_screen_width / 2);
        
        for (int sx = start_x; sx < end_x; sx++) {
            if (sx < 0 || sx >= g_engine.displayWidth) continue;
            
            /* Z-buffer check */
            int strip = sx / g_engine.stripWidth;
            if (strip >= 0 && strip < g_engine.rayCount) {
                if (z_buffer[strip] > 0 && sprite->distance > z_buffer[strip]) {
                    continue; /* Sprite detrás de una pared */
                }
            }
            
            /* Calcular coordenada de textura X */
            float tex_x_f = ((float)(sx - start_x) / sprite_screen_width) * sprite_texture->width;
            int tex_x = (int)tex_x_f;
            if (tex_x < 0 || tex_x >= sprite_texture->width) continue;
            
            /* Renderizar columna del sprite */
            for (int sy = screen_y; sy < screen_y + (int)sprite_screen_height; sy++) {
                if (sy < 0 || sy >= g_engine.displayHeight) continue;
                
                /* Calcular coordenada de textura Y */
                float tex_y_f = ((float)(sy - screen_y) / sprite_screen_height) * sprite_texture->height;
                int tex_y = (int)tex_y_f;
                if (tex_y < 0 || tex_y >= sprite_texture->height) continue;
                
                uint32_t pixel = ray_sample_texture(sprite_texture, tex_x, tex_y);
                
                /* Verificar transparencia */
                uint8_t alpha = (pixel >> 24) & 0xFF;
                if (alpha == 0) continue;
                
                /* Aplicar fog */
                if (g_engine.fogOn) {
                    pixel = ray_fog_pixel(pixel, sprite->distance);
                }
                
                gr_put_pixel(dest, sx, sy, pixel);
            }
        }
    }
}

/* ============================================================================
   MAIN RENDER FUNCTION
   ============================================================================ */

void ray_render_frame(GRAPH *dest)
{
    if (!dest) {
        fprintf(stderr, "RAY_RENDER: dest is NULL\n");
        return;
    }
    
    if (!g_engine.initialized) {
        fprintf(stderr, "RAY_RENDER: Engine not initialized\n");
        return;
    }
    
    if (!g_engine.raycaster.grids) {
        fprintf(stderr, "RAY_RENDER: No grids loaded\n");
        return;
    }
    
    if (g_engine.rayCount <= 0) {
        fprintf(stderr, "RAY_RENDER: Invalid rayCount: %d\n", g_engine.rayCount);
        return;
    }
    
    printf("RAY_RENDER: Starting render - %dx%d, rayCount=%d\n", 
           dest->width, dest->height, g_engine.rayCount);
    
    /* Limpiar pantalla con color de cielo */
    gr_clear(dest);
    
    /* Crear buffer de rayhits */
    RAY_RayHit *all_rayhits = (RAY_RayHit*)malloc(g_engine.rayCount * RAY_MAX_RAYHITS * sizeof(RAY_RayHit));
    int *rayhit_counts = (int*)calloc(g_engine.rayCount, sizeof(int));
    float *z_buffer = (float*)malloc(g_engine.rayCount * sizeof(float));
    
    if (!all_rayhits || !rayhit_counts || !z_buffer) {
        fprintf(stderr, "RAY_RENDER: Error allocating buffers\n");
        if (all_rayhits) free(all_rayhits);
        if (rayhit_counts) free(rayhit_counts);
        if (z_buffer) free(z_buffer);
        return;
    }
    
    printf("RAY_RENDER: Buffers allocated\n");
    
    // Inicializar z-buffer
    for (int i = 0; i < g_engine.rayCount; i++) {
        z_buffer[i] = FLT_MAX;
    }
    
    // Resetear rayhit flags de sprites
    for (int i = 0; i < g_engine.num_sprites; i++) {
        g_engine.sprites[i].rayhit = 0;
    }
    
    // ========================================
    // RAYCAST PHASE
    // ========================================
    
    for (int strip = 0; strip < g_engine.rayCount; strip++) {
        float strip_angle = g_engine.stripAngles[strip];
        int num_hits = 0;
        
        ray_raycaster_raycast(&g_engine.raycaster,
                             &all_rayhits[strip * RAY_MAX_RAYHITS],
                             &num_hits,
                             (int)g_engine.camera.x,
                             (int)g_engine.camera.y,
                             g_engine.camera.z,
                             g_engine.camera.rot,
                             strip_angle,
                             strip,
                             g_engine.sprites,
                             g_engine.num_sprites);
        
        rayhit_counts[strip] = num_hits;
        
        // Actualizar z-buffer con el hit más cercano
        for (int h = 0; h < num_hits; h++) {
            RAY_RayHit *hit = &all_rayhits[strip * RAY_MAX_RAYHITS + h];
            if (hit->wallType > 0 && hit->distance < z_buffer[strip]) {
                z_buffer[strip] = hit->distance;
            }
        }
    }
    
    // ========================================
    // RENDER PHASE
    // ========================================
    
    // Renderizar paredes, suelos y techos
    // ESTRATEGIA: Renderizar de atrás hacia adelante (farthest to nearest)
    // Esto permite ver niveles superiores flotantes mientras evita duplicados
    
    for (int strip = 0; strip < g_engine.rayCount; strip++) {
        int num_hits = rayhit_counts[strip];
        
        if (num_hits == 0) continue;
        
        /* Ordenar hits por distancia (más lejano primero) usando bubble sort simple */
        RAY_RayHit *hits = &all_rayhits[strip * RAY_MAX_RAYHITS];
        for (int i = 0; i < num_hits - 1; i++) {
            for (int j = 0; j < num_hits - i - 1; j++) {
                if (hits[j].distance < hits[j + 1].distance) {
                    RAY_RayHit temp = hits[j];
                    hits[j] = hits[j + 1];
                    hits[j + 1] = temp;
                }
            }
        }
        
        /* Renderizar todos los hits de atrás hacia adelante */
        /* Esto permite ver niveles superiores mientras el más cercano tapa lo que está detrás */
        for (int h = 0; h < num_hits; h++) {
            RAY_RayHit *rayHit = &hits[h];
            
            if (rayHit->wallType == 0) continue;
            
            // Calcular altura de pared en pantalla
            int wall_screen_height = (int)ray_strip_screen_height(g_engine.viewDist,
                                                                   rayHit->correctDistance,
                                                                   RAY_TILE_SIZE);
            
            float player_screen_z = ray_strip_screen_height(g_engine.viewDist,
                                                           rayHit->correctDistance,
                                                           g_engine.camera.z);
            
            // Obtener textura de pared
            GRAPH *wall_texture = bitmap_get(g_engine.fpg_id, rayHit->wallType);
            
            // Renderizar pared
            if (g_engine.drawWalls && wall_texture) {
                ray_draw_wall_strip(dest, rayHit, wall_screen_height, player_screen_z,
                                   wall_texture, rayHit->horizontal);
            }
            
            // Renderizar suelo y techo solo para el hit más cercano
            if (h == num_hits - 1) {  // Último hit = más cercano
                if (g_engine.drawTexturedFloor && g_engine.drawCeiling) {
                    ray_draw_floor_ceiling_strip(dest, rayHit, wall_screen_height, player_screen_z);
                }
            }
        }
    }
    
    // Renderizar sprites
    ray_draw_sprites(dest, z_buffer);
    
    // Liberar memoria
    free(all_rayhits);
    free(rayhit_counts);
    free(z_buffer);
    
    printf("RAY_RENDER: Frame complete\n");
}
