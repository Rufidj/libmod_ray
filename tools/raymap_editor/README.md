# RayMap Editor - Editor Visual de Mapas para libmod_ray

Editor visual de mapas con Qt Creator para el módulo de raycasting de BennuGD2.

## Características

- ✅ **Editor visual de grids** con 3 niveles (0, 1, 2)
- ✅ **Carga de texturas FPG** de BennuGD2 (con soporte gzip)
- ✅ **Paleta de texturas** con vista previa
- ✅ **Edición de paredes, suelos y techos**
- ✅ **Soporte completo para puertas**
  - Puertas verticales (ID 1001-1500) con indicador 🚪V
  - Puertas horizontales (ID 1501+) con indicador 🚪H
  - Botones dedicados con código de colores
- ✅ **Interfaz visual mejorada**
  - Iconos emoji para mejor identificación
  - Botones con colores (verde/azul/naranja)
  - Indicadores visuales en el grid
- ✅ **Pintar con mouse** (click izquierdo = pintar, click derecho = borrar)
- ✅ **Zoom** con rueda del mouse
- ✅ **Guardar/Cargar** archivos .raymap (versión 2 con cámara)
- ✅ **Retrocompatibilidad** con mapas versión 1
- ✅ **Exportar a texto** (formato CSV)
- 🚧 **Colocación de sprites** (próximamente)
- 🚧 **Marcador de cámara** (próximamente)

## Requisitos

### Dependencias

```bash
# Ubuntu/Debian
sudo apt install qt5-default qtbase5-dev libqt5widgets5 zlib1g-dev cmake

# Fedora/RHEL
sudo dnf install qt5-qtbase-devel zlib-devel cmake

# Arch Linux
sudo pacman -S qt5-base zlib cmake
```

## Compilación

### Opción 1: Con qmake (recomendado)

```bash
cd /home/ruben/BennuGD2/modules/libmod_ray/tools/raymap_editor
qmake raymap_editor.pro
make
```

### Opción 2: Con CMake

```bash
cd /home/ruben/BennuGD2/modules/libmod_ray/tools/raymap_editor
mkdir build && cd build
cmake ..
make
```

## Uso

### 1. Iniciar el editor

```bash
./raymap_editor
```

### 2. Cargar texturas

1. **Archivo → Cargar Texturas FPG...**
2. Seleccionar el archivo `textures.fpg`
3. Las texturas aparecerán en el panel derecho

### 3. Crear un nuevo mapa

1. **Archivo → Nuevo Mapa**
2. Especificar dimensiones (ej: 16x16)
3. Seleccionar textura de la paleta
4. Pintar en el grid con el mouse

### 4. Editar niveles

- Usar el selector **"Nivel"** en la barra de herramientas
- Cambiar entre Nivel 0, 1, 2

### 5. Editar modo

- **Paredes**: Editar paredes del mapa
- **Suelo**: Editar texturas del suelo
- **Techo**: Editar texturas del techo

### 6. Añadir puertas

#### Puertas Verticales (🚪V)
1. Click en el botón **🚪 Puerta V** (azul)
2. El ID se ajusta automáticamente al rango 1001-1500
3. Pintar en el mapa como una pared normal
4. Las puertas aparecen con borde azul y etiqueta 🚪V

#### Puertas Horizontales (🚪H)
1. Click en el botón **🚪 Puerta H** (naranja)
2. El ID se ajusta automáticamente al rango 1501+
3. Pintar en el mapa
4. Las puertas aparecen con borde naranja y etiqueta 🚪H

**Nota**: El spinner "Textura ID" cambia automáticamente según el tipo:
- Pared: ID directo (1-999)
- Puerta V: ID + 1001
- Puerta H: ID + 1501

### 7. Controles del mouse

- **Click izquierdo + arrastrar**: Pintar textura seleccionada
- **Click derecho + arrastrar**: Borrar (poner a 0)
- **Rueda del mouse**: Zoom in/out

### 8. Guardar el mapa

1. **Archivo → Guardar Como...**
2. Guardar como archivo `.raymap`

## Formato de Archivo

### .raymap Versión 2

El editor guarda mapas en formato `.raymap` versión 2, que incluye:

- Grids de paredes (3 niveles)
- Grids de suelo (3 niveles)
- Grids de techo (3 niveles)
- Lista de sprites
- **Posición inicial de cámara** (nuevo en v2)

### Retrocompatibilidad

El editor puede abrir mapas versión 1 (sin cámara) y los convierte automáticamente a versión 2 al guardar.

## Integración con BennuGD2

Para usar los mapas creados con el editor en BennuGD2, necesitas actualizar el módulo `libmod_ray` para soportar la posición de cámara (ver sección siguiente).

```bennugd
import "mod_ray";

process main()
begin
    fpg_textures = load_fpg("textures.fpg");
    RAY_INIT(800, 600, 90, 2);
    RAY_LOAD_MAP("mi_mapa.raymap", fpg_textures);
    
    // Usar posición de cámara del mapa (requiere módulo actualizado)
    if (RAY_HAS_MAP_CAMERA())
        RAY_SET_CAMERA_FROM_MAP();
    end
    
    while (!key(_ESC))
        RAY_RENDER();
        frame;
    end
    
    RAY_SHUTDOWN();
end
```

## Actualización del Módulo

Para soportar la posición de cámara en los mapas, se necesita actualizar `libmod_ray`. Los cambios están documentados en el plan de implementación.

## Estructura del Proyecto

```
raymap_editor/
├── main.cpp              # Punto de entrada
├── mainwindow.h/cpp      # Ventana principal
├── grideditor.h/cpp      # Widget de edición de grid
├── texturepalette.h/cpp  # Paleta de texturas
├── fpgloader.h/cpp       # Cargador de archivos FPG
├── raymapformat.h/cpp    # Lector/escritor de .raymap
├── mapdata.h             # Estructuras de datos
├── spriteeditor.h/cpp    # Editor de sprites (stub)
├── cameramarker.h/cpp    # Marcador de cámara (stub)
├── raymap_editor.pro     # Proyecto qmake
└── CMakeLists.txt        # Proyecto CMake
```

## Atajos de Teclado

- **Ctrl+N**: Nuevo mapa
- **Ctrl+O**: Abrir mapa
- **Ctrl+S**: Guardar
- **Ctrl+Shift+S**: Guardar como
- **Ctrl+Q**: Salir
- **Ctrl++**: Acercar zoom
- **Ctrl+-**: Alejar zoom

## Próximas Características

- [ ] Editor de sprites con drag & drop
- [ ] Marcador visual de posición de cámara
- [ ] Importación desde formato de texto
- [ ] Soporte para thin walls y thick walls
- [ ] Vista previa 3D del mapa
- [ ] Deshacer/Rehacer
- [ ] Copiar/Pegar regiones

## Solución de Problemas

### Error: "No se pudo abrir el archivo .fpg"

Asegúrate de que el archivo FPG existe y tiene permisos de lectura.

### Error: "Formato .fpg inválido"

El archivo debe ser un FPG de BennuGD2 con formato F32.

### El editor no compila

Verifica que tienes instaladas todas las dependencias de Qt5 y zlib.

## Licencia

Este editor es parte del proyecto BennuGD2 y se distribuye bajo la misma licencia.
