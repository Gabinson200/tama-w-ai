#include "touch.h"
#define USE_ARDUINO_GFX_LIBRARY // Must be defined before including the xiao round screen library
#include "lv_xiao_round_screen.h"
#include <Arduino.h>
#include <math.h>

// Forward declaration of the event callback for shape clicks.
static void shape_event_cb(lv_event_t * e);

//------------------- Boundary Checks ------------------------

/**
 * Check if a point (x, y) lies within a square defined by its corner coordinates.
 *
 * @param x       X coordinate of the point.
 * @param y       Y coordinate of the point.
 * @param x_min   Minimum X boundary.
 * @param x_max   Maximum X boundary.
 * @param y_min   Minimum Y boundary.
 * @param y_max   Maximum Y boundary.
 * @return        True if the point is within the square, false otherwise.
 */
bool is_within_square_bounds(int x, int y, int x_min, int x_max, int y_min, int y_max) {
    return (x >= x_min && x <= x_max && y >= y_min && y <= y_max);
}

/**
 * Check if a point (x, y) lies within a square area defined by its center and half-dimensions.
 *
 * @param x          X coordinate of the point.
 * @param y          Y coordinate of the point.
 * @param center_x   Center X coordinate of the square.
 * @param center_y   Center Y coordinate of the square.
 * @param half_width Half the width of the square.
 * @param half_height Half the height of the square.
 * @return           True if the point is within the square, false otherwise.
 */
bool is_within_square_bounds_center(int x, int y, int center_x, int center_y, int half_width, int half_height) {
    return is_within_square_bounds(x, y,
                                   center_x - half_width, center_x + half_width,
                                   center_y - half_height, center_y + half_height);
}

/**
 * Check if a point (x, y) lies within a circle defined by its center and radius.
 *
 * @param x         X coordinate of the point.
 * @param y         Y coordinate of the point.
 * @param center_x  Center X coordinate of the circle.
 * @param center_y  Center Y coordinate of the circle.
 * @param radius    Radius of the circle.
 * @return          True if the point is within the circle, false otherwise.
 */
bool is_within_circle_bounds(int x, int y, int center_x, int center_y, int radius) {
    int dx = x - center_x;
    int dy = y - center_y;
    return (dx * dx + dy * dy) <= (radius * radius);
}

//----------------------- Touch Validation ------------------------

/**
 * Validate and retrieve the current touch coordinates.
 * Clamps the coordinates to the display boundaries (assumed 240x240).
 *
 * @param touchX Pointer to store the X coordinate.
 * @param touchY Pointer to store the Y coordinate.
 * @return       True if a touch is detected, false otherwise.
 */
bool validate_touch(lv_coord_t* touchX, lv_coord_t* touchY) {
    if (!chsc6x_is_pressed()) {
        return false;
    }
    
    // Retrieve touch coordinates
    chsc6x_get_xy(touchX, touchY);

    // Debug print the raw coordinates
    //Serial.print("validate_touch -> raw coords: (");
    //Serial.print(*touchX);
    //Serial.print(", ");
    //Serial.print(*touchY);
    //Serial.println(")");

    // Clamp the values to valid display coordinates (0 to 240)
    *touchX = constrain(*touchX, 0, 240);
    *touchY = constrain(*touchY, 0, 240);
    
    return true;
}

/**
 * Retrieve the current touch coordinates.
 * Optionally prints the coordinates for debugging.
 *
 * @param x     Pointer to store the X coordinate.
 * @param y     Pointer to store the Y coordinate.
 * @param print If true, prints the coordinates to the Serial monitor.
 * @return      True if a touch is detected, false otherwise.
 */
bool get_touch(lv_coord_t* x, lv_coord_t* y, bool print) {
    if (chsc6x_is_pressed()) {
        lv_coord_t touchX, touchY;
        chsc6x_get_xy(&touchX, &touchY);

        // Clamp the coordinates using Arduino's constrain()
        touchX = constrain(touchX, 0, 240);
        touchY = constrain(touchY, 0, 240);

        if (print) {
            Serial.print("Touch coordinates: X = ");
            Serial.print(touchX);
            Serial.print(", Y = ");
            Serial.println(touchY);
        }
        // Update provided pointers with the valid coordinates
        *x = touchX;
        *y = touchY;
        return true;
    }
    return false;
}

