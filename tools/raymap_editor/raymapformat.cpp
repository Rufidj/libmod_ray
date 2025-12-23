#include "raymapformat.h"
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <cstring>

RayMapFormat::RayMapFormat()
{
}

bool RayMapFormat::loadMap(const QString &filename, MapData &mapData,
                           std::function<void(const QString&)> progressCallback)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "No se pudo abrir el archivo:" << filename;
        return false;
    }
    
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    
    // Leer header - USAR readRawData para compatibilidad binaria exacta con C
    RAY_MapHeader header;
    in.readRawData(header.magic, 8);
    in.readRawData(reinterpret_cast<char*>(&header.version), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&header.map_width), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&header.map_height), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&header.num_levels), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&header.num_sprites), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&header.num_thin_walls), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&header.num_thick_walls), sizeof(uint32_t));
    
    // Leer num_spawn_flags si es versión 3+
    if (header.version >= 3) {
        in.readRawData(reinterpret_cast<char*>(&header.num_spawn_flags), sizeof(uint32_t));
    } else {
        header.num_spawn_flags = 0;
    }
    
    // Verificar magic number
    if (memcmp(header.magic, "RAYMAP\x1a", 7) != 0) {
        qWarning() << "Formato de archivo inválido";
        file.close();
        return false;
    }
    
    // Verificar versión
    if (header.version < 1 || header.version > 4) {
        qWarning() << "Versión no soportada:" << header.version;
        file.close();
        return false;
    }
    
    qDebug() << "Cargando mapa versión" << header.version 
             << "de tamaño" << header.map_width << "x" << header.map_height;
    
    // Leer campos de cámara si es versión 2+
    if (header.version >= 2) {
        in.readRawData(reinterpret_cast<char*>(&header.camera_x), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&header.camera_y), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&header.camera_z), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&header.camera_rot), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&header.camera_pitch), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&header.skyTextureID), sizeof(int32_t));
        
        mapData.camera.x = header.camera_x;
        mapData.camera.y = header.camera_y;
        mapData.camera.z = header.camera_z;
        mapData.camera.rotation = header.camera_rot;
        mapData.camera.pitch = header.camera_pitch;
        mapData.camera.enabled = true;
        mapData.skyTextureID = header.skyTextureID;
        
        qDebug() << "Cámara cargada:" << mapData.camera.x << mapData.camera.y << mapData.camera.z;
        qDebug() << "Skybox ID:" << mapData.skyTextureID;
    } else {
        mapData.camera.enabled = false;
        mapData.skyTextureID = 0;
    }
    
    // Redimensionar mapa
    mapData.resize(header.map_width, header.map_height);
    mapData.num_levels = header.num_levels;
    
    if (progressCallback) progressCallback("Cargando grids de paredes...");
    
    // Leer grids principales
    if (!readGrid(in, mapData.grid0, header.map_width, header.map_height)) {
        qWarning() << "Error leyendo grid0";
        file.close();
        return false;
    }
    
    if (!readGrid(in, mapData.grid1, header.map_width, header.map_height)) {
        qWarning() << "Error leyendo grid1";
        file.close();
        return false;
    }
    
    if (!readGrid(in, mapData.grid2, header.map_width, header.map_height)) {
        qWarning() << "Error leyendo grid2";
        file.close();
        return false;
    }
    
    if (progressCallback) progressCallback("Cargando sprites...");
    
    // Leer sprites
    mapData.sprites.clear();
    for (uint32_t i = 0; i < header.num_sprites; i++) {
        SpriteData sprite;
        int texture_id, w, h, level;
        float x, y, z, rot;
        
        in >> texture_id >> x >> y >> z >> w >> h >> level >> rot;
        
        sprite.texture_id = texture_id;
        sprite.x = x;
        sprite.y = y;
        sprite.z = z;
        sprite.w = w;
        sprite.h = h;
        sprite.level = level;
        sprite.rot = rot;
        
        mapData.sprites.append(sprite);
    }
    
    qDebug() << "Sprites cargados:" << mapData.sprites.size();
    
    // Saltar thin walls (standalone, no los editamos)
    for (uint32_t i = 0; i < header.num_thin_walls; i++) {
        // Cada thin wall tiene: x1, y1, x2, y2, wallType, horizontal, height, z, slope, hidden
        // = 4 floats + 3 ints + 3 floats + 1 int = 8 floats + 4 ints = 32 + 16 = 48 bytes
        in.skipRawData(48);
    }
    
    // Leer ThickWalls
    mapData.thickWalls.clear();
    for (uint32_t i = 0; i < header.num_thick_walls; i++) {
        ThickWall tw;
        
        // Leer campos básicos - USAR readRawData para compatibilidad binaria exacta
        in.readRawData(reinterpret_cast<char*>(&tw.type), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&tw.slopeType), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&tw.slope), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&tw.x), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&tw.y), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&tw.w), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&tw.h), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&tw.ceilingTextureID), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&tw.floorTextureID), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&tw.startHeight), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&tw.endHeight), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&tw.invertedSlope), sizeof(int));
        
        // Leer puntos si es TRIANGLE o QUAD
        if (tw.type == 2 || tw.type == 3) {  // TRIANGLE o QUAD
            int num_points;
            in >> num_points;
            for (int p = 0; p < num_points; p++) {
                QPointF point;
                float px, py;
                in >> px >> py;
                point.setX(px);
                point.setY(py);
                tw.points.append(point);
            }
        }
        
        
        // Leer ThinWalls del ThickWall - USAR readRawData
        int numThinWalls;
        in.readRawData(reinterpret_cast<char*>(&numThinWalls), sizeof(int));
        for (int t = 0; t < numThinWalls; t++) {
            ThinWall thin;
            in.readRawData(reinterpret_cast<char*>(&thin.x1), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&thin.y1), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&thin.x2), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&thin.y2), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&thin.wallType), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&thin.horizontal), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&thin.height), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&thin.z), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&thin.slope), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&thin.hidden), sizeof(int));
            tw.thinWalls.append(thin);
        }
        
        mapData.thickWalls.append(tw);
    }
    
    qDebug() << "ThickWalls cargados:" << mapData.thickWalls.size();
    
    // Verificar si quedan datos en el archivo (floor/ceiling grids)
    // Solo intentar leer si no estamos al final del archivo
    if (!in.atEnd()) {
        qDebug() << "Leyendo floor/ceiling grids...";
        
        // Leer floor grids (3 niveles)
        if (!readGrid(in, mapData.floor0, header.map_width, header.map_height)) {
            qWarning() << "Error leyendo floor0, inicializando a 0";
            mapData.floor0.fill(0, header.map_width * header.map_height);
        }
        if (!readGrid(in, mapData.floor1, header.map_width, header.map_height)) {
            qWarning() << "Error leyendo floor1, inicializando a 0";
            mapData.floor1.fill(0, header.map_width * header.map_height);
        }
        if (!readGrid(in, mapData.floor2, header.map_width, header.map_height)) {
            qWarning() << "Error leyendo floor2, inicializando a 0";
            mapData.floor2.fill(0, header.map_width * header.map_height);
        }
        
        // Leer ceiling grids (3 niveles)
        if (!readGrid(in, mapData.ceiling0, header.map_width, header.map_height)) {
            qWarning() << "Error leyendo ceiling0, inicializando a 0";
            mapData.ceiling0.fill(0, header.map_width * header.map_height);
        }
        if (!readGrid(in, mapData.ceiling1, header.map_width, header.map_height)) {
            qWarning() << "Error leyendo ceiling1, inicializando a 0";
            mapData.ceiling1.fill(0, header.map_width * header.map_height);
        }
        if (!readGrid(in, mapData.ceiling2, header.map_width, header.map_height)) {
            qWarning() << "Error leyendo ceiling2, inicializando a 0";
            mapData.ceiling2.fill(0, header.map_width * header.map_height);
        }
        
        // Leer floor height grids (3 niveles) - opcional
        if (!in.atEnd()) {
            qDebug() << "Leyendo floor height grids...";
            if (!readFloatGrid(in, mapData.floorHeight0, header.map_width, header.map_height)) {
                qWarning() << "Error leyendo floorHeight0, inicializando a 0.0";
                mapData.floorHeight0.fill(0.0f, header.map_width * header.map_height);
            }
            if (!readFloatGrid(in, mapData.floorHeight1, header.map_width, header.map_height)) {
                qWarning() << "Error leyendo floorHeight1, inicializando a 0.0";
                mapData.floorHeight1.fill(0.0f, header.map_width * header.map_height);
            }
            if (!readFloatGrid(in, mapData.floorHeight2, header.map_width, header.map_height)) {
                qWarning() << "Error leyendo floorHeight2, inicializando a 0.0";
                mapData.floorHeight2.fill(0.0f, header.map_width * header.map_height);
            }
        }
        
        // Leer spawn flags si es versión 3+
        if (header.version >= 3 && !in.atEnd()) {
            qDebug() << "Leyendo spawn flags...";
            mapData.spawnFlags.clear();
            for (uint32_t i = 0; i < header.num_spawn_flags; i++) {
                SpawnFlag flag;
                in.readRawData(reinterpret_cast<char*>(&flag.flagId), sizeof(int));
                in.readRawData(reinterpret_cast<char*>(&flag.x), sizeof(float));
                in.readRawData(reinterpret_cast<char*>(&flag.y), sizeof(float));
                in.readRawData(reinterpret_cast<char*>(&flag.z), sizeof(float));
                in.readRawData(reinterpret_cast<char*>(&flag.level), sizeof(int));
                mapData.spawnFlags.append(flag);
            }
            qDebug() << "Spawn flags cargadas:" << mapData.spawnFlags.size();
        } else {
            mapData.spawnFlags.clear();
        }
    } else {
        qDebug() << "Mapa sin floor/ceiling data (versión antigua), inicializando a 0";
        // Inicializar todos los grids a 0
        int gridSize = header.map_width * header.map_height;
        mapData.floor0.fill(0, gridSize);
        mapData.floor1.fill(0, gridSize);
        mapData.floor2.fill(0, gridSize);
        mapData.ceiling0.fill(0, gridSize);
        mapData.ceiling1.fill(0, gridSize);
        mapData.ceiling2.fill(0, gridSize);
        mapData.floorHeight0.fill(0.0f, gridSize);
        mapData.floorHeight1.fill(0.0f, gridSize);
        mapData.floorHeight2.fill(0.0f, gridSize);
        mapData.spawnFlags.clear();
    }
    
    file.close();
    qDebug() << "Mapa cargado exitosamente";
    return true;
}

