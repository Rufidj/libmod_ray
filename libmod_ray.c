#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
   VARIABLES GLOBALES DEL MÓDULO
   ============================================================================ */

RAY_Raycaster* global_raycaster = NULL;
GRAPH* ray_render_buffer = NULL;
int ray_fpg_id = -1; /* -1 indica que no hay FPG asignado */

/* Cámara */
float ray_camera_x = 0.0f;
float ray_camera_y = 0.0f;
float ray_camera_z = 32.0f;
float ray_camera_angle = 0.0f;
float ray_camera_pitch = 0.0f;
float ray_camera_fov = 60.0f;

/* Configuración de suelo y techo */
int ray_floor_texture = 0;
int ray_ceiling_texture = 0;
int ray_ceiling_enabled = 1; /* 1 = techo, 0 = cielo abierto */

/* Arrays dinámicos de objetos */
RAY_ThinWall* ray_thin_walls = NULL;
int ray_num_thin_walls = 0;
int ray_thin_walls_capacity = 0;

RAY_Sprite* ray_sprites = NULL;
int ray_num_sprites = 0;
int ray_sprites_capacity = 0;

RAY_ThickWall* ray_thick_walls = NULL;
int ray_num_thick_walls = 0;
int ray_thick_walls_capacity = 0;

/* ============================================================================
   FUNCIONES EXPORTADAS - INICIALIZACIÓN
   ============================================================================ */

/**
 * RAY_CREATE_MAP(width, height, levels, tile_size, fpg_id)
 * fpg_id es opcional, si no se pasa se usa -1 (sin texturas)
 */
int64_t libmod_ray_create_map(INSTANCE *my, int64_t *params)
{
    int width = (int)params[0];
    int height = (int)params[1];
    int levels = (int)params[2];
    int tile_size = (int)params[3];
    int fpg_id = (int)params[4];
    
    /* Liberar mapa anterior si existe */
    if (global_raycaster) {
        ray_destroy_raycaster(global_raycaster);
        global_raycaster = NULL;
    }
    
    /* Crear nuevo raycaster */
    global_raycaster = ray_create_raycaster(width, height, levels, tile_size);
    
    if (!global_raycaster) {
        return 0;
    }
    
    /* Asignar FPG */
    ray_fpg_id = fpg_id;
    
    /* Inicializar arrays de objetos */
    ray_num_thin_walls = 0;
    ray_num_sprites = 0;
    
    return 1;
}

/**
 * RAY_DESTROY_MAP()
 */
int64_t libmod_ray_destroy_map(INSTANCE *my, int64_t *params)
{
    if (global_raycaster) {
        ray_destroy_raycaster(global_raycaster);
        global_raycaster = NULL;
    }
    
    /* Liberar thin walls */
    if (ray_thin_walls) {
        free(ray_thin_walls);
        ray_thin_walls = NULL;
        ray_num_thin_walls = 0;
        ray_thin_walls_capacity = 0;
    }
    
    /* Liberar sprites */
    if (ray_sprites) {
        free(ray_sprites);
        ray_sprites = NULL;
        ray_num_sprites = 0;
        ray_sprites_capacity = 0;
    }
    
    return 1;
}

/**
 * RAY_SET_CELL(x, y, z, wall_type)
 * Versión simple - solo establece el tipo de pared con forma sólida
 */
int64_t libmod_ray_set_cell(INSTANCE *my, int64_t *params)
{
    if (!global_raycaster) return 0;
    
    int x = (int)params[0];
    int y = (int)params[1];
    int z = (int)params[2];
    int wall_type = (int)params[3];
    
    /* Validar coordenadas */
    if (x < 0 || x >= global_raycaster->grid_width ||
        y < 0 || y >= global_raycaster->grid_height ||
        z < 0 || z >= global_raycaster->grid_count) {
        return 0;
    }
    
    int offset = x + y * global_raycaster->grid_width;
    global_raycaster->grids[z][offset].wall_type = wall_type;
    global_raycaster->grids[z][offset].shape = TILE_SOLID;  /* Por defecto sólida */
    global_raycaster->grids[z][offset].rotation = 0;
    
    return 1;
}

