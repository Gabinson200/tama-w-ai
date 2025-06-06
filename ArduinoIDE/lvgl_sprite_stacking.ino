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
#include "sprites.h"              // sprite image declarations
#include "background.h"           // background functions
#include <string.h> // for strncmp(), snprintf()

// -------------------------
// GLOBAL VARIABLES & OBJECTS
// -------------------------

// --- State Flags & Movement Variables ---
// movement state
bool   hasUserDestination         = false;
Point  currentUserDestination     = {0,0};
bool   inPostUserTargetCooldown   = false;
unsigned long postUserTargetTime  = 0;

// random wander state
bool   wanderingActive            = false;
unsigned long lastWanderTime      = 0;
const unsigned long WANDER_INTERVAL = 60UL * 1000UL; // 1 min

// stepping
const unsigned long STEP_INTERVAL = 1000; // 1 times a second

// hold & cooldown durations
const unsigned long USER_HOLD_MS  = 2000;
const unsigned long COOLDOWN_MS   = 60UL * 1000UL;  // also 1 min

enum class MoveState { Idle, ToUser, ToWander };
static MoveState   moveState       = MoveState::Idle;
static Point       moveTarget      = {0,0};
static unsigned long lastStepTime  = 0;


// --- For Interrupt Animation on myStack ---
static bool myStack_tap_handled = false; // Flag to debounce taps on myStack for item toggle
bool show_items = false;

// Animation object for myStack
SpriteStackAnimation* currentActiveAnimation = nullptr; // Pointer to the currently active animation


int catFrameCount = sizeof(cat_images) / sizeof(cat_images[0]);
int frogFrameCount = sizeof(frog_images) / sizeof(frog_images[0]);
int burgerFrameCount = sizeof(burger_images) / sizeof(burger_images[0]);
int bedFrameCount = sizeof(bed_images) / sizeof(bed_images[0]);
int ballFrameCount = sizeof(ball_images) / sizeof(ball_images[0]);

SpriteStack myStack(frog_images, frogFrameCount, 0, 1.0, 1.0, 200.0f);
SpriteStack burgerStack(burger_images, burgerFrameCount, 0, 1.0, 1.0, 100.0f);
SpriteStack bedStack(bed_images, bedFrameCount, 0, 1.0, 1.0, 100.0f);
SpriteStack ballStack(ball_images, ballFrameCount, 0, 1.0, 1.0, 100.0f);

SpriteStack* sortable_stacks[] = { &myStack, &burgerStack, &bedStack, &ballStack };
const int NUM_SORTABLE_STACKS = sizeof(sortable_stacks) / sizeof(sortable_stacks[0]);

Point g_spritePosition = {120, 160}; 
Point burgerPosition = {80, 200};    
Point bedPosition = {180, 200};     
Point ballPosition = {120, 200};

bool inCatchingGame = false;

I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire); 
lv_obj_t * mainScreen = NULL;


void reset_walk_to_random_point_state(); // Forward declaration

// Animation objects for myStack (used for interrupt)
RotationAnimation MyRotationAnim(myStack, 0, 360, 3000, 0, false); // No delay for interrupt
NoNoAnimation NoNoAnim(myStack, -25, 25, 1500, 0);
NodAnimation NodAnim(myStack, -10, 0, 3000, 0);
DanceAnimation DanceAnim(myStack, -45, 45, 3000, 0);
DeselectionAnimation DeseAnim(myStack, -15, 15, 3000, 0);
SelectionAnimation SelectAnim(myStack, 0, 360, 3000, 0);

RotationAnimation turnLeft(myStack, 0, 22, 1000, 0, true); 
RotationAnimation turnRight(myStack, 0, -22, 1000, 0, true);


SelectionAnimation burgerSelectAnim(burgerStack, 0, 360, 3000, 0);
NoNoAnimation bedSelectAnim(bedStack, -15, 15, 3000, 0);
DeselectionAnimation ballSelectAnim(ballStack, -25, 25, 1500, 0);


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

