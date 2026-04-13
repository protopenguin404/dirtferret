#include "engine/engine.h"
#include "engine/region.h"
#include "lua/runtime.h"
#include "schema/core.capnp.h"
#include <capnp/ez-rpc.h>
#include <capnp/rpc-twoparty.h>
#include <kj/async-io.h>
#include <kj/timer.h>

#include <iostream>
#include <memory>

extern "C" {
#include <lua.h>
}

// --- RPC server implementation ---

class CoreImpl final : public Core::Server {
private:
  Engine &engine_;
  kj::Maybe<Ui::Client> ui_;
  uint32_t viewport_width_ = 800;
  uint32_t viewport_height_ = 600;

public:
  CoreImpl(Engine &engine) : engine_(engine) {}

  kj::Promise<void> attachUi(AttachUiContext context) override {
    auto params = context.getParams();
    ui_ = params.getUi();
    viewport_width_ = params.getWidth();
    viewport_height_ = params.getHeight();

    std::cerr << "[rpc] UI attached (" << viewport_width_ << "x"
              << viewport_height_ << ")" << std::endl;

    // State change callback: push title/url/loading updates to TUI
    engine_.set_state_callback([this](int32_t buffer_id) {
      KJ_IF_MAYBE(ui, ui_) {
        // Title
        {
          auto req = ui->onTitleChangedRequest();
          req.setBufferId(buffer_id);
          req.setTitle(engine_.get_title(buffer_id));
          req.send().detach([](kj::Exception &&) {});
        }
        // URL
        {
          auto req = ui->onUrlChangedRequest();
          req.setBufferId(buffer_id);
          req.setUrl(engine_.get_url(buffer_id));
          req.send().detach([](kj::Exception &&) {});
        }
        // Loading state
        {
          auto req = ui->onLoadingStateChangedRequest();
          req.setBufferId(buffer_id);
          req.setLoading(engine_.is_loading(buffer_id));
          req.setCanGoBack(engine_.can_go_back(buffer_id));
          req.setCanGoForward(engine_.can_go_forward(buffer_id));
          req.send().detach([](kj::Exception &&) {});
        }
        // Progress
        {
          auto req = ui->onLoadProgressRequest();
          req.setBufferId(buffer_id);
          req.setProgress(engine_.load_progress(buffer_id));
          req.send().detach([](kj::Exception &&) {});
        }
      }
    });

    // Buffer lifecycle notifications
    engine_.set_buffer_created_callback([this](int32_t buffer_id) {
      KJ_IF_MAYBE(ui, ui_) {
        auto req = ui->onBufferCreatedRequest();
        auto info = req.initInfo();
        info.setId(buffer_id);
        info.setUrl(engine_.get_url(buffer_id));
        info.setTitle(engine_.get_title(buffer_id));
        info.setLoading(engine_.is_loading(buffer_id));
        req.send().detach([](kj::Exception &&) {});
      }
    });

    engine_.set_buffer_closed_callback([this](int32_t buffer_id) {
      KJ_IF_MAYBE(ui, ui_) {
        auto req = ui->onBufferClosedRequest();
        req.setBufferId(buffer_id);
        req.send().detach([](kj::Exception &&) {});
      }
    });

    // Create the initial buffer
    engine_.create_buffer("about:blank", viewport_width_, viewport_height_);

    // Load default Lua config
    auto *lua = engine_.lua_runtime();
    if (lua) {
      lua->exec_string("package.path = 'lua/?.lua;lua/?/init.lua;' .. package.path");
      lua->exec_file("lua/dirtferret/init.lua");
    }

    return kj::READY_NOW;
  }

  kj::Promise<void> createBuffer(CreateBufferContext context) override {
    auto url = context.getParams().getUrl();
    std::cerr << "[rpc] createBuffer(" << url.cStr() << ")" << std::endl;
    auto id = engine_.create_buffer(url, viewport_width_, viewport_height_);
    context.getResults().setBufferId(id);
    return kj::READY_NOW;
  }

