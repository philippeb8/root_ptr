/**
    @file
    Boost QIntrusiveList.hpp header file.

    @note
    Root Pointer - Deterministic Memory Manager.

    Copyright (c) 2003-2017 Phil Bouchard <pbouchard8@gmail.com>.

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