/**
 * RAY_GET_CELL(x, y, z)
 */
int64_t libmod_ray_get_cell(INSTANCE *my, int64_t *params)
{
    if (!global_raycaster) return 0;
    
    int x = (int)params[0];
    int y = (int)params[1];
    int z = (int)params[2];
    
    if (x < 0 || x >= global_raycaster->grid_width ||
        y < 0 || y >= global_raycaster->grid_height ||
        z < 0 || z >= global_raycaster->grid_count) {
        return 0;
    }
    
    int offset = x + y * global_raycaster->grid_width;
    return global_raycaster->grids[z][offset].wall_type;
}

/* ============================================================================
   FUNCIONES EXPORTADAS - CARGA/GUARDADO
   ============================================================================ */

/**
 * RAY_SAVE_MAP(filename)
 * Guarda el mapa actual en formato RAYMAP v2
 */
int64_t libmod_ray_save_map(INSTANCE *my, int64_t *params)
{
    if (!global_raycaster) return 0;
    
    const char* filename = string_get(params[0]);
    if (!filename) return 0;
    
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: No se pudo crear el archivo %s\n", filename);
        return 0;
    }
    
    /* Escribir cabecera */
    char magic[8] = "RAYMAP02";
    uint32_t version = 2;
    uint32_t grid_width = global_raycaster->grid_width;
    uint32_t grid_height = global_raycaster->grid_height;
    uint32_t grid_count = global_raycaster->grid_count;
    uint32_t tile_size = global_raycaster->tile_size;
    uint32_t num_sectors = 0;  /* TODO: Cuando se implemente el sistema de sectores */
    uint32_t num_thin_walls = ray_num_thin_walls;
    uint32_t num_sprites = ray_num_sprites;
    
    fwrite(magic, 8, 1, f);
    fwrite(&version, sizeof(uint32_t), 1, f);
    fwrite(&grid_width, sizeof(uint32_t), 1, f);
    fwrite(&grid_height, sizeof(uint32_t), 1, f);
    fwrite(&grid_count, sizeof(uint32_t), 1, f);
    fwrite(&tile_size, sizeof(uint32_t), 1, f);
    fwrite(&num_sectors, sizeof(uint32_t), 1, f);
    fwrite(&num_thin_walls, sizeof(uint32_t), 1, f);
    fwrite(&num_sprites, sizeof(uint32_t), 1, f);
    
    /* Escribir grids (formato v2: GridCell completo) */
    int grid_size = grid_width * grid_height;
    for (int z = 0; z < grid_count; z++) {
        fwrite(global_raycaster->grids[z], sizeof(GridCell), grid_size, f);
    }
    
    /* Escribir sectores */
    /* TODO: Cuando se implemente el sistema de sectores */
    
    /* Escribir ThinWalls */
    if (num_thin_walls > 0) {
        fwrite(ray_thin_walls, sizeof(RAY_ThinWall), num_thin_walls, f);
    }
    
    /* Escribir Sprites */
    if (num_sprites > 0) {
        fwrite(ray_sprites, sizeof(RAY_Sprite), num_sprites, f);
    }
    
    fclose(f);
    
    return 1;
}

/**
 * RAY_LOAD_MAP(filename, fpg_id)
 * Carga un mapa en formato RAYMAP v2 y asigna el FPG de texturas
 */
