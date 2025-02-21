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

#include "ball_sprites_true_color_alpha.h"

LV_IMG_DECLARE(bb000);  LV_IMG_DECLARE(bb001);  LV_IMG_DECLARE(bb002);
LV_IMG_DECLARE(bb003);  LV_IMG_DECLARE(bb004);  LV_IMG_DECLARE(bb005);
LV_IMG_DECLARE(bb006);  LV_IMG_DECLARE(bb007);  LV_IMG_DECLARE(bb008);
LV_IMG_DECLARE(bb009);  LV_IMG_DECLARE(bb010);  LV_IMG_DECLARE(bb011);
LV_IMG_DECLARE(bb012);  LV_IMG_DECLARE(bb013);  LV_IMG_DECLARE(bb014);
LV_IMG_DECLARE(bb015);

const lv_img_dsc_t *ball_images[] = {
  &bb000, &bb001, &bb002, &bb003, &bb004,
  &bb005, &bb006, &bb007, &bb008, &bb009,
  &bb010, &bb011, &bb012, &bb013, &bb014,
  &bb015
};

// -------------------------
// GLOBAL VARIABLES & OBJECTS
// -------------------------
bool userControlActive = false;
bool userTouchActive   = false;
unsigned long userTouchStartTime = 0;
Point userTarget = {0, 0};
bool inCatchingGame = false;
bool targetReached = false;
unsigned long targetReachedTime = 0;
int spriteFrameCount = 16;  // Total frames in the cat sprite set.
// Create the player sprite stack using cat_images.
SpriteStack myStack(cat_images, spriteFrameCount, 0, 3.0, 1.0, 100.0f);
// Global variable holding the sprite stack's current position.
Point g_spritePosition = {120, 120};
swipe_tracker_t spriteSwipeTracker = { SWIPE_IDLE, false, SWIPE_DIR_NONE, 0, 0, 0, 0 };
float swipeRollOffset = 0;

// IMPORTANT: Global pointer for the main screen.
// catch_game.cpp will return here when the game ends.
lv_obj_t * mainScreen = NULL;

// -------------------------
// UTILITY FUNCTIONS
// -------------------------
int random_int(int min, int max) {
  return min + rand() % (max - min + 1);
}

Point random_point(int x_min, int x_max, int y_min, int y_max) {
  Point p;
  p.x = random_int(x_min, x_max);
  p.y = random_int(y_min, y_max);
  return p;
}

// Create a gradient background. If parent is NULL, lv_scr_act() is used.
void set_gradient_background(lv_obj_t *parent) {
  if (parent == NULL) parent = lv_scr_act();
  lv_coord_t parent_w = lv_obj_get_width(parent);
  lv_coord_t parent_h = lv_obj_get_height(parent);
  if (parent_w == 0) parent_w = 240;
  if (parent_h == 0) parent_h = 240;

  // Top (sky) background.
  lv_obj_t * top_bg = lv_obj_create(parent);
  lv_obj_remove_style_all(top_bg);
  lv_obj_set_size(top_bg, parent_w, parent_h * 100 / 240);
  lv_obj_align(top_bg, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(top_bg, lv_color_hex(0x4682B4), 0);
  lv_obj_set_style_bg_grad_color(top_bg, lv_color_hex(0x87CEEB), 0);
  lv_obj_set_style_bg_grad_dir(top_bg, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(top_bg, LV_OPA_COVER, 0);
  lv_obj_move_background(top_bg);

  // Bottom (grass) background.
  lv_obj_t * bottom_bg = lv_obj_create(parent);
  lv_obj_remove_style_all(bottom_bg);
  lv_obj_set_size(bottom_bg, parent_w, parent_h * 140 / 240);
  lv_obj_align(bottom_bg, LV_ALIGN_TOP_LEFT, 0, parent_h * 100 / 240);
  lv_obj_set_style_bg_color(bottom_bg, lv_color_hex(0x3CB371), 0);
  lv_obj_set_style_bg_grad_color(bottom_bg, lv_color_hex(0x98FB98), 0);
  lv_obj_set_style_bg_grad_dir(bottom_bg, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(bottom_bg, LV_OPA_COVER, 0);
  lv_obj_move_background(bottom_bg);
}

// -------------------------
// SETUP & LOOP
// -------------------------
void setup() {
  lv_init();
  lv_xiao_disp_init();
  lv_xiao_touch_init();

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
  // If user touches the center region (adjust parameters as needed) and game is not active, start the catching game.
  if (get_touch_in_area_center(120, 120, 50, 50, true) && !inCatchingGame) {
    createCatchingGameScreen();
  }

  // If in catching game, update its logic.
  if (inCatchingGame) {
    updateCatchingGame();
  }


  lv_task_handler();
  delay(16);
}