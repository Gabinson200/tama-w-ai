// -------------------------
// INCLUDES & DEFINITIONS
// -------------------------
#include <Arduino.h>
#include <lvgl.h>
#include "lv_xiao_round_screen.h"
#include "touch.h"     // Provides validate_touch() and other touch helper functions.
#include "sprite_stack.h"
#include "anim.h"
#include <stdlib.h>
#include <math.h>
#include "catch_game.h"

// -------------------------
// STRUCTURES & FORWARD DECLARATIONS
// -------------------------


// -------------------------
// SPRITE IMAGE DECLARATIONS
// -------------------------
#include "black_cat_true_color_alpha.h"

LV_IMG_DECLARE(black_cat000);  LV_IMG_DECLARE(black_cat001);  LV_IMG_DECLARE(black_cat002);
LV_IMG_DECLARE(black_cat003);  LV_IMG_DECLARE(black_cat004);  LV_IMG_DECLARE(black_cat005);
LV_IMG_DECLARE(black_cat006);  LV_IMG_DECLARE(black_cat007);  LV_IMG_DECLARE(black_cat008);
LV_IMG_DECLARE(black_cat009);  LV_IMG_DECLARE(black_cat010);  LV_IMG_DECLARE(black_cat011);
LV_IMG_DECLARE(black_cat012);  LV_IMG_DECLARE(black_cat013);  LV_IMG_DECLARE(black_cat014);
LV_IMG_DECLARE(black_cat015);

const lv_img_dsc_t *cat_images[] = {
  &black_cat000, &black_cat001, &black_cat002, &black_cat003, &black_cat004,
  &black_cat005, &black_cat006, &black_cat007, &black_cat008, &black_cat009,
  &black_cat010, &black_cat011, &black_cat012, &black_cat013, &black_cat014,
  &black_cat015
};

// -------------------------
// GLOBAL VARIABLES & OBJECTS
// -------------------------
bool userControlActive = false;  // True when a valid long press (>3 sec) is active.
bool userTouchActive   = false;  // True while a valid touch is active.
unsigned long userTouchStartTime = 0;
Point userTarget = {0, 0};
bool inCatchingGame = false;
// These globals manage the post-target wait before wander mode.
bool targetReached = false;
unsigned long targetReachedTime = 0;
int spriteFrameCount = 16; // Total number of frames in your cat sprite set.
// Create the sprite stack (using cat_images in this example).
SpriteStack myStack(cat_images, spriteFrameCount, 0, 3.0, 1.0, 100.0f);

// Global variable holding the sprite stack's current position.
// (Initialize with the starting position.)
Point g_spritePosition = {120, 120};
swipe_tracker_t spriteSwipeTracker = { SWIPE_IDLE, false, SWIPE_DIR_NONE, 0, 0, 0, 0 };
// Global variable to track the additional roll offset due to swipe gestures.
float swipeRollOffset = 0;

// Function prototypes (renamed to avoid conflicts)
bool moveSpriteToTarget(SpriteStack &sprite_stack, const Point &target);
void walk_to_random_point(SpriteStack &sprite_stack);

// -------------------------
// UTILITY FUNCTIONS
// -------------------------

// Return a random integer in [min, max]
int random_int(int min, int max) {
  return min + rand() % (max - min + 1);
}

// Return a random point within the given bounds.
Point random_point(int x_min, int x_max, int y_min, int y_max) {
  Point p;
  p.x = random_int(x_min, x_max);
  p.y = random_int(y_min, y_max);
  return p;
}

// Create a gradient background.
// (Modified to take an optional parent. If parent is NULL, uses lv_scr_act().)
void set_gradient_background(lv_obj_t *parent) {
  if (parent == NULL) parent = lv_scr_act();

  // Optionally get the parent’s dimensions
  lv_coord_t parent_w = lv_obj_get_width(parent);
  lv_coord_t parent_h = lv_obj_get_height(parent);

  // For this example, assume a 240×240 screen if the parent's size is 0.
  if (parent_w == 0) parent_w = 240;
  if (parent_h == 0) parent_h = 240;

  // Create the top background (sky)
  lv_obj_t *top_bg = lv_obj_create(parent);
  lv_obj_remove_style_all(top_bg);
  lv_obj_set_size(top_bg, parent_w, parent_h * 100 / 240);
  lv_obj_align(top_bg, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(top_bg, lv_color_hex(0x4682B4), 0);   // Light blue
  lv_obj_set_style_bg_grad_color(top_bg, lv_color_hex(0x87CEEB), 0); // Darker blue
  lv_obj_set_style_bg_grad_dir(top_bg, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(top_bg, LV_OPA_COVER, 0);
  lv_obj_move_background(top_bg);  // Ensure this is in the back

  // Create the bottom background (grass)
  lv_obj_t *bottom_bg = lv_obj_create(parent);
  lv_obj_remove_style_all(bottom_bg);
  lv_obj_set_size(bottom_bg, parent_w, parent_h * 140 / 240);
  lv_obj_align(bottom_bg, LV_ALIGN_TOP_LEFT, 0, parent_h * 100 / 240);
  lv_obj_set_style_bg_color(bottom_bg, lv_color_hex(0x3CB371), 0);   // Light green
  lv_obj_set_style_bg_grad_color(bottom_bg, lv_color_hex(0x98FB98), 0); // Darker green
  lv_obj_set_style_bg_grad_dir(bottom_bg, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(bottom_bg, LV_OPA_COVER, 0);
  lv_obj_move_background(bottom_bg);  // Ensure this is behind other objects
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
  lv_init();
  lv_xiao_disp_init();
  lv_xiao_touch_init();
  set_gradient_background();

  myStack.create(lv_scr_act());
  // Initialize the sprite stack's position using the global variable.
  myStack.setPosition(g_spritePosition.x, g_spritePosition.y);
  myStack.setZoom(100);  // Initial zoom.

  // Seed the random number generator.
  randomSeed(analogRead(0));

}

void loop() {
  // uncomment to test rotations of a sprite stack
  // test_sprite_stack(myStack);

  // uncomment to test the walking functionality
  //test_user_and_random_walk(myStack);
  
  if (get_touch_in_area_center(120, 120, 50, 50, true) && !inCatchingGame) {
    //Serial.println("SpriteStack pressed! Starting rotation animation.");
    //myRotationAnim.start();
    //SelectAnim.start();
    // Start the catching game.
    createCatchingGameScreen();
  }

  //if(SelectAnim.isActive()){
    //myRotationAnim.update();
    //SelectAnim.update();
  //}
  if(inCatchingGame){
    updateCatchingGame();
  }


  // --- 5. Process LVGL tasks and delay to run ~60 FPS ---
  lv_task_handler();
  delay(16);

}