bool RayMapFormat::saveMap(const QString &filename, const MapData &mapData,
                           std::function<void(const QString&)> progressCallback)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "No se pudo crear el archivo:" << filename;
        return false;
    }
    
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    
    // Preparar header (versión 4 - grids por nivel)
    RAY_MapHeader header;
    memcpy(header.magic, "RAYMAP\x1a", 7);
    header.magic[7] = 0;
    header.version = 4;  // Versión 4 con grids por nivel
    header.map_width = mapData.width;
    header.map_height = mapData.height;
    header.num_levels = mapData.num_levels;
    header.num_sprites = mapData.sprites.size();
    header.num_thin_walls = 0;  // El editor no maneja ThinWalls standalone, solo dentro de ThickWalls
    header.num_thick_walls = mapData.thickWalls.size();
    header.num_spawn_flags = mapData.spawnFlags.size();
    header.camera_x = mapData.camera.x;
    header.camera_y = mapData.camera.y;
    header.camera_z = mapData.camera.z;
    header.camera_rot = mapData.camera.rotation;
    header.camera_pitch = mapData.camera.pitch;
    header.skyTextureID = mapData.skyTextureID;
    
    // Escribir header - USAR writeRawData para compatibilidad binaria exacta con C
    out.writeRawData(header.magic, 8);
    out.writeRawData(reinterpret_cast<const char*>(&header.version), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&header.map_width), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&header.map_height), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&header.num_levels), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&header.num_sprites), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&header.num_thin_walls), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&header.num_thick_walls), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&header.num_spawn_flags), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&header.camera_x), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&header.camera_y), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&header.camera_z), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&header.camera_rot), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&header.camera_pitch), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&header.skyTextureID), sizeof(int32_t));
    
    
    if (progressCallback) progressCallback("Guardando grids de paredes...");
    
    // Escribir grids principales
    // DEBUG: Contar puertas antes de guardar
    int door_count = 0;
    for (int val : mapData.grid0) if (val >= 1001 && val <= 2000) door_count++;
    for (int val : mapData.grid1) if (val >= 1001 && val <= 2000) door_count++;
    for (int val : mapData.grid2) if (val >= 1001 && val <= 2000) door_count++;
    if (door_count > 0) {
        qDebug() << "EDITOR: Guardando" << door_count << "puertas en el mapa";
    }
    
    // DEBUG: Imprimir primeros valores del grid0 antes de guardar
    qDebug() << "DEBUG saveMap: grid0 size=" << mapData.grid0.size() << " width=" << mapData.width;
    qDebug() << "DEBUG saveMap: Primeros 20 valores de grid0:";
    for (int i = 0; i < 20 && i < mapData.grid0.size(); i++) {
        qDebug() << "  grid0[" << i << "] =" << mapData.grid0[i];
    }
    
    writeGrid(out, mapData.grid0);
    writeGrid(out, mapData.grid1);
    writeGrid(out, mapData.grid2);
    
    if (progressCallback) progressCallback("Guardando sprites...");
    
    // Escribir sprites
    for (const SpriteData &sprite : mapData.sprites) {
        out << sprite.texture_id;
        out << sprite.x;
        out << sprite.y;
        out << sprite.z;
        out << sprite.w;
        out << sprite.h;
        out << sprite.level;
        out << sprite.rot;
    }
    
    // Escribir ThickWalls
    for (const ThickWall &tw : mapData.thickWalls) {
        // Escribir campos básicos - USAR writeRawData para compatibilidad binaria exacta
        out.writeRawData(reinterpret_cast<const char*>(&tw.type), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&tw.slopeType), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&tw.slope), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&tw.x), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&tw.y), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&tw.w), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&tw.h), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&tw.ceilingTextureID), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&tw.floorTextureID), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&tw.startHeight), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&tw.endHeight), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&tw.invertedSlope), sizeof(int));
        
        // Escribir puntos si es TRIANGLE o QUAD
        if (tw.type == 2 || tw.type == 3) {  // TRIANGLE o QUAD
            out << tw.points.size();
            for (const QPointF &point : tw.points) {
                out << static_cast<float>(point.x());
                out << static_cast<float>(point.y());
            }
        }
        
        // Escribir ThinWalls del ThickWall - USAR writeRawData
        int numThinWalls = tw.thinWalls.size();
        out.writeRawData(reinterpret_cast<const char*>(&numThinWalls), sizeof(int));
        for (const ThinWall &thin : tw.thinWalls) {
            out.writeRawData(reinterpret_cast<const char*>(&thin.x1), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&thin.y1), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&thin.x2), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&thin.y2), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&thin.wallType), sizeof(int));
            out.writeRawData(reinterpret_cast<const char*>(&thin.horizontal), sizeof(int));
            out.writeRawData(reinterpret_cast<const char*>(&thin.height), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&thin.z), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&thin.slope), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&thin.hidden), sizeof(int));
        }
    }
    
    qDebug() << "ThickWalls guardados:" << mapData.thickWalls.size();
    
    // Escribir floor grids
    writeGrid(out, mapData.floor0);
    writeGrid(out, mapData.floor1);
    writeGrid(out, mapData.floor2);
    
    // Escribir ceiling grids
    writeGrid(out, mapData.ceiling0);
    writeGrid(out, mapData.ceiling1);
    writeGrid(out, mapData.ceiling2);
    
    // Escribir floor height grids
    writeFloatGrid(out, mapData.floorHeight0);
    writeFloatGrid(out, mapData.floorHeight1);
    writeFloatGrid(out, mapData.floorHeight2);
    
    // Escribir spawn flags (versión 3+)
    if (progressCallback) progressCallback("Guardando spawn flags...");
    for (const SpawnFlag &flag : mapData.spawnFlags) {
        out.writeRawData(reinterpret_cast<const char*>(&flag.flagId), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&flag.x), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&flag.y), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&flag.z), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&flag.level), sizeof(int));
    }
    
    qDebug() << "Spawn flags guardadas:" << mapData.spawnFlags.size();
    
    file.close();
    qDebug() << "Mapa guardado exitosamente como versión 3";
    return true;
}

