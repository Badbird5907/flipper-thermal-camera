#include "display.h"

#include "font5x3.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdio.h>
#include <string.h>

#define IMAGE_W 96
#define IMAGE_H 48
#define SIDE_X  97
#define CROSS_X 46
#define CROSS_Y 23
#define CROSS_R 4

typedef struct {
    float min;
    float max;
    float spot;
    uint8_t norm[THERMAL_FRAME_PIXELS];
} ThermalStats;

void thermal_display_draw_char(Canvas* canvas, int x, int y, char c) {
    draw_char(canvas, x, y, c);
}

void thermal_display_draw_str(Canvas* canvas, int x, int y, const char* s) {
    while(*s) {
        thermal_display_draw_char(canvas, x, y, *s++);
        x += 4;
    }
}

static void thermal_display_draw_degree(Canvas* canvas, int x, int y) {
    canvas_draw_dot(canvas, x, y);
    canvas_draw_dot(canvas, x + 1, y);
    canvas_draw_dot(canvas, x, y + 1);
    canvas_draw_dot(canvas, x + 1, y + 1);
}

void thermal_display_draw_temp(Canvas* canvas, int x, int y, float temp) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%.1f", (double)temp);
    thermal_display_draw_str(canvas, x, y, buf);
    thermal_display_draw_degree(canvas, x + (int)strlen(buf) * 4, y);
}

static void thermal_display_compute_stats(const float* frame, ThermalStats* stats) {
    stats->min = frame[0];
    stats->max = frame[0];
    const size_t spot_x = CROSS_X / 3U;
    const size_t spot_display_y = CROSS_Y / 2U;
    const size_t spot_sensor_y = THERMAL_FRAME_HEIGHT - 1U - spot_display_y;
    stats->spot = frame[(spot_sensor_y * THERMAL_FRAME_WIDTH) + spot_x];

    for(size_t i = 1; i < THERMAL_FRAME_PIXELS; i++) {
        if(frame[i] < stats->min) {
            stats->min = frame[i];
        }
        if(frame[i] > stats->max) {
            stats->max = frame[i];
        }
    }

    float range = stats->max - stats->min;
    if(range < 0.1f) {
        range = 0.1f;
    }

    for(size_t i = 0; i < THERMAL_FRAME_PIXELS; i++) {
        float scaled = ((frame[i] - stats->min) / range) * 255.0f;
        if(scaled < 0.0f) {
            scaled = 0.0f;
        } else if(scaled > 255.0f) {
            scaled = 255.0f;
        }
        stats->norm[i] = (uint8_t)scaled;
    }
}

static void thermal_display_draw_waiting(Canvas* canvas, const ThermalCameraRenderData* data) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 48, 18, AlignCenter, AlignTop, "MLX90640");
    canvas_draw_str_aligned(
        canvas,
        48,
        32,
        AlignCenter,
        AlignTop,
        data->sensor_present ? "Waiting frame" : "Not detected");
}

static void thermal_display_draw_crosshair(Canvas* canvas, bool blink) {
    if(!blink) {
        return;
    }

    canvas_set_color(canvas, ColorBlack);
    for(int d = -CROSS_R; d <= CROSS_R; d++) {
        if(d == 0) {
            continue;
        }
        canvas_draw_dot(canvas, CROSS_X + d, CROSS_Y);
        canvas_draw_dot(canvas, CROSS_X, CROSS_Y + d);
    }

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, CROSS_X - 1, CROSS_Y);
    canvas_draw_dot(canvas, CROSS_X + 1, CROSS_Y);
    canvas_draw_dot(canvas, CROSS_X, CROSS_Y - 1);
    canvas_draw_dot(canvas, CROSS_X, CROSS_Y + 1);
    canvas_set_color(canvas, ColorBlack);
}

static void thermal_display_draw_image(
    Canvas* canvas,
    const ThermalStats* stats,
    bool iso,
    bool live) {
    for(uint8_t sy = 0; sy < THERMAL_FRAME_HEIGHT; sy++) {
        const uint8_t source_y = THERMAL_FRAME_HEIGHT - 1U - sy;
        for(uint8_t sx = 0; sx < THERMAL_FRAME_WIDTH; sx++) {
            uint8_t value = stats->norm[(source_y * THERMAL_FRAME_WIDTH) + sx];
            const bool iso_pixel = iso && ((value > 108U) && (value < 148U));

            if(live) {
                value = (value > 32U) ? (uint8_t)(value - 32U) : 0U;
            }

            for(uint8_t dy = 0; dy < 2; dy++) {
                for(uint8_t dx = 0; dx < 3; dx++) {
                    const int px = (int)sx * 3 + dx;
                    const int py = (int)sy * 2 + dy;
                    const bool draw = iso_pixel ? (((px + py) & 1) == 0) :
                                                  dither_pixel(px, py, value);
                    if(draw) {
                        canvas_draw_dot(canvas, px, py);
                    }
                }
            }
        }
    }
}

static void thermal_display_draw_fill(Canvas* canvas, int x, int y, float fill) {
    if(fill < 0.0f) {
        fill = 0.0f;
    } else if(fill > 1.0f) {
        fill = 1.0f;
    }

    thermal_display_draw_char(canvas, x, y, '[');
    thermal_display_draw_char(canvas, x + 16, y, ']');
    const uint8_t filled = (uint8_t)(fill * 12.0f);
    for(uint8_t i = 0; i < 12U; i++) {
        if(i < filled) {
            canvas_draw_line(canvas, x + 4 + i, y + 1, x + 4 + i, y + 4);
        } else if((i & 1U) == 0U) {
            canvas_draw_dot(canvas, x + 4 + i, y + 4);
        }
    }
}

