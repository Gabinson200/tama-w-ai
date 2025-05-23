#ifndef TOUCH_SENSOR_FUNCTIONS_H
#define TOUCH_SENSOR_FUNCTIONS_H

#include <lvgl.h>
#include <Arduino.h>

struct TouchInfo {
    bool is_pressed = false;
    lv_coord_t x = 0;
    lv_coord_t y = 0;
    unsigned long timestamp = 0;
};

void update_global_touch_info(TouchInfo* info);

bool is_within_square_bounds(int x, int y, int x_min, int x_max, int y_min, int y_max);
bool is_within_square_bounds_center(int x, int y, int center_x, int center_y, int half_width, int half_height);
bool is_within_circle_bounds(int x, int y, int center_x, int center_y, int radius);

void draw_area(lv_area_t area, bool is_circle, bool clear_previous);

enum class GestureState {
    IDLE, POSSIBLE, BEGAN, ENDED, FAILED, CANCELLED
};
const char* gestureStateToString(GestureState state);

class BaseGestureRecognizer {
public:
    BaseGestureRecognizer();
    virtual ~BaseGestureRecognizer() {}
    virtual void update(const TouchInfo& current_touch_info) = 0;
    virtual void reset();
    void cancel();
    GestureState get_state() const;
    void set_target_area(lv_area_t area);
    lv_area_t get_target_area() const;
    void set_enabled(bool enable);
    bool is_enabled() const;
    void* user_data;
protected:
    GestureState current_state;
    lv_area_t target_area;
    bool enabled;
    TouchInfo start_touch_info;
    bool is_within_target_area(lv_coord_t x, lv_coord_t y) const;
};

class TapGestureRecognizer : public BaseGestureRecognizer {
public:
    TapGestureRecognizer(unsigned long max_duration_ms = 100, lv_coord_t max_movement_pixels = 48,
                         unsigned long tap_confirmation_delay_ms = 80);

    void update(const TouchInfo& current_touch_info) override;
    void reset() override;

    void set_config(unsigned long max_duration_ms, lv_coord_t max_movement_pixels, unsigned long tap_confirmation_delay_ms);

    void notify_current_press_is_claimed();
    bool is_waiting_for_confirmation() const;

    // Methods to get tap end coordinates
    lv_coord_t get_tap_x() const { return tap_x; }
    lv_coord_t get_tap_y() const { return tap_y; }

private:
    unsigned long max_duration_ms;
    lv_coord_t max_movement_pixels;
    bool waiting_for_tap_confirmation;
    unsigned long tap_potential_release_timestamp;
    unsigned long tap_confirmation_delay_ms;
    bool ongoing_press_is_claimed_by_other;
    lv_coord_t tap_x; // [NEW] Store tap release coordinates
    lv_coord_t tap_y; // [NEW]
};

class LongPressGestureRecognizer : public BaseGestureRecognizer {
public:
    LongPressGestureRecognizer(unsigned long min_duration_ms = 1000, lv_coord_t max_movement_pixels = 24);
    void update(const TouchInfo& current_touch_info) override;
    void reset() override;
    void set_config(unsigned long min_duration_ms, lv_coord_t max_movement_pixels);
    lv_coord_t recognized_at_x;
    lv_coord_t recognized_at_y;
private:
    unsigned long min_duration_ms;
    lv_coord_t max_movement_pixels;
};

typedef enum { SWIPE_DIR_NONE = 0, SWIPE_DIR_LEFT, SWIPE_DIR_RIGHT, SWIPE_DIR_UP, SWIPE_DIR_DOWN } swipe_dir_t;
typedef enum { SWIPE_IDLE = 0, SWIPE_PRESSED, SWIPE_DRAGGING } swipe_state_t;
typedef struct {
    swipe_state_t state;
    bool swipeDetected;
    swipe_dir_t swipeDir;
    lv_coord_t startX, startY;
    lv_coord_t currentX, currentY;
    lv_coord_t lastGoodX, lastGoodY;
} swipe_tracker_t;

void update_swipe_state(int x_min, int x_max, int y_min, int y_max,
                        int min_swipe_length, swipe_tracker_t *tracker,
                        const TouchInfo& current_touch_info);


#endif