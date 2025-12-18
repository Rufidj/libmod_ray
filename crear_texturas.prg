// crear_texturas.prg
// Programa para crear texturas de ejemplo y el FPG
import "libmod_gfx";
import "libmod_misc";

process main()
private
    int fpg_id;
    int graph_id;
begin
    set_mode(640, 480, 32);
    
    say("Creando FPG de texturas...");
    
    fpg_id = fpg_new();
    
    // ===== PAREDES (IDs 1-4) =====
    
    // Pared 1 - Ladrillos rojos
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgb(150, 50, 50));
    // Dibujar patrón de ladrillos
    drawing_map(0, graph_id);
    drawing_color(rgb(100, 30, 30));
    for(int y=0; y<128; y+=16)
        for(int x=0; x<128; x+=32)
            draw_rect(x, y, x+30, y+14);
        end
        for(int x=16; x<128; x+=32)
            draw_rect(x, y+16, x+30, y+30);
        end
    end
    fpg_add(fpg_id, 1, graph_id);
    
    // Pared 2 - Piedra gris
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgb(100, 100, 100));
    drawing_map(0, graph_id);
    drawing_color(rgb(70, 70, 70));
    for(int y=0; y<128; y+=32)
        for(int x=0; x<128; x+=32)
            draw_rect(x+2, y+2, x+30, y+30);
        end
    end
    fpg_add(fpg_id, 2, graph_id);
    
    // Pared 3 - Madera
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgb(139, 90, 43));
    drawing_map(0, graph_id);
    drawing_color(rgb(101, 67, 33));
    for(int x=0; x<128; x+=8)
        draw_line(x, 0, x, 128);
    end
    fpg_add(fpg_id, 3, graph_id);
    
    // Pared 4 - Metal
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgb(180, 180, 200));
    drawing_map(0, graph_id);
    drawing_color(rgb(140, 140, 160));
    for(int y=0; y<128; y+=64)
        draw_line(0, y, 128, y);
    end
    for(int x=0; x<128; x+=64)
        draw_line(x, 0, x, 128);
    end
    fpg_add(fpg_id, 4, graph_id);
    
    // ===== SUELO (ID 5) =====
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgb(80, 80, 80));
    drawing_map(0, graph_id);
    drawing_color(rgb(60, 60, 60));
    for(int y=0; y<128; y+=64)
        for(int x=0; x<128; x+=64)
            if((x+y) % 128 == 0)
                draw_box(x, y, x+64, y+64);
            end
        end
    end
    fpg_add(fpg_id, 5, graph_id);
    
    // ===== TECHO (ID 6) =====
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgb(50, 50, 70));
    drawing_map(0, graph_id);
    drawing_color(rgb(40, 40, 60));
    for(int y=0; y<128; y+=32)
        for(int x=0; x<128; x+=32)
            draw_rect(x+2, y+2, x+30, y+30);
        end
    end
    fpg_add(fpg_id, 6, graph_id);
    
    // ===== SPRITES (IDs 10-14) =====
    
    // Sprite 10 - Columna
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgba(0, 0, 0, 0)); // Transparente
    drawing_map(0, graph_id);
    drawing_color(rgb(200, 200, 200));
    draw_box(48, 0, 80, 128);
    drawing_color(rgb(150, 150, 150));
    draw_box(40, 0, 48, 20);
    draw_box(80, 0, 88, 20);
    draw_box(40, 108, 48, 128);
    draw_box(80, 108, 88, 128);
    fpg_add(fpg_id, 10, graph_id);
    
    // Sprite 11 - Enemigo rojo
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgba(0, 0, 0, 0));
    drawing_map(0, graph_id);
    drawing_color(rgb(200, 50, 50));
    draw_fcircle(64, 64, 40);
    drawing_color(rgb(255, 255, 255));
    draw_fcircle(50, 50, 8);
    draw_fcircle(78, 50, 8);
    drawing_color(rgb(0, 0, 0));
    draw_fcircle(50, 50, 4);
    draw_fcircle(78, 50, 4);
    fpg_add(fpg_id, 11, graph_id);
    
    // Sprite 12 - Enemigo azul
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgba(0, 0, 0, 0));
    drawing_map(0, graph_id);
    drawing_color(rgb(50, 50, 200));
    draw_fcircle(64, 64, 40);
    drawing_color(rgb(255, 255, 255));
    draw_fcircle(50, 50, 8);
    draw_fcircle(78, 50, 8);
    drawing_color(rgb(0, 0, 0));
    draw_fcircle(50, 50, 4);
    draw_fcircle(78, 50, 4);
    fpg_add(fpg_id, 12, graph_id);
    
    // Sprite 13 - Item (estrella dorada)
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgba(0, 0, 0, 0));
    drawing_map(0, graph_id);
    drawing_color(rgb(255, 215, 0));
    // Dibujar estrella simple
    draw_fcircle(64, 64, 30);
    drawing_color(rgb(255, 255, 0));
    draw_fcircle(64, 64, 20);
    fpg_add(fpg_id, 13, graph_id);
    
    // Sprite 14 - Item (diamante)
    graph_id = new_map(128, 128, 32);
    map_clear(0, graph_id, rgba(0, 0, 0, 0));
    drawing_map(0, graph_id);
    drawing_color(rgb(100, 200, 255));
    draw_fcircle(64, 64, 25);
    drawing_color(rgb(200, 230, 255));
    draw_fcircle(64, 64, 15);
    fpg_add(fpg_id, 14, graph_id);
    
    // Guardar FPG
    fpg_save(fpg_id, "textures.fpg");
    
    say("FPG creado exitosamente: textures.fpg");
    say("Texturas incluidas:");
    say("  IDs 1-4: Paredes (ladrillo, piedra, madera, metal)");
    say("  ID 5: Suelo (baldosas)");
    say("  ID 6: Techo (paneles)");
    say("  IDs 10-14: Sprites (columna, enemigos, items)");
    say("");
    say("Ahora puedes ejecutar test_ray.prg");
end