/*
// Helper struct for bounding box
struct SpriteStackRect {
    int x1, y1, x2, y2;
};

// Calculates the screen bounding box of a SpriteStack
SpriteStackRect get_sprite_stack_rect(SpriteStack& stack) {
    if (stack.getLVGLObject() == nullptr) {
        return {0, 0, 0, 0}; // Return an empty rect if stack not created
    }
    Point pos = stack.getPosition();
    int w_base, h_base;
    stack.getDim(w_base, h_base);
    float zoom_factor = stack.getZoomPercent() / 100.0f;

    int current_w = (int)(w_base * zoom_factor);
    int current_h = (int)(h_base * zoom_factor);
    int half_w = current_w / 2;
    int half_h = current_h / 2;

    return {
        pos.x - half_w,
        pos.y - half_h,
        pos.x + half_w,
        pos.y + half_h
    };
}

// Checks if two SpriteStackRects overlap
bool do_rects_overlap(const SpriteStackRect& r1, const SpriteStackRect& r2) {
    // If one rectangle is on left side of other
    if (r1.x1 >= r2.x2 || r2.x1 >= r1.x2) {
        return false;
    }
    // If one rectangle is above other
    if (r1.y1 >= r2.y2 || r2.y1 >= r1.y2) {
        return false;
    }
    return true;
}
*/
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
        float dist_sq = dx * dx + dy * dy; // no need to use sqrt here cuz its slow just square  the dist below

        if (dist_sq < 4.0f) { // Threshold for arrival
            currentPos = internal_target_memory; 
            sprite_stack.setPosition(currentPos.x, currentPos.y);
            sprite_stack.setRotation(0, 0, 0); 
            moving_to_current_target = false; // Stop movement, target reached
            // internal_target_memory = {-1,-1}; // Optionally clear to signify completion for this specific target
            // Serial.println("moveSpriteToTarget: Target reached.");
            return true; 
        } else {
            // Continue moving
            float dist = sqrtf(dist_sq);
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

/*
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
*/
/*
int compareSpriteStacksByY(const void* a, const void* b) {
    SpriteStack* stackA = *(SpriteStack**)a;
    SpriteStack* stackB = *(SpriteStack**)b;
    int yA = stackA->getPosition().y;
    int yB = stackB->getPosition().y;
    if (yA < yB) return -1; 
    if (yA > yB) return 1;  
    return 0; 
}
*/
void updateSpriteStackZOrder() {
    static int idx = 0;
    const int n = NUM_SORTABLE_STACKS;
    // compare item idx and idx+1
    if (idx < n-1) {
        auto *A = sortable_stacks[idx];
        auto *B = sortable_stacks[idx+1];
        if (A->getPosition().y > B->getPosition().y) {
            // swap them
            sortable_stacks[idx]   = B;
            sortable_stacks[idx+1] = A;
            // only bring the two you swapped
            B->bringToForeground();
            A->bringToForeground();
        }
        idx++;
    } else {
        // once you’ve walked the whole array, start over
        idx = 0;
    }
}

// -------------------------
// INTERACTION LOGIC (MODIFIED)
// -------------------------
// Non-blocking per-loop movement step (no input checks here!)

void stepMovement() {
  unsigned long now = millis();

  // 1) Finish any post-user-target cooldown
  if (inPostUserTargetCooldown && (now - postUserTargetTime >= COOLDOWN_MS)) {
    inPostUserTargetCooldown = false;
    wanderingActive = false; // Ensure wandering is also reset after cooldown
    if (moveState != MoveState::ToUser) { // Avoid transitioning to Idle if a new user target was set during cooldown
        moveState = MoveState::Idle;
    }
    Serial.println("Cooldown finished.");
  }

  // --- MODIFICATION FOR REQUIREMENT 2 ---
  // Check for an active user destination. This can interrupt wandering or a previous user destination.
  if (hasUserDestination) {
    // If not already moving to the current user destination, or if we were idle/wandering
    if (moveState == MoveState::Idle || moveState == MoveState::ToWander ||
        (moveTarget.x != currentUserDestination.x || moveTarget.y != currentUserDestination.y)) {
      
      Serial.print("stepMovement: Adopting new/updated user destination X=");
      Serial.print(currentUserDestination.x); Serial.print(" Y="); Serial.println(currentUserDestination.y);
      
      moveTarget = currentUserDestination;
      moveState = MoveState::ToUser;
      // lastStepTime is not reset here, allowing movement to start/adjust on the next valid interval.
      // The long press handler already cleared inPostUserTargetCooldown and wanderingActive.
    }
  } 
  // --- END MODIFICATION ---
  
  // 2) If idle, no user destination pending, and not in cooldown, consider wandering
  else if (moveState == MoveState::Idle && !inPostUserTargetCooldown) { // Added !hasUserDestination implied by else
    if (!wanderingActive) {
      wanderingActive = true;
      lastWanderTime = now;
      // Serial.println("Wandering enabled, waiting for WANDER_INTERVAL.");
    } else if (now - lastWanderTime >= WANDER_INTERVAL) {
      int gw = lv_disp_get_hor_res(NULL);
      int gh = lv_disp_get_ver_res(NULL);
      moveTarget = random_point(40, gw - 40, gh / 2, gh - 40); // Pick a random point on the ground
      moveState = MoveState::ToWander;
      lastWanderTime = now; // Reset timer for the next wander decision
      Serial.print("Wandering to new random point: X="); Serial.print(moveTarget.x); Serial.print(" Y="); Serial.println(moveTarget.y);
    }
  }

  // 3) If we’re actively moving (to user target or wander point), perform a step
  if ((moveState == MoveState::ToUser || moveState == MoveState::ToWander) && (now - lastStepTime >= STEP_INTERVAL)) {
    lastStepTime = now;

    float dx = moveTarget.x - g_spritePosition.x;
    float dy = moveTarget.y - g_spritePosition.y;
    float dist = sqrtf(dx * dx + dy * dy);
    const float speed = 3; // Movement speed in pixels per step

    if (dist < speed) { // Arrived at the target
      g_spritePosition = moveTarget;
      myStack.setPosition(g_spritePosition.x, g_spritePosition.y);
      myStack.setRotation(0, 0, 0); // Face forward upon arrival

      Serial.print("Arrived at target: X="); Serial.print(moveTarget.x); Serial.print(" Y="); Serial.println(moveTarget.y);

      if (moveState == MoveState::ToUser) {
        hasUserDestination = false; // Clear the flag, as we've reached the user's destination
        inPostUserTargetCooldown = true; // Start cooldown period
        postUserTargetTime = now;
        Serial.println("User target reached. Cooldown initiated.");
      } else if (moveState == MoveState::ToWander) {
        // Just reached a wander point. No cooldown. Will decide next action (wander/idle) in next cycle.
         Serial.println("Wander point reached.");
      }
      moveState = MoveState::Idle; // Return to Idle to decide next action

    } else { // Still moving towards the target
      float move_step_x = (dx / dist) * speed;
      float move_step_y = (dy / dist) * speed;
      g_spritePosition.x += round(move_step_x);
      g_spritePosition.y += round(move_step_y);
      myStack.setPosition(g_spritePosition.x, g_spritePosition.y);

      // Update rotation to face the direction of movement
      float angle_rad = atan2(dy, dx);
      float roll_angle_deg = degrees(angle_rad) + 270; // Adjust for sprite orientation
      myStack.setRotation(0, 0, roll_angle_deg);

      // Update zoom based on Y position
      int groundY = lv_disp_get_ver_res(NULL) / 2;
      float base_zoom = myStack.getInitialZoomPercent();
      float zoom_multiplier = map(g_spritePosition.y, groundY, 200, base_zoom, base_zoom + 100.0f);
      myStack.setZoom(zoom_multiplier);
    }
  }
}


//--------------------------------------------------------------------------------
// Dump all immediate children of 'parent', printing only labels and their texts.
//--------------------------------------------------------------------------------
void dump_children_of(lv_obj_t* parent) {
    uint32_t cnt = lv_obj_get_child_cnt(parent);
    Serial.print(F("dump_children_of: parent=0x"));
    Serial.print((uintptr_t)parent, HEX);
    Serial.print(F("  child_count="));
    Serial.println(cnt);

    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(parent, i);
        Serial.print(F("  ["));
        Serial.print(i);
        Serial.print(F("] obj=0x"));
        Serial.print((uintptr_t)child, HEX);

        if (lv_obj_has_class(child, &lv_label_class)) {
            const char* txt = lv_label_get_text(child);
            Serial.print(F("  <Label> \""));
            Serial.print(txt ? txt : "");
            Serial.println(F("\""));
        }
        else {
            Serial.println(F("  <Other object>"));
        }
    }
    Serial.println();
}

