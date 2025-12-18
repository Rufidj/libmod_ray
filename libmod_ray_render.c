#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL.h>

/* Acceso a variables globales de BennuGD2 para renderizado directo */
#ifdef USE_SDL2_GPU
#include <SDL_gpu.h>
extern GPU_Target* gRenderer;
#else
extern SDL_Renderer* gRenderer;
#endif

/* ============================================================================
   FUNCIONES DE RENDERIZADO
   Optimizadas con acceso directo a SDL2 para máximo rendimiento
   ============================================================================ */

/* Configuración de niebla */
float ray_fog_start = 500.0f;
float ray_fog_end = 2000.0f;
uint32_t ray_fog_color = 0xFF808080; /* Gris */
uint32_t ray_sky_color = 0xFF87CEEB; /* Azul cielo */

/**
 * Aplicar efecto de niebla a un color basado en la distancia
 * OPTIMIZADO: inline para evitar overhead de llamada a función
 */
static inline uint32_t apply_fog_inline(uint32_t color, float distance)
{
    /* Early exit si no hay niebla */
    if (distance <= ray_fog_start) {
        return color;
    }
    
    if (distance >= ray_fog_end) {
        return ray_fog_color;
    }
    
    /* Interpolar entre color original y color de niebla */
    float fog_factor = (distance - ray_fog_start) / (ray_fog_end - ray_fog_start);
    
    uint8_t r1 = (color >> 16) & 0xFF;
    uint8_t g1 = (color >> 8) & 0xFF;
    uint8_t b1 = color & 0xFF;
    uint8_t a1 = (color >> 24) & 0xFF;
    
    uint8_t r2 = (ray_fog_color >> 16) & 0xFF;
    uint8_t g2 = (ray_fog_color >> 8) & 0xFF;
    uint8_t b2 = ray_fog_color & 0xFF;
    
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * fog_factor);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * fog_factor);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * fog_factor);
    
    return (a1 << 24) | (r << 16) | (g << 8) | b;
}

/* Wrapper para mantener compatibilidad */
uint32_t ray_apply_fog(uint32_t color, float distance)
{
    return apply_fog_inline(color, distance);
}



/* ============================================================================
   FRAME CACHE - PRE-CÁLCULO OPTIMIZADO
   ============================================================================ */

/**
 * Crear caché de frame para pre-cálculo
 */
RAY_FrameCache* ray_create_frame_cache(int screen_w, int screen_h, int max_hits_per_column)
{
    RAY_FrameCache* cache = (RAY_FrameCache*)malloc(sizeof(RAY_FrameCache));
    if (!cache) return NULL;
    
    cache->screen_w = screen_w;
    cache->screen_h = screen_h;
    cache->max_hits_per_column = max_hits_per_column;
    cache->valid = 0;
    
    /* Allocar arrays */
    cache->column_hits = (RAY_RayHit*)malloc(sizeof(RAY_RayHit) * screen_w * max_hits_per_column);
    cache->column_hit_counts = (int*)calloc(screen_w, sizeof(int));
    cache->z_buffer = (float*)malloc(sizeof(float) * screen_w * screen_h);
    cache->wall_top = (int*)malloc(sizeof(int) * screen_w);
    cache->wall_bottom = (int*)malloc(sizeof(int) * screen_w);
    
    cache->max_visible_sectors = 256;
    cache->visible_sectors = (int*)malloc(sizeof(int) * cache->max_visible_sectors);
    cache->num_visible_sectors = 0;
    
    /* Inicializar Z-buffer con valores infinitos */
    for (int i = 0; i < screen_w * screen_h; i++) {
        cache->z_buffer[i] = 999999.0f;
    }
    
    return cache;
}

/**
 * Destruir caché de frame
 */
void ray_destroy_frame_cache(RAY_FrameCache* cache)
{
    if (!cache) return;
    
    if (cache->column_hits) free(cache->column_hits);
    if (cache->column_hit_counts) free(cache->column_hit_counts);
    if (cache->z_buffer) free(cache->z_buffer);
    if (cache->wall_top) free(cache->wall_top);
    if (cache->wall_bottom) free(cache->wall_bottom);
    if (cache->visible_sectors) free(cache->visible_sectors);
    
    free(cache);
}

/**
 * Pre-calcular todos los raycasts para el frame
 */