bool RayMapFormat::exportToText(const QString &directory, const MapData &mapData)
{
    QDir dir(directory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Crear archivo de configuración
    QFile configFile(dir.filePath("config.txt"));
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "No se pudo crear config.txt";
        return false;
    }
    
    QTextStream config(&configFile);
    config << "# Configuración del mapa\n";
    config << "grid0=nivel0.txt\n";
    config << "grid1=nivel1.txt\n";
    config << "grid2=nivel2.txt\n";
    config << "floor=floor.txt\n";
    config << "ceiling=ceiling.txt\n";
    config << "sprites=sprites.txt\n";
    config << "\n# Posición inicial de la cámara\n";
    config << "camera_x=" << mapData.camera.x << "\n";
    config << "camera_y=" << mapData.camera.y << "\n";
    config << "camera_z=" << mapData.camera.z << "\n";
    config << "camera_rot=" << mapData.camera.rotation << "\n";
    config << "camera_pitch=" << mapData.camera.pitch << "\n";
    configFile.close();
    
    // TODO: Exportar grids a CSV
    // TODO: Exportar sprites
    
    qDebug() << "Mapa exportado a formato de texto en" << directory;
    return true;
}

bool RayMapFormat::importFromText(const QString &configFile, MapData &mapData)
{
    // TODO: Implementar importación desde CSV
    Q_UNUSED(configFile);
    Q_UNUSED(mapData);
    qWarning() << "Importación desde texto no implementada aún";
    return false;
}