static lv_obj_t * fps_label;
static lv_obj_t * mem_label;
static lv_style_t small_font_style;

// --- Add these to your global variables ---
static lv_obj_t *custom_fps_label;
static lv_obj_t *custom_mem_label;
static lv_style_t monitor_style;
static volatile uint32_t fps_counter = 0;

// --- Replace your sysmon_relocator_cb with this new function ---
static void custom_monitor_update_cb(lv_timer_t * timer) {
    // 1. Get stats from LVGL
    int32_t fps = fps_counter;
    fps_counter = 0;
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    uint32_t used_kb = (mon.total_size - mon.free_size) / 1024;

    // 2. Update the text of your labels
    lv_label_set_text_fmt(custom_fps_label, "%" LV_PRId32 " FPS", fps);
    lv_label_set_text_fmt(custom_mem_label, "%" LV_PRIu32 " kB", used_kb);

    // 3. Recalculate position and center the labels
    lv_coord_t pw = lv_obj_get_width(custom_fps_label);
    lv_coord_t ph = lv_obj_get_height(custom_fps_label);
    lv_coord_t mw = lv_obj_get_width(custom_mem_label);
    lv_coord_t mh = lv_obj_get_height(custom_mem_label);

    lv_coord_t screen_w = lv_disp_get_hor_res(nullptr);
    lv_coord_t screen_h = lv_disp_get_ver_res(nullptr);
    const lv_coord_t gap    = 10;
    const lv_coord_t bottom = 210;

    lv_coord_t total_w = mw + pw + gap;
    lv_coord_t start_x = (screen_w - total_w) / 2;
    lv_coord_t y       = screen_h - max(mh, ph) - bottom;

    // 4. Set the new positions
    lv_obj_set_pos(custom_mem_label, start_x, y);
    lv_obj_set_pos(custom_fps_label, start_x + mw + gap, y);
}

