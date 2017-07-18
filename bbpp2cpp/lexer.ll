/**
    BBPP2CPP - Javascript to C++ converter.

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
%option prefix="BBPP2CPP"

%%


"not"                                   {
                                                return BBPP2CPPParser::FUNCTION1stNOT;
                                        }

"<<"                                    {
                                                return BBPP2CPPParser::FUNCTION2ndLEFTSHIFT;
                                        }

">>"                                    {
                                                return BBPP2CPPParser::FUNCTION2ndRIGHTSHIFT;
                                        }

"&&"                                    {
                                                return BBPP2CPPParser::FUNCTION2ndAND;
                                        }

"xor"                                   {
                                                return BBPP2CPPParser::FUNCTION2ndXOR;
                                        }

"xnor"                                  {
                                                return BBPP2CPPParser::FUNCTION2ndXOR;
                                        }

"||"                                    {
                                                return BBPP2CPPParser::FUNCTION2ndOR;
                                        }

"=="                                    {
                                                return BBPP2CPPParser::FUNCTION2ndEQUAL;
                                        }

"<"                                     {
                                                return BBPP2CPPParser::FUNCTION2ndLESS;
                                        }

">"                                     {
                                                return BBPP2CPPParser::FUNCTION2ndGREATER;
                                        }

"!="                                    {
                                                return BBPP2CPPParser::FUNCTION2ndNOTEQUAL;
                                        }

"<="                                    {
                                                return BBPP2CPPParser::FUNCTION2ndLESSEQUAL;
                                        }

">="                                    {
                                                return BBPP2CPPParser::FUNCTION2ndGREATEREQUAL;
                                        }

"++"                                    {
                                                return BBPP2CPPParser::FUNCTION2ndINCREMENT;
                                        }

"--"                                    {
                                                return BBPP2CPPParser::FUNCTION2ndDECREMENT;
                                        }

[0-9]+                                  {
                                                static_cast<BBPP2CPPParser *>(this)->parserlval.s = yytext;
                                                return BBPP2CPPParser::INTEGER;
                                        }

\.[0-9]+(e[+-]?[0-9]+)?                 {
                                                static_cast<BBPP2CPPParser *>(this)->parserlval.s = yytext;
                                                return BBPP2CPPParser::DOUBLE;
                                        }

[0-9]+(\.[0-9]*)?(e[+-]?[0-9]+)?        {
                                                static_cast<BBPP2CPPParser *>(this)->parserlval.s = yytext;
                                                return BBPP2CPPParser::DOUBLE;
                                        }

\;                                      {
                                                return BBPP2CPPParser::EOL;
                                        }

\/\/.*                                  {
                                        }

[ \t]+                                  {
                                        }

"class"                                 {
                                                return BBPP2CPPParser::CLASS;
                                        }

"struct"                                {
                                                return BBPP2CPPParser::CLASS;
                                        }

"const"                                 {
                                                return BBPP2CPPParser::CONST;
                                        }

"new"                                   {
                                                return BBPP2CPPParser::NEW;
                                        }

"return"                                {
                                                return BBPP2CPPParser::RETURN;
                                        }

"function"                              {
                                                return BBPP2CPPParser::FUNCTION;
                                        }

"var"                                   {
                                                return BBPP2CPPParser::VAR;
                                        }

"if"                                    {
                                                return BBPP2CPPParser::IF;
                                        }

"else"                                  {
                                                return BBPP2CPPParser::ELSE;
                                        }

"for"                                   {
                                                return BBPP2CPPParser::FOR;
                                        }

"while"                                 {
                                                return BBPP2CPPParser::WHILE;
                                        }

[a-zA-Z_][a-zA-Z_0-9]*                  {
                                                static_cast<BBPP2CPPParser *>(this)->parserlval.s = yytext;
                                                return BBPP2CPPParser::ID;
                                        }

L?\"(\\.|[^\\"])*\"                     {
                                                static_cast<BBPP2CPPParser *>(this)->parserlval.s = yytext;
                                                return BBPP2CPPParser::STRING;
                                        }
                                        
\n                                      {
                                                ++ yylineno;
                                        }
                                        
.                                       {
                                                return * yytext;
                                        }
%%
