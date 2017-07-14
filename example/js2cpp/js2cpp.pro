TARGET          = js2cpp

INCLUDEPATH     += ../include
INCLUDEPATH     += ../../include

LIBS            += -lfl

QMAKE_CXXFLAGS  += -std=c++11

QMAKE_LEX       = flex
QMAKE_LEXFLAGS  = -+ -8 -i -f -L -olex.lexer.c
QMAKE_YACC      = bison++
QMAKE_YACCFLAGS = -d -l

YACCSOURCES     += parser.yy

LEXSOURCES      += lexer.ll
