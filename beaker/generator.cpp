// Copyright (c) 2015 Andrew Sutton
// All rights reserved

#include "generator.hpp"
#include "type.hpp"
#include "expr.hpp"
#include "stmt.hpp"
#include "decl.hpp"
#include "mangle.hpp"
#include "evaluator.hpp"

#include "llvm/IR/Type.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include <iostream>


// -------------------------------------------------------------------------- //
// Mapping of names

// Synthesize a name using the linkage model for
// the declaration's language. Currently, there
// are two linkage models:
//
//    - C
//    - Beaker
//
// NOTE: Currently, these are the same. However, these
// differ be as Beaker evolves.
String
Generator::get_name(Decl const* d)
{
  if (d->is_foreign())
    return d->name()->spelling();
  else
    return mangle(d);
}


// -------------------------------------------------------------------------- //
// Mapping of types
//
// The type generator transforms a beaker type into
// its correspondiong LLVM type.


llvm::Type*
Generator::get_type(Type const* t)
{
  struct Fn
  {
    Generator& g;
    llvm::Type* operator()(Id_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Boolean_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Character_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Integer_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Function_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Array_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Block_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Reference_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Record_type const* t) const { return g.get_type(t); }
  };
  return apply(t, Fn{*this});
}


// The program is unsound if we ever reach this
// function. Id-types are replaced with their
// referenced declarations during elaboration.
llvm::Type*
Generator::get_type(Id_type const* t)
{
  std::stringstream ss;
  ss << "unresolved id-type '" << *t->symbol() << '\'';
  throw std::runtime_error(ss.str());
}


// Return the 1 bit integer type.
llvm::Type*
Generator::get_type(Boolean_type const*)
{
  return build.getInt1Ty();
}


// Return the 8 bit integer type.
llvm::Type*
Generator::get_type(Character_type const*)
{
  return build.getInt8Ty();
}


// Return the 32 bit integer type.
llvm::Type*
Generator::get_type(Integer_type const*)
{
  return build.getInt32Ty();
}


// Return a function type.
llvm::Type*
Generator::get_type(Function_type const* t)
{
  std::vector<llvm::Type*> ts;
  ts.reserve(t->parameter_types().size());
  for (Type const* t1 : t->parameter_types())
    ts.push_back(get_type(t1));
  llvm::Type* r = get_type(t->return_type());
  return llvm::FunctionType::get(r, ts, false);
}


// Return an array type.
llvm::Type*
Generator::get_type(Array_type const* t)
{
  llvm::Type* t1 = get_type(t->type());
  Value v = evaluate(t->extent());
  return llvm::ArrayType::get(t1, v.get_integer());
}


// A chunk is just a pointer to an object
// of the underlying type.
llvm::Type*
Generator::get_type(Block_type const* t)
{
  llvm::Type* t1 = get_type(t->type());
  return llvm::PointerType::getUnqual(t1);
}


// Translate reference types into pointer types in the
// generic address space.
//
// TODO: Actually do this?
llvm::Type*
Generator::get_type(Reference_type const* t)
{
  llvm::Type* t1 = get_type(t->type());
  return llvm::PointerType::getUnqual(t1);
}


// Return the structure type corresponding to the
// declaration of t. If, for some reason, we encounter
// the type before its declaration, just emit the
// definition now.
llvm::Type*
Generator::get_type(Record_type const* t)
{
  auto const* bind = types.lookup(t->declaration());
  if (!bind) {
    // Note that we have to do a 2nd lookup because
    // we don't return anything from generating
    // declarations.
    gen(t->declaration());
    bind = types.lookup(t->declaration());
  }
  return bind->second;
}


// -------------------------------------------------------------------------- //
// Code generation for expressions
//
// An expression is transformed into a sequence instructions whose
// intermediate results are saved in registers.

