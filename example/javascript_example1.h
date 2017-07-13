/*!
  \file
  \brief Javascript emulation.
*/
/*
  Copyright Phil Bouchard 2017
  Distributed under the Boost Software License, Version 1.0.
  See accompanying file LICENSE_1_0.txt or copy at
  http://www.boost.org/LICENSE_1_0.txt

  See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_JAVASCRIPT_EXAMPLE1_INCLUDED
#define BOOST_JAVASCRIPT_EXAMPLE1_INCLUDED

#include <boost/smart_ptr/root_ptr.hpp>

#include <list>
#include <string>
#include <iostream>
#include <functional>


using namespace std;
using namespace Qt;


struct type
{
    virtual ~type() {}
    virtual QNodePtr<type> & operator () () {}
    virtual QNodePtr<type> & operator () (QNodePtr<type> &) {}
    virtual QNodePtr<type> & operator () (QNodePtr<type> &, QNodePtr<type> &) {}
    virtual QNodePtr<type> & operator () (QNodePtr<type> &, QNodePtr<type> &, QNodePtr<type> &) {}
    
    virtual ostream & flush(ostream & out) const { return out; }
    
    friend ostream & operator << (ostream & out, QNodePtr<type> const & t) { return t->flush(out); }
};


template <typename T>
    struct type_t : type
    {
        T t;

        template <typename... U>
            type_t(U... u) : t(u...)
            {
            }
            
        virtual ostream & flush(ostream & out) const { return out << t; }
    };

  
template <typename T>
    struct function0_t : type
    {
        typedef typename function<T>::result_type result_type;

        function<T> t;

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
        typedef typename function<T>::result_type result_type;

        function<T> t;

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
        typedef typename function<T>::result_type result_type;

        function<T> t;

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
    struct QNodeStack : list<pair<string, QNodePtr<T>>>
    {
        typedef list<pair<string, QNodePtr<T>>> base;
        typedef typename base::iterator iterator;
        typedef typename base::reverse_iterator reverse_iterator;
        
        using base::rbegin;
        using base::rend;
        
        reverse_iterator at(string const & s) 
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
            int const n;
            
            Reserve(int n) : n(n) {}
            
            ~Reserve()
            {
                for (int i = 0; i < n; ++ i)
                    stack().pop_back();
            }
        };
    };
    
    
#endif