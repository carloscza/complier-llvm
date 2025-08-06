// lexer.hpp

#ifndef LEXER_H
#define LEXER_H

#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <cstdint>

namespace my_lexer
{
    using u8 = unsigned char;            // create an alias/type-def for an '8-bit unsigned integer' (ie., a byte).
    static_assert(sizeof(u8) == 1);      // assert that u8 data types are exactly 1 byte.
    static_assert(CHAR_BIT   == 8);      // assert that 1 byte is 8 bits.
    using i32 = int32_t;                 // create an alias/type-def for an '32-bit signed integer'.

    // Lexer:
    struct Lexer
    {
        // Constructor:
        Lexer(u8 *lex_iter)
            :lex_iter{lex_iter}
        {
            init_keywords();
            head = lex();
        }

        // Overloading Operators:
        int  operator*()  { return head;  }
        void operator++() { head = lex(); }
        
        // Member Methods:
        i32 get_value() { return value; }

        private:
            u8 *lex_iter;
            int head;
            size_t line = 0;
            i32 value;
            std::vector<i32> keywords;

            int  lex();
            void init_keywords();
            int  lex_comment();
    };


    // Look-Up-Table:
    struct LUT
    {
        // Constructor:
        consteval LUT(std::string_view sv, bool null_terminator = false)
            : lut{}
        {
            for (u8 ch : sv)      { lut[ch] = 1; }
            if  (null_terminator) { lut[0]  = 1; } 
        }

        int operator()(u8 i) const { return lut[i]; }

        private:
            u8 lut[256];
    }; 
    

    struct IDManager
    {
        i32 operator[](u8 *start, u8 *end)
        {
            return (*this)[std::string_view((char *)start, end - start)];
        }

        i32 operator[](std::string_view name)
        {
            if(string_to_id.contains(name)) { return static_cast<i32>(string_to_id[name]); }

            id_to_string.emplace_back(
                (u8 *)name.data(),
                (u8 *)(name.data() + name.size())
            );
            auto& vec = id_to_string.back();
            string_to_id[std::string_view((char *)vec.data(), vec.size())] = id_to_string.size() - 1;
            return id_to_string.size() - 1;
        }

        std::string_view operator[](i32 id)
        {
            auto& vec = id_to_string.at(id);
            return std::string_view((char *)vec.data(), vec.size());
        }

        private:
            std::vector<std::vector<u8>> id_to_string;
            std::unordered_map<std::string_view, size_t> string_to_id;
    };
    
    static IDManager ids;
} // end my_lexer namespace

#endif