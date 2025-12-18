// create_textures.prg
// Programa para crear un FPG de texturas de prueba para el raycaster
import "libmod_gfx";
import "libmod_misc";

PROCESS main()
PRIVATE
    int fpg_id;
    int tex1, tex2, tex3;
BEGIN
    set_mode(640, 480, 32);
    
    say("Creando texturas de prueba...");
    
    // Crear FPG
    fpg_id = fpg_new();
    
    // Textura 1: Ladrillos rojos (64x64)
    tex1 = new_map(64, 64, 32);
    map_clear(0, tex1, rgb(180, 50, 50));
    
    // Dibujar líneas de ladrillos
    for(y=0; y<64; y+=16)
        drawing_map(0, tex1);
        drawing_color(rgb(100, 30, 30));
        draw_line(0, y, 63, y);
    end
    for(x=0; x<64; x+=16)
        drawing_map(0, tex1);
        drawing_color(rgb(100, 30, 30));
        draw_line(x, 0, x, 63);
    end
    
    fpg_add(fpg_id, 1, tex1);
    say("Textura 1 creada (ladrillos rojos)");
    
    // Textura 2: Piedra gris (64x64)
    tex2 = new_map(64, 64, 32);
    map_clear(0, tex2, rgb(120, 120, 120));
    
    // Agregar ruido/textura
    for(y=0; y<64; y+=4)
        for(x=0; x<64; x+=4)
            int r = rand(80, 160);
            drawing_map(0, tex2);
            drawing_color(rgb(r, r, r));
            draw_box(x, y, x+3, y+3);
        end
    end
    
    fpg_add(fpg_id, 2, tex2);
    say("Textura 2 creada (piedra gris)");
    
    // Textura 3: Madera (64x64)
    tex3 = new_map(64, 64, 32);
    map_clear(0, tex3, rgb(139, 90, 43));
    
    // Vetas de madera
    for(y=0; y<64; y+=8)
        drawing_map(0, tex3);
        drawing_color(rgb(100, 60, 30));
        draw_line(0, y, 63, y);
        draw_line(0, y+1, 63, y+1);
    end
    
    fpg_add(fpg_id, 3, tex3);
    say("Textura 3 creada (madera)");
    
    // Guardar FPG
    if (fpg_save(fpg_id, "textures.fpg") == 0)
        say("FPG guardado correctamente en textures.fpg");
    else
        say("ERROR al guardar el FPG");
    end
    
    say("Presiona ESC para salir");
    
    LOOP
        if (key(_esc)) exit(); end
        FRAME;
    END
END
