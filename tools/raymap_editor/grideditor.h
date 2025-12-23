#ifndef GRIDEDITOR_H
#define GRIDEDITOR_H

#include <QWidget>
#include <QPixmap>
#include <QMap>
#include <QPoint>
#include "mapdata.h"

class GridEditor : public QWidget
{
    Q_OBJECT
    
public:
    explicit GridEditor(QWidget *parent = nullptr);
    
    // Configurar datos del mapa
    void setMapData(MapData *data);
    
    // Configurar texturas disponibles
    void setTextures(const QMap<int, QPixmap> &textures);
    
    // Configurar nivel actual (0, 1, 2)
    void setCurrentLevel(int level);
    
    // Configurar modo de edición (walls, floor, ceiling, spawn_flags, floor_height, slope)
    enum EditMode { MODE_WALLS, MODE_FLOOR, MODE_CEILING, MODE_SPAWN_FLAGS, MODE_FLOOR_HEIGHT, MODE_SLOPE };
    void setEditMode(EditMode mode);
    
    // Configurar textura seleccionada para pintar
    void setSelectedTexture(int textureId);
    
    // Configurar parámetros de slope
    void setSlopeType(int type);  // 1=WEST_EAST, 2=NORTH_SOUTH
    void setSlopeStartHeight(float height);
    void setSlopeEndHeight(float height);
    void setSlopeInverted(bool inverted);
    
    // Configurar zoom
    void setZoom(float zoom);
    
    void setTextureList(const QMap<int, QPixmap> &textures);
    void setCurrentTexture(int textureId);
    
    // Camera position
    void setCameraPosition(int x, int y);
    void getCameraPosition(int &x, int &y) const;
    bool hasCameraPosition() const { return m_hasCameraPosition; }
    
    // Obtener celda bajo el cursor
    QPoint getCellAtPosition(const QPoint &pos);
    
signals:
    void cellClicked(int x, int y, int value);
    void cellHovered(int x, int y);
    void cameraPlaced(int x, int y);
    void spawnFlagPlaced(int flagId, int x, int y);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    
private:
    MapData *m_mapData;
    QMap<int, QPixmap> m_textures;
    int m_currentLevel;
    EditMode m_editMode;
    int m_selectedTexture;
    float m_zoom;
    int m_cellSize;  // Tamaño de celda en píxeles
    
    bool m_isPainting;
    QPoint m_lastCell;

    int m_currentTexture; // Added from instruction
    
    // Slope editing
    bool m_drawingSlope;
    QPoint m_slopeStart;
    int m_slopeType;  // 1=WEST_EAST, 2=NORTH_SOUTH
    float m_slopeStartHeight;
    float m_slopeEndHeight;
    bool m_slopeInverted;
    
    // Camera position
    bool m_hasCameraPosition; // Added from instruction
    int m_cameraX; // Added from instruction
    int m_cameraY; // Added from instruction
    
    void paintCell(int x, int y);
    void drawGrid(QPainter &painter);
    void drawCells(QPainter &painter);
    void drawGridLines(QPainter &painter);
    
    QVector<int>* getCurrentGrid();
};

#endif // GRIDEDITOR_H