// ------------------------------------------------------
// TIMER CALLBACK
// This runs once every second (immediately after sysmon updates).
// ------------------------------------------------------
static void sysmon_relocator_cb(lv_timer_t * timer) {
  (void)timer; // unused

  // 1) Shrink both labels to Montserrat 8px
  lv_obj_add_style(fps_label, &small_font_style, LV_PART_MAIN);
  lv_obj_add_style(mem_label, &small_font_style, LV_PART_MAIN);

  // 2) Re-measure their sizes
  lv_coord_t pw = lv_obj_get_width(fps_label);
  lv_coord_t ph = lv_obj_get_height(fps_label);
  lv_coord_t mw = lv_obj_get_width(mem_label);
  lv_coord_t mh = lv_obj_get_height(mem_label);

  // 3) Compute bottom-center placement (10 px gap, 5 px margin)
  lv_coord_t screen_w = lv_disp_get_hor_res(nullptr);
  lv_coord_t screen_h = lv_disp_get_ver_res(nullptr);
  const lv_coord_t gap    = 10;
  const lv_coord_t bottom = 5;

  lv_coord_t total_w = mw + pw + gap;
  lv_coord_t start_x = (screen_w - total_w) / 2;
  lv_coord_t y       = screen_h - max(mh, ph) - bottom;

  // 4) Move MEM (left) then FPS (right)
  lv_obj_set_pos(mem_label,  start_x,         y);
  lv_obj_set_pos(fps_label, start_x + mw + gap, y);
}

