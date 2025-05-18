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
const unsigned long USER_DESTINATION_HOLD_DURATION = 2000; 
const unsigned long POST_USER_TARGET_COOLDOWN_DURATION = 7000; 

// --- For Interrupt Animation on myStack ---
bool myStackIsPerformingInterruptAnim = false;    
SpriteStackAnimation* currentInterruptAnimation = nullptr; 
bool wasMovingToUserDestBeforeInterrupt = false;  
Point userDestBeforeInterrupt = {0,0};          

int animIndex = 0; // Index for cycling through interrupt animations

int catFrameCount = sizeof(cat_images) / sizeof(cat_images[0]);
int frogFrameCount = sizeof(frog_images) / sizeof(frog_images[0]);
int burgerFrameCount = sizeof(burger_images) / sizeof(burger_images[0]);
int bedFrameCount = sizeof(bed_images) / sizeof(bed_images[0]);
int ballFrameCount = sizeof(ball_images) / sizeof(ball_images[0]);

SpriteStack myStack(cat_images, catFrameCount, 0, 1.0, 1.0, 150.0f);
SpriteStack burgerStack(burger_images, burgerFrameCount, 0, 1.0, 1.0, 100.0f);
SpriteStack bedStack(bed_images, bedFrameCount, 0, 1.0, 1.0, 100.0f);

SpriteStack* sortable_stacks[] = { &myStack, &burgerStack, &bedStack };
const int NUM_SORTABLE_STACKS = sizeof(sortable_stacks) / sizeof(sortable_stacks[0]);

Point g_spritePosition = {120, 160}; 
Point burgerPosition = {80, 140};    
Point bedPosition = {180, 140};      

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
RotationAnimation MyRotationAnim(myStack, 0, 360, 3000, 0); // No delay for interrupt
NoNoAnimation NoNoAnim(myStack, -25, 25, 1500, 0);
NodAnimation NodAnim(myStack, -10, 0, 3000, 0);
DanceAnimation DanceAnim(myStack, -45, 45, 3000, 0);
DeselectionAnimation DeseAnim(myStack, -15, 15, 3000, 0);
SelectionAnimation SelectAnim(myStack, 0, 360, 3000, 0);


// -------------------------
// UTILITY FUNCTIONS
// (random_int, random_point, interpolate_color - unchanged)
// -------------------------
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
// (render_scene - unchanged from previous version with correct layering)
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
        lv_opa_t px_opa = (lv_opa_t)(LV_OPA_COVER * (1.0f - normalized_dist)); 
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
// (moveSpriteToTarget, compareSpriteStacksByY, updateSpriteStackZOrder - unchanged)
// -------------------------
bool moveSpriteToTarget(SpriteStack &sprite_stack, Point &currentPos, const Point &target) {
    static bool moving_to_current_target = false; 
    static Point internal_target_memory = {-1, -1}; 
    static unsigned long lastUpdate = 0;
    const int delay_ms = 30; 

    // Check if the externally supplied 'target' has changed.
    if (internal_target_memory.x != target.x || internal_target_memory.y != target.y) {
        moving_to_current_target = false; // Force re-initialization for the new target
        internal_target_memory = target;  // Remember this new target
        lastUpdate = millis() - delay_ms; // Prime lastUpdate to allow immediate processing of the new target
        Serial.print("moveSpriteToTarget: New target acquired: X="); Serial.print(target.x); Serial.print(", Y="); Serial.println(target.y);
    }

    // Initialize movement (set rotation and flag) only if not already moving towards the current internal_target_memory
    if (!moving_to_current_target) {
        // Only set rotation if we are not already at the target (to avoid spinning in place)
        float initial_dx = internal_target_memory.x - currentPos.x;
        float initial_dy = internal_target_memory.y - currentPos.y;
        if (sqrt(initial_dx * initial_dx + initial_dy * initial_dy) >= 2.0f) { // Threshold to prevent spinning
            float angle_rad = atan2(initial_dy, initial_dx);
            float roll_angle_deg = degrees(angle_rad) + 270; 
            sprite_stack.setRotation(0, 0, roll_angle_deg); 
        } else {
            sprite_stack.setRotation(0,0,0); // Already at target, face forward
        }
        moving_to_current_target = true;
        // lastUpdate is already primed if it's a new target.
        // If it's resuming after reaching a target, this block is re-entered,
        // and lastUpdate from the previous cycle is used, which is fine.
        // Serial.print("moveSpriteToTarget: Initializing movement to X="); Serial.print(internal_target_memory.x); Serial.print(", Y="); Serial.println(internal_target_memory.y);
    }

    unsigned long now = millis();
    // Only proceed with movement steps if we are flagged as moving and enough time has passed
    if (moving_to_current_target && (now - lastUpdate >= (unsigned long)delay_ms)) {
        lastUpdate = now;
        float dx = internal_target_memory.x - currentPos.x;
        float dy = internal_target_memory.y - currentPos.y;
        float dist = sqrt(dx * dx + dy * dy);

        if (dist < 2.0f) { // Threshold for arrival
            currentPos = internal_target_memory; 
            sprite_stack.setPosition(currentPos.x, currentPos.y);
            sprite_stack.setRotation(0, 0, 0); 
            moving_to_current_target = false; // Stop movement, target reached
            // internal_target_memory = {-1,-1}; // Optionally clear to signify completion for this specific target
            // Serial.println("moveSpriteToTarget: Target reached.");
            return true; 
        } else {
            // Continue moving
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
            
            float base_zoom = sprite_stack.getInitialZoomPercent();
            float size_mult = map(currentPos.y, 120, 200, base_zoom, base_zoom + 100); 
          
            sprite_stack.setZoom(size_mult);
        }
    }
    return false; // Not yet at the target or not time to update
}


