#include <iostream>
#include "parser_yacc.h"

using namespace std;


int main()
{
    BBPP2CPPParser parser;

    parser.switch_streams(& cin, & cerr);

    if (! parser.parserparse())
        cout << parser.value.s << endl;
}