//----------------------- Drawable Shape for Debugging -----------------------

// Global pointer to keep track of the last drawn shape (used for debugging touch areas)
static lv_obj_t *last_shape = NULL;

/**
 * Draws a touchable area on the screen for debugging.
 *
 * @param area          The rectangular area to be drawn.
 * @param is_circle     True to draw the area as a circle; false for a rectangle.
 * @param clear_previous If true, removes the previously drawn shape.
 */
void draw_area(lv_area_t area, bool is_circle, bool clear_previous) {
    // Clear the previous shape if requested
    if (clear_previous && last_shape != NULL) {
        lv_obj_del(last_shape);
        last_shape = NULL;
    }

    if (is_circle) {
        // Create a circle by making an object with rounded corners.
        lv_obj_t *circle = lv_obj_create(lv_scr_act());
        int radius = (area.x2 - area.x1) / 2;  // Assumes the area is a square for a circle
        
        lv_obj_set_size(circle, radius * 2, radius * 2);
        lv_obj_set_pos(circle, area.x1, area.y1);
        
        // Set the style to render a perfect circle.
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(circle, lv_color_hex(0x0000FF), 0);  // Blue background color.
        lv_obj_set_style_bg_opa(circle, LV_OPA_50, 0);                 // 50% opacity.
        lv_obj_set_style_border_width(circle, 0, 0);                     // No border.
        
        last_shape = circle;
    } else {
        // Create a rectangular touch area.
        lv_obj_t *rect = lv_obj_create(lv_scr_act());
        lv_obj_set_size(rect, area.x2 - area.x1, area.y2 - area.y1);
        lv_obj_set_pos(rect, area.x1, area.y1);
        
        // Set styles for a rectangle with sharp corners.
        lv_obj_set_style_radius(rect, 0, 0);
        lv_obj_set_style_bg_color(rect, lv_color_hex(0x0000FF), 0);  // Blue background color.
        lv_obj_set_style_bg_opa(rect, LV_OPA_50, 0);                 // 50% opacity.
        lv_obj_set_style_border_width(rect, 0, 0);
        
        last_shape = rect;
    }

    // Attach an event callback for click events on the shape.
    lv_obj_add_event_cb(last_shape, shape_event_cb, LV_EVENT_CLICKED, NULL);
}

/**
 * Event callback for shape clicks.
 * Logs whether a circle or rectangle was touched.
 *
 * @param e LVGL event pointer.
 */
static void shape_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    // Determine if the touched object is a circle by checking its radius style.
    bool is_circle = (lv_obj_get_style_radius(obj, 0) == LV_RADIUS_CIRCLE);
    Serial.print("Touch detected on ");
    Serial.println(is_circle ? "circle" : "rectangle");
}

//----------------------- Touch Detection in Specified Areas -----------------------

/**
 * Checks if a touch event occurs within a specified rectangular area.
 * Optionally visualizes the area on the screen.
 *
 * @param x_min Minimum X coordinate of the area.
 * @param x_max Maximum X coordinate of the area.
 * @param y_min Minimum Y coordinate of the area.
 * @param y_max Maximum Y coordinate of the area.
 * @param view  If true, draws the area on the screen for debugging.
 * @return      True if a valid touch is detected within the area, false otherwise.
 */
bool get_touch_in_area(int x_min, int x_max, int y_min, int y_max, bool view) {
    lv_coord_t touchX, touchY;

    if (view) {
        lv_area_t area = { x_min, y_min, x_max, y_max };
        draw_area(area, false, true);  // Draw as rectangle and clear previous shape.
    }

    return validate_touch(&touchX, &touchY) &&
           is_within_square_bounds(touchX, touchY, x_min, x_max, y_min, y_max);
}

/**
 * Overloaded function for center-based rectangular area detection.
 *
 * @param center_x   Center X coordinate of the area.
 * @param center_y   Center Y coordinate of the area.
 * @param half_width Half the width of the area.
 * @param half_height Half the height of the area.
 * @param view       If true, draws the area on the screen for debugging.
 * @return           True if a valid touch is detected within the area, false otherwise.
 */
bool get_touch_in_area_center(int center_x, int center_y, int half_width, int half_height, bool view) {
    return get_touch_in_area(center_x - half_width, center_x + half_width,
                             center_y - half_height, center_y + half_height, view);
}

