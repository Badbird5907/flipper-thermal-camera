#pragma once

#include <gui/canvas.h>
#include <stdbool.h>
#include <stdint.h>

#define THERMAL_FRAME_WIDTH  32U
#define THERMAL_FRAME_HEIGHT 24U
#define THERMAL_FRAME_PIXELS 768U

typedef enum {
    ThermalCameraModeHeat,
    ThermalCameraModeIso,
    ThermalCameraModeSpot,
    ThermalCameraModeLive,
    ThermalCameraModeCount,
} ThermalCameraMode;

typedef struct {
    float frame[THERMAL_FRAME_PIXELS];
    bool has_frame;
    bool sensor_present;
    bool frozen;
    ThermalCameraMode mode;
    float emissivity;
    uint8_t refresh_rate_hz;
    uint8_t battery_pct;
    uint32_t last_frame_age_ms;
    uint32_t overlay_remaining_ms;
    uint32_t tick;
    float fps;
} ThermalCameraRenderData;

static const uint8_t BAYER4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

static inline bool dither_pixel(int px, int py, uint8_t t) {
    return t > (BAYER4[py & 3][px & 3] * 17);
}

void thermal_display_draw_char(Canvas* canvas, int x, int y, char c);
void thermal_display_draw_str(Canvas* canvas, int x, int y, const char* s);
void thermal_display_draw_temp(Canvas* canvas, int x, int y, float temp);
void thermal_display_render_mode(Canvas* canvas, const ThermalCameraRenderData* data);
void thermal_display_render_heatmap(
    Canvas* canvas,
    const ThermalCameraRenderData* data,
    bool iso,
    bool spot,
    bool live);

void thermal_camera_view_heatmap(Canvas* canvas, const ThermalCameraRenderData* data);
void thermal_camera_view_iso(Canvas* canvas, const ThermalCameraRenderData* data);
void thermal_camera_view_spot(Canvas* canvas, const ThermalCameraRenderData* data);
void thermal_camera_view_scan(Canvas* canvas, const ThermalCameraRenderData* data);
