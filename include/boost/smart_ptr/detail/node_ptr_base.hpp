/**
    \file
    \brief Boost detail/node_ptr_base.hpp header file.

    Patent Pending
    'SOURCE TO SOURCE COMPILER, COMPILATION METHOD, AND
    COMPUTER-READABLE MEDIUM FOR PREDICTABLE MEMORY MANAGEMENT'

    Copyright (C) 2021 Fornux Inc.

    Phil Bouchard, Founder & CEO
    Fornux Inc.
    phil@fornux.com
    20 Poirier St, Gatineau, Quebec, Canada, J8V 1A6

    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt
*/


#ifndef BOOST_DETAIL_NODE_PTR_BASE_HPP
#define BOOST_DETAIL_NODE_PTR_BASE_HPP


#include <stdexcept>

#include <boost/smart_ptr/detail/classof.hpp>
#include <boost/smart_ptr/detail/node_base.hpp>


namespace boost
{

namespace smart_ptr
{

namespace detail
{


#ifndef BOOST_DISABLE_THREADS
/** Main global mutex used for thread safety */
static recursive_mutex & static_mutex()
{
    static recursive_mutex mutex_;
    
    return mutex_;
}
#endif
    
    
struct static_cast_tag {};
struct dynamic_cast_tag {};


/**
    Smart pointer optimized for speed and memory usage.
    
    This class represents a basic smart pointer interface.
*/

template <typename T>
    class node_ptr_common
    {
        template <typename> friend class node_ptr_common;

        // Borland 5.5.1 specific workaround
        typedef node_ptr_common<T> this_type;

    protected:
        typedef T value_type;

        value_type * po_;

    public:
        explicit node_ptr_common() 
        : po_(nullptr)
        {
        }

        ~node_ptr_common()
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
            if (po_)
            {
                header()->release();
            }
        }

        template <typename V, typename PoolAllocator>
            explicit node_ptr_common(node<V, PoolAllocator> * p) 
            : po_(p->element())
            {
            }

        template <typename V, size_t S, typename PoolAllocator>
            explicit node_ptr_common(node<std::array<V, S>, PoolAllocator> * p) 
            : po_(p->element()->data())
            {
            }

        template <typename V>
            explicit node_ptr_common(node_ptr_common<V> const & p) 
            : po_(p.share())
            {
            }

#if defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            node_ptr_common(node_ptr_common<V> && p)
            : po_(std::move(p.po_))
            {
            }
#endif

        template <typename V>
            explicit node_ptr_common(node_ptr_common<V> const & p, static_cast_tag const &) 
            : po_(static_cast<value_type *>(p.share()))
            {
            }

        template <typename V>
            explicit node_ptr_common(node_ptr_common<V> const & p, dynamic_cast_tag const &) 
            : po_(dynamic_cast<value_type *>(p.share()))
            {
            }

            node_ptr_common(node_ptr_common<value_type> const & p) 
            : po_(p.share())
            {
            }

        template <typename V, typename PoolAllocator>
            node_ptr_common & operator = (node<V, PoolAllocator> * p)
            {
                reset(p->element());
                
                return * this;
            }
            
        template <typename V, size_t S, typename PoolAllocator>
            node_ptr_common & operator = (node<std::array<V, S>, PoolAllocator> * p)
            {
                reset(p->element()->data());
                
                return * this;
            }
            
        template <typename V>
            node_ptr_common & operator = (node_ptr_common<V> const & p)
            {
                if (po_ != p.po_)
                {
                    reset(p.share());
                }
                return * this;
            }

            node_ptr_common & operator = (node_ptr_common<value_type> const & p)
            {
                return operator = <value_type>(p);
            }

        value_type * get() const
        {
            return po_;
        }

        value_type * share() const
        {
            if (po_)
            {
                header()->add_ref_copy();
            }
            return po_;
        }

        void reset(value_type * p)
        {
            if (po_)
            {
                header()->release();
            }
            po_ = p;
        }

        long use_count() const // never throws
        {
            return header()->use_count();
        }

    protected:
        node_base * header() const
        {
            return static_cast<node_base *>
            (
                (typename node_element<value_type>::classof)
                (
                    static_cast<value_type *>
                    (
                        rootof<is_polymorphic<value_type>::value>::get
                        (
                            po_
                        )
                    )
                )
            );
        }
    };


} // namespace detail

} // namespace smart_ptr

} // namespace boost


#endif
