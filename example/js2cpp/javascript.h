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


#ifndef BOOST_JAVASCRIPT_EXAMPLE1_INCLUDED
#define BOOST_JAVASCRIPT_EXAMPLE1_INCLUDED

#include <boost/smart_ptr/root_ptr.hpp>

#include <list>
#include <string>
#include <iostream>
#include <functional>


namespace js2cpp
{
using namespace Qt;


struct type
{
    virtual ~type() {}
    virtual QNodePtr<type> & operator () () {}
    virtual QNodePtr<type> & operator () (QNodePtr<type> &) {}
    virtual QNodePtr<type> & operator () (QNodePtr<type> &, QNodePtr<type> &) {}
    virtual QNodePtr<type> & operator () (QNodePtr<type> &, QNodePtr<type> &, QNodePtr<type> &) {}
    
    virtual std::ostream & flush(std::ostream & out) const { return out; }
    
    friend std::ostream & operator << (std::ostream & out, QNodePtr<type> const & t) { return t->flush(out); }
};


template <typename T>
    struct type_t : type
    {
        T t;

        template <typename... U>
            type_t(U... u) : t(u...)
            {
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

        virtual QNodePtr<type> & operator () () 
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

        virtual QNodePtr<type> & operator () (QNodePtr<type> & t1) 
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

        virtual QNodePtr<type> & operator () (QNodePtr<type> & t1, QNodePtr<type> & t2) 
        { 
            return t.operator () (t1, t2); 
        }
    };
  
    
template <typename T>
    struct QNodeStack : std::list<std::pair<std::string, QNodePtr<T>>>
    {
        typedef std::list<std::pair<std::string, QNodePtr<T>>> base;
        typedef typename base::iterator iterator;
        typedef typename base::reverse_iterator reverse_iterator;
        
        using base::rbegin;
        using base::rend;
        
        reverse_iterator at(std::string const & s) 
        { 
            for (reverse_iterator i = rbegin(); i != rend(); ++ i)
                if (s == i->first)
                    return i;

            return rend();
        }        
    };

    
template <typename T>
    struct QStackArea
    {
        static QNodeStack<T> & stack()
        {
            static QNodeStack<T> stack_;
            
            return stack_;
        }
        
        struct Reserve
        {
            int n;
            
            Reserve(int n = 0) : n(n) {}
            
            ~Reserve()
            {
                for (int i = 0; i < n; ++ i)
                    stack().pop_back();
            }
        };
    };
}
    
#endif