  kj::Promise<void> closeBuffer(CloseBufferContext context) override {
    auto id = context.getParams().getBufferId();
    std::cerr << "[rpc] closeBuffer(" << id << ")" << std::endl;
    engine_.close_buffer(id);
    return kj::READY_NOW;
  }

  kj::Promise<void> listBuffers(ListBuffersContext context) override {
    auto ids = engine_.list_buffer_ids();
    auto results = context.getResults();
    auto list = results.initBuffers(ids.size());
    for (size_t i = 0; i < ids.size(); ++i) {
      auto id = ids[i];
      list[i].setId(id);
      list[i].setUrl(engine_.get_url(id));
      list[i].setTitle(engine_.get_title(id));
      list[i].setLoading(engine_.is_loading(id));
      list[i].setCanGoBack(engine_.can_go_back(id));
      list[i].setCanGoForward(engine_.can_go_forward(id));
      list[i].setLoadProgress(engine_.load_progress(id));
    }
    return kj::READY_NOW;
  }

  kj::Promise<void> getBufferInfo(GetBufferInfoContext context) override {
    auto id = context.getParams().getBufferId();
    auto info = context.getResults().getInfo();
    info.setId(id);
    info.setUrl(engine_.get_url(id));
    info.setTitle(engine_.get_title(id));
    info.setLoading(engine_.is_loading(id));
    info.setCanGoBack(engine_.can_go_back(id));
    info.setCanGoForward(engine_.can_go_forward(id));
    info.setLoadProgress(engine_.load_progress(id));
    return kj::READY_NOW;
  }

  kj::Promise<void> navigate(NavigateContext context) override {
    auto params = context.getParams();
    std::string url = params.getUrl();
    auto id = params.getBufferId();
    std::cerr << "[rpc] navigate(" << id << ", " << url << ")" << std::endl;
    engine_.navigate(id, url);
    return kj::READY_NOW;
  }

  kj::Promise<void> goBack(GoBackContext context) override {
    engine_.go_back(context.getParams().getBufferId());
    return kj::READY_NOW;
  }

  kj::Promise<void> goForward(GoForwardContext context) override {
    engine_.go_forward(context.getParams().getBufferId());
    return kj::READY_NOW;
  }

  kj::Promise<void> reload(ReloadContext context) override {
    engine_.reload(context.getParams().getBufferId());
    return kj::READY_NOW;
  }

  kj::Promise<void> stopLoad(StopLoadContext context) override {
    engine_.stop_load(context.getParams().getBufferId());
    return kj::READY_NOW;
  }

  kj::Promise<void> setActiveBuffer(SetActiveBufferContext context) override {
    auto id = context.getParams().getBufferId();
    engine_.set_active_buffer(id);
    return kj::READY_NOW;
  }

  kj::Promise<void> getActiveBuffer(GetActiveBufferContext context) override {
    context.getResults().setBufferId(engine_.active_buffer_id());
    return kj::READY_NOW;
  }

  kj::Promise<void> resize(ResizeContext context) override {
    auto params = context.getParams();
    auto id = params.getBufferId();
    viewport_width_ = params.getWidth();
    viewport_height_ = params.getHeight();
    engine_.resize(id, viewport_width_, viewport_height_);
    return kj::READY_NOW;
  }

  kj::Promise<void> sendKeyEvent(SendKeyEventContext context) override {
    auto params = context.getParams();
    auto id = params.getBufferId();
    auto event = params.getEvent();
    engine_.send_key_event(id,
                           static_cast<uint32_t>(event.getType()),
                           event.getKeyCode(),
                           event.getCharacter(),
                           event.getModifiers());
    return kj::READY_NOW;
  }

