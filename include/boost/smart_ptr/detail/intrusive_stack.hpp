/**
    @file
    Boost QIntrusiveStack.hpp header file.

    @note
    Copyright (c) 2008 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_INTRUSIVE_STACK_HPP_INCLUDED
#define BOOST_INTRUSIVE_STACK_HPP_INCLUDED


#include <boost/smart_ptr/detail/classof.hpp>


namespace Qt
{

using namespace boost;

namespace smart_ptr
{

namespace detail
{


struct QIntrusiveStackNode
{
    QIntrusiveStackNode * next;
    
    void insert(QIntrusiveStackNode * const p)
    {
        p->next = next;
        next = p;
    }
};


class QIntrusiveStackBase
{
protected:
    QIntrusiveStackNode impl;
    
    QIntrusiveStackBase()
    { 
        clear(); 
    }

    void clear()
    {
        impl.next = & impl;
    }
};


/**
    Static stack.
    
    Rewritten stack template with explicit access to internal QNodes.  This 
    allows usages of tags already part of an object, used to group objects 
    together without the need of any memory allocation.
*/

class QIntrusiveStack : protected QIntrusiveStackBase
{
    typedef QIntrusiveStackBase base;

public:
    typedef QIntrusiveStackNode QNode;
    typedef QIntrusiveStackNode * pointer;
    template <typename T, QIntrusiveStackNode T::* P> 
        struct iterator;

protected:
    using base::impl;

public:
    pointer begin()
    { 
        return impl.next; 
    }
    
    pointer end()
    { 
        return & impl; 
    }

    bool empty() const
    { 
        return impl.next == & impl; 
    }
    
    void push(pointer i)
    {
        end()->insert(i);
    }
};


template <typename T, QIntrusiveStackNode T::* P>
    struct QIntrusiveStack::iterator
    {
        typedef iterator self_type;
        typedef QIntrusiveStackNode QNodeType;

        iterator() 
        : node_() 
        {
        }
        
        iterator(QIntrusiveStack::pointer __x) 
        : node_(__x) 
        {
        }

        T & operator * () const
        { 
            return * classof(P, node_); 
        }
        
        T * operator -> () const
        { 
            return classof(P, node_); 
        }

        self_type & operator ++ ()
        {
            node_ = node_->next;
            return * this;
        }

        bool operator == (const self_type & x) const 
        { 
            return node_ == x.node_; 
        }
        
        bool operator != (const self_type & x) const 
        { 
            return node_ != x.node_; 
        }

        QNodeType * node_;
    };


} // namespace detail

} // namespace smart_ptr

} // namespace Qt


#endif // #ifndef BOOST_INTRUSIVE_STACK_HPP_INCLUDED