int64_t libmod_ray_load_map(INSTANCE *my, int64_t *params)
{
    const char* filename = string_get(params[0]);
    int fpg_id = (int)params[1];
    
    if (!filename) return 0;
    
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: No se pudo abrir el archivo %s\n", filename);
        return 0;
    }
    
    /* Leer cabecera */
    char magic[9] = {0};
    if (fread(magic, 8, 1, f) != 1) {
        fclose(f);
        return 0;
    }
    
    /* Verificar magic number */
    if (strcmp(magic, "RAYMAP02") != 0 && strcmp(magic, "RAYMAP01") != 0) {
        fprintf(stderr, "ERROR: Formato de archivo inválido\n");
        fclose(f);
        return 0;
    }
    
    int is_v2 = (strcmp(magic, "RAYMAP02") == 0);
    
    uint32_t version, grid_width, grid_height, grid_count, tile_size;
    uint32_t num_sectors = 0, num_thin_walls, num_sprites;
    
    fread(&version, sizeof(uint32_t), 1, f);
    fread(&grid_width, sizeof(uint32_t), 1, f);
    fread(&grid_height, sizeof(uint32_t), 1, f);
    fread(&grid_count, sizeof(uint32_t), 1, f);
    fread(&tile_size, sizeof(uint32_t), 1, f);
    
    if (is_v2) {
        fread(&num_sectors, sizeof(uint32_t), 1, f);
    }
    
    fread(&num_thin_walls, sizeof(uint32_t), 1, f);
    fread(&num_sprites, sizeof(uint32_t), 1, f);
    
    /* Liberar mapa anterior si existe */
    if (global_raycaster) {
        ray_destroy_raycaster(global_raycaster);
        global_raycaster = NULL;
    }
    
    /* Crear nuevo raycaster */
    global_raycaster = ray_create_raycaster(grid_width, grid_height, 
                                           grid_count, tile_size);
    if (!global_raycaster) {
        fclose(f);
        return 0;
    }
    
    /* Asignar FPG */
    ray_fpg_id = fpg_id;
    
    /* Leer grids */
    int grid_size = grid_width * grid_height;
    for (int z = 0; z < grid_count; z++) {
        if (is_v2) {
            /* Formato v2: GridCell completo */
            fread(global_raycaster->grids[z], sizeof(GridCell), grid_size, f);
        } else {
            /* Formato v1: solo int32_t, convertir a GridCell */
            int32_t* temp_grid = (int32_t*)malloc(sizeof(int32_t) * grid_size);
            fread(temp_grid, sizeof(int32_t), grid_size, f);
            
            for (int i = 0; i < grid_size; i++) {
                global_raycaster->grids[z][i].wall_type = temp_grid[i];
                global_raycaster->grids[z][i].shape = TILE_SOLID;
                global_raycaster->grids[z][i].rotation = 0;
                global_raycaster->grids[z][i].flags = 0;
                global_raycaster->grids[z][i].reserved = 0;
            }
            
            free(temp_grid);
        }
    }
    
    /* Leer sectores (solo en v2) */
    if (is_v2 && num_sectors > 0) {
        /* TODO: Implementar lectura de sectores cuando se agregue el sistema */
        fseek(f, sizeof(RAY_Sector) * num_sectors, SEEK_CUR);
    }
    
    /* Leer ThinWalls */
    if (num_thin_walls > 0) {
        if (ray_thin_walls) {
            free(ray_thin_walls);
        }
        
        ray_thin_walls = (RAY_ThinWall*)malloc(sizeof(RAY_ThinWall) * num_thin_walls);
        ray_thin_walls_capacity = num_thin_walls;
        ray_num_thin_walls = num_thin_walls;
        
        fread(ray_thin_walls, sizeof(RAY_ThinWall), num_thin_walls, f);
    }
    
    /* Leer Sprites */
    if (num_sprites > 0) {
        if (ray_sprites) {
            free(ray_sprites);
        }
        
        ray_sprites = (RAY_Sprite*)malloc(sizeof(RAY_Sprite) * num_sprites);
        ray_sprites_capacity = num_sprites;
        ray_num_sprites = num_sprites;
        
        fread(ray_sprites, sizeof(RAY_Sprite), num_sprites, f);
    }
    
    fclose(f);
    
    return 1;
}

/**
 * RAY_UNLOAD_MAP()
 * Libera toda la memoria del mapa actual
 */
