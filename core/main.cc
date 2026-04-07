#include "engine/engine.h"
#include "schema/core.capnp.h"
#include <capnp/ez-rpc.h>
#include <capnp/rpc-twoparty.h>
#include <kj/async-io.h>
#include <kj/timer.h>

#include <iostream>

// --- RPC server implementation ---

class CoreImpl final : public Core::Server {
private:
  Engine &engine_;
  kj::Maybe<Ui::Client> ui_;

public:
  CoreImpl(Engine &engine) : engine_(engine) {}

  kj::Promise<void> attachUi(AttachUiContext context) override {
    auto params = context.getParams();
    ui_ = params.getUi();
    uint32_t w = params.getWidth();
    uint32_t h = params.getHeight();

    std::cerr << "[rpc] UI attached (" << w << "x" << h << ")" << std::endl;

    engine_.setup_frame_pool(w, h);
    engine_.resize(0, w, h);

    // Frame callback, notify the frontend
    engine_.set_frame_callback([this](int32_t id, const void *, int w, int h) {
      KJ_IF_MAYBE (ui, ui_) {
        auto req = ui->onFrameRequest();
        req.setBufferId(id);
        req.setShmName(engine_.frame_shm_name());
        req.setWidth(w);
        req.setHeight(h);
        req.setFormat(::PixelFormat::BGRA);
        // Fire and forget — don't block OnPaint waiting for TUI
        req.send().detach([](kj::Exception &&e) {
          std::cerr << "[rpc] onFrame error: " << e.getDescription().cStr()
                    << std::endl;
        });
      }
    });

    return ::kj::READY_NOW;
  }

  kj::Promise<void> createBuffer(CreateBufferContext context) override {
    std::cerr << "[rpc] createBuffer (stub)" << std::endl;
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
};

// --- CEF pump: drive CEF's message loop from KJ ---

kj::Promise<void> pumpCef(Engine &engine, kj::Timer &timer) {
  engine.do_message_loop_work();
  return timer.afterDelay(16 * kj::MILLISECONDS).then([&engine, &timer]() {
    return pumpCef(engine, timer);
  });
}

// --- Stdin command reader: type URLs to navigate ---

kj::Promise<void> readStdin(Engine &engine, kj::AsyncInputStream &input,
                            kj::Vector<char> &lineBuf) {
  auto buf = kj::heapArray<char>(1);
  auto ptr = buf.begin();
  return input.tryRead(ptr, 1, 1).then(
      [&engine, &input, &lineBuf,
       buf = kj::mv(buf)](size_t n) mutable -> kj::Promise<void> {
        if (n == 0)
          return kj::READY_NOW; // EOF

        char c = buf[0];
        if (c == '\n') {
          lineBuf.add('\0');
          std::string line(lineBuf.begin());
          lineBuf.clear();

          if (line == "q" || line == "quit") {
            std::cerr << "[core] Quit." << std::endl;
            return kj::READY_NOW; // breaks the loop
          } else if (line == "b" || line == "back") {
            engine.go_back(0);
          } else if (line == "f" || line == "forward") {
            engine.go_forward(0);
          } else if (line == "r" || line == "reload") {
            engine.reload(0);
          } else if (line == "t" || line == "title") {
            std::cerr << "[info] " << engine.get_title(0) << std::endl;
          } else if (line == "u" || line == "url") {
            std::cerr << "[info] " << engine.get_url(0) << std::endl;
          } else if (!line.empty()) {
            // Treat as URL
            if (line.find("://") == std::string::npos)
              line = "https://" + line;
            engine.navigate(0, line);
          }

          return readStdin(engine, input, lineBuf);
        } else {
          lineBuf.add(c);
          return readStdin(engine, input, lineBuf);
        }
      });
}

int main(int argc, char *argv[]) {
  // CEF MUST initialize first — child processes re-exec this binary
  // and need to exit before any other framework touches state.
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

  // RPC server on TCP 5000
  capnp::TwoPartyServer server(kj::heap<CoreImpl>(engine));
  auto address = io.provider->getNetwork()
                     .parseAddress("127.0.0.1", 5000)
                     .wait(io.waitScope);
  auto listener = address->listen();
  auto listenPromise = server.listen(*listener);
  std::cerr << "[rpc] Listening on 127.0.0.1:5000" << std::endl;

  // CEF message pump (~60fps)
  auto cefPump = pumpCef(engine, io.provider->getTimer());

  // Stdin command loop
  auto stdinPipe = io.lowLevelProvider->wrapInputFd(0);
  kj::Vector<char> lineBuf;

  std::cerr << "\n--- dirtferret prototype ---" << std::endl;
  std::cerr << "Type a URL to navigate, or:" << std::endl;
  std::cerr << "  b/back, f/forward, r/reload" << std::endl;
  std::cerr << "  t/title, u/url" << std::endl;
  std::cerr << "  q/quit" << std::endl;
  std::cerr << "---\n" << std::endl;

  auto stdinLoop = readStdin(engine, *stdinPipe, lineBuf);

  // Run until stdin exits (quit command or EOF)
  stdinLoop.wait(io.waitScope);

  engine.shutdown();
  std::cerr << "[core] Shutdown." << std::endl;
  return 0;
}
