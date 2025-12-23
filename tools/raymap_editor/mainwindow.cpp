#include "mainwindow.h"
#include "fpgloader.h"
#include "raymapformat.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include <QApplication>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("RayMap Editor");
    resize(1200, 800);
    
    // Crear editor de grid central
    m_gridEditor = new GridEditor(this);
    m_gridEditor->setMapData(&m_mapData);
    
    // Envolver en scroll area
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(m_gridEditor);
    scrollArea->setWidgetResizable(false);
    setCentralWidget(scrollArea);
    
    // Crear componentes de UI
    createActions();
    createMenus();
    createToolbars();
    createDockWindows();
    createStatusBar();
    
    // Conectar señales
    connect(m_gridEditor, &GridEditor::cellClicked, this, &MainWindow::onCellClicked);
    connect(m_gridEditor, &GridEditor::cellHovered, this, &MainWindow::onCellHovered);
    
    updateWindowTitle();
    updateStatusBar("Listo");
}

MainWindow::~MainWindow()
{
}

void MainWindow::createActions()
{
    // File menu
    m_newAction = new QAction(tr("&Nuevo Mapa"), this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &MainWindow::onNewMap);
    
    m_openAction = new QAction(tr("&Abrir Mapa..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpenMap);
    
    m_saveAction = new QAction(tr("&Guardar"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveMap);
    
    m_saveAsAction = new QAction(tr("Guardar &Como..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveMapAs);
    
    m_loadTexturesAction = new QAction(tr("Cargar &Texturas FPG..."), this);
    connect(m_loadTexturesAction, &QAction::triggered, this, &MainWindow::onLoadTextures);
    
    m_exportTextAction = new QAction(tr("&Exportar a Texto..."), this);
    connect(m_exportTextAction, &QAction::triggered, this, &MainWindow::onExportText);
    
    m_exitAction = new QAction(tr("&Salir"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::onExit);
    
    // Edit menu
    m_clearLevelAction = new QAction(tr("&Limpiar Nivel Actual"), this);
    connect(m_clearLevelAction, &QAction::triggered, this, &MainWindow::onClearLevel);
    
    // View menu
    m_zoomInAction = new QAction(tr("&Acercar"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::onZoomIn);
    
    m_zoomOutAction = new QAction(tr("&Alejar"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::onZoomOut);
    
    m_zoomResetAction = new QAction(tr("&Zoom 100%"), this);
    connect(m_zoomResetAction, &QAction::triggered, this, &MainWindow::onZoomReset);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&Archivo"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_loadTexturesAction);
    fileMenu->addAction(m_exportTextAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);
    
    QMenu *editMenu = menuBar()->addMenu(tr("&Editar"));
    editMenu->addAction(m_clearLevelAction);
    
    QMenu *viewMenu = menuBar()->addMenu(tr("&Ver"));
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_zoomResetAction);
}

void MainWindow::createToolbars()
{
    QToolBar *toolbar = addToolBar(tr("Principal"));
    toolbar->setIconSize(QSize(32, 32));
    
    // Selector de nivel
    toolbar->addWidget(new QLabel(" <b>Nivel:</b> "));
    m_levelCombo = new QComboBox(this);
    m_levelCombo->addItem("🔷 Nivel 0");
    m_levelCombo->addItem("🔶 Nivel 1");
    m_levelCombo->addItem("🔴 Nivel 2");
    m_levelCombo->setMinimumWidth(120);
    connect(m_levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLevelChanged);
    toolbar->addWidget(m_levelCombo);
    
    toolbar->addSeparator();
    
    // Selector de modo
    toolbar->addWidget(new QLabel(" <b>Modo:</b> "));
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("🧱 Paredes");
    m_modeCombo->addItem("⬛ Suelo");
    m_modeCombo->addItem("⬜ Techo");
    m_modeCombo->addItem("🚩 Spawn Flags");
    // DESHABILITADO: Altura y rampas por el momento
    // m_modeCombo->addItem("📐 Altura Suelo");
    // m_modeCombo->addItem("📊 Slope/Rampa");
    m_modeCombo->setMinimumWidth(140);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);
    toolbar->addWidget(m_modeCombo);
    
    toolbar->addSeparator();
    
    // Tipo de tile (pared, puerta vertical, puerta horizontal)
    toolbar->addWidget(new QLabel(" <b>Tipo:</b> "));
    
    m_wallButton = new QPushButton("🧱 Pared", this);
    m_wallButton->setCheckable(true);
    m_wallButton->setChecked(true);
    m_wallButton->setMinimumWidth(80);
    m_wallButton->setStyleSheet("QPushButton:checked { background-color: #4CAF50; color: white; }");
    connect(m_wallButton, &QPushButton::clicked, this, &MainWindow::onWallButtonClicked);
    toolbar->addWidget(m_wallButton);
    
    m_doorVButton = new QPushButton("🚪 Puerta V", this);
    m_doorVButton->setCheckable(true);
    m_doorVButton->setMinimumWidth(90);
    m_doorVButton->setStyleSheet("QPushButton:checked { background-color: #2196F3; color: white; }");
    connect(m_doorVButton, &QPushButton::clicked, this, &MainWindow::onDoorVButtonClicked);
    toolbar->addWidget(m_doorVButton);
    
    m_doorHButton = new QPushButton("🚪 Puerta H", this);
    m_doorHButton->setCheckable(true);
    m_doorHButton->setMinimumWidth(90);
    m_doorHButton->setStyleSheet("QPushButton:checked { background-color: #FF9800; color: white; }");
    connect(m_doorHButton, &QPushButton::clicked, this, &MainWindow::onDoorHButtonClicked);
    toolbar->addWidget(m_doorHButton);
    
    toolbar->addSeparator();
    
    // Textura seleccionada
    toolbar->addWidget(new QLabel(" <b>Textura ID:</b> "));
    m_selectedTextureSpinBox = new QSpinBox(this);
    m_selectedTextureSpinBox->setRange(0, 9999);
    m_selectedTextureSpinBox->setValue(1);
    m_selectedTextureSpinBox->setMinimumWidth(80);
    connect(m_selectedTextureSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int value) {
                // Ajustar valor según el tipo de tile
                int adjustedValue = value;
                if (m_doorVButton->isChecked()) {
                    // Puertas verticales: 1000 + texture_id
                    adjustedValue = 1000 + value;
                } else if (m_doorHButton->isChecked()) {
                    // Puertas horizontales: 1500 + texture_id
                    adjustedValue = 1500 + value;
                }
                m_gridEditor->setSelectedTexture(adjustedValue);
                updateStatusBar(QString("Textura seleccionada: %1").arg(adjustedValue));
            });
    toolbar->addWidget(m_selectedTextureSpinBox);
    
    toolbar->addSeparator();
    
    // Selector de Skybox
    toolbar->addWidget(new QLabel(" <b>Skybox ID:</b> "));
    m_skyboxSpinBox = new QSpinBox(this);
    m_skyboxSpinBox->setRange(0, 9999);
    m_skyboxSpinBox->setValue(0);  // 0 = sin skybox
    m_skyboxSpinBox->setMinimumWidth(80);
    m_skyboxSpinBox->setToolTip("ID de textura para el skybox (0 = sin skybox)");
    connect(m_skyboxSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int value) {
                m_mapData.skyTextureID = value;
                updateStatusBar(QString("Skybox ID: %1").arg(value));
            });
    toolbar->addWidget(m_skyboxSpinBox);
}

void MainWindow::createDockWindows()
{
    // Dock de texturas
    m_textureDock = new QDockWidget(tr("Texturas"), this);
    m_texturePalette = new TexturePalette(this);
    m_textureDock->setWidget(m_texturePalette);
    addDockWidget(Qt::RightDockWidgetArea, m_textureDock);
    
    connect(m_texturePalette, &TexturePalette::textureSelected,
            this, &MainWindow::onTextureSelected);
    
    // DESHABILITADO: Controles de Slope por el momento
    /*
    // Dock de controles de slope
    m_slopeDock = new QDockWidget(tr("Controles de Slope"), this);
    QWidget *slopeWidget = new QWidget();
    QVBoxLayout *slopeLayout = new QVBoxLayout(slopeWidget);
    
    // Tipo de slope
    slopeLayout->addWidget(new QLabel("<b>Tipo de Slope:</b>"));
    m_slopeTypeCombo = new QComboBox();
    m_slopeTypeCombo->addItem("West → East (1)");
    m_slopeTypeCombo->addItem("North → South (2)");
    connect(m_slopeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSlopeTypeChanged);
    slopeLayout->addWidget(m_slopeTypeCombo);
    
    // Altura inicial
    slopeLayout->addWidget(new QLabel("<b>Altura Inicial:</b>"));
    m_slopeStartHeightSpin = new QDoubleSpinBox();
    m_slopeStartHeightSpin->setRange(0, 512);
    m_slopeStartHeightSpin->setValue(0);
    m_slopeStartHeightSpin->setSingleStep(16);
    connect(m_slopeStartHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onSlopeStartHeightChanged);
    slopeLayout->addWidget(m_slopeStartHeightSpin);
    
    // Altura final
    slopeLayout->addWidget(new QLabel("<b>Altura Final:</b>"));
    m_slopeEndHeightSpin = new QDoubleSpinBox();
    m_slopeEndHeightSpin->setRange(0, 512);
    m_slopeEndHeightSpin->setValue(128);
    m_slopeEndHeightSpin->setSingleStep(16);
    connect(m_slopeEndHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onSlopeEndHeightChanged);
    slopeLayout->addWidget(m_slopeEndHeightSpin);
    
    // Invertido (ceiling slope)
    m_slopeInvertedCheck = new QCheckBox("Invertido (Ceiling Slope)");
    connect(m_slopeInvertedCheck, &QCheckBox::toggled,
            this, &MainWindow::onSlopeInvertedChanged);
    slopeLayout->addWidget(m_slopeInvertedCheck);
    
    slopeLayout->addStretch();
    m_slopeDock->setWidget(slopeWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_slopeDock);
    */
}

void MainWindow::createStatusBar()
{
    m_statusLabel = new QLabel("Listo");
    m_coordsLabel = new QLabel("X: -, Y: -");
    
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_coordsLabel);
}

void MainWindow::onNewMap()
{
    bool ok;
    int width = QInputDialog::getInt(this, tr("Nuevo Mapa"),
                                     tr("Ancho del mapa:"), 16, 4, 128, 1, &ok);
    if (!ok) return;
    
    int height = QInputDialog::getInt(this, tr("Nuevo Mapa"),
                                      tr("Alto del mapa:"), 16, 4, 128, 1, &ok);
    if (!ok) return;
    
    m_mapData.resize(width, height);
    m_currentMapFile.clear();
    
    // Resetear skybox a 0 (sin skybox)
    m_mapData.skyTextureID = 0;
    m_skyboxSpinBox->setValue(0);
    
    m_gridEditor->update();
    updateWindowTitle();
    updateStatusBar(QString("Nuevo mapa creado: %1x%2").arg(width).arg(height));
}

void MainWindow::onOpenMap()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Abrir Mapa"), "", tr("RayMap Files (*.raymap);;All Files (*)"));
    
    if (filename.isEmpty()) return;
    
    // Crear diálogo de progreso
    QProgressDialog progress("Cargando mapa...", QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setCancelButton(nullptr);
    
    auto progressCallback = [&progress](const QString& msg) {
        progress.setLabelText(msg);
        QApplication::processEvents();
    };
    
    if (RayMapFormat::loadMap(filename, m_mapData, progressCallback)) {
        m_currentMapFile = filename;
        m_gridEditor->update();
        
        // Actualizar skybox spinbox con el valor cargado
        m_skyboxSpinBox->setValue(m_mapData.skyTextureID);
        
        updateWindowTitle();
        updateStatusBar(QString("Mapa cargado: %1").arg(filename));
    } else {
        QMessageBox::critical(this, tr("Error"),
            tr("No se pudo cargar el mapa"));
    }
}

void MainWindow::onSaveMap()
{
    if (m_currentMapFile.isEmpty()) {
        onSaveMapAs();
        return;
    }
    
    // Crear diálogo de progreso
    QProgressDialog progress("Guardando mapa...", QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setCancelButton(nullptr); // No permitir cancelar
    
    auto progressCallback = [&progress](const QString& msg) {
        progress.setLabelText(msg);
        QApplication::processEvents();
    };
    
    if (RayMapFormat::saveMap(m_currentMapFile, m_mapData, progressCallback)) {
        updateStatusBar(QString("Mapa guardado: %1").arg(m_currentMapFile));
    } else {
        QMessageBox::critical(this, tr("Error"),
            tr("No se pudo guardar el mapa"));
    }
}

void MainWindow::onSaveMapAs()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Guardar Mapa Como"), "", tr("RayMap Files (*.raymap);;All Files (*)"));
    
    if (filename.isEmpty()) return;
    
    // Crear diálogo de progreso
    QProgressDialog progress("Guardando mapa...", QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setCancelButton(nullptr);
    
    auto progressCallback = [&progress](const QString& msg) {
        progress.setLabelText(msg);
        QApplication::processEvents();
    };
    
    if (RayMapFormat::saveMap(filename, m_mapData, progressCallback)) {
        m_currentMapFile = filename;
        updateWindowTitle();
        updateStatusBar(QString("Mapa guardado: %1").arg(filename));
    } else {
        QMessageBox::critical(this, tr("Error"),
            tr("No se pudo guardar el mapa"));
    }
}

void MainWindow::onLoadTextures()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Cargar Texturas FPG"), "", tr("FPG Files (*.fpg);;All Files (*)"));
    
    if (filename.isEmpty()) return;
    
    // Crear diálogo de progreso
    QProgressDialog progress("Cargando texturas FPG...", "Cancelar", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    
    // Callback de progreso
    auto progressCallback = [&progress](int current, int total, const QString& texName) {
        progress.setLabelText(QString("Cargando %1...").arg(texName));
        if (total > 0) {
            progress.setMaximum(total);
            progress.setValue(current);
        }
        QApplication::processEvents(); // Actualizar UI
    };
    
    if (FPGLoader::loadFPG(filename, m_mapData.textures, progressCallback)) {
        m_currentFPGFile = filename;
        
        // Actualizar paleta de texturas
        m_texturePalette->setTextures(m_mapData.textures);
        
        // Actualizar grid editor con texturas
        QMap<int, QPixmap> textureMap = FPGLoader::getTextureMap(m_mapData.textures);
        m_gridEditor->setTextures(textureMap);
        
        progress.setValue(100);
        updateStatusBar(QString("Texturas cargadas: %1 (%2 texturas)")
                        .arg(filename)
                        .arg(m_mapData.textures.size()));
        
        QMessageBox::information(this, tr("Éxito"),
            QString("Se cargaron %1 texturas desde el archivo FPG")
                .arg(m_mapData.textures.size()));
    } else {
        QMessageBox::critical(this, tr("Error"),
            tr("No se pudo cargar el archivo FPG"));
    }
}

void MainWindow::onExportText()
{
    QString directory = QFileDialog::getExistingDirectory(this,
        tr("Seleccionar Directorio para Exportar"));
    
    if (directory.isEmpty()) return;
    
    if (RayMapFormat::exportToText(directory, m_mapData)) {
        updateStatusBar(QString("Mapa exportado a: %1").arg(directory));
        QMessageBox::information(this, tr("Éxito"),
            tr("Mapa exportado exitosamente a formato de texto"));
    } else {
        QMessageBox::critical(this, tr("Error"),
            tr("No se pudo exportar el mapa"));
    }
}

void MainWindow::onExit()
{
    close();
}

void MainWindow::onClearLevel()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        tr("Confirmar"), tr("¿Limpiar el nivel actual?"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        QVector<int> *grid = nullptr;
        
        switch (m_modeCombo->currentIndex()) {
            case 0: grid = m_mapData.getGrid(m_levelCombo->currentIndex()); break;
            case 1: grid = m_mapData.getFloorGrid(m_levelCombo->currentIndex()); break;
            case 2: grid = m_mapData.getCeilingGrid(m_levelCombo->currentIndex()); break;
            case 3: 
                // Floor height usa floats, no ints - manejar separadamente
                {
                    QVector<float> *heightGrid = m_mapData.getFloorHeightGrid(m_levelCombo->currentIndex());
                    if (heightGrid) {
                        heightGrid->fill(0.0f);
                        m_gridEditor->update();
                        updateStatusBar("Altura de suelo limpiada");
                    }
                }
                return;
        }
        
        if (grid) {
            grid->fill(0);
            m_gridEditor->update();
            updateStatusBar("Nivel limpiado");
        }
    }
}

void MainWindow::onZoomIn()
{
    m_gridEditor->setZoom(m_gridEditor->property("zoom").toFloat() + 0.25f);
}

void MainWindow::onZoomOut()
{
    m_gridEditor->setZoom(m_gridEditor->property("zoom").toFloat() - 0.25f);
}

void MainWindow::onZoomReset()
{
    m_gridEditor->setZoom(1.0f);
}

void MainWindow::onLevelChanged(int level)
{
    m_gridEditor->setCurrentLevel(level);
    updateStatusBar(QString("Nivel cambiado a: %1").arg(level));
}

void MainWindow::onModeChanged(int mode)
{
    m_gridEditor->setEditMode(static_cast<GridEditor::EditMode>(mode));
    QString modeName;
    switch (mode) {
        case 0: modeName = "Paredes"; break;
        case 1: modeName = "Suelo"; break;
        case 2: modeName = "Techo"; break;
        case 3: modeName = "Spawn Flags"; break;
        case 4: modeName = "Altura de Suelo"; break;
        case 5: modeName = "Slope/Rampa"; break;
    }
    updateStatusBar(QString("Modo cambiado a: %1").arg(modeName));
}

void MainWindow::onCellClicked(int x, int y, int value)
{
    Q_UNUSED(value);
    m_coordsLabel->setText(QString("X: %1, Y: %2").arg(x).arg(y));
}

void MainWindow::onCellHovered(int x, int y)
{
    m_coordsLabel->setText(QString("X: %1, Y: %2").arg(x).arg(y));
}

void MainWindow::onTextureSelected(int textureId)
{
    // Determinar tipo según el ID
    if (textureId >= 1500) {
        m_doorHButton->setChecked(true);
        m_wallButton->setChecked(false);
        m_doorVButton->setChecked(false);
        m_selectedTextureSpinBox->setValue(textureId - 1500);
    } else if (textureId >= 1000 && textureId < 1500) {
        m_doorVButton->setChecked(true);
        m_wallButton->setChecked(false);
        m_doorHButton->setChecked(false);
        m_selectedTextureSpinBox->setValue(textureId - 1000);
    } else {
        m_wallButton->setChecked(true);
        m_doorVButton->setChecked(false);
        m_doorHButton->setChecked(false);
        m_selectedTextureSpinBox->setValue(textureId);
    }
    
    m_gridEditor->setSelectedTexture(textureId);
    updateStatusBar(QString("Textura seleccionada: %1").arg(textureId));
}

void MainWindow::onWallButtonClicked()
{
    m_wallButton->setChecked(true);
    m_doorVButton->setChecked(false);
    m_doorHButton->setChecked(false);
    
    int value = m_selectedTextureSpinBox->value();
    m_gridEditor->setSelectedTexture(value);
    updateStatusBar("Modo: Pared normal");
}

void MainWindow::onDoorVButtonClicked()
{
    m_wallButton->setChecked(false);
    m_doorVButton->setChecked(true);
    m_doorHButton->setChecked(false);
    
    int value = 1000 + m_selectedTextureSpinBox->value();
    m_gridEditor->setSelectedTexture(value);
    updateStatusBar(QString("Modo: Puerta Vertical (ID: %1)").arg(value));
}

void MainWindow::onDoorHButtonClicked()
{
    m_wallButton->setChecked(false);
    m_doorVButton->setChecked(false);
    m_doorHButton->setChecked(true);
    
    int value = 1500 + m_selectedTextureSpinBox->value();
    m_gridEditor->setSelectedTexture(value);
    updateStatusBar(QString("Modo: Puerta Horizontal (ID: %1)").arg(value));
}

void MainWindow::onSlopeTypeChanged(int index)
{
    m_gridEditor->setSlopeType(index + 1);  // 1=WEST_EAST, 2=NORTH_SOUTH
    updateStatusBar(QString("Tipo de slope: %1").arg(index == 0 ? "West→East" : "North→South"));
}

void MainWindow::onSlopeStartHeightChanged(double value)
{
    m_gridEditor->setSlopeStartHeight(static_cast<float>(value));
    updateStatusBar(QString("Altura inicial: %1").arg(value));
}

void MainWindow::onSlopeEndHeightChanged(double value)
{
    m_gridEditor->setSlopeEndHeight(static_cast<float>(value));
    updateStatusBar(QString("Altura final: %1").arg(value));
}

void MainWindow::onSlopeInvertedChanged(bool checked)
{
    m_gridEditor->setSlopeInverted(checked);
    updateStatusBar(checked ? "Slope invertido (ceiling)" : "Slope normal (floor)");
}

void MainWindow::updateWindowTitle()
{
    QString title = "RayMap Editor";
    if (!m_currentMapFile.isEmpty()) {
        title += " - " + m_currentMapFile;
    } else {
        title += " - [Sin guardar]";
    }
    setWindowTitle(title);
}

void MainWindow::updateStatusBar(const QString &message)
{
    m_statusLabel->setText(message);
}
