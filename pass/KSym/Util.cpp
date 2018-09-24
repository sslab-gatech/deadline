#include "Project.h"

string Helper::getValueName(Value *v) {
  if(!v->hasName()){
    return to_string(reinterpret_cast<uintptr_t>(v));
  } else {
    return v->getName().str();
  }
}

string Helper::getValueType(Value *v) {
  if(Instruction *inst = dyn_cast<Instruction>(v)){
    return string(inst->getOpcodeName());
  } else {
    return string("value " + to_string(v->getValueID()));
  }
}

string Helper::getValueRepr(Value *v) {
  string str;
  raw_string_ostream stm(str);

  v->print(stm);
  stm.flush();

  return str;
}

string Helper::getCtypeRepr(Type *t) {
  string str;
  raw_string_ostream stm(str);

  t->print(stm);
  stm.flush();

  return str;
}

string Helper::getExprType(Z3_context ctxt, Z3_ast ast) {
  return string(Z3_sort_to_string(ctxt, Z3_get_sort(ctxt, ast)));
}

string Helper::getExprRepr(Z3_context ctxt, Z3_ast ast) {
  return string(Z3_ast_to_string(ctxt, ast));
}

void Helper::convertDotInName(string &name) {
  std::replace(name.begin(), name.end(), '.', '_');
}

void Dumper::valueName(Value *val) {
  errs() << Helper::getValueName(val) << "\n";
}

void Dumper::typedValue(Value *val) {
  errs() 
    << "[" << Helper::getValueType(val) << "]"
    << Helper::getValueRepr(val)
    << "\n";
}

void Dumper::ctypeValue(Value *val) {
  errs()
    << "[" << Helper::getCtypeRepr(val->getType()) << "]"
    << Helper::getValueRepr(val)
    << "\n";
}

void Dumper::typedExpr(Z3_context ctxt, Z3_ast ast) {
  errs() 
    << "[" << Helper::getExprType(ctxt, ast) << "]"
    << Helper::getExprRepr(ctxt, ast)
    << "\n";
}
