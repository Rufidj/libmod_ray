import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_ray";

Global
    int fpg_textures;
    int render_graph;
    float cam_x = 160.0;
    float cam_y = 160.0;
    float cam_z = 0.0;
    float cam_angle = 0.0;
    float cam_pitch = 0.0;
    int fps_counter;
    int fps_display;
End

Process main()
Private
    int i;
    int sector_a, sector_b;
End
Begin
    set_mode(640, 480, 32);
    set_fps(60, 0);
    
    // Crear FPG de texturas
    fpg_textures = fpg_new();
    
    // Textura 1: Pared roja
    i = new_map(64, 64, 32);
    map_clear(0, i, rgb(200, 50, 50));
    fpg_add(fpg_textures, 1, i);
    unload_map(0, i);
    
    // Textura 2: Suelo gris
    i = new_map(64, 64, 32);
    map_clear(0, i, rgb(100, 100, 100));
    fpg_add(fpg_textures, 2, i);
    unload_map(0, i);
    
    // Textura 3: Techo azul
    i = new_map(64, 64, 32);
    map_clear(0, i, rgb(50, 50, 150));
    fpg_add(fpg_textures, 3, i);
    unload_map(0, i);
    
    // Textura 4: Pared verde (habitación 2)
    i = new_map(64, 64, 32);
    map_clear(0, i, rgb(50, 200, 50));
    fpg_add(fpg_textures, 4, i);
    unload_map(0, i);
    
    // Textura 5: Suelo marrón (habitación 2)
    i = new_map(64, 64, 32);
    map_clear(0, i, rgb(150, 100, 50));
    fpg_add(fpg_textures, 5, i);
    unload_map(0, i);
    
    // Crear mapa: 10x10 tiles, 1 nivel, tile_size=64
    RAY_CREATE_MAP(10, 10, 1, 64, fpg_textures);
    
    // HABITACIÓN A (izquierda): 5x5 tiles (0-4, 0-4)
    // Paredes exteriores
    for(i=0; i<5; i++)
        RAY_SET_CELL(i, 0, 0, 1);  // Pared norte
        RAY_SET_CELL(i, 4, 0, 1);  // Pared sur
        RAY_SET_CELL(0, i, 0, 1);  // Pared oeste
    end
    // Pared este (con apertura para portal)
    RAY_SET_CELL(4, 0, 0, 1);
    RAY_SET_CELL(4, 1, 0, 1);
    // Dejar 4,2 vacío para portal
    RAY_SET_CELL(4, 3, 0, 1);
    RAY_SET_CELL(4, 4, 0, 1);
    
    // HABITACIÓN B (derecha): 5x5 tiles (5-9, 0-4)
    // Pared oeste (con apertura para portal)
    RAY_SET_CELL(5, 0, 0, 4);
    RAY_SET_CELL(5, 1, 0, 4);
    // Dejar 5,2 vacío para portal
    RAY_SET_CELL(5, 3, 0, 4);
    RAY_SET_CELL(5, 4, 0, 4);
    // Paredes exteriores
    for(i=5; i<10; i++)
        RAY_SET_CELL(i, 0, 0, 4);  // Pared norte
        RAY_SET_CELL(i, 4, 0, 4);  // Pared sur
        RAY_SET_CELL(9, i-5, 0, 4);  // Pared este
    end
    
    // Crear sectores
    sector_a = RAY_ADD_SECTOR(0, 0, 4, 4);  // Habitación A
    RAY_SET_SECTOR_FLOOR(sector_a, 0.0, 2);
    RAY_SET_SECTOR_CEILING(sector_a, 64.0, 3, 1);
    
    sector_b = RAY_ADD_SECTOR(5, 0, 9, 4);  // Habitación B
    RAY_SET_SECTOR_FLOOR(sector_b, 0.0, 5);
    RAY_SET_SECTOR_CEILING(sector_b, 64.0, 3, 1);
    
    say("Mapa creado:");
    say("- Habitación A (roja): Sector " + sector_a);
    say("- Habitación B (verde): Sector " + sector_b);
    say("- Portal automático entre ellas");
    say("");
    say("Controles:");
    say("- Flechas: Mover");
    say("- A/D: Rotar");
    say("- ESC: Salir");
    
    // Crear buffer de renderizado
    render_graph = new_map(640, 480, 32);
    
    // Iniciar cámara en habitación A
    cam_x = 160.0;  // Centro de habitación A
    cam_y = 160.0;
    cam_angle = 0.0;
    
    fps_counter = 0;
    fps_display = 0;
    
    // Loop principal
    Loop
        // Controles
        if(key(_left))
            cam_x -= cos(cam_angle) * 3.0;
            cam_y -= sin(cam_angle) * 3.0;
        end
        
        if(key(_right))
            cam_x += cos(cam_angle) * 3.0;
            cam_y += sin(cam_angle) * 3.0;
        end
        
        if(key(_up))
            cam_x += sin(cam_angle) * 3.0;
            cam_y -= cos(cam_angle) * 3.0;
        end
        
        if(key(_down))
            cam_x -= sin(cam_angle) * 3.0;
            cam_y += cos(cam_angle) * 3.0;
        end
        
        if(key(_a))
            cam_angle -= 0.05;
        end
        
        if(key(_d))
            cam_angle += 0.05;
        end
        
        if(key(_esc))
            break;
        end
        
        // Renderizar
        RAY_RENDER(render_graph, fpg_textures);
        
        // Mostrar en pantalla
        put(0, render_graph, 320, 240);
        
        // FPS
        fps_counter++;
        if(timer[0] > 100)
            fps_display = fps_counter * 10;
            fps_counter = 0;
            timer[0] = 0;
        end
        
        write(0, 10, 10, 0, "FPS: " + fps_display);
        write(0, 10, 25, 0, "Pos: " + cam_x + ", " + cam_y);
        write(0, 10, 40, 0, "Angle: " + (cam_angle * 180.0 / 3.14159));
        
        frame;
    End
    
    // Limpiar
    unload_map(0, render_graph);
    fpg_unload(fpg_textures);
    RAY_DESTROY_MAP();
    
    let_me_alone();
End
