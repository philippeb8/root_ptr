/**
    @file
    Boost QIntrusiveList.hpp header file.

    @note
    Copyright (c) 2008 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_INTRUSIVE_LIST_HPP_INCLUDED
#define BOOST_INTRUSIVE_LIST_HPP_INCLUDED


#include <boost/smart_ptr/detail/classof.hpp>


namespace Qt
{

using namespace boost;

namespace smart_ptr
{

namespace detail
{


struct QIntrusiveListNode
{
    QIntrusiveListNode * next;
    QIntrusiveListNode * prev;
    
    QIntrusiveListNode() : next(this), prev(this)
    {
    }

    void insert(QIntrusiveListNode * const p)
    {
        p->next = this;
        p->prev = prev;
        
        prev->next = p;
        prev = p;
    }

    void erase()
    {
        prev->next = next;
        next->prev = prev;
        
        next = this;
        prev = this;
    }
    
    ~QIntrusiveListNode()
    {
        erase();
    }
};


class QIntrusiveListBase
{
protected:
    QIntrusiveListNode impl;

    void clear()
    {
        impl.next = & impl;
        impl.prev = & impl;
    }
};


/**
    Static list.
    
    Rewritten list template with explicit access to internal QNodes.  This 
    allows usages of tags already part of an object, used to group objects 
    together without the need of any memory allocation.
*/

class QIntrusiveList : protected QIntrusiveListBase
{
    typedef QIntrusiveListBase base;

public:
    typedef QIntrusiveListNode QNode;
    typedef QIntrusiveListNode * pointer;
    template <typename T, QIntrusiveListNode T::* P> 
        struct iterator;

protected:
    using base::impl;

public:
    QIntrusiveList()                                
    {
    }
    
    QIntrusiveList(QIntrusiveList & x)
    {
        merge(x);
    }
    
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
    
    void push_front(pointer i)
    {
        begin()->insert(i);
    }
    
    void push_back(pointer i)
    {
        end()->insert(i);
    }
    
    void merge(QIntrusiveList& x)
    {
        if (! x.empty())
        {
            x.impl.prev->next = impl.next;
            impl.next->prev = x.impl.prev;
            
            impl.next = x.impl.next;
            x.impl.next->prev = & impl;

            x.clear();
        }
    }
};


template <typename T, QIntrusiveListNode T::* P>
    struct QIntrusiveList::iterator
    {
        typedef iterator self_type;
        typedef QIntrusiveListNode QNodeType;

        iterator(QIntrusiveList::pointer __x) 
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

        self_type & operator = (self_type const & x)
        {
            node_ = x.node_;
            return * this;
        }

        self_type & operator ++ ()
        {
            node_ = node_->next;
            return * this;
        }

        self_type & operator -- ()
        {
            node_ = node_->prev;
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


#endif // #ifndef BOOST_INTRUSIVE_LIST_HPP_INCLUDED
