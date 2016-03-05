/**
    @file
    Boost detail/system_pool_w32.hpp header file.

    @note
    Copyright (c) 2016 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_DETAIL_SYSTEM_POOL_W32_HPP_INCLUDED
#define BOOST_DETAIL_SYSTEM_POOL_W32_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <list>
#include <stack>
#include <limits>

#include <boost/pool/pool.hpp>

#define _X86_
#include <processthreadsapi.h>


namespace boost
{

namespace detail
{

namespace bp
{


struct system_pool_tag { };
    
    
template <typename Tag, unsigned RequestedSize, typename UserAllocator = default_user_allocator_new_delete>
    struct system_pool
    {
        typedef Tag tag;
        typedef UserAllocator user_allocator;
        typedef typename boost::pool<UserAllocator>::size_type size_type;
        typedef typename boost::pool<UserAllocator>::difference_type difference_type;

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
            ULONG_PTR range[2];

            GetCurrentThreadStackLimits(&range[0], &range[1]);

            return PtrToUlong(p) < range[0] || PtrToUlong(p) > range[1];
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


#endif  // #ifndef BOOST_DETAIL_SYSTEM_POOL_W32_HPP_INCLUDED
