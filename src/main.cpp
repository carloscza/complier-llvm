#include <iostream>
#include "lexer.cpp"
#include "tools.cpp"

// Use 'constexpr' to guarantee that the compiler constructs the 'test_case[]' array..
// at compile time.
// Use '#embed' to have file 'lexer_test.c' read at compile time and embedded directly..
// into the binary.  
// Using 'u8' to store raw bytes of chars of the embedded file since using latin-1 intepretation.
static my_lexer::u8 test_case[] = {
    #embed "../tests/lexer_test.c"
    ,0
};

int main(void)
{
    my_lexer::Lexer lex{test_case};
    while(*lex)
    {
        if('int' == *lex)
        {
            std::cerr << lex.get_value() << "\n";
        }
        else if('id' == *lex)
        {
            std::cerr << my_lexer::ids[lex.get_value()] << "\n";
        }
        else
        {
            std::cerr << my_tools::token_to_string(*lex) << "\n";
        }
        ++lex;
    }
    return 0;
}