// -----------------------------------------
//         Header guards and prototypes
// -----------------------------------------
#ifndef TOUCH_SENSOR_FUNCTIONS_H
#define TOUCH_SENSOR_FUNCTIONS_H

#include <lvgl.h>
#define USE_ARDUINO_GFX_LIBRARY

// Display/Touch init (from lv_xiao_round_screen.h)
void lv_xiao_disp_init(void);
void lv_xiao_touch_init(void);

// Bounds checking
bool is_within_square_bounds(int x, int y, int x_min, int x_max, int y_min, int y_max);
bool is_within_square_bounds_center(int x, int y, int center_x, int center_y, int half_width, int half_height);
bool is_within_circle_bounds(int x, int y, int center_x, int center_y, int radius);

// Drawing function, mainly for debugging
void draw_area(lv_area_t area, bool is_circle, bool clear_previous);
static void shape_event_cb(lv_event_t * e);

// Touch functions
bool validate_touch(lv_coord_t* touchX, lv_coord_t* touchY);
bool get_touch(lv_coord_t* x, lv_coord_t* y, bool print = false);

// Checking if user touched certain bounds
bool get_touch_in_area(int x_min, int x_max, int y_min, int y_max, bool view = false);
bool get_touch_in_area_center(int center_x, int center_y, int half_width, int half_height, bool view = false);
bool get_touch_in_area_circle(int center_x, int center_y, int radius, bool view = false);

// Press (hold) functions
/*
bool pressed(int duration, int x_min, int x_max, int y_min, int y_max, bool view = false);
bool pressed_center(int duration, int center_x, int center_y, int half_width, int half_height, bool view = false);
bool pressed_circle(int duration, int center_x, int center_y, int radius, bool view = false);
*/

// ----------------New non blocking touch functions
// --- Gesture States ---
enum class GestureState {
    IDLE,       // Not tracking, ready to begin
    POSSIBLE,   // Initial conditions met, could become a gesture
    BEGAN,      // Gesture has officially begun (e.g., long press duration met)
    ENDED,      // Gesture completed successfully
    FAILED,     // Conditions not met, gesture did not complete
    CANCELLED   // Gesture was externally cancelled
};

const char* gestureStateToString(GestureState state);

// --- Touch Point Structure (simplified) ---
struct TouchInfo {
    bool is_pressed = false;
    lv_coord_t x = 0;
    lv_coord_t y = 0;
    unsigned long timestamp = 0;
};

// --- Base Class ---
class BaseGestureRecognizer {
public:
    BaseGestureRecognizer();
    virtual ~BaseGestureRecognizer() {}

    virtual void update(const TouchInfo& current_touch_info) = 0;
    virtual void reset();
    void cancel(); // New method to explicitly cancel

    GestureState get_state() const;
    void set_target_area(lv_area_t area);
    lv_area_t get_target_area() const;
    void set_enabled(bool enable);
    bool is_enabled() const;

    void* user_data; // Optional, for associating data

protected:
    GestureState current_state;
    lv_area_t target_area;
    bool enabled;
    TouchInfo start_touch_info; // Store initial touch down info

    bool is_within_target_area(lv_coord_t x, lv_coord_t y) const;
};

// --- Tap Gesture Recognizer ---
class TapGestureRecognizer : public BaseGestureRecognizer {
public:
    TapGestureRecognizer(unsigned long max_duration_ms = 300, lv_coord_t max_movement = 10);

    void update(const TouchInfo& current_touch_info) override;
    void reset() override;

    void set_config(unsigned long max_duration_ms, lv_coord_t max_movement);

private:
    unsigned long max_duration_ms;
    lv_coord_t max_movement_pixels;
};

// --- Long Press Gesture Recognizer ---
class LongPressGestureRecognizer : public BaseGestureRecognizer {
public:
    LongPressGestureRecognizer(unsigned long min_duration_ms = 1000, lv_coord_t max_movement = 15);

    void update(const TouchInfo& current_touch_info) override;
    void reset() override;

    void set_config(unsigned long min_duration_ms, lv_coord_t max_movement);

    // Get the location where the long press was recognized
    lv_coord_t recognized_at_x;
    lv_coord_t recognized_at_y;

private:
    unsigned long min_duration_ms;
    lv_coord_t max_movement_pixels;
};

//---------------- For swiping functions ----------------------

//------------------------------------------------------------------------------
// Swipe Direction Enumeration
//------------------------------------------------------------------------------
typedef enum {
    SWIPE_DIR_NONE = 0,  // No swipe detected
    SWIPE_DIR_LEFT,      // Swipe from right to left
    SWIPE_DIR_RIGHT,     // Swipe from left to right
    SWIPE_DIR_UP,        // Swipe from bottom to top
    SWIPE_DIR_DOWN       // Swipe from top to bottom
} swipe_dir_t;

//------------------------------------------------------------------------------
// Swipe State Enumeration
//------------------------------------------------------------------------------
typedef enum {
    SWIPE_IDLE = 0,      // Waiting for a touch event or after a swipe is finalized
    SWIPE_PRESSED,       // Initial press detected (waiting to see if drag occurs)
    SWIPE_DRAGGING       // Drag in progress (touch is moving)
} swipe_state_t;

//------------------------------------------------------------------------------
// Swipe Tracker Structure
//------------------------------------------------------------------------------
// This structure holds the current state and coordinates for swipe detection.
typedef struct {
    swipe_state_t state;       // Current state of the swipe (idle, pressed, dragging)
    bool swipeDetected;        // Set to true when a valid swipe is detected
    swipe_dir_t swipeDir;      // Direction of the detected swipe
    lv_coord_t startX, startY; // Coordinates where the initial press occurred
    lv_coord_t currentX, currentY; // Most recent touch coordinates
    lv_coord_t lastGoodX, lastGoodY; // Last valid coordinates (to filter out spurious jumps)
} swipe_tracker_t;

/**
 * @brief Non-blocking swipe detection function.
 *
 * This function monitors touch input and updates the swipe_tracker_t
 * structure. The swipe is recognized only if the initial press is within the
 * specified bounding box. Once pressed, the user may drag outside the box.
 *
 * @param x_min           Minimum X coordinate of the initial press area.
 * @param x_max           Maximum X coordinate of the initial press area.
 * @param y_min           Minimum Y coordinate of the initial press area.
 * @param y_max           Maximum Y coordinate of the initial press area.
 * @param min_swipe_length Minimum distance (in pixels) required for a valid swipe.
 * @param tracker         Pointer to the swipe_tracker_t object that stores swipe state.
 */
void update_swipe_state(int x_min, int x_max,
                        int y_min, int y_max,
                        int min_swipe_length,
                        swipe_tracker_t *tracker);

#endif