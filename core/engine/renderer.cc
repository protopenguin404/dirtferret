#include "engine/renderer.h"

void Renderer::set_view_size(int width, int height) {
    view_width_ = width;
    view_height_ = height;
}

void Renderer::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect(0, 0, view_width_, view_height_);
}

void Renderer::OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer,
                       int width, int height) {
    // Only handle full-page paints, not popups
    if (type != PET_VIEW) return;

    if (paint_callback_) {
        paint_callback_(buffer, width, height, dirtyRects);
    }
}
