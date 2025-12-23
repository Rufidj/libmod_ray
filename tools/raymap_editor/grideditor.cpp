#include "grideditor.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <cmath>

GridEditor::GridEditor(QWidget *parent)
    : QWidget(parent)
    , m_mapData(nullptr)
    , m_currentLevel(0)
    , m_editMode(MODE_WALLS)
    , m_selectedTexture(1)
    , m_zoom(1.0f)
    , m_cellSize(32)
    , m_isPainting(false)
    , m_drawingSlope(false)
    , m_slopeType(1)
    , m_slopeStartHeight(0.0f)
    , m_slopeEndHeight(128.0f)
    , m_slopeInverted(false)
{
    setMouseTracking(true);
    setMinimumSize(512, 512);
}

void GridEditor::setMapData(MapData *data)
{
    m_mapData = data;
    update();
}

void GridEditor::setTextures(const QMap<int, QPixmap> &textures)
{
    m_textures = textures;
    update();
}

void GridEditor::setCurrentLevel(int level)
{
    if (level >= 0 && level < 3) {
        m_currentLevel = level;
        update();
    }
}

void GridEditor::setEditMode(EditMode mode)
{
    m_editMode = mode;
    update();
}

void GridEditor::setSelectedTexture(int textureId)
{
    m_selectedTexture = textureId;
}

void GridEditor::setSlopeType(int type)
{
    m_slopeType = type;
}

void GridEditor::setSlopeStartHeight(float height)
{
    m_slopeStartHeight = height;
}

void GridEditor::setSlopeEndHeight(float height)
{
    m_slopeEndHeight = height;
}

void GridEditor::setSlopeInverted(bool inverted)
{
    m_slopeInverted = inverted;
}

void GridEditor::setZoom(float zoom)
{
    m_zoom = qBound(0.25f, zoom, 4.0f);
    m_cellSize = static_cast<int>(32 * m_zoom);
    update();
}

QPoint GridEditor::getCellAtPosition(const QPoint &pos)
{
    int x = pos.x() / m_cellSize;
    int y = pos.y() / m_cellSize;
    
    // DEBUG: Imprimir coordenadas para diagnosticar offset
    qDebug() << "DEBUG getCellAtPosition: pos=" << pos << " cellSize=" << m_cellSize 
             << " zoom=" << m_zoom << " -> cell(" << x << "," << y << ")";
    
    return QPoint(x, y);
}

void GridEditor::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.fillRect(rect(), Qt::darkGray);
    
    if (!m_mapData) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "No hay mapa cargado");
        return;
    }
    
    drawCells(painter);
    drawGridLines(painter);
    
    // Dibujar slopes existentes
    for (const ThickWall &slope : m_mapData->thickWalls) {
        if (slope.type == 1) {  // RECT
            QRectF rect(slope.x / 128.0f * m_cellSize,
                       slope.y / 128.0f * m_cellSize,
                       slope.w / 128.0f * m_cellSize,
                       slope.h / 128.0f * m_cellSize);
            
            // Color según tipo de slope
            QColor color = slope.invertedSlope ? QColor(200, 100, 255, 100) : QColor(100, 200, 255, 100);
            painter.fillRect(rect, color);
            
            // Borde
            painter.setPen(QPen(slope.invertedSlope ? Qt::magenta : Qt::cyan, 2));
            painter.drawRect(rect);
            
            // Indicador de dirección
            painter.setPen(Qt::white);
            QString dir = slope.slopeType == 1 ? "W→E" : "N→S";
            painter.drawText(rect, Qt::AlignCenter, dir);
        }
    }
    
    // Dibujar preview de slope siendo dibujado
    if (m_drawingSlope && m_editMode == MODE_SLOPE) {
        QPoint currentPos = mapFromGlobal(QCursor::pos());
        QPoint cell = getCellAtPosition(currentPos);
        
        int x1 = qMin(m_slopeStart.x(), cell.x()) * m_cellSize;
        int y1 = qMin(m_slopeStart.y(), cell.y()) * m_cellSize;
        int x2 = (qMax(m_slopeStart.x(), cell.x()) + 1) * m_cellSize;
        int y2 = (qMax(m_slopeStart.y(), cell.y()) + 1) * m_cellSize;
        
        QRect previewRect(x1, y1, x2 - x1, y2 - y1);
        painter.fillRect(previewRect, QColor(255, 255, 0, 80));
        painter.setPen(QPen(Qt::yellow, 2, Qt::DashLine));
        painter.drawRect(previewRect);
    }
    
    // Dibujar spawn flags
    for (const SpawnFlag &flag : m_mapData->spawnFlags) {
        if (flag.level != m_currentLevel) continue;
        
        // Convertir coordenadas del mundo a píxeles
        float pixelX = (flag.x / 128.0f) * m_cellSize;
        float pixelY = (flag.y / 128.0f) * m_cellSize;
        
        // Dibujar bandera
        int flagSize = qMax(16, m_cellSize / 3);
        QPointF flagPos(pixelX, pixelY);
        
        // Círculo de fondo
        painter.setBrush(QColor(255, 200, 0, 200));
        painter.setPen(QPen(Qt::black, 2));
        painter.drawEllipse(flagPos, flagSize/2, flagSize/2);
        
        // Número de flag
        painter.setPen(Qt::black);
        QFont font = painter.font();
        font.setPointSize(qMax(8, m_cellSize / 4));
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(QRectF(pixelX - flagSize, pixelY - flagSize, 
                               flagSize*2, flagSize*2), 
                        Qt::AlignCenter, QString::number(flag.flagId));
    }
}