llvm::Value*
Generator::gen(Expr const* e)
{
  struct Fn
  {
    Generator& g;
    llvm::Value* operator()(Literal_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Id_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Decl_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Add_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Sub_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Mul_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Div_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Rem_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Neg_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Pos_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Eq_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Ne_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Lt_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Gt_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Le_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Ge_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(And_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Or_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Not_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Call_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Dot_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Field_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Method_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Index_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Value_conv const* e) const { return g.gen(e); }
    llvm::Value* operator()(Block_conv const* e) const { return g.gen(e); }
    llvm::Value* operator()(Default_init const* e) const { return g.gen(e); }
    llvm::Value* operator()(Copy_init const* e) const { return g.gen(e); }
    llvm::Value* operator()(Reference_init const* e) const { return g.gen(e); }
  };

  return apply(e, Fn{*this});
}


// Return the value corresponding to a literal expression.
llvm::Value*
Generator::gen(Literal_expr const* e)
{
  // TODO: Write better type queries.
  //
  // TODO: Write a better interface for values.
  Value v = evaluate(e);
  Type const* t = e->type();
  if (t == get_boolean_type())
    return build.getInt1(v.get_integer());
  if (t == get_character_type())
    return build.getInt8(v.get_integer());
  if (t == get_integer_type())
    return build.getInt32(v.get_integer());

  // FIXME: How should we generate array literals? Are
  // these global constants or are they local alloca
  // objects. Does it depend on context?

  // A string literal produces a new global string constant.
  // and returns a pointer to an array of N characters.
  if (is_string(t)) {
    Array_value a = v.get_array();
    String s = a.get_string();

    // FIXME: This does not unify equivalent strings.
    // Maybe we needt maintain a mapping in order to
    // avoid redunancies.
    auto iter = strings.find(s);
    if (iter == strings.end()) {
      llvm::Value* v = build.CreateGlobalString(s);
      iter = strings.emplace(s, v).first;
    }
    return iter->second;
  }

  else
    throw std::runtime_error("cannot generate function literal");
}


llvm::Value*
Generator::gen(Id_expr const* e)
{
  lingo_unreachable();
}


// Returns the value associated with the declaration.
llvm::Value*
Generator::gen(Decl_expr const* e)
{
  auto const* bind = stack.lookup(e->declaration());
  llvm::Value* result = bind->second;

  // Fetch the value from a reference declaration.
  Decl const* decl = bind->first;
  if (is_reference(decl))
    return build.CreateLoad(result);

  return result;
}


llvm::Value*
Generator::gen(Add_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateAdd(l, r);
}


llvm::Value*
Generator::gen(Sub_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateSub(l, r);
}


llvm::Value*
Generator::gen(Mul_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateMul(l, r);
}


llvm::Value*
Generator::gen(Div_expr const* e)
{
  //TODO: Find the command to generate a multiply
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateSDiv(l, r);
}


llvm::Value*
Generator::gen(Rem_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateSRem(l, r);
}


llvm::Value*
Generator::gen(Neg_expr const* e)
{
  llvm::Value* l = gen(e);
  return build.CreateNeg(l);
}


llvm::Value*
Generator::gen(Pos_expr const* e)
{
  Value v = evaluate(e);
  return build.getInt32(v.get_integer());
}


llvm::Value*
Generator::gen(Eq_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpEQ(l, r);
}


llvm::Value*
Generator::gen(Ne_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpNE(l, r);
}


llvm::Value*
Generator::gen(Lt_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpSLT(l, r);
}


llvm::Value*
Generator::gen(Gt_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpSGT(l, r);
}


llvm::Value*
Generator::gen(Le_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpSLE(l, r);
}


llvm::Value*
Generator::gen(Ge_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpSGE(l, r);
}


llvm::Value*
Generator::gen(And_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateAnd(l, r);
}


llvm::Value*
Generator::gen(Or_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateOr(l, r);
}


llvm::Value*
Generator::gen(Not_expr const* e)
{
  llvm::Value* v = gen(e);
  return build.CreateNot(v);
}


// Note that method calls have been explicitly
// rewritten to free function calls.
llvm::Value*
Generator::gen(Call_expr const* e)
{
  llvm::Value* fn = gen(e->target());
  std::vector<llvm::Value*> args;
  for (Expr const* a : e->arguments())
    args.push_back(gen(a));
  return build.CreateCall(fn, args);
}


// NOTE: The IR builder will automatically compact
// nested member expressions into a single GEP
// instruction. We don't have to do anything more
// complex than this.
//
// FIXME: Rewrite this.
llvm::Value*
Generator::gen(Dot_expr const* e)
{
  lingo_unreachable();
}


llvm::Value*
Generator::gen(Field_expr const* e)
{
  llvm::Value* obj = gen(e->container());
  std::vector<llvm::Value*> args {
    build.getInt32(0),                  // 0th element from base
    build.getInt32(e->field()->index()) // nth element in struct
  };
  return build.CreateGEP(obj, args);
}


// Just generate the base object. This will be used
// as the argument for the method call.
llvm::Value*
Generator::gen(Method_expr const* e)
{
  return gen(e->container());
}


llvm::Value*
Generator::gen(Index_expr const* e)
{
  llvm::Value* arr = gen(e->array());
  llvm::Value* ix = gen(e->index());
  std::vector<llvm::Value*> args {
    build.getInt32(0), // 0th element from base
    ix                 // requested index
  };
  return build.CreateGEP(arr, args);
}


llvm::Value*
Generator::gen(Value_conv const* e)
{
  llvm::Value* v = gen(e->source());
  return build.CreateLoad(v);
}


llvm::Value*
Generator::gen(Block_conv const* e)
{
  // Generate the array value.
  llvm::Value* a = gen(e->source());

  // Decay the array pointer to an array into
  // a pointer to the first object. This effectively
  // returns a pointer to the first object in the
  // array.
  llvm::Value *zero = build.getInt32(0);
  llvm::Value *args[] = { zero, zero };
  return build.CreateInBoundsGEP(a, args);
}


// TODO: Return the value or store it?
llvm::Value*
Generator::gen(Default_init const* e)
{
  Type const* t = e->type();
  llvm::Type* type = get_type(t);

  // Scalar types should get a 0 value in the
  // appropriate type.
  if (is_scalar(t))
    return llvm::ConstantInt::get(type, 0);

  // Aggregate types are zero initialized.
  //
  // NOTE: This isn't actually correct. Aggregate types
  // should be memberwise default initialized.
  if (is_aggregate(t))
    return llvm::ConstantAggregateZero::get(type);

  throw std::runtime_error("unhahndled default initializer");
}


// TODO: Return the value or store it?
llvm::Value*
Generator::gen(Copy_init const* e)
{
  return gen(e->value());
}


llvm::Value*
Generator::gen(Reference_init const* e)
{
  return gen(e->object());
}


// -------------------------------------------------------------------------- //
// Code generation for statements
//
// The statement generator is responsible for
// the generation of statements at block scope.

void
Generator::gen(Stmt const* s)
{
  struct Fn
  {
    Generator& g;
    void operator()(Empty_stmt const* s) { g.gen(s); }
    void operator()(Block_stmt const* s) { g.gen(s); }
    void operator()(Assign_stmt const* s) { g.gen(s); }
    void operator()(Return_stmt const* s) { g.gen(s); }
    void operator()(If_then_stmt const* s) { g.gen(s); }
    void operator()(If_else_stmt const* s) { g.gen(s); }
    void operator()(While_stmt const* s) { g.gen(s); }
    void operator()(Break_stmt const* s) { g.gen(s); }
    void operator()(Continue_stmt const* s) { g.gen(s); }
    void operator()(Expression_stmt const* s) { g.gen(s); }
    void operator()(Declaration_stmt const* s) { g.gen(s); }
  };
  apply(s, Fn{*this});
}


void
Generator::gen(Empty_stmt const* s)
{
  
}


// Generate code for a sequence of statements.
// Note that this does not correspond to a basic
// block since we don't need any terminators
// in the following program.
//
//    {
//      { ; }
//    }
//
// We only need new blocks for specific control
// flow concepts.
void
Generator::gen(Block_stmt const* s)
{
  for (Stmt const* s1 : s->statements())
    gen(s1);
}


void
Generator::gen(Assign_stmt const* s)
{
  llvm::Value* lhs = gen(s->object());
  llvm::Value* rhs = gen(s->value());
  build.CreateStore(rhs, lhs);
}


void
Generator::gen(Return_stmt const* s)
{
  llvm::Value* v = gen(s->value());
  build.CreateRet(v);
}

//For these instructions I need to use a llvm::BasicBLock*
void
Generator::gen(If_then_stmt const* s)
{
  llvm::Value* cond_var = gen(s->condition());
  
  cond_var = build.CreateFCmpONE(cond_var, 
				 llvm::ConstantFP::get(llvm::getGlobalContext(), 
						 llvm::APFloat(0.0)), 
				 "if_cond");

  

  llvm::Function* function = build.GetInsertBlock()->getParent();

  llvm::BasicBlock* block_true 
    = llvm::BasicBlock::Create(llvm::getGlobalContext(), "true", function);

  llvm::BasicBlock* block_next = llvm::BasicBlock::Create(llvm::getGlobalContext(),
							  "next");

  build.CreateCondBr(cond_var, block_true, block_next);

  build.SetInsertPoint(block_true);

  gen(s->body());

  build.CreateBr(block_next);

  build.SetInsertPoint(block_next);
			      
  
}


void
Generator::gen(If_else_stmt const* s)
{
  llvm::Value* cond_var = gen(s->condition());
  
  cond_var = build.CreateFCmpONE(cond_var, 
				 llvm::ConstantFP::get(llvm::getGlobalContext(), 
						 llvm::APFloat(0.0)), 
				 "if_cond");

  

  llvm::Function* function = build.GetInsertBlock()->getParent();

  llvm::BasicBlock* block_true 
    = llvm::BasicBlock::Create(llvm::getGlobalContext(), "true", function);
  
  llvm::BasicBlock* block_false
    = llvm::BasicBlock::Create(llvm::getGlobalContext(), "else");

  llvm::BasicBlock* block_next = llvm::BasicBlock::Create(llvm::getGlobalContext(),
							  "next");

  build.CreateCondBr(cond_var, block_true, block_false);

  build.SetInsertPoint(block_true);

  gen(s->true_branch());

  build.CreateBr(block_next);

  build.SetInsertPoint(block_false);
  
  gen(s->false_branch());

  build.CreateBr(block_next);

  build.SetInsertPoint(block_next);
  
  //throw std::runtime_error("not implemented");
}


void
Generator::gen(While_stmt const* s)
{
  //llvm::Function* function = build.GetInsertBlock()->getParent();
  
  llvm::BasicBlock* body = llvm::BasicBlock::Create(llvm::getGlobalContext(), "body");

  llvm::BasicBlock* loop = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loop");

  llvm::BasicBlock* merge = llvm::BasicBlock::Create(llvm::getGlobalContext(), "merge");
  
  llvm::Value*  cond_var = gen(s->condition());

  cond_var = build.CreateFCmpONE(cond_var, 
				 llvm::ConstantFP::get(llvm::getGlobalContext(), 
						       llvm::APFloat(0.0)), 
				 "while_cond");


  build.SetInsertPoint(body);

  build.CreateCondBr(cond_var, loop, merge);

  build.SetInsertPoint(loop);

  gen(s->body());

  build.CreateBr(body);

  build.SetInsertPoint(merge);
  
  //throw std::runtime_error("not implemented");
}


void
Generator::gen(Break_stmt const* s)
{

  llvm::BasicBlock* break_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "break");

  build. CreateBr(break_block);

  build.SetInsertPoint(break_block);
  /*
  llvm::BasicBlock::iterator mod = build.GetInsertPoint();
  
  llvm::BasicBlock::iterator temp;

  for(auto x : mod)
    temp = x;

  build.CreateBr(temp);
  */
  //throw std::runtime_error("not implemented");
}


