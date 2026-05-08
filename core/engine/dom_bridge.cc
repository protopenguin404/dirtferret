#include "engine/dom_bridge.h"

#include "include/cef_parser.h"

#include <iostream>
#include <sstream>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

DomBridge::DomBridge(CefRefPtr<CefBrowser> browser)
    : browser_(browser) {}

// ---------------------------------------------------------------------------
// CDP transport
// ---------------------------------------------------------------------------

int DomBridge::send_cdp(const std::string& method,
                        CefRefPtr<CefDictionaryValue> params,
                        CdpCallback callback) {
    int id = next_message_id_++;
    if (callback) {
        pending_[id] = std::move(callback);
    }
    browser_->GetHost()->ExecuteDevToolsMethod(id, method, params);
    return id;
}

int DomBridge::send_cdp(const std::string& method, CdpCallback callback) {
    return send_cdp(method, nullptr, std::move(callback));
}

// ---------------------------------------------------------------------------
// CefDevToolsMessageObserver overrides
// ---------------------------------------------------------------------------

bool DomBridge::OnDevToolsMessage(CefRefPtr<CefBrowser> /*browser*/,
                                  const void* /*message*/,
                                  size_t /*message_size*/) {
    // Let CEF split into OnDevToolsMethodResult / OnDevToolsEvent.
    return false;
}

void DomBridge::OnDevToolsMethodResult(CefRefPtr<CefBrowser> /*browser*/,
                                       int message_id, bool success,
                                       const void* result,
                                       size_t result_size) {
    auto it = pending_.find(message_id);
    if (it == pending_.end()) {
        return;
    }

    CdpCallback cb = std::move(it->second);
    pending_.erase(it);

    CefRefPtr<CefDictionaryValue> dict;
    if (result && result_size > 0) {
        CefRefPtr<CefValue> parsed =
            CefParseJSON(result, result_size, JSON_PARSER_RFC);
        if (parsed && parsed->GetType() == VTYPE_DICTIONARY) {
            dict = parsed->GetDictionary();
        }
    }

    cb(success, dict);
}

void DomBridge::OnDevToolsEvent(CefRefPtr<CefBrowser> /*browser*/,
                                const CefString& method,
                                const void* /*params*/,
                                size_t /*params_size*/) {
    if (method == "DOM.documentUpdated") {
        std::cerr << "[dom] document updated, invalidating root nodeId\n";
        root_node_id_ = -1;
        // Re-fetch root so subsequent queries work.
        ensure_root([](){});
        if (on_document_updated_) {
            on_document_updated_();
        }
    }
}

void DomBridge::OnDevToolsAgentAttached(CefRefPtr<CefBrowser> /*browser*/) {
    std::cerr << "[dom] DevTools agent attached\n";
}