void GridEditor::mousePressEvent(QMouseEvent *event)
{
    if (!m_mapData) return;
    
    QPoint cell = getCellAtPosition(event->pos());
    
    if (cell.x() >= 0 && cell.x() < m_mapData->width &&
        cell.y() >= 0 && cell.y() < m_mapData->height) {
        
        m_isPainting = true;
        m_lastCell = cell;
        
        if (event->button() == Qt::LeftButton) {
            if (m_editMode == MODE_SLOPE) {
                // Iniciar dibujo de slope
                m_drawingSlope = true;
                m_slopeStart = cell;
                update();
            } else if (m_editMode == MODE_SPAWN_FLAGS) {
                // Colocar spawn flag
                // Calcular posición en el centro de la celda (en coordenadas del mundo)
                float worldX = (cell.x() + 0.5f) * 128.0f;  // TILE_SIZE = 128
                float worldY = (cell.y() + 0.5f) * 128.0f;
                
                // Buscar el siguiente ID de flag disponible
                int nextFlagId = 1;
                for (const SpawnFlag &flag : m_mapData->spawnFlags) {
                    if (flag.flagId >= nextFlagId) {
                        nextFlagId = flag.flagId + 1;
                    }
                }
                
                // Crear nueva flag
                SpawnFlag newFlag;
                newFlag.flagId = nextFlagId;
                newFlag.x = worldX;
                newFlag.y = worldY;
                newFlag.z = 0.0f;
                newFlag.level = m_currentLevel;
                
                m_mapData->spawnFlags.append(newFlag);
                
                emit spawnFlagPlaced(nextFlagId, cell.x(), cell.y());
                qDebug() << "Spawn flag" << nextFlagId << "colocada en" << worldX << worldY;
                update();
            } else if (m_editMode == MODE_FLOOR_HEIGHT) {
                // Aumentar altura en 0.25
                QVector<float> *heightGrid = m_mapData->getFloorHeightGrid(m_currentLevel);
                if (heightGrid) {
                    int index = cell.y() * m_mapData->width + cell.x();
                    (*heightGrid)[index] = qMin((*heightGrid)[index] + 0.25f, 1.0f);
                    update();
                }
            } else {
                paintCell(cell.x(), cell.y());
            }
        } else if (event->button() == Qt::RightButton) {
            if (m_editMode == MODE_SPAWN_FLAGS) {
                // Eliminar spawn flag en esta celda
                float worldX = (cell.x() + 0.5f) * 128.0f;
                float worldY = (cell.y() + 0.5f) * 128.0f;
                
                // Buscar flag cercana (dentro de 64 unidades)
                for (int i = m_mapData->spawnFlags.size() - 1; i >= 0; i--) {
                    const SpawnFlag &flag = m_mapData->spawnFlags[i];
                    float dx = flag.x - worldX;
                    float dy = flag.y - worldY;
                    float dist = sqrtf(dx*dx + dy*dy);
                    
                    if (dist < 64.0f && flag.level == m_currentLevel) {
                        qDebug() << "Eliminando spawn flag" << flag.flagId;
                        m_mapData->spawnFlags.removeAt(i);
                        update();
                        break;
                    }
                }
            } else if (m_editMode == MODE_FLOOR_HEIGHT) {
                // Disminuir altura en 0.25
                QVector<float> *heightGrid = m_mapData->getFloorHeightGrid(m_currentLevel);
                if (heightGrid) {
                    int index = cell.y() * m_mapData->width + cell.x();
                    (*heightGrid)[index] = qMax((*heightGrid)[index] - 0.25f, 0.0f);
                    update();
                }
            } else {
                // Borrar (poner a 0)
                QVector<int> *grid = getCurrentGrid();
                if (grid) {
                    (*grid)[cell.y() * m_mapData->width + cell.x()] = 0;
                    update();
                }
            }
        }
        
        QVector<int> *grid = getCurrentGrid();
        if (grid) {
            int value = (*grid)[cell.y() * m_mapData->width + cell.x()];
            emit cellClicked(cell.x(), cell.y(), value);
        }
    }
}

void GridEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_mapData) return;
    
    QPoint cell = getCellAtPosition(event->pos());
    
    if (cell.x() >= 0 && cell.x() < m_mapData->width &&
        cell.y() >= 0 && cell.y() < m_mapData->height) {
        
        emit cellHovered(cell.x(), cell.y());
        
        if (m_isPainting && cell != m_lastCell) {
            m_lastCell = cell;
            
            if (event->buttons() & Qt::LeftButton) {
                if (m_editMode == MODE_FLOOR_HEIGHT) {
                    QVector<float> *heightGrid = m_mapData->getFloorHeightGrid(m_currentLevel);
                    if (heightGrid) {
                        int index = cell.y() * m_mapData->width + cell.x();
                        (*heightGrid)[index] = qMin((*heightGrid)[index] + 0.25f, 1.0f);
                        update();
                    }
                } else if (m_editMode != MODE_SPAWN_FLAGS && m_editMode != MODE_SLOPE) {
                    // No pintar en modo spawn flags ni slope al arrastrar
                    paintCell(cell.x(), cell.y());
                }
            } else if (event->buttons() & Qt::RightButton) {
                if (m_editMode == MODE_FLOOR_HEIGHT) {
                    QVector<float> *heightGrid = m_mapData->getFloorHeightGrid(m_currentLevel);
                    if (heightGrid) {
                        int index = cell.y() * m_mapData->width + cell.x();
                        (*heightGrid)[index] = qMax((*heightGrid)[index] - 0.25f, 0.0f);
                        update();
                    }
                } else if (m_editMode != MODE_SPAWN_FLAGS && m_editMode != MODE_SLOPE) {
                    // No borrar en modo spawn flags ni slope al arrastrar
                    QVector<int> *grid = getCurrentGrid();
                    if (grid) {
                        (*grid)[cell.y() * m_mapData->width + cell.x()] = 0;
                        update();
                    }
                }
            }
        }
    }
}

void GridEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_drawingSlope && m_editMode == MODE_SLOPE) {
        // Crear ThickWall con slope
        QPoint cell = getCellAtPosition(event->pos());
        
        if (cell.x() >= 0 && cell.x() < m_mapData->width &&
            cell.y() >= 0 && cell.y() < m_mapData->height) {
            
            // Calcular rectángulo
            int x1 = qMin(m_slopeStart.x(), cell.x());
            int y1 = qMin(m_slopeStart.y(), cell.y());
            int x2 = qMax(m_slopeStart.x(), cell.x());
            int y2 = qMax(m_slopeStart.y(), cell.y());
            
            int w = x2 - x1 + 1;
            int h = y2 - y1 + 1;
            
            if (w > 0 && h > 0) {
                ThickWall slope;
                slope.type = 1;  // RECT
                slope.slopeType = m_slopeType;
                slope.x = x1 * 128.0f;  // TILE_SIZE = 128
                slope.y = y1 * 128.0f;
                slope.w = w * 128.0f;
                slope.h = h * 128.0f;
                slope.z = 0.0f;
                slope.startHeight = m_slopeStartHeight;
                slope.endHeight = m_slopeEndHeight;
                slope.tallerHeight = qMax(m_slopeStartHeight, m_slopeEndHeight);
                slope.invertedSlope = m_slopeInverted ? 1 : 0;
                slope.floorTextureID = m_selectedTexture;
                slope.ceilingTextureID = m_selectedTexture;
                
                // Generar ThinWalls (4 paredes) - Port exacto de createRectThickWall
                // Orden: 0=West, 1=East, 2=North, 3=South
                ThinWall west, east, north, south;
                
                // West wall (vertical, lado izquierdo)
                west.x1 = slope.x;
                west.y1 = slope.y;
                west.x2 = slope.x;
                west.y2 = slope.y + slope.h;
                west.horizontal = 0;
                west.wallType = m_selectedTexture;
                west.z = slope.z;
                
                // East wall (vertical, lado derecho)
                east.x1 = slope.x + slope.w;
                east.y1 = slope.y;
                east.x2 = slope.x + slope.w;
                east.y2 = slope.y + slope.h;
                east.horizontal = 0;
                east.wallType = m_selectedTexture;
                east.z = slope.z;
                
                // North wall (horizontal, lado superior)
                north.x1 = slope.x;
                north.y1 = slope.y;
                north.x2 = slope.x + slope.w;
                north.y2 = slope.y;
                north.horizontal = 1;
                north.wallType = m_selectedTexture;
                north.z = slope.z;
                
                // South wall (horizontal, lado inferior)
                south.x1 = slope.x;
                south.y1 = slope.y + slope.h;
                south.x2 = slope.x + slope.w;
                south.y2 = slope.y + slope.h;
                south.horizontal = 1;
                south.wallType = m_selectedTexture;
                south.z = slope.z;

                
                // Calcular slopes y alturas iniciales para las paredes
                if (slope.invertedSlope) {
                    float newStartHeight = slope.tallerHeight - slope.startHeight;
                    float newEndHeight = slope.tallerHeight - slope.endHeight;
                    if (newStartHeight == 0) newStartHeight = 1;
                    if (newEndHeight == 0) newEndHeight = 1;
                    
                    if (slope.slopeType == 1) { // WEST_EAST
                        slope.slope = (slope.endHeight - slope.startHeight) / slope.w;
                        west.height = newStartHeight; west.z = slope.z + slope.startHeight; west.slope = 0;
                        east.height = newEndHeight; east.z = slope.z + slope.endHeight; east.slope = 0;
                        north.height = slope.endHeight; north.z = slope.z; north.slope = slope.slope;
                        south.height = slope.endHeight; south.z = slope.z; south.slope = slope.slope;
                    } else { // NORTH_SOUTH
                        slope.slope = (slope.endHeight - slope.startHeight) / slope.h;
                        west.height = slope.endHeight; west.z = slope.z; west.slope = slope.slope;
                        east.height = slope.endHeight; east.z = slope.z; east.slope = slope.slope;
                        north.height = newStartHeight; north.z = slope.z + slope.startHeight; north.slope = 0;
                        south.height = newEndHeight; south.z = slope.z + slope.endHeight; south.slope = 0;
                    }
                } else {
                    if (slope.slopeType == 1) { // WEST_EAST
                        slope.slope = (slope.endHeight - slope.startHeight) / slope.w;
                        west.height = slope.startHeight; west.z = slope.z; west.slope = 0;
                        east.height = slope.endHeight; east.z = slope.z; east.slope = 0;
                        north.height = slope.endHeight; north.z = slope.z; north.slope = slope.slope;
                        south.height = slope.endHeight; south.z = slope.z; south.slope = slope.slope;
                    } else { // NORTH_SOUTH
                        slope.slope = (slope.endHeight - slope.startHeight) / slope.h;
                        west.height = slope.endHeight; west.z = slope.z; west.slope = slope.slope;
                        east.height = slope.endHeight; east.z = slope.z; east.slope = slope.slope;
                        north.height = slope.startHeight; north.z = slope.z; north.slope = 0;
                        south.height = slope.endHeight; south.z = slope.z; south.slope = 0;
                    }
                }
                
                west.hidden = 0; east.hidden = 0; north.hidden = 0; south.hidden = 0;

                slope.thinWalls.append(west);
                slope.thinWalls.append(east);
                slope.thinWalls.append(north);
                slope.thinWalls.append(south);
                
                m_mapData->thickWalls.append(slope);
                qDebug() << "Slope creado con" << slope.thinWalls.size() << "thin walls. Slope value:" << slope.slope;
            }
        }
        
        m_drawingSlope = false;
        update();
    }
    
    m_isPainting = false;
}

