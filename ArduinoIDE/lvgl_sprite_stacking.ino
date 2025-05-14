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

// Array of pointers to the black cat images
// Consider making this `PROGMEM` if memory is extremely tight, though LVGL might handle this.
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

// Array of pointers to the ball images
const lv_img_dsc_t *ball_images[] = {
  &bb000, &bb001, &bb002, &bb003, &bb004,
  &bb005, &bb006, &bb007, &bb008, &bb009,
  &bb010, &bb011, &bb012, &bb013, &bb014,
  &bb015
};

// Frog sprite images
#include "frog.h" // Contains the actual image data
LV_IMG_DECLARE(frog_1); LV_IMG_DECLARE(frog_2);
LV_IMG_DECLARE(frog_3); LV_IMG_DECLARE(frog_4); LV_IMG_DECLARE(frog_5);
LV_IMG_DECLARE(frog_6); LV_IMG_DECLARE(frog_7);

// Array of pointers to the frog images
const lv_img_dsc_t *frog_images[] = {
  &frog_1, &frog_2, &frog_3, &frog_4, &frog_5,
  &frog_6, &frog_7
};


// -------------------------
// GLOBAL VARIABLES & OBJECTS
// -------------------------

// --- State Flags ---
bool userControlActive = false;     // True if sprite is currently controlled by user touch-hold
bool userTouchActive   = false;     // True if a touch is currently active (pressed down) in the control area
unsigned long userTouchStartTime = 0; // Timestamp when userTouchActive became true
bool inCatchingGame = false;        // True if the catching game is currently active
bool targetReached = false;         // True if the sprite has reached its target in userControlActive mode
unsigned long targetReachedTime = 0;  // Timestamp when targetReached became true

Point userTarget = {0, 0};
int spriteFrameCount = sizeof(frog_images) / sizeof(frog_images[0]);
// Create the player sprite stack using cat_images.
SpriteStack myStack(frog_images, spriteFrameCount, 0, 3.0, 1.0, 200.0f);
// Global variable holding the sprite stack's current position.
Point g_spritePosition = {120, 120};
swipe_tracker_t spriteSwipeTracker = { SWIPE_IDLE, false, SWIPE_DIR_NONE, 0, 0, 0, 0 };
float swipeRollOffset = 0;

// Function prototypes, might be moved to a different file
bool moveSpriteToTarget(SpriteStack &sprite_stack, const Point &target);
void walk_to_random_point(SpriteStack &sprite_stack);

// --- Real-Time Clock (RTC) ---
I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire); // RTC object

// IMPORTANT: Global pointer for the main screen.
// catch_game.cpp will return here when the game ends.
lv_obj_t * mainScreen = NULL;

// -------------------------
// BACKGROUND OBJECTS
// We keep them global so we can control z-order easily if needed
// -------------------------
static lv_obj_t * top_bg = NULL;
static lv_obj_t * bottom_bg = NULL;

// -------------------------
// CANVAS FOR CELESTIAL BODY (SUN / MOON)
// Using LV_IMG_CF_TRUE_COLOR_ALPHA
// -------------------------
#define CELESTIAL_SIZE 60
static lv_obj_t * celestial_canvas = NULL;

// We'll not manually store the pixel data in a static array.
// Instead we'll let LVGL handle it by calling lv_canvas_set_buffer.
// But we *do* need a buffer. In 32-bit color with alpha, each pixel = 4 bytes.
static lv_color_t celestial_buf[CELESTIAL_SIZE * CELESTIAL_SIZE]; // must be large enough

// -------------------------
// UTILITY FUNCTIONS
// -------------------------

/**
 * @brief Generates a random integer within a specified range (inclusive).
 * @param min Minimum value.
 * @param max Maximum value.
 * @return Random integer between min and max.
 */
int random_int(int min, int max) {
  if (min > max) { // Basic error check
    // Swap min and max or return an error indicator
    int temp = min;
    min = max;
    max = temp;
  }
  return min + rand() % (max - min + 1);
}

/**
 * @brief Generates a random point within specified screen boundaries.
 * @param x_min Minimum x-coordinate.
 * @param x_max Maximum x-coordinate.
 * @param y_min Minimum y-coordinate.
 * @param y_max Maximum y-coordinate.
 * @return Point struct with random x and y coordinates.
 */
