/**
    XPP2CPP - X++ to C++ converter.

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

%define CONSTRUCTOR_INIT : indent(0)

%define MEMBERS val value; int indent;


%token          EOL
%token          VAR
%token          IF
%token          ELSE
%token          FOR
%token          WHILE
%token          RETURN
%token          NEW
%token          CONST
%token          CLASS

%token  <s>     ID
%token  <s>     DOUBLE
%token  <s>     INTEGER
%token  <s>     STRING

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
%type   <s>     type
%type   <s>     type_list
%type   <s>     type_modifier
%type   <s>     member
%type   <s>     member_list
%type   <s>     scope_list
%type   <s>     terminal
%type   <s>     expression
%type   <s>     expression_list
%type   <s>     expression_binary
%type   <s>     expression_add
%type   <s>     expression_mul
%type   <s>     expression_signed
%type   <s>     expression_unary
%type   <s>     expression_factorial

%%

start:                  statement_list
                        {
                                value.s = "/*\n";
                                value.s += "    XPP2CPP - Generated Code.\n";
                                value.s += '\n';
                                value.s += "    Copyright (C) 2017  Phil Bouchard <pbouchard8@gmail.com>\n";
                                value.s += '\n';
                                value.s += "    This program is free software: you can redistribute it and/or modify\n";
                                value.s += "    it under the terms of the GNU General Public License as published by\n";
                                value.s += "    the Free Software Foundation, either version 3 of the License, or\n";
                                value.s += "    (at your option) any later version.\n";
                                value.s += '\n';
                                value.s += "    This program is distributed in the hope that it will be useful,\n";
                                value.s += "    but WITHOUT ANY WARRANTY; without even the implied warranty of\n";
                                value.s += "    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n";
                                value.s += "    GNU General Public License for more details.\n";
                                value.s += '\n';
                                value.s += "    You should have received a copy of the GNU General Public License\n";
                                value.s += "    along with this program.  If not, see <http://www.gnu.org/licenses/>.\n";
                                value.s += "*/\n";
                                value.s += '\n';
                                value.s += '\n';
                                value.s += "#include \"xpp2cpp.h\"\n";
                                value.s += "#include <iostream>\n";
                                value.s += '\n';
                                value.s += "using namespace std;\n";
                                value.s += "using namespace boost;\n";
                                value.s += "using namespace xpp2cpp;\n";
                                value.s += '\n';
                                value.s += '\n';
                                
                                value.s += "int main()\n";
                                value.s += $1;
                                value.s += '\n';
                                
                                YYACCEPT;
                        }
                        ;

statement_list:         statement_list statement
                        {
                                $$ = $1 + $2;
                        }
                        |
                        statement
                        {
                                $$ = $1;
                        }
                        |
                        {
                                $$ = "";
                        }
                        ;

statement:              expression EOL
                        {
                                $$ = $1 + "; ";
                        }
                        |
                        '{' statement_list '}'
                        {
                                $$ = "{";

                                $$ += "node_proxy __x; ";
                                $$ += "node_ptr<type> __temporary(__x); ";
                        
                                $$ += $2;
                                $$ += "}";
                        }
                        |
                        IF '(' expression ')' statement
                        {
                                $$ = "if (" + $3 + ")" + $5;
                        }
                        |
                        IF '(' expression ')' statement ELSE statement
                        {
                                $$ = "if (" + $3 + ") " + $5 + " else " + $7;
                        }
                        |
                        WHILE '(' expression ')' statement
                        {
                                $$ = "while (" + $3 + ") " + $5;
                        }
                        |
                        FOR '(' expression EOL expression EOL expression ')' statement
                        {
                                $$ = "for (" + $3 + "; " + $5 + "; " + $7 + ") " + $9;
                        }
                        |
                        RETURN expression EOL
                        {
                                $$ = "return __result = " + $2 + "; ";
                        }
                        |
                        CLASS ID '{' member_list '}' EOL
                        {
                                $$ = "struct " + $2 + " {" + $4 + "}; ";
                        }
                        |
                        CLASS ID ':' type_list '{' member_list '}' EOL
                        {
                                $$ = "struct " + $2 + " : " + $4 + " {" + $6 + "}; ";
                        }
                        ;

member_list:            member_list member
                        {
                                $$ = $1 + $2;
                        }
                        |
                        member
                        {
                                $$ = $1;
                        }
                        |
                        {
                                $$ = "";
                        }
                        ;

member:                 expression EOL
                        {
                                $$ = $1 + "; ";
                        }
                        ;
                        
expression:             {
                                $$ = "";
                        }
                        |
                        expression_binary
                        {
                                $$ = $1;
                        }
                        ;

