#ifndef LV_XIAO_ROUND_SCREEN_H
#define LV_XIAO_ROUND_SCREEN_H

#include <Arduino.h>
#include <lvgl.h>
#include <SPI.h>
#include <Wire.h>

/* 
  Uncomment exactly ONE of these if you haven't
  defined them in your build environment / platformio.ini
*/
// #define USE_TFT_ESPI_LIBRARY
#define USE_ARDUINO_GFX_LIBRARY

// Screen parameters
#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   240
#define LVGL_BUFF_SIZE  20   // Number of rows in LVGL draw buffer

// I2C capacitive touch driver
#define CHSC6X_I2C_ID          0x2e
#define CHSC6X_MAX_POINTS_NUM   1
#define CHSC6X_READ_POINT_LEN   5
#define TOUCH_INT               D7

#ifndef XIAO_BL
#define XIAO_BL D6
#endif
#define XIAO_DC D3
#define XIAO_CS D1

// If SPI_FREQ is not defined elsewhere, define it here.
#ifndef SPI_FREQ
#define SPI_FREQ 20000000 // 20 MHz SPI bus frequency
#endif

// Define color BLACK if not already defined
#ifndef BLACK
#define BLACK 0x0000
#endif

// Global screen rotation variable (0..3 typically)
extern uint8_t screen_rotation;

/*------------------------------------------------------------------------------
 *  DISPLAY DRIVER Prototypes
 *-----------------------------------------------------------------------------*/
#if LVGL_VERSION_MAJOR == 9
void xiao_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
#elif LVGL_VERSION_MAJOR == 8
void xiao_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
#else
#error "Not supported LVGL version (only v8 or v9)."
#endif

void xiao_disp_init(void);
void lv_xiao_disp_init(void);

/*------------------------------------------------------------------------------
 *  TOUCH DRIVER (chsc6x) Prototypes
 *-----------------------------------------------------------------------------*/
bool chsc6x_is_pressed(void);
void chsc6x_convert_xy(uint8_t *x, uint8_t *y);
void chsc6x_get_xy(lv_coord_t *x, lv_coord_t *y);

#if LVGL_VERSION_MAJOR == 9
void chsc6x_read(lv_indev_t *indev, lv_indev_data_t *data);
#elif LVGL_VERSION_MAJOR == 8
void chsc6x_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
#endif

void lv_xiao_touch_init(void);

#endif // LV_XIAO_ROUND_SCREEN_H