  kj::Promise<void> sendMouseEvent(SendMouseEventContext context) override {
    auto params = context.getParams();
    auto id = params.getBufferId();
    auto event = params.getEvent();
    engine_.send_mouse_event(id,
                             static_cast<uint32_t>(event.getType()),
                             event.getX(), event.getY(),
                             static_cast<uint32_t>(event.getButton()),
                             event.getModifiers());
    return kj::READY_NOW;
  }

  kj::Promise<void> sendScrollEvent(SendScrollEventContext context) override {
    auto params = context.getParams();
    engine_.send_scroll_event(params.getBufferId(),
                              params.getDeltaX(),
                              params.getDeltaY());
    return kj::READY_NOW;
  }

  kj::Promise<void> resolveKeybind(ResolveKeybindContext context) override {
    auto params = context.getParams();
    std::string mode_str = params.getMode();
    uint32_t character = params.getCharacter();
    uint32_t modifiers = params.getModifiers();

    std::string key_str;
    if (character < 128 && character > 0) {
      key_str = std::string(1, static_cast<char>(character));
    }

    std::cerr << "[keybind] resolve: mode='" << mode_str
              << "' char=" << character
              << " key='" << key_str
              << "' mods=" << modifiers << std::endl;

    auto *lua = engine_.lua_runtime();
    std::string action;
    std::string arg;

    if (!lua || !lua->state()) {
      std::cerr << "[keybind] FAIL: no Lua runtime" << std::endl;
    } else {
      auto *L = lua->state();
      lua_getglobal(L, "dirtferret");
      if (!lua_istable(L, -1)) {
        std::cerr << "[keybind] FAIL: dirtferret global is not a table" << std::endl;
      } else {
        lua_getfield(L, -1, "keymap");
        if (!lua_istable(L, -1)) {
          std::cerr << "[keybind] FAIL: dirtferret.keymap is not a table" << std::endl;
        } else {
          lua_getfield(L, -1, "_maps");
          if (!lua_istable(L, -1)) {
            std::cerr << "[keybind] FAIL: dirtferret.keymap._maps missing (keymap.lua not loaded?)" << std::endl;
          } else {
            lua_getfield(L, -1, mode_str.c_str());
            if (!lua_istable(L, -1)) {
              std::cerr << "[keybind] WARN: no bindings for mode '" << mode_str << "'" << std::endl;
            } else {
              if (!key_str.empty()) {
                lua_getfield(L, -1, key_str.c_str());
                if (lua_isstring(L, -1)) {
                  action = lua_tostring(L, -1);
                  std::cerr << "[keybind] HIT: '" << key_str << "' -> '" << action << "'" << std::endl;
                } else {
                  std::cerr << "[keybind] MISS: no binding for '" << key_str << "' in mode '" << mode_str << "'" << std::endl;
                }
                lua_pop(L, 1);
              } else {
                std::cerr << "[keybind] SKIP: non-printable character " << character << std::endl;
              }
            }
          }
        }
      }
      lua_settop(L, 0);
    }

    std::cerr << "[keybind] result: action='" << action << "' arg='" << arg << "'" << std::endl;
    context.getResults().setAction(action);
    context.getResults().setArg(arg);
    return kj::READY_NOW;
  }

  // ---- DOM queries (async — CDP round-trips) ----

  kj::Promise<void> elementAt(ElementAtContext context) override {
    auto params = context.getParams();
    auto paf = kj::newPromiseAndFulfiller<void>();
    auto fulfiller = std::make_shared<kj::Own<kj::PromiseFulfiller<void>>>(
        kj::mv(paf.fulfiller));

    engine_.element_at(params.getBufferId(), params.getX(), params.getY(),
        [context, fulfiller](ElementInfo info) mutable {
          auto el = context.getResults().getElement();
          el.setNodeId(info.node_id);
          el.setTag(info.tag);
          el.setText(info.text);
          auto b = el.getBounds();
          b.setX(info.bounds_x);
          b.setY(info.bounds_y);
          b.setWidth(info.bounds_width);
          b.setHeight(info.bounds_height);
          auto attrs = el.initAttributes(info.attributes.size());
          for (size_t i = 0; i < info.attributes.size(); ++i) {
            attrs[i].setName(info.attributes[i].first);
            attrs[i].setValue(info.attributes[i].second);
          }
          (*fulfiller)->fulfill();
        });

    return kj::mv(paf.promise);
  }

