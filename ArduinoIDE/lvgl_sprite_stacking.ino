// -------------------------
// INCLUDES & DEFINITIONS
// -------------------------
#include <Arduino.h>
#include <lvgl.h>
#include "lv_xiao_round_screen.h" // Specific display library
#include "touch.h"                // Touch input handling (should include GestureRecognizers.h or its content)
#include "sprite_stack.h"         // Custom sprite stacking class
#include "anim.h"                 // Animation classes
#include <stdlib.h>               // For rand(), randomSeed()
#include <math.h>                 // For math functions like atan2, sqrt, cos, sin
#include "catch_game.h"           // Catching game logic
#include "I2C_BM8563.h"           // RTC library

// If GestureRecognizers.h is a separate file and not included via touch.h:
// #include "GestureRecognizers.h"


// -------------------------
// SPRITE IMAGE DECLARATIONS
// (Your image declarations remain the same)
// -------------------------
// Black Cat sprite images
#include "black_cat_true_color_alpha.h"
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
#include "ball_sprites_true_color_alpha.h"
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
#include "frog.h"
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
bool hasUserDestination = false;
Point currentUserDestination = {0,0};
bool inPostUserTargetCooldown = false;
unsigned long postUserTargetCooldownStartTime = 0;
const unsigned long USER_DESTINATION_HOLD_DURATION = 2000; // Adjusted from 5000 for easier testing
const unsigned long POST_USER_TARGET_COOLDOWN_DURATION = 3000; // Adjusted from 7000

// --- For Interrupt Animation on myStack ---
bool myStackIsPerformingInterruptAnim = false;
SpriteStackAnimation* currentInterruptAnimation = nullptr;
bool wasMovingToUserDestBeforeInterrupt = false;
Point userDestBeforeInterrupt = {0,0};
// static bool myStack_tap_handled = false; // This is handled by mystack_toggle_tap_recognizer's state
bool show_items = false;

// --- Global Gesture Recognizer Instances ---
//TapGestureRecognizer hamburger_tap_recognizer;
LongPressGestureRecognizer mystack_anim_recognizer;
//TapGestureRecognizer mystack_toggle_tap_recognizer;
//LongPressGestureRecognizer background_long_press_recognizer;
// Add this near the top with other global variables or defines
bool GESTURE_DEBUG_ENABLED = true; // Set to true to enable gesture debug prints, false to disable
TouchInfo global_touch_info; // Single source of truth for current frame's touch


int animIndex = 0; // Index for cycling through interrupt animations

int catFrameCount = sizeof(cat_images) / sizeof(cat_images[0]);
// int frogFrameCount = sizeof(frog_images) / sizeof(frog_images[0]); // Unused
int burgerFrameCount = sizeof(burger_images) / sizeof(burger_images[0]);
int bedFrameCount = sizeof(bed_images) / sizeof(bed_images[0]);
// int ballFrameCount = sizeof(ball_images) / sizeof(ball_images[0]); // Unused

SpriteStack myStack(cat_images, catFrameCount, 0, 1.0, 1.0, 200.0f); // Initial zoom 100%
SpriteStack burgerStack(burger_images, burgerFrameCount, 0, 1.0, 1.0, 100.0f);
SpriteStack bedStack(bed_images, bedFrameCount, 0, 1.0, 1.0, 100.0f);

// Ensure burgerStack and bedStack are included if they are to be sorted when visible
SpriteStack* sortable_stacks[] = { &myStack, &burgerStack, &bedStack };
const int NUM_SORTABLE_STACKS = sizeof(sortable_stacks) / sizeof(sortable_stacks[0]);


Point g_spritePosition = {120, 160};
Point burgerPosition = {60, 140}; // Adjusted for potential visibility
Point bedPosition = {180, 140};  // Adjusted

bool inCatchingGame = false;

I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);
lv_obj_t * mainScreen = NULL;

static lv_obj_t * top_bg = nullptr;
static lv_obj_t * bottom_bg = nullptr;
static lv_obj_t* celestial_canvas= nullptr;
static bool scene_ready = false;

