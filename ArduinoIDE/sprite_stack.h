    #ifndef SPRITE_STACK_H
    #define SPRITE_STACK_H

    #include <lvgl.h>
    // Arduino.h will provide M_PI and DEG_TO_RAD macro, and other math functions if not already via lvgl/C std lib
    #include <Arduino.h> // Include for DEG_TO_RAD macro and potentially math functions like cos, sin, fabs

    // Define your Point struct (if not defined elsewhere)
    struct Point {
        int x;
        int y;
    };

    // DEG_TO_RAD is defined in Arduino.h (via Common.h), so we don't define it here.
    /**
    * @param scale how intense the top down view animations will be
    * @param camera_distance how much the z position of a slice affects its rotated_x and rotated_y offsets during pitch and yaw.
    */
    class SpriteStack {
      private:
        lv_obj_t **images = nullptr;
        const lv_img_dsc_t **sprite_set;
        int num_slices;
        int starting_index;
        float base_x, base_y;
        int width, height;
        float pitch = 0, yaw = 0, roll = 0;
        float depth_scale;
        float layer_offset;
        uint16_t base_zoom_lvgl; // LVGL zoom (256 = 100%)
        float current_zoom_percent; // Store the percentage for comparison
        float init_zoom_percent;

        bool created = false;
        float camera_distance = 200.0f;

        // Optimization members
        bool dirty = true; // Flag to indicate if an update is needed
        float cached_cos_pitch = 1.0f;
        float cached_sin_pitch = 0.0f;
        float cached_cos_yaw = 1.0f;
        float cached_sin_yaw = 0.0f;
        float cached_zoom_factor = 1.0f;
        float cached_center_projection = 0.005f; // 1.0f / 200.0f by default

        uint16_t last_lvgl_zoom_applied = 0; // To track if zoom actually changed for LVGL
        int16_t last_lvgl_roll_applied = 0;  // To track if roll actually changed for LVGL (roll * 10)

        // No need for a separate invalidate() method, setters handle dirty flag and cache.

      public:
        SpriteStack(const lv_img_dsc_t** sprites, int count, int starting_index = 0,
                    float scale = 1.0, float offset = 1.0,
                    float initial_zoom_percent = 100.0f);
        ~SpriteStack();
        void create(lv_obj_t* parent);
        void destroy();
        lv_obj_t* getLVGLObject() const;

        void setZoom(float percent);
        float getZoomPercent() const;
        float getInitialZoomPercent() const;
        uint16_t getLvglZoom() const;
        void getDim(int &width, int &height) const;

        void setPosition(float x, float y);
        Point getPosition() const;

        void setRotation(float p, float y, float r);
        void getRotation(float &p, float &y, float &r) const;

        void setLayerOffset(float offset);
        float getLayerOffset() const;

        void setCameraDistance(float distance);
        float getCameraDistance() const;

        void update();
        bool isDirty() const;

        void bringToForeground();
    };

    void test_sprite_stack(SpriteStack &my_stack);
    // extern int spriteFrameCount; // If this is used elsewhere, keep it.
    #endif // SPRITE_STACK_H