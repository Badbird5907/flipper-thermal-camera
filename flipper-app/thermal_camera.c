#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>

#include "mlx90640/mlx90640_i2c.h"
#include "ui/display.h"

#define TAG "ThermalCamera"

#define THERMAL_CAMERA_VIEW_MAIN       0U
#define THERMAL_CAMERA_REFRESH_4HZ_REG 0x03U
#define THERMAL_CAMERA_REFRESH_8HZ_REG 0x04U
#define THERMAL_CAMERA_OVERLAY_MS      2000U
#define THERMAL_CAMERA_UI_TICK_MS      125U

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    View* view;
    FuriThread* sensor_thread;
    FuriMutex* frame_mutex;

    float frame[THERMAL_FRAME_PIXELS];
    bool has_frame;
    bool sensor_present;
    bool frozen;
    volatile bool stop_requested;
    ThermalCameraMode mode;
    float emissivity;
    uint8_t refresh_rate_hz;
    uint8_t refresh_rate_reg;
    float fps;
    uint32_t last_frame_tick;
    uint32_t overlay_until_tick;
} ThermalCameraApp;

static uint32_t thermal_camera_tick_to_ms(uint32_t tick) {
    return (tick * 1000U) / furi_kernel_get_tick_frequency();
}

static uint8_t thermal_camera_hz_to_reg(uint8_t hz) {
    return (hz >= 8U) ? THERMAL_CAMERA_REFRESH_8HZ_REG : THERMAL_CAMERA_REFRESH_4HZ_REG;
}

static uint8_t thermal_camera_reg_to_hz(uint8_t reg) {
    return (reg >= THERMAL_CAMERA_REFRESH_8HZ_REG) ? 8U : 4U;
}

static uint8_t thermal_camera_effective_refresh_reg(const ThermalCameraApp* app) {
    return (app->mode == ThermalCameraModeLive) ? THERMAL_CAMERA_REFRESH_8HZ_REG :
                                                  app->refresh_rate_reg;
}

static void thermal_camera_request_redraw(ThermalCameraApp* app) {
    ThermalCameraApp** model = view_get_model(app->view);
    UNUSED(model);
    view_commit_model(app->view, true);
}

static void thermal_camera_set_sensor_present(ThermalCameraApp* app, bool present) {
    furi_check(furi_mutex_acquire(app->frame_mutex, FuriWaitForever) == FuriStatusOk);
    app->sensor_present = present;
    if(!present) {
        app->has_frame = false;
        app->fps = 0.0f;
    }
    furi_check(furi_mutex_release(app->frame_mutex) == FuriStatusOk);
}

static bool thermal_camera_init_sensor(ThermalCameraApp* app) {
    uint16_t eeprom[MLX90640_EEPROM_DUMP_NUM];
    paramsMLX90640 params;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    const bool ready = furi_hal_i2c_is_device_ready(
        &furi_hal_i2c_handle_external, (uint8_t)(MLX90640_I2C_ADDRESS << 1), 100);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(!ready) {
        thermal_camera_set_sensor_present(app, false);
        return false;
    }

    if(!mlx90640_read_eeprom(eeprom) || !mlx90640_extract_params(eeprom, &params)) {
        thermal_camera_set_sensor_present(app, false);
        return false;
    }

    furi_check(furi_mutex_acquire(app->frame_mutex, FuriWaitForever) == FuriStatusOk);
    const uint8_t refresh = thermal_camera_effective_refresh_reg(app);
    const float emissivity = app->emissivity;
    furi_check(furi_mutex_release(app->frame_mutex) == FuriStatusOk);

    mlx90640_set_emissivity(emissivity);
    if(!mlx90640_configure(refresh)) {
        thermal_camera_set_sensor_present(app, false);
        return false;
    }

    thermal_camera_set_sensor_present(app, true);
    return true;
}

static int32_t thermal_camera_sensor_thread(void* context) {
    ThermalCameraApp* app = context;
    float local_frame[THERMAL_FRAME_PIXELS];
    bool initialized = false;
    uint8_t configured_refresh = 0U;

    while(!app->stop_requested) {
        if(!initialized) {
            initialized = thermal_camera_init_sensor(app);
            configured_refresh = 0U;
            if(!initialized) {
                furi_delay_ms(1000U);
                continue;
            }
        }

        furi_check(furi_mutex_acquire(app->frame_mutex, FuriWaitForever) == FuriStatusOk);
        const bool frozen = app->frozen;
        const uint8_t refresh = thermal_camera_effective_refresh_reg(app);
        const float emissivity = app->emissivity;
        furi_check(furi_mutex_release(app->frame_mutex) == FuriStatusOk);

        if(refresh != configured_refresh) {
            if(!mlx90640_configure(refresh)) {
                initialized = false;
                continue;
            }
            configured_refresh = refresh;
        }

        mlx90640_set_emissivity(emissivity);

        if(frozen) {
            furi_delay_ms(50U);
            continue;
        }

        if(!mlx90640_read_frame(local_frame)) {
            initialized = false;
            thermal_camera_set_sensor_present(app, false);
            continue;
        }

        const uint32_t now = furi_get_tick();

        furi_check(furi_mutex_acquire(app->frame_mutex, FuriWaitForever) == FuriStatusOk);
        memcpy(app->frame, local_frame, sizeof(app->frame));
        app->has_frame = true;
        app->sensor_present = true;
        if(app->last_frame_tick != 0U) {
            const uint32_t delta = now - app->last_frame_tick;
            if(delta > 0U) {
                app->fps = (float)furi_kernel_get_tick_frequency() / (float)delta;
            }
        }
        app->last_frame_tick = now;
        furi_check(furi_mutex_release(app->frame_mutex) == FuriStatusOk);

        furi_delay_ms(thermal_camera_reg_to_hz(configured_refresh) >= 8U ? 125U : 250U);
    }

    return 0;
}

