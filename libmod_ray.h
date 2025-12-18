#ifndef __LIBMOD_RAY_H
#define __LIBMOD_RAY_H

#include <stdint.h>
#include <math.h>

/* Inclusiones necesarias de BennuGD2 */
#include "bgddl.h"
#include "libbggfx.h"
#include "g_bitmap.h"
#include "g_blit.h"
#include "g_pixel.h"
#include "g_clear.h"
#include "g_grlib.h"
#include "xstrings.h"

/* Constantes */
#define RAY_MAX_THIN_WALLS 2048
#define RAY_MAX_SPRITES 1024
#define RAY_MAX_THICK_WALLS 512

#define RAY_THICK_WALL_TYPE_NONE 0
#define RAY_THICK_WALL_TYPE_RECT 1
#define RAY_THICK_WALL_TYPE_TRIANGLE 2
#define RAY_THICK_WALL_TYPE_QUAD 3

#define RAY_SLOPE_TYPE_WEST_EAST 1
#define RAY_SLOPE_TYPE_NORTH_SOUTH 2

/* Estructura de punto simple */
typedef struct {
    float x, y;
} RAY_Point;

/* Forward declarations */
struct RAY_ThickWall;

/* ThinWall - Pared delgada (linedef estilo Doom) */
typedef struct {
    float x1, y1, x2, y2;
    int wall_type;
    int horizontal;
    float height;
    float z;
    float slope;
    int hidden;
    struct RAY_ThickWall* thick_wall;
} RAY_ThinWall;

/* ThickWall - Área cerrada con suelo/techo */
typedef struct RAY_ThickWall {
    int type;
    int slope_type;
    RAY_ThinWall* thin_walls;
    int num_thin_walls;
    int capacity_thin_walls;
    
    /* Para THICK_WALL_TYPE_RECT */
    float x, y, w, h;
    
    /* Para THICK_WALL_TYPE_TRIANGLE y QUAD */
    RAY_Point* points;
    int num_points;
    
    float slope;
    int ceiling_texture_id;
    int floor_texture_id;
    float start_height, end_height;
    float taller_height;
    int inverted_slope;
    
    float height;
    float z;
} RAY_ThickWall;

/* Sprite - Billboard */
typedef struct {
    float x, y, z;
    int w, h;
    int level;
    int dir;
    float rot;
    int speed;
    int move_speed;
    float rot_speed;
    float distance;
    int texture_id;
    int cleanup;
    int frame_rate;
    int frame;
    int hidden;
    int jumping;
    float height_jumped;
    int rayhit;
} RAY_Sprite;

/* ============================================================================
   CONSTANTES DE FORMAS DE TILES
   ============================================================================ */

/* Formas de tiles */
#define TILE_EMPTY          0   /* Vacío (sin colisión) */
#define TILE_SOLID          1   /* Cuadrado completo (pared sólida) */
#define TILE_DIAGONAL_NE    2   /* Diagonal ↗ (NorEste-SurOeste) */
#define TILE_DIAGONAL_NW    3   /* Diagonal ↖ (NorOeste-SurEste) */
#define TILE_HALF_NORTH     4   /* Media pared norte */
#define TILE_HALF_SOUTH     5   /* Media pared sur */
#define TILE_HALF_EAST      6   /* Media pared este */
#define TILE_HALF_WEST      7   /* Media pared oeste */
#define TILE_CORNER_NE      8   /* Esquina noreste */
#define TILE_CORNER_NW      9   /* Esquina noroeste */
#define TILE_CORNER_SE      10  /* Esquina sureste */
#define TILE_CORNER_SW      11  /* Esquina suroeste */

/**
 * GridCell - Celda del grid con forma y rotación
 */
typedef struct {
    int32_t wall_type;    /* ID de textura (0 = vacío, >0 = textura) */
    uint8_t shape;        /* Forma de la tile (TILE_SOLID, TILE_DIAGONAL_NE, etc.) */
    uint8_t rotation;     /* Rotación: 0=0°, 1=90°, 2=180°, 3=270° */
    uint8_t flags;        /* Flags adicionales (reservado) */
    uint8_t reserved;     /* Reservado para uso futuro */
} GridCell;

/**
 * Portal - Conexión entre dos sectores para renderizado optimizado
 * Permite ver de un sector a otro con clipping progresivo
 */
typedef struct {
    int32_t portal_id;         /* ID único del portal */
    int32_t from_sector;       /* ID del sector origen */
    int32_t to_sector;         /* ID del sector destino */
    
    /* Geometría del portal en coordenadas de mundo */
    float x1, y1;              /* Punto inicial del portal */
    float x2, y2;              /* Punto final del portal */
    float bottom_z;            /* Altura inferior del portal */
    float top_z;               /* Altura superior del portal */
    
    /* Flags */
    int32_t bidirectional;     /* 1 = se puede ver en ambas direcciones, 0 = solo from→to */
    int32_t enabled;           /* 1 = activo, 0 = cerrado/invisible */
} RAY_Portal;

