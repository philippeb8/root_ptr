INCLUDEPATH += ../include
INCLUDEPATH += ../../include

QMAKE_CXXFLAGS += -std=c++11

QMAKE_LEX       = flex
QMAKE_LEXFLAGS  = -+ -8 -i -f -L -olex.lexer.c
QMAKE_YACC      = bison++
QMAKE_YACCFLAGS = -d -l

TARGET    = js2cpp

YACCSOURCES += parser.yy parser_postfix.yy

LEXSOURCES  += lexer.ll

HEADERS	 += parser_yacc.h parser_postfix_yacc.h
SOURCES	 += parser_yacc.cpp parser_postfix_yacc.cpp lexer_lex.cpp