void ray_precalculate_frame(RAY_Raycaster* rc, RAY_FrameCache* cache,
                            float cam_x, float cam_y, float cam_z,
                            float cam_angle, float fov)
{
    if (!rc || !cache) return;
    
    float screen_distance = ray_screen_distance((float)cache->screen_w, fov * M_PI / 180.0f);
    
    /* Resetear contadores */
    cache->num_visible_sectors = 0;
    for (int i = 0; i < cache->screen_w; i++) {
        cache->column_hit_counts[i] = 0;
    }
    
    /* Hacer raycast para cada columna */
    for (int strip = 0; strip < cache->screen_w; strip++) {
        float screen_x = strip - cache->screen_w / 2.0f;
        float strip_angle = ray_strip_angle(screen_x, screen_distance);
        
        /* Obtener puntero al inicio de hits para esta columna */
        RAY_RayHit* column_hits = &cache->column_hits[strip * cache->max_hits_per_column];
        int num_hits = 0;
        
        /* Hacer raycast */
        ray_raycast(rc, column_hits, &num_hits,
                   cam_x, cam_y, cam_z,
                   cam_angle, strip_angle, strip,
                   NULL, 0);
        
        /* Limitar hits al máximo permitido */
        if (num_hits > cache->max_hits_per_column) {
            num_hits = cache->max_hits_per_column;
        }
        
        cache->column_hit_counts[strip] = num_hits;
        
        /* Ordenar hits por distancia (más lejanos primero) */
        for (int i = 0; i < num_hits - 1; i++) {
            for (int j = i + 1; j < num_hits; j++) {
                if (column_hits[i].distance < column_hits[j].distance) {
                    RAY_RayHit temp = column_hits[i];
                    column_hits[i] = column_hits[j];
                    column_hits[j] = temp;
                }
            }
        }
    }
    
    cache->valid = 1;
}

/**
 * Construir Z-buffer desde los hits pre-calculados
 */
void ray_build_zbuffer(RAY_FrameCache* cache, float screen_distance, float cam_z, int tile_size)
{
    if (!cache || !cache->valid) return;
    
    int screen_w = cache->screen_w;
    int screen_h = cache->screen_h;
    
    /* Inicializar Z-buffer con valores infinitos */
    for (int i = 0; i < screen_w * screen_h; i++) {
        cache->z_buffer[i] = 999999.0f;
    }
    
    /* Construir Z-buffer desde hits de paredes */
    for (int strip = 0; strip < screen_w; strip++) {
        RAY_RayHit* column_hits = &cache->column_hits[strip * cache->max_hits_per_column];
        int num_hits = cache->column_hit_counts[strip];
        
        if (num_hits == 0) {
            cache->wall_top[strip] = screen_h / 2;
            cache->wall_bottom[strip] = screen_h / 2;
            continue;
        }
        
        /* Usar el hit más cercano para determinar wall bounds */
        RAY_RayHit* closest_hit = &column_hits[num_hits - 1];
        
        float wall_height = ray_strip_screen_height(screen_distance,
                                                    closest_hit->correct_distance,
                                                    closest_hit->wall_height);
        
        float playerScreenZ = ray_strip_screen_height(screen_distance,
                                                      closest_hit->correct_distance,
                                                      cam_z);
        
        int wall_top = (int)((screen_h - wall_height) / 2.0f + playerScreenZ);
        int wall_bottom = wall_top + (int)wall_height;
        
        /* Clipping */
        if (wall_top < 0) wall_top = 0;
        if (wall_bottom > screen_h) wall_bottom = screen_h;
        
        cache->wall_top[strip] = wall_top;
        cache->wall_bottom[strip] = wall_bottom;
        
        /* Llenar Z-buffer para píxeles de pared */
        for (int y = wall_top; y < wall_bottom; y++) {
            if (y >= 0 && y < screen_h) {
                cache->z_buffer[y * screen_w + strip] = closest_hit->distance;
            }
        }
    }
}


/**
 * Obtener textura del FPG
 */
static GRAPH* ray_get_texture(int texture_id)
{
    /* Si no hay FPG asignado, retornar NULL */
    if (ray_fpg_id < 0) {
        return NULL;
    }
    
    /* Usar bitmap_get para obtener gráfico del FPG */
    return bitmap_get(ray_fpg_id, texture_id);
}