static bool walking_randomly_active_flag = false; 
static Point current_random_destination;

void reset_walk_to_random_point_state() {
    walking_randomly_active_flag = false;
    Serial.println("Random walk state reset.");
}

void walk_to_random_point(SpriteStack &sprite_stack, Point &currentPos) {
  const int margin = 40; 

  if (!walking_randomly_active_flag) {
    current_random_destination = random_point(margin, lv_disp_get_hor_res(NULL) - margin, 
                                       lv_disp_get_ver_res(NULL) / 2, lv_disp_get_ver_res(NULL) - margin); 
    walking_randomly_active_flag = true;
    Serial.print("walk_to_random_point: New random destination: X="); Serial.print(current_random_destination.x); Serial.print(", Y="); Serial.println(current_random_destination.y);
  }

  if (moveSpriteToTarget(sprite_stack, currentPos, current_random_destination)) {
    walking_randomly_active_flag = false; 
    // Serial.println("walk_to_random_point: Random destination reached.");
  }
}

int compareSpriteStacksByY(const void* a, const void* b) {
    SpriteStack* stackA = *(SpriteStack**)a;
    SpriteStack* stackB = *(SpriteStack**)b;
    int yA = stackA->getPosition().y;
    int yB = stackB->getPosition().y;
    if (yA < yB) return -1; 
    if (yA > yB) return 1;  
    return 0; 
}

void updateSpriteStackZOrder() {
    qsort(sortable_stacks, NUM_SORTABLE_STACKS, sizeof(SpriteStack*), compareSpriteStacksByY);
    for (int i = 0; i < NUM_SORTABLE_STACKS; ++i) {
        if (sortable_stacks[i]) {
            sortable_stacks[i]->bringToForeground();
        }
    }
}

// -------------------------
// INTERACTION LOGIC (MODIFIED)
// -------------------------

void test_user_and_random_walk(SpriteStack &sprite_stack, Point &currentPos) {
    // If this function is called for myStack AND it's performing an interrupt animation, skip movement.
    if (myStackIsPerformingInterruptAnim) {
        Serial.println("test_user_and_random_walk: Bypassed for myStack due to interrupt animation.");
        return; 
    }

    lv_coord_t touchX, touchY;
    bool isTouch = validate_touch(&touchX, &touchY); 

    static bool touch_is_being_held = false;
    static unsigned long touch_hold_start_time = 0;

    // User destination setting logic - only if not in interrupt animation
    if (!myStackIsPerformingInterruptAnim) {
        if (isTouch && (touchX >= 0 && touchX <= 200) && (touchY >= 120 && touchY <= 220)) { 
            if (!touch_is_being_held) { 
                touch_is_being_held = true;
                touch_hold_start_time = millis();
            } else { 
                if (millis() - touch_hold_start_time >= USER_DESTINATION_HOLD_DURATION) { 
                    Serial.print("New user destination set for myStack: X="); Serial.print(touchX); Serial.print(", Y="); Serial.println(touchY);
                    currentUserDestination.x = touchX;
                    currentUserDestination.y = touchY;
                    hasUserDestination = true;        
                    inPostUserTargetCooldown = false; 
                    reset_walk_to_random_point_state(); 
                }
            }
        }
    }


    // Movement execution logic
    if (hasUserDestination) {
        if (moveSpriteToTarget(sprite_stack, currentPos, currentUserDestination)) {
            Serial.println("myStack reached User destination.");
            hasUserDestination = false;             
            inPostUserTargetCooldown = true;        
            postUserTargetCooldownStartTime = millis();
        }
    } else if (inPostUserTargetCooldown) {
        if (millis() - postUserTargetCooldownStartTime >= POST_USER_TARGET_COOLDOWN_DURATION) {
            Serial.println("myStack Post-user target cooldown finished.");
            inPostUserTargetCooldown = false;
        }
    } else {
        // Only myStack does random walk if not doing user dest or in cooldown
        walk_to_random_point(sprite_stack, currentPos);
        // Other sprites could have their own independent movement logic here if needed
    }
}


