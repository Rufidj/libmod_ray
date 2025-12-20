/*
 * libmod_ray.c - Raycasting Module for BennuGD2
 * Port of Andrew Lim's SDL2 Raycasting Engine
 * https://github.com/andrew-lim/sdl2-raycast
 */

#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
   ESTADO GLOBAL DEL MOTOR
   ============================================================================ */

RAY_Engine g_engine = {0};

/* ============================================================================
   UTILIDADES
   ============================================================================ */

int ray_is_door(int wallType) {
    return ray_is_vertical_door(wallType) || ray_is_horizontal_door(wallType);
}

int ray_is_vertical_door(int wallType) {
    return wallType > RAY_DOOR_VERTICAL_MIN && wallType <= RAY_DOOR_VERTICAL_MAX;
}

int ray_is_horizontal_door(int wallType) {
    return wallType > RAY_DOOR_HORIZONTAL_MIN;
}

/* ============================================================================
   INICIALIZACIÓN Y FINALIZACIÓN
   ============================================================================ */

int64_t libmod_ray_init(INSTANCE *my, int64_t *params) {
    int screen_w = (int)params[0];
    int screen_h = (int)params[1];
    int fov = (int)params[2];
    int strip_width = (int)params[3];
    
    if (g_engine.initialized) {
        fprintf(stderr, "RAY: Motor ya inicializado\n");
        return 0;
    }
    
    /* Configuración básica - NO inicializar ventana, solo configurar el motor */
    g_engine.displayWidth = screen_w;
    g_engine.displayHeight = screen_h;
    g_engine.fovDegrees = fov;
    g_engine.fovRadians = (float)fov * M_PI / 180.0f;
    g_engine.stripWidth = strip_width;
    g_engine.rayCount = screen_w / strip_width;
    g_engine.viewDist = ray_screen_distance((float)screen_w, g_engine.fovRadians);
    
    /* Precalcular ángulos de strips */
    g_engine.stripAngles = (float*)malloc(g_engine.rayCount * sizeof(float));
    if (!g_engine.stripAngles) {
        fprintf(stderr, "RAY: Error al asignar memoria para stripAngles\n");
        return 0;
    }
    
    for (int strip = 0; strip < g_engine.rayCount; strip++) {
        float screenX = (g_engine.rayCount / 2 - strip) * strip_width;
        g_engine.stripAngles[strip] = ray_strip_angle(screenX, g_engine.viewDist);
    }
    
    /* Inicializar cámara */
    memset(&g_engine.camera, 0, sizeof(RAY_Camera));
    g_engine.camera.moveSpeed = RAY_TILE_SIZE / 16.0f;
    g_engine.camera.rotSpeed = 1.5f * M_PI / 180.0f;
    
    /* Inicializar arrays dinámicos */
    g_engine.sprites_capacity = RAY_MAX_SPRITES;
    g_engine.sprites = (RAY_Sprite*)calloc(g_engine.sprites_capacity, sizeof(RAY_Sprite));
    
    g_engine.thin_walls_capacity = RAY_MAX_THIN_WALLS;
    g_engine.thinWalls = (RAY_ThinWall**)calloc(g_engine.thin_walls_capacity, sizeof(RAY_ThinWall*));
    
    g_engine.thick_walls_capacity = RAY_MAX_THICK_WALLS;
    g_engine.thickWalls = (RAY_ThickWall**)calloc(g_engine.thick_walls_capacity, sizeof(RAY_ThickWall*));
    
    /* Opciones de renderizado por defecto */
    g_engine.drawMiniMap = 1;
    g_engine.drawTexturedFloor = 1;
    g_engine.drawCeiling = 1;
    g_engine.drawWalls = 1;
    g_engine.drawWeapon = 1;
    g_engine.fogOn = 0;
    g_engine.skyTextureID = 0;  /* 0 = color sólido azul */
    g_engine.skipDrawnFloorStrips = 1;
    g_engine.skipDrawnSkyboxStrips = 1;
    g_engine.skipDrawnHighestCeilingStrips = 1;
    g_engine.highestCeilingLevel = 3;
    
    g_engine.initialized = 1;
    
    printf("RAY: Motor inicializado - %dx%d, FOV=%d, stripWidth=%d, rayCount=%d\n",
           screen_w, screen_h, fov, strip_width, g_engine.rayCount);
    printf("RAY: NOTA - La ventana debe ser inicializada con set_mode() antes de RAY_INIT\n");
    
    return 1;
}

