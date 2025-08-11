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

            Value* get_fmt(const char *fmt)
            {
                Value *&res = formats[fmt];
                if (!res) { res = builder.CreateGlobalString(fmt, "", 0, &mod); }
                return res;
            }

            void gen_prog(my_parser::Program& p) { for(auto& s : p.body) { gen_stmt(s); } }
            void gen_stmt(my_parser::Stmt& s) 
            {
                builder.CreateCall(functions["printf"], {get_fmt("%d\n"), gen_expr(s.body)});
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