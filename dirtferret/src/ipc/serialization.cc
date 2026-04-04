#include "ipc/serialization.h"

#include <cstring>
#include <iostream>

namespace cef_terminal {

namespace {

// --- Write helpers (little-endian) ---

void write_u8(std::vector<uint8_t>& buf, uint8_t v) {
    buf.push_back(v);
}

void write_u16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(v & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
}

void write_u32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(v & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >> 24) & 0xFF);
}

void write_i64(std::vector<uint8_t>& buf, int64_t v) {
    uint64_t u;
    std::memcpy(&u, &v, 8);
    for (int i = 0; i < 8; i++) {
        buf.push_back((u >> (i * 8)) & 0xFF);
    }
}

void write_f64(std::vector<uint8_t>& buf, double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, 8);
    for (int i = 0; i < 8; i++) {
        buf.push_back((bits >> (i * 8)) & 0xFF);
    }
}

void write_str(std::vector<uint8_t>& buf, const std::string& s) {
    write_u32(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

// Value tag mapping matches std::variant index:
// 0 = monostate (null), 1 = bool, 2 = int64_t, 3 = double, 4 = string
void write_value(std::vector<uint8_t>& buf, const Value& v) {
    uint8_t tag = static_cast<uint8_t>(v.index());
    write_u8(buf, tag);
    switch (tag) {
        case 0: break;  // null
        case 1: write_u8(buf, std::get<bool>(v) ? 1 : 0); break;
        case 2: write_i64(buf, std::get<int64_t>(v)); break;
        case 3: write_f64(buf, std::get<double>(v)); break;
        case 4: write_str(buf, std::get<std::string>(v)); break;
    }
}

// --- Read helpers ---
// All readers take a bool& ok flag. On underflow, ok is set to false
// and a zero/empty value is returned. Callers check ok after each
// deserialization sequence.

uint8_t read_u8(const uint8_t* data, size_t len, size_t& off, bool& ok) {
    if (!ok || off + 1 > len) { ok = false; return 0; }
    return data[off++];
}

uint16_t read_u16(const uint8_t* data, size_t len, size_t& off, bool& ok) {
    if (!ok || off + 2 > len) { ok = false; return 0; }
    uint16_t v = data[off] | (static_cast<uint16_t>(data[off + 1]) << 8);
    off += 2;
    return v;
}

uint32_t read_u32(const uint8_t* data, size_t len, size_t& off, bool& ok) {
    if (!ok || off + 4 > len) { ok = false; return 0; }
    uint32_t v = data[off]
        | (static_cast<uint32_t>(data[off + 1]) << 8)
        | (static_cast<uint32_t>(data[off + 2]) << 16)
        | (static_cast<uint32_t>(data[off + 3]) << 24);
    off += 4;
    return v;
}

int64_t read_i64(const uint8_t* data, size_t len, size_t& off, bool& ok) {
    if (!ok || off + 8 > len) { ok = false; return 0; }
    uint64_t u = 0;
    for (int i = 0; i < 8; i++) {
        u |= static_cast<uint64_t>(data[off + i]) << (i * 8);
    }
    off += 8;
    int64_t v;
    std::memcpy(&v, &u, 8);
    return v;
}

double read_f64(const uint8_t* data, size_t len, size_t& off, bool& ok) {
    if (!ok || off + 8 > len) { ok = false; return 0.0; }
    uint64_t u = 0;
    for (int i = 0; i < 8; i++) {
        u |= static_cast<uint64_t>(data[off + i]) << (i * 8);
    }
    off += 8;
    double v;
    std::memcpy(&v, &u, 8);
    return v;
}

std::string read_str(const uint8_t* data, size_t len, size_t& off, bool& ok) {
    uint32_t slen = read_u32(data, len, off, ok);
    if (!ok || off + slen > len) { ok = false; return {}; }
    std::string s(reinterpret_cast<const char*>(data + off), slen);
    off += slen;
    return s;
}

Value read_value(const uint8_t* data, size_t len, size_t& off, bool& ok) {
    uint8_t tag = read_u8(data, len, off, ok);
    if (!ok) return Value{std::monostate{}};
    switch (tag) {
        case 0: return Value{std::monostate{}};
        case 1: return Value{read_u8(data, len, off, ok) != 0};
        case 2: return Value{read_i64(data, len, off, ok)};
        case 3: return Value{read_f64(data, len, off, ok)};
        case 4: return Value{read_str(data, len, off, ok)};
        default:
            std::cerr << "[ipc] Unknown value tag: " << (int)tag << std::endl;
            ok = false;
            return Value{std::monostate{}};
    }
}

std::vector<uint8_t> serialize_named_args(const std::string& name, const Args& args) {
    std::vector<uint8_t> buf;
    write_str(buf, name);
    auto args_bytes = serialize_args(args);
    buf.insert(buf.end(), args_bytes.begin(), args_bytes.end());
    return buf;
}

}  // namespace

// --- Public API ---

std::vector<uint8_t> serialize_args(const Args& args) {
    std::vector<uint8_t> buf;
    write_u16(buf, static_cast<uint16_t>(args.size()));
    for (const auto& [key, val] : args) {
        write_str(buf, key);
        write_value(buf, val);
    }
    return buf;
}

Args deserialize_args(const uint8_t* data, size_t len, size_t& off) {
    bool ok = true;
    uint16_t count = read_u16(data, len, off, ok);
    Args args;
    for (uint16_t i = 0; i < count && ok; i++) {
        std::string key = read_str(data, len, off, ok);
        Value val = read_value(data, len, off, ok);
        if (ok) {
            args[std::move(key)] = std::move(val);
        }
    }
    if (!ok) {
        std::cerr << "[ipc] Warning: args deserialization underflow" << std::endl;
    }
    return args;
}

std::vector<uint8_t> serialize_command(const Command& cmd) {
    return serialize_named_args(cmd.name, cmd.args);
}

Command deserialize_command(const std::vector<uint8_t>& data) {
    bool ok = true;
    size_t off = 0;
    std::string name = read_str(data.data(), data.size(), off, ok);
    Args args = deserialize_args(data.data(), data.size(), off);
    return Command{std::move(name), std::move(args)};
}

std::vector<uint8_t> serialize_query(const Query& query) {
    return serialize_named_args(query.name, query.args);
}

Query deserialize_query(const std::vector<uint8_t>& data) {
    bool ok = true;
    size_t off = 0;
    std::string name = read_str(data.data(), data.size(), off, ok);
    Args args = deserialize_args(data.data(), data.size(), off);
    return Query{std::move(name), std::move(args)};
}

std::vector<uint8_t> serialize_command_result(const CommandResult& result) {
    std::vector<uint8_t> buf;
    write_u8(buf, result.ok ? 1 : 0);
    write_str(buf, result.error);
    return buf;
}

CommandResult deserialize_command_result(const std::vector<uint8_t>& data) {
    bool ok = true;
    size_t off = 0;
    bool result_ok = read_u8(data.data(), data.size(), off, ok) != 0;
    std::string error = read_str(data.data(), data.size(), off, ok);
    if (result_ok) return CommandResult::success();
    return CommandResult::failure(std::move(error));
}

std::vector<uint8_t> serialize_query_result(const QueryResult& result) {
    std::vector<uint8_t> buf;
    write_u8(buf, result.ok ? 1 : 0);
    write_str(buf, result.error);
    auto args_bytes = serialize_args(result.data);
    buf.insert(buf.end(), args_bytes.begin(), args_bytes.end());
    return buf;
}

QueryResult deserialize_query_result(const std::vector<uint8_t>& data) {
    bool ok = true;
    size_t off = 0;
    bool result_ok = read_u8(data.data(), data.size(), off, ok) != 0;
    std::string error = read_str(data.data(), data.size(), off, ok);
    Args args = deserialize_args(data.data(), data.size(), off);
    if (result_ok) return QueryResult::success(std::move(args));
    return QueryResult::failure(std::move(error));
}

}  // namespace cef_terminal
