/*
 * Raycaster Module Exports for BennuGD2
 */

#ifndef __LIBMOD_RAY_EXPORTS
#define __LIBMOD_RAY_EXPORTS

#include "bgddl.h"

#if defined(__BGDC__) || !defined(__STATIC__)

#include "libmod_ray.h"

/* Constantes exportadas */
DLCONSTANT __bgdexport(libmod_ray, constants_def)[] = {
    /* Tipos de ThickWall */
    {"RAY_THICK_WALL_NONE", TYPE_INT, RAY_THICK_WALL_TYPE_NONE},
    {"RAY_THICK_WALL_RECT", TYPE_INT, RAY_THICK_WALL_TYPE_RECT},
    {"RAY_THICK_WALL_TRIANGLE", TYPE_INT, RAY_THICK_WALL_TYPE_TRIANGLE},
    {"RAY_THICK_WALL_QUAD", TYPE_INT, RAY_THICK_WALL_TYPE_QUAD},
    
    /* Tipos de Slope */
    {"RAY_SLOPE_WEST_EAST", TYPE_INT, RAY_SLOPE_TYPE_WEST_EAST},
    {"RAY_SLOPE_NORTH_SOUTH", TYPE_INT, RAY_SLOPE_TYPE_NORTH_SOUTH},
    
    {NULL, 0, 0}
};

DLSYSFUNCS __bgdexport(libmod_ray, functions_exports)[] = {
    /* Inicialización y configuración del mapa */
    FUNC("RAY_CREATE_MAP", "IIIII", TYPE_INT, libmod_ray_create_map),
    FUNC("RAY_DESTROY_MAP", "", TYPE_INT, libmod_ray_destroy_map),
    FUNC("RAY_SET_CELL", "IIII", TYPE_INT, libmod_ray_set_cell),
    FUNC("RAY_GET_CELL", "III", TYPE_INT, libmod_ray_get_cell),
    
    /* Carga/Guardado de mapas (para editor) */
    FUNC("RAY_LOAD_MAP", "SI", TYPE_INT, libmod_ray_load_map),
    FUNC("RAY_UNLOAD_MAP", "", TYPE_INT, libmod_ray_unload_map),
    FUNC("RAY_SAVE_MAP", "S", TYPE_INT, libmod_ray_save_map),
    
    /* Información del mapa */
    FUNC("RAY_GET_MAP_WIDTH", "", TYPE_INT, libmod_ray_get_map_width),
    FUNC("RAY_GET_MAP_HEIGHT", "", TYPE_INT, libmod_ray_get_map_height),
    FUNC("RAY_GET_MAP_LEVELS", "", TYPE_INT, libmod_ray_get_map_levels),
    FUNC("RAY_GET_TILE_SIZE", "", TYPE_INT, libmod_ray_get_tile_size),
    
    /* Cámara */
    FUNC("RAY_SET_CAMERA", "FFFFF", TYPE_INT, libmod_ray_set_camera),
    FUNC("RAY_GET_CAMERA_X", "", TYPE_FLOAT, libmod_ray_get_camera_x),
    FUNC("RAY_GET_CAMERA_Y", "", TYPE_FLOAT, libmod_ray_get_camera_y),
    FUNC("RAY_GET_CAMERA_Z", "", TYPE_FLOAT, libmod_ray_get_camera_z),
    FUNC("RAY_GET_CAMERA_ANGLE", "", TYPE_FLOAT, libmod_ray_get_camera_angle),
    
    /* Movimiento */
    FUNC("RAY_MOVE_FORWARD", "F", TYPE_INT, libmod_ray_move_forward),
    FUNC("RAY_MOVE_BACKWARD", "F", TYPE_INT, libmod_ray_move_backward),

    FUNC("RAY_ROTATE", "F", TYPE_INT, libmod_ray_rotate),
    
    /* Renderizado */
    FUNC("RAY_RENDER", "II", TYPE_INT, libmod_ray_render),
    
    /* Sectores */
    FUNC("RAY_ADD_SECTOR", "IIII", TYPE_INT, libmod_ray_add_sector),
    FUNC("RAY_SET_SECTOR_FLOOR", "IFI", TYPE_INT, libmod_ray_set_sector_floor),
    FUNC("RAY_SET_SECTOR_CEILING", "IFII", TYPE_INT, libmod_ray_set_sector_ceiling),
    FUNC("RAY_GET_SECTOR_AT", "II", TYPE_INT, libmod_ray_get_sector_at),
    
    /* ThinWalls */
    FUNC("RAY_ADD_THIN_WALL", "FFFFFF", TYPE_INT, libmod_ray_add_thin_wall),
    FUNC("RAY_REMOVE_THIN_WALL", "I", TYPE_INT, libmod_ray_remove_thin_wall),
    FUNC("RAY_GET_THIN_WALL_COUNT", "", TYPE_INT, libmod_ray_get_thin_wall_count),
    FUNC("RAY_GET_THIN_WALL_DATA", "IPPPPPP", TYPE_INT, libmod_ray_get_thin_wall_data),
    FUNC("RAY_SET_THIN_WALL_DATA", "IFFFFFF", TYPE_INT, libmod_ray_set_thin_wall_data),
    
    /* Sprites */
    FUNC("RAY_ADD_SPRITE", "FFFIII", TYPE_INT, libmod_ray_add_sprite),
    FUNC("RAY_REMOVE_SPRITE", "I", TYPE_INT, libmod_ray_remove_sprite),
    FUNC("RAY_GET_SPRITE_COUNT", "", TYPE_INT, libmod_ray_get_sprite_count),
    FUNC("RAY_GET_SPRITE_DATA", "IPPPPPP", TYPE_INT, libmod_ray_get_sprite_data),
    FUNC("RAY_SET_SPRITE_DATA", "IFFFIII", TYPE_INT, libmod_ray_set_sprite_data),
    
    FUNC(0, 0, 0, 0)
};

#endif

/* Hooks del módulo */
void __bgdexport(libmod_ray, module_initialize)();
void __bgdexport(libmod_ray, module_finalize)();

#endif /* __LIBMOD_RAY_EXPORTS */
