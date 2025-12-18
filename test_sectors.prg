// Programa de prueba para el sistema de sectores
import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_ray";

GLOBAL
    int screen_w = 800;
    int screen_h = 600;
END

PROCESS main()
PRIVATE
    int fpg_result;
    int fpg_id = 0;
    int sector1, sector2, sector3;
BEGIN
    set_mode(screen_w, screen_h);
    set_fps(120, 0);
    window_set_title("Raycaster - Test de Sectores");
    
    // Cargar texturas
    fpg_result = fpg_load("textures.fpg");
    if (fpg_result == -1)
        say("ADVERTENCIA: No se pudo cargar textures.fpg");
        fpg_id = -1;
    else
        say("Texturas cargadas correctamente");
    end
    
    // Crear mapa 20x20
    if (ray_create_map(20, 20, 1, 64, fpg_id) == 0)
        say("ERROR: No se pudo crear el mapa");
        exit();
    end
    
    say("Creando mapa con sectores...");
    
    // Crear paredes exteriores
    for(x=0; x<20; x++)
        ray_set_cell(x, 0, 0, 1);
        ray_set_cell(x, 19, 0, 1);
    end
    for(y=0; y<20; y++)
        ray_set_cell(0, y, 0, 1);
        ray_set_cell(19, y, 0, 1);
    end
    
    // Crear paredes internas
    for(x=8; x<12; x++)
        ray_set_cell(x, 8, 0, 2);
        ray_set_cell(x, 12, 0, 2);
    end
    for(y=8; y<12; y++)
        ray_set_cell(8, y, 0, 2);
        ray_set_cell(12, y, 0, 2);
    end
    
    // SECTORES:
    
    // Sector 1: Habitación principal (con techo)
    sector1 = ray_add_sector(1, 1, 18, 18);
    ray_set_sector_floor(sector1, 0.0, 2);      // Suelo: textura 2 (piedra)
    ray_set_sector_ceiling(sector1, 64.0, 3, 1); // Techo: textura 3 (madera), con techo
    say("Sector 1 (habitación principal): " + sector1);
    
    // Sector 2: Habitación central (sin techo - cielo abierto)
    sector2 = ray_add_sector(9, 9, 11, 11);
    ray_set_sector_floor(sector2, 0.0, 1);      // Suelo: textura 1 (ladrillos)
    ray_set_sector_ceiling(sector2, 64.0, 0, 0); // Sin techo (cielo abierto)
    say("Sector 2 (patio central): " + sector2);
    
    // Sector 3: Pasillo elevado
    sector3 = ray_add_sector(5, 5, 7, 15);
    ray_set_sector_floor(sector3, 10.0, 3);     // Suelo elevado: textura 3
    ray_set_sector_ceiling(sector3, 74.0, 2, 1); // Techo más alto: textura 2
    say("Sector 3 (pasillo elevado): " + sector3);
    
    say("Sectores creados exitosamente");
    
    // Configurar cámara
    ray_set_camera(640.0, 640.0, 9.0, 0.0, 60.0);
    
    // Iniciar renderizado
    raycaster_display();
    
    LOOP
        // Controles
        if (key(_w)) ray_move_forward(5.0); end
        if (key(_s)) ray_move_backward(5.0); end
        if (key(_left)) ray_rotate(-0.05); end
        if (key(_right)) ray_rotate(0.05); end
        
        // Info y DEBUG
        write(0, 10, 10, 0, "WS: Adelante/Atrás | Flechas: Rotar | ESC: Salir");
        write(0, 10, 30, 0, "FPS: " + frame_info.fps);
        write(0, 10, 50, 0, "Sectores: 3 (habitación, patio, pasillo)");
        write(0, 10, 150, 0, "Presiona W o A y observa las coordenadas");
        
        // DEBUG: Mostrar coordenadas de cámara
        write(0, 10, 70, 0, "Cam X: " + ray_get_camera_x());
        write(0, 10, 90, 0, "Cam Y: " + ray_get_camera_y());
        write(0, 10, 110, 0, "Cam Z: " + ray_get_camera_z());
        write(0, 10, 130, 0, "Cam Angle: " + ray_get_camera_angle());
        
        if (key(_esc))
            ray_unload_map();
            exit();
        end
        
        FRAME;
        WRITE_DELETE(all_text);
    END
END

PROCESS raycaster_display()
BEGIN
    LOOP
        graph = ray_render(screen_w, screen_h);
        x = screen_w / 2;
        y = screen_h / 2;
        FRAME;
    END
END