  kj::Promise<void> query(QueryContext context) override {
    auto params = context.getParams();
    std::string selector = params.getSelector();
    auto paf = kj::newPromiseAndFulfiller<void>();
    auto fulfiller = std::make_shared<kj::Own<kj::PromiseFulfiller<void>>>(
        kj::mv(paf.fulfiller));

    engine_.query(params.getBufferId(), selector,
        [context, fulfiller](
            std::vector<ElementInfo> elements) mutable {
          auto results = context.getResults();
          auto list = results.initElements(elements.size());
          for (size_t i = 0; i < elements.size(); ++i) {
            list[i].setNodeId(elements[i].node_id);
            list[i].setTag(elements[i].tag);
            list[i].setText(elements[i].text);
            auto b = list[i].getBounds();
            b.setX(elements[i].bounds_x);
            b.setY(elements[i].bounds_y);
            b.setWidth(elements[i].bounds_width);
            b.setHeight(elements[i].bounds_height);
            auto attrs = list[i].initAttributes(elements[i].attributes.size());
            for (size_t j = 0; j < elements[i].attributes.size(); ++j) {
              attrs[j].setName(elements[i].attributes[j].first);
              attrs[j].setValue(elements[i].attributes[j].second);
            }
          }
          (*fulfiller)->fulfill();
        });

    return kj::mv(paf.promise);
  }

  // ---- Region management ----

  kj::Promise<void> regionAdd(RegionAddContext context) override {
    auto params = context.getParams();
    auto rid = engine_.region_add(params.getBufferId(), params.getX(), params.getY());
    context.getResults().setRegionId(rid);
    return kj::READY_NOW;
  }

  kj::Promise<void> regionRemove(RegionRemoveContext context) override {
    auto params = context.getParams();
    engine_.region_remove(params.getBufferId(), params.getRegionId());
    return kj::READY_NOW;
  }

  kj::Promise<void> regionMove(RegionMoveContext context) override {
    auto params = context.getParams();
    engine_.region_move(params.getBufferId(), params.getRegionId(),
                        params.getX(), params.getY());
    return kj::READY_NOW;
  }

  kj::Promise<void> regionSelect(RegionSelectContext context) override {
    auto params = context.getParams();
    auto scope = static_cast<Scope>(static_cast<int>(params.getScope()));
    std::string selector_arg = params.getSelectorArg();
    auto paf = kj::newPromiseAndFulfiller<void>();
    auto fulfiller = std::make_shared<kj::Own<kj::PromiseFulfiller<void>>>(
        kj::mv(paf.fulfiller));

    engine_.region_select(params.getBufferId(), scope, selector_arg,
        [fulfiller]() mutable {
          (*fulfiller)->fulfill();
        });

    return kj::mv(paf.promise);
  }

  kj::Promise<void> regionClear(RegionClearContext context) override {
    engine_.region_clear(context.getParams().getBufferId());
    return kj::READY_NOW;
  }

  kj::Promise<void> getRegions(GetRegionsContext context) override {
    auto id = context.getParams().getBufferId();
    auto regions = engine_.get_regions(id);
    auto results = context.getResults();
    auto list = results.initRegions(regions.size());
    for (size_t i = 0; i < regions.size(); ++i) {
      list[i].setId(regions[i].id);
      list[i].setAnchorX(regions[i].anchor.x);
      list[i].setAnchorY(regions[i].anchor.y);
      list[i].setHeadX(regions[i].head.x);
      list[i].setHeadY(regions[i].head.y);
    }
    return kj::READY_NOW;
  }

