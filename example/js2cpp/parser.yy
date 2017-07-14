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

#define yyFlexLexer lexerFlexLexer
#include <FlexLexer.h>

#include <string>
#include <sstream>
#include "javascript.h"


struct val
{
    string s;
    stringstream t;
};

#define ON symbol = true;

#define OFF symbol = false;

#endif
%}

%name JS2CPPParser

%define INHERIT : public lexerFlexLexer

%define STYPE val

%define LEX_BODY { return lexerFlexLexer::yylex(); }

%define ERROR_BODY { * yyout << string("Invalid Statement"); }

%define CONSTRUCTOR_INIT : symbol(false)

%define MEMBERS bool symbol; type_t<type_p> config; void state(int s) { yy_start = 1 + 2 * s; }


%token		EOL
%token		FUNCTION1stRESULT

%token          VAR
%token		IF
%token		ELSE
%token		FOR
%token		WHILE
%token		EXIT

%token	<c>	ID
%token	<i>	INDEX
%token	<d>	DOUBLE
%token	<d>	INTEGER


%token	<ed>	FUNCTION0ed
%token	<fd>	FUNCTION1st
%token	<gd>	FUNCTION2nd
%token  <gd>    FUNCTION2ndINCREMENT
%token  <gd>    FUNCTION2ndDECREMENT
%token	<hd>	FUNCTION3rd


%right	'='
%left	FUNCTION2ndOR
%left	FUNCTION2ndXOR
%left	FUNCTION2ndAND
%left	FUNCTION1stNOT
%left	FUNCTION2ndEQUAL FUNCTION2ndLESS FUNCTION2ndGREATER FUNCTION2ndNOTEQUAL FUNCTION2ndLESSEQUAL FUNCTION2ndGREATEREQUAL
%left	FUNCTION2ndLEFTSHIFT FUNCTION2ndRIGHTSHIFT
%left	'+' '-'
%left	'*' '/' ':' FUNCTION2ndMODULO
%right	'^'
%left	'!'


%type	<t>	start
%type	<t>	statement
%type	<t>	statement_list
%type	<t>	number
%type	<t>	terminal
%type	<t>	expression
%type	<t>	expression_list
%type	<t>	expression_binary
%type	<t>	expression_add
%type	<t>	expression_mul
%type	<t>	expression_signed
%type	<t>	expression_unsigned
%type	<t>	expression_unary
%type	<t>	expression_factorial


%%

start:			statement_list
                        {
                                config.value = $1;
                                YYACCEPT;
                        }
                        ;

statement_list:		statement_list statement
                        {
                                $$ << $1.rdbuf() << $2.rdbuf();
                        }
                        |
                        statement
                        {
                                $$ << $1.rdbuf();
                        }
                        ;

statement:		expression_binary EOL
                        {
                                $$ << $1.rdbuf() << "; " << endl;
                        }
                        |
                        '{' statement_list '}'
                        {
                                $$ << "{" << $2.rdbuf() << "}";
                        }
                        |
                        IF '(' expression ')' statement
                        {
                                $$ << "if (" << $3.rdbuf() << ")" << $5.rdbuf();
                        }
                        |
                        IF '(' expression ')' statement ELSE statement
                        {
                                $$ << "if (" << $3.rdbuf() << ")" << $5.rdbuf() << "else" << $7.rdbuf();
                        }
                        |
                        WHILE '(' expression ')' statement
                        {
                                $$ << "while (" << $3.rdbuf() << ")" << $5.rdbuf();
                        }
                        |
                        FOR '(' expression EOL expression EOL expression ')' statement
                        {
                                $$ << "for (" << $3.rdbuf() << "; " << $5.rdbuf() << "; " << $7.rdbuf() << ")" << $9.rdbuf();
                        }
                        |
                        EXIT
                        {
                                $$ << "exit(-1)";
                        }
                        ;

expression:		{
                                if (symbol)
                                {
                                        parsererror(yytext);
                                        YYABORT;
                                }
                        }
                        expression_binary
                        {
                                $$ << $1.rdbuf();
                        }
                        ;

