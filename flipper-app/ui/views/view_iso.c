#include "../display.h"

void thermal_camera_view_iso(Canvas* canvas, const ThermalCameraRenderData* data) {
    thermal_display_render_heatmap(canvas, data, true, false, false);
}