/**
 * ClipWindow - Ventana de clipping para renderizado recursivo de portales
 * Define qué columnas y filas son visibles a través de un portal
 */
typedef struct {
    int x_min;                 /* Columna mínima visible (inclusive) */
    int x_max;                 /* Columna máxima visible (inclusive) */
    float* y_top;              /* Array[screen_w] límite superior por columna */
    float* y_bottom;           /* Array[screen_w] límite inferior por columna */
    int screen_w;              /* Ancho de pantalla (para validación) */
    int screen_h;              /* Alto de pantalla (para validación) */
} RAY_ClipWindow;

/**
 * Sector - Define un área del mapa con propiedades de suelo/techo
 */
typedef struct {
    uint32_t sector_id;        /* ID único del sector */
    
    /* Bounds del sector en el grid */
    int32_t min_x, min_y;      /* Esquina mínima */
    int32_t max_x, max_y;      /* Esquina máxima */
    
    /* Alturas */
    float floor_height;        /* Altura del suelo (ej: 0.0) */
    float ceiling_height;      /* Altura del techo (ej: 128.0) */
    int32_t has_ceiling;       /* 1 = tiene techo, 0 = cielo abierto */
    
    /* Texturas */
    int32_t floor_texture;     /* ID de textura del suelo */
    int32_t ceiling_texture;   /* ID de textura del techo */
    
    /* Iluminación */
    float light_level;         /* 0.0 (oscuro) a 1.0 (brillante) */
    
    /* Portales */
    int32_t* portal_ids;       /* Array de IDs de portales en este sector */
    int32_t num_portals;       /* Número de portales */
    int32_t portals_capacity;  /* Capacidad del array */
} RAY_Sector;


/* RayHit - Información de colisión de un rayo */
typedef struct {
    float x, y;
    int wall_x, wall_y;
    int wall_type;
    int strip;
    float tile_x;
    float squared_distance;
    float distance;
    float correct_distance;
    int horizontal;
    float ray_angle;
    RAY_Sprite* sprite;
    int level;
    int right;
    int up;
    RAY_ThinWall* thin_wall;
    float wall_height;
    float inverted_z;
    
    /* Sibling para slopes */
    float sibling_wall_height;
    float sibling_distance;
    float sibling_correct_distance;
    float sibling_thin_wall_z;
    float sibling_inverted_z;
    
    float sort_distance;
} RAY_RayHit;

/**
 * Raycaster - Estructura principal del motor
 */
typedef struct {
    GridCell** grids;          /* Array de grids (uno por nivel Z) */
    int grid_width;            /* Ancho del grid */
    int grid_height;           /* Alto del grid */
    int grid_count;            /* Número de niveles */
    int tile_size;             /* Tamaño de cada tile */
    
    /* Array dinámico de sectores */
    RAY_Sector* sectors;       /* Array de sectores */
    int num_sectors;           /* Número de sectores actuales */
    int sectors_capacity;      /* Capacidad del array */
    
    /* Array dinámico de portales */
    RAY_Portal* portals;       /* Array de portales */
    int num_portals;           /* Número de portales actuales */
    int portals_capacity;      /* Capacidad del array */
} RAY_Raycaster;


/**
 * FrameCache - Caché de pre-cálculo para renderizado optimizado
 * Almacena todos los raycast results y Z-buffer para evitar cálculos redundantes
 */
typedef struct {
    /* Raycast results por columna */
    RAY_RayHit* column_hits;       /* Array [screen_w * MAX_HITS_PER_COLUMN] */
    int* column_hit_counts;        /* Array [screen_w] número de hits por columna */
    int max_hits_per_column;       /* Máximo de hits por columna (ej: 16) */
    
    /* Z-buffer para oclusión */
    float* z_buffer;               /* Array [screen_w * screen_h] profundidad por píxel */
    int* wall_top;                 /* Array [screen_w] Y superior de pared por columna */
    int* wall_bottom;              /* Array [screen_w] Y inferior de pared por columna */
    
    /* Sectores visibles */
    int* visible_sectors;          /* Array de IDs de sectores visibles */
    int num_visible_sectors;       /* Número de sectores visibles */
    int max_visible_sectors;       /* Capacidad del array */
    
    /* Dimensiones */
    int screen_w;
    int screen_h;
    
    /* Estado de validez */
    int valid;                     /* 1 si el caché es válido, 0 si necesita recalcularse */
} RAY_FrameCache;


