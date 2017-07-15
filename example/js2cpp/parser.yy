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

#include <sstream>


struct val
{
    std::stringstream s;
    
    val()
    {
    }
    
    val(val & v)
    {
        s << v.s.rdbuf();
    }
    
    val & operator = (val & v)
    {
        s << v.s.rdbuf();
        
        return * this;
    }
};

#define ON symbol = true;

#define OFF symbol = false;

#endif
%}

%name JS2CPPParser

%define INHERIT : public JS2CPPFlexLexer

%define STYPE val

%define LEX_BODY { return JS2CPPFlexLexer::yylex(); }

%define ERROR_BODY { * yyout << JS2CPPFlexLexer::lineno() << ": parse error before '" << JS2CPPFlexLexer::YYText() << "'" << std::endl; }

%define CONSTRUCTOR_INIT : symbol(false)

%define MEMBERS bool symbol;


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
                                $$ << $1.rdbuf();
                                YYACCEPT;
                        }
                        ;

statement_list:		statement_list statement
                        {
                                $$ << $1.rdbuf() << $2.rdbuf() << ";" << std::endl;
                        }
                        |
                        statement
                        {
                                $$ << $1.rdbuf() << ";" << std::endl;
                        }
                        ;

statement:		expression EOL
                        {
                                $$ << $1.rdbuf();
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
                        RETURN expression EOL
                        {
                                $$ << "return " << $2.rdbuf();
                        }
                        ;

expression:		{
                                if (symbol)
                                {
                                        parsererror(yytext);
                                        YYABORT;
                                }
                        }
                        |
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
                        FUNCTION1stNOT expression_binary
                        {
                                $$ << "! " << $2.rdbuf();
                        }
                        |
                        expression_binary '=' expression_binary
                        {
                                $$ << $1.rdbuf() << " = " << $3.rdbuf();
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
                        expression_mul '%' expression_mul
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
                        expression FUNCTION2ndINCREMENT
                        {
                                $$ << $1.rdbuf() << " ++";
                        }
                        |
                        expression FUNCTION2ndDECREMENT
                        {
                                $$ << $1.rdbuf() << " --";
                        }
                        |
                        FUNCTION2ndINCREMENT expression
                        {
                                $$ << "++ " << $2.rdbuf();
                        }
                        |
                        FUNCTION2ndDECREMENT expression
                        {
                                $$ << "-- " << $2.rdbuf();
                        }
                        |
                        '(' expression ')'
                        {
                                $$ << "(" << $2.rdbuf() << ")";
                        }
                        |
                        '[' expression_list ']'
                        {
                                $$ << "[" << $2.rdbuf() << "]";
                        }
                        |
                        terminal
                        {
                                $$ << $1.rdbuf();
                        }
                        |
                        expression '(' expression_list ')'
                        {
                                $$ << $1.rdbuf() << "(" << $3.rdbuf() << ")";
                        }
                        |
                        FUNCTION '(' expression_list ')' statement
                        {
                                $$ << "function (" << $3.rdbuf() << ")" << $5.rdbuf();
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
                                $$ << $1.rdbuf();
                        }
                        |
                        VAR ID
                        {
                                $$ << $2.rdbuf();
                        }
                        ;

number:			INTEGER
                        {
                                $$ << $1.rdbuf();
                        }
                        |
                        DOUBLE
                        {
                                $$ << $1.rdbuf();
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
