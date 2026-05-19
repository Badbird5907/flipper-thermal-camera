#include "../display.h"

void thermal_camera_view_heatmap(Canvas* canvas, const ThermalCameraRenderData* data) {
    thermal_display_render_heatmap(canvas, data, false, false, false);
}
