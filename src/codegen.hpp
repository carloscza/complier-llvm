#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/NoFolder.h"
#include <unordered_map>
#include <string> 
#include <vector>
#include "tools.hpp"
#include "lexer.hpp"
#include "parser.hpp"

namespace llvm 
{

    struct Compiler
    {
        Compiler(my_lexer::u8 *text) :
            prog(my_parser::Parser{text}()),
            ctx(),
            mod("main.cpp", ctx),
            builder(ctx)
        {
            mod.setTargetTriple(Triple(sys::getDefaultTargetTriple()).str());
            setup();
            gen_prog(prog);
        }

        ~Compiler()
        {
            builder.CreateRet(builder.getInt32(0));

            if constexpr (true) { mod.print(outs(), 0); }

            if (verifyModule(mod, &errs())) { ABORT("Module verification failed"); }

            std::error_code error_opening_file;
            raw_fd_ostream file("main.bc", error_opening_file);
            if (error_opening_file) { ABORT("error writing to main.bc: " << error_opening_file); }
            WriteBitcodeToFile(mod, file);
        }
        
        private:
            my_parser::Program prog;
            LLVMContext ctx;
            Module mod;
            IRBuilder<NoFolder> builder;
            std::unordered_map<std::string, Function *> functions;
            std::unordered_map<std::string, Value *> formats;

            std::vector<BasicBlock*> continue_stack, break_stack;




            struct SymbolTable
            {
                // Variable/Array struct:
                struct Symbol
                {
                    Value *alloca; // mem location of var/arr
                    bool is_array; // var or arr ?
                };


                SymbolTable() { ++(*this); } // Global scope is a scope and must track.

                void operator++() { tables.push_back({}); } // push scope to stack.
                void operator--() { tables.pop_back();    } // pop scope from stack.

                Symbol operator[](my_lexer::i32 name)
                {
                    // Search for variable within each scope in the stack of scopes.
                    for (size_t i = tables.size() - 1 ; i != (size_t)-1 ; --i)
                    {
                        if(tables[i].contains(name)) { return tables[i][name]; }
                    }

                    // Search did not find variable/array.
                    ABORT("Failed to find symbol " << my_lexer::ids[name]);
                }
                
                void push(my_lexer::i32 name, Value *alloca, bool is_array) { tables.back()[name] = {alloca, is_array}; } // pushing variables/arrays in tables/scopes.

                private:
                    std::vector<std::unordered_map<my_lexer::i32, Symbol>> tables; // a vector of scopes. outer index scope level. inner index var name.

            } symbols; // declare a SymbolTable named 'symbols'






            Value* get_fmt(const char *fmt)
            {
                Value *&res = formats[fmt];
                if (!res) { res = builder.CreateGlobalString(fmt, "", 0, &mod); }
                return res;
            }

            void gen_prog(my_parser::Program& p) { for(auto& s : p.body) { gen_stmt(s); } }


