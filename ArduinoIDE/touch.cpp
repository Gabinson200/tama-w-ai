#include "touch.h"
#include "lv_xiao_round_screen.h" // For chsc6x_is_pressed, chsc6x_get_xy etc.
// Arduino.h and math.h are included via touch.h if needed by its own content

static const unsigned long DEBOUNCE_TOUCH_PRESS_MS   = 20;
static const unsigned long DEBOUNCE_TOUCH_RELEASE_MS = 20;
static const lv_coord_t   MAX_JUMP                   = 30;  // tune as needed

// persisted between calls
static bool current_debounced_pressed_state = false;
static bool last_raw_touch_state_for_debounce = false;
static unsigned long last_state_change_time_for_debounce = 0;

// for jump‐filtering
static lv_coord_t last_known_x_for_release = 0;
static lv_coord_t last_known_y_for_release = 0;
static bool       have_last_good = false;
static bool       last_debounced = false;


void update_global_touch_info(TouchInfo* info) {
  // 1) read raw
  bool raw = chsc6x_is_pressed();
  unsigned long now = millis();

  // 2) debounce
  if (raw != last_raw_touch_state_for_debounce) {
    last_state_change_time_for_debounce = now;
    last_raw_touch_state_for_debounce = raw;
  }
  if (raw) {
    if (!current_debounced_pressed_state &&
        (now - last_state_change_time_for_debounce) >= DEBOUNCE_TOUCH_PRESS_MS) {
      current_debounced_pressed_state = true;
    }
  } else {
    if (current_debounced_pressed_state &&
        (now - last_state_change_time_for_debounce) >= DEBOUNCE_TOUCH_RELEASE_MS) {
      current_debounced_pressed_state = false;
    }
  }

  info->is_pressed = current_debounced_pressed_state;
  info->timestamp  = now;

  // 3) seed jump‐filter on the *first* debounced press
  if (!last_debounced && current_debounced_pressed_state) {
    lv_coord_t tx, ty;
    chsc6x_get_xy(&tx, &ty);
    last_known_x_for_release = constrain(tx, 0, 239);
    last_known_y_for_release = constrain(ty, 0, 239);
    have_last_good = true;
  }
  last_debounced = current_debounced_pressed_state;

  // 4) while pressed, sample raw & filter jumps
  if (current_debounced_pressed_state) {
    lv_coord_t tx, ty;
    chsc6x_get_xy(&tx, &ty);
    tx = constrain(tx, 0, 239);
    ty = constrain(ty, 0, 239);

    if (have_last_good &&
        abs(tx - last_known_x_for_release) < MAX_JUMP &&
        abs(ty - last_known_y_for_release) < MAX_JUMP) {
      last_known_x_for_release = tx;
      last_known_y_for_release = ty;
    }
    info->x = last_known_x_for_release;
    info->y = last_known_y_for_release;
  }
  else {
    // 5) on release, hold onto last good and reset for next press
    info->x = last_known_x_for_release;
    info->y = last_known_y_for_release;
    have_last_good = false;
  }
}


