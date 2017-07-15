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


%header{
#ifndef PARSER_H
#define PARSER_H

#if !defined(yyFlexLexerOnce)
#undef yylex
#define yyFlexLexer JS2CPPFlexLexer
#include <FlexLexer.h>
#endif

#include <string>


struct val
{
    std::string s;
};


#endif
%}

%name JS2CPPParser

%define INHERIT : public JS2CPPFlexLexer

%define STYPE val

%define LEX_BODY { return JS2CPPFlexLexer::yylex(); }

%define ERROR_BODY { * yyout << JS2CPPFlexLexer::lineno() << ": parse error before '" << JS2CPPFlexLexer::YYText() << "'" << std::endl; }

%define MEMBERS val value;


%token          EOL
%token          VAR
%token          IF
%token          ELSE
%token          FOR
%token          WHILE
%token          RETURN

%token  <s>     ID
%token  <s>     DOUBLE
%token  <s>     INTEGER

%token          FUNCTION
%token          FUNCTION2ndINCREMENT
%token          FUNCTION2ndDECREMENT

%right          '='
%left           FUNCTION2ndOR
%left           FUNCTION2ndXOR
%left           FUNCTION2ndAND
%left           FUNCTION1stNOT
%left           FUNCTION2ndEQUAL FUNCTION2ndLESS FUNCTION2ndGREATER FUNCTION2ndNOTEQUAL FUNCTION2ndLESSEQUAL FUNCTION2ndGREATEREQUAL
%left           FUNCTION2ndLEFTSHIFT FUNCTION2ndRIGHTSHIFT
%left           '+' '-'
%left           '*' '/' '%'
%right          '^'
%left           '!'

%type   <s>     start
%type   <s>     statement
%type   <s>     statement_list
%type   <s>     number
%type   <s>     terminal
%type   <s>     expression
%type   <s>     expression_list
%type   <s>     expression_binary
%type   <s>     expression_add
%type   <s>     expression_mul
%type   <s>     expression_signed
%type   <s>     expression_unsigned
%type   <s>     expression_unary
%type   <s>     expression_factorial

%%

start:			statement_list
                        {
                                value.s = $1;
                                YYACCEPT;
                        }
                        ;

statement_list:		statement_list statement
                        {
                                $$ = $1 + $2;
                        }
                        |
                        statement
                        {
                                $$ = $1;
                        }
                        ;

statement:		expression EOL
                        {
                                $$ = $1 + ";";
                        }
                        |
                        '{' statement_list '}'
                        {
                                $$ = "{" + $2 + "}";
                        }
                        |
                        IF '(' expression ')' statement
                        {
                                $$ = "if (" + $3 + ")" + $5;
                        }
                        |
                        IF '(' expression ')' statement ELSE statement
                        {
                                $$ = "if (" + $3 + ")" + $5 + "else" + $7;
                        }
                        |
                        WHILE '(' expression ')' statement
                        {
                                $$ = "while (" + $3 + ")" + $5;
                        }
                        |
                        FOR '(' expression EOL expression EOL expression ')' statement
                        {
                                $$ = "for (" + $3 + "; " + $5 + "; " + $7 + ")" + $9;
                        }
                        |
                        RETURN expression EOL
                        {
                                $$ = "return " + $2 + ";";
                        }
                        ;

expression:		{
                                $$ = "";
                        }
                        |
                        expression_binary
                        {
                                $$ = $1;
                        }
                        ;