expression_binary:	expression_add
                        {
                                $$ << $1.rdbuf();
                        }
                        |
                        ID '=' expression_binary
                        {
                                $$ << $1 << " = " << $3.rdbuf();
                        }
                        |
                        FUNCTION1stNOT expression_binary
                        {
                                $$ << "! " << $2.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndOR expression_binary
                        {
                                $$ << $1.rdbuf() << " || " << $3.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndXOR expression_binary
                        {
                                $$ << $1.rdbuf() << " XOR " << $3.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndAND expression_binary
                        {
                                $$ << $1.rdbuf() << " && " << $3.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndEQUAL expression_binary
                        {
                                $$ << $1.rdbuf() << " == " << $3.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndLESS expression_binary
                        {
                                $$ << $1.rdbuf() << " < " << $3.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndGREATER expression_binary
                        {
                                $$ << $1.rdbuf() << " > " << $3.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndNOTEQUAL expression_binary
                        {
                                $$ << $1.rdbuf() << " != " << $3.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndLESSEQUAL expression_binary
                        {
                                $$ << $1.rdbuf() << " <= " << $3.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndGREATEREQUAL expression_binary
                        {
                                $$ << $1.rdbuf() << " >= " << $3.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndLEFTSHIFT expression_binary
                        {
                                $$ << $1.rdbuf() << " << " << $3.rdbuf();
                        }
                        |
                        expression_binary FUNCTION2ndRIGHTSHIFT expression_binary
                        {
                                $$ << $1.rdbuf() << " >> " << $3.rdbuf();
                        }
                        ;

expression_add:		expression_mul
                        {
                                $$ << $1.rdbuf();
                        }
                        |
                        expression_add '+' expression_add
                        {
                                $$ << $1.rdbuf() << " + " << $3.rdbuf();
                        }
                        |
                        expression_add '-' expression_add
                        {
                                $$ << $1.rdbuf() << " - " << $3.rdbuf();
                        }
                        ;

expression_mul:		expression_signed
                        {
                                $$ << $1.rdbuf();
                        }
                        |
                        expression_mul '*' expression_mul
                        {
                                $$ << $1.rdbuf() << " * " << $3.rdbuf();
                        }
                        |
                        expression_mul '/' expression_mul
                        {
                                $$ << $1.rdbuf() << " / " << $3.rdbuf();
                        }
                        |
                        expression_mul FUNCTION2ndMODULO expression_mul
                        {
                                $$ << $1.rdbuf() << " % " << $3.rdbuf();
                        }
                        ;

expression_signed:	expression_unary
                        {
                                $$ << $1.rdbuf();
                        }
                        |
                        expression_unary '^' {OFF} expression_signed
                        {
                                $$ << $1.rdbuf() << "^" << $4.rdbuf();
                        }
                        ;

expression_unsigned:	expression_factorial
                        {
                                $$ << $1.rdbuf();
                        }
                        |
                        expression_factorial '^' {OFF} expression_signed
                        {
                                $$ << $1.rdbuf() << "^" << $4.rdbuf();
                        }
                        ;

expression_unary:	expression_factorial
                        {
                                $$ << $1.rdbuf();
                        }
                        |
                        '|' expression '|'
                        {
                                $$ << "|" << $2.rdbuf() << "|";
                        }
                        |
                        '+' expression_unary
                        {
                                $$ << "+ " << $2.rdbuf();
                        }
                        |
                        '-' expression_unary
                        {
                                $$ << "- " << $2.rdbuf();
                        }
                        ;

expression_factorial:	expression_factorial '!'
                        {
                                $$ << $1.rdbuf() << " !";
                        }
                        |
                        ID FUNCTION2ndINCREMENT
                        {
                                $$ << $1 << " ++";
                        }
                        ID FUNCTION2ndDECREMENT
                        {
                                $$ << $1 << " --";
                        }
                        |
                        |
                        '(' expression ')'
                        {
                                $$ << "(" << $1.rdbuf() << ")";
                        }
                        |
                        '[' expression_list ']'
                        {
                                $$ << "[" << $1.rdbuf() << "]";
                        }
                        |
                        terminal
                        {
                                $$ << $1.rdbuf();
                        }
                        |
                        FUNCTION0ed '(' ')'
                        {
                                if (symbol)
                                {
                                        parsererror(yytext);
                                        YYABORT;
                                }

                                $$ << $1 << "(" << ")";
                        }
                        |
                        FUNCTION1st '(' expression ')'
                        {
                                $$ << $1 << "(" << $3.rdbuf() << ")";
                        }
                        |
                        FUNCTION2nd '(' expression ',' expression ')'
                        {
                                $$ << $1 << "(" << $3.rdbuf() << ", " << $5.rdbuf() << ")";
                        }
                        |
                        FUNCTION3rd '(' expression ',' expression ',' expression ')'
                        {
                                $$ << $1 << "(" << $3.rdbuf() << ", " << $5.rdbuf() << ", " << $7.rdbuf() << ")";
                        }
                        ;

terminal:		number
                        {
                                if (symbol)
                                {
                                        parsererror(yytext);
                                        YYABORT;
                                }

                                $$ << $1.rdbuf();
                        }
                        |
                        ID
                        {
                                $$ << $1;
                        }
                        ;

number:			INTEGER
                        {
                                $$ << $1;
                        }
                        |
                        DOUBLE
                        {
                                $$ << $1;
                        }
                        ;

expression_list:	expression_list ',' expression
                        {
                                $$ << $1.rdbuf() << ", " << $3.rdbuf();
                        }
                        |
                        expression
                        {
                                $$ << $1.rdbuf();
                        }
                        ;


%%