void
Generator::gen(Continue_stmt const* s)
{
  llvm::BasicBlock* continue_block = build.GetInsertBlock();

  build.CreateBr(continue_block);
  
  //throw std::runtime_error("not implemented");
}


void
Generator::gen(Expression_stmt const* s)
{
  gen(s->expression());
}


void
Generator::gen(Declaration_stmt const* s)
{
  gen(s->declaration());
}


// -------------------------------------------------------------------------- //
// Code generation for declarations
//
// TODO: We can't generate all of the code for a module
// in a single pass. We probably need to break this up
// into a number of smaller declaration generators. For
// example, generators that:
//
//    - produce declarations
//    - produce global initializers
//    - produce global destructors
//    - other stuff
//
// In, it might not be worthwhile to have a number
// of sub-generators that refer to the top-level
// generator.

void
Generator::gen(Decl const* d)
{
  struct Fn
  {
    Generator& g;
    void operator()(Variable_decl const* d) { return g.gen(d); }
    void operator()(Function_decl const* d) { return g.gen(d); }
    void operator()(Parameter_decl const* d) { return g.gen(d); }
    void operator()(Record_decl const* d) { return g.gen(d); }
    void operator()(Field_decl const* d) { return g.gen(d); }
    void operator()(Method_decl const* d) { return g.gen(d); }
    void operator()(Module_decl const* d) { return g.gen(d); }
  };
  return apply(d, Fn{*this});
}


