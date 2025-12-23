#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QScrollArea>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDockWidget>
#include "mapdata.h"
#include "grideditor.h"
#include "texturepalette.h"
#include "spriteeditor.h"
#include "cameramarker.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
private slots:
    // File menu
    void onNewMap();
    void onOpenMap();
    void onSaveMap();
    void onSaveMapAs();
    void onLoadTextures();
    void onExportText();
    void onExit();
    
    // Edit menu
    void onClearLevel();
    
    // View menu
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();
    
    // Level/mode selection
    void onLevelChanged(int level);
    void onModeChanged(int mode);
    
    // Grid events
    void onCellClicked(int x, int y, int value);
    void onCellHovered(int x, int y);
    
    // Texture selection
    void onTextureSelected(int textureId);
    
    // Tile type selection
    void onWallButtonClicked();
    void onDoorVButtonClicked();
    void onDoorHButtonClicked();
    
    // Slope controls
    void onSlopeTypeChanged(int index);
    void onSlopeStartHeightChanged(double value);
    void onSlopeEndHeightChanged(double value);
    void onSlopeInvertedChanged(bool checked);
    
private:
    // UI Components
    GridEditor *m_gridEditor;
    TexturePalette *m_texturePalette;
    SpriteEditor *m_spriteEditor;
    CameraMarker *m_cameraMarker;
    
    // Dock widgets
    QDockWidget *m_textureDock;
    QDockWidget *m_slopeDock;
    
    // Slope controls
    QComboBox *m_slopeTypeCombo;
    QDoubleSpinBox *m_slopeStartHeightSpin;
    QDoubleSpinBox *m_slopeEndHeightSpin;
    QCheckBox *m_slopeInvertedCheck;
    
    // Toolbar widgets
    QComboBox *m_levelCombo;
    QComboBox *m_modeCombo;
    QComboBox *m_tileTypeCombo;
    QSpinBox *m_selectedTextureSpinBox;
    QSpinBox *m_skyboxSpinBox;  // Selector de skybox
    QPushButton *m_wallButton;
    QPushButton *m_doorVButton;
    QPushButton *m_doorHButton;
    
    // Status bar
    QLabel *m_statusLabel;
    QLabel *m_coordsLabel;
    
    // Data
    MapData m_mapData;
    QString m_currentMapFile;
    QString m_currentFPGFile;
    
    // UI Setup
    void createActions();
    void createMenus();
    void createToolbars();
    void createDockWindows();
    void createStatusBar();
    
    // Actions
    QAction *m_newAction;
    QAction *m_openAction;
    QAction *m_saveAction;
    QAction *m_saveAsAction;
    QAction *m_loadTexturesAction;
    QAction *m_exportTextAction;
    QAction *m_exitAction;
    
    QAction *m_clearLevelAction;
    
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_zoomResetAction;
    
    // Helpers
    void updateWindowTitle();
    void updateStatusBar(const QString &message);
};

#endif // MAINWINDOW_H