/**
 * Renderizar una columna de pared
 */
void ray_render_wall_strip(GRAPH* buffer, RAY_RayHit* hit,
                           int strip, int screen_h,
                           float screen_distance, float cam_z)
{
    if (!buffer || !hit) return;
    
    /* Calcular altura de la pared en pantalla */
    float wall_height = ray_strip_screen_height(screen_distance, 
                                                hit->correct_distance,
                                                hit->wall_height);
    
    /* Calcular offset vertical basado en la altura de la cámara */
    /* COPIADO DE src/main.cpp líneas 2202-2204 */
    float playerScreenZ = ray_strip_screen_height(screen_distance,
                                                  hit->correct_distance,
                                                  cam_z);
    
    /* Calcular posición vertical (centrada + offset de altura del jugador) */
    int wall_top = (int)((screen_h - wall_height) / 2.0f + playerScreenZ);
    int wall_bottom = wall_top + (int)wall_height;
    
    /* Clipping */
    if (wall_top < 0) wall_top = 0;
    if (wall_bottom > screen_h) wall_bottom = screen_h;
    
    /* Obtener textura */
    GRAPH* texture = ray_get_texture(hit->wall_type);
    
    if (texture) {
        /* Renderizar columna con textura */
        int tex_x = (int)hit->tile_x;
        if (tex_x < 0) tex_x = 0;
        if (tex_x >= texture->width) tex_x = texture->width - 1;
        
        for (int y = wall_top; y < wall_bottom; y++) {
            /* Calcular coordenada Y de textura */
            float tex_y_ratio = (float)(y - wall_top) / wall_height;
            int tex_y = (int)(tex_y_ratio * texture->height);
            if (tex_y < 0) tex_y = 0;
            if (tex_y >= texture->height) tex_y = texture->height - 1;
            
            /* Obtener color del pixel de la textura - OPTIMIZADO */
            uint32_t color = gr_get_pixel(texture, tex_x, tex_y);
            
            /* Aplicar niebla */
            color = apply_fog_inline(color, hit->distance);
            
            /* Dibujar pixel en el buffer - OPTIMIZADO */
            if (strip >= 0 && strip < buffer->width && y >= 0 && y < buffer->height) {
                gr_put_pixel(buffer, strip, y, color);
            }
        }
    } else {
        /* Sin textura, usar color sólido basado en el tipo de pared */
        uint32_t wall_color = 0xFF808080; /* Gris por defecto */
        
        /* Variar color según orientación para mejor percepción de profundidad */
        if (hit->horizontal) {
            wall_color = 0xFF606060; /* Más oscuro para horizontales */
        }
        
        wall_color = ray_apply_fog(wall_color, hit->distance);
        
        for (int y = wall_top; y < wall_bottom; y++) {
            if (strip >= 0 && strip < buffer->width && y >= 0 && y < buffer->height) {
                gr_put_pixel(buffer, strip, y, wall_color);
            }
        }
    }
}

/**
 * Renderizar suelo y techo basado en sectores
 */
