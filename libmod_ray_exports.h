/*
 * libmod_ray - Raycasting Module Exports for BennuGD2
 * Port of Andrew Lim's SDL2 Raycasting Engine
 */

#ifndef __LIBMOD_RAY_EXPORTS
#define __LIBMOD_RAY_EXPORTS

#include "bgddl.h"

#if defined(__BGDC__) || !defined(__STATIC__)

#include "libmod_ray.h"

/* Constantes exportadas */
DLCONSTANT __bgdexport(libmod_ray, constants_def)[] = {
    /* Tipos de ThickWall */
    {"RAY_THICK_WALL_RECT", TYPE_INT, RAY_THICK_WALL_TYPE_RECT},
    {"RAY_THICK_WALL_TRIANGLE", TYPE_INT, RAY_THICK_WALL_TYPE_TRIANGLE},
    {"RAY_THICK_WALL_QUAD", TYPE_INT, RAY_THICK_WALL_TYPE_QUAD},
    
    /* Tipos de Slope */
    {"RAY_SLOPE_WEST_EAST", TYPE_INT, RAY_SLOPE_TYPE_WEST_EAST},
    {"RAY_SLOPE_NORTH_SOUTH", TYPE_INT, RAY_SLOPE_TYPE_NORTH_SOUTH},
    
    /* Constantes del motor */
    {"RAY_TILE_SIZE", TYPE_INT, RAY_TILE_SIZE},
    {"RAY_TEXTURE_SIZE", TYPE_INT, RAY_TEXTURE_SIZE},
    
    {NULL, 0, 0}
};

DLSYSFUNCS __bgdexport(libmod_ray, functions_exports)[] = {
    /* Inicialización */
    FUNC("RAY_INIT", "IIII", TYPE_INT, libmod_ray_init),
    FUNC("RAY_SHUTDOWN", "", TYPE_INT, libmod_ray_shutdown),
    
    /* Carga de mapas */
    FUNC("RAY_LOAD_MAP", "SI", TYPE_INT, libmod_ray_load_map),
    FUNC("RAY_FREE_MAP", "", TYPE_INT, libmod_ray_free_map),
    
    /* Cámara - Getters */
    FUNC("RAY_GET_CAMERA_X", "", TYPE_FLOAT, libmod_ray_get_camera_x),
    FUNC("RAY_GET_CAMERA_Y", "", TYPE_FLOAT, libmod_ray_get_camera_y),
    FUNC("RAY_GET_CAMERA_Z", "", TYPE_FLOAT, libmod_ray_get_camera_z),
    FUNC("RAY_GET_CAMERA_ROT", "", TYPE_FLOAT, libmod_ray_get_camera_rot),
    FUNC("RAY_GET_CAMERA_PITCH", "", TYPE_FLOAT, libmod_ray_get_camera_pitch),
    
    /* Cámara - Setter */
    FUNC("RAY_SET_CAMERA", "FFFFF", TYPE_INT, libmod_ray_set_camera),
    
    /* Movimiento */
    FUNC("RAY_MOVE_FORWARD", "F", TYPE_INT, libmod_ray_move_forward),
    FUNC("RAY_MOVE_BACKWARD", "F", TYPE_INT, libmod_ray_move_backward),
    FUNC("RAY_STRAFE_LEFT", "F", TYPE_INT, libmod_ray_strafe_left),
    FUNC("RAY_STRAFE_RIGHT", "F", TYPE_INT, libmod_ray_strafe_right),
    FUNC("RAY_ROTATE", "F", TYPE_INT, libmod_ray_rotate),
    FUNC("RAY_LOOK_UP_DOWN", "F", TYPE_INT, libmod_ray_look_up_down),
    FUNC("RAY_JUMP", "", TYPE_INT, libmod_ray_jump),
    
    /* Renderizado */
    FUNC("RAY_RENDER", "", TYPE_INT, libmod_ray_render),
    
    /* Configuración */
    FUNC("RAY_SET_FOG", "I", TYPE_INT, libmod_ray_set_fog),
    FUNC("RAY_SET_DRAW_MINIMAP", "I", TYPE_INT, libmod_ray_set_draw_minimap),
    FUNC("RAY_SET_DRAW_WEAPON", "I", TYPE_INT, libmod_ray_set_draw_weapon),
    
    /* Puertas */
    FUNC("RAY_TOGGLE_DOOR", "II", TYPE_INT, libmod_ray_toggle_door),
    
    /* Sprites dinámicos */
    FUNC("RAY_ADD_SPRITE", "FFFIII", TYPE_INT, libmod_ray_add_sprite),
    FUNC("RAY_REMOVE_SPRITE", "I", TYPE_INT, libmod_ray_remove_sprite),
    
    FUNC(0, 0, 0, 0)
};

#endif

/* Hooks del módulo */
void __bgdexport(libmod_ray, module_initialize)();
void __bgdexport(libmod_ray, module_finalize)();

#endif /* __LIBMOD_RAY_EXPORTS */
