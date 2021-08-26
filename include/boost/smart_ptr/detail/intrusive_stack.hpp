/**
    \file
    Boost intrusive_stack.hpp header file.

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


#ifndef BOOST_INTRUSIVE_STACK_HPP_INCLUDED
#define BOOST_INTRUSIVE_STACK_HPP_INCLUDED


#include <boost/smart_ptr/detail/classof.hpp>


namespace boost
{

namespace smart_ptr
{

namespace detail
{


struct intrusive_stack_node
{
    intrusive_stack_node * next;
    
    void insert(intrusive_stack_node * const p)
    {
        p->next = next;
        next = p;
    }
};


class intrusive_stack_base
{
protected:
    intrusive_stack_node impl;
    
    intrusive_stack_base()
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
    
    Rewritten stack template with explicit access to internal nodes.  This 
    allows usages of tags already part of an object, used to group objects 
    together without the need of any memory allocation.
*/

class intrusive_stack : protected intrusive_stack_base
{
    typedef intrusive_stack_base base;

public:
    typedef intrusive_stack_node node;
    typedef intrusive_stack_node * pointer;
    template <typename T, intrusive_stack_node T::* P> 
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


template <typename T, intrusive_stack_node T::* P>
    struct intrusive_stack::iterator
    {
        typedef iterator self_type;
        typedef intrusive_stack_node node_type;

        iterator() 
        : node_() 
        {
        }
        
        iterator(intrusive_stack::pointer __x) 
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

        node_type * node_;
    };


} // namespace detail

} // namespace smart_ptr

} // namespace boost


#endif // #ifndef BOOST_INTRUSIVE_STACK_HPP_INCLUDED
