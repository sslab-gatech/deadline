#include "Project.h"

void Logger::vec() {
  assert(cur->is_array());

  cur->emplace_back(json::array());

  cur = &cur->back();
  stk.push(cur);
}

void Logger::vec(const string &key) {
  assert(cur->is_object());

  auto res = cur->emplace(key, json::array());
  assert(res.second);

  cur = &res.first.value(); 
  stk.push(cur);
}

void Logger::map() {
  assert(cur->is_array());

  cur->emplace_back(json::object());

  cur = &cur->back();
  stk.push(cur);
}

void Logger::map(const string &key) {
  assert(cur->is_object());

  auto res = cur->emplace(key, json::object());
  assert(res.second);

  cur = &res.first.value(); 
  stk.push(cur);
}

template<typename T>
void Logger::log(const T &msg) {
  assert(cur->is_array());
  cur->emplace_back(msg);
}

void Logger::log(const char *msg) {
  assert(cur->is_array());
  cur->emplace_back(msg);
}

template<typename T>
void Logger::log(const string &key, const T &msg) {
  assert(cur->is_object());
  cur->emplace(key, msg);
}

void Logger::log(const string &key, const char *msg) {
  assert(cur->is_object());
  cur->emplace(key, msg);
}

void Logger::pop() {
  stk.pop();
  cur = stk.top();
}

void Logger::dump(raw_ostream &stm, int indent) {
  stm << rec.dump(indent);
}

void Logger::dump(const string &fn, int indent) {
  error_code ec;
  raw_fd_ostream stm(StringRef(fn), ec, sys::fs::F_RW);
  assert(ec.value() == 0);

  dump(stm, indent);
}

#define EXT_TEMPLATE(type)                                              \
  template void Logger::log<type>(const type &msg);                     \
  template void Logger::log<type>(const string &key, const type &msg);

EXT_TEMPLATE(bool);
EXT_TEMPLATE(int);
EXT_TEMPLATE(unsigned int);
EXT_TEMPLATE(long);
EXT_TEMPLATE(unsigned long);
EXT_TEMPLATE(string);
