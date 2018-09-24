#ifndef UTIL_H_
#define UTIL_H_

#include "Project.h"

class Helper {
  public:
    // LLVM value
    static string getValueName(Value *v);
    static string getValueType(Value *v);
    static string getValueRepr(Value *v);
    static string getCtypeRepr(Type *t);

    // Z3 expr
    static string getExprType(Z3_context ctxt, Z3_ast ast);
    static string getExprRepr(Z3_context ctxt, Z3_ast ast);

    // string conversion
    static void convertDotInName(string &name);
};

class Dumper {
  public:
    Dumper() {}
    ~Dumper() {}

    // LLVM value
    void valueName(Value *val);
    void typedValue(Value *val);
    void ctypeValue(Value *val);

    // Z3 expr
    void typedExpr(Z3_context ctxt, Z3_ast ast);
};

#endif /* UTIL_H_ */
