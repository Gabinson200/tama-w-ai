// -------------------------
// INCLUDES & DEFINITIONS
// -------------------------
#include <Arduino.h>
#include <lvgl.h>
#include "lv_xiao_round_screen.h" // Specific display library
#include "touch.h"                // Touch input handling
#include "sprite_stack.h"         // Custom sprite stacking class
#include "anim.h"                 // Animation classes
#include <stdlib.h>               // For rand(), randomSeed()
#include <math.h>                 // For math functions like atan2, sqrt, cos, sin
#include "catch_game.h"           // Catching game logic
#include "I2C_BM8563.h"           // RTC library


// -------------------------
// SPRITE IMAGE DECLARATIONS
// -------------------------

// Black Cat sprite images
#include "black_cat_true_color_alpha.h" // Contains the actual image data
LV_IMG_DECLARE(black_cat000); LV_IMG_DECLARE(black_cat001); LV_IMG_DECLARE(black_cat002);
LV_IMG_DECLARE(black_cat003); LV_IMG_DECLARE(black_cat004); LV_IMG_DECLARE(black_cat005);
LV_IMG_DECLARE(black_cat006); LV_IMG_DECLARE(black_cat007); LV_IMG_DECLARE(black_cat008);
LV_IMG_DECLARE(black_cat009); LV_IMG_DECLARE(black_cat010); LV_IMG_DECLARE(black_cat011);
LV_IMG_DECLARE(black_cat012); LV_IMG_DECLARE(black_cat013); LV_IMG_DECLARE(black_cat014);
LV_IMG_DECLARE(black_cat015);

const lv_img_dsc_t *cat_images[] = {
  &black_cat000, &black_cat001, &black_cat002, &black_cat003, &black_cat004,
  &black_cat005, &black_cat006, &black_cat007, &black_cat008, &black_cat009,
  &black_cat010, &black_cat011, &black_cat012, &black_cat013, &black_cat014,
  &black_cat015
};

// Ball sprite images
#include "ball_sprites_true_color_alpha.h" // Contains the actual image data
LV_IMG_DECLARE(bb000); LV_IMG_DECLARE(bb001); LV_IMG_DECLARE(bb002);
LV_IMG_DECLARE(bb003); LV_IMG_DECLARE(bb004); LV_IMG_DECLARE(bb005);
LV_IMG_DECLARE(bb006); LV_IMG_DECLARE(bb007); LV_IMG_DECLARE(bb008);
LV_IMG_DECLARE(bb009); LV_IMG_DECLARE(bb010); LV_IMG_DECLARE(bb011);
LV_IMG_DECLARE(bb012); LV_IMG_DECLARE(bb013); LV_IMG_DECLARE(bb014);
LV_IMG_DECLARE(bb015);

const lv_img_dsc_t *ball_images[] = {
  &bb000, &bb001, &bb002, &bb003, &bb004,
  &bb005, &bb006, &bb007, &bb008, &bb009,
  &bb010, &bb011, &bb012, &bb013, &bb014,
  &bb015
};

#include "burger.h"
LV_IMG_DECLARE(burger_1); LV_IMG_DECLARE(burger_2); LV_IMG_DECLARE(burger_3);
LV_IMG_DECLARE(burger_4); LV_IMG_DECLARE(burger_5); LV_IMG_DECLARE(burger_6);
LV_IMG_DECLARE(burger_7); LV_IMG_DECLARE(burger_8);

const lv_img_dsc_t *burger_images[] = {
  &burger_1, &burger_2, &burger_3, &burger_4, &burger_5,
  &burger_6, &burger_7, &burger_8
};

// Frog sprite images
#include "frog.h" // Contains the actual image data
LV_IMG_DECLARE(frog_1); LV_IMG_DECLARE(frog_2);
LV_IMG_DECLARE(frog_3); LV_IMG_DECLARE(frog_4); LV_IMG_DECLARE(frog_5);
LV_IMG_DECLARE(frog_6); LV_IMG_DECLARE(frog_7);

const lv_img_dsc_t *frog_images[] = {
  &frog_1, &frog_2, &frog_3, &frog_4, &frog_5,
  &frog_6, &frog_7
};

