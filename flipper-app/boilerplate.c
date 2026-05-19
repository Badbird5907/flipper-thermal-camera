#include "boilerplate.h"

typedef enum {
    ThermalCameraViewMain,
} ThermalCameraView;

typedef struct {
    bool probe_complete;
    bool sensor_present;
} ThermalCameraModel;

static bool thermal_camera_probe_sensor(void) {
    bool ready;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    ready = furi_hal_i2c_is_device_ready(
        &furi_hal_i2c_handle_external, MLX90640_I2C_ADDRESS, MLX90640_PROBE_TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return ready;
}

static void thermal_camera_draw_callback(Canvas* canvas, void* model) {
    furi_assert(model);
    ThermalCameraModel* view_model = model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Thermal Camera");

    canvas_set_font(canvas, FontSecondary);

    if(!view_model->probe_complete) {
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Powering 5V rail");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Checking MLX90640...");
    } else if(view_model->sensor_present) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "MLX90640 detected");
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, "Ready");
    } else {
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Please insert");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "MLX90640 device");
    }

    elements_button_left(canvas, "Back");
}

static bool thermal_camera_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    ThermalCameraApp* app = context;

    if((event->type == InputTypeShort) && (event->key == InputKeyBack)) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    return false;
}

static void thermal_camera_tick_callback(void* context) {
    furi_assert(context);
    ThermalCameraApp* app = context;

    if(app->probe_complete) {
        return;
    }

    app->sensor_present = thermal_camera_probe_sensor();
    app->probe_attempt++;

    if(app->sensor_present || (app->probe_attempt > MLX90640_PROBE_RETRIES)) {
        app->probe_complete = true;
    }

    with_view_model(
        app->view,
        ThermalCameraModel * model,
        {
            model->probe_complete = app->probe_complete;
            model->sensor_present = app->sensor_present;
        },
        true);
}

static ThermalCameraApp* thermal_camera_app_alloc(void) {
    ThermalCameraApp* app = malloc(sizeof(ThermalCameraApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    app->probe_attempt = 0;
    app->probe_complete = false;
    app->sensor_present = false;

    notification_message(app->notification, &sequence_display_backlight_on);

    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(ThermalCameraModel));
    with_view_model(
        app->view,
        ThermalCameraModel * model,
        {
            model->probe_complete = app->probe_complete;
            model->sensor_present = app->sensor_present;
        },
        false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, thermal_camera_draw_callback);
    view_set_input_callback(app->view, thermal_camera_input_callback);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, thermal_camera_tick_callback, 1000);
    view_dispatcher_add_view(app->view_dispatcher, ThermalCameraViewMain, app->view);

    return app;
}

static void thermal_camera_app_free(ThermalCameraApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, ThermalCameraViewMain);
    view_free_model(app->view);
    view_free(app->view);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t thermal_camera_app(void* p) {
    UNUSED(p);
    ThermalCameraApp* app = thermal_camera_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, ThermalCameraViewMain);

    furi_hal_power_suppress_charge_enter();
    furi_hal_power_enable_otg();

    view_dispatcher_run(app->view_dispatcher);

    furi_hal_power_disable_otg();
    furi_hal_power_suppress_charge_exit();
    thermal_camera_app_free(app);

    return 0;
}
