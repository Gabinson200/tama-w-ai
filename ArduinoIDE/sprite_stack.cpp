#include "sprite_stack.h"
#include <Arduino.h> // For millis(), DEG_TO_RAD macro, and math functions (cos, sin, fabs)
#include <lvgl.h> // Include for LVGL functions like lv_obj_invalidate

// -------------------------
// SPRITESTACK CLASS DEFINITION
// -------------------------

// Constructor
SpriteStack::SpriteStack(const lv_img_dsc_t** sprites, int count, int starting_index,
                         float scale, float offset, float initial_zoom_percent)
  : sprite_set(sprites),
    num_slices(count),
    starting_index(starting_index),
    base_x(0), base_y(0),
    pitch(0), yaw(0), roll(0),
    depth_scale(scale),
    layer_offset(offset),
    current_zoom_percent(initial_zoom_percent),
    init_zoom_percent(initial_zoom_percent),
    created(false),
    camera_distance(200.0f),
    dirty(true) {
    images = new lv_obj_t*[num_slices];
    for(int i = 0; i < num_slices; ++i) {
        images[i] = nullptr;
    }

    // Initialize width and height from the first sprite
    if (sprite_set != nullptr && num_slices > 0 && sprite_set[0] != nullptr) {
        this->width = sprite_set[0]->header.w;
        this->height = sprite_set[0]->header.h;
    } else {
        this->width = 0; // Default or error value
        this->height = 0; // Default or error value
    }

    base_zoom_lvgl = static_cast<uint16_t>(256 * (current_zoom_percent / 100.0f));
    cached_zoom_factor = base_zoom_lvgl / 256.0f;
    if (fabs(camera_distance) > 1e-5) { // Avoid division by zero
         cached_center_projection = 1.0f / camera_distance;
    } else {
         cached_center_projection = 1.0f / 1e-5f; // A very small denominator if camera_distance is zero
    }

    float pitch_rad = pitch * DEG_TO_RAD; // DEG_TO_RAD is now from Arduino.h
    float yaw_rad   = yaw   * DEG_TO_RAD;
    cached_cos_pitch = cos(pitch_rad);
    cached_sin_pitch = sin(pitch_rad);
    cached_cos_yaw   = cos(yaw_rad);
    cached_sin_yaw   = sin(yaw_rad);
}

SpriteStack::~SpriteStack() {
  destroy();
    if (images) {
        delete[] images;
        images = nullptr;
    }
}

