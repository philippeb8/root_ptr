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
#include <functional>

using namespace std;
using namespace Qt;


struct type
{
    type() {}
    virtual ~type() {}
    virtual QNodePtr<type> operator () () {}
    virtual QNodePtr<type> operator () (QNodePtr<type> &) {}
    virtual QNodePtr<type> operator () (QNodePtr<type> &, QNodePtr<type> &) {}
    virtual QNodePtr<type> operator () (QNodePtr<type> &, QNodePtr<type> &, QNodePtr<type> &) {}
    
    virtual ostream & flush(ostream & out) const { return out; }
    
    friend ostream & operator << (ostream & out, QNodePtr<type> const & t) { return t->flush(out); }
    //friend QNodePtr<type> const & operator ++ (QNodePtr<type> const & t1);
    friend QNodePtr<type> operator + (QNodePtr<type> const & t1, QNodePtr<type> const & t2);
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

/*
inline QNodePtr<type> const & operator ++ (QNodePtr<type> const & t1)
{ 
    if (type_t<int> * p1 = dynamic_cast<type_t<int> *>(t1.get()))
        return ++ p1->t, t1;
}
*/

inline QNodePtr<type> operator + (QNodePtr<type> const & t1, QNodePtr<type> const & t2)
{ 
    if (type_t<int> * p1 = dynamic_cast<type_t<int> *>(t1.get()))
        if (type_t<int> * p2 = dynamic_cast<type_t<int> *>(t2.get()))
            return make_node<type_t<int>>(t1.proxy(), type_t<int>(p1->t + p2->t)); 
}
  
template <typename T>
    struct function_t : type
    {
        typedef typename function<T>::result_type result_type;

        function<T> t;

        template <typename... U>
            function_t(U... u) : t(u...) 
            {
            }

        virtual QNodePtr<type> operator () () 
        { 
            return t.operator () (); 
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
    struct QNodeLocal
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
    QNodeStack<T> QNodeLocal<T>::stack;
    
int main()
{
    cout << __PRETTY_FUNCTION__ << ": SCOPE 1 - BEGIN" << endl; 
    {
        QNodeProxy x; // 1st proxy
        QNodeLocal<type>::Reserve r(1);
        QNodeLocal<type>::stack.push_back(make_pair("f", make_node<function_t<QNodePtr<type> ()>>(x, function_t<QNodePtr<type> ()>([] () -> QNodePtr<type> 
        { 
            cout << __PRETTY_FUNCTION__ << ": SCOPE 2 - BEGIN" << endl; 
            QNodeProxy x; // 2nd proxy
            QNodeLocal<type>::Reserve r(2);
            QNodeLocal<type>::stack.push_back(make_pair("a", make_node<type_t<int>>(x, type_t<int>(10))));
            QNodeLocal<type>::stack.push_back(make_pair("v", (*make_node<function_t<QNodePtr<type> ()>>(x, function_t<QNodePtr<type> ()>([] () -> QNodePtr<type> 
            { 
                cout << __PRETTY_FUNCTION__ << ": SCOPE 3 - BEGIN" << endl; 
                QNodeProxy x; // 3rd proxy
                QNodeLocal<type>::Reserve r(1);
                QNodeLocal<type>::stack.push_back(make_pair("b", make_node<type_t<int>>(x, type_t<int>(10))));
                
                return (QNodeLocal<type>::stack.at("a")->second + QNodeLocal<type>::stack.at("b")->second);
            })))()));

            cout << QNodeLocal<type>::stack.at("v")->second << endl;

            return QNodePtr<type>(x);
        }))));

        (*QNodeLocal<type>::stack.at("f")->second)();
    }
}

/*
// Example
struct A : type
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
*/