int64_t libmod_ray_shutdown(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) {
        return 0;
    }
    
    /* Liberar stripAngles */
    if (g_engine.stripAngles) {
        free(g_engine.stripAngles);
        g_engine.stripAngles = NULL;
    }
    
    /* Liberar sprites */
    if (g_engine.sprites) {
        free(g_engine.sprites);
        g_engine.sprites = NULL;
    }
    
    /* Liberar thin walls */
    if (g_engine.thinWalls) {
        free(g_engine.thinWalls);
        g_engine.thinWalls = NULL;
    }
    
    /* Liberar thick walls */
    if (g_engine.thickWalls) {
        for (int i = 0; i < g_engine.num_thick_walls; i++) {
            if (g_engine.thickWalls[i]) {
                ray_thick_wall_free(g_engine.thickWalls[i]);
                free(g_engine.thickWalls[i]);
            }
        }
        free(g_engine.thickWalls);
        g_engine.thickWalls = NULL;
    }
    
    /* Liberar grids */
    if (g_engine.raycaster.grids) {
        for (int i = 0; i < g_engine.raycaster.gridCount; i++) {
            if (g_engine.raycaster.grids[i]) {
                free(g_engine.raycaster.grids[i]);
            }
        }
        free(g_engine.raycaster.grids);
        g_engine.raycaster.grids = NULL;
    }
    
    /* Liberar floor/ceiling grids */
    if (g_engine.floorGrid) {
        free(g_engine.floorGrid);
        g_engine.floorGrid = NULL;
    }
    if (g_engine.ceilingGrid) {
        free(g_engine.ceilingGrid);
        g_engine.ceilingGrid = NULL;
    }
    
    /* Liberar doors */
    if (g_engine.doors) {
        free(g_engine.doors);
        g_engine.doors = NULL;
    }
    
    memset(&g_engine, 0, sizeof(RAY_Engine));
    
    printf("RAY: Motor finalizado\n");
    return 1;
}

/* ============================================================================
   CÁMARA - GETTERS
   ============================================================================ */

int64_t libmod_ray_get_camera_x(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    return *(int64_t*)&g_engine.camera.x;
}

int64_t libmod_ray_get_camera_y(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    return *(int64_t*)&g_engine.camera.y;
}

int64_t libmod_ray_get_camera_z(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    return *(int64_t*)&g_engine.camera.z;
}

int64_t libmod_ray_get_camera_rot(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    return *(int64_t*)&g_engine.camera.rot;
}

int64_t libmod_ray_get_camera_pitch(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    return *(int64_t*)&g_engine.camera.pitch;
}

/* ============================================================================
   CÁMARA - SETTER
   ============================================================================ */

int64_t libmod_ray_set_camera(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float x = *(float*)&params[0];
    float y = *(float*)&params[1];
    float z = *(float*)&params[2];
    float rot = *(float*)&params[3];
    float pitch = *(float*)&params[4];
    
    g_engine.camera.x = x;
    g_engine.camera.y = y;
    g_engine.camera.z = z;
    g_engine.camera.rot = rot;
    g_engine.camera.pitch = pitch;
    
    /* Limitar pitch */
    const float max_pitch = M_PI / 2.0f * 0.99f;
    if (g_engine.camera.pitch > max_pitch) g_engine.camera.pitch = max_pitch;
    if (g_engine.camera.pitch < -max_pitch) g_engine.camera.pitch = -max_pitch;
    
    return 1;
}

/* ============================================================================
   MOVIMIENTO
   ============================================================================*/
/* Movement functions */
int64_t libmod_ray_move_forward(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    float newX = g_engine.camera.x + cosf(g_engine.camera.rot) * speed;
    float newY = g_engine.camera.y - sinf(g_engine.camera.rot) * speed;
    
    /* Clamp to map bounds */
    float mapWidth = g_engine.raycaster.gridWidth * RAY_TILE_SIZE;
    float mapHeight = g_engine.raycaster.gridHeight * RAY_TILE_SIZE;
    if (newX < 0) newX = 0;
    if (newX >= mapWidth) newX = mapWidth - 1;
    if (newY < 0) newY = 0;
    if (newY >= mapHeight) newY = mapHeight - 1;
    
    /* TODO: Implementar detección de colisiones */
    g_engine.camera.x = newX;
    g_engine.camera.y = newY;
    
    return 1;
}

int64_t libmod_ray_move_backward(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    float newX = g_engine.camera.x - cosf(g_engine.camera.rot) * speed;
    float newY = g_engine.camera.y + sinf(g_engine.camera.rot) * speed;
    
    /* Clamp to map bounds */
    float mapWidth = g_engine.raycaster.gridWidth * RAY_TILE_SIZE;
    float mapHeight = g_engine.raycaster.gridHeight * RAY_TILE_SIZE;
    if (newX < 0) newX = 0;
    if (newX >= mapWidth) newX = mapWidth - 1;
    if (newY < 0) newY = 0;
    if (newY >= mapHeight) newY = mapHeight - 1;
    
    /* TODO: Implementar detección de colisiones */
    g_engine.camera.x = newX;
    g_engine.camera.y = newY;
    
    return 1;
}