// -------------------------
// SETUP & LOOP
// -------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  // 1) Initialize LVGL core and HALs
  lv_init();
  lv_xiao_disp_init();
  lv_xiao_touch_init();
  Wire.begin();
  rtc.begin();

  // 2) Create & load a main screen so layers exist
  mainScreen = lv_obj_create(nullptr);
  lv_obj_set_size(mainScreen,
                  lv_disp_get_hor_res(nullptr),
                  lv_disp_get_ver_res(nullptr));
  lv_scr_load(mainScreen);

  /*
  // 3) Let LVGL run two timer handler calls so built‐in monitors (if any) appear
  lv_timer_handler();
  lv_timer_handler();

  // 4) Dump the System Layer’s immediate children
  Serial.println(F("─── System Layer Immediate Children ───"));
  dump_children_of(lv_layer_sys());

  // 5) (Optional) Dump the Active Screen’s immediate children
  Serial.println(F("─── Active Screen Immediate Children ───"));
  dump_children_of(lv_scr_act());

  
  // 6) Now proceed with the rest of setup (we’ll locate & reposition the labels next)
  //    First: build a Montserrat 8 px style for shrinking.
  lv_style_init(&small_font_style);
  lv_style_set_text_font(&small_font_style, &lv_font_montserrat_8);

  // 6) Find the two sysmon labels in lv_layer_sys()
  {
    lv_obj_t* sys_layer = lv_layer_sys();
    uint32_t cnt = lv_obj_get_child_cnt(sys_layer);
    for (uint32_t i = 0; i < cnt; i++) {
      lv_obj_t* child = lv_obj_get_child(sys_layer, i);
      if (!lv_obj_has_class(child, &lv_label_class)) continue;
      const char* txt = lv_label_get_text(child);
      if      (txt && strstr(txt, "FPS"))  fps_label = child;
      else if (txt && (strstr(txt, "used") || strstr(txt, "kB"))) mem_label = child;
      if (fps_label && mem_label) break;
    }
    if (!fps_label || !mem_label) {
      Serial.println(F("Warning: Could not find one or both sysmon labels!"));
    }
  }

  // 7) If found, create a 1 s LVGL timer to re-style/re-position them each tick
  if (fps_label && mem_label) {
    lv_timer_t * t = lv_timer_create(sysmon_relocator_cb, 1000, nullptr);
    (void)t; // we don’t need to keep the handle
    Serial.println(F("Sysmon relocator timer created."));
  }
  */
  // 1. Create a persistent style for the labels
  lv_style_init(&monitor_style);
  lv_style_set_text_font(&monitor_style, &lv_font_montserrat_8);
  lv_style_set_text_color(&monitor_style, lv_color_white());
  lv_style_set_bg_color(&monitor_style, lv_color_black());
  lv_style_set_bg_opa(&monitor_style, LV_OPA_70);
  lv_style_set_pad_all(&monitor_style, 2);
  lv_style_set_radius(&monitor_style, 2);

  // 2. Create the labels on the system layer (to keep them on top)
  custom_mem_label = lv_label_create(lv_layer_sys());
  lv_obj_add_style(custom_mem_label, &monitor_style, 0);

  custom_fps_label = lv_label_create(lv_layer_sys());
  lv_obj_add_style(custom_fps_label, &monitor_style, 0);

  // 3. Create a single timer to update the text and position
  lv_timer_create(custom_monitor_update_cb, 1000, nullptr);

  // 4. Run the callback once to initialize text and position
  custom_monitor_update_cb(nullptr);
  
  Serial.println(F("Custom system monitors created."));

  myStack.create(mainScreen);
  myStack.setPosition(g_spritePosition.x, g_spritePosition.y);
  myStack.setZoom(200.0f);
  Serial.println("myStack (cat) created.");


  create_scene(mainScreen);

  randomSeed(analogRead(A0));
  Serial.println("Random seed set.");
  Serial.println("Setup complete. Starting loop...");
}

