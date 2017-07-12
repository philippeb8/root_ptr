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


#include <boost/smart_ptr/root_ptr.hpp>
#include <boost/mpl/range_c.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/array.hpp>
#include <boost/container/list.hpp>

#include <map>
#include <list>
#include <vector>
#include <string>
#include <typeinfo>
#include <iostream>
#include <functional>

using namespace std;
using namespace Qt;


struct type
{
    type() {}
    virtual ~type() {}
    virtual void operator () () {}
    virtual void operator () (QNodePtr<type> &) {}
    virtual void operator () (QNodePtr<type> &, QNodePtr<type> &) {}
    virtual void operator () (QNodePtr<type> &, QNodePtr<type> &, QNodePtr<type> &) {}
    
    virtual ostream & flush(ostream & out) const { return out; }
    
    friend ostream & operator << (ostream & out, QNodePtr<type> const & t) { return t->flush(out); }
};


template <typename T>
    struct type_t : type
    {
        T t;

        template <typename... U>
            type_t(U... u) : type(), t(u...)
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

        virtual void operator () () 
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

        virtual void operator () (QNodePtr<type> & t1) 
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

        virtual void operator () (QNodePtr<type> & t1, QNodePtr<type> & t2) 
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
        static QNodeStack<T> stack;
        
        struct Reserve
        {
            int const n;
            
            Reserve(int n) : n(n) {}
            
            ~Reserve()
            {
                for (int i = 0; i < n; ++ i)
                    stack.pop_back();
            }
        };
    };

template <typename T>
    QNodeStack<T> QStackArea<T>::stack;

    
/**
    Emulation of the following Javascript code:
    
    function foo()
    {
        var object;
        var result = function() { return object }
        return function() { return bar( object ) }
    }();
*/


int main()
{
    cout << __PRETTY_FUNCTION__ << ": BEGIN" << endl; 
    {
        QNodeProxy x;
        QStackArea<type>::Reserve r(3);
        QStackArea<type>::stack.push_back(make_pair("bar", make_node<function2_t<void (QNodePtr<type> &, QNodePtr<type> &)>>(x, function2_t<void (QNodePtr<type> &, QNodePtr<type> &)>([] (QNodePtr<type> & result, QNodePtr<type> &) -> void
        { 
            cout << __PRETTY_FUNCTION__ << endl; // main()::__lambda0

            QNodeProxy x;

            result = make_node<type_t<int>>(x, type_t<int>(10));
            
            return;
        }))));
        QStackArea<type>::stack.push_back(make_pair("result", make_node<type_t<int>>(x, type_t<int>(20))));
        QStackArea<type>::stack.push_back(make_pair("foo", make_node<function1_t<void (QNodePtr<type> &)>>(x, function1_t<void (QNodePtr<type> &)>([] (QNodePtr<type> & result) -> void
        { 
            cout << __PRETTY_FUNCTION__ << endl; // main()::__lambda1
            
            QNodeProxy x;
            QStackArea<type>::Reserve r(2);
            QStackArea<type>::stack.push_back(make_pair("object", make_node<type_t<int>>(x, type_t<int>(30))));
            QStackArea<type>::stack.push_back(make_pair("result", make_node<function1_t<void (QNodePtr<type> &)>>(x, function1_t<void (QNodePtr<type> &)>([] (QNodePtr<type> & result) -> void
            { 
                cout << __PRETTY_FUNCTION__ << endl; 

                result = QStackArea<type>::stack.at("object")->second;

                return;
            }))));
            
            result = make_node<function1_t<void (QNodePtr<type> &)>>(x, function1_t<void (QNodePtr<type> &)>([] (QNodePtr<type> & result) -> void
            { 
                cout << __PRETTY_FUNCTION__ << endl; // main()::__lambda1::__lambda3
                
                QNodeProxy x;
                QStackArea<type>::Reserve r(1);
                QStackArea<type>::stack.push_back(make_pair("result", make_node<type>(x, type()))); 
            
                (*QStackArea<type>::stack.at("bar")->second)(QStackArea<type>::stack.at("result")->second, QStackArea<type>::stack.at("object")->second);
                
                result = QStackArea<type>::stack.at("result")->second;
                
                return;
            }));
            
            return;
        }))));

        (* QStackArea<type>::stack.at("foo")->second)(QStackArea<type>::stack.at("result")->second);
        (* QStackArea<type>::stack.at("result")->second)(QStackArea<type>::stack.at("result")->second);
    }
    cout << __PRETTY_FUNCTION__ << ": END" << endl; 
}