  // ---- Cursor navigation ----

  kj::Promise<void> cursorInit(CursorInitContext context) override {
    auto paf = kj::newPromiseAndFulfiller<void>();
    auto fulfiller = std::make_shared<kj::Own<kj::PromiseFulfiller<void>>>(
        kj::mv(paf.fulfiller));

    engine_.cursor_init(context.getParams().getBufferId(),
        [context, fulfiller](bool active) mutable {
          context.getResults().setActive(active);
          (*fulfiller)->fulfill();
        });

    return kj::mv(paf.promise);
  }

  kj::Promise<void> cursorNext(CursorNextContext context) override {
    auto paf = kj::newPromiseAndFulfiller<void>();
    auto fulfiller = std::make_shared<kj::Own<kj::PromiseFulfiller<void>>>(
        kj::mv(paf.fulfiller));

    engine_.cursor_next(context.getParams().getBufferId(),
        [fulfiller]() mutable {
          (*fulfiller)->fulfill();
        });

    return kj::mv(paf.promise);
  }

  kj::Promise<void> cursorPrev(CursorPrevContext context) override {
    auto paf = kj::newPromiseAndFulfiller<void>();
    auto fulfiller = std::make_shared<kj::Own<kj::PromiseFulfiller<void>>>(
        kj::mv(paf.fulfiller));

    engine_.cursor_prev(context.getParams().getBufferId(),
        [fulfiller]() mutable {
          (*fulfiller)->fulfill();
        });

    return kj::mv(paf.promise);
  }

  kj::Promise<void> cursorMoveDir(CursorMoveDirContext context) override {
    auto params = context.getParams();
    auto paf = kj::newPromiseAndFulfiller<void>();
    auto fulfiller = std::make_shared<kj::Own<kj::PromiseFulfiller<void>>>(
        kj::mv(paf.fulfiller));

    engine_.cursor_move_dir(params.getBufferId(),
        params.getDx(), params.getDy(), params.getExtend(),
        [fulfiller]() mutable {
          (*fulfiller)->fulfill();
        });

    return kj::mv(paf.promise);
  }

  kj::Promise<void> cursorActivate(CursorActivateContext context) override {
    engine_.cursor_activate(context.getParams().getBufferId());
    return kj::READY_NOW;
  }

  kj::Promise<void> cursorClear(CursorClearContext context) override {
    engine_.cursor_clear(context.getParams().getBufferId());
    return kj::READY_NOW;
  }

  // ---- Match list ----

  kj::Promise<void> matchSet(MatchSetContext context) override {
    auto params = context.getParams();
    std::string selector = params.getSelector();
    auto paf = kj::newPromiseAndFulfiller<void>();
    auto fulfiller = std::make_shared<kj::Own<kj::PromiseFulfiller<void>>>(
        kj::mv(paf.fulfiller));

    engine_.match_set(params.getBufferId(), selector,
        [context, fulfiller](uint32_t count) mutable {
          context.getResults().setCount(count);
          (*fulfiller)->fulfill();
        });

    return kj::mv(paf.promise);
  }

  kj::Promise<void> matchNext(MatchNextContext context) override {
    auto paf = kj::newPromiseAndFulfiller<void>();
    auto fulfiller = std::make_shared<kj::Own<kj::PromiseFulfiller<void>>>(
        kj::mv(paf.fulfiller));

    engine_.match_next(context.getParams().getBufferId(),
        [fulfiller]() mutable {
          (*fulfiller)->fulfill();
        });

    return kj::mv(paf.promise);
  }

  kj::Promise<void> matchPrev(MatchPrevContext context) override {
    auto paf = kj::newPromiseAndFulfiller<void>();
    auto fulfiller = std::make_shared<kj::Own<kj::PromiseFulfiller<void>>>(
        kj::mv(paf.fulfiller));

    engine_.match_prev(context.getParams().getBufferId(),
        [fulfiller]() mutable {
          (*fulfiller)->fulfill();
        });

    return kj::mv(paf.promise);
  }