#define CELESTIAL_SIZE 40
static uint8_t celestial_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(CELESTIAL_SIZE, CELESTIAL_SIZE)];

void reset_walk_to_random_point_state(); // Forward declaration

// Animation objects for myStack (used for interrupt)
RotationAnimation MyRotationAnim(myStack, 0, 360, 3000, 0);
NoNoAnimation NoNoAnim(myStack, -25, 25, 3000, 0);
NodAnimation NodAnim(myStack, -15, 0, 3000, 0); // Adjusted angle for visibility
DanceAnimation DanceAnim(myStack, -30, 30, 3000, 0); // Adjusted angle
DeselectionAnimation DeseAnim(myStack, -15, 15, 3000, 0);
SelectionAnimation SelectAnim(myStack, 0, 360, 3000, 0); // Full rotation, up and down

SelectionAnimation burgerSelectAnim(burgerStack, 0, 360, 1000, 0);

// -------------------------
// UTILITY FUNCTIONS
// -------------------------
// --- Helper to define a SpriteStack's interactive area ---
// This assumes stack.getPosition() returns the CENTER of the stack.
lv_area_t get_stack_area(const SpriteStack& stack) { // Changed to pass by const reference
    Point stack_center_pos = stack.getPosition();
    int base_render_width = 0;
    int base_render_height = 0;
    stack.getDim(base_render_width, base_render_height); // Gets base w,h of the first image

    float current_zoom_factor = stack.getZoomPercent() / 100.0f;

    // Calculate visual half-width/height based on zoom
    // This assumes the touch area should roughly match the visual size
    lv_coord_t visual_half_width = (lv_coord_t)(base_render_width * current_zoom_factor);
    lv_coord_t visual_half_height = (lv_coord_t)(base_render_height * current_zoom_factor);

    lv_area_t area;
    area.x1 = stack_center_pos.x - visual_half_width;
    area.x2 = stack_center_pos.x + visual_half_width;
    area.y1 = stack_center_pos.y - visual_half_height;
    area.y2 = stack_center_pos.y + visual_half_height;
    return area;
}

int random_int(int min, int max) {
  if (min > max) { int temp = min; min = max; max = temp; }
  if (min == max) return min;
  return min + rand() % (max - min + 1);
}

Point random_point(int x_min, int x_max, int y_min, int y_max) {
  Point p;
  p.x = random_int(x_min, x_max);
  p.y = random_int(y_min, y_max);
  return p;
}

static inline lv_color_t interpolate_color(lv_color_t c1, lv_color_t c2, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  uint8_t mix = (uint8_t)(t * 255.0f);
  return lv_color_mix(c2, c1, mix);
}

// -------------------------
// SCENE RENDERING
// -------------------------
void render_scene(lv_obj_t* parent = nullptr) {
  if (!scene_ready) {
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

    scene_ready = true;
  }

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
  const int sky_height = lv_obj_get_height(top_bg);

  const int orbit_center_x = screen_width / 2;
  const int orbit_radius_x = screen_width / 2;
  const int orbit_radius_y = sky_height * 0.8f;
  const int horizon_y_offset = sky_height;

  int celestial_pos_x = orbit_center_x - cosf(celestial_angle_rad) * orbit_radius_x;
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
        lv_opa_t px_opa = (lv_opa_t)(LV_OPA_COVER * (1.0f - powf(normalized_dist, 0.5f))); // Adjusted for softer edge
        lv_canvas_set_px_opa(celestial_canvas, x_px, y_px, px_opa);
      } else {
        lv_canvas_set_px_opa(celestial_canvas, x_px, y_px, LV_OPA_TRANSP);
      }
    }
  }
  lv_obj_set_pos(celestial_canvas, celestial_pos_x - CELESTIAL_SIZE / 2, celestial_pos_y - CELESTIAL_SIZE / 2);
  lv_obj_invalidate(celestial_canvas);

  lv_obj_move_background(bottom_bg);
  lv_obj_move_background(celestial_canvas);
  lv_obj_move_background(top_bg);
}


