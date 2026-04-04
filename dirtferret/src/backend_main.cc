#include "api/dispatcher.h"
#include "backend/engine.h"
#include "ipc/serialization.h"
#include "ipc/unix_socket_transport.h"

#include <iostream>
#include <memory>
#include <vector>
#include <unistd.h>

static const std::string SOCKET_PATH = "/tmp/cef-terminal.sock";

int main(int argc, char* argv[]) {
    // --- CEF engine ---
    cef_terminal::Engine engine;

    if (!engine.initialize(argc, argv)) {
        int code = engine.child_exit_code();
        if (code >= 0) {
            return code;  // clean child process exit
        }
        std::cerr << "[backend] Engine init failed." << std::endl;
        return 1;
    }

    std::cerr << "[backend] Starting cef-backend..." << std::endl;

    // --- API layer ---
    cef_terminal::Dispatcher dispatcher;
    engine.register_handlers(dispatcher);

    // --- IPC server ---
    cef_terminal::UnixSocketTransport server;
    std::vector<std::unique_ptr<cef_terminal::Transport>> clients;

    server.listen(SOCKET_PATH, [&clients](std::unique_ptr<cef_terminal::Transport> client) {
        std::cerr << "[backend] Client connected." << std::endl;
        clients.push_back(std::move(client));
    });

    // --- Main loop ---
    std::cerr << "[backend] Entering main loop..." << std::endl;
    bool running = true;
    while (running) {
        engine.tick();
        server.poll_accept();

        // Process messages from all connected clients.
        for (auto it = clients.begin(); it != clients.end(); ) {
            auto& client = *it;

            if (!client->connected()) {
                std::cerr << "[backend] Client disconnected, removing." << std::endl;
                it = clients.erase(it);
                continue;
            }

            auto msg = client->receive();
            if (!msg) {
                ++it;
                continue;
            }

            // Deserialize, dispatch through the API layer, respond.
            cef_terminal::Message response;
            response.id = msg->id;

            if (msg->type == cef_terminal::MessageType::COMMAND) {
                auto cmd = cef_terminal::deserialize_command(msg->payload);
                std::cerr << "[backend] Dispatching command: " << cmd.name << std::endl;
                auto result = dispatcher.dispatch(cmd);
                response.type = cef_terminal::MessageType::COMMAND_RESULT;
                response.payload = cef_terminal::serialize_command_result(result);

            } else if (msg->type == cef_terminal::MessageType::QUERY) {
                auto query = cef_terminal::deserialize_query(msg->payload);
                std::cerr << "[backend] Dispatching query: " << query.name << std::endl;
                auto result = dispatcher.dispatch(query);
                response.type = cef_terminal::MessageType::QUERY_RESULT;
                response.payload = cef_terminal::serialize_query_result(result);

            } else {
                std::cerr << "[backend] Unknown message type, ignoring." << std::endl;
                ++it;
                continue;
            }

            client->send(response);
            ++it;
        }

        // Don't spin the CPU — small sleep between loop iterations.
        usleep(1000);  // 1ms
    }

    engine.shutdown();
    std::cerr << "[backend] Shutdown." << std::endl;
    return 0;
}