void SpriteStack::create(lv_obj_t *parent) {
    if (created) return;

    for (int j = 0; j < num_slices; j++) {
        images[j] = lv_img_create(parent);
        if (!images[j]) {
            // LVGL_LOG_WARN("Failed to create image for slice %d", j); // Optional: log warning
            continue;
        }
        // lv_obj_set_parent(images[j], parent); // Not needed if parent passed to lv_img_create

        int sprite_idx = (starting_index + j) % num_slices;
        lv_img_set_src(images[j], sprite_set[sprite_idx]);

        lv_obj_add_flag(images[j], LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_clear_flag(images[j], LV_OBJ_FLAG_CLICKABLE);

        lv_img_set_zoom(images[j], base_zoom_lvgl);
        lv_img_set_angle(images[j], static_cast<int16_t>(roll * 10));
    }
    created = true;
    dirty = true;
}

void SpriteStack::destroy() {
    if(images) {
        for (int j = 0; j < num_slices; j++) {
            if(images[j]) {
                lv_obj_del(images[j]);
                images[j] = nullptr;
            }
        }
    }
    created = false;
}

// Implementation for the new method to get the base LVGL object
lv_obj_t* SpriteStack::getLVGLObject() const {
    if (created && images && images[0]) {
        return images[0]; // Return the base image object
    }
    return nullptr; // Return null if not created or images are invalid
}

void SpriteStack::getDim(int &w, int &h) const {
    // Use the already initialized member variables
    w = this->width;
    h = this->height;

    // as afallback if members weren't initialized:
    if (sprite_set != nullptr && num_slices > 0 && sprite_set[0] != nullptr) {
        w = sprite_set[0]->header.w;
        h = sprite_set[0]->header.h;
    } else {
        w = 0; // Or some other default/error indicator
        h = 0;
    }
}

void SpriteStack::setZoom(float percent) {
    if (fabs(current_zoom_percent - percent) > 1e-5) {
        current_zoom_percent = percent;
        base_zoom_lvgl = static_cast<uint16_t>(256 * (current_zoom_percent / 100.0f));
        cached_zoom_factor = base_zoom_lvgl / 256.0f;
        dirty = true;
    }
}

float SpriteStack::getZoomPercent() const {
  return current_zoom_percent;
}

float SpriteStack::getInitialZoomPercent() const {
  return init_zoom_percent;
}

uint16_t SpriteStack::getLvglZoom() const {
    return base_zoom_lvgl;
}

void SpriteStack::setPosition(float x, float y) {
    if (fabs(base_x - x) > 1e-5 || fabs(base_y - y) > 1e-5) {
        base_x = x;
        base_y = y;
        dirty = true;
    }
}

Point SpriteStack::getPosition() const {
    Point p;
    p.x = static_cast<int>(round(base_x)); // Round for better pixel accuracy
    p.y = static_cast<int>(round(base_y));
    return p;
}

void SpriteStack::setRotation(float p, float y, float r) {
    bool rotation_changed = false;
    if (fabs(pitch - p) > 1e-5) {
        pitch = p;
        float pitch_rad = pitch * DEG_TO_RAD;
        cached_cos_pitch = cos(pitch_rad);
        cached_sin_pitch = sin(pitch_rad);
        rotation_changed = true;
    }
    if (fabs(yaw - y) > 1e-5) {
        yaw = y;
        float yaw_rad   = yaw   * DEG_TO_RAD;
        cached_cos_yaw   = cos(yaw_rad);
        cached_sin_yaw   = sin(yaw_rad);
        rotation_changed = true;
    }
    if (fabs(roll - r) > 1e-5) {
        roll = r;
        rotation_changed = true;
    }
    if (rotation_changed) {
        dirty = true;
    }
}

void SpriteStack::getRotation(float &p, float &y, float &r) const {
    p = pitch;
    y = yaw;
    r = roll;
}

void SpriteStack::setLayerOffset(float offset) {
    if (fabs(layer_offset - offset) > 1e-5) {
        layer_offset = offset;
        dirty = true;
    }
}

float SpriteStack::getLayerOffset() const {
    return layer_offset;
}

void SpriteStack::setCameraDistance(float distance) {
    if (fabs(camera_distance - distance) > 1e-5) {
        camera_distance = distance;
        if (fabs(camera_distance) > 1e-5) { // Avoid division by zero
            cached_center_projection = 1.0f / camera_distance;
        } else {
            cached_center_projection = 1.0f / 1e-5f;
        }
        dirty = true;
    }
}

float SpriteStack::getCameraDistance() const {
    return camera_distance;
}

bool SpriteStack::isDirty() const {
    return dirty;
}

void SpriteStack::update() {
    if (!created || !dirty) {
        return;
    }

    bool overall_zoom_needs_lvgl_update = (last_lvgl_zoom_applied != base_zoom_lvgl);
    int16_t current_lvgl_roll = static_cast<int16_t>(roll * 10.0f); // Round for angle
    bool overall_roll_needs_lvgl_update = (last_lvgl_roll_applied != current_lvgl_roll);

    for (int j = 0; j < num_slices; j++) {
        if (!images[j]) continue;

        int sprite_idx = (starting_index + j) % num_slices;
        lv_obj_t* img = images[j];
        const lv_img_dsc_t* sprite_asset = sprite_set[sprite_idx];

        if (overall_zoom_needs_lvgl_update) {
            lv_img_set_zoom(img, base_zoom_lvgl);
        }

        float z = (j - (num_slices - 1) / 2.0f) * depth_scale;
        float projection_denominator = camera_distance - z;
        float projection;

        if (fabs(projection_denominator) < 1e-5) { // Avoid division by zero or very small numbers
            // Make object very large/far or handle as an error/clamp
            projection = 1.0f / ( (projection_denominator >= 0) ? 1e-5f : -1e-5f );
        } else {
            projection = 1.0f / projection_denominator;
        }

        float effective_projection_ratio = projection / cached_center_projection;
        if (fabs(cached_center_projection) < 1e-5) { // if camera_distance was 0
             effective_projection_ratio = 1.0f; // Avoid division by zero if center_projection is zero
        }


        float rotated_x = z * cached_sin_yaw * effective_projection_ratio;
        float rotated_y = z * cached_sin_pitch * effective_projection_ratio;

        float stack_offset = j * layer_offset * cached_cos_pitch * cached_zoom_factor;

        lv_coord_t w = sprite_asset->header.w;
        lv_coord_t h = sprite_asset->header.h;

        float final_x = base_x + rotated_x - (w * cached_zoom_factor) / 2.0f;
        float final_y = base_y + rotated_y - (h * cached_zoom_factor) / 2.0f - stack_offset;

        lv_obj_set_pos(img, static_cast<lv_coord_t>(round(final_x)), static_cast<lv_coord_t>(round(final_y)));

        if (overall_roll_needs_lvgl_update) {
            lv_img_set_angle(img, current_lvgl_roll);
        }
    }

    if (overall_zoom_needs_lvgl_update) {
        last_lvgl_zoom_applied = base_zoom_lvgl;
    }
    if (overall_roll_needs_lvgl_update) {
        last_lvgl_roll_applied = current_lvgl_roll;
    }

    dirty = false;

    // Invalidate the object to force LVGL to redraw its area
    // We only need to invalidate the base object, as LVGL manages the layered images
    if (images && images[0]) {
        lv_obj_invalidate(images[0]);
    }
}

/**
* @brief Moves all LVGL image objects of this sprite stack to the foreground
* of their parent. This is used for Z-ordering multiple SpriteStack instances.
*/
void SpriteStack::bringToForeground() {
    if (!created || !images) return;

    // Iterate through all image layers of this stack
    for (int i = 0; i < num_slices; ++i) {
        if (images[i]) {
            // lv_obj_move_foreground() makes this specific image layer
            // the last child to be drawn on its parent, effectively placing it on top.
            lv_obj_move_foreground(images[i]);
        }
    }
}

// test_sprite_stack function
void test_sprite_stack(SpriteStack &my_stack ){
    static int phase = 0;
    static float angle = 0.0f;
    const float increment = 2.0f; // Degrees per call

    static uint32_t last_phase_change_time = 0;
    static uint32_t last_angle_update_time_ph3 = 0; // For consistent speed in phase 3
    uint32_t current_time_ms = millis();

    // Phase cycling logic
    if (phase < 3) { // Phases 0, 1, 2: rotate one axis for a full 360 degrees
        angle += increment;
        if (angle >= 360.0f) {
            angle = 0.0f;
            phase++;
            if (phase == 3) { // Just entered phase 3
                last_phase_change_time = current_time_ms;
                // Reset angles for phase 3 to start from 0
                // (Static variables for phase 3 angles will hold their values across calls)
            }
        }
    } else { // Phase 3: combined rotation, then reset
        // Stay in phase 3 for a certain duration (e.g., 5 seconds)
        if (current_time_ms - last_phase_change_time > 5000) {
            phase = 0;
            angle = 0.0f; // Reset angle for phase 0
        }
    }

    float p = 0.0f, y = 0.0f, r = 0.0f;

    switch(phase) {
        case 0: // Pitch only
            p = angle;
            break;
        case 1: // Yaw only
            y = angle;
            break;
        case 2: // Roll only
            r = angle;
            break;
        case 3: { // Combination: all three axes rotate
            static float pitchAngle_ph3 = 0.0f;
            static float yawAngle_ph3   = 0.0f;
            static float rollAngle_ph3  = 0.0f;

            // Update angles for phase 3 at a consistent rate
            // This example updates them at the same rate as other phases for simplicity
            // For more controlled speed, you'd use delta time.
            // Here, we just increment them each time test_sprite_stack is called during phase 3.

            pitchAngle_ph3 += 1.0f; // Different increments for visual variety
            yawAngle_ph3   += 1.5f;
            rollAngle_ph3  += 2.0f;

            if(pitchAngle_ph3 >= 360.0f) pitchAngle_ph3 -= 360.0f;
            if(yawAngle_ph3   >= 360.0f) yawAngle_ph3   -= 360.0f;
            if(rollAngle_ph3  >= 360.0f) rollAngle_ph3  -= 360.0f;

            p = pitchAngle_ph3;
            y = yawAngle_ph3;
            r = rollAngle_ph3;
            break;
        }
    }
    my_stack.setRotation(p, y, r);
    // The SpriteStack's own update() should be called in your main application loop.
}