int64_t libmod_ray_unload_map(INSTANCE *my, int64_t *params)
{
    /* Liberar raycaster */
    if (global_raycaster) {
        ray_destroy_raycaster(global_raycaster);
        global_raycaster = NULL;
    }
    
    /* Liberar thin walls */
    if (ray_thin_walls) {
        free(ray_thin_walls);
        ray_thin_walls = NULL;
        ray_num_thin_walls = 0;
        ray_thin_walls_capacity = 0;
    }
    
    /* Liberar sprites */
    if (ray_sprites) {
        free(ray_sprites);
        ray_sprites = NULL;
        ray_num_sprites = 0;
        ray_sprites_capacity = 0;
    }
    
    /* Resetear FPG */
    ray_fpg_id = -1;
    
    return 1;
}

/* ============================================================================
   FUNCIONES EXPORTADAS - INFORMACIÓN DEL MAPA
   ============================================================================ */

int64_t libmod_ray_get_map_width(INSTANCE *my, int64_t *params)
{
    return global_raycaster ? global_raycaster->grid_width : 0;
}

int64_t libmod_ray_get_map_height(INSTANCE *my, int64_t *params)
{
    return global_raycaster ? global_raycaster->grid_height : 0;
}

int64_t libmod_ray_get_map_levels(INSTANCE *my, int64_t *params)
{
    return global_raycaster ? global_raycaster->grid_count : 0;
}

int64_t libmod_ray_get_tile_size(INSTANCE *my, int64_t *params)
{
    return global_raycaster ? global_raycaster->tile_size : 0;
}

/* ============================================================================
   FUNCIONES EXPORTADAS - CÁMARA
   ============================================================================ */

int64_t libmod_ray_set_camera(INSTANCE *my, int64_t *params)
{
    /* Convertir de int64_t a float usando punteros */
    ray_camera_x = *(float*)&params[0];
    ray_camera_y = *(float*)&params[1];
    ray_camera_z = *(float*)&params[2];
    ray_camera_angle = *(float*)&params[3];
    ray_camera_fov = *(float*)&params[4];
    
    return 1;
}

int64_t libmod_ray_get_camera_x(INSTANCE *my, int64_t *params)
{
    return *(int64_t*)&ray_camera_x;
}

int64_t libmod_ray_get_camera_y(INSTANCE *my, int64_t *params)
{
    return *(int64_t*)&ray_camera_y;
}

int64_t libmod_ray_get_camera_z(INSTANCE *my, int64_t *params)
{
    return *(int64_t*)&ray_camera_z;
}

int64_t libmod_ray_get_camera_angle(INSTANCE *my, int64_t *params)
{
    return *(int64_t*)&ray_camera_angle;
}

/* ============================================================================
   FUNCIONES EXPORTADAS - MOVIMIENTO
   ============================================================================ */

int64_t libmod_ray_move_forward(INSTANCE *my, int64_t *params)
{
    float speed = *(float*)&params[0];
    ray_camera_x += cosf(ray_camera_angle) * speed;
    ray_camera_y += sinf(ray_camera_angle) * speed;
    return 1;
}

int64_t libmod_ray_move_backward(INSTANCE *my, int64_t *params)
{
    float speed = *(float*)&params[0];
    ray_camera_x -= cosf(ray_camera_angle) * speed;
    ray_camera_y -= sinf(ray_camera_angle) * speed;
    return 1;
}


int64_t libmod_ray_rotate(INSTANCE *my, int64_t *params)
{
    float delta = *(float*)&params[0];
    ray_camera_angle += delta;
    
    /* Normalizar ángulo */
    while (ray_camera_angle < 0) ray_camera_angle += 2.0f * M_PI;
    while (ray_camera_angle >= 2.0f * M_PI) ray_camera_angle -= 2.0f * M_PI;
    
    return 1;
}

/* ============================================================================
   FUNCIONES EXPORTADAS - RENDERIZADO
   ============================================================================ */

