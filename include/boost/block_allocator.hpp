/**
	@file
	Boost block_allocator.hpp header file.

	@note
	Copyright (c) 2008 Phil Bouchard <pbouchard8@gmail.com>.

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
#include <boost/detail/block_base.hpp>


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

template <typename T, typename UserPool = system_pool<system_pool_tag, sizeof(char)> >
    class block_allocator
    {
        typedef T                       element_type;

    public:
        typedef element_type            value_type;
        typedef size_t                  size_type;
        typedef ptrdiff_t               difference_type;
        typedef element_type *          pointer;
        typedef const element_type *    const_pointer;
        typedef element_type &          reference;
        typedef const element_type &    const_reference;

        template <typename U>
            struct rebind
            {
                typedef block_allocator<U, UserPool> other;
            };

        block_allocator() throw()                                 {}
        block_allocator(const block_allocator &) throw()        {}
        template <typename U>
            block_allocator(const block_allocator<U, UserPool> &) throw() {}

        ~block_allocator() throw()                                {}
        pointer address(reference x) const                          { return & x; }
        const_pointer address(const_reference x) const              { return & x; }

        size_type max_size() const throw()
        {
            return size_t(-1) / sizeof(T);
        }

        pointer allocate(size_type s, const void * = 0)
        {
            //block<value_type, UserPool> * p = (block<value_type, UserPool> *) block<value_type, UserPool>::operator new(sizeof(block<value_type, UserPool>));
            block<value_type, UserPool> * p = new block<value_type, UserPool>();

            return p->element();
        }

        void construct(pointer p, const T & x)
        {
            //::new (static_cast<block<value_type, UserPool> *>(typename block<value_type, UserPool>::roofof(p))) block_base;
            //::new (p) T(x);
        }

        void destroy(pointer p)
        {
            //static_cast<block<value_type, UserPool> *>(typename block<value_type, UserPool>::roofof(p))->~block_base();
            ////p->~T();
        }

        void deallocate(pointer p, size_type)
        {
            //operator delete (static_cast<block<value_type, UserPool> *>(typename block<value_type, UserPool>::roofof(p)));
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
