#include "api/command.h"
#include "api/query.h"
#include "api/types.h"
#include "ipc/message.h"
#include "ipc/serialization.h"
#include "ipc/unix_socket_transport.h"

#include <iostream>
#include <string>
#include <unistd.h>

static const std::string SOCKET_PATH = "/tmp/cef-terminal.sock";

// Helper: send a command and wait for the result.
static cef_terminal::CommandResult send_command(
    cef_terminal::UnixSocketTransport& transport,
    uint32_t& next_id,
    const std::string& name,
    cef_terminal::Args args = {})
{
    cef_terminal::Command cmd{name, std::move(args)};
    cef_terminal::Message msg;
    msg.type = cef_terminal::MessageType::COMMAND;
    msg.id = next_id++;
    msg.payload = cef_terminal::serialize_command(cmd);

    if (!transport.send(msg)) {
        return cef_terminal::CommandResult::failure("send failed");
    }

    auto reply = transport.wait_for_message(5000);
    if (!reply) {
        return cef_terminal::CommandResult::failure("no response (timeout)");
    }

    return cef_terminal::deserialize_command_result(reply->payload);
}

// Helper: send a query and wait for the result.
static cef_terminal::QueryResult send_query(
    cef_terminal::UnixSocketTransport& transport,
    uint32_t& next_id,
    const std::string& name,
    cef_terminal::Args args = {})
{
    cef_terminal::Query query{name, std::move(args)};
    cef_terminal::Message msg;
    msg.type = cef_terminal::MessageType::QUERY;
    msg.id = next_id++;
    msg.payload = cef_terminal::serialize_query(query);

    if (!transport.send(msg)) {
        return cef_terminal::QueryResult::failure("send failed");
    }

    auto reply = transport.wait_for_message(5000);
    if (!reply) {
        return cef_terminal::QueryResult::failure("no response (timeout)");
    }

    return cef_terminal::deserialize_query_result(reply->payload);
}

int main(int argc, char* argv[]) {
    std::cerr << "[frontend] Starting cef-frontend..." << std::endl;

    // --- Connect to backend ---
    cef_terminal::UnixSocketTransport transport;
    if (!transport.connect(SOCKET_PATH)) {
        std::cerr << "[frontend] Could not connect to backend." << std::endl;
        return 1;
    }

    uint32_t next_id = 1;

    // Give CEF a moment to finish loading the default page.
    std::cerr << "[frontend] Waiting for initial page load..." << std::endl;
    sleep(2);

    // --- Query the title of the current page ---
    std::cerr << "[frontend] Querying page title..." << std::endl;
    auto title_result = send_query(transport, next_id, "buffer.get_title");
    if (title_result.ok) {
        auto it = title_result.data.find("title");
        if (it != title_result.data.end()) {
            auto* title = std::get_if<std::string>(&it->second);
            if (title) {
                std::cout << "Current page title: " << *title << std::endl;
            }
        }
    } else {
        std::cerr << "[frontend] Query failed: " << title_result.error << std::endl;
    }

    // --- Navigate to a different page ---
    std::string url = "https://www.wikipedia.org";
    if (argc > 1) {
        url = argv[1];  // allow passing URL as argument
    }

    std::cerr << "[frontend] Navigating to " << url << "..." << std::endl;
    cef_terminal::Args nav_args;
    nav_args["url"] = url;
    auto nav_result = send_command(transport, next_id, "buffer.navigate", std::move(nav_args));
    if (!nav_result.ok) {
        std::cerr << "[frontend] Navigate failed: " << nav_result.error << std::endl;
    }

    // Wait for the new page to load.
    std::cerr << "[frontend] Waiting for page load..." << std::endl;
    sleep(3);

    // --- Query the title again ---
    std::cerr << "[frontend] Querying new page title..." << std::endl;
    auto new_title_result = send_query(transport, next_id, "buffer.get_title");
    if (new_title_result.ok) {
        auto it = new_title_result.data.find("title");
        if (it != new_title_result.data.end()) {
            auto* title = std::get_if<std::string>(&it->second);
            if (title) {
                std::cout << "New page title: " << *title << std::endl;
            }
        }
    } else {
        std::cerr << "[frontend] Query failed: " << new_title_result.error << std::endl;
    }

    transport.close();
    std::cerr << "[frontend] Done." << std::endl;
    return 0;
}
