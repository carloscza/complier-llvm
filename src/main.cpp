#include <iostream>
#include <climits>
#include <cassert>
#include <vector>
#include <unordered_map>

#define ABORT(...) { \
    std::cerr << "ABORT: " << __VA_ARGS__ << ", " << __LINE__ << " " << __FILE__ << "\n"; \
    __builtin_trap(); \
 }

// Create alias/type-def for '8-bit unsigned integer' (a byte):
using u8 = unsigned char;
static_assert(sizeof(u8) == 1); // assert u8 data types are exactly 1 byte.
static_assert(CHAR_BIT   == 8); // assert 1 byte is 8 bits.
using i32 = int32_t;

// Use 'constexpr' to guarantee that the compiler constructs the 'test_case[]' array..
// at compile time.
// Use '#embed' to have file 'lexer_test.c' read at compile time and embedded directly..
// into the binary.  
// Using 'u8' to store raw bytes of chars of the embedded file since using latin-1 intepretation.
static u8 test_case[] = {
    #embed "../tests/lexer_test.c"
    ,0
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

struct LUT
{
    consteval LUT(std::string_view sv, bool null_terminator = false)
        : lut{}
    {
        for (u8 ch : sv)
        {
            lut[ch] = 1;
        }
        if (null_terminator) { lut[0] = 1; } 
    }

    int operator()(u8 i) const { return lut[i]; }

    private:
        u8 lut[256];

}; 

struct Lexer 
{
    Lexer(u8 *iterator) 
        : iterator{iterator} 
    { 
        init_keywords();
        head = lex(); 
        std::cout << "This is iterator at init : " << *iterator << "@@@@" << std::endl;
    }


    int  operator *() { return head;  }
    void operator++() { head = lex(); }
    i32  get_value()  { return value; }

    private:
        u8 *iterator;
        int head;
        size_t line = 0;
        i32 value;
        std::vector<i32> keywords;

        void init_keywords()
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

        int lex() 
        { 
            static constexpr LUT is_ws(" \n\t\r\v\f");
            while(is_ws(*iterator)) { line += *iterator == '\n'; ++iterator; }

            static constexpr LUT is_mono(";~^*%():{}[]+-,", true);
            if(is_mono(*iterator)) { return *iterator++; }

            static constexpr LUT is_digit("0123456789");
            if(is_digit(*iterator))
            {
                value = 0;
                while(is_digit(*iterator))
                {
                    if(__builtin_mul_overflow(value, 10, &value)) { ABORT("D:"); }
                    if(__builtin_add_overflow(value, *iterator++ - '0', &value)) { ABORT("D:<"); }
                }
                return 'int';
            }

            static constexpr LUT is_id("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");
            static constexpr LUT still_id("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789");
            if(is_id(*iterator))
            {
                u8 *start = iterator;
                while(still_id(*iterator)) { ++iterator; }
                u8 *end = iterator;
                value = ids[start, end];

                if ( value < keywords.size() && keywords[value] > -1) { return keywords[value]; }
                return 'id';
            }

            switch (*iterator)
            {
                case '/':
                    {
                        if(iterator[1] == '/') { return lex_comment(); }
                        return *iterator++;
                    }
                    break;
                case '|':
                    {
                        if(iterator[1] == '|') { iterator += 2; return '||';}
                        return *iterator++;
                    }
                    break;
                case '&':
                    {
                        if(iterator[1] == '&') { iterator += 2; return '&&';}
                        return *iterator++;
                    }
                    break;
                case '=':
                    {
                        if(iterator[1] == '=') { iterator += 2; return '==';}
                        return *iterator++;
                    }
                    break;
                case '!':
                    {
                        if(iterator[1] == '=') { iterator += 2; return '!=';}
                        return *iterator++;
                    }
                    break;
                case '<':
                    {
                        if(iterator[1] == '<') { iterator += 2; return '<<';}
                        if(iterator[1] == '=') { iterator += 2; return '<=';}
                        return *iterator++;
                    }
                    break;
                case '>':
                    {
                        if(iterator[1] == '>') { iterator += 2; return '>>';}
                        if(iterator[1] == '=') { iterator += 2; return '>=';}
                        return *iterator++;
                    }
                    break;
                default:
                    {
                        ABORT(" D: " << (char)*iterator << ", " << (int)*iterator << "?\n");
                    }
                    break;
            }
            return *iterator++; 
        }


        int lex_comment()
        {
            iterator += 2;
            static constexpr LUT is_new_line("\n", true);
            while(!is_new_line(*iterator)) { ++iterator; }
            return lex();
        }
};

static std::string tok2str(int tok) 
{
    std::string s;
    for(int i = sizeof(int) - 1; i > -1; --i)
    {
        if(((char *)&tok)[i]) { s += ((char *)&tok)[i]; }
    }
    return s;
}

int main(void)
{
    Lexer lex{test_case};
    while(*lex)
    {
        if('int' == *lex)
        {
            std::cerr << lex.get_value() << "\n";
        }
        else if('id' == *lex)
        {
            std::cerr << ids[lex.get_value()] << "\n";
        }
        else
        {
            std::cerr << tok2str(*lex) << "\n";
        }
        ++lex;
    }
    return 0;
}