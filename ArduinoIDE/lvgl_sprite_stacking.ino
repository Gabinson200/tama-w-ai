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

// --- Global Variables for Touch and Gestures ---
TouchInfo currentGlobalTouch;
TapGestureRecognizer myTapRecognizer(500, 48, 20); // Max Duration 150ms, Max Move 15px, Confirmation Delay 80ms - Adjust as needed
LongPressGestureRecognizer myLongPressRecognizer(1000, 240); // Min Duration 700ms, Max Move 20px - Adjust as needed
swipe_tracker_t mySwipeTracker;

// --- For Interrupt Animation on myStack ---
bool myStackIsPerformingInterruptAnim = false;    
bool wasMovingToUserDestBeforeInterrupt = false;  
Point userDestBeforeInterrupt = {0,0};          
static bool myStack_tap_handled = false; // Flag to debounce taps on myStack for item toggle
bool show_items = false;

// Animation object for myStack
SpriteStackAnimation* currentActiveAnimation = nullptr; // Pointer to the currently active animation


int animIndex = 0; // Index for cycling through interrupt animations

int catFrameCount = sizeof(cat_images) / sizeof(cat_images[0]);
int frogFrameCount = sizeof(frog_images) / sizeof(frog_images[0]);
int burgerFrameCount = sizeof(burger_images) / sizeof(burger_images[0]);
int bedFrameCount = sizeof(bed_images) / sizeof(bed_images[0]);
int ballFrameCount = sizeof(ball_images) / sizeof(ball_images[0]);

SpriteStack myStack(cat_images, catFrameCount, 0, 1.0, 1.0, 200.0f);
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


void reset_walk_to_random_point_state(); // Forward declaration

// Animation objects for myStack (used for interrupt)
RotationAnimation MyRotationAnim(myStack, 0, 360, 3000, 0); // No delay for interrupt
NoNoAnimation NoNoAnim(myStack, -25, 25, 1500, 0);
NodAnimation NodAnim(myStack, -10, 0, 3000, 0);
DanceAnimation DanceAnim(myStack, -45, 45, 3000, 0);
DeselectionAnimation DeseAnim(myStack, -15, 15, 3000, 0);
SelectionAnimation SelectAnim(myStack, 0, 360, 3000, 0);

SelectionAnimation burgerSelectAnim(burgerStack, 0, 360, 3000, 0);

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

bool is_tap_on_sprite(SpriteStack& sprite, lv_coord_t tap_x, lv_coord_t tap_y) {
    if (sprite.getLVGLObject() == nullptr) return false;

    Point spritePos = sprite.getPosition();
    int w, h;
    sprite.getDim(w,h);
    float zoomFactor = sprite.getZoomPercent() / 100.0f;
    int displayW = w * zoomFactor;
    int displayH = h * zoomFactor;

    int hitbox_half_width = displayW / 2;
    int hitbox_half_height = displayH / 2;

    //bool hit = (tap_x >= spritePos.x - hitbox_half_width && tap_x <= spritePos.x + hitbox_half_width &&
    //            tap_y >= spritePos.y - hitbox_half_height && tap_y <= spritePos.y + hitbox_half_height);

    bool hit = is_within_square_bounds_center(tap_x, tap_y, spritePos.x, spritePos.y, hitbox_half_width, hitbox_half_height);
    return hit;
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

/*
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
                if (millis() - touch_hold_start_time >= USER_DESTINATION_HOLD_DURATION && touch_is_being_held) { 
                    Serial.print("New user destination set for myStack: X="); Serial.print(touchX); Serial.print(", Y="); Serial.println(touchY);
                    currentUserDestination.x = touchX;
                    currentUserDestination.y = touchY;
                    hasUserDestination = true;        
                    inPostUserTargetCooldown = false; 
                    touch_is_being_held = false;
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
*/

/*
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
*/

/*
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
*/

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
  //lv_indev_t *indev = lv_indev_get_next(nullptr);
  //lv_indev_enable(indev, false);

  //lv_indev_t* indev = lv_indev_get_next(NULL);
  //lv_indev_disable(indev);

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
  myStack.setZoom(200.0f);
  Serial.println("myStack (cat) created.");

  // Burger and Bed are initially not created, they will be created when show_items becomes true.
  // Remove their creation from setup:
  /*
  burgerStack.create(mainScreen);
  burgerStack.setPosition(burgerPosition.x, burgerPosition.y);
  burgerStack.setZoom(100.0f);
  Serial.println("burgerStack created.");

  bedStack.create(mainScreen);
  bedStack.setPosition(bedPosition.x, bedPosition.y);
  bedStack.setZoom(200.0f);
  Serial.println("bedStack created.");
  */

  // --- Initialize Swipe Tracker ---
  mySwipeTracker.state = SWIPE_IDLE;
  mySwipeTracker.swipeDetected = false;

  create_scene(mainScreen);

  randomSeed(analogRead(A0));
  Serial.println("Random seed set.");
  Serial.println("Setup complete. Starting loop...");
}

