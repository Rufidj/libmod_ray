#ifndef MAPDATA_H
#define MAPDATA_H

#include <QString>
#include <QPixmap>
#include <QVector>
#include <QMap>
#include <cstdint>

// Estructura para texturas del FPG
struct TextureEntry {
    QString filename;
    uint32_t id;
    QPixmap pixmap;

    TextureEntry() : id(0) {}
    TextureEntry(const QString &fname, uint32_t tid) : filename(fname), id(tid) {}
};

// Estructura para sprites en el mapa
struct SpriteData {
    float x, y, z;
    int texture_id;
    int w, h;
    int level;
    float rot;  // En radianes
    
    SpriteData() : x(0), y(0), z(0), texture_id(0), w(128), h(128), level(0), rot(0) {}
};

// Estructura para ThinWalls (paredes delgadas)
struct ThinWall {
    float x1, y1, x2, y2;
    int wallType;
    int horizontal;
    float height;
    float z;
    float slope;
    int hidden;
    
    ThinWall() : x1(0), y1(0), x2(0), y2(0), wallType(0), horizontal(0), 
                 height(0), z(0), slope(0), hidden(0) {}
};

// Estructura para ThickWalls (paredes gruesas / slopes)
struct ThickWall {
    int type;  // 0=none, 1=rect, 2=triangle, 3=quad
    int slopeType;  // 0=none, 1=WEST_EAST, 2=NORTH_SOUTH
    
    // Para RECT
    float x, y, w, h;
    
    // Para TRIANGLE y QUAD
    QVector<QPointF> points;
    
    // Propiedades
    float z;
    float slope;
    int ceilingTextureID;
    int floorTextureID;
    float startHeight;
    float endHeight;
    float tallerHeight;
    int invertedSlope;
    
    // ThinWalls que forman este ThickWall
    QVector<ThinWall> thinWalls;
    
    ThickWall() : type(0), slopeType(0), x(0), y(0), w(0), h(0), z(0), 
                  slope(0), ceilingTextureID(0), floorTextureID(0),
                  startHeight(0), endHeight(0), tallerHeight(0), invertedSlope(0) {}
};

// Estructura para posición de cámara
struct CameraData {
    float x, y, z;
    float rotation;  // En radianes
    float pitch;     // En radianes
    bool enabled;    // Si está definida en el mapa
    
    CameraData() : x(384.0f), y(384.0f), z(0.0f), rotation(0.0f), pitch(0.0f), enabled(false) {}
};

// Estructura para spawn flags (posiciones de spawn para sprites)
struct SpawnFlag {
    int flagId;      // ID único de la flag (1, 2, 3...)
    float x, y, z;   // Posición en el mundo
    int level;       // Nivel del grid (0, 1, 2)
    
    SpawnFlag() : flagId(1), x(384.0f), y(384.0f), z(0.0f), level(0) {}
    SpawnFlag(int id, float px, float py, float pz, int lv) 
        : flagId(id), x(px), y(py), z(pz), level(lv) {}
};

// Estructura principal del mapa
struct MapData {
    int width;
    int height;
    int num_levels;
    
    // Grids principales (paredes)
    QVector<int> grid0;  // Nivel 0
    QVector<int> grid1;  // Nivel 1
    QVector<int> grid2;  // Nivel 2
    
    // Grids de suelo (3 niveles)
    QVector<int> floor0;
    QVector<int> floor1;
    QVector<int> floor2;
    
    // Grids de techo (3 niveles)
    QVector<int> ceiling0;
    QVector<int> ceiling1;
    QVector<int> ceiling2;
    
    // Grids de altura de suelo (3 niveles) - para rampas y escalones
    QVector<float> floorHeight0;
    QVector<float> floorHeight1;
    QVector<float> floorHeight2;
    
    // Sprites
    QVector<SpriteData> sprites;
    
    // Spawn Flags
    QVector<SpawnFlag> spawnFlags;
    
    // ThickWalls (slopes/ramps)
    QVector<ThickWall> thickWalls;
    
    // Cámara
    CameraData camera;
    
    // Skybox
    int skyTextureID = 0;  // 0 = sin skybox
    
    // Texturas cargadas
    QVector<TextureEntry> textures;
    
    MapData() : width(16), height(16), num_levels(3) {
        resize(width, height);
    }
    
    void resize(int w, int h) {
        width = w;
        height = h;
        int size = w * h;
        
        // Redimensionar grids de paredes
        grid0.resize(size);
        grid1.resize(size);
        grid2.resize(size);
        
        // Redimensionar grids de suelo
        floor0.resize(size);
        floor1.resize(size);
        floor2.resize(size);
        
        // Redimensionar grids de techo
        ceiling0.resize(size);
        ceiling1.resize(size);
        ceiling2.resize(size);
        
        // Redimensionar grids de altura de suelo
        floorHeight0.resize(size);
        floorHeight1.resize(size);
        floorHeight2.resize(size);
        
        // CRÍTICO: Inicializar TODO a 0 para evitar datos basura
        grid0.fill(0);
        grid1.fill(0);
        grid2.fill(0);
        floor0.fill(0);
        floor1.fill(0);
        floor2.fill(0);
        ceiling0.fill(0);
        ceiling1.fill(0);
        ceiling2.fill(0);
        floorHeight0.fill(0.0f);
        floorHeight1.fill(0.0f);
        floorHeight2.fill(0.0f);
    }
    
    // Obtener grid por nivel
    QVector<int>* getGrid(int level) {
        switch(level) {
            case 0: return &grid0;
            case 1: return &grid1;
            case 2: return &grid2;
            default: return nullptr;
        }
    }
    
    QVector<int>* getFloorGrid(int level) {
        switch(level) {
            case 0: return &floor0;
            case 1: return &floor1;
            case 2: return &floor2;
            default: return nullptr;
        }
    }
    
    QVector<int>* getCeilingGrid(int level) {
        switch(level) {
            case 0: return &ceiling0;
            case 1: return &ceiling1;
            case 2: return &ceiling2;
            default: return nullptr;
        }
    }
    
    QVector<float>* getFloorHeightGrid(int level) {
        switch(level) {
            case 0: return &floorHeight0;
            case 1: return &floorHeight1;
            case 2: return &floorHeight2;
            default: return nullptr;
        }
    }
};

#endif // MAPDATA_H