void loop() {
    fps_counter++;

    unsigned long t_loop_start, t_touch, t_anim, t_step, t_zorder, t_stack_update, t_lvgl, t_loop_end;
    static unsigned long last_log_time = 0;

    t_loop_start = millis();
    TouchEvent ev = get_touch_event();
    t_touch = millis();

    // dispatch it
    switch(ev.type) {
      case TouchEventType::TAP:
        Serial.println("tapped");
        if(is_stack_tapped(myStack, ev.x, ev.y)) {
          if(moveState == MoveState::Idle){
            start_anim(&SelectAnim);
          }
        }

        if(show_items){
          if(is_stack_tapped(burgerStack, ev.x, ev.y)) {
            start_anim(&burgerSelectAnim);
          }
          if(is_stack_tapped(bedStack, ev.x, ev.y)) {
            start_anim(&bedSelectAnim);
          }
          if(is_stack_tapped(ballStack, ev.x, ev.y)) {
            //auto* a = SelectionAnimation(myStack, 0, 360, 3000, 0);
            // drop it if the queue is already full
            start_anim(&ballSelectAnim);
          }
        }
        break;
      case TouchEventType::LONG_PRESS_BEGAN:
        Serial.println("pressed");
        if(is_stack_tapped(myStack, ev.x, ev.y)) {
          Serial.println("my stack is pressed");
          if(!show_items){
            burgerStack.create(mainScreen);
            burgerStack.setPosition(burgerPosition.x, burgerPosition.y);
            burgerStack.setZoom(100.0f);
            Serial.println("burgerStack created.");

            bedStack.create(mainScreen);
            bedStack.setPosition(bedPosition.x, bedPosition.y);
            bedStack.setZoom(200.0f);
            Serial.println("bedStack created.");

            ballStack.create(mainScreen);
            ballStack.setPosition(ballPosition.x, ballPosition.y);
            ballStack.setZoom(100.0f);
            Serial.println("ballStack created.");

            show_items = true;
          }else if(show_items){
            bedStack.destroy();
            burgerStack.destroy();
            ballStack.destroy();
            Serial.println("menu items destroyed.");
            show_items = false;
          }
        }
        
        if(ev.y >= lv_disp_get_ver_res(NULL)/2){
          currentUserDestination     = {ev.x, ev.y};
          hasUserDestination         = true;
          inPostUserTargetCooldown   = false;
          wanderingActive            = false;
          Serial.println("User dest declared");
        }
        
        break;
      case TouchEventType::SWIPE_LEFT:
        Serial.println("swipe left");
        if(is_stack_tapped(myStack, ev.x, ev.y) && moveState == MoveState::Idle) {
          start_anim(&turnLeft);
        }
        break;
      case TouchEventType::SWIPE_RIGHT:
        Serial.println("swipe right");
        if(is_stack_tapped(myStack, ev.x, ev.y) && moveState == MoveState::Idle) {
          start_anim(&turnRight);
        }
        break;

      default:
        break;
    }


    update_background(rtc);  // move sun/moon, swap day/night, etc.

    driveAnimations();
    t_anim = millis();

    stepMovement();
    t_step = millis();

    // 6. Update Sprite Stack
    myStack.update(); // Update the visual state of myStack    
    if(show_items){
      bedStack.update();
      burgerStack.update();
      ballStack.update();
    }
    t_stack_update = millis();

    // this has some blocking issue
    updateSpriteStackZOrder();
    t_zorder = millis();

    lv_task_handler();
    t_lvgl = millis();

    // Log durations periodically, e.g., every 1-2 seconds
    
    if (millis() - last_log_time > 2000) {
        Serial.println("---- Loop Timing (ms) ----");
        Serial.print("GetTouch: "); Serial.println(t_touch - t_loop_start);
        Serial.print("DriveAnim: "); Serial.println(t_anim - t_touch);
        Serial.print("StepMovement: "); Serial.println(t_step - t_anim); // This should be small
        Serial.print("StackUpdates: "); Serial.println(t_stack_update - t_step); // Suspect this could be large
        Serial.print("Z reordering: "); Serial.println(t_zorder - t_stack_update); // Suspect this could be large
        Serial.print("LVGL_Task: "); Serial.println(t_lvgl - t_zorder);    // And/or this
        Serial.println("LoopDelay: 2"); // The fixed delay(2)
        Serial.print("Actual Loop Total: "); Serial.println(millis() - t_loop_start);
        Serial.println("--------------------------");
        last_log_time = millis();
    }
    

    delay(2); // Adjust for desired loop rate
}


  