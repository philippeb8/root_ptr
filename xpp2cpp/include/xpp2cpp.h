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


#ifndef XPP2CPP_INCLUDED
#define XPP2CPP_INCLUDED

#include <boost/smart_ptr/root_ptr.hpp>

#include <list>
#include <string>
#include <iostream>
#include <functional>


namespace xpp2cpp
{
struct type
{
    virtual ~type() {}

    virtual node_ptr<type> & operator () () { throw std::runtime_error("wrong number of arguments"); }
    virtual node_ptr<type> & operator () (node_ptr<type> &) { throw std::runtime_error("wrong number of arguments"); }
    virtual node_ptr<type> & operator () (node_ptr<type> &, node_ptr<type> &) { throw std::runtime_error("wrong number of arguments"); }
    virtual node_ptr<type> & operator () (node_ptr<type> &, node_ptr<type> &, node_ptr<type> &) { throw std::runtime_error("wrong number of arguments"); }
    
    virtual node_ptr<type> & add(node_ptr<type> & __result, node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual node_ptr<type> & sub(node_ptr<type> & __result, node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual node_ptr<type> & mul(node_ptr<type> & __result, node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual node_ptr<type> & div(node_ptr<type> & __result, node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }

    virtual std::ostream & flush(std::ostream & out) const { return out; }
    
    friend std::ostream & operator << (std::ostream & out, node_ptr<type> const & t) { return t->flush(out); }
};


template <typename T>
    struct type_t : type
    {
        T t;

        template <typename... U>
            type_t(U... u) : t(u...)
            {
            }
            
        virtual node_ptr<type> & add(node_ptr<type> & __result, node_ptr<type> const & p) const
        {
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = make_node<type>(__result.proxy(), type_t<T>(t + q->t));
            
            throw std::runtime_error("invalid operand types");
        }
        
        virtual node_ptr<type> & sub(node_ptr<type> & __result, node_ptr<type> const & p) const
        {
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = make_node<type>(__result.proxy(), type_t<T>(t - q->t));
            
            throw std::runtime_error("invalid operand types");
        }
        
        virtual node_ptr<type> & mul(node_ptr<type> & __result, node_ptr<type> const & p) const
        {
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = make_node<type>(__result.proxy(), type_t<T>(t * q->t));
            
            throw std::runtime_error("invalid operand types");
        }
        
        virtual node_ptr<type> & div(node_ptr<type> & __result, node_ptr<type> const & p) const
        {
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = make_node<type>(__result.proxy(), type_t<T>(t / q->t));
            
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

        virtual node_ptr<type> & operator () () 
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

        virtual node_ptr<type> & operator () (node_ptr<type> & t1) 
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

        virtual node_ptr<type> & operator () (node_ptr<type> & t1, node_ptr<type> & t2) 
        { 
            return t.operator () (t1, t2); 
        }
    };

    
template <typename T, typename U>
    inline auto add(node_ptr<type> & __result, T const & p, U const & q) -> decltype(p + q)
    {
        return p + q;
    }

    inline node_ptr<type> & add(node_ptr<type> & __result, node_ptr<type> const & p, node_ptr<type> const & q)
    {
        return __result = p->add(__result, q);
    }

    
template <typename T, typename U>
    inline auto sub(node_ptr<type> & __result, T const & p, U const & q) -> decltype(p - q)
    {
        return p - q;
    }

    inline node_ptr<type> & sub(node_ptr<type> & __result, node_ptr<type> const & p, node_ptr<type> const & q)
    {
        return __result = p->sub(__result, q);
    }

    
template <typename T, typename U>
    inline auto mul(node_ptr<type> & __result, T const & p, U const & q) -> decltype(p * q)
    {
        return p * q;
    }

    inline node_ptr<type> & mul(node_ptr<type> & __result, node_ptr<type> const & p, node_ptr<type> const & q)
    {
        return __result = p->mul(__result, q);
    }

    
template <typename T, typename U>
    inline auto div(node_ptr<type> & __result, T const & p, U const & q) -> decltype(p / q)
    {
        return p / q;
    }

    inline node_ptr<type> & div(node_ptr<type> & __result, node_ptr<type> const & p, node_ptr<type> const & q)
    {
        return __result = p->div(__result, q);
    }    
}
    
#endif