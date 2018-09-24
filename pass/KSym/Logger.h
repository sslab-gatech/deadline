#ifndef LOGGER_H_
#define LOGGER_H_

#include "Project.h"

class Logger {
  public:
    Logger() : rec(json::object()), cur(&rec) {
      stk.push(cur);
    }

    ~Logger() {}

    // level + 1
    void vec(); 
    void vec(const string &key);

    void map();
    void map(const string &key);

    // stay on same level
    template<typename T> 
    void log(const T &msg);
    void log(const char *msg);

    template<typename T> 
    void log(const string &key, const T &msg);
    void log(const string &key, const char *msg);
    
    // level - 1
    void pop();

    // dump to file
    void dump(raw_ostream &stm, int indent = 2);
    void dump(const string &fn, int indent = 2);

  protected:
    json rec;
    json *cur;
    stack<json *> stk;
};

#endif /* LOGGER_H_ */
