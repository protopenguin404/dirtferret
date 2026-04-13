#pragma once

#include "engine/region.h"  // Point, ElementInfo, DirtyRect
#include "include/cef_browser.h"
#include "include/cef_devtools_message_observer.h"
#include "include/cef_registration.h"
#include "include/cef_values.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

class DomBridge : public CefDevToolsMessageObserver {
public:
    explicit DomBridge(CefRefPtr<CefBrowser> browser);

    // --- Queries (async, callback-based) ---
    void element_at(int x, int y, std::function<void(ElementInfo)> callback);
    void query(const std::string& selector,
               std::function<void(std::vector<ElementInfo>)> callback);
    void bounds(int node_id, std::function<void(DirtyRect)> callback);
    void text(int node_id, std::function<void(std::string)> callback);

    // --- Visual rendering ---
    void highlight_node(int node_id, uint8_t r, uint8_t g, uint8_t b, float a);
    void clear_highlight();
    void inject_cursor_visual(uint32_t cursor_id, int x, int y);
    void remove_cursor_visual(uint32_t cursor_id);
    void clear_all_visuals();

    // --- Document update callback ---
    void set_on_document_updated(std::function<void()> cb) { on_document_updated_ = std::move(cb); }

    // --- Lifecycle ---
    void enable();   // DOM.enable() + Overlay.enable() + cache root nodeId
    void disable();

    // --- CefDevToolsMessageObserver ---
    bool OnDevToolsMessage(CefRefPtr<CefBrowser> browser,
                           const void* message, size_t message_size) override;
    void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                                int message_id, bool success,
                                const void* result, size_t result_size) override;
    void OnDevToolsEvent(CefRefPtr<CefBrowser> browser,
                         const CefString& method,
                         const void* params, size_t params_size) override;
    void OnDevToolsAgentAttached(CefRefPtr<CefBrowser> browser) override;
    void OnDevToolsAgentDetached(CefRefPtr<CefBrowser> browser) override;

private:
    using CdpCallback = std::function<void(bool success, CefRefPtr<CefDictionaryValue> result)>;

    int send_cdp(const std::string& method, CefRefPtr<CefDictionaryValue> params,
                 CdpCallback callback);
    int send_cdp(const std::string& method, CdpCallback callback);

    // Ensure root_node_id_ is cached, then invoke continuation.
    void ensure_root(std::function<void()> continuation);

    // Parse CDP box model quad into DirtyRect (bounding box of the quad).
    static DirtyRect box_model_to_rect(CefRefPtr<CefListValue> content_quad);

    CefRefPtr<CefBrowser> browser_;
    CefRefPtr<CefRegistration> registration_;
    int next_message_id_ = 1;
    int root_node_id_ = -1;
    std::map<int, CdpCallback> pending_;
    std::function<void()> on_document_updated_;

    IMPLEMENT_REFCOUNTING(DomBridge);
};