void loop() {
    // 1. Get debounced touch information
    update_global_touch_info(&currentGlobalTouch);

    // 2. Update LongPress and Swipe recognizers FIRST
    myLongPressRecognizer.update(currentGlobalTouch);
    update_swipe_state(0, 239, 0, 239, 60, &mySwipeTracker, currentGlobalTouch); // Params: x_min, x_max, y_min, y_max, min_swipe_length

    // 3. Notify TapRecognizer if the current press has been claimed by an ongoing LongPress/Swipe
    if (currentGlobalTouch.is_pressed) {
        if (myLongPressRecognizer.get_state() == GestureState::BEGAN || myLongPressRecognizer.get_state() == GestureState::FAILED ||
            mySwipeTracker.state == SWIPE_DRAGGING) {
            myTapRecognizer.notify_current_press_is_claimed();
        }
    }
    // TapRecognizer's internal 'ongoing_press_is_claimed_by_other' flag is now managed by its own update/reset logic combined with this notification.

    // 4. Update Tap recognizer
    myTapRecognizer.update(currentGlobalTouch);

    // 5. Handle Gesture Completion Conflicts
    if (myTapRecognizer.is_waiting_for_confirmation()) {
        bool cancel_tap_due_to_conflict = false;
        if (mySwipeTracker.swipeDetected) {
            Serial.println("Loop: Swipe completed, tap was pending. Cancelling tap."); // Debug
            cancel_tap_due_to_conflict = true;
        }
        if (myLongPressRecognizer.get_state() == GestureState::ENDED) {
            Serial.println("Loop: Long press ended, tap was pending. Cancelling tap."); // Debug
            cancel_tap_due_to_conflict = true;
        }

        if (cancel_tap_due_to_conflict && !currentGlobalTouch.is_pressed) { // Ensure it was a release
            myTapRecognizer.cancel();
        }
    }

    // 6. React to Gesture States
    GestureState tapState = myTapRecognizer.get_state();
    GestureState longPressState = myLongPressRecognizer.get_state();
    static GestureState prevTapState = GestureState::IDLE;
    static GestureState prevLongPressState = GestureState::IDLE;
    static swipe_state_t prevSwipeState = SWIPE_IDLE; // For logging swipe start/end if needed

    if (tapState == GestureState::ENDED) {
        lv_coord_t tap_x = myTapRecognizer.get_tap_x(); // Get X coordinate of the tap
        lv_coord_t tap_y = myTapRecognizer.get_tap_y(); // Get Y coordinate of the tap
        Serial.println("--------------------------------");
        Serial.print("EVENT: Tap Ended at X: "); Serial.print(tap_x);
        Serial.print(", Y: "); Serial.println(tap_y);
        /*
        if (currentActiveAnimation == nullptr || !currentActiveAnimation->isActive()) { // Check if no animation is currently active
            if (is_tap_on_sprite(myStack, tap_x, tap_y)) {
                Serial.println("Tap on myStack detected! Starting Dance Animation.");
                currentActiveAnimation = &DanceAnim; // Set DanceAnim as the current animation
                currentActiveAnimation->start(); // Start the animation
            } else {
                Serial.println("Tap was not on myStack.");
            }
        } else {
            Serial.println("Tap ignored, an animation is already active.");
        }
        Serial.println("--------------------------------");
        myTapRecognizer.reset(); // Reset recognizer for next tap
        */

    }

    if (longPressState != prevLongPressState) {
        if (longPressState == GestureState::BEGAN) {
            Serial.println("--------------------------------");
            Serial.print("EVENT: Long Press BEGAN at X:"); Serial.print(myLongPressRecognizer.recognized_at_x);
            Serial.print(" Y:"); Serial.println(myLongPressRecognizer.recognized_at_y);
            Serial.println("--------------------------------");
        } else if (longPressState == GestureState::ENDED) {
            Serial.println("--------------------------------");
            Serial.println("EVENT: Long Press ENDED");
            // For Long Press end, currentGlobalTouch reflects the release point if needed for other logic
            Serial.print("  Touch Release X: "); Serial.print(currentGlobalTouch.x);
            Serial.print(", Y: "); Serial.println(currentGlobalTouch.y);
            Serial.println("--------------------------------");
        } else if (longPressState == GestureState::FAILED) {
            Serial.println("--------------------------------");
            Serial.println("EVENT: Long Press FAILED");
            Serial.println("--------------------------------");
        }
    }

    if (mySwipeTracker.swipeDetected) {
        Serial.println("--------------------------------");
        Serial.print("EVENT: Swipe Detected: ");
        switch (mySwipeTracker.swipeDir) {
            case SWIPE_DIR_LEFT: Serial.println("LEFT"); break;
            case SWIPE_DIR_RIGHT: Serial.println("RIGHT"); break;
            case SWIPE_DIR_UP: Serial.println("UP"); break;
            case SWIPE_DIR_DOWN: Serial.println("DOWN"); break;
            default: Serial.println("NONE?"); break;
        }
        Serial.print("  From ("); Serial.print(mySwipeTracker.startX); Serial.print(","); Serial.print(mySwipeTracker.startY);
        Serial.print(") to ("); Serial.print(mySwipeTracker.lastGoodX);Serial.print(","); Serial.print(mySwipeTracker.lastGoodY); Serial.println(")");
        Serial.println("--------------------------------");
    }

    // Update previous states for next cycle's change detection in logging
    prevTapState = tapState;
    prevLongPressState = longPressState;
    prevSwipeState = mySwipeTracker.state; // If you want to log swipe drag start etc.

    update_background(rtc);  // move sun/moon, swap day/night, etc.

    // 5. Update active animation
    if (currentActiveAnimation != nullptr && currentActiveAnimation->isActive()) { // Check if an animation is active
        currentActiveAnimation->update(); // Update the animation's state
        if (!currentActiveAnimation->isActive()) { // Check if animation just finished
            Serial.println("Animation finished.");
            currentActiveAnimation = nullptr; // Clear the active animation pointer
        }
    }

    test_anims(myStack, myTapRecognizer);

    // 6. Update Sprite Stack
    myStack.update(); // Update the visual state of myStack

    lv_task_handler();
    delay(2); // Adjust for desired loop rate
}


  /*
  // Explicitly call validate_touch to ensure consistent polling of the touch controller.
  // This helps stabilize the raw touch readings used by the hold logic and other touch checks.
  lv_coord_t dummyX, dummyY;
  // The return value and coordinates are not directly used here, but the call ensures
  // the underlying chsc6x_is_pressed() and chsc6x_get_xy() are executed regularly,
  // which is crucial for the debounced touch state used by the hold logic.
  validate_touch(&dummyX, &dummyY);

  render_scene(mainScreen);

  handle_interrupt_animation_state(); // Check for interrupt animation completion

  test_anims();

  // Movement logic for myStack (includes the refined hold detection)
  test_user_and_random_walk(myStack, g_spritePosition);


    // --- Non-blocking logic to toggle items on tap of myStack ---
    // Use get_touch_in_area_center which is non-blocking
    Point myStackPos = myStack.getPosition();
    bool isTouched_on_myStack_for_toggle = get_touch_in_area_center(
        myStackPos.x - 10, myStackPos.y - 20,
        myStack.getZoomPercent() * 0.08,
        myStack.getZoomPercent() * 0.12,
        true // View the area for debugging
    );

    if (isTouched_on_myStack_for_toggle && !myStack_tap_handled) {
        // Only trigger on the rising edge of the touch (when it first becomes true)
        Serial.println("myStack tapped for item toggle!");
        myStack_tap_handled = true; // Mark as handled

        if (show_items == false) {
            // Create items
            burgerStack.create(mainScreen);
            burgerStack.setPosition(burgerPosition.x, burgerPosition.y);
            burgerStack.setZoom(100.0f);
            Serial.println("burgerStack created.");

            bedStack.create(mainScreen);
            bedStack.setPosition(bedPosition.x, bedPosition.y);
            bedStack.setZoom(200.0f);
            Serial.println("bedStack created.");

            show_items = true;
        } else {
            // Destroy items
            bedStack.destroy();
            burgerStack.destroy();
            show_items = false;
        }
    } else if (!isTouched_on_myStack_for_toggle) {
        // Reset the handled flag when touch is released
        myStack_tap_handled = false;
    }


  // Touch handling for burger (if applicable and uses get_touch_in_area_center)
  // This remains as is. get_touch_in_area_center is non-blocking.
  if(show_items && get_touch_in_area_center(burgerPosition.x, burgerPosition.y, 20, 20, true)){
    Serial.println("burger touched");
    burgerSelectAnim.start();
  }


  // Update sprite stacks regardless of touch
  myStack.update();
    // Only update burger and bed if they exist (show_items is true)
    if (show_items) {
        if (burgerSelectAnim.isActive())     burgerSelectAnim.update();
        burgerStack.update();
        bedStack.update();
    }


  updateSpriteStackZOrder(); // This should happen after position updates and before lv_task_handler
  
  lv_task_handler(); // This is crucial for LVGL to process updates and redraw

  delay(10); // Keep a small delay to yield time
  */
//}