#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include "I2C_BM8563.h"


#define CELESTIAL_SIZE 40

// Global variables for status arcs
extern uint8_t hunger_value;
extern uint8_t happiness_value;
extern uint8_t energy_value;

static uint8_t celestial_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(CELESTIAL_SIZE, CELESTIAL_SIZE)];
// how often to really redraw the *entire* scene:
static const unsigned long SCENE_UPDATE_INTERVAL_MS = 60UL * 1000UL; // 1 minute

/**
* @brief Moves all LVGL image objects of this sprite stack to the foreground
* of their parent. This is used for Z-ordering multiple SpriteStack instances.
* @param c1 lv_color 1
* @param c2 lv_color 2
* @param t degree to which to mix the two colors
*/
static inline lv_color_t interpolate_color(lv_color_t c1, lv_color_t c2, float t);

/**
* @brief Updates the position of the celestial body / day night cycle by checking once every minute.
* update takes a while so 
* @param rtc instance of a real time clock 
*/
void update_background(I2C_BM8563 &rtc);

/**
* @brief Create the background objects on the passed screen which are then updated with update_background
* @param parent lv screen object on which to create the background objects 
*/
void create_scene(lv_obj_t* parent = nullptr);

void set_gradient_background(lv_obj_t *parent, I2C_BM8563 rtc);

void timer_cb(lv_timer_t *timer);

void create_arcs(lv_obj_t *parent);

#endif
