/*!
    \brief Backbone++ Programming Language.

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


#ifndef BBPP_INCLUDED
#define BBPP_INCLUDED

#include <boost/smart_ptr/root_ptr.hpp>

#include <list>
#include <string>
#include <iostream>
#include <functional>


namespace bbpp
{
struct type
{
    virtual ~type() {}

    virtual boost::node_ptr<type> operator () () { throw std::runtime_error("wrong number of arguments"); }
    virtual boost::node_ptr<type> operator () (boost::node_ptr<type> &) { throw std::runtime_error("wrong number of arguments"); }
    virtual boost::node_ptr<type> operator () (boost::node_ptr<type> &, boost::node_ptr<type> &) { throw std::runtime_error("wrong number of arguments"); }
    virtual boost::node_ptr<type> operator () (boost::node_ptr<type> &, boost::node_ptr<type> &, boost::node_ptr<type> &) { throw std::runtime_error("wrong number of arguments"); }
    
    virtual boost::node_ptr<type> operator_add(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_sub(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_mul(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_div(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_mod(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_not(boost::node_ptr<type> & __result) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_or(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_xor(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_and(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_equal(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_less(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_greater(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_notequal(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_lessequal(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_greaterequal(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_leftshift(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_rightshift(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_pow(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_abs(boost::node_ptr<type> & __result) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_neg(boost::node_ptr<type> & __result) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_factorial(boost::node_ptr<type> & __result) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_postinc(boost::node_ptr<type> & __result) { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_postdec(boost::node_ptr<type> & __result) { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_preinc(boost::node_ptr<type> & __result) { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> operator_predec(boost::node_ptr<type> & __result) { throw std::runtime_error("undefined type"); }
    
    virtual std::ostream & flush(std::ostream & out) const { return out; }
    
    friend std::ostream & operator << (std::ostream & out, boost::node_ptr<type> const & t) { return t->flush(out); }
};


template <typename T>
    struct type_t : type
    {
        T t;

        template <typename... U>
            type_t(U... u) : t(u...)
            {
            }
            
        virtual boost::node_ptr<type> operator_add(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p) const
        {
            boost::node_proxy __x;
            
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = boost::make_node<type_t<T>>(__x, type_t<T>(t + q->t));
            
            throw std::runtime_error("invalid operand types");
        }
        
        virtual boost::node_ptr<type> operator_sub(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p) const
        {
            boost::node_proxy __x;
            
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = boost::make_node<type_t<T>>(__x, type_t<T>(t - q->t));
            
            throw std::runtime_error("invalid operand types");
        }
        
        virtual boost::node_ptr<type> operator_mul(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p) const
        {
            boost::node_proxy __x;
            
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = boost::make_node<type_t<T>>(__x, type_t<T>(t * q->t));
            
            throw std::runtime_error("invalid operand types");
        }
        
        virtual boost::node_ptr<type> operator_div(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p) const
        {
            boost::node_proxy __x;
            
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = boost::make_node<type_t<T>>(__x, type_t<T>(t / q->t));
            
            throw std::runtime_error("invalid operand types");
        }

        virtual std::ostream & flush(std::ostream & out) const { return out << t; }
    };

  
template <typename T>
    struct function0_t : type
    {
        typedef typename std::function<T>::result_type result_type;

        std::function<T> t;

        template <typename... U>
            function0_t(U... u) : t(u...) 
            {
            }

        virtual boost::node_ptr<type> operator () () 
        { 
            return t.operator () (); 
        }
    };

  
template <typename T>
    struct function1_t : type
    {
        typedef typename std::function<T>::result_type result_type;

        std::function<T> t;

        template <typename... U>
            function1_t(U... u) : t(u...) 
            {
            }

        virtual boost::node_ptr<type> operator () (boost::node_ptr<type> & t1) 
        { 
            return t.operator () (t1); 
        }
    };

  
template <typename T>
    struct function2_t : type
    {
        typedef typename std::function<T>::result_type result_type;

        std::function<T> t;

        template <typename... U>
            function2_t(U... u) : t(u...) 
            {
            }

        virtual boost::node_ptr<type> operator () (boost::node_ptr<type> & t1, boost::node_ptr<type> & t2) 
        { 
            return t.operator () (t1, t2); 
        }
    };

#define CONSTRUCT_UNARY_FUNCTION(name, function)                                                                                                                \
template <typename T>                                                                                                                                           \
    inline auto operator_ ## name(boost::node_ptr<type> & __result, T const & p) -> decltype(function(p))                                                       \
    {                                                                                                                                                           \
        return function(p);                                                                                                                                     \
    }                                                                                                                                                           \
                                                                                                                                                                \
    inline boost::node_ptr<type> operator_ ## name(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p)                                         \
    {                                                                                                                                                           \
        return __result = p->operator_ ## name(__result);                                                                                                       \
    }

#define CONSTRUCT_UNARY_PREOPERATOR(name, symbol)                                                                                                               \
template <typename T>                                                                                                                                           \
    inline auto operator_ ## name(boost::node_ptr<type> & __result, T & p) -> decltype(p)                                                                       \
    {                                                                                                                                                           \
        return symbol p;                                                                                                                                        \
    }                                                                                                                                                           \
                                                                                                                                                                \
    inline boost::node_ptr<type> operator_ ## name(boost::node_ptr<type> & __result, boost::node_ptr<type> & p)                                               \
    {                                                                                                                                                           \
        return __result = p->operator_ ## name(__result);                                                                                                       \
    }

#define CONSTRUCT_UNARY_POSTOPERATOR(name, symbol)                                                                                                              \
template <typename T>                                                                                                                                           \
    inline auto operator_ ## name(boost::node_ptr<type> & __result, T const & p) -> decltype(p)                                                                 \
    {                                                                                                                                                           \
        return p symbol;                                                                                                                                        \
    }                                                                                                                                                           \
                                                                                                                                                                \
    inline boost::node_ptr<type> operator_ ## name(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p)                                         \
    {                                                                                                                                                           \
        return __result = p->operator_ ## name(__result);                                                                                                       \
    }

#define CONSTRUCT_BINARY_FUNCTION(name, function)                                                                                                               \
template <typename T, typename U>                                                                                                                               \
    inline auto operator_ ## name(boost::node_ptr<type> & __result, T const & p, U const & q) -> decltype(function(p, q))                                       \
    {                                                                                                                                                           \
        return function(p, q);                                                                                                                                  \
    }                                                                                                                                                           \
                                                                                                                                                                \
    inline boost::node_ptr<type> operator_ ## name(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p, boost::node_ptr<type> const & q)        \
    {                                                                                                                                                           \
        return __result = p->operator_ ## name(__result, q);                                                                                                    \
    }

#define CONSTRUCT_BINARY_OPERATOR(name, symbol)                                                                                                                 \
template <typename T, typename U>                                                                                                                               \
    inline auto operator_ ## name(boost::node_ptr<type> & __result, T const & p, U const & q) -> decltype(p symbol q)                                           \
    {                                                                                                                                                           \
        return p symbol q;                                                                                                                                      \
    }                                                                                                                                                           \
                                                                                                                                                                \
    inline boost::node_ptr<type> operator_ ## name(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p, boost::node_ptr<type> const & q)        \
    {                                                                                                                                                           \
        return __result = p->operator_ ## name(__result, q);                                                                                                    \
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
