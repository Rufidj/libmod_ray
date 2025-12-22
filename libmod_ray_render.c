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
extern void ray_update_physics(float delta_time);

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
    /* Calcular límite superior para no sobrescribir el techo */
    int ceiling_end_y = (g_engine.displayHeight - wall_screen_height) / 2 + (int)player_screen_z;
    
    for (int y = 0; y < wall_screen_height; y++) {
        int dst_y = screen_y + y;
        
        /* No dibujar donde está el techo */
        if (dst_y < ceiling_end_y) continue;
        
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
    
    int floor_start_y;
    
    if (wall_screen_height == 0) {
        // No hay paredes sólidas - renderizar desde el centro
        floor_start_y = (int)center_plane;
    } else {
        // Hay paredes - renderizar DESPUÉS de la pared para no sobreescribirla
        floor_start_y = (g_engine.displayHeight - wall_screen_height) / 2 + wall_screen_height;
        floor_start_y += (int)player_screen_z;
        
        // Asegurar que no empiece antes del centro
        if (floor_start_y < center_plane) {
            floor_start_y = (int)center_plane;
        }
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
    
    /* Calcular nivel de la cámara */
    int camera_level = (int)(g_engine.camera.z / RAY_TILE_SIZE);
    if (camera_level < 0) camera_level = 0;
    if (camera_level > 2) camera_level = 2;
    
    /* Solo renderizar el techo del nivel donde está la cámara */
    /* TODO: Implementar grids de techo separados por nivel para renderizado multinivel */
    float ceiling_height = (camera_level + 1) * RAY_TILE_SIZE;
    
    int ceiling_end_y = (g_engine.displayHeight - wall_screen_height) / 2;
    ceiling_end_y += (int)player_screen_z;
    
    for (int screen_y = 0; screen_y < ceiling_end_y && screen_y < g_engine.displayHeight; screen_y++) {
        if (center_plane - screen_y <= 0) continue;
        
        float ratio = (ceiling_height - eye_height) / (center_plane - screen_y);
        float straight_distance = g_engine.viewDist * ratio;
        float diagonal_distance = straight_distance * cos_factor;
        
        /* Calcular posición en el mundo */
        float x_end = g_engine.camera.x + diagonal_distance * cosf(rayHit->rayAngle);
        float y_end = g_engine.camera.y + diagonal_distance * -sinf(rayHit->rayAngle);
        
        /* Calcular tile basándose en la posición proyectada */
        int tile_x = (int)(x_end / RAY_TILE_SIZE);
        int tile_y = (int)(y_end / RAY_TILE_SIZE);
        
        if (tile_x < 0 || tile_x >= g_engine.raycaster.gridWidth ||
            tile_y < 0 || tile_y >= g_engine.raycaster.gridHeight) {
            continue;
        }
        
        /* Obtener tipo de tile de techo del grid actual */
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
        float sprite_angle = atan2f(-dy, dx);  // Invertir dy
        
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
        GRAPH *sprite_texture = NULL;
        
        /* Si el sprite está vinculado a un proceso, usar su graph dinámico */
        if (sprite->process_ptr != NULL) {
            /* Usar instance_graph() para obtener el graph del proceso */
            sprite_texture = instance_graph(sprite->process_ptr);
            
            /* Si el proceso no tiene graph válido, usar textureID como fallback */
            if (!sprite_texture && sprite->textureID > 0) {
                sprite_texture = bitmap_get(g_engine.fpg_id, sprite->textureID);
            }
        } else {
            /* Sprite estático - usar textureID del FPG */
            sprite_texture = bitmap_get(g_engine.fpg_id, sprite->textureID);
        }
        
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
        return;
    }
    
    if (!g_engine.initialized) {
        return;
    }
    
    if (!g_engine.raycaster.grids) {
        return;
    }
    
    if (g_engine.rayCount <= 0) {
        fprintf(stderr, "RAY_RENDER: Invalid rayCount: %d\n", g_engine.rayCount);
        return;
    }
    
    /* Actualizar física y animaciones (asumiendo ~60 FPS) */
    ray_update_physics(1.0f / 60.0f);
    
    /* Limpiar buffer con color de cielo (como en OLD) */
    uint32_t sky_color = 0x87CEEB; /* Sky blue: RGB(135, 206, 235) */
    gr_clear_as(dest, sky_color);
    
    /* Renderizar cielo - skybox o color sólido */
    if (g_engine.skyTextureID > 0) {
        /* Renderizar skybox texture */
        GRAPH *sky_texture = bitmap_get(g_engine.fpg_id, g_engine.skyTextureID);
        if (sky_texture) {
            /* Skybox panorámico simple
             * La textura representa 360° horizontalmente
             * Mapear la rotación de la cámara + FOV a la textura */
            
            int sky_height = dest->height / 2;
            
            for (int x = 0; x < dest->width; x++) {
                /* Calcular el ángulo para este pixel de pantalla
                 * Basado en FOV y posición en pantalla */
                float fov_rad = g_engine.fovRadians;
                float screen_angle = ((float)x / dest->width - 0.5f) * fov_rad;
                float total_angle = g_engine.camera.rot + screen_angle;
                
                /* Normalizar a [0, 2π] */
                total_angle = fmodf(total_angle, 2.0f * M_PI);
                if (total_angle < 0) total_angle += 2.0f * M_PI;
                
                /* Mapear a coordenada X de textura (0 a width-1) */
                int tex_x = (int)((total_angle / (2.0f * M_PI)) * sky_texture->width);
                if (tex_x >= sky_texture->width) tex_x = sky_texture->width - 1;
                if (tex_x < 0) tex_x = 0;
                
                /* Dibujar columna vertical */
                for (int y = 0; y < sky_height; y++) {
                    /* Mapear Y de pantalla a Y de textura */
                    int tex_y = (y * sky_texture->height) / sky_height;
                    if (tex_y >= sky_texture->height) tex_y = sky_texture->height - 1;
                    
                    uint32_t pixel = ray_sample_texture(sky_texture, tex_x, tex_y);
                    gr_put_pixel(dest, x, y, pixel);
                }
            }
        } else {
            /* Si no se encuentra la textura, usar color sólido */
            uint32_t sky_color = 0x87CEEB;
            gr_clear_as(dest, sky_color);
        }
    } else {
        /* Color sólido azul cielo */
        uint32_t sky_color = 0x87CEEB; /* Sky blue: RGB(135, 206, 235) */
        gr_clear_as(dest, sky_color);
    }
    
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
    
    /* Buffers allocated */
    
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
        
    }
    
    /* Recolectar todos los hits de todos los strips */
    int total_hits = 0;
    for (int x = 0; x < g_engine.rayCount; x++) {
        total_hits += rayhit_counts[x];
    }
    
    /* Total hits calculated */
    
    // ========================================
    // FLOOR AND CEILING RENDERING FIRST
    // Renderizar ANTES de las paredes para que las paredes se dibujen encima
    // ========================================
    
    for (int x = 0; x < g_engine.rayCount; x++) {
        int num_hits = rayhit_counts[x];
        RAY_RayHit *hits = &all_rayhits[x * RAY_MAX_RAYHITS];
        
        // Calcular parámetros de renderizado basados en el hit más cercano QUE NO SEA PUERTA
        int wall_screen_height = 0;  // Por defecto 0 para que el suelo se vea completo
        float player_screen_z = 0.0f;
        
        // Crear un rayHit para este strip
        RAY_RayHit floor_hit;
        floor_hit.strip = x;
        floor_hit.rayAngle = g_engine.camera.rot + g_engine.stripAngles[x];
        
        // Buscar si hay puertas en este strip
        int has_door = 0;
        for (int h = 0; h < num_hits; h++) {
            if (ray_is_door(hits[h].wallType)) {
                has_door = 1;
                break;
            }
        }
        
        // Si hay una puerta, SIEMPRE usar wall_height=0 para ver el suelo completo
        if (has_door) {
            floor_hit.correctDistance = RAY_TILE_SIZE * 10.0f;
            wall_screen_height = 0;  // Ver suelo completo a través de puertas
            
            static int debug_floor = 0;
            if (debug_floor < 3) {
                printf("RAY_FLOOR: Strip %d tiene PUERTA - forzando wall_height=0 para ver suelo\n", x);
                debug_floor++;
            }
        } else {
            // No hay puertas - buscar pared más cercana para clipear correctamente
            RAY_RayHit *closest_wall = NULL;
            for (int h = num_hits - 1; h >= 0; h--) {
                closest_wall = &hits[h];
                break;
            }
            
            if (closest_wall) {
                floor_hit.correctDistance = closest_wall->correctDistance;
                
                wall_screen_height = (int)ray_strip_screen_height(g_engine.viewDist,
                                                                   closest_wall->correctDistance,
                                                                   RAY_TILE_SIZE);
                
                player_screen_z = ray_strip_screen_height(g_engine.viewDist,
                                                          closest_wall->correctDistance,
                                                          g_engine.camera.z);
            } else {
                // No hay hits - espacio abierto
                floor_hit.correctDistance = RAY_TILE_SIZE * 10.0f;
                wall_screen_height = 0;
            }
        }
        
        // Renderizar suelo y techo para este strip
        // CORREGIDO: Usar OR (||) en lugar de AND (&&) para permitir renderizado independiente
        if (g_engine.drawTexturedFloor || g_engine.drawCeiling) {
            ray_draw_floor_ceiling_strip(dest, &floor_hit, wall_screen_height, player_screen_z);
        }
    }
    
    // ========================================
    // WALL RENDERING
    // Renderizar paredes DESPUÉS del suelo
    // ========================================
    
    /* Renderizar cada strip */
    for (int x = 0; x < g_engine.rayCount; x++) {
        int num_hits = rayhit_counts[x];
        RAY_RayHit *hits = &all_rayhits[x * RAY_MAX_RAYHITS];
        
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
            
            
            // Convertir ID de puerta a ID de textura
            int texture_id = rayHit->wallType;
            int is_door = ray_is_door(texture_id);
            float door_offset = 0.0f;
            
            static int door_detection_debug = 0;
            
            if (is_door) {
                /* Obtener estado de la puerta */
                int door_grid_offset = rayHit->wallX + rayHit->wallY * g_engine.raycaster.gridWidth;
                
                if (door_detection_debug < 5) {
                    printf("RAY_RENDER: Puerta detectada! wallType=%d, pos=(%d,%d), grid_offset=%d\n",
                           rayHit->wallType, rayHit->wallX, rayHit->wallY, door_grid_offset);
                    door_detection_debug++;
                }
                
                if (g_engine.doors && door_grid_offset >= 0 && 
                    door_grid_offset < g_engine.raycaster.gridWidth * g_engine.raycaster.gridHeight) {
                    RAY_Door *door = &g_engine.doors[door_grid_offset];
                    door_offset = door->offset;
                    
                    if (door_detection_debug < 5) {
                        printf("RAY_RENDER: Door state=%d, offset=%.2f, animating=%d\n",
                               door->state, door->offset, door->animating);
                        door_detection_debug++;
                    }
                }
                
                // Puertas verticales: 1001-1500 → restar 1000
                // Puertas horizontales: 1501+ → restar 1500
                if (ray_is_vertical_door(texture_id)) {
                    texture_id = texture_id - 1000;
                } else {
                    texture_id = texture_id - 1500;
                }
                
                // DEBUG: Mostrar cuando se renderiza una puerta (solo primeras 3)
                static int door_render_count = 0;
                if (door_render_count < 3) {
                    printf("RAY_RENDER: Renderizando puerta ID=%d → textura=%d en strip %d, offset=%.2f\n",
                           rayHit->wallType, texture_id, x, door_offset);
                    door_render_count++;
                }
            }
            
            // Obtener textura de pared
            GRAPH *wall_texture = bitmap_get(g_engine.fpg_id, texture_id);
            
            // DEBUG: Verificar si la textura se cargó
            if (is_door) {
                static int texture_check_count = 0;
                if (texture_check_count < 3) {
                    printf("RAY_RENDER: Textura %d %s (fpg_id=%d)\n",
                           texture_id, wall_texture ? "CARGADA" : "NO ENCONTRADA", g_engine.fpg_id);
                    texture_check_count++;
                }
            }
            
            // Renderizar pared
            if (g_engine.drawWalls && wall_texture) {
                /* Aplicar offset de animación */
                if (is_door && door_offset > 0.0f) {
                    static int door_offset_debug = 0;
                    
                    /* Para puertas VERTICALES, deslizar horizontalmente (modificar tileX) */
                    if (ray_is_vertical_door(rayHit->wallType)) {
                        if (door_offset_debug < 10) {
                            printf("RAY_RENDER: Puerta VERTICAL - offset %.2f, tileX original=%.2f\n", 
                                   door_offset, rayHit->tileX);
                            door_offset_debug++;
                        }
                        
                        /* Modificar tileX para crear efecto de deslizamiento horizontal */
                        rayHit->tileX += door_offset * RAY_TILE_SIZE;
                        
                        /* Si tileX sale del rango de la textura, la puerta está "fuera de vista" */
                        if (rayHit->tileX >= RAY_TILE_SIZE) {
                            /* Puerta completamente abierta - no renderizar */
                            if (door_offset_debug < 10) {
                                printf("RAY_RENDER: Puerta vertical fuera de vista\n");
                                door_offset_debug++;
                            }
                            goto skip_wall_render;
                        }
                    } 
                    /* Para puertas HORIZONTALES, deslizar verticalmente (reducir altura) */
                    else {
                        if (door_offset_debug < 10) {
                            printf("RAY_RENDER: Puerta HORIZONTAL - offset %.2f, altura original=%d\n", 
                                   door_offset, wall_screen_height);
                            door_offset_debug++;
                        }
                        
                        /* Reducir altura de pared para crear efecto de deslizamiento vertical */
                        int original_height = wall_screen_height;
                        wall_screen_height = (int)(wall_screen_height * (1.0f - door_offset));
                        
                        /* Ajustar player_screen_z para que la puerta se deslice desde abajo hacia arriba */
                        /* RESTAR la diferencia para que suba en lugar de bajar */
                        player_screen_z -= (original_height - wall_screen_height);
                        
                        /* Si la altura es muy pequeña, no renderizar */
                        if (wall_screen_height < 2) {
                            if (door_offset_debug < 10) {
                                printf("RAY_RENDER: Puerta horizontal completamente abierta\n");
                                door_offset_debug++;
                            }
                            goto skip_wall_render;
                        }
                    }
                }
                
                ray_draw_wall_strip(dest, rayHit, wall_screen_height, player_screen_z,
                                   wall_texture, rayHit->horizontal);
            }
            
            skip_wall_render:;
            
            // Suelo ya renderizado ANTES de las paredes
        }
    }
    
    // Renderizar sprites (después de paredes)
    ray_draw_sprites(dest, z_buffer);
    
    // Liberar memoria
    free(all_rayhits);
    free(rayhit_counts);
    free(z_buffer);
}