// -------------------------
// MOVEMENT & Y-SORTING FUNCTIONS
// -------------------------
bool moveSpriteToTarget(SpriteStack &sprite_stack, Point &currentPos, const Point &target) {
    static bool moving_to_current_target = false;
    static Point internal_target_memory = {-1, -1};
    static unsigned long lastUpdate = 0;
    const int delay_ms = 30;

    if (internal_target_memory.x != target.x || internal_target_memory.y != target.y) {
        moving_to_current_target = false;
        internal_target_memory = target;
        lastUpdate = millis() - delay_ms;
        // Serial.print("moveSpriteToTarget: New target: X="); Serial.print(target.x); Serial.print(", Y="); Serial.println(target.y);
    }

    if (!moving_to_current_target) {
        float initial_dx = internal_target_memory.x - currentPos.x;
        float initial_dy = internal_target_memory.y - currentPos.y;
        if (sqrt(initial_dx * initial_dx + initial_dy * initial_dy) >= 2.0f) {
            float angle_rad = atan2(initial_dy, initial_dx);
            float roll_angle_deg = degrees(angle_rad) + 270;
            if (roll_angle_deg >= 360) roll_angle_deg -=360;
            sprite_stack.setRotation(0, 0, roll_angle_deg);
        } else {
            sprite_stack.setRotation(0,0,0);
        }
        moving_to_current_target = true;
    }

    unsigned long now = millis();
    if (moving_to_current_target && (now - lastUpdate >= (unsigned long)delay_ms)) {
        lastUpdate = now;
        float dx = internal_target_memory.x - currentPos.x;
        float dy = internal_target_memory.y - currentPos.y;
        float dist = sqrt(dx * dx + dy * dy);

        if (dist < 2.0f) {
            currentPos = internal_target_memory;
            sprite_stack.setPosition(currentPos.x, currentPos.y);
            sprite_stack.setRotation(0, 0, 0);
            moving_to_current_target = false;
            return true;
        } else {
            float step = 1.5f; // Speed of movement
             // Ensure step is not greater than dist to prevent overshoot
            if (step > dist) step = dist;

            float step_x = (dx / dist) * step;
            float step_y = (dy / dist) * step;

            currentPos.x += round(step_x);
            currentPos.y += round(step_y);
            sprite_stack.setPosition(currentPos.x, currentPos.y);

            float new_angle_rad = atan2((float)(internal_target_memory.y - currentPos.y),
                                     (float)(internal_target_memory.x - currentPos.x));
            float new_roll_angle_deg = degrees(new_angle_rad) + 270;
            if (new_roll_angle_deg >= 360) new_roll_angle_deg -=360;

            sprite_stack.setRotation(0, 0, new_roll_angle_deg);

            // Dynamic zoom based on Y position
            // Map Y from 100 (further, smaller) to 200 (closer, larger)
            // Assuming base zoom is 100% (what's passed as init_zoom_percent)
            float min_y_for_zoom = 100.0f;
            float max_y_for_zoom = 200.0f;
            float min_zoom_perc = sprite_stack.getInitialZoomPercent() * 0.8f; // Example: 80% of initial
            float max_zoom_perc = sprite_stack.getInitialZoomPercent() * 1.5f; // Example: 150% of initial

            float mapped_zoom_perc = map(currentPos.y, min_y_for_zoom, max_y_for_zoom, min_zoom_perc, max_zoom_perc);
            mapped_zoom_perc = constrain(mapped_zoom_perc, min(min_zoom_perc, max_zoom_perc), max(min_zoom_perc, max_zoom_perc) );
            sprite_stack.setZoom(mapped_zoom_perc);
        }
    }
    return false;
}


static bool walking_randomly_active_flag = false;
static Point current_random_destination;

void reset_walk_to_random_point_state() {
    walking_randomly_active_flag = false;
    // Serial.println("Random walk state reset.");
}

