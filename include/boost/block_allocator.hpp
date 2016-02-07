/**
	@file
	Boost block_allocator.hpp header file.

	@note
	Copyright (c) 2008 Phil Bouchard <phil@fornux.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_BLOCK_ALLOCATOR_HPP_INCLUDED
#define BOOST_BLOCK_ALLOCATOR_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <boost/block_ptr.hpp>


namespace boost
{

namespace detail
{

namespace bp
{


/**
    STL compliant allocator.
    
    @note
    Default object contructor is called inside allocate() to save temporaries.
*/

template <typename T>
    class block_allocator
    {
        typedef T                       element_type;

    public:
        typedef block<T>              value_type;
        typedef size_t                  size_type;
        typedef ptrdiff_t               difference_type;
        typedef T *                     pointer;
        typedef const T *               const_pointer;
        typedef element_type &          reference;
        typedef const element_type &    const_reference;

        template <typename U>
            struct rebind
            {
                typedef block_allocator<U> other;
            };

        block_allocator() throw()                                 {}
        block_allocator(const block_allocator &) throw()        {}
        template <typename U>
            block_allocator(const block_allocator<U> &) throw() {}

        ~block_allocator() throw()                                {}
        pointer address(reference x) const                          { return & x; }
        const_pointer address(const_reference x) const              { return & x; }

        size_type max_size() const throw()
        {
            return size_t(-1) / sizeof(T);
        }

        pointer allocate(size_type s, const void * = 0)
        {
            //value_type * p = (value_type *) value_type::operator new(sizeof(value_type));
            value_type * p = new value_type();

            return p->element();
        }

        void construct(pointer p, const T & x)
        {
            //::new (p) owned_base;
            //::new (p->element()) T(x);
        }

        void destroy(pointer p)
        {
            p->reset();
        }

        void deallocate(pointer p, size_type)
        {
        }
    };

template <typename T>
    inline bool operator == (const block_allocator<T> &, const block_allocator<T> &)
    {
        return true;
    }

template <typename T>
    inline bool operator != (const block_allocator<T> &, const block_allocator<T> &)
    {
        return false;
    }


} // namespace bp

} // namespace detail

using detail::bp::block_allocator;
using detail::bp::operator ==;
using detail::bp::operator !=;

} // namespace boost

#endif  // #ifndef BOOST_SHIFTED_ALLOCATOR_HPP_INCLUDED