int64_t libmod_ray_render(INSTANCE *my, int64_t *params)
{
    if (!global_raycaster) return 0;
    
    int screen_w = (int)params[0];
    int screen_h = (int)params[1];
    
    /* Crear o reutilizar buffer de renderizado */
    static GRAPH* last_render_buffer = NULL;
    static int last_w = 0;
    static int last_h = 0;
    
    /* Si cambió el tamaño o no existe, crear nuevo buffer */
    if (last_w != screen_w || last_h != screen_h || last_render_buffer == NULL) {
        if (last_render_buffer != NULL) {
            /* Liberar buffer anterior si existe */
            bitmap_destroy(last_render_buffer);
        }
        last_render_buffer = bitmap_new_syslib(screen_w, screen_h);
        last_w = screen_w;
        last_h = screen_h;
    }
    
    if (last_render_buffer == NULL) {
        fprintf(stderr, "ERROR: No se pudo crear el buffer de renderizado\n");
        return 0;
    }
    
    /* Renderizar frame */
    ray_render_frame(global_raycaster, last_render_buffer,
                    ray_camera_x, ray_camera_y, ray_camera_z,
                    ray_camera_angle, ray_camera_pitch, ray_camera_fov,
                    screen_w, screen_h);
    
    /* Devolver el ID del gráfico renderizado */
    return last_render_buffer->code;
}

/* ============================================================================
   FUNCIONES EXPORTADAS - THIN WALLS
   ============================================================================ */

int64_t libmod_ray_add_thin_wall(INSTANCE *my, int64_t *params)
{
    /* Expandir array si es necesario */
    if (ray_num_thin_walls >= ray_thin_walls_capacity) {
        int new_capacity = ray_thin_walls_capacity == 0 ? 64 : ray_thin_walls_capacity * 2;
        RAY_ThinWall* new_array = (RAY_ThinWall*)realloc(ray_thin_walls, 
                                                          sizeof(RAY_ThinWall) * new_capacity);
        if (!new_array) return -1;
        ray_thin_walls = new_array;
        ray_thin_walls_capacity = new_capacity;
    }
    
    RAY_ThinWall* tw = &ray_thin_walls[ray_num_thin_walls];
    memset(tw, 0, sizeof(RAY_ThinWall));
    
    tw->x1 = *(float*)&params[0];
    tw->y1 = *(float*)&params[1];
    tw->x2 = *(float*)&params[2];
    tw->y2 = *(float*)&params[3];
    tw->wall_type = (int)params[4];
    tw->height = *(float*)&params[5];
    tw->z = 0;
    tw->hidden = 0;
    
    return ray_num_thin_walls++;
}

int64_t libmod_ray_remove_thin_wall(INSTANCE *my, int64_t *params)
{
    int index = (int)params[0];
    if (index < 0 || index >= ray_num_thin_walls) return 0;
    
    /* Mover el último elemento a la posición eliminada */
    if (index < ray_num_thin_walls - 1) {
        ray_thin_walls[index] = ray_thin_walls[ray_num_thin_walls - 1];
    }
    ray_num_thin_walls--;
    
    return 1;
}

int64_t libmod_ray_get_thin_wall_count(INSTANCE *my, int64_t *params)
{
    return ray_num_thin_walls;
}

int64_t libmod_ray_get_thin_wall_data(INSTANCE *my, int64_t *params)
{
    int index = (int)params[0];
    if (index < 0 || index >= ray_num_thin_walls) return 0;
    
    RAY_ThinWall* tw = &ray_thin_walls[index];
    
    /* Escribir datos a punteros proporcionados */
    if (params[1]) *(float*)params[1] = tw->x1;
    if (params[2]) *(float*)params[2] = tw->y1;
    if (params[3]) *(float*)params[3] = tw->x2;
    if (params[4]) *(float*)params[4] = tw->y2;
    if (params[5]) *(int*)params[5] = tw->wall_type;
    if (params[6]) *(float*)params[6] = tw->height;
    
    return 1;
}