void walk_to_random_point(SpriteStack &sprite_stack, Point &currentPos) {
  const int margin = 20; // Reduced margin to allow more screen space

  if (!walking_randomly_active_flag) {
    // Keep Y in the "ground" area, e.g., bottom 2/3 of the screen
    int minY = lv_disp_get_ver_res(NULL) / 3;
    int maxY = lv_disp_get_ver_res(NULL) - margin - (sprite_stack.getZoomPercent()/100.0f * 30 / 2); // Adjust for sprite height
    if (maxY <= minY) maxY = minY + margin;


    current_random_destination = random_point(margin, lv_disp_get_hor_res(NULL) - margin,
                                       120, lv_disp_get_ver_res(NULL) - margin);
    walking_randomly_active_flag = true;
    // Serial.print("walk_to_random_point: New random dest: X="); Serial.print(current_random_destination.x); Serial.print(", Y="); Serial.println(current_random_destination.y);
  }

  if (moveSpriteToTarget(sprite_stack, currentPos, current_random_destination)) {
    walking_randomly_active_flag = false;
  }
}

int compareSpriteStacksByY(const void* a, const void* b) {
    SpriteStack* stackA = *(SpriteStack**)a;
    SpriteStack* stackB = *(SpriteStack**)b;

    // Handle cases where one or both stacks might not be "created" yet (burger/bed)
    // This typically means their underlying LVGL objects don't exist.
    // getPosition() should still return their base_x, base_y.
    // For sorting, if one isn't "created" (visible), it could be treated as behind.
    // However, this sort is for visible objects. Assume they are valid if in this array and processed.

    int yA = stackA->getPosition().y;
    int yB = stackB->getPosition().y;
    if (yA < yB) return -1;
    if (yA > yB) return 1;
    return 0;
}

void updateSpriteStackZOrder() {
    // Filter out stacks that are not currently created/visible if necessary,
    // or ensure sortable_stacks only contains pointers to active stacks.
    // For now, assuming all in sortable_stacks are meant to be layered if they exist.
    qsort(sortable_stacks, NUM_SORTABLE_STACKS, sizeof(SpriteStack*), compareSpriteStacksByY);
    for (int i = 0; i < NUM_SORTABLE_STACKS; ++i) {
        if (sortable_stacks[i] != nullptr && sortable_stacks[i]->getLVGLObject() != nullptr) { // Check if stack is created
            sortable_stacks[i]->bringToForeground();
        }
    }
}

// -------------------------
// INTERACTION LOGIC (Old - To be removed or refactored)
// -------------------------
// test_user_and_random_walk function is no longer the primary way to set destination.
// Its movement logic is now integrated into the main loop based on hasUserDestination flag.
// Its random walk part is called explicitly from the main loop.
// This function can be removed if not used elsewhere.

// -------------------------
// ANIMATION STATE HANDLING
// -------------------------
void handle_interrupt_animation_state() {
    if (myStackIsPerformingInterruptAnim && currentInterruptAnimation != nullptr) {
        if (!currentInterruptAnimation->isActive()) {
            Serial.println("GR: Interrupt animation finished. Restoring state.");
            myStackIsPerformingInterruptAnim = false;

            if (wasMovingToUserDestBeforeInterrupt) {
                hasUserDestination = true;
                currentUserDestination = userDestBeforeInterrupt;
                // Serial.print("Resuming user destination: X="); Serial.print(currentUserDestination.x); Serial.print(", Y="); Serial.println(currentUserDestination.y);
            }
            currentInterruptAnimation = nullptr;
        }
    }
}

// test_anims() function is also obsolete as its logic is within gesture recognizer ENDED state.