#include "bed.h"
LV_IMG_DECLARE(bed_1); LV_IMG_DECLARE(bed_2); LV_IMG_DECLARE(bed_3);
LV_IMG_DECLARE(bed_4); LV_IMG_DECLARE(bed_5); LV_IMG_DECLARE(bed_6);

const lv_img_dsc_t *bed_images[] = {
  &bed_1, &bed_2, &bed_3, &bed_4, &bed_5, &bed_6
};

// -------------------------
// GLOBAL VARIABLES & OBJECTS
// -------------------------

// --- State Flags & Movement Variables ---
// For test_user_and_random_walk
bool hasUserDestination = false;                // True if there's an active, unreached user destination
Point currentUserDestination = {0,0};           // Stores the coordinates of the user's target
bool inPostUserTargetCooldown = false;          // True if sprite reached user target and is waiting
unsigned long postUserTargetCooldownStartTime = 0;// Timestamp for cooldown start
const unsigned long USER_DESTINATION_HOLD_DURATION = 2000; // Milliseconds to hold touch for destination
const unsigned long POST_USER_TARGET_COOLDOWN_DURATION = 8000; // Milliseconds to wait after reaching user target

// For general sprite state
int animIndex = 0;

// Calculate frame counts dynamically
int catFrameCount = sizeof(cat_images) / sizeof(cat_images[0]);
int frogFrameCount = sizeof(frog_images) / sizeof(frog_images[0]);
int burgerFrameCount = sizeof(burger_images) / sizeof(burger_images[0]);
int bedFrameCount = sizeof(bed_images) / sizeof(bed_images[0]);
int ballFrameCount = sizeof(ball_images) / sizeof(ball_images[0]);

// Create the player sprite stack using frog_images.
SpriteStack myStack(cat_images, catFrameCount, 0, 3.0, 1.0, 100.0f);
SpriteStack burgerStack(burger_images, burgerFrameCount, 0, 2.0, 1.0, 100.0f);
SpriteStack bedStack(bed_images, bedFrameCount, 0, 3.0, 1.0, 100.0f);

// Global variable holding the sprite stack's current position.
Point g_spritePosition = {120, 160}; // For myStack
Point burgerPosition = {80, 140};    // For burgerStack
Point bedPosition = {180, 140};      // For bedStack

// --- Catching game ---
bool inCatchingGame = false;

// --- Real-Time Clock (RTC) ---
I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire); // RTC object

lv_obj_t * mainScreen = NULL;



// -------------------------
// BACKGROUND OBJECTS
// -------------------------
static lv_obj_t * top_bg = nullptr;
static lv_obj_t * bottom_bg = nullptr;
static lv_obj_t* celestial_canvas= nullptr;
static bool scene_ready = false;

#define CELESTIAL_SIZE 40
static uint8_t celestial_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(CELESTIAL_SIZE, CELESTIAL_SIZE)];

// -------------------------
// UTILITY FUNCTIONS
// -------------------------
int random_int(int min, int max) {
  if (min > max) {
    int temp = min;
    min = max;
    max = temp;
  }
  if (min == max) return min; // Avoid modulo by zero if min == max
  return min + rand() % (max - min + 1);
}

Point random_point(int x_min, int x_max, int y_min, int y_max) {
  Point p;
  p.x = random_int(x_min, x_max);
  p.y = random_int(y_min, y_max);
  return p;
}

static inline lv_color_t interpolate_color(lv_color_t c1,
                                           lv_color_t c2,
                                           float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  uint8_t mix = (uint8_t)(t * 255.0f);
  return lv_color_mix(c2, c1, mix);
}

