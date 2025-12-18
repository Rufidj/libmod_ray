# libmod_ray - Raycasting Module for BennuGD2

A high-performance 3D raycasting engine module for BennuGD2, featuring multi-level maps, textured walls, floors, ceilings, and sprite rendering.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![BennuGD2](https://img.shields.io/badge/BennuGD2-compatible-green.svg)

## Features

- ✨ **Multi-level 3D rendering** - Support for up to 3 vertical levels
- 🧱 **Textured walls** - Full texture mapping with perspective correction
- 🎨 **Floor and ceiling rendering** - Perspective-correct textured surfaces
- 👾 **Sprite system** - Billboard sprites with Z-buffering
- 🌫️ **Fog effects** - Distance-based fog for atmosphere
- 🚪 **Interactive doors** - Toggle-able door system
- 🎮 **Full camera control** - Movement, rotation, pitch, and jumping
- 📦 **Binary map format** - Efficient `.raymap` file format
- 🛠️ **Map builder tool** - Convert text files to binary maps

## Quick Start

### 1. Compile the Module

```bash
cd modules/libmod_ray
mkdir build && cd build
cmake ..
make
```

### 2. Create a Map

Use the map builder tool to create maps from text files:

```bash
cd tools
./map_builder example/config.txt ../test.raymap
```

### 3. Run the Test Program

```bash
cd ..
bgdi test_ray.dcb
```

## Usage Example

```bennugd
import "libmod_ray";
import "libmod_gfx";

process main()
private
    int fpg_textures;
begin
    set_mode(800, 600, 32);
    
    // Load textures
    fpg_textures = fpg_load("textures.fpg");
    
    // Initialize raycasting engine
    RAY_INIT(800, 600, 90, 2);
    
    // Load map
    RAY_LOAD_MAP("test.raymap", fpg_textures);
    
    // Set camera position
    RAY_SET_CAMERA(512.0, 512.0, 0.0, 0.0, 0.0);
    
    // Main loop
    loop
        // Handle input
        if (key(_w)) RAY_MOVE_FORWARD(4.0); end
        if (key(_s)) RAY_MOVE_BACKWARD(4.0); end
        if (key(_a)) RAY_STRAFE_LEFT(4.0); end
        if (key(_d)) RAY_STRAFE_RIGHT(4.0); end
        if (key(_left)) RAY_ROTATE(-0.05); end
        if (key(_right)) RAY_ROTATE(0.05); end
        if (key(_space)) RAY_JUMP(); end
        
        // Render frame
        graph = RAY_RENDER();
        put(0, graph, 400, 300);
        
        frame;
    end
    
    // Cleanup
    RAY_FREE_MAP();
    RAY_SHUTDOWN();
end
```

## API Reference

### Initialization

- `RAY_INIT(width, height, fov, strip_width)` - Initialize the engine
- `RAY_SHUTDOWN()` - Shutdown and cleanup

### Map Management

- `RAY_LOAD_MAP(filename, fpg_id)` - Load a `.raymap` file
- `RAY_FREE_MAP()` - Free current map

### Camera Control

- `RAY_SET_CAMERA(x, y, z, rot, pitch)` - Set camera position
- `RAY_GET_CAMERA_X()` - Get camera X position
- `RAY_GET_CAMERA_Y()` - Get camera Y position
- `RAY_GET_CAMERA_Z()` - Get camera Z position
- `RAY_GET_CAMERA_ROT()` - Get camera rotation
- `RAY_GET_CAMERA_PITCH()` - Get camera pitch

### Movement

- `RAY_MOVE_FORWARD(speed)` - Move forward
- `RAY_MOVE_BACKWARD(speed)` - Move backward
- `RAY_STRAFE_LEFT(speed)` - Strafe left
- `RAY_STRAFE_RIGHT(speed)` - Strafe right
- `RAY_ROTATE(angle)` - Rotate camera
- `RAY_LOOK_UP_DOWN(angle)` - Look up/down
- `RAY_JUMP()` - Jump

### Rendering

- `RAY_RENDER()` - Render frame, returns graph ID

### Configuration

- `RAY_SET_FOG(enabled)` - Enable/disable fog
- `RAY_SET_DRAW_MINIMAP(enabled)` - Enable/disable minimap
- `RAY_SET_DRAW_WEAPON(enabled)` - Enable/disable weapon

### Interaction

- `RAY_TOGGLE_DOOR(x, y)` - Toggle door at grid position
- `RAY_ADD_SPRITE(x, y, z, tex_id, w, h)` - Add dynamic sprite
- `RAY_REMOVE_SPRITE(id)` - Remove sprite

## Map Format

Maps are created from text files and compiled to binary `.raymap` format.

### Configuration File (`config.txt`)

```
grid0=nivel0.txt
grid1=nivel1.txt
grid2=nivel2.txt
floor=floor.txt
ceiling=ceiling.txt
sprites=sprites.txt
```

### Grid Files

16x16 CSV grid where each number represents a texture ID:

```
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1
1,0,0,0,0,2,2,2,2,0,0,0,0,0,0,1
...
```

- `0` = Empty space
- `1-999` = Wall texture ID from FPG
- `1000-1500` = Vertical doors
- `1501+` = Horizontal doors

### Sprites File

CSV format: `x, y, z, texture_id, width, height, level, rotation`

```
512.0, 512.0, 0.0, 10, 128, 128, 0, 0.0
768.0, 384.0, 0.0, 11, 128, 128, 0, 0.0
```

## Texture Requirements

Create an FPG file with 128x128 textures:

- **IDs 1-4**: Wall textures
- **ID 5**: Floor texture
- **ID 6**: Ceiling texture
- **IDs 10-14**: Sprite textures

Use the included `crear_texturas.prg` to generate example textures.

## Building Maps

The `map_builder` tool converts text files to binary format:

```bash
./map_builder config.txt output.raymap
```

See `tools/README.md` for detailed documentation.

## Controls (Test Program)

- **WASD** - Move
- **Arrow Keys** - Rotate camera / Look up-down
- **Space** - Jump
- **Enter** - Open/close doors
- **F** - Toggle fog
- **M** - Toggle minimap
- **ESC** - Exit

## Technical Details

### Architecture

- **Raycasting Engine**: DDA algorithm for wall detection
- **Rendering**: Back-to-front painter's algorithm for multi-level support
- **Texturing**: Perspective-correct texture mapping
- **Sprites**: Z-buffered billboard rendering
- **Physics**: Simple gravity and collision detection

### Performance

- Optimized for 800x600 resolution
- Configurable strip width (2-4 recommended)
- FOV: 60-90 degrees recommended
- Supports up to 1000 sprites, 1000 thin walls, 100 thick walls

### Limitations

- Maximum 3 vertical levels
- Grid-based collision (128 units per tile)
- No dynamic lighting (fog only)
- No texture filtering

## Project Structure

```
libmod_ray/
├── libmod_ray.c              # Main module code
├── libmod_ray.h              # Header definitions
├── libmod_ray_exports.h      # BennuGD2 exports
├── libmod_ray_map.c          # Map loading
├── libmod_ray_raycasting.c   # Raycasting algorithm
├── libmod_ray_render.c       # Rendering system
├── libmod_ray_shape.c        # Geometry utilities
├── test_ray.prg              # Test program
├── tools/
│   ├── map_builder.c         # Map conversion tool
│   ├── README.md             # Tool documentation
│   └── example/              # Example map files
└── CMakeLists.txt            # Build configuration
```

## Credits

Based on Andrew Lim's SDL2 Raycasting Engine:
https://github.com/andrew-lim/sdl2-raycast

Ported to C and integrated with BennuGD2 by Ruben.

## License

MIT License - See LICENSE file for details

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues.

## Roadmap

- [ ] Per-level floor and ceiling textures
- [ ] Skybox support
- [ ] Animated textures
- [ ] Dynamic lighting
- [ ] Texture filtering options
- [ ] Minimap implementation
- [ ] Weapon rendering
- [ ] Sound integration

## Support

For issues and questions, please open an issue on GitHub.