// -------------------------
// SETUP
// -------------------------
void setup() {
  Serial.begin(115200);
  unsigned long setup_start_time = millis();
  while (!Serial && (millis() - setup_start_time < 2000)) { delay(10); } // Timeout for Serial
  Serial.println("LVGL Sprite Stack - Setup Start");

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
  myStack.setZoom(200.0f); // Set initial zoom
  Serial.println("myStack (cat) created.");
  int w, h;
  myStack.getDim(w,h);
  Serial.print("myStack base dims: W="); Serial.print(w); Serial.print(", H="); Serial.println(h);


  // Gesture Recognizer Configurations
  // Hamburger area will be set when it's created. Initial config is minimal.
  //hamburger_tap_recognizer.set_config(1000, 40); // 300ms, 10px move

  mystack_anim_recognizer.set_config(1000, 80); // long press for animation
  //mystack_toggle_tap_recognizer.set_config(500, 40); // Tap for item toggle

  //lv_area_t background_area;
  //background_area.x1 = 0;
  //background_area.y1 = 120; // Example: Only allow hold on bottom part of screen
  //background_area.x2 = lv_disp_get_hor_res(NULL) - 1;
  //background_area.y2 = lv_disp_get_ver_res(NULL) - 1;
  //background_long_press_recognizer.set_target_area(background_area);
  //background_long_press_recognizer.set_config(3000, 40); // 25px move tolerance

  randomSeed(analogRead(A0)); // Seed random number generator
  Serial.println("Random seed set.");
  Serial.println("Setup complete. Starting loop...");
}

