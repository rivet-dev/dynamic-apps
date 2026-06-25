import "mod_say"
import "mod_rand"
import "mod_screen"
import "mod_video"
import "mod_map"
import "mod_key"

// Bouncing box example
process main()
private
  int vx=5, vy=5;

begin
  set_mode(640, 480, 16, MODE_WINDOW);
  graph = map_new(20, 20, 16);
  map_clear(0, graph, rgb(255, 0, 0));
  x = 320; y = 240;
  while(! key(_esc))
    if(x > 630 || x < 10)
      vx = -vx;
    end;
    if(y > 470 || y < 10)
      vy = -vy;
    end;
    x += vx; y += vy; // Move the box
    frame;
  end;
end;
