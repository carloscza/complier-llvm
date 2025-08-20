// parser.hpp

#ifndef PARSER_HPP
#define PARSER_HPP

#include <variant>
#include "lexer.hpp"
#include <vector>
#include <optional>
#include <iostream>


namespace my_parser
{
    template<class... Variants>
    struct Var : public std::variant<Variants...>
    {
        using std::variant<Variants...>::variant;

        template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
        template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

        template<typename... Visitors>
        auto operator()(Visitors&&... v) -> decltype(std::visit(overloaded{std::forward<Visitors>(v)...}, *this))
        {
            return std::visit(overloaded{std::forward<Visitors>(v)...}, *this);
        }
    };

    struct Expr;
    struct IntLiteral { my_lexer::i32 body; };
    struct MathOp { int op; std::vector<Expr> body; };

    struct Variable { my_lexer::i32 name; }; 
    struct Array    { my_lexer::i32 name; std::vector<Expr> size; }; // size is Expr since it covers both MathOp and IntLiteral.
                                                                     // semantic check in codegen (declaration v. access).
                                                                     // vector bc cycle between Expr and Array (infinite recursion since Expr variant of Array).

    struct Expr : public Var<IntLiteral, MathOp, Variable, Array> { using Var<IntLiteral, MathOp, Variable, Array>::Var; };
    
    struct Stmt;

    struct Block { std::vector<Stmt> body; };
    struct Break {};
    struct Continue {};
    struct Loop { Block body; }; 
    struct If { std::vector<Expr> cond; Block body; std::optional<Block> else_body; };
    struct Nop {};
    

    struct Let { Expr body; };
    struct Assign { Expr lhs, rhs; };

    struct Stmt : public Var<Block, Break, Continue, Loop, If, Nop, Expr, Let, Assign> {
        using Var<Block, Break, Continue, Loop, If, Nop, Expr, Let ,Assign>::Var;
    };
    struct Program { std::vector<Stmt> body; };

    struct Parser
    {
        Parser(my_lexer::u8 *text)
            : lex{text}
        {}
        
        my_lexer::i32 expect (int token)
            {
                if (token != *lex) [[unlikely]] { ABORT("You've done a bad thing mr crabs."); }
                my_lexer::i32 res = lex.get_value();
                ++lex;
                return res;
            }

        Program operator()() { return parse_program(); }
        
        private:
            my_lexer::Lexer lex;

            Program parse_program()
            {
                Program p;
                while(*lex) { p.body.push_back(parse_stmt()); }
                return p;
            }

            // BLOCK -> '{' STMT* '}'
            Block parse_block()
            {
                expect('{');
                std::vector<Stmt> body;
                while(*lex && *lex != '}') { body.push_back(parse_stmt()); }  
                expect('}');
                return Block{std::move(body)};
            }

            /*
            STMT -> 'break' ';'
                | 'continue' ';'
                | 'loop' BLOCK
                |  BLOCK
                | ';'
                | 'if' EXPR BLOCK ('else' BLOCK)?
                |  EXPR ';'
                | 'let' VARIABLE ';'
                |  EXPR '=' EXPR ';'
            */
            Stmt parse_stmt() 
            {
                switch(*lex)
                {
                    case 'brk':  { ++lex; expect(';'); return Break{};    } break; // 'break' ';'
                    case 'cont': { ++lex; expect(';'); return Continue{}; } break; // 'continue' ';'
                    case 'loop': { ++lex; return Loop{parse_block()};     } break; // 'loop' BLOCK
                    case '{':    { return parse_block();  } break;                 //  BLOCK
                    case ';':    { ++lex; return Nop{};   } break;                 //  ';'
                    case 'if':                                                     //  'if' EXPR BLOCK ('else' BLOCK)? 
                    {
                        ++lex;                                                     // 'if'
                        Expr cond  = parse_expr();                                 // EXPR
                        Block body = parse_block();                                // BLOCK
                        std::optional<Block> else_body;
                        if(*lex == 'else')
                        {
                            ++lex;                                                 // 'else'
                            else_body = parse_block();                             // BLOCK
                        }
                        return If{{std::move(cond)}, std::move(body), std::move(else_body)}; // return If struct.
                    } break;
                    case 'let': { ++lex; return Let{parse_variable()}; } break;   // 'let VARIABLE ';'
                    default:                                                       // EXPR ';'
                    {
                        auto lhs = parse_expr();
                        // Check if an Assign stmt:
                        if ( *lex == '=')
                        {
                            ++lex;
                            auto rhs = parse_expr();              // get rhs expression being assigned to lhs.
                            expect(';');
                            return Assign{move(lhs), move(rhs)};  // return assignment.
                        }
                        expect(';');
                        return lhs;                               // otherwise, just an expression.
                    } break;
                }
            }

