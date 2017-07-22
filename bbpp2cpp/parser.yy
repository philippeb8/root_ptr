/**
    BBPP2CPP - BB++ to C++ converter.

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
#define yyFlexLexer BBPP2CPPFlexLexer
#include <FlexLexer.h>
#endif

#include <set>
#include <string>
#include <functional>

#include <boost/lexical_cast.hpp>


struct val
{
    std::string s;
};


#endif
%}

%name BBPP2CPPParser

%define INHERIT : public BBPP2CPPFlexLexer

%define STYPE val

%define LEX_BODY { return BBPP2CPPFlexLexer::yylex(); }

%define ERROR_BODY { * yyout << BBPP2CPPFlexLexer::lineno() << ": parse error before '" << BBPP2CPPFlexLexer::YYText() << "'" << std::endl; }

%define MEMBERS val value; int indent = 0, counter = 0; std::string header, footer; std::set<std::string> type;


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
%token          VIRTUAL
%token          STATIC
%token          OPERATOR
%token          ARROW
%token          AUTO

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
%type   <s>     operator
%type   <s>     type
%type   <s>     type_list
%type   <s>     type_modifier
%type   <s>     type_modifier_list
%type   <s>     parameter_list
%type   <s>     member
%type   <s>     member_list
%type   <s>     qualifier
%type   <s>     qualifier_list
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
                                value.s += "#include \"bbpp.h\"\n";
                                value.s += "#include <iostream>\n";
                                value.s += '\n';
                                value.s += "using namespace std;\n";
                                value.s += "using namespace bbpp;\n";
                                value.s += "using namespace boost;\n";
                                value.s += '\n';
                                value.s += '\n';
                                value.s += header;
                                value.s += '\n';
                                value.s += '\n';
                                value.s += $1;
                                value.s += '\n';
                                value.s += '\n';
                                value.s += footer;
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
                                $$ = "return proxy(__y, " + $2 + "); ";
                        }
                        |
                        CLASS ID EOL
                        {
                                type.insert($2);
                                
                                $$ = "struct " + $2 + "; ";
                        }
                        |
                        CLASS ID '{' '}' EOL
                        {
                                type.insert($2);
                                
                                header += "struct " + $2 + " {node_proxy const & __x;}; ";

                                $$ = "";
                        }
                        |
                        CLASS ID '{' {type.insert($2);} member_list '}' EOL
                        {
                                header += "struct " + $2 + " {node_proxy const & __x; " + $5 + "}; ";

                                $$ = "";
                        }
                        |
                        CLASS ID ':' type_list '{' '}' EOL
                        {
                                type.insert($2);
                                
                                header += "struct " + $2 + " : " + $4 + " {}; ";

                                $$ = "";
                        }
                        |
                        CLASS ID ':' type_list '{' {type.insert($2);} member_list '}' EOL
                        {
                                header += "struct " + $2 + " : " + $4 + " {" + $7 + "}; ";

                                $$ = "";
                        }
                        |
                        type_modifier ID '(' ')' statement
                        {
                                header += $1 + ' ' + $2 + '(' + ')' + ';';

                                $$ = $1 + ' ' + $2 + '(' + ')' + $5;
                        }
                        |
                        type_modifier ID '(' type_modifier_list ')' statement
                        {
                                header += $1 + ' ' + $2 + '(' + $4 + ')' + ';';

                                $$ = $1 + ' ' + $2 + '(' + $4 + ')' + $6;
                        }
                        |
                        type_modifier operator '(' ')' statement
                        {
                                header += $1 + ' ' + $2 + ' ' + '(' + ')' + ';';

                                $$ = $1 + ' ' + $2 + ' ' + '(' + ')' + $5;
                        }
                        |
                        type_modifier operator '(' type_modifier_list ')' statement
                        {
                                header += $1 + ' ' + $2 + ' ' + '(' + $4 + ')' + ';';

                                $$ = $1 + ' ' + $2 + ' ' + '(' + $4 + ')' + $6;
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
                        ;

member:                 expression EOL
                        {
                                $$ = $1 + "; ";
                        }
                        |
                        type_modifier ID '(' ')' statement
                        {
                                $$ = $1 + $2 + '(' + ')' + $5;
                        }
                        |
                        type_modifier ID '(' type_modifier_list ')' statement
                        {
                                $$ = $1 + $2 + '(' + $4 + ')' + $6;
                        }
                        |
                        type_modifier ID '(' ')' CONST statement
                        {
                                $$ = $1 + $2 + '(' + ')' + " const " + $6;
                        }
                        |
                        type_modifier ID '(' type_modifier_list ')' CONST statement
                        {
                                $$ = $1 + $2 + '(' + $4 + ')' + " const " + $7;
                        }
                        |
                        type_modifier operator '(' ')' statement
                        {
                                $$ = $1 + ' ' + $2 + ' ' + '(' + ')' + $5;
                        }
                        |
                        type_modifier operator '(' type_modifier_list ')' statement
                        {
                                $$ = $1 + ' ' + $2 + ' ' + '(' + $4 + ')' + $6;
                        }
                        |
                        type_modifier operator '(' ')' CONST statement
                        {
                                $$ = $1 + ' ' + $2 + ' ' + '(' + ')' + " const " + $6;
                        }
                        |
                        type_modifier operator '(' type_modifier_list ')' CONST statement
                        {
                                $$ = $1 + ' ' + $2 + ' ' + '(' + $4 + ')' + " const " + $7;
                        }
                        |
                        ID '(' ')' statement
                        {
                                $$ = $1 + "(node_proxy const & __y): __x(__y) " + $4;
                        }
                        |
                        ID '(' parameter_list ')' statement
                        {
                                $$ = $1 + "(node_proxy const & __y, " + $3 + "): __x(__y) " + $5;
                        }
                        |
                        '~' ID '(' ')' statement
                        {
                                $$ = '~' + $2 + '(' + ')' + $5;
                        }
                        ;
                        
operator:               OPERATOR '+'
                        {
                                $$ = "operator +";
                        }
                        |
                        OPERATOR '-'
                        {
                                $$ = "operator -";
                        }
                        |
                        OPERATOR '*'
                        {
                                $$ = "operator *";
                        }
                        |
                        OPERATOR '/'
                        {
                                $$ = "operator /";
                        }
                        |
                        OPERATOR FUNCTION2ndLEFTSHIFT
                        {
                                $$ = "operator <<";
                        }
                        |
                        OPERATOR FUNCTION2ndRIGHTSHIFT
                        {
                                $$ = "operator >>";
                        }
                        ;
                        
qualifier_list:         qualifier_list qualifier
                        {
                                $$ = $1 + " " + $2;
                        }
                        |
                        qualifier
                        {
                                $$ = $1;
                        }
                        ;
                        
qualifier:              STATIC
                        {
                                $$ = "static";
                        }
                        |
                        VIRTUAL
                        {
                                $$ = "virtual";
                        }
                        ;

expression:             expression_binary
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
                                $$ = "operator_not(__x, " + $2 + ")";
                        }
                        |
                        expression_binary '=' expression_binary
                        {
                                $$ = $1 + " = " + $3;
                        }
                        |
                        expression_binary FUNCTION2ndOR expression_binary
                        {
                                $$ = "operator_or(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndXOR expression_binary
                        {
                                $$ = "operator_xor(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndAND expression_binary
                        {
                                $$ = "operator_and(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndEQUAL expression_binary
                        {
                                $$ = "operator_equal(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndLESS expression_binary
                        {
                                $$ = "operator_less(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndGREATER expression_binary
                        {
                                $$ = "operator_greater(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndNOTEQUAL expression_binary
                        {
                                $$ = "operator_notequal(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndLESSEQUAL expression_binary
                        {
                                $$ = "operator_lessequal(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_binary FUNCTION2ndGREATEREQUAL expression_binary
                        {
                                $$ = "operator_greaterequal(__x, " + $1 + ", " + $3 + ")";
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
                                $$ = "operator_add(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_add '-' expression_add
                        {
                                $$ = "operator_sub(__x, " + $1 + ", " + $3 + ")";
                        }
                        ;

expression_mul:         expression_signed
                        {
                                $$ = $1;
                        }
                        |
                        expression_mul '*' expression_mul
                        {
                                $$ = "operator_mul(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_mul '/' expression_mul
                        {
                                $$ = "operator_div(__x, " + $1 + ", " + $3 + ")";
                        }
                        |
                        expression_mul '%' expression_mul
                        {
                                $$ = "operator_mod(__x, " + $1 + ", " + $3 + ")";
                        }
                        ;

expression_signed:      expression_unary
                        {
                                $$ = $1;
                        }
                        |
                        expression_unary '^' expression_signed
                        {
                                $$ = "operator_pow(__x, " + $1 + ", " + $3 + ")";
                        }
                        ;

expression_unary:       expression_factorial
                        {
                                $$ = $1;
                        }
                        |
                        '|' expression '|'
                        {
                                $$ = "operator_abs(__x, " + $2 + ")";
                        }
                        |
                        '+' expression_unary
                        {
                                $$ = $2;
                        }
                        |
                        '-' expression_unary
                        {
                                $$ = "operator_neg(__x, " + $2 + ")";
                        }
                        ;

expression_factorial:   expression_factorial '!'
                        {
                                $$ = "operator_factorial(__x, " + $1 + ")";
                        }
                        |
                        expression FUNCTION2ndINCREMENT
                        {
                                $$ = "operator_postinc(__x, " + $1 + ")";
                        }
                        |
                        expression FUNCTION2ndDECREMENT
                        {
                                $$ = "operator_postdec(__x, " + $1 + ")";
                        }
                        |
                        FUNCTION2ndINCREMENT expression
                        {
                                $$ = "operator_preinc(__x, " + $2 + ")";
                        }
                        |
                        FUNCTION2ndDECREMENT expression
                        {
                                $$ = "operator_predec(__x, " + $2 + ")";
                        }
                        |
                        '(' ')'
                        {
                                $$ = "()";
                        }
                        |
                        '(' expression ')'
                        {
                                $$ = "(" + $2 + ")";
                        }
                        |
                        '[' ']'
                        {
                                $$ = "[]";
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
                                if (type.find($1) != type.end())
                                    $$ = $1 + "(__x)";
                                else
                                    $$ = "dereference(" + $1 + ")(__x)";
                        }
                        |
                        expression '(' expression_list ')'
                        {
                                if (type.find($1) != type.end())
                                    $$ = $1 + "(__x)";
                                else
                                    $$ = "dereference(" + $1 + ")(__x, " + $3 + ")";
                        }
                        |
                        FUNCTION '(' ')' statement
                        {
                                std::string name = "__" + boost::lexical_cast<std::string>(counter ++);
                                
                                header += "auto " + name + "(node_proxy & __y) " + $4;
                                header += "typedef decltype(" + name + ") * " + name + "_p_t; ";
                                
                                $$ = "& " + name;
                        }
                        |
                        FUNCTION '(' parameter_list ')' statement
                        {
                                std::string name = "__" + boost::lexical_cast<std::string>(counter ++);
                                
                                header += "auto " + name + "(node_proxy & __y, " + $3 + ") " + $5;
                                header += "typedef decltype(" + name + ") * " + name + "_p_t; ";
                                
                                $$ = "& " + name;
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
                        NEW type '(' ')'
                        {
                                if (type.find($2) != type.end())
                                    $$ = "make_fastnode<" + $2 + ">(__x, __x)";
                                else
                                    $$ = "make_fastnode<" + $2 + ">(__x)";
                        }
                        |
                        NEW type '(' expression_list ')'
                        {
                                if (type.find($2) != type.end())
                                    $$ = "make_fastnode<" + $2 + ">(__x, __x, " + $4 + ")";
                                else
                                    $$ = "make_fastnode<" + $2 + ">(__x, " + $4 + ")";
                        }
                        |
                        NEW FUNCTION '(' ')' statement
                        {
                                std::string name = "__" + boost::lexical_cast<std::string>(counter ++);
                                
                                header += "auto " + name + "(node_proxy & __y) " + $5;
                                header += "typedef decltype(& " + name + ") " + name + "_p_t; ";
                                
                                $$ = "make_fastnode<" + name + "_p_t>(__x, &" + name + ")";
                        }
                        |
                        NEW FUNCTION '(' parameter_list ')' statement
                        {
                                std::string name = "__" + boost::lexical_cast<std::string>(counter ++);
                                
                                header += "auto " + name + "(node_proxy & __y, " + $4 + ") " + $6;
                                header += "typedef decltype(& " + name + ") " + name + "_p_t; ";
                                
                                $$ = "make_fastnode<" + name + "_p_t>(__x, &" + name + ")";
                        }
                        |
                        type_modifier type
                        {
                                $$ = $1 + " " + $2;
                        }
                        |
                        type_modifier type '=' expression
                        {
                                $$ = $1 + ' ' + $2 + " = " + $4;
                        }
                        |
                        AUTO type '=' expression
                        {
                                $$ = "decltype(" + $4 + ") " + $2 + " = " + $4;
                        }
                        ;
                        
parameter_list:         parameter_list ',' type_modifier type
                        {
                                $$ = $1 + ", " + $3 + " " + $4;
                        }
                        |
                        type_modifier type
                        {
                                $$ = $1 + " " +  $2;
                        }
                        ;

type_modifier_list:     type_modifier_list ',' type_modifier
                        {
                                $$ = $1 + ", " + $3;
                        }
                        |
                        type_modifier
                        {
                                $$ = $1;
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
                        ID FUNCTION2ndLESS FUNCTION2ndGREATER
                        {
                                $$ = $1 + "<>";
                        }
                        |
                        ID FUNCTION2ndLESS type_list FUNCTION2ndGREATER
                        {
                                $$ = $1 + '<' + $3 + '>';
                        }
                        ;

type_list:              type_list ',' type
                        {
                                $$ = $1 + ",  " + $3;
                        }
                        |
                        type
                        {
                                $$ = $1;
                        }
                        ;

scope_list:             scope_list '.' type
                        {
                                $$ = "dereference(" + $1 +")." + $3;
                        }
                        |
                        type
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