void render_scene(lv_obj_t* parent = nullptr) {
  if (!scene_ready) {
    if (!parent) parent = lv_scr_act();
    top_bg = lv_obj_create(parent);
    lv_obj_remove_style_all(top_bg);
    lv_obj_set_size(top_bg, lv_obj_get_width(parent), lv_obj_get_height(parent) * 120 / 240);
    lv_obj_align(top_bg, LV_ALIGN_TOP_LEFT, 0, 0);

    bottom_bg = lv_obj_create(parent);
    lv_obj_remove_style_all(bottom_bg);
    lv_obj_set_size(bottom_bg, lv_obj_get_width(parent), lv_obj_get_height(parent) * 120 / 240);
    lv_obj_align(bottom_bg, LV_ALIGN_TOP_LEFT, 0, lv_obj_get_height(parent) * 120 / 240);

    celestial_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(celestial_canvas, celestial_buf,
                         CELESTIAL_SIZE, CELESTIAL_SIZE,
                         LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_remove_style_all(celestial_canvas);
    lv_obj_set_style_bg_opa(celestial_canvas, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(celestial_canvas, 0, 0);
    scene_ready = true;
  }

  I2C_BM8563_TimeTypeDef ts;
  rtc.getTime(&ts);
  int hour   = ts.hours;
  int minute = ts.minutes;

  int tot_min = hour * 60 + minute;
  bool isDay = (hour >= 6 && hour < 19);

  lv_color_t sky_top   = isDay ? lv_color_hex(0x4682B4) : lv_color_hex(0x000000);
  lv_color_t sky_bot   = isDay ? lv_color_hex(0x87CEEB) : lv_color_hex(0x2F4F4F);
  lv_obj_set_style_bg_color(top_bg, sky_top, 0);
  lv_obj_set_style_bg_grad_color(top_bg, sky_bot, 0);
  lv_obj_set_style_bg_grad_dir(top_bg, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(top_bg, LV_OPA_COVER, 0);
  //lv_obj_move_background(top_bg);

  lv_color_t grd_top   = isDay ? lv_color_hex(0x3CB371) : lv_color_hex(0x2E8B57);
  lv_color_t grd_bot   = isDay ? lv_color_hex(0x98FB98) : lv_color_hex(0x006400);
  lv_obj_set_style_bg_color(bottom_bg, grd_top, 0);
  lv_obj_set_style_bg_grad_color(bottom_bg, grd_bot, 0);
  lv_obj_set_style_bg_grad_dir(bottom_bg, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(bottom_bg, LV_OPA_COVER, 0);
  //lv_obj_move_background(bottom_bg);

  float t_celestial; // Renamed to avoid conflict
  if (isDay) {
    t_celestial = constrain((tot_min - 360) / 720.0f, 0.0f, 1.0f); // 6 AM to 6 PM
    t_celestial = 1.0f - t_celestial; 
  } else {
    int adj_tot_min = (hour < 6) ? (tot_min + 24*60) : tot_min; 
    t_celestial = constrain((adj_tot_min - (18*60)) / 720.0f, 0.0f, 1.0f); // 6 PM to 6 AM
  }
  float angle = PI * (1.0f - t_celestial); 

  const int cx = lv_obj_get_width(lv_scr_act()) / 2; 
  const int cy = lv_obj_get_height(top_bg);       
  const int r_orbit = lv_obj_get_width(lv_scr_act()) / 2 - CELESTIAL_SIZE / 2 - 10; 

  int bx = cx - cosf(angle) * r_orbit; 
  int by = cy - sinf(angle) * r_orbit * 0.7f; 

  lv_color_t col_center = isDay ? lv_color_hex(0xFFFF00) : lv_color_hex(0xE0E0E0); 
  lv_color_t col_edge   = isDay ? lv_color_hex(0xFFCC00) : lv_color_hex(0xB0B0B0);

  lv_canvas_fill_bg(celestial_canvas, lv_color_black(), LV_OPA_TRANSP); 

  int radius = CELESTIAL_SIZE / 2 - 2; 
  if (!isDay) { 
      // float phase_t = fmod(t_celestial * 2.0f, 1.0f); 
      // radius = (CELESTIAL_SIZE / 2 - 2) * (0.5f + 0.5f * cosf(phase_t * 2.0f * PI));
  }

  for (int y_px = 0; y_px < CELESTIAL_SIZE; y_px++) {
    for (int x_px = 0; x_px < CELESTIAL_SIZE; x_px++) {
      int dx = x_px - CELESTIAL_SIZE / 2;
      int dy = y_px - CELESTIAL_SIZE / 2;
      float dist = sqrtf(dx*dx + dy*dy);
      if (dist <= radius) {
        float f = dist / radius; // f is 0 at center, 1 at edge
        lv_color_t c = interpolate_color(col_center, col_edge, powf(f, 1.5f));
        lv_canvas_set_px_color(celestial_canvas, x_px, y_px, c);
        // Opacity: LV_OPA_COVER at center (f=0), LV_OPA_TRANSP at edge (f=1)
        lv_opa_t current_opa = (lv_opa_t)(LV_OPA_COVER * (1.0f - f));
        lv_canvas_set_px_opa(celestial_canvas, x_px, y_px, current_opa);
      } else {
        lv_canvas_set_px_opa(celestial_canvas, x_px, y_px, LV_OPA_TRANSP); 
      }
    }
  }
  lv_obj_set_pos(celestial_canvas, bx - CELESTIAL_SIZE / 2, by - CELESTIAL_SIZE / 2);
  //lv_obj_move_foreground(celestial_canvas);
  lv_obj_invalidate(celestial_canvas);

  // 3. Ground in front of Celestial body
  // After top_bg (0) and celestial_canvas (1), move bottom_bg to index 2.
  // Sequence: move bottom_bg to back, then celestial_canvas to back, then top_bg to back.
  lv_obj_move_background(bottom_bg);        // bottom_bg @ 0, top_bg @ 1, celestial_canvas @ 2
  lv_obj_move_background(celestial_canvas); // celestial_canvas @ 0, bottom_bg @ 1, top_bg @ 2
  lv_obj_move_background(top_bg);           // top_bg @ 0, celestial_canvas @ 1, bottom_bg @ 2
                                            // This results in: Sky, Celestial, Ground. Correct.
}


// -------------------------
// MOVEMENT FUNCTIONS
// -------------------------
bool moveSpriteToTarget(SpriteStack &sprite_stack, Point &currentPos, const Point &target) {
    static bool moving_to_current_target = false; 
    static Point internal_target_memory = {-1, -1}; 
    static unsigned long lastUpdate = 0;
    const int delay_ms = 30; // Target update rate for movement

    // Check if the externally supplied 'target' has changed.
    // This is the core of the interruption logic.
    if (internal_target_memory.x != target.x || internal_target_memory.y != target.y) {
        moving_to_current_target = false; // Force re-initialization for the new target
        internal_target_memory = target;  // Remember this new target
        Serial.print("moveSpriteToTarget: New target acquired: X="); Serial.print(target.x); Serial.print(", Y="); Serial.println(target.y);
    }

    if (!moving_to_current_target) {
        // Initialize movement towards the 'internal_target_memory'
        float angle_rad = atan2((float)(internal_target_memory.y - currentPos.y),
                                (float)(internal_target_memory.x - currentPos.x));
        float roll_angle_deg = degrees(angle_rad) + 270; // Adjust +90 based on sprite's "front"
        
        sprite_stack.setRotation(0, 0, roll_angle_deg); // Simplified pitch for now
        moving_to_current_target = true;
        lastUpdate = millis(); // Initialize lastUpdate time
        Serial.print("moveSpriteToTarget: Initializing movement to X="); Serial.print(internal_target_memory.x); Serial.print(", Y="); Serial.println(internal_target_memory.y);
    }

    unsigned long now = millis();
    if (now - lastUpdate >= (unsigned long)delay_ms) {
        lastUpdate = now;
        float dx = internal_target_memory.x - currentPos.x;
        float dy = internal_target_memory.y - currentPos.y;
        float dist = sqrt(dx * dx + dy * dy);

        if (dist < 2.0f) { // Threshold for arrival
            currentPos = internal_target_memory; 
            sprite_stack.setPosition(currentPos.x, currentPos.y);
            sprite_stack.setRotation(0, 0, 0); // Face "forward" or user
            moving_to_current_target = false; 
            internal_target_memory = {-1,-1}; // Clear memory to ensure next different target forces re-init
            Serial.println("moveSpriteToTarget: Target reached.");
            return true; 
        } else {
            float step = 1.5f; 
            float step_x = (dx / dist) * step;
            float step_y = (dy / dist) * step;
            
            currentPos.x += round(step_x);
            currentPos.y += round(step_y);
            sprite_stack.setPosition(currentPos.x, currentPos.y);

            // Continuously update angle to face the target
            float new_angle_rad = atan2((float)(internal_target_memory.y - currentPos.y),
                                     (float)(internal_target_memory.x - currentPos.x));
            float new_roll_angle_deg = degrees(new_angle_rad) + 270;
            sprite_stack.setRotation(0, 0, new_roll_angle_deg);
            
            // Zoom based on Y position (perspective)
            float size_mult = map(currentPos.y, 120, 200, 100, 200); 
            sprite_stack.setZoom(size_mult);
        }
    }
    return false; // Not yet at the target
}

void walk_to_random_point(SpriteStack &sprite_stack, Point &currentPos) {
  static bool walking_randomly_active = false; 
  static Point random_destination;
  const int margin = 40; // Keep sprite within visible bounds

  if (!walking_randomly_active) {
    // Pick a new random destination on the "ground" (lower part of screen)
    random_destination = random_point(margin, lv_disp_get_hor_res(NULL) - margin, 
                                      lv_disp_get_ver_res(NULL) / 2, lv_disp_get_ver_res(NULL) - margin - 20); // -20 to be a bit above bottom
    walking_randomly_active = true;
    Serial.print("walk_to_random_point: New random destination: X="); Serial.print(random_destination.x); Serial.print(", Y="); Serial.println(random_destination.y);
    // Initial turn will be handled by the first call to moveSpriteToTarget
  }

  // Attempt to move towards the current random_destination
  if (moveSpriteToTarget(sprite_stack, currentPos, random_destination)) {
    walking_randomly_active = false; // Reached the random destination, will pick a new one on next suitable call
    Serial.println("walk_to_random_point: Random destination reached.");
  }
  // If moveSpriteToTarget is interrupted by a user destination, 
  // walking_randomly_active will remain true, and this function will try to resume
  // towards random_destination when next called, unless a new random_destination is picked.
}

void test_user_and_random_walk(SpriteStack &sprite_stack, Point &currentPos) {
    // --- 1. Process Touch Input ---
    lv_coord_t touchX, touchY;
    bool isTouch = validate_touch(&touchX, &touchY); 

    static bool touch_is_being_held = false;
    static unsigned long touch_hold_start_time = 0;

    if (isTouch && touchX >= 0 && touchY >= 120) { // Touch on ground
        if (!touch_is_being_held) { 
            touch_is_being_held = true;
            touch_hold_start_time = millis();
        } else { 
                // Hold duration met, set as new user destination ONLY if not already actively pursuing one
                // OR if you want to allow changing destination mid-way:
            if (millis() - touch_hold_start_time >= USER_DESTINATION_HOLD_DURATION) { // Allows changing target
                Serial.println("New user destination set by hold.");
                currentUserDestination.x = touchX;
                currentUserDestination.y = touchY;
                hasUserDestination = true;         // Prioritize this user destination
                inPostUserTargetCooldown = false;  // New destination overrides any cooldown
                // moveSpriteToTarget will see the new currentUserDestination and adapt.
            }
        }
    } else { 
        if (touch_is_being_held) { 
            touch_is_being_held = false;
            // Optional: Handle tap release if USER_DESTINATION_HOLD_DURATION was not met
            // For example, if you want a tap to also set a destination:
            // if (millis() - touch_hold_start_time < USER_DESTINATION_HOLD_DURATION) {
            //    Serial.println("New user destination set by tap.");
            //    currentUserDestination.x = touchX_last_valid; // Need to store last valid touchX for tap
            //    currentUserDestination.y = touchY_last_valid; // Need to store last valid touchY for tap
            //    hasUserDestination = true;
            //    inPostUserTargetCooldown = false;
            // }
        }
    }

    // --- 2. Movement Logic ---
    if (hasUserDestination) {
        if (moveSpriteToTarget(sprite_stack, currentPos, currentUserDestination)) {
            Serial.println("User destination reached.");
            hasUserDestination = false;             // Clear the user destination flag
            inPostUserTargetCooldown = true;        // Start cooldown
            postUserTargetCooldownStartTime = millis();
        }
    } else if (inPostUserTargetCooldown) {
        if (millis() - postUserTargetCooldownStartTime >= POST_USER_TARGET_COOLDOWN_DURATION) {
            Serial.println("Post-user target cooldown finished.");
            inPostUserTargetCooldown = false;
        }
        // Else, sprite remains idle at the last user destination during cooldown
    } else {
        // No active user destination and not in cooldown, so walk randomly
        walk_to_random_point(sprite_stack, currentPos);
    }
}


RotationAnimation MyRotationAnim(myStack, 0, 360, 2000, 100);
NoNoAnimation NoNoAnim(myStack, -25, 25, 1500, 100);
NodAnimation NodAnim(myStack, -10, 0, 1500, 500);
DanceAnimation DanceAnim(myStack, -20, 0, 2500, 100);
DeselectionAnimation DeseAnim(myStack, -15, 15, 1000, 100);
SelectionAnimation SelectAnim(myStack, 0, 360, 1500, 100);

void test_anims() {
  static bool wasTouched = false;
  // Call get_touch_in_area_center with the correct number of arguments
  bool isTouched = get_touch_in_area_center(myStack.getPosition().x, myStack.getPosition().y, 30, 30, true);

  if (isTouched && !wasTouched && !inCatchingGame) { 
    bool any_anim_active = MyRotationAnim.isActive() || NoNoAnim.isActive() || NodAnim.isActive() ||
                           DanceAnim.isActive() || DeseAnim.isActive() || SelectAnim.isActive();
    if (!any_anim_active) {
        Serial.print("SpriteStack pressed! Starting animation index: "); Serial.println(animIndex);
        switch (animIndex) {
          case 0: DanceAnim.start();      break;
          case 1: MyRotationAnim.start(); break;
          case 2: NoNoAnim.start();       break;
          case 3: NodAnim.start();        break;
          case 4: DeseAnim.start();       break; 
          case 5: SelectAnim.start();     break;
        }
        animIndex = (animIndex + 1) % 6;
    }
  }
  wasTouched = isTouched;

  if (DanceAnim.isActive())      DanceAnim.update();
  if (MyRotationAnim.isActive()) MyRotationAnim.update();
  if (NoNoAnim.isActive())       NoNoAnim.update();
  if (NodAnim.isActive())        NodAnim.update();
  if (DeseAnim.isActive())       DeseAnim.update();
  if (SelectAnim.isActive())     SelectAnim.update();
}

// -------------------------
// SETUP & LOOP
// -------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000); 
  Serial.println("LVGL Sprite Stack Test - Setup Start");

  lv_init();
  Serial.println("LVGL initialized.");

  lv_xiao_disp_init();
  Serial.println("Display initialized.");

  lv_xiao_touch_init();
  Serial.println("Touch initialized.");

  Wire.begin();
  rtc.begin();
  Serial.println("RTC initialized.");

  mainScreen = lv_obj_create(NULL);
  lv_obj_remove_style_all(mainScreen); 
  lv_obj_set_size(mainScreen, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL)); 
  lv_scr_load(mainScreen);
  Serial.println("Main screen created and loaded.");

  myStack.create(mainScreen);
  myStack.setPosition(g_spritePosition.x, g_spritePosition.y);
  myStack.setZoom(100.0f); 
  Serial.println("myStack (frog) created.");

  burgerStack.create(mainScreen);
  burgerStack.setPosition(burgerPosition.x, burgerPosition.y);
  burgerStack.setZoom(100.0f);
  Serial.println("burgerStack created.");

  bedStack.create(mainScreen);
  bedStack.setPosition(bedPosition.x, bedPosition.y);
  bedStack.setZoom(100.0f);
  Serial.println("bedStack created.");
  
  randomSeed(analogRead(A0)); 
  Serial.println("Random seed set.");
  Serial.println("Setup complete. Starting loop...");
}

void loop() {
  render_scene(mainScreen);

  myStack.update();
  burgerStack.update();
  bedStack.update();

  test_anims(); 
  test_user_and_random_walk(myStack, g_spritePosition); 

  lv_task_handler();
  delay(10); 
}