void
Generator::gen_local(Variable_decl const* d)
{
  // Create the alloca instruction at the beginning of
  // the function. Not at the point where we get it.
  llvm::BasicBlock& b = fn->getEntryBlock();
  llvm::IRBuilder<> tmp(&b, b.begin());
  llvm::Type* type = get_type(d->type());
  String name = d->name()->spelling();
  llvm::Value* ptr = tmp.CreateAlloca(type, nullptr, name);

  // Save the decl binding.
  stack.top().bind(d, ptr);

  // Generate the initializer.
  llvm::Value* init = gen(d->init());

  // Store the result in the object.
  build.CreateStore(init, ptr);
}


void
Generator::gen_global(Variable_decl const* d)
{
  String      name = get_name(d);
  llvm::Type* type = get_type(d->type());

  // Try to generate a constant initializer.
  llvm::Constant* init = nullptr;
  if (!d->is_foreign()) {

    // FIXME: If the initializer can be reduced to a value,
    // then generate that constant. If not, we need dynamic
    // initialization of global variables.

    init = llvm::Constant::getNullValue(type);

    // llvm::Value* val = gen(d->init());
    // if (llvm::Constant* c = llvm::dyn_cast<llvm::Constant>(val)) {
    //   init = c;
    // }
  }


  // Note that the aggregate 0 only applies to aggregate
  // types. We can't apply it to initializers for scalars.

  // Build the global variable, automatically adding
  // it to the module.
  llvm::GlobalVariable* var = new llvm::GlobalVariable(
    *mod,                                  // owning module
    type,                                  // type
    false,                                 // is constant
    llvm::GlobalVariable::ExternalLinkage, // linkage,
    init,                                  // initializer
    name                                   // name
  );

  // Create a binding for the new variable.
  stack.top().bind(d, var);
}