void ray_render_floor_ceiling(GRAPH* buffer, RAY_Raycaster* rc,
                               int screen_w, int screen_h,
                               float cam_x, float cam_y, float cam_z,
                               float cam_angle, float cam_fov)
{
    if (!rc || !rc->sectors || rc->num_sectors == 0) {
        /* Sin sectores, renderizar cielo simple */
        for (int y = 0; y < screen_h / 2; y++) {
            for (int x = 0; x < screen_w; x++) {
                gr_put_pixel(buffer, x, y, ray_sky_color);
            }
        }
        return;
    }
    
    float screen_distance = (screen_w / 2.0f) / tanf(cam_fov * M_PI / 180.0f / 2.0f);
    
    /* Renderizar mitad superior (techo) y mitad inferior (suelo) */
    for (int y = 0; y < screen_h; y++) {
        int is_floor = (y > screen_h / 2);
        float row_distance;
        
        if (is_floor) {
            /* Suelo */
            row_distance = (cam_z * screen_distance) / (y - screen_h / 2.0f);
        } else {
            /* Techo */
            float ceiling_height = rc->tile_size; /* Altura por defecto */
            row_distance = ((ceiling_height - cam_z) * screen_distance) / (screen_h / 2.0f - y);
        }
        
        /* Limitar distancia para evitar renderizado infinito */
        if (row_distance > 800.0f || row_distance < 0.1f) {
            /* Renderizar cielo */
            for (int x = 0; x < screen_w; x++) {
                gr_put_pixel(buffer, x, y, ray_sky_color);
            }
            continue;
        }
        
        for (int x = 0; x < screen_w; x++) {
            /* Calcular ángulo del rayo */
            float ray_angle = cam_angle + atanf((x - screen_w / 2.0f) / screen_distance);
            
            /* Calcular posición en el mundo */
            float world_x = cam_x + cosf(ray_angle) * row_distance;
            float world_y = cam_y + sinf(ray_angle) * row_distance;
            
            /* Convertir a coordenadas de grid */
            int grid_x = (int)(world_x / rc->tile_size);
            int grid_y = (int)(world_y / rc->tile_size);
            
            /* Buscar sector en esta posición */
            RAY_Sector* sector = ray_find_sector_at(rc, grid_x, grid_y);
            
            if (sector) {
                if (is_floor) {
                    /* Renderizar suelo del sector */
                    GRAPH* texture = ray_get_texture(sector->floor_texture);
                    if (texture) {
                        /* Usar coordenadas de grid + parte fraccionaria */
                        /* Esto ancla las texturas al grid del mundo */
                        float frac_x = (world_x / rc->tile_size) - grid_x;
                        float frac_y = (world_y / rc->tile_size) - grid_y;
                        
                        /* Asegurar que frac esté en [0, 1) */
                        if (frac_x < 0) frac_x += 1.0f;
                        if (frac_y < 0) frac_y += 1.0f;
                        if (frac_x >= 1.0f) frac_x -= 1.0f;
                        if (frac_y >= 1.0f) frac_y -= 1.0f;
                        
                        /* Mapear a coordenadas de textura */
                        int tex_x = (int)(frac_x * texture->width);
                        int tex_y = (int)(frac_y * texture->height);
                        
                        /* Clamp para seguridad */
                        if (tex_x < 0) tex_x = 0;
                        if (tex_y < 0) tex_y = 0;
                        if (tex_x >= texture->width) tex_x = texture->width - 1;
                        if (tex_y >= texture->height) tex_y = texture->height - 1;
                        
                        uint32_t color = gr_get_pixel(texture, tex_x, tex_y);
                        color = ray_apply_fog(color, row_distance);
                        gr_put_pixel(buffer, x, y, color);
                    } else {
                        /* Sin textura, usar color sólido */
                        gr_put_pixel(buffer, x, y, 0x404040);
                    }
                } else {
                    /* Renderizar techo del sector */
                    if (sector->has_ceiling) {
                        GRAPH* texture = ray_get_texture(sector->ceiling_texture);
                        if (texture) {
                            /* Usar coordenadas de grid + parte fraccionaria */
                            float frac_x = (world_x / rc->tile_size) - grid_x;
                            float frac_y = (world_y / rc->tile_size) - grid_y;
                            
                            /* Asegurar que frac esté en [0, 1) */
                            if (frac_x < 0) frac_x += 1.0f;
                            if (frac_y < 0) frac_y += 1.0f;
                            if (frac_x >= 1.0f) frac_x -= 1.0f;
                            if (frac_y >= 1.0f) frac_y -= 1.0f;
                            
                            /* Mapear a coordenadas de textura */
                            int tex_x = (int)(frac_x * texture->width);
                            int tex_y = (int)(frac_y * texture->height);
                            
                            /* Clamp para seguridad */
                            if (tex_x < 0) tex_x = 0;
                            if (tex_y < 0) tex_y = 0;
                            if (tex_x >= texture->width) tex_x = texture->width - 1;
                            if (tex_y >= texture->height) tex_y = texture->height - 1;
                            
                            uint32_t color = gr_get_pixel(texture, tex_x, tex_y);
                            color = ray_apply_fog(color, row_distance);
                            gr_put_pixel(buffer, x, y, color);
                        } else {
                            /* Sin textura, usar color sólido */
                            gr_put_pixel(buffer, x, y, 0x606060);
                        }
                    } else {
                        /* Cielo abierto */
                        gr_put_pixel(buffer, x, y, ray_sky_color);
                    }
                }
            } else {
                /* Fuera de cualquier sector, renderizar vacío */
                gr_put_pixel(buffer, x, y, is_floor ? 0x000000 : ray_sky_color);
            }
        }
    }
}

