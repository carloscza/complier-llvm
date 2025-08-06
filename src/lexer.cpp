#include <iostream>
#include <climits>
#include <cassert>
#include <vector>
#include <unordered_map>
#include <string_view>
#include "lexer.hpp"

namespace my_lexer
{
    static IDManager ids;

    void Lexer::init_keywords()
    {
        auto init_keyword = [&](std::string_view name, int value)
        {
            i32 id = ids[name];
            while(keywords.size() <= id) { keywords.push_back(-1); }
            keywords[id] = value;
        };
        init_keyword("let", 'let');
        init_keyword("break", 'brk');
        init_keyword("continue", 'cont');
        init_keyword("return", 'ret');
        init_keyword("loop", 'loop');
        init_keyword("if", 'if');
        init_keyword("else", 'else');
    }

    int Lexer::lex() 
    { 
        static constexpr LUT is_ws(" \n\t\r\v\f");
        while(is_ws(*lex_iter)) { line += *lex_iter == '\n'; ++lex_iter; }

        static constexpr LUT is_mono(";~^*%():{}[]+-,", true);
        if(is_mono(*lex_iter)) { return *lex_iter++; }

        static constexpr LUT is_digit("0123456789");
        if(is_digit(*lex_iter))
        {
            value = 0;
            while(is_digit(*lex_iter))
            {
                if(__builtin_mul_overflow(value, 10, &value)) { ABORT("D:"); }
                if(__builtin_add_overflow(value, *lex_iter++ - '0', &value)) { ABORT("D:<"); }
            }
            return 'int';
        }

        static constexpr LUT    is_id("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");
        static constexpr LUT still_id("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789");
        if(is_id(*lex_iter))
        {
            u8 *start = lex_iter;
            while (still_id(*lex_iter)) { ++lex_iter; }
            u8 *end   = lex_iter;
            value = ids[start, end];

            if ( value < keywords.size() && keywords[value] > -1) { return keywords[value]; }
            return 'id';
        }

        switch (*lex_iter)
        {
            case '/':
                {
                    if(lex_iter[1] == '/') { return lex_comment(); }
                    return *lex_iter++;
                }
                break;
            case '|':
                {
                    if(lex_iter[1] == '|') { lex_iter += 2; return '||';}
                    return *lex_iter++;
                }
                break;
            case '&':
                {
                    if(lex_iter[1] == '&') { lex_iter += 2; return '&&';}
                    return *lex_iter++;
                }
                break;
            case '=':
                {
                    if(lex_iter[1] == '=') { lex_iter += 2; return '==';}
                    return *lex_iter++;
                }
                break;
            case '!':
                {
                    if(lex_iter[1] == '=') { lex_iter += 2; return '!=';}
                    return *lex_iter++;
                }
                break;
            case '<':
                {
                    if(lex_iter[1] == '<') { lex_iter += 2; return '<<';}
                    if(lex_iter[1] == '=') { lex_iter += 2; return '<=';}
                    return *lex_iter++;
                }
                break;
            case '>':
                {
                    if(lex_iter[1] == '>') { lex_iter += 2; return '>>';}
                    if(lex_iter[1] == '=') { lex_iter += 2; return '>=';}
                    return *lex_iter++;
                }
                break;
            default:
                {
                    ABORT(" D: " << (char)*lex_iter << ", " << (int)*lex_iter << "?\n");
                }
                break;
        }
        return *lex_iter++; 
    }

    int Lexer::lex_comment()
    {
        lex_iter += 2;
        static constexpr LUT is_new_line("\n", true);
        while(!is_new_line(*lex_iter)) { ++lex_iter; }
        return lex();
    }
} // END my_lexer namespace