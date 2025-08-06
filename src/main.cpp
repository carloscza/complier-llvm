#include <iostream>
#include "lexer.cpp"
#include "tools.cpp"
#include "parser.hpp"

// Use 'constexpr' to guarantee that the compiler constructs the 'test_case[]' array..
// at compile time.
// Use '#embed' to have file 'lexer_test.c' read at compile time and embedded directly..
// into the binary.  
// Using 'u8' to store raw bytes of chars of the embedded file since using latin-1 intepretation.
static my_lexer::u8 test_case[] = {
    #embed "../tests/calc.c"
    ,0
};

int main(void)
{
    my_parser::Program prog = my_parser::Parser{test_case}();

    return 0;
}