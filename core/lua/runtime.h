#pragma once

#include <functional>
#include <string>

struct lua_State;

class LuaRuntime {
public:
  LuaRuntime();
  ~LuaRuntime();

  bool initialize();
  bool exec_string(const std::string &code);
  bool exec_file(const std::string &path);

  std::string get_opt_string(const std::string &key);
  double get_opt_number(const std::string &key);
  bool get_opt_bool(const std::string &key);

  using CFunction = int (*)(lua_State *);
  void register_function(const std::string &path, CFunction fn);

  lua_State *state() const { return L_; }

private:
  lua_State *L_ = nullptr;
  void setup_dirtferret_table();
};