            Expr parse_expr() { return parse_or(); }


            // VARIABLE -> ID ('[' EXPR ']')?
            Expr parse_variable()
            {
                my_lexer::i32 name = expect('id');

                // Check whether Array or Variable:
                // Bracket means Array:
                if(*lex == '[')
                {
                    ++lex;
                    auto size = parse_expr();
                    expect(']');
                    return Array{name, {move(size)}}; // return array.
                }

                // Its a Variable:
                return Variable{name}; // return variable.
            }


            /*
            PRIMARY -> INT
                    |  VARIABLE            
                    |  '(' EXPR ')'
            */
            Expr parse_primary()
            {
                if ('(' == *lex)
                {
                    ++lex;
                    Expr res = parse_expr();
                    expect(')');
                    return res;                                   // EXPR
                }
                if ('id' == *lex) { return parse_variable(); }    // VARIABLE
                return IntLiteral{expect('int')};                 // INT
            }

            Expr parse_unary()
            {
                while(1)
                {
                    switch(*lex)
                    {
                        case '+': case '-': case '~': case '!':
                        {
                            int op = *lex;
                            ++lex;
                            return MathOp{op, {parse_unary()}};
                        }
                        break;
                        default: { return parse_primary(); } break;
                    }
                }
            }

            Expr parse_mul()
            {
                Expr lhs = parse_unary();
                while(1)
                {
                    switch (*lex)
                    {
                        case '<<': case '>>': case '&': case '*': case '/': case '%':
                        {
                            int op = *lex;
                            ++lex;
                            Expr rhs = parse_unary();
                            lhs = MathOp{op, {std::move(lhs), std::move(rhs)}};
                        }
                        break;
                        default: { return lhs; } break;
                    }
                }
            }

            Expr parse_add()
            {
                Expr lhs = parse_mul();
                while(1)
                {
                    switch (*lex)
                    {
                        case '+': case '-': case '^': case '|':
                        {
                            int op = *lex;
                            ++lex;
                            Expr rhs = parse_mul();
                            lhs = MathOp{op, {std::move(lhs), std::move(rhs)}};
                        }
                        break;
                        default: { return lhs; } break;
                    }
                }
            }

            Expr parse_rel() 
            {
                Expr lhs = parse_add();
                while(1)
                {
                    switch (*lex)
                    {
                        case '<': case '>': case '<=': case '>=': case '==': case '!=':
                        {
                            int op = *lex;
                            ++lex;
                            Expr rhs = parse_add();
                            lhs = MathOp{op, {std::move(lhs), std::move(rhs)}};
                        }
                        break;
                        default: { return lhs; } break;
                    }
                }
            }

            Expr parse_and()
            {
                Expr lhs = parse_rel();
                while(*lex == '&&')
                {
                    ++lex;
                    Expr rhs = parse_rel();
                    lhs = MathOp{'&&', {std::move(lhs), std::move(rhs)}};
                }
                return lhs;
            }

            Expr parse_or()
            {
                Expr lhs = parse_and();
                while(*lex == '||')
                {
                    ++lex;
                    Expr rhs = parse_and();
                    lhs = MathOp{'||', {std::move(lhs), std::move(rhs)}};
                }
                return lhs;
            }
    };
} // end my_parser namespace

#endif