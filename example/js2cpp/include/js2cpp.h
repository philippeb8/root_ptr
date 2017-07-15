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


struct QType
{
    virtual ~QType() {}
    virtual QNodePtr<QType> & operator () () { throw std::runtime_error("wrong number of arguments"); }
    virtual QNodePtr<QType> & operator () (QNodePtr<QType> &) { throw std::runtime_error("wrong number of arguments"); }
    virtual QNodePtr<QType> & operator () (QNodePtr<QType> &, QNodePtr<QType> &) { throw std::runtime_error("wrong number of arguments"); }
    virtual QNodePtr<QType> & operator () (QNodePtr<QType> &, QNodePtr<QType> &, QNodePtr<QType> &) { throw std::runtime_error("wrong number of arguments"); }
    
    virtual std::ostream & flush(std::ostream & out) const { return out; }
    
    friend std::ostream & operator << (std::ostream & out, QNodePtr<QType> const & t) { return t->flush(out); }
};


template <typename T>
    struct QTypeType : QType
    {
        T t;

        template <typename... U>
            QTypeType(U... u) : t(u...)
            {
            }
            
        virtual std::ostream & flush(std::ostream & out) const { return out << t; }
    };

  
template <typename T>
    struct QFunction0Type : QType
    {
        typedef typename std::function<T>::result_type result_type;

        std::function<T> t;

        template <typename... U>
            QFunction0Type(U... u) : t(u...) 
            {
            }

        virtual QNodePtr<QType> & operator () () 
        { 
            return t.operator () (); 
        }
    };

  
template <typename T>
    struct QFunction1Type : QType
    {
        typedef typename std::function<T>::result_type result_type;

        std::function<T> t;

        template <typename... U>
            QFunction1Type(U... u) : t(u...) 
            {
            }

        virtual QNodePtr<QType> & operator () (QNodePtr<QType> & t1) 
        { 
            return t.operator () (t1); 
        }
    };

  
template <typename T>
    struct QFunction2Type : QType
    {
        typedef typename std::function<T>::result_type result_type;

        std::function<T> t;

        template <typename... U>
            QFunction2Type(U... u) : t(u...) 
            {
            }

        virtual QNodePtr<QType> & operator () (QNodePtr<QType> & t1, QNodePtr<QType> & t2) 
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