/* Variables globales exportadas */
extern RAY_Raycaster* global_raycaster;
extern GRAPH* ray_render_buffer;
extern int ray_fpg_id;
extern int ray_floor_texture;
extern int ray_ceiling_texture;
extern int ray_ceiling_enabled;

/* Cámara */
extern float ray_camera_x;
extern float ray_camera_y;
extern float ray_camera_z;
extern float ray_camera_angle;
extern float ray_camera_pitch;
extern float ray_camera_fov;

/* Arrays dinámicos de objetos */
extern RAY_ThinWall* ray_thin_walls;
extern int ray_num_thin_walls;
extern int ray_thin_walls_capacity;

extern RAY_Sprite* ray_sprites;
extern int ray_num_sprites;
extern int ray_sprites_capacity;

extern RAY_ThickWall* ray_thick_walls;
extern int ray_num_thick_walls;
extern int ray_thick_walls_capacity;

/* Configuración de renderizado */
extern float ray_fog_start;
extern float ray_fog_end;
extern uint32_t ray_fog_color;
extern uint32_t ray_sky_color;

/* ============================================================================
   FUNCIONES DE GEOMETRÍA (libmod_ray_shape.c)
   ============================================================================ */

int ray_lines_intersect(float x1, float y1, float x2, float y2,
                        float x3, float y3, float x4, float y4,
                        float* ix, float* iy);

int ray_point_in_rect(float ptx, float pty, float x, float y, float w, float h);

float ray_sign(float p1x, float p1y, float p2x, float p2y, float p3x, float p3y);

int ray_point_in_triangle(float ptx, float pty,
                          float v1x, float v1y,
                          float v2x, float v2y,
                          float v3x, float v3y);

int ray_point_in_quad(float ptx, float pty,
                      float v1x, float v1y,
                      float v2x, float v2y,
                      float v3x, float v3y,
                      float v4x, float v4y);

/* ============================================================================
   FUNCIONES DE PORTALES (libmod_ray_portals.c)
   ============================================================================ */

/* Gestión de portales */
int ray_add_portal(RAY_Raycaster* rc,
                   int from_sector, int to_sector,
                   float x1, float y1, float x2, float y2,
                   float bottom_z, float top_z,
                   int bidirectional);

void ray_remove_portal(RAY_Raycaster* rc, int portal_id);
void ray_enable_portal(RAY_Raycaster* rc, int portal_id, int enabled);
RAY_Portal* ray_get_portal(RAY_Raycaster* rc, int portal_id);

/* Gestión de ClipWindow */
RAY_ClipWindow* ray_create_clip_window(int screen_w, int screen_h);
void ray_destroy_clip_window(RAY_ClipWindow* window);
void ray_reset_clip_window(RAY_ClipWindow* window);

/* Detección automática */
void ray_detect_portals_automatic(RAY_Raycaster* rc);

/* Proyección de portales */
int ray_project_portal(RAY_Portal* portal, 
                       float cam_x, float cam_y, float cam_angle,
                       float screen_distance, int screen_w, int screen_h,
                       RAY_ClipWindow* parent_clip,
                       RAY_ClipWindow* out_clip);

void ray_clip_window_intersect(RAY_ClipWindow* a, RAY_ClipWindow* b, RAY_ClipWindow* out);

/* Renderizado recursivo */
int ray_find_camera_sector(RAY_Raycaster* rc, float cam_x, float cam_y);
void ray_render_sector_recursive(RAY_Raycaster* rc, GRAPH* render_buffer,
                                 int sector_id,
                                 float cam_x, float cam_y, float cam_z,
                                 float cam_angle, float cam_pitch, float fov,
                                 int screen_w, int screen_h,
                                 RAY_ClipWindow* clip_window);
void ray_render_with_portals(RAY_Raycaster* rc, GRAPH* render_buffer,
                             float cam_x, float cam_y, float cam_z,
                             float cam_angle, float cam_pitch, float fov,
                             int screen_w, int screen_h);

/* ============================================================================
   FUNCIONES DE RAYCASTING (libmod_ray_raycasting.c)
   ============================================================================ */


RAY_Raycaster* ray_create_raycaster(int grid_width, int grid_height, 
                                    int grid_count, int tile_size);
void ray_destroy_raycaster(RAY_Raycaster* rc);

float ray_screen_distance(float screen_width, float fov_radians);
float ray_strip_angle(float screen_x, float screen_distance);
float ray_strip_screen_height(float screen_distance, float correct_distance, 
                              float tile_size);

void ray_raycast(RAY_Raycaster* rc, RAY_RayHit* hits, int* num_hits,
                float player_x, float player_y, float player_z,
                float player_rot, float strip_angle, int strip_idx,
                RAY_Sprite* sprites, int num_sprites);

void ray_raycast_thin_walls(RAY_RayHit* hits, int* num_hits,
                            RAY_ThinWall* thin_walls, int num_thin_walls,
                            float player_x, float player_y, float player_z,
                            float player_rot, float strip_angle, int strip_idx);