void GridEditor::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y() > 0 ? 0.1f : -0.1f;
    setZoom(m_zoom + delta);
}

void GridEditor::paintCell(int x, int y)
{
    if (!m_mapData) return;
    
    QVector<int> *grid = getCurrentGrid();
    if (!grid) return;
    
    int index = y * m_mapData->width + x;
    
    // DEBUG: Imprimir qué se está pintando
    qDebug() << "DEBUG paintCell: x=" << x << " y=" << y << " width=" << m_mapData->width 
             << " index=" << index << " texture=" << m_selectedTexture;
    
    (*grid)[index] = m_selectedTexture;
    
    // Si estamos pintando una pared (modo WALLS) y es una puerta (ID >= 1001),
    // asegurarnos de que hay floor y ceiling en esa celda
    if (m_editMode == MODE_WALLS && m_selectedTexture >= 1001) {
        QVector<int> *floorGrid = m_mapData->getFloorGrid(m_currentLevel);
        QVector<int> *ceilingGrid = m_mapData->getCeilingGrid(m_currentLevel);
        
        if (floorGrid && (*floorGrid)[index] == 0) {
            // Buscar floor de celda adyacente para copiar
            int floorTex = 0;
            // Intentar arriba, abajo, izquierda, derecha
            if (y > 0 && (*floorGrid)[(y-1) * m_mapData->width + x] > 0)
                floorTex = (*floorGrid)[(y-1) * m_mapData->width + x];
            else if (y < m_mapData->height-1 && (*floorGrid)[(y+1) * m_mapData->width + x] > 0)
                floorTex = (*floorGrid)[(y+1) * m_mapData->width + x];
            else if (x > 0 && (*floorGrid)[y * m_mapData->width + (x-1)] > 0)
                floorTex = (*floorGrid)[y * m_mapData->width + (x-1)];
            else if (x < m_mapData->width-1 && (*floorGrid)[y * m_mapData->width + (x+1)] > 0)
                floorTex = (*floorGrid)[y * m_mapData->width + (x+1)];
            
            if (floorTex > 0) {
                (*floorGrid)[index] = floorTex;
            }
        }
        
        if (ceilingGrid && (*ceilingGrid)[index] == 0) {
            // Buscar ceiling de celda adyacente para copiar
            int ceilingTex = 0;
            if (y > 0 && (*ceilingGrid)[(y-1) * m_mapData->width + x] > 0)
                ceilingTex = (*ceilingGrid)[(y-1) * m_mapData->width + x];
            else if (y < m_mapData->height-1 && (*ceilingGrid)[(y+1) * m_mapData->width + x] > 0)
                ceilingTex = (*ceilingGrid)[(y+1) * m_mapData->width + x];
            else if (x > 0 && (*ceilingGrid)[y * m_mapData->width + (x-1)] > 0)
                ceilingTex = (*ceilingGrid)[y * m_mapData->width + (x-1)];
            else if (x < m_mapData->width-1 && (*ceilingGrid)[y * m_mapData->width + (x+1)] > 0)
                ceilingTex = (*ceilingGrid)[y * m_mapData->width + (x+1)];
            
            if (ceilingTex > 0) {
                (*ceilingGrid)[index] = ceilingTex;
            }
        }
    }
    
    update();
}