// -------------------------
// LOOP
// -------------------------
void loop() {
    // 1. Get current touch state
    global_touch_info.is_pressed = validate_touch(&global_touch_info.x, &global_touch_info.y);
    global_touch_info.timestamp = millis();

    // Basic touch logging for debugging
    if (global_touch_info.is_pressed) {
        Serial.print("Raw Touch: X="); Serial.print(global_touch_info.x);
        Serial.print(", Y="); Serial.println(global_touch_info.y);
    }

    lv_area_t current_mystack_touch_area = get_stack_area(myStack);
    mystack_anim_recognizer.set_target_area(current_mystack_touch_area);

    mystack_anim_recognizer.update(global_touch_info); //update recognizer with glbl touch

    // check state of recgonizer
    GestureState anim_state = mystack_anim_recognizer.get_state();
    if (anim_state == GestureState::ENDED){
        Serial.print("MAIN_APP: SUCCESS! Anim Hold Recognized at (");
        myStack.setRotation(0, 0, 0);
        currentInterruptAnimation = nullptr;

        switch (animIndex) {
          case 0: DanceAnim.start(); currentInterruptAnimation = &DanceAnim; break;
          case 1: MyRotationAnim.start(); currentInterruptAnimation = &MyRotationAnim; break;
          case 2: NoNoAnim.start(); currentInterruptAnimation = &NoNoAnim; break;
          case 3: NodAnim.start(); currentInterruptAnimation = &NodAnim; break;
          case 4: DeseAnim.start(); currentInterruptAnimation = &DeseAnim; break;
          case 5: SelectAnim.start(); currentInterruptAnimation = &SelectAnim; break;
        }
        animIndex = (animIndex + 1) % 6;
        mystack_anim_recognizer.reset();
    } else if (anim_state == GestureState::FAILED || anim_state == GestureState::CANCELLED) {
        // Debug prints within the recognizer should explain why it failed.
        mystack_anim_recognizer.reset();
    }

    // if screen is not pressed and anim has finished or not started ie possible
    if(!global_touch_info.is_pressed){
        if(mystack_anim_recognizer.get_state() == GestureState::POSSIBLE){
            mystack_anim_recognizer.reset();
        }
    }

    render_scene(mainScreen);
    handle_interrupt_animation_state();

    if (MyRotationAnim.isActive()) MyRotationAnim.update();
    if (NoNoAnim.isActive()) NoNoAnim.update();
    if (NodAnim.isActive()) NodAnim.update();
    if (DanceAnim.isActive()) DanceAnim.update();
    if (DeseAnim.isActive()) DeseAnim.update();
    if (SelectAnim.isActive()) SelectAnim.update();
    if (burgerSelectAnim.isActive()) burgerSelectAnim.update();

    myStack.update();

    updateSpriteStackZOrder();
    lv_task_handler();
    delay(15);


    /*
    // 2. Update dynamic target areas for gesture recognizers
    lv_area_t current_mystack_touch_area = get_stack_area(myStack);
    //Serial.print("myStack Calculated Touch Area: x1="); Serial.print(current_mystack_touch_area.x1);
    //Serial.print(", y1="); Serial.print(current_mystack_touch_area.y1);
    //Serial.print(", x2="); Serial.print(current_mystack_touch_area.x2);
    //Serial.print(", y2="); Serial.println(current_mystack_touch_area.y2);
    mystack_anim_tap_recognizer.set_target_area(current_mystack_touch_area);
    mystack_toggle_tap_recognizer.set_target_area(current_mystack_touch_area);
    // Hamburger area is updated when burgerStack is created

    // 3. Set recognizer enabled states based on game logic
    hamburger_tap_recognizer.set_enabled(show_items && burgerStack.getLVGLObject() != nullptr);
    bool can_mystack_be_tapped = !inCatchingGame && !myStackIsPerformingInterruptAnim;
    mystack_anim_tap_recognizer.set_enabled(can_mystack_be_tapped);
    mystack_toggle_tap_recognizer.set_enabled(can_mystack_be_tapped);
    background_long_press_recognizer.set_enabled(!myStackIsPerformingInterruptAnim);


    // 4. Update all gesture recognizers
    hamburger_tap_recognizer.update(global_touch_info);
    mystack_anim_tap_recognizer.update(global_touch_info);
    mystack_toggle_tap_recognizer.update(global_touch_info);
    background_long_press_recognizer.update(global_touch_info);

    // 5. Process gesture states & handle conflicts
    bool tap_action_taken_this_frame = false;

    // Hamburger Tap
    if (hamburger_tap_recognizer.get_state() == GestureState::ENDED) {
        Serial.println("GR: Hamburger Tap Recognized!");
        burgerSelectAnim.start(); // Make sure burgerSelectAnim targets burgerStack
        hamburger_tap_recognizer.reset();
        background_long_press_recognizer.cancel(); // Tap on UI cancels background long press
        background_long_press_recognizer.reset();
        tap_action_taken_this_frame = true;
    }

    // myStack Animation Tap (Priority over toggle if areas overlap significantly)
    if (mystack_anim_tap_recognizer.get_state() == GestureState::ENDED) {
        Serial.println("GR: myStack Animation Tap Recognized!");
        myStackIsPerformingInterruptAnim = true;
        wasMovingToUserDestBeforeInterrupt = hasUserDestination;
        if (hasUserDestination) userDestBeforeInterrupt = currentUserDestination;
        hasUserDestination = false;
        reset_walk_to_random_point_state();
        myStack.setRotation(0, 0, 0);
        currentInterruptAnimation = nullptr;

        switch (animIndex) {
          case 0: DanceAnim.start(); currentInterruptAnimation = &DanceAnim; break;
          case 1: MyRotationAnim.start(); currentInterruptAnimation = &MyRotationAnim; break;
          case 2: NoNoAnim.start(); currentInterruptAnimation = &NoNoAnim; break;
          case 3: NodAnim.start(); currentInterruptAnimation = &NodAnim; break;
          case 4: DeseAnim.start(); currentInterruptAnimation = &DeseAnim; break;
          case 5: SelectAnim.start(); currentInterruptAnimation = &SelectAnim; break;
        }
        animIndex = (animIndex + 1) % 6;

        mystack_anim_tap_recognizer.reset();
        mystack_toggle_tap_recognizer.cancel(); // This tap type takes precedence
        mystack_toggle_tap_recognizer.reset();
        background_long_press_recognizer.cancel();
        background_long_press_recognizer.reset();
        tap_action_taken_this_frame = true;
    }

    // myStack Item Toggle Tap
    if (mystack_toggle_tap_recognizer.get_state() == GestureState::ENDED) {
        Serial.println("GR: myStack Item Toggle Tap Recognized!");
        if (!show_items) {
            burgerStack.create(mainScreen);
            burgerStack.setPosition(burgerPosition.x, burgerPosition.y);
            burgerStack.setZoom(100.0f);
            lv_area_t actual_burger_area = get_stack_area(burgerStack); // Update hamburger area
            hamburger_tap_recognizer.set_target_area(actual_burger_area);
            Serial.print("GR: Burger area updated: x1="); Serial.print(actual_burger_area.x1);
            Serial.print(" y1="); Serial.print(actual_burger_area.y1);
            Serial.print(" x2="); Serial.print(actual_burger_area.x2);
            Serial.print(" y2="); Serial.println(actual_burger_area.y2);


            bedStack.create(mainScreen);
            bedStack.setPosition(bedPosition.x, bedPosition.y);
            bedStack.setZoom(100.0f); // Bed zoom was 200, ensure 100 is intended
            show_items = true;
            Serial.println("GR: Items created.");
        } else {
            if(bedStack.getLVGLObject()) bedStack.destroy(); // Check if created before destroying
            if(burgerStack.getLVGLObject()) burgerStack.destroy();
            show_items = false;
            Serial.println("GR: Items destroyed.");
        }
        mystack_toggle_tap_recognizer.reset();
        background_long_press_recognizer.cancel();
        background_long_press_recognizer.reset();
        tap_action_taken_this_frame = true;
    }

    // Background Long Press
    if (background_long_press_recognizer.get_state() == GestureState::ENDED) {
        if (!myStackIsPerformingInterruptAnim) {
            Serial.print("GR: Background Long Press! Dest: X=");
            Serial.print(background_long_press_recognizer.recognized_at_x);
            Serial.print(", Y="); Serial.println(background_long_press_recognizer.recognized_at_y);

            currentUserDestination.x = background_long_press_recognizer.recognized_at_x;
            currentUserDestination.y = background_long_press_recognizer.recognized_at_y;
            hasUserDestination = true;
            inPostUserTargetCooldown = false;
            reset_walk_to_random_point_state(); // Stop random walk
        }
        background_long_press_recognizer.reset(); // Always reset after handling
    }

    // Reset POSSIBLE recognizers if touch is released and they didn't complete
    if (!global_touch_info.is_pressed) {
        if (hamburger_tap_recognizer.get_state() == GestureState::POSSIBLE) hamburger_tap_recognizer.reset();
        if (mystack_anim_tap_recognizer.get_state() == GestureState::POSSIBLE) mystack_anim_tap_recognizer.reset();
        if (mystack_toggle_tap_recognizer.get_state() == GestureState::POSSIBLE) mystack_toggle_tap_recognizer.reset();
        if (background_long_press_recognizer.get_state() == GestureState::POSSIBLE) background_long_press_recognizer.reset();
    }

    // 6. Update Animations and Game State
    render_scene(mainScreen);
    handle_interrupt_animation_state();

    if (MyRotationAnim.isActive()) MyRotationAnim.update();
    if (NoNoAnim.isActive()) NoNoAnim.update();
    if (NodAnim.isActive()) NodAnim.update();
    if (DanceAnim.isActive()) DanceAnim.update();
    if (DeseAnim.isActive()) DeseAnim.update();
    if (SelectAnim.isActive()) SelectAnim.update();
    if (burgerSelectAnim.isActive()) burgerSelectAnim.update();

    // 7. Sprite Movement Logic
    if (!myStackIsPerformingInterruptAnim) {
        if (hasUserDestination) {
            if (moveSpriteToTarget(myStack, g_spritePosition, currentUserDestination)) {
                Serial.println("GR: Reached User destination.");
                hasUserDestination = false;
                inPostUserTargetCooldown = true;
                postUserTargetCooldownStartTime = millis();
            }
        } else if (inPostUserTargetCooldown) {
            if (millis() - postUserTargetCooldownStartTime >= POST_USER_TARGET_COOLDOWN_DURATION) {
                inPostUserTargetCooldown = false;
                Serial.println("GR: Post-user target cooldown finished.");
            }
        } else { // Idle: No user destination, not in cooldown
            walk_to_random_point(myStack, g_spritePosition);
        }
    }

    // 8. Update SpriteStack LVGL objects and Z-order
    myStack.update();
    if (show_items) {
        if(burgerStack.getLVGLObject()) burgerStack.update(); // Update only if created
        if(bedStack.getLVGLObject()) bedStack.update();
    }
    updateSpriteStackZOrder(); // Ensure this handles potentially uncreated stacks correctly

    // 9. LVGL Task Handler
    lv_task_handler();
    delay(10); // Small delay for stability and to yield time
    */
}