            void gen_stmt(my_parser::Stmt& s) 
            {
                if(builder.GetInsertBlock()->getTerminator()) { return; }

                auto gen_block = [&](my_parser::Block& block){
                    ++symbols; // push the block's scope on to stack of scopes (an empty hash table).
                    for(auto& s : block.body) { gen_stmt(s); } // create IR for each stmt in block.
                    --symbols; // pop its scope/hash table.
                };
                s(
                    gen_block,
                    [&](my_parser::Let& let) { // Generate IR for 'let' stmts.
                        let.body(
                            [&](my_parser::Variable& v) {
                                AllocaInst *alloca = builder.CreateAlloca(builder.getInt32Ty(), nullptr, ""); // allocate mem for a single 32-bit.
                                symbols.push(v.name, alloca, false); // push variable to current scope hash table to track.
                            },
                            [&](my_parser::Array& arr) {
                                arr.size[0]( // an expr
                                    [&](my_parser::IntLiteral& lit) {
                                        AllocaInst *alloca = builder.CreateAlloca(builder.getInt32Ty(), builder.getInt32(lit.body), ""); // allocate mem for IntLiteral many 32-bits.
                                        symbols.push(arr.name, alloca, true); // push variable to current scope hash table.
                                    },
                                    [&](auto&) { ABORT("Tried to declare array with non IntLiteral size"); } // enforce constant int size for array declaration.
                                );
                            },
                            [&](auto&){ ABORT ("Let tried to declare non var or array"); }
                        );
                    },
                    [&](my_parser::Assign& ass) { // Generate IR for Assign stmts.
                        ass.lhs(
                            [&](my_parser::Variable& v) {   // lhs expr is a variable
                                Value *rhs = gen_expr(ass.rhs); // get rhs expr
                                auto symbol = symbols[v.name]; //  check if variable exists/declared (if not SymbolTable op[] handles with ABORT).
                                if(symbol.is_array) { ABORT("Tried to assign to a variable as array"); } // "variable"(identifier) is actually an array so can't assign.
                                builder.CreateStore(rhs, symbol.alloca); // store rhs into lhs mem location.
                            },
                            [&](my_parser::Array& arr) {  // lhs expr is an array.
                                Value *rhs = gen_expr(ass.rhs); // get rhs expr.
                                auto symbol = symbols[arr.name]; // check is exists/declared.
                                if(!symbol.is_array) { ABORT("Tried to assign to array as variable"); } // identifier was used like an array but not array so can't assign.
                                Value *index = gen_expr(arr.size[0]); // generate array's index which is an expr.
                                Value *gep = builder.CreateGEP(builder.getInt32Ty(), symbol.alloca, index); // calculate mem location of array index (offset).
                                builder.CreateStore(rhs, gep); // store rhs into array location.
                            },
                            [&](auto&){ ABORT("Assigning to non var or array?"); }
                        );
                    },
                    [&](my_parser::Break& stmt){
                        if(!break_stack.size()) { ABORT("Break outside of a loop"); } // if no loop stmt (empty). Nowhere to branch to.
                                                                                      // break; causes a branch.
                        builder.CreateBr(break_stack.back());
                    },
                    [&](my_parser::Continue& stmt){
                        if(!continue_stack.size()) { ABORT("Continue outside of a loop"); } // if no loop stmt (empty). Nowhere to branch to.
                                                                                            // continue; causes a branch.
                        builder.CreateBr(continue_stack.back());
                    },
                    [&](my_parser::Loop& stmt){
                        BasicBlock *current_block = builder.GetInsertBlock(); // save bb before loop.
                        BasicBlock *loop_block    = BasicBlock::Create(ctx, "", current_block->getParent()); // create new bb for loop block.
                        BasicBlock *merge_block   = BasicBlock::Create(ctx, "", current_block->getParent()); // create new bb for block after loop.

                        continue_stack.push_back(loop_block); // push loop block to stack (continue branches to top of loop block).
                        break_stack.push_back(merge_block);   // push next sequential block to stack (break branche to next block).

                        builder.CreateBr(loop_block);         // start loop.

                        builder.SetInsertPoint(loop_block);   // point/move builder to loop block to inster IR for its body.
                        gen_block(stmt.body);                 // make IR for loop block instructions.

                        if(!builder.GetInsertBlock()->getTerminator()) { builder.CreateBr(loop_block); } // keep branching to top of loop if no terminator (break/continue).
                        
                        // loop done.
                        builder.SetInsertPoint(merge_block);  // point/move builder to block after loop. 

                        continue_stack.pop_back();            // pop last loop block.
                        break_stack.pop_back();               // pop last loop block.
                    },
                    [&](my_parser::If& stmt){
                        BasicBlock *current_block = builder.GetInsertBlock(); // save bb before if-stmt.
                        BasicBlock *if_block = BasicBlock::Create(ctx, "", current_block->getParent()); // create new bb for if-block
                        BasicBlock *else_block = BasicBlock::Create(ctx, "", current_block->getParent()); // create new bb for else-block
                        BasicBlock *merge_block = BasicBlock::Create(ctx, "", current_block->getParent()); // create new bb for block after if-else.

                        Value *cond = i32toi1(gen_expr(stmt.cond[0]));  // convert 32-bit to 1-bit for cond-br.
                        builder.CreateCondBr(cond, if_block, else_block); // cond-branch to if-block or else-block based on cond.

                        builder.SetInsertPoint(if_block); // point/move builder to if-block bb.
                        gen_block(stmt.body);             // make IR for if-block (goes in if-block bb).
                        if(!builder.GetInsertBlock()->getTerminator()) { builder.CreateBr(merge_block); } // branch to bb after if-else stmt.

                        builder.SetInsertPoint(else_block); // point/move builder to else-block bb.

                        if(stmt.else_body) { gen_block(*stmt.else_body); } // make IR for else-block.
                        if(!builder.GetInsertBlock()->getTerminator()) { builder.CreateBr(merge_block); } // branch to bb after if-else stmt (bc no fall through).

                        builder.SetInsertPoint(merge_block); // move/point builder to bb after if-else stmt.
                    },
                    [&](my_parser::Nop& stmt){},
                    [&](my_parser::Expr& expr){
                        builder.CreateCall(functions["printf"], {get_fmt("%d\n"), gen_expr(expr)});
                    }
                );
            }

            void setup()
            {
                {
                    FunctionType* sig = FunctionType::get(Type::getInt32Ty(ctx), {}, false);
                    functions["main"] = Function::Create(sig, GlobalValue::ExternalLinkage, "main", mod);
                    BasicBlock *block = BasicBlock::Create(ctx, "entry", functions["main"]);
                    builder.SetInsertPoint(block);
                }

                {
                    FunctionType* sig = FunctionType::get(Type::getInt32Ty(ctx), {builder.getPtrTy()}, true);
                    functions["printf"] = Function::Create(sig, GlobalValue::ExternalLinkage, "printf", mod);
                }
            }

