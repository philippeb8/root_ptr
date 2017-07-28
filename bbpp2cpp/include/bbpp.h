/*!
    Backbone++ Programming Language.

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


#ifndef BBPP_INCLUDED
#define BBPP_INCLUDED

#include <boost/smart_ptr/root_ptr.hpp>

#include <list>
#include <string>
#include <iostream>


namespace bbpp
{
template <typename T>
    inline T & dereference(boost::node_ptr<T> & t)
    {
        return * t;
    }

template <typename T>
    inline T const & dereference(boost::node_ptr<T> const & t)
    {
        return * t;
    }

template <typename T>
    inline T & dereference(T & t)
    {
        return t;
    }
    
template <typename T>
    inline T const & dereference(T const & t)
    {
        return t;
    }
    
template <typename T>
    inline boost::node_ptr<T> & proxy(boost::node_proxy const & x, boost::node_ptr<T> & t)
    {
        return t.proxy(x), t;
    }

template <typename T>
    inline boost::node_ptr<T> const & proxy(boost::node_proxy const & x, boost::node_ptr<T> const & t)
    {
        return t.proxy(x), t;
    }

template <typename T>
    inline T & proxy(boost::node_proxy const & x, T & t)
    {
        return t;
    }    

template <typename T>
    inline T const & proxy(boost::node_proxy const & x, T const & t)
    {
        return t;
    }        

    
#define CONSTRUCT_UNARY_FUNCTION(name, function)                                     \
template <typename T>                                                                \
    inline auto operator_ ## name(boost::node_proxy & __y, T const & p)              \
    {                                                                                \
        return function(dereference(p));                                             \
    }                                                                                

#define CONSTRUCT_UNARY_PREOPERATOR(name, symbol)                                    \
template <typename T>                                                                \
    inline auto operator_ ## name(boost::node_proxy & __y, T & p)                    \
    {                                                                                \
        return symbol dereference(p);                                                \
    }                                                                                

#define CONSTRUCT_UNARY_POSTOPERATOR(name, symbol)                                   \
template <typename T>                                                                \
    inline auto operator_ ## name(boost::node_proxy & __y, T const & p)              \
    {                                                                                \
        return dereference(p) symbol;                                                \
    }                                                                                

#define CONSTRUCT_BINARY_FUNCTION(name, function)                                    \
template <typename T, typename U>                                                    \
    inline auto operator_ ## name(boost::node_proxy & __y, T const & p, U const & q) \
    {                                                                                \
        return function(dereference(p), dereference(q));                             \
    }                                                                                

#define CONSTRUCT_BINARY_OPERATOR(name, symbol)                                      \
template <typename T, typename U>                                                    \
    inline auto operator_ ## name(boost::node_proxy & __y, T const & p, U const & q) \
    {                                                                                \
        return dereference(p) symbol dereference(q);                                 \
    }                                                                                                                                                           
          

CONSTRUCT_BINARY_OPERATOR(add, +)
CONSTRUCT_BINARY_OPERATOR(sub, -)
CONSTRUCT_BINARY_OPERATOR(mul, *)
CONSTRUCT_BINARY_OPERATOR(div, /)
CONSTRUCT_BINARY_OPERATOR(mod, %)
CONSTRUCT_UNARY_PREOPERATOR(not, not)
CONSTRUCT_BINARY_OPERATOR(or, or)
CONSTRUCT_BINARY_OPERATOR(xor, xor)
CONSTRUCT_BINARY_OPERATOR(and, and)
CONSTRUCT_BINARY_OPERATOR(equal, ==)
CONSTRUCT_BINARY_OPERATOR(less, <)
CONSTRUCT_BINARY_OPERATOR(greater, >)
CONSTRUCT_BINARY_OPERATOR(notequal, !=)
CONSTRUCT_BINARY_OPERATOR(lessequal, <=)
CONSTRUCT_BINARY_OPERATOR(greaterequal, >=)
//CONSTRUCT_BINARY_OPERATOR(leftshift, <<)
//CONSTRUCT_BINARY_OPERATOR(rightshift, >>)
CONSTRUCT_BINARY_FUNCTION(pow, pow)
CONSTRUCT_UNARY_FUNCTION(abs, abs)
CONSTRUCT_UNARY_PREOPERATOR(neg, -)
//CONSTRUCT_BINARY_OPERATOR(factorial, std::factorial())
CONSTRUCT_UNARY_PREOPERATOR(preinc, ++)
CONSTRUCT_UNARY_PREOPERATOR(predec, --)
CONSTRUCT_UNARY_POSTOPERATOR(postinc, ++)
CONSTRUCT_UNARY_POSTOPERATOR(postdec, --)
}
    
#endif