// Generate code for a variable declaration. Note that
// code generation depends heavily on context. Globals
// and locals are very different.
//
// TODO: If we add class/record types, then we also
// need to handle member variables as well. Maybe.
void
Generator::gen(Variable_decl const* d)
{
  if (is_global_variable(d))
    return gen_global(d);
  else
    return gen_local(d);
}


void
Generator::gen(Function_decl const* d)
{
  String name = get_name(d);
  llvm::Type* type = get_type(d->type());

  // Build the function.
  llvm::FunctionType* ftype = llvm::cast<llvm::FunctionType>(type);
  fn = llvm::Function::Create(
    ftype,                           // function type
    llvm::Function::ExternalLinkage, // linkage
    name,                            // name
    mod);                            // owning module

  // Create a new binding for the variable.
  stack.top().bind(d, fn);

  // If the declaration is not defined, then don't
  // do any of this stuff...
  if (!d->body())
    return;

  // Establish a new binding environment for declarations
  // related to this function.
  Symbol_sentinel scope(*this);

  // Build the argument list. Note that
  {
    auto ai = fn->arg_begin();
    auto pi = d->parameters().begin();
    while (ai != fn->arg_end()) {
      Decl const* p = *pi;
      llvm::Argument* a = &*ai;
      a->setName(p->name()->spelling());

      // Create an initial name binding for the function
      // parameter. Note that we're going to overwrite
      // this when we create locals for each parameter.
      stack.top().bind(p, a);

      ++ai;
      ++pi;
    }
  }

  // Build the entry point for the function
  // and make that the insertion point.
  llvm::BasicBlock* b = llvm::BasicBlock::Create(cxt, "entry", fn);
  build.SetInsertPoint(b);

  // TODO: Create a local variable for the return value.
  // Return statements will write here.
  ret = build.CreateAlloca(fn->getReturnType());

  // Generate a local variable for each of the variables.
  for (Decl const* p : d->parameters())
    gen(p);

  // Generate the body of the function.
  gen(d->body());

  // TODO: Create an exit block and allow code to
  // jump directly to that block after storing
  // the return value.

  // Reset stateful info.
  ret = nullptr;
  fn = nullptr;
}