  kj::Promise<void> matchClear(MatchClearContext context) override {
    engine_.match_clear(context.getParams().getBufferId());
    return kj::READY_NOW;
  }
};

// --- CEF pump ---

kj::Promise<void> pumpCef(Engine &engine, kj::Timer &timer) {
  engine.do_message_loop_work();
  return timer.afterDelay(16 * kj::MILLISECONDS).then([&engine, &timer]() {
    return pumpCef(engine, timer);
  });
}

// --- Stdin command reader ---

kj::Promise<void> readStdin(Engine &engine, kj::AsyncInputStream &input,
                            kj::Vector<char> &lineBuf) {
  auto buf = kj::heapArray<char>(1);
  auto ptr = buf.begin();
  return input.tryRead(ptr, 1, 1).then(
      [&engine, &input, &lineBuf,
       buf = kj::mv(buf)](size_t n) mutable -> kj::Promise<void> {
        if (n == 0)
          return kj::READY_NOW;

        char c = buf[0];
        if (c == '\n') {
          lineBuf.add('\0');
          std::string line(lineBuf.begin());
          lineBuf.clear();

          if (line == "q" || line == "quit") {
            std::cerr << "[core] Quit." << std::endl;
            return kj::READY_NOW;
          }

          auto active = engine.active_buffer_id();
          if (active < 0) {
            std::cerr << "[core] No active buffer." << std::endl;
          } else if (line == "b" || line == "back") {
            engine.go_back(active);
          } else if (line == "f" || line == "forward") {
            engine.go_forward(active);
          } else if (line == "r" || line == "reload") {
            engine.reload(active);
          } else if (line == "t" || line == "title") {
            std::cerr << "[info] " << engine.get_title(active) << std::endl;
          } else if (line == "u" || line == "url") {
            std::cerr << "[info] " << engine.get_url(active) << std::endl;
          } else if (line == "new") {
            engine.create_buffer("about:blank", 800, 600);
          } else if (!line.empty()) {
            if (line.find("://") == std::string::npos)
              line = "https://" + line;
            engine.navigate(active, line);
          }

          return readStdin(engine, input, lineBuf);
        } else {
          lineBuf.add(c);
          return readStdin(engine, input, lineBuf);
        }
      });
}

int main(int argc, char *argv[]) {
  Engine engine;
  if (!engine.initialize(argc, argv)) {
    int code = engine.child_exit_code();
    if (code >= 0)
      return code;
    std::cerr << "[core] Engine init failed." << std::endl;
    return 1;
  }

  std::cerr << "[core] dirtferret-core starting..." << std::endl;

  auto io = kj::setupAsyncIo();

  capnp::TwoPartyServer server(kj::heap<CoreImpl>(engine));
  auto address = io.provider->getNetwork()
                     .parseAddress("127.0.0.1", 5000)
                     .wait(io.waitScope);
  auto listener = address->listen();
  auto listenPromise = server.listen(*listener);
  std::cerr << "[rpc] Listening on 127.0.0.1:5000" << std::endl;

  auto cefPump = pumpCef(engine, io.provider->getTimer());

  auto stdinPipe = io.lowLevelProvider->wrapInputFd(0);
  kj::Vector<char> lineBuf;

  std::cerr << "\n--- dirtferret prototype ---" << std::endl;
  std::cerr << "Type a URL, or: b/back, f/forward, r/reload, new, q/quit"
            << std::endl;
  std::cerr << "---\n" << std::endl;

  auto stdinLoop = readStdin(engine, *stdinPipe, lineBuf);
  stdinLoop.wait(io.waitScope);

  engine.shutdown();
  std::cerr << "[core] Shutdown." << std::endl;
  return 0;
}
