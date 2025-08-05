#include<string>

namespace my_tools
{
    static std::string token_to_string(int tok) 
    {
        std::string s;
        for(int i = sizeof(int) - 1; i > -1; --i)
        {
            if(((char *)&tok)[i]) { s += ((char *)&tok)[i]; }
        }
        return s;
    }
} // END my_tools namespace