static void thermal_display_draw_delta(Canvas* canvas, int x, int y, float delta) {
    canvas_draw_dot(canvas, x + 2, y);
    canvas_draw_dot(canvas, x + 1, y + 1);
    canvas_draw_dot(canvas, x + 3, y + 1);
    canvas_draw_line(canvas, x, y + 4, x + 4, y + 4);

    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f", (double)delta);
    thermal_display_draw_str(canvas, x + 7, y, buf);
}

static void thermal_display_draw_sidebar(
    Canvas* canvas,
    const ThermalCameraRenderData* data,
    const ThermalStats* stats,
    bool iso,
    bool spot) {
    canvas_draw_line(canvas, 96, 0, 96, 47);
    thermal_display_draw_str(canvas, SIDE_X + 1, 1, "HI");
    thermal_display_draw_temp(canvas, SIDE_X + 1, 8, stats->max);

    if(spot) {
        thermal_display_draw_str(canvas, SIDE_X + 1, 17, "SP");
        thermal_display_draw_temp(canvas, SIDE_X + 1, 24, stats->spot);
    } else {
        thermal_display_draw_str(canvas, SIDE_X + 1, 17, iso ? "ISO" : "LO");
        thermal_display_draw_temp(
            canvas, SIDE_X + 1, 24, iso ? ((stats->min + stats->max) * 0.5f) : stats->min);
    }

    const float range = (stats->max - stats->min) < 0.1f ? 0.1f : (stats->max - stats->min);
    thermal_display_draw_fill(canvas, SIDE_X + 1, 34, (stats->spot - stats->min) / range);
    thermal_display_draw_delta(canvas, SIDE_X + 1, 42, range);

    if(data->frozen) {
        canvas_draw_box(canvas, 121, 1, 5, 5);
    }
}

static void thermal_display_draw_battery(Canvas* canvas, int x, int y, uint8_t pct) {
    canvas_draw_frame(canvas, x, y, 16, 8);
    canvas_draw_box(canvas, x + 16, y + 2, 2, 4);

    const uint8_t fill = (uint8_t)((CLAMP(pct, 100U, 0U) * 12U) / 100U);
    if(fill > 0U) {
        canvas_draw_box(canvas, x + 2, y + 2, fill, 4);
    }
}

static void thermal_display_draw_status(Canvas* canvas, const ThermalCameraRenderData* data) {
    static const char* labels[] = {"HEAT", " ISO", "SPOT", "LIVE"};
    char fps[8];

    canvas_draw_line(canvas, 0, 48, 127, 48);
    thermal_display_draw_str(canvas, 2, 50, labels[data->mode]);
    snprintf(fps, sizeof(fps), "%uFPS", data->mode == ThermalCameraModeLive ? 8U : data->refresh_rate_hz);
    if(data->fps > 0.1f) {
        snprintf(fps, sizeof(fps), "%.0fFPS", (double)data->fps);
    }
    thermal_display_draw_str(canvas, 26, 50, fps);
    thermal_display_draw_str(canvas, 56, 50, "MLX");
    thermal_display_draw_battery(canvas, 108, 50, data->battery_pct);
}

static void thermal_display_draw_overlay(Canvas* canvas, const ThermalCameraRenderData* data) {
    if(data->overlay_remaining_ms == 0U) {
        return;
    }

    char buf[12];
    snprintf(buf, sizeof(buf), "E %.2f", (double)data->emissivity);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 35, 19, 58, 16);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 35, 19, 58, 16);
    thermal_display_draw_str(canvas, 41, 25, buf);
}

void thermal_display_render_heatmap(
    Canvas* canvas,
    const ThermalCameraRenderData* data,
    bool iso,
    bool spot,
    bool live) {
    canvas_set_color(canvas, ColorBlack);

    if(data->has_frame) {
        ThermalStats stats;
        thermal_display_compute_stats(data->frame, &stats);
        thermal_display_draw_image(canvas, &stats, iso, live);
        thermal_display_draw_sidebar(canvas, data, &stats, iso, spot);

        if(!live) {
            const bool blink_on = !spot || (((data->tick / 250U) & 1U) == 0U);
            thermal_display_draw_crosshair(canvas, blink_on);
        }
    } else {
        thermal_display_draw_waiting(canvas, data);
        canvas_draw_line(canvas, 96, 0, 96, 47);
        thermal_display_draw_str(canvas, SIDE_X + 1, 1, "HI");
        thermal_display_draw_str(canvas, SIDE_X + 1, 17, "LO");
    }

    thermal_display_draw_status(canvas, data);
    thermal_display_draw_overlay(canvas, data);
}

void thermal_display_render_mode(Canvas* canvas, const ThermalCameraRenderData* data) {
    switch(data->mode) {
    case ThermalCameraModeIso:
        thermal_camera_view_iso(canvas, data);
        break;
    case ThermalCameraModeSpot:
        thermal_camera_view_spot(canvas, data);
        break;
    case ThermalCameraModeLive:
        thermal_camera_view_scan(canvas, data);
        break;
    case ThermalCameraModeHeat:
    default:
        thermal_camera_view_heatmap(canvas, data);
        break;
    }
}