int64_t libmod_ray_strafe_left(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    float newX = g_engine.camera.x + cosf(g_engine.camera.rot - M_PI / 2) * speed;
    float newY = g_engine.camera.y - sinf(g_engine.camera.rot - M_PI / 2) * speed;
    
    /* Clamp to map bounds */
    float mapWidth = g_engine.raycaster.gridWidth * RAY_TILE_SIZE;
    float mapHeight = g_engine.raycaster.gridHeight * RAY_TILE_SIZE;
    if (newX < 0) newX = 0;
    if (newX >= mapWidth) newX = mapWidth - 1;
    if (newY < 0) newY = 0;
    if (newY >= mapHeight) newY = mapHeight - 1;
    
    /* TODO: Implementar detección de colisiones */
    g_engine.camera.x = newX;
    g_engine.camera.y = newY;
    
    return 1;
}

int64_t libmod_ray_strafe_right(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    float newX = g_engine.camera.x + cosf(g_engine.camera.rot + M_PI / 2) * speed;
    float newY = g_engine.camera.y - sinf(g_engine.camera.rot + M_PI / 2) * speed;
    
    /* Clamp to map bounds */
    float mapWidth = g_engine.raycaster.gridWidth * RAY_TILE_SIZE;
    float mapHeight = g_engine.raycaster.gridHeight * RAY_TILE_SIZE;
    if (newX < 0) newX = 0;
    if (newX >= mapWidth) newX = mapWidth - 1;
    if (newY < 0) newY = 0;
    if (newY >= mapHeight) newY = mapHeight - 1;
    
    /* TODO: Implementar detección de colisiones */
    g_engine.camera.x = newX;
    g_engine.camera.y = newY;
    
    return 1;
}

int64_t libmod_ray_rotate(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float delta = *(float*)&params[0];
    g_engine.camera.rot += delta;
    
    /* Normalizar ángulo */
    while (g_engine.camera.rot < 0) g_engine.camera.rot += RAY_TWO_PI;
    while (g_engine.camera.rot >= RAY_TWO_PI) g_engine.camera.rot -= RAY_TWO_PI;
    
    return 1;
}

int64_t libmod_ray_look_up_down(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float delta = *(float*)&params[0];
    g_engine.camera.pitch += delta;
    
    /* Limitar pitch */
    const float max_pitch = M_PI / 2.0f * 0.99f;
    if (g_engine.camera.pitch > max_pitch) g_engine.camera.pitch = max_pitch;
    if (g_engine.camera.pitch < -max_pitch) g_engine.camera.pitch = -max_pitch;
    
    return 1;
}

int64_t libmod_ray_jump(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    if (!g_engine.camera.jumping) {
        g_engine.camera.jumping = 1;
        g_engine.camera.heightJumped = 0;
    }
    
    return 1;
}

/* ============================================================================
   UPDATE - Actualización de física (llamar cada frame)
   ============================================================================ */

void ray_update_physics(float delta_time) {
    /* Constantes de salto (del motor original) */
    const float MAX_JUMP_DISTANCE = 3.0f * RAY_TILE_SIZE;
    const float HALF_JUMP_DISTANCE = MAX_JUMP_DISTANCE / 2.0f;
    const float JUMP_SPEED = 8.0f; /* Velocidad de salto ajustable */
    
    if (g_engine.camera.jumping) {
        /* Fase ascendente del salto */
        if (g_engine.camera.heightJumped < HALF_JUMP_DISTANCE) {
            float jump_increment = JUMP_SPEED * delta_time;
            g_engine.camera.z += jump_increment;
            g_engine.camera.heightJumped += jump_increment;
            
            /* Alcanzó el punto máximo */
            if (g_engine.camera.heightJumped >= HALF_JUMP_DISTANCE) {
                g_engine.camera.heightJumped = HALF_JUMP_DISTANCE;
            }
        }
        /* Fase descendente del salto */
        else if (g_engine.camera.heightJumped < MAX_JUMP_DISTANCE) {
            float fall_increment = JUMP_SPEED * delta_time;
            g_engine.camera.z -= fall_increment;
            g_engine.camera.heightJumped += fall_increment;
            
            /* Terminó el salto */
            if (g_engine.camera.heightJumped >= MAX_JUMP_DISTANCE) {
                g_engine.camera.jumping = 0;
                g_engine.camera.heightJumped = 0;
            }
        }
    }
    
    /* Limpiar sprites marcados para eliminación */
    int write_idx = 0;
    for (int read_idx = 0; read_idx < g_engine.num_sprites; read_idx++) {
        if (!g_engine.sprites[read_idx].cleanup) {
            if (write_idx != read_idx) {
                g_engine.sprites[write_idx] = g_engine.sprites[read_idx];
            }
            write_idx++;
        }
    }
    g_engine.num_sprites = write_idx;
}