void handle_interrupt_animation_state() {
    if (myStackIsPerformingInterruptAnim && currentInterruptAnimation != nullptr) {
        if (!currentInterruptAnimation->isActive()) {
            Serial.println("Interrupt animation for myStack finished. Restoring state.");
            myStackIsPerformingInterruptAnim = false;
            
            if (wasMovingToUserDestBeforeInterrupt) {
                hasUserDestination = true;
                currentUserDestination = userDestBeforeInterrupt;
                Serial.print("Resuming user destination for myStack: X="); Serial.print(currentUserDestination.x); Serial.print(", Y="); Serial.println(currentUserDestination.y);
            } else {
                // If not previously moving to user dest, random walk will resume naturally
                // as test_user_and_random_walk is no longer bypassed for myStack,
                // and reset_walk_to_random_point_state() was called at interrupt start.
                // Serial.println("myStack to resume random walk or idle.");
            }
            currentInterruptAnimation = nullptr; 
        }
    }
}


void test_anims() {
  static bool wasTouched_anim_trigger = false; // Renamed to be specific
  Point stackPos = myStack.getPosition();
  // Use a slightly larger, more reliable hit area for tapping the sprite
  bool isTouched_on_myStack = get_touch_in_area_center(stackPos.x-10, stackPos.y-20, 
                                                       myStack.getZoomPercent() * 0.08, // Approx 8% of current visual width
                                                       myStack.getZoomPercent() * 0.12, // Approx 12% of current visual height
                                                       true); 

  if (isTouched_on_myStack && !wasTouched_anim_trigger && !inCatchingGame) { 
    // Check if myStack is ALREADY doing an interrupt animation OR any of its standard anims are active
    bool is_myStack_already_animating = myStackIsPerformingInterruptAnim ||
                                     MyRotationAnim.isActive() || NoNoAnim.isActive() || NodAnim.isActive() ||
                                     DanceAnim.isActive() || DeseAnim.isActive() || SelectAnim.isActive();
    
    if (!is_myStack_already_animating) {
        Serial.print("myStack tapped! Initiating interrupt animation, index: "); Serial.println(animIndex);
        myStackIsPerformingInterruptAnim = true; 

        wasMovingToUserDestBeforeInterrupt = hasUserDestination;
        if (hasUserDestination) {
            userDestBeforeInterrupt = currentUserDestination;
        }
        
        hasUserDestination = false; // Temporarily stop user dest pursuit
        reset_walk_to_random_point_state(); // Stop current random walk

        myStack.setRotation(0, 0, 0); // Face screen
        
        switch (animIndex) {
          case 0: DanceAnim.start(); currentInterruptAnimation = &DanceAnim; break;
          case 1: MyRotationAnim.start(); currentInterruptAnimation = &MyRotationAnim; break;
          case 2: NoNoAnim.start(); currentInterruptAnimation = &NoNoAnim; break;
          case 3: NodAnim.start(); currentInterruptAnimation = &NodAnim; break;
          case 4: DeseAnim.start(); currentInterruptAnimation = &DeseAnim; break; 
          case 5: SelectAnim.start(); currentInterruptAnimation = &SelectAnim; break;
        }
        animIndex = (animIndex + 1) % 6; 
    }
  }
  wasTouched_anim_trigger = isTouched_on_myStack;

  // Update all animation objects. Their own isActive() will control if they run.
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
  Serial.println("LVGL Sprite Stack Tamagotchi - Setup Start");

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
  Serial.println("myStack (cat) created.");

  burgerStack.create(mainScreen);
  burgerStack.setPosition(burgerPosition.x, burgerPosition.y);
  burgerStack.setZoom(100.0f);
  Serial.println("burgerStack created.");

  bedStack.create(mainScreen);
  bedStack.setPosition(bedPosition.x, bedPosition.y);
  bedStack.setZoom(200.0f);
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

  handle_interrupt_animation_state(); // Check for interrupt animation completion

  test_anims(); // Handles starting interrupt anims & updating all standard anims
  
  // Movement logic is now conditional inside test_user_and_random_walk
  test_user_and_random_walk(myStack, g_spritePosition); 

  updateSpriteStackZOrder();

  lv_task_handler();
  delay(10); 
}
