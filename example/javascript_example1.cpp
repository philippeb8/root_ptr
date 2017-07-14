/*!
  \file
  \brief Javascript emulation.
*/
/*
    Root Pointer - Deterministic Memory Manager.

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


#include "javascript.h"


/**
    Emulation of the following Javascript code:
    
    var temporary;
    
    function bar(object)
    {
        return 10;
    }
    
    function foo()
    {
        var object;
        var result = function() { return object }
        return function() { return bar( object ) }
    }
    
    for (var i = 0; i < 1000000; ++ i)
        (foo())(temporary);
*/


int main()
{
    //cout << __PRETTY_FUNCTION__ << ": BEGIN" << endl; 
    {
        QNodeProxy x;
        QStackArea<type>::Reserve r(3);
        QStackArea<type>::stack().push_back(make_pair("bar", make_node<function2_t<QNodePtr<type> & (QNodePtr<type> &, QNodePtr<type> &)>>(x, function2_t<QNodePtr<type> & (QNodePtr<type> &, QNodePtr<type> &)>([] (QNodePtr<type> & result, QNodePtr<type> &) -> QNodePtr<type> &
        { 
            //cout << __PRETTY_FUNCTION__ << endl; // main()::__lambda0

            QNodeProxy x;

            return result = make_node<type_t<int>>(x, type_t<int>(10));
        }))));
        QStackArea<type>::stack().push_back(make_pair("temporary", make_node<type>(x, type())));
        QStackArea<type>::stack().push_back(make_pair("foo", make_node<function1_t<QNodePtr<type> & (QNodePtr<type> &)>>(x, function1_t<QNodePtr<type> & (QNodePtr<type> &)>([] (QNodePtr<type> & result) -> QNodePtr<type> &
        { 
            //cout << __PRETTY_FUNCTION__ << endl; // main()::__lambda1
            
            QNodeProxy x;
            QStackArea<type>::Reserve r(2);
            QStackArea<type>::stack().push_back(make_pair("object", make_node<type_t<int>>(x, type_t<int>(30))));
            QStackArea<type>::stack().push_back(make_pair("result", make_node<function1_t<QNodePtr<type> & (QNodePtr<type> &)>>(x, function1_t<QNodePtr<type> & (QNodePtr<type> &)>([] (QNodePtr<type> & result) -> QNodePtr<type> &
            { 
                //cout << __PRETTY_FUNCTION__ << endl; 

                return result = QStackArea<type>::stack().at("object")->second;
            }))));
            
            return result = make_node<function1_t<QNodePtr<type> & (QNodePtr<type> &)>>(x, function1_t<QNodePtr<type> & (QNodePtr<type> &)>([] (QNodePtr<type> & result) -> QNodePtr<type> &
            { 
                //cout << __PRETTY_FUNCTION__ << endl; // main()::__lambda1::__lambda3
                
                QNodeProxy x;
                QStackArea<type>::Reserve r(1);
                QStackArea<type>::stack().push_back(make_pair("temporary", make_node<type>(x, type()))); 
            
                return result = (* QStackArea<type>::stack().at("bar")->second)(QStackArea<type>::stack().at("temporary")->second, QStackArea<type>::stack().at("object")->second);
            }));
        }))));

        for (int i = 0; i < 1000000; ++ i)
            cout << (* (* QStackArea<type>::stack().at("foo")->second)(QStackArea<type>::stack().at("temporary")->second))(QStackArea<type>::stack().at("temporary")->second) << endl;
    }
    //cout << __PRETTY_FUNCTION__ << ": END" << endl; 
}