            Value* i1toi32(Value *i1)
            {
                return builder.CreateZExt(i1, builder.getInt32Ty());
            }

            Value* i32toi1(Value *i32)
            {
                return builder.CreateICmpNE(i32, builder.getInt32(0));
            }

            Value* gen_expr(my_parser::Expr& e)
            {
                return e (
                    [&](my_parser::Variable& v) -> Value * { // Expr is variable
                        auto symbol = symbols[v.name]; // check if var exists/declared
                        if(symbol.is_array) { ABORT("We're not going to allow pointer math, simplifies our type system"); } // variable actually an array so it would be mem address (pointer). not allowed in lang.
                        return builder.CreateLoad(builder.getInt32Ty(), symbol.alloca); // load value of variable to register.
                    },
                    [&](my_parser::Array& arr) -> Value * { // Expr is an array
                        auto symbol = symbols[arr.name]; // check if exists
                        if(!symbol.is_array) { ABORT("We don't have index operator overloads"); } // using variable as array. does not have [] operator overloads.
                        Value *index = gen_expr(arr.size[0]); // generate index (an expr).
                        Value *gep = builder.CreateGEP(builder.getInt32Ty(), symbol.alloca, index); // get correct location withing array (offset).
                        return builder.CreateLoad(builder.getInt32Ty(), gep); // load array value into register.
                    }, 
                    [&](my_parser::IntLiteral& lit) -> Value* { return builder.getInt32(lit.body); },
                    [&](my_parser::MathOp& op) {
                        if(op.op == '||' || op.op == '&&')
                        {
                            Value *lhs = gen_expr(op.body[0]);

                            BasicBlock *current_block = builder.GetInsertBlock();
                            BasicBlock *second_eval_block = BasicBlock::Create(ctx, "", current_block->getParent());

                            BasicBlock *merge_block = BasicBlock::Create(ctx, "", current_block->getParent());

                            Value *lhs_as_i1 = i32toi1(lhs);
                            if (op.op == '||')
                            {
                                builder.CreateCondBr(lhs_as_i1, merge_block, second_eval_block);
                            }
                            else
                            {
                                builder.CreateCondBr(lhs_as_i1, second_eval_block, merge_block);
                            }
                            

                            builder.SetInsertPoint(second_eval_block);
                            Value *rhs_as_i1 = i32toi1(gen_expr(op.body[1]));
                            second_eval_block = builder.GetInsertBlock();
                            builder.CreateBr(merge_block);

                            builder.SetInsertPoint(merge_block);
                            PHINode *phi = builder.CreatePHI(builder.getInt1Ty(), 2);
                            phi->addIncoming(lhs_as_i1, current_block);
                            phi->addIncoming(rhs_as_i1, second_eval_block);
                            return i1toi32(phi);
                        }

                        std::vector<Value *> args;
                        for (my_parser::Expr& ex : op.body) { args.push_back(gen_expr(ex)); }
                        switch(op.op) {
                            case '~': { return builder.CreateNot(args[0]); } break;
                            case '!': { return i1toi32(builder.CreateICmpEQ(args[0], builder.getInt32(0))); } break;
                            case '<<': { return builder.CreateShl(args[0], args[1]); } break;
                            case '>>': { return builder.CreateAShr(args[0], args[1]); } break;
                            case '&': { return builder.CreateAnd(args[0], args[1]); } break;
                            case '*': { return builder.CreateMul(args[0], args[1]); } break;
                            case '/': { return builder.CreateSDiv(args[0], args[1]); } break;
                            case '%': { return builder.CreateSRem(args[0], args[1]); } break;
                            case '+': { 
                                if(op.body.size() == 1) {
                                    return args[0];
                                }
                                else {
                                    return builder.CreateAdd(args[0], args[1]);
                                }
                            } break;
                            case '-': {
                                if(op.body.size() == 1) {
                                    return builder.CreateSub(builder.getInt32(0), args[0]);
                                }
                                else {
                                    return builder.CreateSub(args[0], args[1]);
                                }
                            } break;
                            case '^': { return builder.CreateXor(args[0], args[1]); } break;
                            case '|': { return builder.CreateOr(args[0], args[1]); } break;
                            case '>':  { return i1toi32(builder.CreateICmpSGT(args[0], args[1])); } break;
                            case '<':  { return i1toi32(builder.CreateICmpSLT(args[0], args[1])); } break;
                            case '<=': { return i1toi32(builder.CreateICmpSLE(args[0], args[1])); } break;
                            case '>=': { return i1toi32(builder.CreateICmpSGE(args[0], args[1])); } break;
                            case '==': { return i1toi32(builder.CreateICmpEQ(args[0], args[1])); } break;
                            case '!=': { return i1toi32(builder.CreateICmpNE(args[0], args[1])); } break;
                            default: { ABORT("unhandled MathOp? " << my_tools::token_to_string(op.op)); } break;
                        }
                    },
                    [&](auto&) { ABORT("unhandled expression?"); }
                );
            }
    };

} // end - llvm namespace