/**
 * Checks if a touch event occurs within a specified circular area.
 * Optionally visualizes the circle.
 *
 * @param center_x Center X coordinate of the circle.
 * @param center_y Center Y coordinate of the circle.
 * @param radius   Radius of the circle.
 * @param view     If true, draws the circle on the screen for debugging.
 * @return         True if a valid touch is detected within the circle, false otherwise.
 */
bool get_touch_in_area_circle(int center_x, int center_y, int radius, bool view) {
    lv_coord_t touchX, touchY;

    if (view) {
        lv_area_t area = {
            center_x - radius, 
            center_y - radius,
            center_x + radius, 
            center_y + radius
        };
        draw_area(area, true, true);  // Draw as a circle and clear previous shape.
    }

    return validate_touch(&touchX, &touchY) &&
           is_within_circle_bounds(touchX, touchY, center_x, center_y, radius);
}

//----------------------- Touch Press Handling -----------------------

/**
 * Blocks execution until the touch sensor is released.
 */
void wait_for_release() {
    while (chsc6x_is_pressed()) {
        delay(10);
    }
}

/**
 * Blocks until the user presses and holds within the specified rectangular area
 * for the given duration. If the user releases or moves out before the time elapses,
 * the function returns false.
 *
 * @param duration Duration (in milliseconds) the touch must be held.
 * @param x_min    Minimum X coordinate of the area.
 * @param x_max    Maximum X coordinate of the area.
 * @param y_min    Minimum Y coordinate of the area.
 * @param y_max    Maximum Y coordinate of the area.
 * @param view     If true, draws the area on the screen for debugging.
 * @return         True if the area was pressed continuously for the duration; false otherwise.
 */
bool pressed(int duration, int x_min, int x_max, int y_min, int y_max, bool view) {
    lv_coord_t touchX, touchY;
    unsigned long pressStart = 0;
    bool isPressed = false;

    if (view) {
        lv_area_t area = { x_min, y_min, x_max, y_max };
        draw_area(area, false, true); // Draw the rectangular area for debugging.
    }

    // Blocking loop: wait until the press duration requirement is met or the touch is released.
    while (true) {
        if (validate_touch(&touchX, &touchY)) {
            // Check if the touch is within the specified rectangular area.
            bool isInArea = is_within_square_bounds(touchX, touchY, x_min, x_max, y_min, y_max);

            if (isInArea) {
                // If this is the first detection in the area, mark the start time.
                if (!isPressed) {
                    pressStart = millis();
                    isPressed  = true;
                }
                // If the touch has been held for the required duration, return true.
                if (millis() - pressStart >= (unsigned long)duration) {
                    return true;
                }
            } else {
                // Reset if the touch moves outside the area.
                isPressed  = false;
                pressStart = 0;
            }
        } else {
            // If no valid touch is detected after a press has started, return false.
            if (isPressed) {
                return false;
            }
            delay(2); // Delay to prevent busy-waiting.
        }
    }
}

/**
 * Overloaded function for center-based rectangular press detection.
 *
 * @param duration   Duration (in milliseconds) the touch must be held.
 * @param center_x   Center X coordinate of the area.
 * @param center_y   Center Y coordinate of the area.
 * @param half_width Half the width of the area.
 * @param half_height Half the height of the area.
 * @param view       If true, draws the area on the screen for debugging.
 * @return           True if the area was pressed continuously for the duration; false otherwise.
 */
bool pressed_center(int duration, int center_x, int center_y, int half_width, int half_height, bool view) {
    return pressed(duration,
                   center_x - half_width, center_x + half_width,
                   center_y - half_height, center_y + half_height, view);
}

/**
 * Blocks until the user presses and holds within the specified circular area
 * for the given duration. Returns false if the user releases or moves outside the area.
 *
 * @param duration Duration (in milliseconds) the touch must be held.
 * @param center_x Center X coordinate of the circle.
 * @param center_y Center Y coordinate of the circle.
 * @param radius   Radius of the circular area.
 * @param view     If true, draws the circle on the screen for debugging.
 * @return         True if the circle was pressed continuously for the duration; false otherwise.
 */
