/**
    @file
    Boost detail/system_pool_gcc_x86.hpp header file.

    @note
    Copyright (c) 2016 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_DETAIL_SYSTEM_POOL_GCC_X86_HPP_INCLUDED
#define BOOST_DETAIL_SYSTEM_POOL_GCC_X86_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <list>
#include <stack>
#include <limits>

#include <boost/pool/pool.hpp>


namespace boost
{

namespace detail
{

namespace bp
{


template <typename Tag, unsigned RequestedSize, typename UserAllocator = default_user_allocator_new_delete>
    struct system_pool
    {
        typedef Tag tag;
        typedef UserAllocator user_allocator;
        typedef typename pool<UserAllocator>::size_type size_type;
        typedef typename pool<UserAllocator>::difference_type difference_type;

        static void * malloc BOOST_PREVENT_MACRO_SUBSTITUTION()
        {
            return (UserAllocator::malloc)();
        }
        static void * ordered_malloc()
        {
            return (UserAllocator::malloc)();
        }
        static void * ordered_malloc(const size_type n)
        {
            return (UserAllocator::malloc)(n);
        }
        static bool is_from(void * p)
        {
            int x;

            asm("movq %1, %%rax;"
                "cmpq %%rsp, %%rax;"
                "jbe 1f;"
                "movl $1,%0;"
                "jmp 2f;"
                "1:"
                "movl $0,%0;"
                "2:"
                : "=r" (x)
                : "r" (p)
                );

            return ! x;
        }
        static void free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr)
        {
            (UserAllocator::free)(static_cast<char *>(ptr));
        }
        static void ordered_free(void * const ptr)
        {
            (UserAllocator::free)(static_cast<char *>(ptr));
        }
        static void free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr, const size_type n)
        {
            (UserAllocator::free)(static_cast<char *>(ptr));
        }
        static void ordered_free(void * const ptr, const size_type n)
        {
            (UserAllocator::free)(static_cast<char *>(ptr));
        }
        static bool release_memory()
        {
        }
        static bool purge_memory()
        {
        }
    };


} // namespace bp

} // namespace detail

} // namespace boost


#endif  // #ifndef BOOST_DETAIL_SYSTEM_POOL_GCC_X86_HPP_INCLUDED
