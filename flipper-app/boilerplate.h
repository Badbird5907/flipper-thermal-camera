#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <notification/notification_messages.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <stdlib.h>

#define TAG "ThermalCamera"

#define MLX90640_I2C_ADDRESS 0x33
#define MLX90640_PROBE_RETRIES 2
#define MLX90640_PROBE_TIMEOUT_MS 100

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    View* view;
    uint8_t probe_attempt;
    bool probe_complete;
    bool sensor_present;
} ThermalCameraApp;
