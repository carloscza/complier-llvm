// parser.hpp

#ifndef PARSER_HPP
#define PARSER_HPP

#include <variant>
#include "lexer.hpp"
#include <vector>

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
    struct Expr : public Var<IntLiteral, MathOp> { using Var<IntLiteral, MathOp>::Var; };
    struct Stmt { Expr body; };
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

            Stmt parse_stmt() 
            {
                Stmt s;
                s.body = parse_expr();
                expect(';');
                return s;
            }

            Expr parse_expr() { return parse_or(); }
            
            Expr parse_primary()
            {
                if ('(' == *lex)
                {
                    ++lex;
                    Expr res = parse_expr();
                    expect(')');
                    return res;
                }
                return IntLiteral{expect('int')};
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