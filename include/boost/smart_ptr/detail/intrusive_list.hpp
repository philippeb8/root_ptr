/**
    \file
    Boost intrusive_list.hpp header file.

    \note
    Copyright (C) 2021 Fornux Inc.
    
    Phil Bouchard, Founder & CTO
    Fornux Inc.
    phil@fornux.com
    20 Poirier St, Gatineau, Quebec, Canada, J8V 1A6
    
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/


#ifndef BOOST_INTRUSIVE_LIST_HPP_INCLUDED
#define BOOST_INTRUSIVE_LIST_HPP_INCLUDED


#include <boost/smart_ptr/detail/classof.hpp>


namespace boost
{

namespace smart_ptr
{

namespace detail
{


struct intrusive_list_node
{
    intrusive_list_node * next;
    intrusive_list_node * prev;
    
    intrusive_list_node() : next(this), prev(this)
    {
    }

    void insert(intrusive_list_node * const p)
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
    
    ~intrusive_list_node()
    {
        erase();
    }
};


class intrusive_list_base
{
protected:
    intrusive_list_node impl;

    void clear()
    {
        impl.next = & impl;
        impl.prev = & impl;
    }
};


/**
    Static list.
    
    Rewritten list template with explicit access to internal nodes.  This 
    allows usages of tags already part of an object, used to group objects 
    together without the need of any memory allocation.
*/

class intrusive_list : protected intrusive_list_base
{
    typedef intrusive_list_base base;

public:
    typedef intrusive_list_node node;
    typedef intrusive_list_node * pointer;
    template <typename T, intrusive_list_node T::* P> 
        struct iterator;

protected:
    using base::impl;

public:
    intrusive_list()                                
    {
    }
    
    intrusive_list(intrusive_list & x)
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
    
    void merge(intrusive_list& x)
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


template <typename T, intrusive_list_node T::* P>
    struct intrusive_list::iterator
    {
        typedef iterator self_type;
        typedef intrusive_list_node node_type;

        iterator(intrusive_list::pointer __x) 
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

        node_type * node_;
    };


} // namespace detail

} // namespace smart_ptr

} // namespace boost


#endif // #ifndef BOOST_INTRUSIVE_LIST_HPP_INCLUDED
