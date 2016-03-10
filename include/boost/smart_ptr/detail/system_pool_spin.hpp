/**
    @file
    Boost detail/system_pool_spin.hpp header file.

    @note
    Copyright (c) 2016 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_DETAIL_SYSTEM_POOL_SPIN_HPP_INCLUDED
#define BOOST_DETAIL_SYSTEM_POOL_SPIN_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <boost/pool/pool.hpp>


namespace boost
{

namespace smart_ptr
{

namespace detail
{


struct system_pool_tag { };
    
    
template <typename Tag, unsigned RequestedSize, typename UserAllocator = default_user_allocator_new_delete>
    struct system_pool : public boost::singleton_pool<Tag, RequestedSize, UserAllocator>
    {
    };


} // namespace detail

} // namespace smart_ptr

} // namespace boost


#endif  // #ifndef BOOST_DETAIL_SYSTEM_POOL_GCC_X86_HPP_INCLUDED
