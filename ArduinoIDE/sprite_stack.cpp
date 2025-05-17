#include "sprite_stack.h"
#include <math.h>  // For cos, sin, and sqrt

// -------------------------
// SPRITESTACK CLASS DEFINITION
// -------------------------
// Constructor
SpriteStack::SpriteStack(const lv_img_dsc_t** sprites, int count, int starting_index,
                         float scale, float offset, float initial_zoom)
  : sprite_set(sprites), num_slices(count), starting_index(starting_index), depth_scale(scale), layer_offset(offset) {
    images = new lv_obj_t*[num_slices];
    setZoom(initial_zoom);
}


// Create image objects and add them to the LVGL parent.
void SpriteStack::create(lv_obj_t *parent) {
    for (int j = 0; j < num_slices; j++) {
        int i = (starting_index + j) % num_slices;  // Correct index calculation.
        images[j] = lv_img_create(parent);
        lv_obj_set_parent(images[j], parent);
        lv_img_set_src(images[j], sprite_set[i]);
        lv_obj_add_flag(images[j], LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_clear_flag(images[j], LV_OBJ_FLAG_CLICKABLE);
        lv_img_set_zoom(images[j], base_zoom);
    }
    created = true;
    update();
}

void SpriteStack::destroy() {
    if(images) {
        for (int j = 0; j < num_slices; j++) {
            int i = (starting_index + j) % num_slices;
            if(images[i]) {
                lv_obj_del(images[i]);
                images[i] = NULL;
            }
        }
    }
}

// Destructor
SpriteStack::~SpriteStack() {
  destroy();
    if (images) {
        delete[] images;
        images = NULL;
    }
}

// Set the zoom level.
void SpriteStack::setZoom(float percent) {
    base_zoom = static_cast<uint16_t>(256 * (percent / 100.0f));
    update();
}

uint16_t SpriteStack::getZoom(){
  return base_zoom;
}

// Set the base (center) position.
void SpriteStack::setPosition(float x, float y) {
    base_x = x;
    base_y = y;
    update();
}

// Get the current position.
Point SpriteStack::getPosition() {
    Point p;
    p.x = (int)base_x;
    p.y = (int)base_y;
    return p;
}

// Set rotation values.
void SpriteStack::setRotation(float p, float y, float r) {
    pitch = p;
    yaw = y;
    roll = r;
    update();
}

void SpriteStack::getRotation(float &p, float &y, float &r) const {
    p = pitch;
    y = yaw;
    r = roll;
}

// Set the layer offset.
void SpriteStack::setLayerOffset(float offset) {
    layer_offset = offset;
    update();
}

// Update each slice's position and angle based on current parameters.
void SpriteStack::update() {
    if (!created) return;

    // Convert angles from degrees to radians.
    float pitch_rad = (pitch * 3.14159f) / 180.0f;
    float yaw_rad   = (yaw   * 3.14159f) / 180.0f;

    float cos_pitch = cos(pitch_rad);
    float sin_pitch = sin(pitch_rad);
    float cos_yaw   = cos(yaw_rad);
    float sin_yaw   = sin(yaw_rad);

    float center_projection = 1.0f / (camera_distance);

    for (int j = 0; j < num_slices; j++) {
        int i = (starting_index + j) % num_slices;  // Correct index
        lv_obj_t* img = images[j];
        const lv_img_dsc_t* sprite = sprite_set[i];

        lv_img_set_zoom(img, base_zoom);
        float zoom_factor = base_zoom / 256.0f;

        // Compute depth offset.
        float z = (i - num_slices / 2.0f) * depth_scale;
        float projection = 1.0f / (camera_distance - z);

        // Apply yaw and pitch rotations.
        float rotated_x = z * sin_yaw * projection / center_projection;
        float rotated_y = z * sin_pitch * projection / center_projection;
        float stack_offset = i * layer_offset * cos_pitch * zoom_factor;

        // Get sprite dimensions.
        lv_coord_t w = sprite->header.w;
        lv_coord_t h = sprite->header.h;

        // Calculate final positions.
        float final_x = base_x + rotated_x - (w * zoom_factor) / 2;
        float final_y = base_y + rotated_y - (h * zoom_factor) / 2 - stack_offset;

        lv_obj_set_pos(img, final_x, final_y);
        lv_img_set_angle(img, roll * 10);
    }
}


void test_sprite_stack(SpriteStack &my_stack ){
    // Phase 0: Pitch only
    // Phase 1: Yaw only
    // Phase 2: Roll only
    // Phase 3: Combination (all three rotating)
    static int phase = 0;  
    // Angle counter for phases 0–2 (isolated axis rotations)
    static float angle = 0.0f;  
    const float increment = 2.0f;  

    if(phase < 3) {
        angle += increment;
        if(angle >= 360.0f) {
            angle = 0.0f;
            phase++;  // Move to the next phase after a full rotation
        }
    } else {
        phase = 0;  // Restart the demo sequence
    }
    
    // Initialize rotations to zero.
    float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;
    
    // Set rotations based on the current phase.
    switch(phase) {
        case 0: // Pitch only
            pitch = angle;
            break;
            
        case 1: // Yaw only
            yaw = angle;
            break;
            
        case 2: // Roll only
            roll = angle;
            break;
            
        case 3: { // Combination: all three axes rotate independently
            static float pitchAngle = 0.0f;
            static float yawAngle   = 0.0f;
            static float rollAngle  = 0.0f;
            
            pitchAngle += 1.0f;
            yawAngle   += 2.0f;
            rollAngle  += 3.0f;
            
            if(pitchAngle >= 360.0f) pitchAngle -= 360.0f;
            if(yawAngle   >= 360.0f) yawAngle   -= 360.0f;
            if(rollAngle  >= 360.0f) rollAngle  -= 360.0f;
            
            pitch = pitchAngle;
            yaw   = yawAngle;
            roll  = rollAngle;
            break;
        }
    }
    
    my_stack.setRotation(pitch, yaw, roll);
}