bool pressed_circle(int duration, int center_x, int center_y, int radius, bool view) {
    lv_coord_t touchX, touchY;
    unsigned long pressStart = 0;
    bool isPressed = false;

    if (view) {
        lv_area_t area = {
            center_x - radius, center_y - radius,
            center_x + radius, center_y + radius
        };
        draw_area(area, true, true); // Draw the circular area for debugging.
    }

    // Blocking loop: wait until the press duration requirement is met or the touch is released.
    while (true) {
        if (validate_touch(&touchX, &touchY)) {
            // Check if the touch is within the circular area.
            bool isInArea = is_within_circle_bounds(touchX, touchY, center_x, center_y, radius);

            if (isInArea) {
                if (!isPressed) {
                    pressStart = millis();
                    isPressed  = true;
                }
                if (millis() - pressStart >= (unsigned long)duration) {
                    return true;
                }
            } else {
                // Reset if the touch moves outside the circle.
                isPressed  = false;
                pressStart = 0;
            }
        } else {
            // If a press was detected but the touch is no longer valid, return false.
            if (isPressed) {
                return false;
            }
            delay(2);
        }
    }
}

//-------- Swiping functions -----------------------------------------
//------------------------------------------------------------------------------
// Helper Function: Compute Swipe Direction
//------------------------------------------------------------------------------
// Given a start and end point along with a minimum swipe length threshold,
// this function determines the dominant swipe direction.
static swipe_dir_t compute_swipe_dir(lv_coord_t startX, lv_coord_t startY,
                                     lv_coord_t endX,   lv_coord_t endY,
                                     int min_swipe_length)
{
    int dx = endX - startX;
    int dy = endY - startY;
    int absX = abs(dx);
    int absY = abs(dy);

    // Both movements below the threshold means no valid swipe.
    if (absX < min_swipe_length && absY < min_swipe_length) {
        return SWIPE_DIR_NONE;
    }

    // Determine the dominant axis and return the corresponding swipe direction.
    if (absX > absY) {
        return (dx < 0) ? SWIPE_DIR_LEFT : SWIPE_DIR_RIGHT;
    } else {
        return (dy < 0) ? SWIPE_DIR_UP : SWIPE_DIR_DOWN;
    }
}

//------------------------------------------------------------------------------
// Configuration Constant
//------------------------------------------------------------------------------
// Threshold for ignoring sudden, unrealistic jumps in the touch data.
static const int MAX_JUMP = 80;

/**
 * @brief Non-blocking swipe detection that only requires the initial press 
 *        to be in the bounding box. Once pressed, the user may drag outside.
 *
 * The function implements a state machine with three states:
 *  - SWIPE_IDLE:    Waiting for a valid press within the bounding box.
 *  - SWIPE_PRESSED: Initial press detected; waiting for drag confirmation.
 *  - SWIPE_DRAGGING: Drag is in progress; monitor movement and finalize swipe.
 *
 * @param x_min           Minimum X coordinate of the initial press area.
 * @param x_max           Maximum X coordinate of the initial press area.
 * @param y_min           Minimum Y coordinate of the initial press area.
 * @param y_max           Maximum Y coordinate of the initial press area.
 * @param min_swipe_length Minimum swipe distance to consider it valid.
 * @param tracker         Pointer to the swipe_tracker_t structure.
 */
