/**
    @file
    Boost node_ptr_base.hpp header file.

    @note
    Copyright (c) 2003-2008 Phil Bouchard <pbouchard8@gmail.com>.
    Copyright (c) 2001-2007 Peter Dimov

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
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
        node_ptr_common() 
        : po_(nullptr)
        {
        }

        ~node_ptr_common()
        {
            if (po_)
            {
                header()->release();
            }
        }

        template <typename V, typename PoolAllocator>
            node_ptr_common(node<V, PoolAllocator> * p) 
            : po_(reinterpret_cast<value_type *>(p->element()))
            {
            }

        template <typename V>
            node_ptr_common(node_ptr_common<V> const & p) 
            : po_(reinterpret_cast<value_type *>(p.share()))
            {
            }

        template <typename V>
            node_ptr_common(node_ptr_common<V> const & p, static_cast_tag const &) 
            : po_(static_cast<value_type *>(p.share()))
            {
            }

        template <typename V>
            node_ptr_common(node_ptr_common<V> const & p, dynamic_cast_tag const &) 
            : po_(dynamic_cast<value_type *>(p.share()))
            {
            }

            node_ptr_common(node_ptr_common<value_type> const & p) 
            : po_(p.share())
            {
            }

        template <typename V, typename PoolAllocator>
            node_ptr_common & operator = (node<V> * p)
            {
                reset(p->element());
                return * this;
            }
            
        template <typename V>
            node_ptr_common & operator = (node_ptr_common<V> const & p)
            {
                if (po_ != reinterpret_cast<value_type *>(p.po_))
                {
                    reset(reinterpret_cast<value_type *>(p.share()));
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