Point random_point(int x_min, int x_max, int y_min, int y_max) {
  Point p;
  p.x = random_int(x_min, x_max);
  p.y = random_int(y_min, y_max);
  return p;
}

/**
 * @brief Interpolates between two LVGL colors.
 * @param c1 Start color.
 * @param c2 End color.
 * @param t Interpolation factor (0.0 to 1.0). 0.0 returns c1, 1.0 returns c2.
 * @return Interpolated lv_color_t.
 */
lv_color_t interpolate_color(lv_color_t c1, lv_color_t c2, float t) {
    // Clamp t to the range [0, 1]
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    // LVGL v8 uses lv_color_mix. For older versions or manual interpolation:
    uint8_t r = c1.ch.red   + (uint8_t)((float)(c2.ch.red   - c1.ch.red)   * t);
    uint8_t g = c1.ch.green + (uint8_t)((float)(c2.ch.green - c1.ch.green) * t);
    uint8_t b = c1.ch.blue  + (uint8_t)((float)(c2.ch.blue  - c1.ch.blue)  * t);
    return lv_color_make(r, g, b);
}

// Create a gradient background. If parent is NULL, lv_scr_act() is used.
void set_gradient_background(lv_obj_t *parent) {
  if (parent == NULL) parent = lv_scr_act();
  lv_coord_t parent_w = lv_obj_get_width(parent);
  lv_coord_t parent_h = lv_obj_get_height(parent);
  if (parent_w == 0) parent_w = 240;
  if (parent_h == 0) parent_h = 240;

  // Determine if it's night based on RTC time.
  I2C_BM8563_TimeTypeDef timeStruct;
  rtc.getTime(&timeStruct);
  bool isNight = (timeStruct.hours >= 18 || timeStruct.hours < 6);

  isNight = true; // for testing

  // Top (sky) background.
  lv_obj_t * top_bg = lv_obj_create(parent);
  lv_obj_remove_style_all(top_bg);
  lv_obj_set_size(top_bg, parent_w, parent_h * 100 / 240);
  lv_obj_align(top_bg, LV_ALIGN_TOP_LEFT, 0, 0);

  if(isNight){
    lv_obj_set_style_bg_color(top_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(top_bg, lv_color_hex(0x2F4F4F), 0);
  }else{
    lv_obj_set_style_bg_color(top_bg, lv_color_hex(0x4682B4), 0);
    lv_obj_set_style_bg_grad_color(top_bg, lv_color_hex(0x87CEEB), 0);
  }
  lv_obj_set_style_bg_grad_dir(top_bg, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(top_bg, LV_OPA_COVER, 0);
  lv_obj_move_background(top_bg);

  // Bottom (grass) background.
  lv_obj_t * bottom_bg = lv_obj_create(parent);
  lv_obj_remove_style_all(bottom_bg);
  lv_obj_set_size(bottom_bg, parent_w, parent_h * 140 / 240);
  lv_obj_align(bottom_bg, LV_ALIGN_TOP_LEFT, 0, parent_h * 100 / 240);

  if(isNight){
    lv_obj_set_style_bg_color(bottom_bg, lv_color_hex(0x2E8B57), 0);
    lv_obj_set_style_bg_grad_color(bottom_bg, lv_color_hex(0x006400), 0);
  }else{
    lv_obj_set_style_bg_color(bottom_bg, lv_color_hex(0x3CB371), 0);
    lv_obj_set_style_bg_grad_color(bottom_bg, lv_color_hex(0x98FB98), 0);
  }
  lv_obj_set_style_bg_grad_dir(bottom_bg, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(bottom_bg, LV_OPA_COVER, 0);
  lv_obj_move_background(bottom_bg);
}

// -------------------------
// NEW FUNCTION: UPDATE CELESTIAL BODY (SUN / MOON)
// -------------------------
//
// This function creates (if needed) and updates a canvas that displays either a radial
// gradient sun (daytime) or a radial gradient moon (nighttime) which moves along an arc.
// The arc is computed using the current RTC time.
// The sun is drawn for times between 6:00 and 18:00; otherwise the moon is drawn.
void update_celestial_body(lv_obj_t *parent) {
  if (parent == NULL) parent = lv_scr_act();
  
  // Get current RTC time
  I2C_BM8563_TimeTypeDef timeStruct;
  rtc.getTime(&timeStruct);
  int hour = timeStruct.hours;
  int minute = timeStruct.minutes;
  int total_minutes = hour * 60 + minute;
  
  // Determine day or night
  bool isDay = (hour >= 6 && hour < 18);
  
  // Calculate progress along the 12-hour arc
  float t = 0;
  float angle = 0;
  if (isDay) {
    // Daytime: 6:00(360) -> 18:00(1080)
    t = (total_minutes - 360) / 720.0f;
    if(t < 0) t = 0; if(t > 1) t = 1;
    // angle from PI down to 0
    angle = PI - (t * PI);
  } else {
    // Nighttime: 18:00(1080) -> 6:00 next day(1800)
    int adj_minutes = (hour < 6) ? (hour + 24)*60 + minute : total_minutes;
    t = (adj_minutes - 1080) / 720.0f;
    if(t < 0) t = 0; if(t > 1) t = 1;
    // angle from 0..PI
    angle = t * PI;
  }
  
  // Arc center & radius
  const int arc_center_x = 120;
  const int arc_center_y = 120;
  const int arc_radius   = 90;
  
  // Position on the arc
  int body_x = arc_center_x + (int)(arc_radius * cos(angle));
  int body_y = arc_center_y - (int)(arc_radius * sin(angle));
  
  // Create the canvas if not yet
  if (celestial_canvas == NULL) {
    celestial_canvas = lv_canvas_create(parent);
    
    // Using LV_IMG_CF_TRUE_COLOR (hoping your config supports alpha)
    lv_canvas_set_buffer(celestial_canvas,
                         celestial_buf,
                         CELESTIAL_SIZE,
                         CELESTIAL_SIZE,
                         LV_IMG_CF_TRUE_COLOR);

    // Remove default style
    lv_obj_remove_style_all(celestial_canvas);

    // If needed, bring to foreground or reorder as you like
    lv_obj_move_foreground(celestial_canvas);
  }
  
  // Day or night colors
  lv_color_t center_color, edge_color;
  if (isDay) {
    center_color = lv_color_hex(0xFFFF00); // yellow
    edge_color   = lv_color_hex(0xFFA500); // orange
  } else {
    center_color = lv_color_hex(0xFFFFFF); // white
    edge_color   = lv_color_hex(0xAAAAAA); // gray
  }

  // 1) Clear the canvas with full transparency
  lv_canvas_fill_bg(celestial_canvas, lv_color_black(), LV_OPA_TRANSP);

  // 2) Draw the circular gradient
  int c = CELESTIAL_SIZE / 2;
  float max_dist = (float)c;

  for (int y = 0; y < CELESTIAL_SIZE; y++) {
    for (int x = 0; x < CELESTIAL_SIZE; x++) {
      int dx = x - c;
      int dy = y - c;
      float dist = sqrtf((float)(dx*dx + dy*dy));
      if(dist <= max_dist) {
        float factor = dist / max_dist;  
        // interpolate center_color -> edge_color
        lv_color_t px_color = interpolate_color(center_color, edge_color, factor);

        // EXAMPLE: if you want a fade in alpha near the edges:
        //   alpha = 255 * (1.0f - factor);
        // else just do alpha=255 for a solid circle
        uint8_t alpha = (uint8_t)(255 * (1.0f - factor));
        
        // Now set the pixel's color and alpha in two calls:
        lv_canvas_set_px_color(celestial_canvas, x, y, px_color);
        lv_canvas_set_px_opa(celestial_canvas,  x, y, alpha);
      }
    }
  }
  
  // 3) Position the canvas so it's centered at (body_x, body_y)
  lv_obj_set_pos(celestial_canvas, body_x - c, body_y - c);

  // 4) Force LVGL to redraw
  lv_obj_invalidate(celestial_canvas);
}


// -------------------------
// MOVEMENT FUNCTIONS
// -------------------------

// moveSpriteToTarget() gradually moves the sprite stack toward a target point.
// Once the target is reached, the sprite turns to face the user (assumed at {120,240}).
// Uses the global g_spritePosition so that position information persists between states.
bool moveSpriteToTarget(SpriteStack &sprite_stack, const Point &target) {
  // Use a static flag and timing variables (but use global g_spritePosition for position).
  static bool moving = false;
  static unsigned long lastUpdate = 0;
  static int delay_ms = 0;

  if (!moving) {
    delay_ms = map(g_spritePosition.y, 100, 200, 200, 50);  // Adjust mapping as desired.
    float angle = degrees(atan2((float)(target.y - g_spritePosition.y),
                                 (float)(target.x - g_spritePosition.x))) + 270;
    float stretch = map(g_spritePosition.y, 100, 200, 0, -40);
    sprite_stack.setRotation(stretch, 0, angle);
    moving = true;
  }

  unsigned long now = millis();
  if (now - lastUpdate >= (unsigned long)delay_ms) {
    lastUpdate = now;
    float dx = target.x - g_spritePosition.x;
    float dy = target.y - g_spritePosition.y;
    float dist = sqrt(dx * dx + dy * dy);

    if (dist < 1.0f) {
      // Target reached; update global position and turn toward the user.
      g_spritePosition = target;
      sprite_stack.setPosition(g_spritePosition.x, g_spritePosition.y);
      float finalStretch = map(g_spritePosition.y, 100, 200, 0, -40);
      // Assume the user is at bottom-center (120,240)
      sprite_stack.setRotation(finalStretch, 0, 0); // face user
      moving = false;  // Movement complete.
      return true;
    } else {
      float step = 1.0f;  // Step size (pixels); adjust as needed.
      float step_x = (dx / dist) * step;
      float step_y = (dy / dist) * step;
      g_spritePosition.x += round(step_x);
      g_spritePosition.y += round(step_y);
      sprite_stack.setPosition(g_spritePosition.x, g_spritePosition.y);

      float newAngle = degrees(atan2((float)(target.y - g_spritePosition.y),
                                      (float)(target.x - g_spritePosition.x))) + 270;
      float newStretch = map(g_spritePosition.y, 100, 200, 0, -40);
      sprite_stack.setRotation(newStretch, 0, newAngle);
      float size_mult = map(g_spritePosition.y, 100, 200, 100, 250);
      sprite_stack.setZoom(size_mult);
    }
  }
  return false;  // Target not reached yet.
}

// In explore mode the sprite wanders to random points.
void walk_to_random_point(SpriteStack &sprite_stack) {
  static bool moving = false;
  static Point dest;
  static unsigned long lastUpdate = 0;
  static int delay_ms = 0;
  const int margin = 40;  // Use a margin so the sprite stays visible.

  if (!moving) {
    dest = random_point(margin, 240 - margin, 100, 200);
    delay_ms = map(g_spritePosition.y, 100, 200, 200, 50);
    float angle = degrees(atan2((float)(dest.y - g_spritePosition.y),
                                 (float)(dest.x - g_spritePosition.x))) + 270;
    float stretch = map(g_spritePosition.y, 100, 200, 0, -40);
    sprite_stack.setRotation(stretch, 0, angle);
    moving = true;
  }

  unsigned long now = millis();
  if (now - lastUpdate >= (unsigned long)delay_ms) {
    lastUpdate = now;
    float dx = dest.x - g_spritePosition.x;
    float dy = dest.y - g_spritePosition.y;
    float dist = sqrt(dx * dx + dy * dy);

    if (dist < 1.0f) {
      g_spritePosition = dest;
      sprite_stack.setPosition(g_spritePosition.x, g_spritePosition.y);
      moving = false;  // Next call will pick a new destination.
    } else {
      float step = 1.0f;
      float step_x = (dx / dist) * step;
      float step_y = (dy / dist) * step;
      g_spritePosition.x += round(step_x);
      g_spritePosition.y += round(step_y);
      sprite_stack.setPosition(g_spritePosition.x, g_spritePosition.y);

      float newAngle = degrees(atan2((float)(dest.y - g_spritePosition.y),
                                      (float)(dest.x - g_spritePosition.x))) + 270;
      float newStretch = map(g_spritePosition.y, 100, 200, 0, -40);
      sprite_stack.setRotation(newStretch, 0, newAngle);
      float size_mult = map(g_spritePosition.y, 100, 200, 100, 250);
      sprite_stack.setZoom(size_mult);
    }
  }
}

void test_user_and_random_walk(SpriteStack &sprite_stack) {
  // --- 1. Process Touch Input ---
  lv_coord_t touchX, touchY;
  bool isTouch = validate_touch(&touchX, &touchY);

  // Only check for user input if not already in user control mode.
  if (!userControlActive) {
    // Accept touches only within the defined valid region.
    if (isTouch && (touchX >= 40 && touchX <= 200) && (touchY >= 100 && touchY <= 200)) {
      if (!userTouchActive) {
        // New valid touch: record start time and target.
        userTouchActive = true;
        userTouchStartTime = millis();
        userTarget.x = touchX;
        userTarget.y = touchY;
        // If a new touch occurs, cancel any waiting state.
        targetReached = false;
      } else {
        // If the touch is held continuously for at least 3 seconds, switch to user-controlled mode.
        if (millis() - userTouchStartTime >= 3000) {
          Serial.println("User Location Sensed");
          userControlActive = true;
        }
      }
    } else {
      // If the touch is no longer valid and user control is not active, reset touch state.
      userTouchActive = false;
      userTouchStartTime = 0;
    }
  }
  
  // --- 2. Decide Which Movement to Execute ---
  // When user control is active, call moveSpriteToTarget. This will override any random walk in progress.
  if (userControlActive) {
    bool reached = moveSpriteToTarget(sprite_stack, userTarget);
    if (reached) {
      // When the target is reached, record the time and reset flags.
      targetReached = true;
      targetReachedTime = millis();
      userControlActive = false;
    }
  } 
  else {
    // If no user control is active, perform random wandering.
    // If a target was reached recently, wait 10 seconds before starting a new random walk.
    if (targetReached) {
      if (millis() - targetReachedTime >= 10000) {
        targetReached = false;
        walk_to_random_point(sprite_stack);
      }
      // Otherwise, hold the sprite at the reached target.
    } else {
      walk_to_random_point(sprite_stack);
    }
  }
}



RotationAnimation myRotationAnim(myStack, 0, 360, 3000, 100);
NoNoAnimation NoNoAnim(myStack, -30, 30, 3000, 1000);
NodAnimation NodAnim(myStack, -15, 0, 2000, 500);
DanceAnimation DanceAnim(myStack, -30, 0, 3000, 100);
DeselectionAnimation DeseAnim(myStack, -10, 10, 2000, 100);
SelectionAnimation SelectAnim(myStack, 0, 360, 1000, 100);

// -------------------------
// SETUP & LOOP
// -------------------------
void setup() {
  Serial.begin(115200);
  lv_init();
  lv_xiao_disp_init();
  lv_xiao_touch_init();

  // RTC
  Wire.begin();  // Init I2C
  rtc.begin();   // Init RTC

  // Create and load the main screen.
  mainScreen = lv_obj_create(NULL);
  lv_obj_remove_style_all(mainScreen);
  set_gradient_background(mainScreen);
  lv_scr_load(mainScreen);

  // Initialize the cat sprite stack on the main screen.
  myStack.create(lv_scr_act());
  myStack.setPosition(g_spritePosition.x, g_spritePosition.y);
  myStack.setZoom(100);

  // Seed the random number generator.
  randomSeed(analogRead(0));

}

void loop() {

  // uncomment to test rotations of a sprite stack
  //test_sprite_stack(myStack);

  // uncomment to test the walking functionality
  test_user_and_random_walk(myStack);


  // If user touches the center region (adjust parameters as needed) and game is not active, start the catching game.
  //if (get_touch_in_area_center(120, 120, 50, 50, true) && !inCatchingGame) {
  //  createCatchingGameScreen();
  //}


  /*
  if (get_touch_in_area_center(120, 120, 50, 50, true) && !inCatchingGame) {
    Serial.println("SpriteStack pressed! Starting rotation animation.");
    DanceAnim.start();
    //SelectAnim.start();
    // Start the catching game.
    //createCatchingGameScreen();
  }

  if(DanceAnim.isActive()){
    DanceAnim.update();
    //SelectAnim.update();
  }
  */

  // If in catching game, update its logic.
  //if (inCatchingGame) {
  //  updateCatchingGame();
  //}

  I2C_BM8563_DateTypeDef dateStruct;
  I2C_BM8563_TimeTypeDef timeStruct;

  // Get RTC
  /*
  rtc.getDate(&dateStruct);
  rtc.getTime(&timeStruct);


  Serial.print(dateStruct.year);
  Serial.print(", ");
  Serial.print(dateStruct.month);
  Serial.print(", ");
  Serial.print(dateStruct.date);
  Serial.print(", ");
  Serial.print(timeStruct.hours);
  Serial.print(", ");
  Serial.print(timeStruct.minutes);
  Serial.print(", ");
  Serial.print(timeStruct.seconds);
  Serial.println();
  */

  // Update the moving sun/moon.
  //update_celestial_body(mainScreen);

  lv_task_handler();
  delay(10);
}