void ray_find_intersecting_thin_walls(RAY_RayHit* hits, int* num_hits,
                                     RAY_ThinWall* thin_walls, int num_thin_walls,
                                     float player_x, float player_y,
                                     float ray_end_x, float ray_end_y);

/* ============================================================================
   FUNCIONES DE RENDERIZADO (libmod_ray_render.c)
   ============================================================================ */

void ray_render_frame(RAY_Raycaster* rc, GRAPH* render_buffer,
                     float cam_x, float cam_y, float cam_z,
                     float cam_angle, float cam_pitch, float fov,
                     int screen_w, int screen_h);

void ray_render_wall_strip(GRAPH* buffer, RAY_RayHit* hit,
                           int strip, int screen_h,
                           float screen_distance, float cam_z);

void ray_render_floor_ceiling(GRAPH* buffer, RAY_Raycaster* rc,
                               int screen_w, int screen_h,
                               float cam_x, float cam_y, float cam_z,
                               float cam_angle, float cam_fov);

void ray_render_sprites(GRAPH* buffer, RAY_Sprite* sprites,
                       int num_sprites, float cam_x, float cam_y,
                       float cam_z, float cam_angle, float screen_distance,
                       int screen_w, int screen_h);

uint32_t ray_apply_fog(uint32_t color, float distance);

/* Frame Cache - Pre-cálculo optimizado */
RAY_FrameCache* ray_create_frame_cache(int screen_w, int screen_h, int max_hits_per_column);
void ray_destroy_frame_cache(RAY_FrameCache* cache);
void ray_precalculate_frame(RAY_Raycaster* rc, RAY_FrameCache* cache,
                            float cam_x, float cam_y, float cam_z,
                            float cam_angle, float fov);
void ray_build_zbuffer(RAY_FrameCache* cache, float screen_distance, float cam_z, int tile_size);


/* ============================================================================
   FUNCIONES DE SECTORES (libmod_ray_sectors.c)
   ============================================================================ */

RAY_Sector* ray_find_sector_at(RAY_Raycaster* rc, int grid_x, int grid_y);

/* ============================================================================
   FUNCIONES EXPORTADAS A BENNUGD2 (libmod_ray.c)
   ============================================================================ */

/* Inicialización */
int64_t libmod_ray_create_map(INSTANCE *my, int64_t *params);
int64_t libmod_ray_destroy_map(INSTANCE *my, int64_t *params);
int64_t libmod_ray_set_cell(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_cell(INSTANCE *my, int64_t *params);

/* Carga/Guardado */
int64_t libmod_ray_load_map(INSTANCE *my, int64_t *params);
int64_t libmod_ray_save_map(INSTANCE *my, int64_t *params);

/* Información del mapa */
int64_t libmod_ray_get_map_width(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_map_height(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_map_levels(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_tile_size(INSTANCE *my, int64_t *params);

/* Cámara */
int64_t libmod_ray_set_camera(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_camera_x(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_camera_y(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_camera_z(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_camera_angle(INSTANCE *my, int64_t *params);

/* Movimiento */
int64_t libmod_ray_move_forward(INSTANCE *my, int64_t *params);
int64_t libmod_ray_move_backward(INSTANCE *my, int64_t *params);

int64_t libmod_ray_rotate(INSTANCE *my, int64_t *params);

/* Renderizado */
int64_t libmod_ray_render(INSTANCE *my, int64_t *params);

/* ThinWalls */
int64_t libmod_ray_add_thin_wall(INSTANCE *my, int64_t *params);
int64_t libmod_ray_remove_thin_wall(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_thin_wall_count(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_thin_wall_data(INSTANCE *my, int64_t *params);
int64_t libmod_ray_set_thin_wall_data(INSTANCE *my, int64_t *params);

/* Sprites */
int64_t libmod_ray_add_sprite(INSTANCE *my, int64_t *params);
int64_t libmod_ray_remove_sprite(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_sprite_count(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_sprite_data(INSTANCE *my, int64_t *params);
int64_t libmod_ray_set_sprite_data(INSTANCE *my, int64_t *params);

/* Sectores */
int64_t libmod_ray_add_sector(INSTANCE *my, int64_t *params);
int64_t libmod_ray_set_sector_floor(INSTANCE *my, int64_t *params);
int64_t libmod_ray_set_sector_ceiling(INSTANCE *my, int64_t *params);
int64_t libmod_ray_get_sector_at(INSTANCE *my, int64_t *params);

/* Hooks del módulo */
void __bgdexport(libmod_ray, module_initialize)();
void __bgdexport(libmod_ray, module_finalize)();

#endif /* __LIBMOD_RAY_H */