/**
 * Renderizar sprites como billboards
 */
void ray_render_sprites(GRAPH* buffer, RAY_Sprite* sprites,
                       int num_sprites, float cam_x, float cam_y,
                       float cam_z, float cam_angle, float screen_distance,
                       int screen_w, int screen_h)
{
    if (!buffer || !sprites) return;
    
    for (int i = 0; i < num_sprites; i++) {
        RAY_Sprite* sprite = &sprites[i];
        if (sprite->hidden) continue;
        
        /* Calcular vector desde cámara al sprite */
        float dx = sprite->x - cam_x;
        float dy = sprite->y - cam_y;
        float distance = sqrtf(dx * dx + dy * dy);
        
        if (distance < 1.0f) continue; /* Muy cerca */
        
        /* Calcular ángulo del sprite relativo a la cámara */
        float sprite_angle = atan2f(dy, dx);
        float angle_diff = sprite_angle - cam_angle;
        
        /* Normalizar ángulo */
        while (angle_diff < -M_PI) angle_diff += 2.0f * M_PI;
        while (angle_diff > M_PI) angle_diff -= 2.0f * M_PI;
        
        /* Calcular posición X en pantalla */
        float screen_x = tanf(angle_diff) * screen_distance;
        int sprite_screen_x = (int)(screen_w / 2 + screen_x);
        
        /* Calcular tamaño del sprite en pantalla */
        float sprite_height = screen_distance / distance * sprite->h;
        float sprite_width = screen_distance / distance * sprite->w;
        
        int sprite_top = (int)((screen_h - sprite_height) / 2.0f);
        int sprite_left = (int)(sprite_screen_x - sprite_width / 2.0f);
        
        /* Obtener textura del sprite */
        GRAPH* texture = ray_get_texture(sprite->texture_id);
        if (!texture) continue;
        
        /* Renderizar sprite */
        for (int sy = 0; sy < (int)sprite_height; sy++) {
            int screen_y = sprite_top + sy;
            if (screen_y < 0 || screen_y >= screen_h) continue;
            
            int tex_y = (int)((float)sy / sprite_height * texture->height);
            if (tex_y >= texture->height) tex_y = texture->height - 1;
            
            for (int sx = 0; sx < (int)sprite_width; sx++) {
                int screen_x_pos = sprite_left + sx;
                if (screen_x_pos < 0 || screen_x_pos >= screen_w) continue;
                
                int tex_x = (int)((float)sx / sprite_width * texture->width);
                if (tex_x >= texture->width) tex_x = texture->width - 1;
                
                /* Obtener color del pixel usando gr_get_pixel */
                uint32_t color = gr_get_pixel(texture, tex_x, tex_y);
                
                /* Verificar transparencia (alpha = 0) */
                if ((color & 0xFF000000) == 0) continue;
                
                /* Aplicar niebla */
                color = ray_apply_fog(color, distance);
                
                /* Dibujar pixel usando gr_put_pixel */
                gr_put_pixel(buffer, screen_x_pos, screen_y, color);
            }
        }
    }
}

/**
 * Función principal de renderizado de frame - OPTIMIZADA CON PRE-CÁLCULO
 */
