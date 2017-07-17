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

    virtual boost::node_ptr<type> & operator () () { throw std::runtime_error("wrong number of arguments"); }
    virtual boost::node_ptr<type> & operator () (boost::node_ptr<type> &) { throw std::runtime_error("wrong number of arguments"); }
    virtual boost::node_ptr<type> & operator () (boost::node_ptr<type> &, boost::node_ptr<type> &) { throw std::runtime_error("wrong number of arguments"); }
    virtual boost::node_ptr<type> & operator () (boost::node_ptr<type> &, boost::node_ptr<type> &, boost::node_ptr<type> &) { throw std::runtime_error("wrong number of arguments"); }
    
    virtual boost::node_ptr<type> & add(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> & sub(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> & mul(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }
    virtual boost::node_ptr<type> & div(boost::node_ptr<type> & __result, boost::node_ptr<type> const &) const { throw std::runtime_error("undefined type"); }

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
            
        virtual boost::node_ptr<type> & add(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p) const
        {
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = boost::make_node<type>(__result.proxy(), type_t<T>(t + q->t));
            
            throw std::runtime_error("invalid operand types");
        }
        
        virtual boost::node_ptr<type> & sub(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p) const
        {
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = boost::make_node<type>(__result.proxy(), type_t<T>(t - q->t));
            
            throw std::runtime_error("invalid operand types");
        }
        
        virtual boost::node_ptr<type> & mul(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p) const
        {
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = boost::make_node<type>(__result.proxy(), type_t<T>(t * q->t));
            
            throw std::runtime_error("invalid operand types");
        }
        
        virtual boost::node_ptr<type> & div(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p) const
        {
            if (auto q = dynamic_cast<type_t<T> const *>(p.get()))
                return __result = boost::make_node<type>(__result.proxy(), type_t<T>(t / q->t));
            
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

        virtual boost::node_ptr<type> & operator () () 
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

        virtual boost::node_ptr<type> & operator () (boost::node_ptr<type> & t1) 
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

        virtual boost::node_ptr<type> & operator () (boost::node_ptr<type> & t1, boost::node_ptr<type> & t2) 
        { 
            return t.operator () (t1, t2); 
        }
    };

    
template <typename T, typename U>
    inline auto add(boost::node_ptr<type> & __result, T const & p, U const & q) -> decltype(p + q)
    {
        return p + q;
    }

    inline boost::node_ptr<type> & add(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p, boost::node_ptr<type> const & q)
    {
        return __result = p->add(__result, q);
    }

    
template <typename T, typename U>
    inline auto sub(boost::node_ptr<type> & __result, T const & p, U const & q) -> decltype(p - q)
    {
        return p - q;
    }

    inline boost::node_ptr<type> & sub(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p, boost::node_ptr<type> const & q)
    {
        return __result = p->sub(__result, q);
    }

    
template <typename T, typename U>
    inline auto mul(boost::node_ptr<type> & __result, T const & p, U const & q) -> decltype(p * q)
    {
        return p * q;
    }

    inline boost::node_ptr<type> & mul(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p, boost::node_ptr<type> const & q)
    {
        return __result = p->mul(__result, q);
    }

    
template <typename T, typename U>
    inline auto div(boost::node_ptr<type> & __result, T const & p, U const & q) -> decltype(p / q)
    {
        return p / q;
    }

    inline boost::node_ptr<type> & div(boost::node_ptr<type> & __result, boost::node_ptr<type> const & p, boost::node_ptr<type> const & q)
    {
        return __result = p->div(__result, q);
    }    
}
    
#endif