void GridEditor::drawCells(QPainter &painter)
{
    // Modo especial para floor height
    if (m_editMode == MODE_FLOOR_HEIGHT) {
        QVector<float> *heightGrid = m_mapData->getFloorHeightGrid(m_currentLevel);
        if (!heightGrid) return;
        
        for (int y = 0; y < m_mapData->height; y++) {
            for (int x = 0; x < m_mapData->width; x++) {
                float height = (*heightGrid)[y * m_mapData->width + x];
                
                QRect cellRect(x * m_cellSize, y * m_cellSize, m_cellSize, m_cellSize);
                
                // Color coding por altura
                QColor color;
                if (height <= 0.0f) {
                    color = QColor(34, 139, 34);  // Verde oscuro (0.0)
                } else if (height <= 0.25f) {
                    color = QColor(76, 175, 80);  // Verde (0.25)
                } else if (height <= 0.5f) {
                    color = QColor(255, 235, 59); // Amarillo (0.5)
                } else if (height <= 0.75f) {
                    color = QColor(255, 152, 0);  // Naranja (0.75)
                } else {
                    color = QColor(244, 67, 54);  // Rojo (1.0)
                }
                
                painter.fillRect(cellRect, color);
                
                // Dibujar valor numérico
                painter.setPen(Qt::black);
                QFont font = painter.font();
                font.setPointSize(qMax(6, m_cellSize / 6));
                font.setBold(true);
                painter.setFont(font);
                painter.drawText(cellRect, Qt::AlignCenter, QString::number(height, 'f', 2));
            }
        }
        return;
    }
    
    // Modo normal (walls, floor, ceiling, spawn_flags)
    QVector<int> *grid = getCurrentGrid();
    if (!grid) return;
    
    for (int y = 0; y < m_mapData->height; y++) {
        for (int x = 0; x < m_mapData->width; x++) {
            int value = (*grid)[y * m_mapData->width + x];
            
            QRect cellRect(x * m_cellSize, y * m_cellSize, m_cellSize, m_cellSize);
            
            if (value == 0) {
                // Celda vacía
                painter.fillRect(cellRect, QColor(50, 50, 50));
            } else {
                // Dibujar textura si está disponible
                if (m_textures.contains(value)) {
                    QPixmap texture = m_textures[value];
                    painter.drawPixmap(cellRect, texture);
                    
                    // Indicador visual para puertas
                    if (value >= 1001 && value <= 1500) {
                        // Puerta vertical - borde azul
                        painter.setPen(QPen(QColor(33, 150, 243), 3));
                        painter.drawRect(cellRect.adjusted(1, 1, -1, -1));
                        painter.setPen(Qt::white);
                        painter.drawText(cellRect.adjusted(0, 0, 0, -cellRect.height()/2), 
                                       Qt::AlignCenter, "🚪V");
                    } else if (value >= 1501) {
                        // Puerta horizontal - borde naranja
                        painter.setPen(QPen(QColor(255, 152, 0), 3));
                        painter.drawRect(cellRect.adjusted(1, 1, -1, -1));
                        painter.setPen(Qt::white);
                        painter.drawText(cellRect.adjusted(0, 0, 0, -cellRect.height()/2), 
                                       Qt::AlignCenter, "🚪H");
                    }
                } else {
                    // Si no hay textura, dibujar color según el ID
                    QColor color;
                    QString label;
                    
                    if (value >= 1 && value <= 999) {
                        // Pared normal
                        color = QColor(100, 100, 150);
                        label = QString::number(value);
                    } else if (value >= 1001 && value <= 1500) {
                        // Puerta vertical
                        color = QColor(33, 150, 243);
                        label = QString("🚪V\n%1").arg(value);
                    } else {
                        // Puerta horizontal
                        color = QColor(255, 152, 0);
                        label = QString("🚪H\n%1").arg(value);
                    }
                    painter.fillRect(cellRect, color);
                    
                    // Dibujar etiqueta
                    painter.setPen(Qt::white);
                    QFont font = painter.font();
                    font.setPointSize(qMax(6, m_cellSize / 8));
                    painter.setFont(font);
                    painter.drawText(cellRect, Qt::AlignCenter, label);
                }
            }
        }
    }
}

void GridEditor::drawGridLines(QPainter &painter)
{
    if (!m_mapData) return;
    
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    
    // Líneas verticales
    for (int x = 0; x <= m_mapData->width; x++) {
        int px = x * m_cellSize;
        painter.drawLine(px, 0, px, m_mapData->height * m_cellSize);
    }
    
    // Líneas horizontales
    for (int y = 0; y <= m_mapData->height; y++) {
        int py = y * m_cellSize;
        painter.drawLine(0, py, m_mapData->width * m_cellSize, py);
    }
}

QVector<int>* GridEditor::getCurrentGrid()
{
    if (!m_mapData) return nullptr;
    
    switch (m_editMode) {
        case MODE_WALLS:
            return m_mapData->getGrid(m_currentLevel);
        case MODE_FLOOR:
            return m_mapData->getFloorGrid(m_currentLevel);
        case MODE_CEILING:
            return m_mapData->getCeilingGrid(m_currentLevel);
        case MODE_SPAWN_FLAGS:
            // En modo spawn flags, mostrar el grid de paredes de fondo
            return m_mapData->getGrid(m_currentLevel);
        default:
            return nullptr;
    }
}