expression_binary:      expression_add
                        {
                                $$ = $1;
                        }
                        |
                        FUNCTION1stNOT expression_binary
                        {
                                $$ = "operator_not(__temporary, " + $2 + ")";
                        }
                        |
                        expression_binary '=' expression_binary
                        {
                                $$ = $1 + " = " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndOR expression_binary
                        {
                                $$ = "operator_or(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndXOR expression_binary
                        {
                                $$ = "operator_xor(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndAND expression_binary
                        {
                                $$ = "operator_and(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndEQUAL expression_binary
                        {
                                $$ = "operator_equal(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndLESS expression_binary
                        {
                                $$ = "operator_less(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndGREATER expression_binary
                        {
                                $$ = "operator_greater(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndNOTEQUAL expression_binary
                        {
                                $$ = "operator_notequal(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndLESSEQUAL expression_binary
                        {
                                $$ = "operator_lessequal(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndGREATEREQUAL expression_binary
                        {
                                $$ = "operator_greaterequal(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndLEFTSHIFT expression_binary
                        {
                                $$ = $1 + " << " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndRIGHTSHIFT expression_binary
                        {
                                $$ = $1 + " >> " + $3;
                        }
                        ;

expression_add:         expression_mul
                        {
                                $$ = $1;
                        }
                        |
                        expression_add '+' expression_add
                        {
                                $$ = "operator_add(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_add '-' expression_add
                        {
                                $$ = "operator_sub(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        ;

expression_mul:         expression_signed
                        {
                                $$ = $1;
                        }
                        |
                        expression_mul '*' expression_mul
                        {
                                $$ = "operator_mul(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_mul '/' expression_mul
                        {
                                $$ = "operator_div(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_mul '%' expression_mul
                        {
                                $$ = "operator_mod(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        ;

expression_signed:      expression_unary
                        {
                                $$ = $1;
                        }
                        |
                        expression_unary '^' expression_signed
                        {
                                $$ = "operator_pow(__temporary, " + $1 + ", " + $3 + ")";
                        }
                        ;

expression_unary:       expression_factorial
                        {
                                $$ = $1;
                        }
                        |
                        '|' expression '|'
                        {
                                $$ = "operator_abs(__temporary, " + $2 + ")";
                        }
                        |
                        '+' expression_unary
                        {
                                $$ = $2;
                        }
                        |
                        '-' expression_unary
                        {
                                $$ = "operator_neg(__temporary, " + $2 + ")";
                        }
                        ;

expression_factorial:   expression_factorial '!'
                        {
                                $$ = "operator_factorial(__temporary, " + $1 + ")";
                        }
                        |
                        expression FUNCTION2ndINCREMENT
                        {
                                $$ = "operator_postinc(__temporary, " + $1 + ")";
                        }
                        |
                        expression FUNCTION2ndDECREMENT
                        {
                                $$ = "operator_postdec(__temporary, " + $1 + ")";
                        }
                        |
                        FUNCTION2ndINCREMENT expression
                        {
                                $$ = "operator_preinc(__temporary, " + $2 + ")";
                        }
                        |
                        FUNCTION2ndDECREMENT expression
                        {
                                $$ = "operator_predec(__temporary, " + $2 + ")";
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
                        expression '(' ')'
                        {
                                $$ = "(* " + $1 + ")(__temporary)";
                        }
                        |
                        expression '(' expression_list ')'
                        {
                                $$ = "(* " + $1 + ")(__temporary, " + $3 + ")";
                        }
                        |
                        FUNCTION '(' ')' statement
                        {
                                $$ = "make_fastnode<function1_t<node_ptr<type> & (node_ptr<type> &)>>(__x, function1_t<node_ptr<type> & (node_ptr<type> &)>([] (node_ptr<type> & __result) -> node_ptr<type> & " + $4 + "))";
                        }
                        |
                        FUNCTION '(' ID ')' statement
                        {
                                $$ = "make_fastnode<function2_t<node_ptr<type> & (node_ptr<type> &, node_ptr<type> &)>>(__x, function2_t<node_ptr<type> & (node_ptr<type> &, node_ptr<type> &)>([] (node_ptr<type> & __result, node_ptr<type> & " + $3 + ") -> node_ptr<type> & " + $5 + "))";
                        }
                        ;

terminal:               number
                        {
                                $$ = $1;
                        }
                        |
                        ID
                        {
                                $$ = $1;
                        }
                        |
                        scope_list
                        {
                                $$ = $1;
                        }
                        |
                        type_modifier ID
                        {
                                $$ = $1 + " " + $2;
                        }
                        ;

number:                 INTEGER
                        {
                                $$ = $1;
                        }
                        |
                        DOUBLE
                        {
                                $$ = $1;
                        }
                        |
                        NEW ID '(' expression ')'
                        {
                                $$ = "make_fastnode<type_t<" + $2 + ">>(__x, type_t<" + $2 + ">(" + $4 + "))";
                        }
                        ;
                        
type_modifier:          type
                        {
                                $$ = $1;
                        }
                        |
                        type '&'
                        {
                                $$ = $1 + " &";
                        }
                        |
                        type CONST
                        {
                                $$ = $1 + " const";
                        }
                        |
                        type CONST '&'
                        {
                                $$ = $1 + " const &";
                        }
                        ;

type:                   ID
                        {
                                $$ = $1;
                        }
                        |
                        VAR
                        {
                                $$ = "node_ptr<type>";
                        }
                        ;

type_list:              type_list ',' type
                        {
                                $$ = $1 + ", " + $3;
                        }
                        |
                        type
                        {
                                $$ = $1;
                        }
                        ;

scope_list:             scope_list '.' ID
                        {
                                $$ = $1 + "." + $3;
                        }
                        |
                        ID
                        {
                                $$ = $1;
                        }
                        ;


expression_list:        expression_list ',' expression
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