static lv_obj_t *last_debug_shape = NULL;
// Boundary Checks and draw_area (copied from previous correct version, no changes)
bool is_within_square_bounds(int x, int y, int x_min, int x_max, int y_min, int y_max) {
    return (x >= x_min && x <= x_max && y >= y_min && y <= y_max);
}
bool is_within_square_bounds_center(int x, int y, int center_x, int center_y, int half_width, int half_height) {
    return is_within_square_bounds(x, y, center_x - half_width, center_x + half_width, center_y - half_height, center_y + half_height);
}
bool is_within_circle_bounds(int x, int y, int center_x, int center_y, int radius) {
    long dx = x - center_x; long dy = y - center_y; return (sqrt(dx * dx + dy * dy)) <= ((long)radius * radius);
}
void draw_area(lv_area_t area, bool is_circle, bool clear_previous) {
    if (clear_previous && last_debug_shape != NULL) { lv_obj_del(last_debug_shape); last_debug_shape = NULL; }
    lv_obj_t* shape = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(shape, area.x1, area.y1);
    lv_obj_set_style_bg_opa(shape, LV_OPA_50, 0); lv_obj_set_style_border_width(shape, 0, 0);
    if (is_circle) {
        int width = area.x2 - area.x1; int height = area.y2 - area.y1; int radius = (width < height ? width : height) / 2;
        lv_obj_set_size(shape, radius * 2, radius * 2); lv_obj_align(shape, LV_ALIGN_TOP_LEFT, area.x1 + (width - radius*2)/2, area.y1 + (height - radius*2)/2);
        lv_obj_set_style_radius(shape, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_color(shape, lv_color_hex(0x0000FF), 0);
    } else {
        lv_obj_set_size(shape, area.x2 - area.x1, area.y2 - area.y1); lv_obj_set_style_radius(shape, 0, 0); lv_obj_set_style_bg_color(shape, lv_color_hex(0x00FF00), 0);
    }
    last_debug_shape = shape;
}


const char* gestureStateToString(GestureState state) {
    switch (state) {
        case GestureState::IDLE: return "IDLE"; case GestureState::POSSIBLE: return "POSSIBLE";
        case GestureState::BEGAN: return "BEGAN"; case GestureState::ENDED: return "ENDED";
        case GestureState::FAILED: return "FAILED"; case GestureState::CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

BaseGestureRecognizer::BaseGestureRecognizer()
    : current_state(GestureState::IDLE), enabled(true), user_data(nullptr) {
    target_area = {0, 0, 239, 239}; start_touch_info = {false, 0, 0, 0};
}
void BaseGestureRecognizer::reset() { current_state = GestureState::IDLE; start_touch_info = {false, 0, 0, 0};}
void BaseGestureRecognizer::cancel() {
    if (current_state != GestureState::IDLE && current_state != GestureState::ENDED && current_state != GestureState::FAILED && current_state != GestureState::CANCELLED) {
         current_state = GestureState::CANCELLED;
    }
}
GestureState BaseGestureRecognizer::get_state() const { return current_state; }
void BaseGestureRecognizer::set_target_area(lv_area_t area) { target_area = area; }
lv_area_t BaseGestureRecognizer::get_target_area() const { return target_area; }
void BaseGestureRecognizer::set_enabled(bool enable) {
    enabled = enable; if (!enabled && (current_state != GestureState::IDLE)) { reset(); }
}
bool BaseGestureRecognizer::is_enabled() const { return enabled; }
bool BaseGestureRecognizer::is_within_target_area(lv_coord_t x, lv_coord_t y) const {
    return (x >= target_area.x1 && x <= target_area.x2 && y >= target_area.y1 && y <= target_area.y2);
}

// --- TapGestureRecognizer Implementation ---
TapGestureRecognizer::TapGestureRecognizer(unsigned long max_dur_ms, lv_coord_t max_move, unsigned long conf_delay_ms)
    : BaseGestureRecognizer(), max_duration_ms(max_dur_ms), max_movement_pixels(max_move),
      waiting_for_tap_confirmation(false), tap_potential_release_timestamp(0),
      tap_confirmation_delay_ms(conf_delay_ms), ongoing_press_is_claimed_by_other(false),
      tap_x(0), tap_y(0) {} // [MODIFIED] Initialize new members

void TapGestureRecognizer::notify_current_press_is_claimed() {
    // This flag is set if another gesture (LP BEGAN, Swipe DRAGGING) starts
    // during the current physical press.
    // It's sticky and only cleared by TapGestureRecognizer::reset().
    ongoing_press_is_claimed_by_other = true;
    // Serial.println("Tap: notified press is claimed by other."); // Debug
}

bool TapGestureRecognizer::is_waiting_for_confirmation() const {
    return waiting_for_tap_confirmation;
}

void TapGestureRecognizer::reset() {
    BaseGestureRecognizer::reset();
    waiting_for_tap_confirmation = false;
    ongoing_press_is_claimed_by_other = false; // [MODIFIED] Crucial reset point
    tap_x = 0; // Reset release coordinates
    tap_y = 0;
    // Serial.println("Tap: RESET called, ongoing_press_is_claimed_by_other is false."); // Debug
}

void TapGestureRecognizer::update(const TouchInfo& current_touch_info) {
    if (!enabled) {
        if (waiting_for_tap_confirmation) { waiting_for_tap_confirmation = false; }
        if (current_state != GestureState::IDLE) { reset(); }
        return;
    }

    // [MODIFIED] If finger is up, any "claim" on a previous press session is now void for this recognizer.
    // This ensures the flag is false before evaluating a new potential press,
    // or if it was IDLE and a release just happened.
    if (!current_touch_info.is_pressed) {
        ongoing_press_is_claimed_by_other = false;
    }

    if (current_state == GestureState::ENDED || current_state == GestureState::FAILED || current_state == GestureState::CANCELLED) {
        reset(); // This also clears ongoing_press_is_claimed_by_other
    }

    if (waiting_for_tap_confirmation) {
        if (current_touch_info.is_pressed && current_touch_info.timestamp >= tap_potential_release_timestamp) {
            // Serial.println("Tap Confirmed ABORTED: New press."); // Debug
            waiting_for_tap_confirmation = false;
            current_state = GestureState::FAILED;
        } else if (!current_touch_info.is_pressed) { // Still released
            if (millis() - tap_potential_release_timestamp >= tap_confirmation_delay_ms) {
                //Serial.println("Tap CONFIRMED after delay."); // Debug
                current_state = GestureState::ENDED;
                waiting_for_tap_confirmation = false;
                // tap_x,y are already set
            } else {
                return; // Still waiting for confirmation delay
            }
        } else {
            waiting_for_tap_confirmation = false;
            current_state = GestureState::FAILED;
        }
    }

    if (!waiting_for_tap_confirmation) {
        switch (current_state) {
            case GestureState::IDLE:
                if (current_touch_info.is_pressed) {
                    if (ongoing_press_is_claimed_by_other) {
                        //Serial.println("Tap IDLE: Press ongoing but was claimed. Ignoring."); // Debug
                    } else if (is_within_target_area(current_touch_info.x, current_touch_info.y)) {
                        //Serial.println("Tap IDLE: New press, becoming POSSIBLE."); // Debug
                        start_touch_info = current_touch_info;
                        current_state = GestureState::POSSIBLE;
                        //tap_x = current_touch_info.x; // [MODIFIED] Store release coordinates
                        //tap_y = current_touch_info.y; // [MODIFIED]
                        // ongoing_press_is_claimed_by_other is false here, correct for a new potential tap
                    }
                }
                break;

            case GestureState::POSSIBLE:
                if (!current_touch_info.is_pressed) { // Released
                    unsigned long current_duration = current_touch_info.timestamp - start_touch_info.timestamp;
                    lv_coord_t dx = current_touch_info.x - start_touch_info.x;
                    lv_coord_t dy = current_touch_info.y - start_touch_info.y;
                    long temp_dx = dx; long temp_dy = dy;
                    unsigned long dist_sq = sqrt(temp_dx * temp_dx + temp_dy * temp_dy); // Corrected
                    unsigned long max_move_sq = (unsigned long)max_movement_pixels;

                    if (current_duration <= max_duration_ms && dist_sq <= max_move_sq) {
                        waiting_for_tap_confirmation = true;
                        tap_potential_release_timestamp = current_touch_info.timestamp;
                        tap_x = current_touch_info.x; // [MODIFIED] Store release coordinates
                        tap_y = current_touch_info.y; // [MODIFIED]
                        //Serial.println("Tap POSSIBLE: Release OK, waiting confirmation.");// Debug
                    } else {
                        current_state = GestureState::FAILED;
                        //Serial.println("Tap POSSIBLE: Release FAILED criteria.");// Debug
                    }
                } else { // Still pressed
                    unsigned long current_duration = current_touch_info.timestamp - start_touch_info.timestamp;
                    lv_coord_t dx = current_touch_info.x - start_touch_info.x;
                    lv_coord_t dy = current_touch_info.y - start_touch_info.y;
                    long temp_dx = dx; long temp_dy = dy;
                    unsigned long dist_sq = sqrt(temp_dx * temp_dx + temp_dy * temp_dy); // Corrected
                    unsigned long max_move_sq = (unsigned long)max_movement_pixels;

                    if (current_duration > max_duration_ms || dist_sq > max_move_sq) {
                        current_state = GestureState::FAILED;
                        //Serial.println("Tap POSSIBLE: Pressed FAILED criteria.");// Debug
                    } else if (!is_within_target_area(current_touch_info.x, current_touch_info.y)) {
                        current_state = GestureState::FAILED;
                        //Serial.println("Tap POSSIBLE: Moved out of target area.");// Debug
                    }
                }
                break;
            default: // ENDED, FAILED, CANCELLED are handled by auto-reset
                break;
        }
    }
}

void TapGestureRecognizer::set_config(unsigned long max_dur_ms, lv_coord_t max_move, unsigned long conf_delay_ms) {
    max_duration_ms = max_dur_ms; max_movement_pixels = max_move;
    tap_confirmation_delay_ms = conf_delay_ms; reset();
}

// --- LongPressGestureRecognizer Implementation ---
LongPressGestureRecognizer::LongPressGestureRecognizer(unsigned long min_dur_ms, lv_coord_t max_move)
    : BaseGestureRecognizer(), min_duration_ms(min_dur_ms), max_movement_pixels(max_move),
      recognized_at_x(0), recognized_at_y(0) {}

void LongPressGestureRecognizer::update(const TouchInfo& current_touch_info) {
    if (!enabled) { if (current_state != GestureState::IDLE) reset(); return; }

    if (current_state == GestureState::ENDED || current_state == GestureState::FAILED || current_state == GestureState::CANCELLED) {
        reset();
    }

    switch (current_state) {
        case GestureState::IDLE:
            if (current_touch_info.is_pressed && is_within_target_area(current_touch_info.x, current_touch_info.y)) {
                start_touch_info = current_touch_info; current_state = GestureState::POSSIBLE;
                recognized_at_x = 0; recognized_at_y = 0;
            }
            break;
        case GestureState::POSSIBLE:
            if (!current_touch_info.is_pressed) { current_state = GestureState::FAILED; }
            else {
                unsigned long duration = current_touch_info.timestamp - start_touch_info.timestamp;
                lv_coord_t dx = current_touch_info.x - start_touch_info.x;
                lv_coord_t dy = current_touch_info.y - start_touch_info.y;
                long temp_dx = dx; long temp_dy = dy; // Corrected dist_sq
                unsigned long dist_sq = sqrt(temp_dx * temp_dx + temp_dy * temp_dy);
                unsigned long max_move_sq = (unsigned long)max_movement_pixels;

                if (dist_sq > max_move_sq || !is_within_target_area(current_touch_info.x, current_touch_info.y)) {
                    current_state = GestureState::FAILED;
                } else if (duration >= min_duration_ms) {
                    current_state = GestureState::BEGAN;
                    recognized_at_x = start_touch_info.x; recognized_at_y = start_touch_info.y;
                }
            }
            break;
        case GestureState::BEGAN:
            if (!current_touch_info.is_pressed) { current_state = GestureState::ENDED; }
            else {
                lv_coord_t dx = current_touch_info.x - start_touch_info.x; // Movement from original start
                lv_coord_t dy = current_touch_info.y - start_touch_info.y;
                long temp_dx = dx; long temp_dy = dy; // Corrected dist_sq
                unsigned long dist_sq = sqrt(temp_dx * temp_dx + temp_dy * temp_dy);
                unsigned long max_move_sq = (unsigned long)max_movement_pixels;

                if (dist_sq > max_move_sq || !is_within_target_area(current_touch_info.x, current_touch_info.y)) {
                    current_state = GestureState::FAILED;
                }
            }
            break;
        default: break;
    }
}


void LongPressGestureRecognizer::reset() { BaseGestureRecognizer::reset(); recognized_at_x = 0; recognized_at_y = 0; }
void LongPressGestureRecognizer::set_config(unsigned long min_dur_ms, lv_coord_t max_move) {
    min_duration_ms = min_dur_ms; max_movement_pixels = max_move; reset();
}

// Swipe functions (copied from previous correct version, no changes)
static swipe_dir_t compute_swipe_dir(lv_coord_t sX, lv_coord_t sY, lv_coord_t eX, lv_coord_t eY, int min_len) {
    int dx = eX - sX; int dy = eY - sY; int absX = abs(dx); int absY = abs(dy);
    if (absX < min_len && absY < min_len) return SWIPE_DIR_NONE;
    if (absX > absY) return (dx < 0) ? SWIPE_DIR_LEFT : SWIPE_DIR_RIGHT;
    return (dy < 0) ? SWIPE_DIR_UP : SWIPE_DIR_DOWN;
}
static const int MAX_SWIPE_JUMP = 80;
void update_swipe_state(int x_min, int x_max, int y_min, int y_max, int min_swipe_length, swipe_tracker_t *tracker, const TouchInfo& current_touch_info) {
    tracker->swipeDetected = false; bool is_touch_active = current_touch_info.is_pressed;
    lv_coord_t touch_x = current_touch_info.x; lv_coord_t touch_y = current_touch_info.y;
    static uint8_t swipe_release_debounce_counter = 0;
    switch (tracker->state) {
    case SWIPE_IDLE: swipe_release_debounce_counter = 0;
        if (is_touch_active && is_within_square_bounds(touch_x, touch_y, x_min, x_max, y_min, y_max)) {
            tracker->startX = touch_x; tracker->startY = touch_y; tracker->currentX = touch_x; tracker->currentY = touch_y;
            tracker->lastGoodX = touch_x; tracker->lastGoodY = touch_y; tracker->state = SWIPE_PRESSED;
        } break;
    case SWIPE_PRESSED: swipe_release_debounce_counter = 0;
        if (is_touch_active) {
            tracker->currentX = touch_x; tracker->currentY = touch_y; tracker->lastGoodX = touch_x; tracker->lastGoodY = touch_y;
            tracker->state = SWIPE_DRAGGING;
        } else { tracker->state = SWIPE_IDLE; } break;
    case SWIPE_DRAGGING:
        if (is_touch_active) { swipe_release_debounce_counter = 0;
            int jumpX = abs(touch_x - tracker->lastGoodX); int jumpY = abs(touch_y - tracker->lastGoodY);
            if (jumpX > MAX_SWIPE_JUMP || jumpY > MAX_SWIPE_JUMP) { tracker->currentX = tracker->lastGoodX; tracker->currentY = tracker->lastGoodY; }
            else { tracker->currentX = touch_x; tracker->currentY = touch_y; tracker->lastGoodX = touch_x; tracker->lastGoodY = touch_y; }
        } else { swipe_release_debounce_counter++;
            if (swipe_release_debounce_counter >= 2) {
                swipe_dir_t dir = compute_swipe_dir(tracker->startX, tracker->startY, tracker->lastGoodX, tracker->lastGoodY, min_swipe_length);
                if (dir != SWIPE_DIR_NONE) { tracker->swipeDetected = true; tracker->swipeDir = dir; }
                tracker->state = SWIPE_IDLE; swipe_release_debounce_counter = 0;
            }
        } break;
    default: tracker->state = SWIPE_IDLE; swipe_release_debounce_counter = 0; break;
    }
}