bool RayMapFormat::readGrid(QDataStream &in, QVector<int> &grid, int width, int height)
{
    int size = width * height;
    grid.resize(size);
    
    // Leer como datos raw para compatibilidad binaria exacta con C
    int bytesRead = in.readRawData(reinterpret_cast<char*>(grid.data()), size * sizeof(int));
    
    return bytesRead == size * sizeof(int);
}

bool RayMapFormat::writeGrid(QDataStream &out, const QVector<int> &grid)
{
    // Escribir como datos raw para compatibilidad binaria exacta con C
    int bytesWritten = out.writeRawData(reinterpret_cast<const char*>(grid.data()), grid.size() * sizeof(int));
    
    return bytesWritten == grid.size() * sizeof(int);
}

bool RayMapFormat::readFloatGrid(QDataStream &in, QVector<float> &grid, int width, int height)
{
    int size = width * height;
    grid.resize(size);
    
    // Leer como datos raw para compatibilidad con C
    int bytesRead = in.readRawData(reinterpret_cast<char*>(grid.data()), size * sizeof(float));
    
    return bytesRead == size * sizeof(float);
}

bool RayMapFormat::writeFloatGrid(QDataStream &out, const QVector<float> &grid)
{
    // Escribir como datos raw para compatibilidad con C
    int bytesWritten = out.writeRawData(reinterpret_cast<const char*>(grid.data()), grid.size() * sizeof(float));
    
    return bytesWritten == grid.size() * sizeof(float);
}