expression_binary:	expression_add
                        {
                                $$ = $1;
                        }
                        |
                        FUNCTION1stNOT expression_binary
                        {
                                $$ = "NOT " + $2;
                        }
                        |
                        expression_binary '=' expression_binary
                        {
                                $$ = $1 + " = " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndOR expression_binary
                        {
                                $$ = $1 + " || " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndXOR expression_binary
                        {
                                $$ = $1 + " XOR " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndAND expression_binary
                        {
                                $$ = $1 + " && " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndEQUAL expression_binary
                        {
                                $$ = $1 + " == " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndLESS expression_binary
                        {
                                $$ = $1 + " < " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndGREATER expression_binary
                        {
                                $$ = $1 + " > " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndNOTEQUAL expression_binary
                        {
                                $$ = $1 + " != " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndLESSEQUAL expression_binary
                        {
                                $$ = $1 + " <= " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndGREATEREQUAL expression_binary
                        {
                                $$ = $1 + " >= " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndLEFTSHIFT expression_binary
                        {
                                $$ = $1 + " + " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndRIGHTSHIFT expression_binary
                        {
                                $$ = $1 + " >> " + $3;
                        }
                        ;

expression_add:		expression_mul
                        {
                                $$ = $1;
                        }
                        |
                        expression_add '+' expression_add
                        {
                                $$ = $1 + " + " + $3;
                        }
                        |
                        expression_add '-' expression_add
                        {
                                $$ = $1 + " - " + $3;
                        }
                        ;

expression_mul:		expression_signed
                        {
                                $$ = $1;
                        }
                        |
                        expression_mul '*' expression_mul
                        {
                                $$ = $1 + " * " + $3;
                        }
                        |
                        expression_mul '/' expression_mul
                        {
                                $$ = $1 + " / " + $3;
                        }
                        |
                        expression_mul '%' expression_mul
                        {
                                $$ = $1 + " % " + $3;
                        }
                        ;

expression_signed:	expression_unary
                        {
                                $$ = $1;
                        }
                        |
                        expression_unary '^' expression_signed
                        {
                                $$ = $1 + "^" + $3;
                        }
                        ;

expression_unsigned:	expression_factorial
                        {
                                $$ = $1;
                        }
                        |
                        expression_factorial '^' expression_signed
                        {
                                $$ = $1 + "^" + $3;
                        }
                        ;

expression_unary:	expression_factorial
                        {
                                $$ = $1;
                        }
                        |
                        '|' expression '|'
                        {
                                $$ = "|" + $2 + "|";
                        }
                        |
                        '+' expression_unary
                        {
                                $$ = "+ " + $2;
                        }
                        |
                        '-' expression_unary
                        {
                                $$ = "- " + $2;
                        }
                        ;

expression_factorial:	expression_factorial '!'
                        {
                                $$ = $1 + " !";
                        }
                        |
                        expression FUNCTION2ndINCREMENT
                        {
                                $$ = $1 + " ++";
                        }
                        |
                        expression FUNCTION2ndDECREMENT
                        {
                                $$ = $1 + " --";
                        }
                        |
                        FUNCTION2ndINCREMENT expression
                        {
                                $$ = "++ " + $2;
                        }
                        |
                        FUNCTION2ndDECREMENT expression
                        {
                                $$ = "-- " + $2;
                        }
                        |
                        '(' expression ')'
                        {
                                $$ = "(" + $2 + ")";
                        }
                        |
                        '[' expression_list ']'
                        {
                                $$ = "[" + $2 + "]";
                        }
                        |
                        terminal
                        {
                                $$ = $1;
                        }
                        |
                        expression '(' expression_list ')'
                        {
                                $$ = $1 + "(" + $3 + ")";
                        }
                        |
                        FUNCTION '(' expression_list ')' statement
                        {
                                $$ = "function (" + $3 + ")" + $5;
                        }
                        ;

terminal:		number
                        {
                                $$ = $1;
                        }
                        |
                        ID
                        {
                                $$ = $1;
                        }
                        |
                        VAR ID
                        {
                                $$ = $2;
                        }
                        ;

number:			INTEGER
                        {
                                $$ = $1;
                        }
                        |
                        DOUBLE
                        {
                                $$ = $1;
                        }
                        ;

expression_list:	expression_list ',' expression
                        {
                                $$ = $1 + ", " + $3;
                        }
                        |
                        expression
                        {
                                $$ = $1;
                        }
                        ;


%%