void
Generator::gen(Parameter_decl const* d)
{
  llvm::Type* t = get_type(d->type());
  llvm::Value* a = stack.top().get(d).second;
  llvm::Value* v = build.CreateAlloca(t);
  stack.top().rebind(d, v);
  build.CreateStore(a, v);
}


// Generate a new struct type.
void
Generator::gen(Record_decl const* d)
{
  // If we've already created a type, don't do
  // anything else.
  if (types.lookup(d))
    return;

  // If the record is empty, generate a struct
  // with exactly one byte so that we never have
  // a type with 0 size.
  //
  // FIXME: This isn't right because we are currently
  // mixing methods and fields in the same thing.
  // They need to be separate.
  std::vector<llvm::Type*> ts;
  if (d->fields().empty()) {
    ts.push_back(build.getInt8Ty());
  } else {
    // Construct the type over only the fields.
    for (Decl const* f : d->fields())
      ts.push_back(get_type(f->type()));
  }

  // This will automatically be added to the module,
  // but if it's not used, then it won't be generated.
  llvm::Type* t = llvm::StructType::create(cxt, ts, d->name()->spelling());
  types.bind(d, t);

  // Now, generate code for all other members.
  for (Decl const* m : d->members())
    gen(m);
}


void
Generator::gen(Field_decl const* d)
{
  // NOTE: We should never actually get here.
  lingo_unreachable();
}



// Just call out to the function generator. Name
// mangling is handled in get_name().
void
Generator::gen(Method_decl const* d)
{
  gen(cast<Function_decl>(d));
}


void
Generator::gen(Module_decl const* d)
{
  // Establish the global binding environment.
  Symbol_sentinel scope(*this);

  // Initialize the module.
  //
  // TODO: Make the output name the ".ll" version of the
  // the input name. Although this might also depend on
  // whether we're generating IR or object code?
  assert(!mod);
  mod = new llvm::Module("a.ll", cxt);

  // Generate all top-level declarations.
  for (Decl const* d1 : d->declarations())
    gen(d1);

  // TODO: Make a second pass to generate global
  // constructors for initializers.
}


llvm::Module*
Generator::operator()(Decl const* d)
{
  assert(is<Module_decl>(d));
  gen(d);
  return mod;
}
