/**
    JS2CPP - Javascript to C++ converter.

    Copyright (C) 2017  Phil Bouchard <pbouchard8@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


%{
#include "parser_yacc.h"
#include <iostream>
    
using namespace std;
%}

%option noyywrap
%option prefix="JS2CPP"

%%


"not"                                   {
                                                return JS2CPPParser::FUNCTION1stNOT;
                                        }

"<<"                                    {
                                                return JS2CPPParser::FUNCTION2ndLEFTSHIFT;
                                        }

">>"                                    {
                                                return JS2CPPParser::FUNCTION2ndRIGHTSHIFT;
                                        }

"&&"                                    {
                                                return JS2CPPParser::FUNCTION2ndAND;
                                        }

"xor"                                   {
                                                return JS2CPPParser::FUNCTION2ndXOR;
                                        }

"xnor"                                  {
                                                return JS2CPPParser::FUNCTION2ndXOR;
                                        }

"||"                                    {
                                                return JS2CPPParser::FUNCTION2ndOR;
                                        }

"=="                                    {
                                                return JS2CPPParser::FUNCTION2ndEQUAL;
                                        }

"<"                                     {
                                                return JS2CPPParser::FUNCTION2ndLESS;
                                        }

">"                                     {
                                                return JS2CPPParser::FUNCTION2ndGREATER;
                                        }

"!="                                    {
                                                return JS2CPPParser::FUNCTION2ndNOTEQUAL;
                                        }

"<="                                    {
                                                return JS2CPPParser::FUNCTION2ndLESSEQUAL;
                                        }

">="                                    {
                                                return JS2CPPParser::FUNCTION2ndGREATEREQUAL;
                                        }

"++"                                    {
                                                return JS2CPPParser::FUNCTION2ndINCREMENT;
                                        }

"--"                                    {
                                                return JS2CPPParser::FUNCTION2ndDECREMENT;
                                        }

[0-9]+                                  {
                                                static_cast<JS2CPPParser *>(this)->parserlval.s << yytext;
                                                return JS2CPPParser::INTEGER;
                                        }

\.[0-9]+(e[+-]?[0-9]+)?                 {
                                                static_cast<JS2CPPParser *>(this)->parserlval.s << yytext;
                                                return JS2CPPParser::DOUBLE;
                                        }

[0-9]+(\.[0-9]*)?(e[+-]?[0-9]+)?        {
                                                static_cast<JS2CPPParser *>(this)->parserlval.s << yytext;
                                                return JS2CPPParser::DOUBLE;
                                        }

\;                                      {
                                                return JS2CPPParser::EOL;
                                        }

\/\/.*                                  {
                                        }

[ \t]+                                  {
                                        }

"function"                              {
                                                return JS2CPPParser::FUNCTION;
                                        }

"var"                                   {
                                                return JS2CPPParser::VAR;
                                        }

"if"                                    {
                                                return JS2CPPParser::IF;
                                        }

"else"                                  {
                                                return JS2CPPParser::ELSE;
                                        }

"for"                                   {
                                                return JS2CPPParser::FOR;
                                        }

"while"                                 {
                                                return JS2CPPParser::WHILE;
                                        }

"exit"                                  {
                                                return JS2CPPParser::EXIT;
                                        }

[a-zA-Z_][a-zA-Z_0-9]*                  {
                                                static_cast<JS2CPPParser *>(this)->parserlval.s << yytext;
                                                return JS2CPPParser::ID;
                                        }

.                                       {
                                                return * yytext;
                                        }
%%