void ray_render_frame(RAY_Raycaster* rc, GRAPH* render_buffer,
                      float cam_x, float cam_y, float cam_z,
                      float cam_angle, float cam_pitch, float fov,
                      int screen_w, int screen_h)
{
    if (!rc || !render_buffer) return;
    
    /* Limpiar buffer completamente antes de renderizar */
    gr_clear(render_buffer);
    
    /* Si hay sectores y portales, usar renderizado con portales */
    if (rc->num_sectors > 0 && rc->num_portals > 0) {
        ray_render_with_portals(rc, render_buffer, cam_x, cam_y, cam_z,
                               cam_angle, cam_pitch, fov, screen_w, screen_h);
        /* Por ahora, el renderizado con portales es experimental
         * Continuamos con el renderizado normal para asegurar que algo se vea */
    }
    
    /* Crear frame cache (se reutilizará en frames subsiguientes) */
    static RAY_FrameCache* frame_cache = NULL;
    static int last_w = 0, last_h = 0;
    
    if (!frame_cache || last_w != screen_w || last_h != screen_h) {
        if (frame_cache) ray_destroy_frame_cache(frame_cache);
        frame_cache = ray_create_frame_cache(screen_w, screen_h, 16);
        last_w = screen_w;
        last_h = screen_h;
    }
    
    if (!frame_cache) return;
    
    /* FASE 1: PRE-CÁLCULO - Hacer todos los raycasts una sola vez */
    ray_precalculate_frame(rc, frame_cache, cam_x, cam_y, cam_z, cam_angle, fov);
    
    /* Calcular distancia al plano de proyección */
    float screen_distance = ray_screen_distance((float)screen_w, fov * M_PI / 180.0f);
    
    /* FASE 2: CONSTRUIR Z-BUFFER */
    ray_build_zbuffer(frame_cache, screen_distance, cam_z, rc->tile_size);
    
    /* FASE 3: RENDERIZADO - Usar datos pre-calculados */
    float centerPlane = screen_h / 2.0f;
    float eyeHeight = rc->tile_size / 2.0f + cam_z;
    
    /* Pre-calcular valores constantes fuera del loop */
    float inv_tile_size = 1.0f / rc->tile_size;
    
    /* Renderizar cada columna usando datos cacheados */
    for (int strip = 0; strip < screen_w; strip++) {
        /* Obtener hits pre-calculados para esta columna */
        RAY_RayHit* column_hits = &frame_cache->column_hits[strip * frame_cache->max_hits_per_column];
        int num_hits = frame_cache->column_hit_counts[strip];
        
        /* Calcular ángulo del rayo (necesario para floor/ceiling) */
        float screen_x = strip - screen_w / 2.0f;
        float strip_angle = ray_strip_angle(screen_x, screen_distance);
        float ray_angle = cam_angle + strip_angle;
        const float cosFactor = 1.0f / cosf(cam_angle - ray_angle);
        
        /* Pre-calcular seno y coseno */
        float cos_ray = cosf(ray_angle);
        float sin_ray = -sinf(ray_angle);
        
        /* RENDERIZAR SUELO */
        if (num_hits > 0) {
            int wall_bottom = frame_cache->wall_bottom[strip];
            
            /* Cache para sectores */
            int last_tileX = -1;
            int last_tileY = -1;
            RAY_Sector* cached_sector = NULL;
            GRAPH* cached_texture = NULL;
            int cached_tex_w = 0;
            int cached_tex_h = 0;
            float cached_inv_tex_w = 0.0f;
            float cached_inv_tex_h = 0.0f;
            
            /* OPTIMIZACIÓN: Renderizar cada 2 píxeles verticalmente para floor/ceiling */
            const int step = 2;
            
            for (int screenY = wall_bottom; screenY < screen_h; screenY += step) {
                float ratio = eyeHeight / (screenY - centerPlane);
                float straightDistance = screen_distance * ratio;
                float diagonalDistance = straightDistance * cosFactor;
                
                /* Calcular posición en el mundo */
                float xEnd = (diagonalDistance * cos_ray) + cam_x;
                float yEnd = (diagonalDistance * sin_ray) + cam_y;
                
                /* Optimización: usar casting directo en lugar de modulo */
                int tileX = (int)(xEnd * inv_tile_size);
                int tileY = (int)(yEnd * inv_tile_size);
                
                /* Solo buscar sector si cambiamos de tile */
                if (tileX != last_tileX || tileY != last_tileY) {
                    cached_sector = ray_find_sector_at(rc, tileX, tileY);
                    cached_texture = cached_sector ? ray_get_texture(cached_sector->floor_texture) : NULL;
                    if (cached_texture) {
                        cached_tex_w = cached_texture->width;
                        cached_tex_h = cached_texture->height;
                        cached_inv_tex_w = 1.0f / cached_tex_w;
                        cached_inv_tex_h = 1.0f / cached_tex_h;
                    }
                    last_tileX = tileX;
                    last_tileY = tileY;
                }
                
                if (cached_texture) {
                    /* Calcular coordenadas de textura sin modulo */
                    float frac_x = xEnd - (tileX * rc->tile_size);
                    float frac_y = yEnd - (tileY * rc->tile_size);
                    
                    /* Asegurar que esté en rango [0, tile_size) */
                    if (frac_x < 0) frac_x += rc->tile_size;
                    if (frac_y < 0) frac_y += rc->tile_size;
                    
                    int tex_x = (int)(frac_x * inv_tile_size * cached_tex_w);
                    int tex_y = (int)(frac_y * inv_tile_size * cached_tex_h);
                    
                    /* Clamp */
                    if (tex_x >= cached_tex_w) tex_x = cached_tex_w - 1;
                    if (tex_y >= cached_tex_h) tex_y = cached_tex_h - 1;
                    
                    uint32_t color = gr_get_pixel(cached_texture, tex_x, tex_y);
                    color = apply_fog_inline(color, diagonalDistance);
                    
                    /* Dibujar píxel y el siguiente (para llenar el gap del step) */
                    gr_put_pixel(render_buffer, strip, screenY, color);
                    if (screenY + 1 < screen_h) {
                        gr_put_pixel(render_buffer, strip, screenY + 1, color);
                    }
                }
            }
            
            /* RENDERIZAR TECHO */
            int wall_top = frame_cache->wall_top[strip];
            
            /* Resetear cache */
            last_tileX = -1;
            last_tileY = -1;
            cached_sector = NULL;
            cached_texture = NULL;
            
            for (int screenY = wall_top - 1; screenY >= 0; screenY -= step) {
                float ceilingHeight = rc->tile_size;
                
                float ratio = (ceilingHeight - eyeHeight) / (centerPlane - screenY);
                float straightDistance = screen_distance * ratio;
                float diagonalDistance = straightDistance * cosFactor;
                
                if (diagonalDistance < 0.1f || diagonalDistance > 800.0f) continue;
                
                /* Calcular posición en el mundo */
                float xEnd = (diagonalDistance * cos_ray) + cam_x;
                float yEnd = (diagonalDistance * sin_ray) + cam_y;
                
                int tileX = (int)(xEnd * inv_tile_size);
                int tileY = (int)(yEnd * inv_tile_size);
                
                /* Solo buscar sector si cambiamos de tile */
                if (tileX != last_tileX || tileY != last_tileY) {
                    cached_sector = ray_find_sector_at(rc, tileX, tileY);
                    cached_texture = (cached_sector && cached_sector->has_ceiling) ? 
                                    ray_get_texture(cached_sector->ceiling_texture) : NULL;
                    if (cached_texture) {
                        cached_tex_w = cached_texture->width;
                        cached_tex_h = cached_texture->height;
                        cached_inv_tex_w = 1.0f / cached_tex_w;
                        cached_inv_tex_h = 1.0f / cached_tex_h;
                    }
                    last_tileX = tileX;
                    last_tileY = tileY;
                }
                
                if (cached_texture) {
                    /* Calcular coordenadas de textura sin modulo */
                    float frac_x = xEnd - (tileX * rc->tile_size);
                    float frac_y = yEnd - (tileY * rc->tile_size);
                    
                    if (frac_x < 0) frac_x += rc->tile_size;
                    if (frac_y < 0) frac_y += rc->tile_size;
                    
                    int tex_x = (int)(frac_x * inv_tile_size * cached_tex_w);
                    int tex_y = (int)(frac_y * inv_tile_size * cached_tex_h);
                    
                    if (tex_x >= cached_tex_w) tex_x = cached_tex_w - 1;
                    if (tex_y >= cached_tex_h) tex_y = cached_tex_h - 1;
                    
                    uint32_t color = gr_get_pixel(cached_texture, tex_x, tex_y);
                    color = apply_fog_inline(color, diagonalDistance);
                    
                    /* Dibujar píxel y el siguiente */
                    gr_put_pixel(render_buffer, strip, screenY, color);
                    if (screenY - 1 >= 0) {
                        gr_put_pixel(render_buffer, strip, screenY - 1, color);
                    }
                } else {
                    /* Dibujar cielo */
                    gr_put_pixel(render_buffer, strip, screenY, ray_sky_color);
                    if (screenY - 1 >= 0) {
                        gr_put_pixel(render_buffer, strip, screenY - 1, ray_sky_color);
                    }
                }
            }
        }

        
        /* RENDERIZAR PAREDES usando hits pre-calculados */
        for (int i = 0; i < num_hits; i++) {
            ray_render_wall_strip(render_buffer, &column_hits[i], strip, screen_h,
                                 screen_distance, cam_z);
        }
    }
}