void update_swipe_state(int x_min, int x_max,
                        int y_min, int y_max,
                        int min_swipe_length,
                        swipe_tracker_t *tracker)
{
    // Clear previous swipe detection flag.
    tracker->swipeDetected = false;

    // Read the current touch coordinates.
    lv_coord_t x, y;
    bool isTouchValid = validate_touch(&x, &y);

    // Use a static counter to ensure that a release is sustained and not a glitch.
    static uint8_t releaseCounter = 0;

    switch (tracker->state) {

    //----------------------------------------------------------------------
    // SWIPE_IDLE: Waiting for the initial touch within the bounding box.
    //----------------------------------------------------------------------
    case SWIPE_IDLE:
        releaseCounter = 0;  // Reset release counter for a new swipe cycle.
        if (isTouchValid) {
            // Verify the initial press is within the defined area.
            if ((x >= x_min && x <= x_max) && (y >= y_min && y <= y_max)) {
                // Record the starting position and mark as pressed.
                tracker->startX = x;
                tracker->startY = y;
                tracker->currentX = x;
                tracker->currentY = y;
                tracker->lastGoodX = x;
                tracker->lastGoodY = y;
                tracker->state = SWIPE_PRESSED;
                Serial.println("SWIPE_IDLE -> SWIPE_PRESSED");
            }
        }
        break;

    //----------------------------------------------------------------------
    // SWIPE_PRESSED: Initial press confirmed; waiting for movement.
    //----------------------------------------------------------------------
    case SWIPE_PRESSED:
        releaseCounter = 0;
        if (isTouchValid) {
            // Update current positions and transition to dragging.
            tracker->currentX = x;
            tracker->currentY = y;
            tracker->lastGoodX = x;
            tracker->lastGoodY = y;
            tracker->state = SWIPE_DRAGGING;
            Serial.println("SWIPE_PRESSED -> SWIPE_DRAGGING");
        } else {
            // If the touch is released too soon, reset the state.
            tracker->state = SWIPE_IDLE;
            Serial.println("SWIPE_PRESSED -> SWIPE_IDLE (released quickly)");
        }
        break;

    //----------------------------------------------------------------------
    // SWIPE_DRAGGING: Touch is moving; track updates and determine swipe.
    //----------------------------------------------------------------------
    case SWIPE_DRAGGING:
        if (isTouchValid) {
            releaseCounter = 0;  // Reset the counter if touch continues.

            // Calculate the jump between the new and last valid coordinates.
            int jumpX = abs(x - tracker->lastGoodX);
            int jumpY = abs(y - tracker->lastGoodY);

            // If the movement exceeds MAX_JUMP, treat it as an outlier.
            if (jumpX > MAX_JUMP || jumpY > MAX_JUMP) {
                Serial.print("Ignoring outlier: (");
                Serial.print(x);
                Serial.print(", ");
                Serial.print(y);
                Serial.println(")");
                // Do not update positions; keep the last good data.
            } else {
                // Update current and last good positions normally.
                tracker->currentX = x;
                tracker->currentY = y;
                tracker->lastGoodX = x;
                tracker->lastGoodY = y;
            }
        } else {
            // Touch is not valid (likely released); increment release counter.
            releaseCounter++;
            // After two consecutive invalid reads, finalize the swipe.
            if (releaseCounter >= 2) {
                lv_coord_t endX = tracker->currentX;
                lv_coord_t endY = tracker->currentY;

                // Debug: print start and end coordinates along with displacements.
                Serial.print("Start: (");
                Serial.print(tracker->startX);
                Serial.print(", ");
                Serial.print(tracker->startY);
                Serial.print(") End: (");
                Serial.print(endX);
                Serial.print(", ");
                Serial.print(endY);
                Serial.print(") => dx=");
                Serial.print(endX - tracker->startX);
                Serial.print(", dy=");
                Serial.println(endY - tracker->startY);

                // Determine the swipe direction based on the movement.
                swipe_dir_t dir = compute_swipe_dir(tracker->startX, tracker->startY,
                                                    endX, endY,
                                                    min_swipe_length);

                if (dir != SWIPE_DIR_NONE) {
                    tracker->swipeDetected = true;
                    tracker->swipeDir = dir;
                    Serial.print("SWIPE_DRAGGING -> SWIPE_IDLE, swipeDetected in direction: ");
                    switch (dir) {
                        case SWIPE_DIR_LEFT:  Serial.println("LEFT");  break;
                        case SWIPE_DIR_RIGHT: Serial.println("RIGHT"); break;
                        case SWIPE_DIR_UP:    Serial.println("UP");    break;
                        case SWIPE_DIR_DOWN:  Serial.println("DOWN");  break;
                        default:              Serial.println("NONE?"); break;
                    }
                } else {
                    Serial.println("SWIPE_DRAGGING -> SWIPE_IDLE, short swipe");
                }
                // Reset the state for the next swipe gesture.
                tracker->state = SWIPE_IDLE;
                releaseCounter = 0;
            }
        }
        break;

    //----------------------------------------------------------------------
    // Default: Catch-all (should never occur) and reset.
    //----------------------------------------------------------------------
    default:
        tracker->state = SWIPE_IDLE;
        releaseCounter = 0;
        break;
    }
}