void DomBridge::OnDevToolsAgentDetached(CefRefPtr<CefBrowser> /*browser*/) {
    std::cerr << "[dom] DevTools agent detached, clearing pending callbacks\n";
    pending_.clear();
    root_node_id_ = -1;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void DomBridge::enable() {
    registration_ = browser_->GetHost()->AddDevToolsMessageObserver(this);

    send_cdp("DOM.enable", nullptr);
    send_cdp("Overlay.enable", nullptr);

    // Fetch and cache root node ID.
    auto params = CefDictionaryValue::Create();
    params->SetInt("depth", 0);
    send_cdp("DOM.getDocument", params,
        [this](bool success, CefRefPtr<CefDictionaryValue> result) {
            if (!success || !result) {
                std::cerr << "[dom] DOM.getDocument failed\n";
                return;
            }
            if (result->HasKey("root")) {
                auto root = result->GetDictionary("root");
                if (root && root->HasKey("nodeId")) {
                    root_node_id_ = root->GetInt("nodeId");
                    std::cerr << "[dom] cached root nodeId=" << root_node_id_ << "\n";
                }
            }
        });
}

void DomBridge::disable() {
    send_cdp("Overlay.disable", nullptr);
    send_cdp("DOM.disable", nullptr);

    // Release observer registration.
    registration_ = nullptr;
    pending_.clear();
    root_node_id_ = -1;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void DomBridge::ensure_root(std::function<void()> continuation) {
    if (root_node_id_ >= 0) {
        continuation();
        return;
    }
    auto params = CefDictionaryValue::Create();
    params->SetInt("depth", 0);
    send_cdp("DOM.getDocument", params,
        [this, cont = std::move(continuation)](
            bool success, CefRefPtr<CefDictionaryValue> result) {
            if (success && result && result->HasKey("root")) {
                auto root = result->GetDictionary("root");
                if (root && root->HasKey("nodeId")) {
                    root_node_id_ = root->GetInt("nodeId");
                }
            }
            if (root_node_id_ < 0) {
                std::cerr << "[dom] failed to fetch root nodeId\n";
            }
            cont();
        });
}

DirtyRect DomBridge::box_model_to_rect(CefRefPtr<CefListValue> content_quad) {
    // CDP content quad: 8 doubles [x1,y1, x2,y2, x3,y3, x4,y4]
    if (!content_quad || content_quad->GetSize() < 8) {
        return {0, 0, 0, 0};
    }

    double min_x = content_quad->GetDouble(0);
    double min_y = content_quad->GetDouble(1);
    double max_x = min_x;
    double max_y = min_y;

    for (size_t i = 1; i < 4; ++i) {
        double px = content_quad->GetDouble(i * 2);
        double py = content_quad->GetDouble(i * 2 + 1);
        if (px < min_x) min_x = px;
        if (py < min_y) min_y = py;
        if (px > max_x) max_x = px;
        if (py > max_y) max_y = py;
    }

    DirtyRect rect;
    rect.x = static_cast<int32_t>(min_x);
    rect.y = static_cast<int32_t>(min_y);
    rect.width = static_cast<uint32_t>(max_x - min_x + 0.5);
    rect.height = static_cast<uint32_t>(max_y - min_y + 0.5);
    return rect;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

void DomBridge::element_at(int x, int y,
                           std::function<void(ElementInfo)> callback) {
    auto params = CefDictionaryValue::Create();
    params->SetInt("x", x);
    params->SetInt("y", y);

    send_cdp("DOM.getNodeForLocation", params,
        [this, cb = std::move(callback)](
            bool success, CefRefPtr<CefDictionaryValue> result) {
            if (!success || !result || !result->HasKey("nodeId")) {
                std::cerr << "[dom] DOM.getNodeForLocation failed\n";
                ElementInfo empty;
                cb(empty);
                return;
            }

            int node_id = result->GetInt("nodeId");

            // Step 2: get box model for bounds
            auto box_params = CefDictionaryValue::Create();
            box_params->SetInt("nodeId", node_id);

            send_cdp("DOM.getBoxModel", box_params,
                [this, node_id, cb2 = std::move(cb)](
                    bool box_ok, CefRefPtr<CefDictionaryValue> box_result) {
                    DirtyRect rect = {0, 0, 0, 0};
                    if (box_ok && box_result && box_result->HasKey("model")) {
                        auto model = box_result->GetDictionary("model");
                        if (model && model->HasKey("content")) {
                            rect = box_model_to_rect(model->GetList("content"));
                        }
                    }

                    // Step 3: describe node for tag + attributes
                    auto desc_params = CefDictionaryValue::Create();
                    desc_params->SetInt("nodeId", node_id);

                    send_cdp("DOM.describeNode", desc_params,
                        [node_id, rect, cb3 = std::move(cb2)](
                            bool desc_ok,
                            CefRefPtr<CefDictionaryValue> desc_result) {
                            ElementInfo info;
                            info.node_id = node_id;
                            info.bounds_x = rect.x;
                            info.bounds_y = rect.y;
                            info.bounds_width = rect.width;
                            info.bounds_height = rect.height;

                            if (desc_ok && desc_result &&
                                desc_result->HasKey("node")) {
                                auto node = desc_result->GetDictionary("node");
                                if (node) {
                                    if (node->HasKey("nodeName")) {
                                        info.tag = node->GetString("nodeName")
                                                       .ToString();
                                    }
                                    if (node->HasKey("attributes")) {
                                        auto attrs = node->GetList("attributes");
                                        if (attrs) {
                                            // Attributes are [name, value, name, value, ...]
                                            for (size_t i = 0;
                                                 i + 1 < attrs->GetSize();
                                                 i += 2) {
                                                info.attributes.push_back(
                                                    {attrs->GetString(i).ToString(),
                                                     attrs->GetString(i + 1)
                                                         .ToString()});
                                            }
                                        }
                                    }
                                }
                            }

                            cb3(info);
                        });
                });
        });
}

void DomBridge::query(const std::string& selector,
                      std::function<void(std::vector<ElementInfo>)> callback) {
    ensure_root([this, selector, cb = std::move(callback)]() {
        if (root_node_id_ < 0) {
            std::cerr << "[dom] query: no root node, returning empty\n";
            cb({});
            return;
        }

        auto params = CefDictionaryValue::Create();
        params->SetInt("nodeId", root_node_id_);
        params->SetString("selector", selector);

        send_cdp("DOM.querySelectorAll", params,
            [this, cb2 = std::move(cb)](
                bool success, CefRefPtr<CefDictionaryValue> result) {
                if (!success || !result || !result->HasKey("nodeIds")) {
                    std::cerr << "[dom] DOM.querySelectorAll failed\n";
                    cb2({});
                    return;
                }

                auto node_ids_list = result->GetList("nodeIds");
                if (!node_ids_list || node_ids_list->GetSize() == 0) {
                    cb2({});
                    return;
                }

                // Collect node IDs into a vector.
                auto node_ids = std::make_shared<std::vector<int>>();
                for (size_t i = 0; i < node_ids_list->GetSize(); ++i) {
                    node_ids->push_back(node_ids_list->GetInt(i));
                }

                // Process sequentially: resolve each nodeId into ElementInfo.
                auto results = std::make_shared<std::vector<ElementInfo>>();
                auto idx = std::make_shared<size_t>(0);

                // Recursive lambda via shared_ptr to self.
                auto process_next = std::make_shared<std::function<void()>>();
                *process_next = [this, node_ids, results, idx, cb3 = std::move(cb2),
                                 process_next]() {
                    if (*idx >= node_ids->size()) {
                        cb3(*results);
                        return;
                    }

                    int nid = (*node_ids)[*idx];
                    (*idx)++;

                    // Get box model
                    auto box_params = CefDictionaryValue::Create();
                    box_params->SetInt("nodeId", nid);

                    send_cdp("DOM.getBoxModel", box_params,
                        [this, nid, results, process_next](
                            bool box_ok,
                            CefRefPtr<CefDictionaryValue> box_result) {
                            DirtyRect rect = {0, 0, 0, 0};
                            if (box_ok && box_result &&
                                box_result->HasKey("model")) {
                                auto model =
                                    box_result->GetDictionary("model");
                                if (model && model->HasKey("content")) {
                                    rect = box_model_to_rect(
                                        model->GetList("content"));
                                }
                            }

                            // Describe node
                            auto desc_params = CefDictionaryValue::Create();
                            desc_params->SetInt("nodeId", nid);

                            send_cdp("DOM.describeNode", desc_params,
                                [nid, rect, results, process_next](
                                    bool desc_ok,
                                    CefRefPtr<CefDictionaryValue>
                                        desc_result) {
                                    ElementInfo info;
                                    info.node_id = nid;
                                    info.bounds_x = rect.x;
                                    info.bounds_y = rect.y;
                                    info.bounds_width = rect.width;
                                    info.bounds_height = rect.height;

                                    if (desc_ok && desc_result &&
                                        desc_result->HasKey("node")) {
                                        auto node =
                                            desc_result->GetDictionary(
                                                "node");
                                        if (node) {
                                            if (node->HasKey("nodeName")) {
                                                info.tag =
                                                    node->GetString(
                                                            "nodeName")
                                                        .ToString();
                                            }
                                            if (node->HasKey("attributes")) {
                                                auto attrs =
                                                    node->GetList(
                                                        "attributes");
                                                if (attrs) {
                                                    for (size_t i = 0;
                                                         i + 1 <
                                                         attrs->GetSize();
                                                         i += 2) {
                                                        info.attributes
                                                            .push_back(
                                                                {attrs
                                                                     ->GetString(
                                                                           i)
                                                                     .ToString(),
                                                                 attrs
                                                                     ->GetString(
                                                                           i +
                                                                           1)
                                                                     .ToString()});
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    results->push_back(info);
                                    (*process_next)();
                                });
                        });
                };

                (*process_next)();
            });
    });
}

void DomBridge::bounds(int node_id,
                       std::function<void(DirtyRect)> callback) {
    auto params = CefDictionaryValue::Create();
    params->SetInt("nodeId", node_id);

    send_cdp("DOM.getBoxModel", params,
        [cb = std::move(callback)](
            bool success, CefRefPtr<CefDictionaryValue> result) {
            if (!success || !result || !result->HasKey("model")) {
                std::cerr << "[dom] DOM.getBoxModel failed\n";
                cb({0, 0, 0, 0});
                return;
            }
            auto model = result->GetDictionary("model");
            if (!model || !model->HasKey("content")) {
                cb({0, 0, 0, 0});
                return;
            }
            cb(box_model_to_rect(model->GetList("content")));
        });
}

void DomBridge::text(int node_id,
                     std::function<void(std::string)> callback) {
    auto params = CefDictionaryValue::Create();
    params->SetInt("nodeId", node_id);

    send_cdp("DOM.getOuterHTML", params,
        [cb = std::move(callback)](
            bool success, CefRefPtr<CefDictionaryValue> result) {
            if (!success || !result || !result->HasKey("outerHTML")) {
                std::cerr << "[dom] DOM.getOuterHTML failed\n";
                cb("");
                return;
            }
            cb(result->GetString("outerHTML").ToString());
        });
}

// ---------------------------------------------------------------------------
// Visual rendering
// ---------------------------------------------------------------------------

void DomBridge::highlight_node(int node_id, uint8_t r, uint8_t g, uint8_t b,
                               float a) {
    // Build highlightConfig
    auto color = CefDictionaryValue::Create();
    color->SetInt("r", r);
    color->SetInt("g", g);
    color->SetInt("b", b);
    color->SetDouble("a", static_cast<double>(a));

    auto config = CefDictionaryValue::Create();
    config->SetBool("showInfo", false);
    config->SetDictionary("contentColor", color);

    auto params = CefDictionaryValue::Create();
    params->SetInt("nodeId", node_id);
    params->SetDictionary("highlightConfig", config);

    send_cdp("Overlay.highlightNode", params, nullptr);
}

void DomBridge::clear_highlight() {
    send_cdp("Overlay.hideHighlight", nullptr);
}

void DomBridge::inject_cursor_visual(uint32_t cursor_id, int x, int y) {
    std::ostringstream js;
    js << "(function(){"
       << "var id='dirtferret-cursor-" << cursor_id << "';"
       << "var el=document.getElementById(id);"
       << "if(!el){"
       << "el=document.createElement('div');"
       << "el.id=id;"
       << "el.style.cssText='position:absolute;border:2px solid #ff6600;"
       << "pointer-events:none;z-index:2147483647;';"
       << "document.documentElement.appendChild(el);"
       << "}"
       << "el.style.left='" << x << "px';"
       << "el.style.top='" << y << "px';"
       << "el.style.width='2px';"
       << "el.style.height='16px';"
       << "})()";

    auto params = CefDictionaryValue::Create();
    params->SetString("expression", js.str());

    send_cdp("Runtime.evaluate", params, nullptr);
}

void DomBridge::remove_cursor_visual(uint32_t cursor_id) {
    std::ostringstream js;
    js << "(function(){"
       << "var el=document.getElementById('dirtferret-cursor-" << cursor_id << "');"
       << "if(el)el.remove();"
       << "})()";

    auto params = CefDictionaryValue::Create();
    params->SetString("expression", js.str());

    send_cdp("Runtime.evaluate", params, nullptr);
}

void DomBridge::clear_all_visuals() {
    clear_highlight();

    std::string js =
        "(function(){"
        "var els=document.querySelectorAll('[id^=\"dirtferret-cursor-\"]');"
        "for(var i=0;i<els.length;i++)els[i].remove();"
        "})()";

    auto params = CefDictionaryValue::Create();
    params->SetString("expression", js);

    send_cdp("Runtime.evaluate", params, nullptr);
}
