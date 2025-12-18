// test_raycaster.prg
// Programa de prueba para el módulo Raycaster
import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_ray";

GLOBAL
    int render;
    int screen_w = 640;
    int screen_h = 480;
END

PROCESS main()
PRIVATE
    int fpg_result;
    int fpg_id = 0;  // El FPG se carga en el file ID 0 por defecto
BEGIN
    set_mode(screen_w, screen_h);
    set_fps(0, 0);
    window_set_title("Raycaster Engine Test");
    
    // Cargar texturas desde FPG
    // fpg_load retorna 0 si tiene éxito, -1 si falla
    // El FPG se carga en el file ID 0 por defecto
    fpg_result = fpg_load("textures.fpg");
    if (fpg_result == -1)
        say("ADVERTENCIA: No se pudo cargar textures.fpg");
        say("El raycaster funcionará sin texturas");
        fpg_id = -1;  // Sin texturas
    else
        say("Texturas cargadas correctamente en file 0");
    end
    
    // Intentar cargar mapa existente
    if (ray_load_map("test_map.raymap", fpg_id) == 0)
        say("No se encontró mapa guardado, creando nuevo...");
        
        // Crear mapa 20x20 con 1 nivel, tiles de 64x64, con FPG de texturas
        if (ray_create_map(20, 20, 1, 64, fpg_id) == 0)
            say("ERROR: No se pudo crear el mapa");
            exit();
        end
        
        say("Creando mapa de prueba...");
        
        // Crear paredes alrededor del mapa
        for(x=0; x<20; x++)
            ray_set_cell(x, 0, 0, 1);   // Pared norte
            ray_set_cell(x, 19, 0, 1);  // Pared sur
        end
        for(y=0; y<20; y++)
            ray_set_cell(0, y, 0, 1);   // Pared oeste
            ray_set_cell(19, y, 0, 1);  // Pared este
        end
        
        // Crear algunas paredes internas
        for(x=8; x<12; x++)
            ray_set_cell(x, 8, 0, 2);
            ray_set_cell(x, 12, 0, 2);
        end
        for(y=8; y<12; y++)
            ray_set_cell(8, y, 0, 2);
            ray_set_cell(12, y, 0, 2);
        end
        
        // Pasillo horizontal
        for(x=5; x<15; x++)
            ray_set_cell(x, 15, 0, 3);
        end
        
        // Pasillo vertical
        for(y=5; y<15; y++)
            ray_set_cell(5, y, 0, 3);
        end
        
        say("Mapa creado exitosamente");
        
        // Configurar cámara inicial
        ray_set_camera(640.0, 640.0, 32.0, 0.0, 60.0);
    else
        say("Mapa cargado desde archivo");
    end
    
    // Iniciar proceso de renderizado
    raycaster_display();
    
    LOOP
        // Controles de movimiento
        if (key(_w)) ray_move_forward(5.0); end
        if (key(_s)) ray_move_backward(5.0); end
        
        // Controles de rotación
        if (key(_left)) ray_rotate(-0.05); end
        if (key(_right)) ray_rotate(0.05); end
        
        // Guardar mapa
        if (key(_f5))
            ray_save_map("test_map.raymap");
            say("Mapa guardado");
        end
        
        // Información en pantalla
        write(0, 10, 10, 0, "WS: Adelante/Atrás | Flechas: Rotar");
        write(0, 10, 30, 0, "F5: Guardar | ESC: Salir");
        write(0, 10, 50, 0, "FPS: " + frame_info.fps);
        
        if (key(_esc)) 
            ray_unload_map();  // Liberar memoria del mapa
            exit(); 
        end
        
        FRAME;
        WRITE_DELETE(all_text);
    END
END

PROCESS raycaster_display()
BEGIN
    LOOP
        // Renderizar el mapa Raycaster
        // Retorna el código del GRAPH para mostrarlo
        render = ray_render(screen_w, screen_h);
        
        if (render)
            graph = render;
            x = screen_w / 2;
            y = screen_h / 2;
            // size = 100 significa 100% (tamaño original)
            size = 100;
        else
            say("ERROR: No se pudo renderizar");
        end
        
        FRAME;
    END
END