static void thermal_camera_draw_callback(Canvas* canvas, void* model) {
    furi_assert(model);

    ThermalCameraApp* app = *(ThermalCameraApp**)model;
    ThermalCameraRenderData data;
    memset(&data, 0, sizeof(data));

    const uint32_t now_tick = furi_get_tick();
    furi_check(furi_mutex_acquire(app->frame_mutex, FuriWaitForever) == FuriStatusOk);
    memcpy(data.frame, app->frame, sizeof(data.frame));
    data.has_frame = app->has_frame;
    data.sensor_present = app->sensor_present;
    data.frozen = app->frozen;
    data.mode = app->mode;
    data.emissivity = app->emissivity;
    data.refresh_rate_hz = thermal_camera_reg_to_hz(thermal_camera_effective_refresh_reg(app));
    data.battery_pct = furi_hal_power_get_pct();
    data.fps = app->fps;
    data.tick = thermal_camera_tick_to_ms(now_tick);
    if(app->last_frame_tick != 0U) {
        data.last_frame_age_ms = thermal_camera_tick_to_ms(now_tick - app->last_frame_tick);
    }
    if(app->overlay_until_tick > now_tick) {
        data.overlay_remaining_ms = thermal_camera_tick_to_ms(app->overlay_until_tick - now_tick);
    }
    furi_check(furi_mutex_release(app->frame_mutex) == FuriStatusOk);

    canvas_clear(canvas);
    thermal_display_render_mode(canvas, &data);
}

static bool thermal_camera_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    ThermalCameraApp* app = context;

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    bool handled = false;
    const uint32_t now = furi_get_tick();

    furi_check(furi_mutex_acquire(app->frame_mutex, FuriWaitForever) == FuriStatusOk);
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            app->mode = (ThermalCameraMode)((app->mode + 1U) % ThermalCameraModeCount);
            handled = true;
        } else if(event->key == InputKeyUp) {
            app->emissivity += 0.01f;
            if(app->emissivity > 1.0f) {
                app->emissivity = 1.0f;
            }
            app->overlay_until_tick = now + furi_ms_to_ticks(THERMAL_CAMERA_OVERLAY_MS);
            handled = true;
        } else if(event->key == InputKeyDown) {
            app->emissivity -= 0.01f;
            if(app->emissivity < 0.10f) {
                app->emissivity = 0.10f;
            }
            app->overlay_until_tick = now + furi_ms_to_ticks(THERMAL_CAMERA_OVERLAY_MS);
            handled = true;
        } else if(event->key == InputKeyLeft || event->key == InputKeyRight) {
            app->refresh_rate_hz = (app->refresh_rate_hz == 4U) ? 8U : 4U;
            app->refresh_rate_reg = thermal_camera_hz_to_reg(app->refresh_rate_hz);
            handled = true;
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        app->frozen = !app->frozen;
        handled = true;
    }
    furi_check(furi_mutex_release(app->frame_mutex) == FuriStatusOk);

    if(handled) {
        thermal_camera_request_redraw(app);
    }

    return handled;
}

static void thermal_camera_tick_callback(void* context) {
    furi_assert(context);
    thermal_camera_request_redraw(context);
}

static ThermalCameraApp* thermal_camera_app_alloc(void) {
    ThermalCameraApp* app = malloc(sizeof(ThermalCameraApp));
    furi_check(app);
    memset(app, 0, sizeof(ThermalCameraApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->frame_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->view_dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    app->emissivity = 0.95f;
    app->refresh_rate_hz = 4U;
    app->refresh_rate_reg = THERMAL_CAMERA_REFRESH_4HZ_REG;
    app->mode = ThermalCameraModeHeat;

    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(ThermalCameraApp*));
    ThermalCameraApp** model = view_get_model(app->view);
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, thermal_camera_draw_callback);
    view_set_input_callback(app->view, thermal_camera_input_callback);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, thermal_camera_tick_callback, THERMAL_CAMERA_UI_TICK_MS);
    view_dispatcher_add_view(app->view_dispatcher, THERMAL_CAMERA_VIEW_MAIN, app->view);

    app->sensor_thread = furi_thread_alloc_ex(
        "MLX90640Worker", 4096U, thermal_camera_sensor_thread, app);

    return app;
}

static void thermal_camera_app_free(ThermalCameraApp* app) {
    furi_assert(app);

    if(app->sensor_thread) {
        furi_thread_free(app->sensor_thread);
    }

    view_dispatcher_remove_view(app->view_dispatcher, THERMAL_CAMERA_VIEW_MAIN);
    view_free_model(app->view);
    view_free(app->view);
    view_dispatcher_free(app->view_dispatcher);

    furi_mutex_free(app->frame_mutex);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t thermal_camera_app(void* p) {
    UNUSED(p);

    ThermalCameraApp* app = thermal_camera_app_alloc();

    notification_message(app->notification, &sequence_display_backlight_on);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, THERMAL_CAMERA_VIEW_MAIN);

    furi_hal_power_suppress_charge_enter();
    furi_hal_power_enable_otg();

    furi_thread_start(app->sensor_thread);
    view_dispatcher_run(app->view_dispatcher);

    app->stop_requested = true;
    furi_thread_join(app->sensor_thread);

    furi_hal_power_disable_otg();
    furi_hal_power_suppress_charge_exit();

    thermal_camera_app_free(app);

    return 0;
}
