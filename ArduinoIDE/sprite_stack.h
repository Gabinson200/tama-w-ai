#ifndef SPRITE_STACK_H
#define SPRITE_STACK_H

#include <lvgl.h>

// Define your Point struct (if not defined elsewhere)
struct Point {
    int x;
    int y;
};

// Your complete SpriteStack class definition.
class SpriteStack {
  private:
    lv_obj_t **images = nullptr;
    const lv_img_dsc_t **sprite_set;
    int num_slices;
    int starting_index;
    float base_x, base_y;
    float pitch = 0, yaw = 0, roll = 0;
    float depth_scale;
    float layer_offset;
    uint16_t base_zoom;  // LVGL zoom (256 = 100%)
    bool created = false;
    float camera_distance = 200.0f;

  public:
    SpriteStack(const lv_img_dsc_t** sprites, int count, int starting_index = 0,
                float scale = 1.0, float offset = 1.0,
                float initial_zoom = 100.0f);
    ~SpriteStack(); 
    void create(lv_obj_t* parent);
    void destroy();
    void setZoom(float percent);
    uint16_t getZoom();
    void setPosition(float x, float y);
    Point getPosition();
    void setRotation(float p, float y, float r);
    void getRotation(float &p, float &y, float &r) const;
    void setLayerOffset(float offset);
    void update();
};

void test_sprite_stack(SpriteStack &my_stack);
extern int spriteFrameCount;
#endif // SPRITE_STACK_H