/* ============================================================================
   CONFIGURACIÓN
   ============================================================================ */

int64_t libmod_ray_set_fog(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    g_engine.fogOn = (int)params[0];
    return 1;
}

int64_t libmod_ray_set_draw_minimap(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    g_engine.drawMiniMap = (int)params[0];
    return 1;
}

int64_t libmod_ray_set_draw_weapon(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    g_engine.drawWeapon = (int)params[0];
    return 1;
}

int64_t libmod_ray_set_sky_texture(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    g_engine.skyTextureID = (int)params[0];
    return 1;
}

/* ============================================================================
   PUERTAS
   ============================================================================ */

int64_t libmod_ray_toggle_door(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int cellX = (int)params[0];
    int cellY = (int)params[1];
    
    if (!g_engine.doors) return 0;
    if (cellX < 0 || cellX >= g_engine.raycaster.gridWidth) return 0;
    if (cellY < 0 || cellY >= g_engine.raycaster.gridHeight) return 0;
    
    int offset = cellX + cellY * g_engine.raycaster.gridWidth;
    g_engine.doors[offset] = !g_engine.doors[offset];
    
    return 1;
}

/* ============================================================================
   SPRITES DINÁMICOS
   ============================================================================ */

int64_t libmod_ray_add_sprite(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float x = *(float*)&params[0];
    float y = *(float*)&params[1];
    float z = *(float*)&params[2];
    int textureID = (int)params[3];
    int w = (int)params[4];
    int h = (int)params[5];
    
    if (g_engine.num_sprites >= g_engine.sprites_capacity) {
        fprintf(stderr, "RAY: Máximo de sprites alcanzado\n");
        return -1;
    }
    
    RAY_Sprite *sprite = &g_engine.sprites[g_engine.num_sprites];
    memset(sprite, 0, sizeof(RAY_Sprite));
    
    sprite->x = x;
    sprite->y = y;
    sprite->z = z;
    sprite->w = w;
    sprite->h = h;
    sprite->textureID = textureID;
    sprite->level = 0; /* TODO: Calcular nivel correcto */
    
    g_engine.num_sprites++;
    return g_engine.num_sprites - 1;
}

int64_t libmod_ray_remove_sprite(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int index = (int)params[0];
    
    if (index < 0 || index >= g_engine.num_sprites) {
        return 0;
    }
    
    /* Marcar para eliminación */
    g_engine.sprites[index].cleanup = 1;
    
    return 1;
}

/* ============================================================================
   RENDERIZADO
   ============================================================================ */

/* Declaración de función de renderizado completo */
extern void ray_render_frame(GRAPH *dest);

int64_t libmod_ray_render(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) {
        fprintf(stderr, "RAY: Motor no inicializado\n");
        return 0;
    }
    
    /* Crear o reutilizar graph de renderizado */
    static GRAPH *render_graph = NULL;
    
    if (!render_graph) {
        /* Crear graph la primera vez usando bitmap_new_syslib */
        printf("RAY: Creating render graph %dx%d\n", g_engine.displayWidth, g_engine.displayHeight);
        render_graph = bitmap_new_syslib(g_engine.displayWidth, g_engine.displayHeight);
        printf("RAY: bitmap_new_syslib returned %p\n", (void*)render_graph);
        
        if (!render_graph) {
            fprintf(stderr, "RAY: No se pudo crear graph de renderizado\n");
            return 0;
        }
    }
    
    /* Renderizar frame completo */
    ray_render_frame(render_graph);
    
    /* Retornar el code del graph para que BennuGD lo muestre */
    return render_graph->code;
}

/* ============================================================================
   CARGA DE MAPAS
   ============================================================================ */

/* Declaraciones de funciones de carga de mapas */
extern int64_t libmod_ray_load_map(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_free_map(INSTANCE *my, int64_t *params);

/* ============================================================================
   HOOKS DEL MÓDULO
   ============================================================================ */

void __bgdexport(libmod_ray, module_initialize)() {
    printf("RAY: Módulo libmod_ray cargado\n");
}

void __bgdexport(libmod_ray, module_finalize)() {
    if (g_engine.initialized) {
        libmod_ray_shutdown(NULL, NULL);
    }
    printf("RAY: Módulo libmod_ray descargado\n");
}

/* Incluir exports al final del archivo */
#include "libmod_ray_exports.h"

