/*!
  \file
  \brief Example 2 of using root_ptr.
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
#include <iostream>

using namespace std;
using namespace Qt;


// Example
struct A
{
    string name;
    
    QNodePtr<A> i;
    QNodePtr<int> j;
    
    A(QNodeProxy const & x, string name): name(name), i(x), j(make_node<int>(x, 10)) 
    {
        cout << __PRETTY_FUNCTION__ << ": " << name << endl;         
    }
    
    ~A() 
    { 
        cout << __PRETTY_FUNCTION__ << ": " << name << endl; 
    }
};


// Metadata
namespace Qt
{
template <>
    struct info_t<A>
    {
        static void proxy(A const & o, QNodeProxy const & x)
        {
            o.i.proxy(x);
            o.j.proxy(x);
        }
    };
}

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
    struct QNodeLocal
    {
        static QNodeStack<T> stack;
    };

template <typename T>
    QNodeStack<T> QNodeLocal<T>::stack;
    
int main()
{
    cout << "Scope 0: BEGIN" << endl;
    {
        QNodeProxy x; // 1st proxy
        QNodeLocal<A>::stack.push_back(make_pair("a1", make_node<A>(x, x, "a1")));
        
        cout << "Scope 1: BEGIN" << endl;
        {
            QNodeProxy x; // 2nd proxy
            QNodeLocal<A>::stack.push_back(make_pair("b1", make_node<A>(x, x, "b1")));
            QNodeLocal<A>::stack.push_back(make_pair("b2", make_node<A>(x, x, "b2")));

            QNodeLocal<A>::stack.at("a1")->second = QNodeLocal<A>::stack.at("b1")->second;
            
            QNodeLocal<A>::stack.at("b1")->second = make_node<A>(x, x, "b3");
            QNodeLocal<A>::stack.at("b1")->second->i = QNodeLocal<A>::stack.at("b1")->second; // cycle
            
            QNodeLocal<A>::stack.pop_back();
            QNodeLocal<A>::stack.pop_back();
        }
        cout << "Scope 1: END" << endl;
        
        QNodeLocal<A>::stack.pop_back();
    }
    cout << "Scope 0: END" << endl;
}
