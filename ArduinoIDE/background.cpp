
#include "background.h"

static bool scene_ready = false;
static lv_obj_t * top_bg = nullptr;
static lv_obj_t * bottom_bg = nullptr;
static lv_obj_t* celestial_canvas= nullptr;

static unsigned long lastSceneUpdate = 0;

// -------------------------
// SCENE RENDERING
// -------------------------

static inline lv_color_t interpolate_color(lv_color_t c1, lv_color_t c2, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  uint8_t mix = (uint8_t)(t * 255.0f);
  return lv_color_mix(c2, c1, mix);
}

void update_background(I2C_BM8563 rtc){
  if(scene_ready){
    unsigned long now = millis();
    if (now - lastSceneUpdate >= SCENE_UPDATE_INTERVAL_MS) {
        lastSceneUpdate = now;
        I2C_BM8563_TimeTypeDef ts;
        rtc.getTime(&ts);
        int current_hour   = ts.hours;
        int current_minute = ts.minutes;

        int total_minutes_today = current_hour * 60 + current_minute;
        bool is_daytime = (current_hour >= 6 && current_hour < 19);

        lv_color_t sky_color_top   = is_daytime ? lv_color_hex(0x4682B4) : lv_color_hex(0x000000); 
        lv_color_t sky_color_bottom = is_daytime ? lv_color_hex(0x87CEEB) : lv_color_hex(0x2F4F4F); 
        lv_obj_set_style_bg_color(top_bg, sky_color_top, 0);
        lv_obj_set_style_bg_grad_color(top_bg, sky_color_bottom, 0);
        lv_obj_set_style_bg_grad_dir(top_bg, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(top_bg, LV_OPA_COVER, 0);

        lv_color_t ground_color_top   = is_daytime ? lv_color_hex(0x3CB371) : lv_color_hex(0x2E8B57); 
        lv_color_t ground_color_bottom = is_daytime ? lv_color_hex(0x98FB98) : lv_color_hex(0x006400); 
        lv_obj_set_style_bg_color(bottom_bg, ground_color_top, 0);
        lv_obj_set_style_bg_grad_color(bottom_bg, ground_color_bottom, 0);
        lv_obj_set_style_bg_grad_dir(bottom_bg, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(bottom_bg, LV_OPA_COVER, 0);

        float time_progress_celestial; 
        if (is_daytime) { 
          time_progress_celestial = constrain((float)(total_minutes_today - (6 * 60)) / (13 * 60.0f), 0.0f, 1.0f);
        } else { 
          int minutes_into_night = (total_minutes_today >= (19 * 60)) ? (total_minutes_today - (19 * 60)) : (total_minutes_today + (24*60) - (19*60));
          time_progress_celestial = constrain((float)minutes_into_night / (11 * 60.0f), 0.0f, 1.0f);
        }
        
        float celestial_angle_rad = PI * time_progress_celestial; 

        const int screen_width = lv_obj_get_width(lv_scr_act());
        const int sky_height = lv_obj_get_height(top_bg); // make sure background is created
        //Serial.println(sky_height);
        
        const int orbit_center_x = screen_width / 2;
        const int orbit_radius_x = screen_width / 2;
        const int orbit_radius_y = sky_height / 2; 
        const int horizon_y_offset = sky_height; 

        int celestial_pos_x = orbit_center_x + cosf(celestial_angle_rad) * orbit_radius_x;
        int celestial_pos_y = horizon_y_offset - sinf(celestial_angle_rad) * orbit_radius_y;

        lv_color_t body_center_color = is_daytime ? lv_color_hex(0xFFFF00) : lv_color_hex(0xE0E0E0); 
        lv_color_t body_edge_color   = is_daytime ? lv_color_hex(0xFFCC00) : lv_color_hex(0xB0B0B0);

        lv_canvas_fill_bg(celestial_canvas, lv_color_black(), LV_OPA_TRANSP); 
        int body_radius = CELESTIAL_SIZE / 2; 
        
        for (int y_px = 0; y_px < CELESTIAL_SIZE; y_px++) {
          for (int x_px = 0; x_px < CELESTIAL_SIZE; x_px++) {
            int dx = x_px - CELESTIAL_SIZE / 2;
            int dy = y_px - CELESTIAL_SIZE / 2;
            float dist_from_center = sqrtf(dx*dx + dy*dy);
            if (dist_from_center <= body_radius) {
              float normalized_dist = dist_from_center / body_radius; 
              lv_color_t px_color = interpolate_color(body_center_color, body_edge_color, powf(normalized_dist, 1.5f));
              lv_canvas_set_px_color(celestial_canvas, x_px, y_px, px_color);
              lv_opa_t px_opa = (lv_opa_t)(LV_OPA_COVER * (1.0f - normalized_dist)); 
              lv_canvas_set_px_opa(celestial_canvas, x_px, y_px, px_opa);
            } else {
              lv_canvas_set_px_opa(celestial_canvas, x_px, y_px, LV_OPA_TRANSP); 
            }
          }
        }

        lv_obj_set_pos(celestial_canvas, celestial_pos_x - CELESTIAL_SIZE/2, celestial_pos_y - CELESTIAL_SIZE/2);
        lv_obj_invalidate(celestial_canvas); 

        lv_obj_move_background(bottom_bg);        
        lv_obj_move_background(celestial_canvas); 
        lv_obj_move_background(top_bg);

        Serial.println("Background updated");
    }
  }else{
    Serial.println("Background not created");
  }
}

void create_scene(lv_obj_t* parent) {
  Serial.println("Scene was not created, running setup");
  if (!parent) parent = lv_scr_act(); 
  top_bg = lv_obj_create(parent);
  lv_obj_remove_style_all(top_bg); 
  lv_obj_set_size(top_bg, lv_obj_get_width(parent), lv_obj_get_height(parent) / 2);
  lv_obj_align(top_bg, LV_ALIGN_TOP_LEFT, 0, 0);

  celestial_canvas = lv_canvas_create(parent);
  lv_canvas_set_buffer(celestial_canvas, celestial_buf,
                        CELESTIAL_SIZE, CELESTIAL_SIZE,
                        LV_IMG_CF_TRUE_COLOR_ALPHA);
  lv_obj_remove_style_all(celestial_canvas);
  lv_obj_set_style_bg_opa(celestial_canvas, LV_OPA_TRANSP, 0); 
  lv_obj_set_style_border_width(celestial_canvas, 0, 0);

  bottom_bg = lv_obj_create(parent);
  lv_obj_remove_style_all(bottom_bg); 
  lv_obj_set_size(bottom_bg, lv_obj_get_width(parent), lv_obj_get_height(parent) / 2);
  lv_obj_align(bottom_bg, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  // needed to create bg so we can get sky height in update_background
  lv_task_handler();
  // negates the timer so it also runs once off-the-bat in the main loop
  lastSceneUpdate = millis() - SCENE_UPDATE_INTERVAL_MS - 100;
  scene_ready = true;     
  Serial.println("Scene created");
   
}


