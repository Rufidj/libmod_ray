/*
 * libmod_ray_map.c - Map Loading System
 * Implements loading of .raymap binary format
 */

#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern RAY_Engine g_engine;

/* ============================================================================
   MAP FILE FORMAT (.raymap)
   ============================================================================ */

typedef struct {
    char magic[8];           /* "RAYMAP\x1a" */
    uint32_t version;        /* Version 1 */
    uint32_t map_width;
    uint32_t map_height;
    uint32_t num_levels;     /* Típicamente 3 */
    uint32_t num_sprites;
    uint32_t num_thin_walls;
    uint32_t num_thick_walls;
} RAY_MapHeader;

/* ============================================================================
   MAP LOADING
   ============================================================================ */

int ray_load_map_from_file(const char *filename, int fpg_id)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "RAY: No se puede abrir mapa: %s\n", filename);
        return 0;
    }
    
    /* Leer header */
    RAY_MapHeader header;
    if (fread(&header, sizeof(RAY_MapHeader), 1, f) != 1) {
        fprintf(stderr, "RAY: Error leyendo header del mapa\n");
        fclose(f);
        return 0;
    }
    
    /* Verificar magic */
    if (memcmp(header.magic, "RAYMAP\x1a", 7) != 0) {
        fprintf(stderr, "RAY: Archivo no es un mapa válido\n");
        fclose(f);
        return 0;
    }
    
    /* Verificar versión */
    if (header.version != 1) {
        fprintf(stderr, "RAY: Versión de mapa no soportada: %u\n", header.version);
        fclose(f);
        return 0;
    }
    
    printf("RAY: Cargando mapa %dx%d con %d niveles\n",
           header.map_width, header.map_height, header.num_levels);
    
    /* Crear grids del raycaster */
    ray_raycaster_create_grids(&g_engine.raycaster,
                               header.map_width,
                               header.map_height,
                               header.num_levels,
                               RAY_TILE_SIZE);
    
    /* Leer datos de grids */
    for (uint32_t level = 0; level < header.num_levels; level++) {
        size_t grid_size = header.map_width * header.map_height * sizeof(int);
        if (fread(g_engine.raycaster.grids[level], grid_size, 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo grid nivel %d\n", level);
            fclose(f);
            return 0;
        }
    }
    
    /* Leer sprites */
    g_engine.num_sprites = 0;
    for (uint32_t i = 0; i < header.num_sprites && i < RAY_MAX_SPRITES; i++) {
        RAY_Sprite sprite;
        
        /* Leer datos del sprite */
        if (fread(&sprite.textureID, sizeof(int), 1, f) != 1 ||
            fread(&sprite.x, sizeof(float), 1, f) != 1 ||
            fread(&sprite.y, sizeof(float), 1, f) != 1 ||
            fread(&sprite.z, sizeof(float), 1, f) != 1 ||
            fread(&sprite.w, sizeof(int), 1, f) != 1 ||
            fread(&sprite.h, sizeof(int), 1, f) != 1 ||
            fread(&sprite.level, sizeof(int), 1, f) != 1 ||
            fread(&sprite.rot, sizeof(float), 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo sprite %d\n", i);
            fclose(f);
            return 0;
        }
        
        /* Inicializar resto de campos */
        sprite.dir = 0;
        sprite.speed = 0;
        sprite.moveSpeed = 0;
        sprite.rotSpeed = 0;
        sprite.distance = 0;
        sprite.cleanup = 0;
        sprite.frameRate = 0;
        sprite.frame = 0;
        sprite.hidden = 0;
        sprite.jumping = 0;
        sprite.heightJumped = 0;
        sprite.rayhit = 0;
        
        g_engine.sprites[g_engine.num_sprites++] = sprite;
    }
    
    /* Leer ThinWalls */
    g_engine.num_thin_walls = 0;
    for (uint32_t i = 0; i < header.num_thin_walls && i < RAY_MAX_THIN_WALLS; i++) {
        RAY_ThinWall *tw = (RAY_ThinWall*)malloc(sizeof(RAY_ThinWall));
        if (!tw) continue;
        
        if (fread(&tw->x1, sizeof(float), 1, f) != 1 ||
            fread(&tw->y1, sizeof(float), 1, f) != 1 ||
            fread(&tw->x2, sizeof(float), 1, f) != 1 ||
            fread(&tw->y2, sizeof(float), 1, f) != 1 ||
            fread(&tw->wallType, sizeof(int), 1, f) != 1 ||
            fread(&tw->horizontal, sizeof(int), 1, f) != 1 ||
            fread(&tw->height, sizeof(float), 1, f) != 1 ||
            fread(&tw->z, sizeof(float), 1, f) != 1 ||
            fread(&tw->slope, sizeof(float), 1, f) != 1 ||
            fread(&tw->hidden, sizeof(int), 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo thin wall %d\n", i);
            free(tw);
            fclose(f);
            return 0;
        }
        
        tw->thickWall = NULL; /* Se enlazará después si es necesario */
        g_engine.thinWalls[g_engine.num_thin_walls++] = tw;
    }
    
    /* Leer ThickWalls */
    g_engine.num_thick_walls = 0;
    for (uint32_t i = 0; i < header.num_thick_walls && i < RAY_MAX_THICK_WALLS; i++) {
        RAY_ThickWall *tw = (RAY_ThickWall*)malloc(sizeof(RAY_ThickWall));
        if (!tw) continue;
        
        ray_thick_wall_init(tw);
        
        /* Leer tipo y propiedades básicas */
        if (fread(&tw->type, sizeof(int), 1, f) != 1 ||
            fread(&tw->slopeType, sizeof(int), 1, f) != 1 ||
            fread(&tw->x, sizeof(float), 1, f) != 1 ||
            fread(&tw->y, sizeof(float), 1, f) != 1 ||
            fread(&tw->w, sizeof(float), 1, f) != 1 ||
            fread(&tw->h, sizeof(float), 1, f) != 1 ||
            fread(&tw->ceilingTextureID, sizeof(int), 1, f) != 1 ||
            fread(&tw->floorTextureID, sizeof(int), 1, f) != 1 ||
            fread(&tw->startHeight, sizeof(float), 1, f) != 1 ||
            fread(&tw->endHeight, sizeof(float), 1, f) != 1 ||
            fread(&tw->invertedSlope, sizeof(int), 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo thick wall %d\n", i);
            free(tw);
            fclose(f);
            return 0;
        }
        
        /* Leer puntos si es TRIANGLE o QUAD */
        if (tw->type == RAY_THICK_WALL_TYPE_TRIANGLE || 
            tw->type == RAY_THICK_WALL_TYPE_QUAD) {
            int num_points;
            if (fread(&num_points, sizeof(int), 1, f) != 1) {
                free(tw);
                fclose(f);
                return 0;
            }
            
            tw->num_points = num_points;
            tw->points = (RAY_Point*)malloc(num_points * sizeof(RAY_Point));
            if (!tw->points) {
                free(tw);
                fclose(f);
                return 0;
            }
            
            for (int p = 0; p < num_points; p++) {
                if (fread(&tw->points[p].x, sizeof(float), 1, f) != 1 ||
                    fread(&tw->points[p].y, sizeof(float), 1, f) != 1) {
                    ray_thick_wall_free(tw);
                    free(tw);
                    fclose(f);
                    return 0;
                }
            }
        }
        
        /* Leer ThinWalls del ThickWall */
        int num_thin_walls;
        if (fread(&num_thin_walls, sizeof(int), 1, f) != 1) {
            ray_thick_wall_free(tw);
            free(tw);
            fclose(f);
            return 0;
        }
        
        tw->num_thin_walls = 0;
        tw->thin_walls_capacity = num_thin_walls;
        tw->thinWalls = (RAY_ThinWall*)malloc(num_thin_walls * sizeof(RAY_ThinWall));
        
        for (int t = 0; t < num_thin_walls; t++) {
            RAY_ThinWall *thin = &tw->thinWalls[t];
            if (fread(&thin->x1, sizeof(float), 1, f) != 1 ||
                fread(&thin->y1, sizeof(float), 1, f) != 1 ||
                fread(&thin->x2, sizeof(float), 1, f) != 1 ||
                fread(&thin->y2, sizeof(float), 1, f) != 1 ||
                fread(&thin->wallType, sizeof(int), 1, f) != 1 ||
                fread(&thin->horizontal, sizeof(int), 1, f) != 1 ||
                fread(&thin->height, sizeof(float), 1, f) != 1 ||
                fread(&thin->z, sizeof(float), 1, f) != 1 ||
                fread(&thin->slope, sizeof(float), 1, f) != 1 ||
                fread(&thin->hidden, sizeof(int), 1, f) != 1) {
                ray_thick_wall_free(tw);
                free(tw);
                fclose(f);
                return 0;
            }
            thin->thickWall = tw;
            tw->num_thin_walls++;
        }
        
        g_engine.thickWalls[g_engine.num_thick_walls++] = tw;
    }
    
    /* Leer floor grid */
    size_t grid_size = header.map_width * header.map_height * sizeof(int);
    g_engine.floorGrid = (int*)malloc(grid_size);
    if (g_engine.floorGrid) {
        if (fread(g_engine.floorGrid, grid_size, 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo floor grid\n");
        }
    }
    
    /* Leer ceiling grid */
    g_engine.ceilingGrid = (int*)malloc(grid_size);
    if (g_engine.ceilingGrid) {
        if (fread(g_engine.ceilingGrid, grid_size, 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo ceiling grid\n");
        }
    }
    
    /* Inicializar array de puertas */
    g_engine.doors = (int*)calloc(header.map_width * header.map_height, sizeof(int));
    
    fclose(f);
    
    printf("RAY: Mapa cargado exitosamente\n");
    printf("  - Sprites: %d\n", g_engine.num_sprites);
    printf("  - ThinWalls: %d\n", g_engine.num_thin_walls);
    printf("  - ThickWalls: %d\n", g_engine.num_thick_walls);
    
    return 1;
}

/* ============================================================================
   EXPORTED FUNCTIONS
   ============================================================================ */

int64_t libmod_ray_load_map(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) {
        fprintf(stderr, "RAY: Motor no inicializado\n");
        return 0;
    }
    
    const char *filename = string_get(params[0]);
    int fpg_id = (int)params[1];
    
    /* Verificar que el FPG existe */
    if (!grlib_get(fpg_id)) {
        fprintf(stderr, "RAY: El fpg_id %d no existe\n", fpg_id);
        string_discard(params[0]);
        return 0;
    }
    
    g_engine.fpg_id = fpg_id;
    
    int result = ray_load_map_from_file(filename, fpg_id);
    
    string_discard(params[0]);
    return result;
}

int64_t libmod_ray_free_map(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    /* Liberar grids */
    if (g_engine.raycaster.grids) {
        for (int i = 0; i < g_engine.raycaster.gridCount; i++) {
            if (g_engine.raycaster.grids[i]) {
                free(g_engine.raycaster.grids[i]);
                g_engine.raycaster.grids[i] = NULL;
            }
        }
        free(g_engine.raycaster.grids);
        g_engine.raycaster.grids = NULL;
    }
    
    /* Liberar thin walls */
    for (int i = 0; i < g_engine.num_thin_walls; i++) {
        if (g_engine.thinWalls[i]) {
            free(g_engine.thinWalls[i]);
            g_engine.thinWalls[i] = NULL;
        }
    }
    g_engine.num_thin_walls = 0;
    
    /* Liberar thick walls */
    for (int i = 0; i < g_engine.num_thick_walls; i++) {
        if (g_engine.thickWalls[i]) {
            ray_thick_wall_free(g_engine.thickWalls[i]);
            free(g_engine.thickWalls[i]);
            g_engine.thickWalls[i] = NULL;
        }
    }
    g_engine.num_thick_walls = 0;
    
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
    
    /* Limpiar sprites */
    g_engine.num_sprites = 0;
    
    printf("RAY: Mapa liberado\n");
    return 1;
}