int64_t libmod_ray_set_thin_wall_data(INSTANCE *my, int64_t *params)
{
    int index = (int)params[0];
    if (index < 0 || index >= ray_num_thin_walls) return 0;
    
    RAY_ThinWall* tw = &ray_thin_walls[index];
    tw->x1 = *(float*)&params[1];
    tw->y1 = *(float*)&params[2];
    tw->x2 = *(float*)&params[3];
    tw->y2 = *(float*)&params[4];
    tw->wall_type = (int)params[5];
    tw->height = *(float*)&params[6];
    
    return 1;
}

/* ============================================================================
   FUNCIONES EXPORTADAS - SPRITES
   ============================================================================ */

int64_t libmod_ray_add_sprite(INSTANCE *my, int64_t *params)
{
    /* Expandir array si es necesario */
    if (ray_num_sprites >= ray_sprites_capacity) {
        int new_capacity = ray_sprites_capacity == 0 ? 64 : ray_sprites_capacity * 2;
        RAY_Sprite* new_array = (RAY_Sprite*)realloc(ray_sprites, 
                                                      sizeof(RAY_Sprite) * new_capacity);
        if (!new_array) return -1;
        ray_sprites = new_array;
        ray_sprites_capacity = new_capacity;
    }
    
    RAY_Sprite* sprite = &ray_sprites[ray_num_sprites];
    memset(sprite, 0, sizeof(RAY_Sprite));
    
    sprite->x = *(float*)&params[0];
    sprite->y = *(float*)&params[1];
    sprite->z = *(float*)&params[2];
    sprite->texture_id = (int)params[3];
    sprite->w = (int)params[4];
    sprite->h = (int)params[5];
    sprite->hidden = 0;
    
    return ray_num_sprites++;
}

int64_t libmod_ray_remove_sprite(INSTANCE *my, int64_t *params)
{
    int index = (int)params[0];
    if (index < 0 || index >= ray_num_sprites) return 0;
    
    /* Mover el último elemento a la posición eliminada */
    if (index < ray_num_sprites - 1) {
        ray_sprites[index] = ray_sprites[ray_num_sprites - 1];
    }
    ray_num_sprites--;
    
    return 1;
}

int64_t libmod_ray_get_sprite_count(INSTANCE *my, int64_t *params)
{
    return ray_num_sprites;
}

int64_t libmod_ray_get_sprite_data(INSTANCE *my, int64_t *params)
{
    int index = (int)params[0];
    if (index < 0 || index >= ray_num_sprites) return 0;
    
    RAY_Sprite* sprite = &ray_sprites[index];
    
    /* Escribir datos a punteros proporcionados */
    if (params[1]) *(float*)params[1] = sprite->x;
    if (params[2]) *(float*)params[2] = sprite->y;
    if (params[3]) *(float*)params[3] = sprite->z;
    if (params[4]) *(int*)params[4] = sprite->texture_id;
    if (params[5]) *(int*)params[5] = sprite->w;
    if (params[6]) *(int*)params[6] = sprite->h;
    
    return 1;
}

int64_t libmod_ray_set_sprite_data(INSTANCE *my, int64_t *params)
{
    int index = (int)params[0];
    if (index < 0 || index >= ray_num_sprites) return 0;
    
    RAY_Sprite* sprite = &ray_sprites[index];
    sprite->x = *(float*)&params[1];
    sprite->y = *(float*)&params[2];
    sprite->z = *(float*)&params[3];
    sprite->texture_id = (int)params[4];
    sprite->w = (int)params[5];
    sprite->h = (int)params[6];
    
    return 1;
}

/* ============================================================================
   HOOKS DEL MÓDULO
   ============================================================================ */

void __bgdexport(libmod_ray, module_initialize)()
{
    /* Módulo inicializado */
}

void __bgdexport(libmod_ray, module_finalize)()
{
    /* Limpiar recursos */
    if (global_raycaster) {
        ray_destroy_raycaster(global_raycaster);
        global_raycaster = NULL;
    }
    
    if (ray_thin_walls) {
        free(ray_thin_walls);
        ray_thin_walls = NULL;
    }
    
    if (ray_sprites) {
        free(ray_sprites);
        ray_sprites = NULL;
    }
}

#include "libmod_ray_exports.h"

