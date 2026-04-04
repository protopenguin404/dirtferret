#pragma once

#include "api/command.h"
#include "api/query.h"
#include "api/types.h"

#include <cstdint>
#include <vector>

namespace cef_terminal {

// Serialize/deserialize API types to/from byte buffers for IPC.
// Wire format is little-endian, length-prefixed strings, tagged values.

std::vector<uint8_t> serialize_args(const Args& args);
Args deserialize_args(const uint8_t* data, size_t len, size_t& offset);

std::vector<uint8_t> serialize_command(const Command& cmd);
Command deserialize_command(const std::vector<uint8_t>& data);

std::vector<uint8_t> serialize_query(const Query& query);
Query deserialize_query(const std::vector<uint8_t>& data);

std::vector<uint8_t> serialize_command_result(const CommandResult& result);
CommandResult deserialize_command_result(const std::vector<uint8_t>& data);

std::vector<uint8_t> serialize_query_result(const QueryResult& result);
QueryResult deserialize_query_result(const std::vector<uint8_t>& data);

}  // namespace cef_terminal
