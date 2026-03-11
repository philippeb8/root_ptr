/**
    \file
    Boost intrusive_stack.hpp header file.

    Patent US11288049B2
    'SOURCE TO SOURCE COMPILER, COMPILATION METHOD, AND
    COMPUTER-READABLE MEDIUM FOR PREDICTABLE MEMORY MANAGEMENT'
    
    Copyright (C) 2020-2026 Fornux LLC

    Phil Bouchard, Founder & CEO
    Fornux LLC
    phil@fornux.com
    3909 S Maryland Pkwy Ste 314 #638, Las Vegas, NV, 89119
    
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
    
    intrusive_stack_node() : next(this)
    {
    }

    intrusive_stack_node(intrusive_stack_node * const p) : intrusive_stack_node()
    {
        insert(p);
    }

    void insert(intrusive_stack_node * const p)
    {
        p->next = next;
        next = p;
    }

    bool singleton() const
    {
        return next == this;
    }

    void erase()
    {
        next = this;
    }

    ~intrusive_stack_node()
    {
        erase();
    }
};


/**
    Static stack.
    
    Rewritten stack template with explicit access to internal nodes.  This 
    allows usages of tags already part of an object, used to group objects 
    together without the need of any memory allocation.
*/

struct intrusive_stack : intrusive_stack_node
{
    typedef intrusive_stack_node base;

    typedef intrusive_stack node;
    typedef intrusive_stack * pointer;
    template <typename T, intrusive_stack T::* P> 
        struct iterator;
        
    using base::base;
    

    intrusive_stack(intrusive_stack const &) = delete;
    
    pointer begin()
    { 
        return static_cast<intrusive_stack *>(next); 
    }
    
    pointer end()
    { 
        return this; 
    }

    bool empty() const
    { 
        return next == this; 
    }
    
    void push(pointer i)
    {
        end()->insert(i);
    }
};


template <typename T, intrusive_stack T::* P>
    struct intrusive_stack::iterator
    {
        typedef iterator self_type;
        typedef intrusive_stack node_type;

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
            node_ = static_cast<intrusive_stack::pointer>(node_->next);
            
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
