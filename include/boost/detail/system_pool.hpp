/**
    @file
    Boost detail/system_pool.hpp header file.

    @note
    Copyright (c) 2016 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_DETAIL_SYSTEM_POOL_HPP_INCLUDED
#define BOOST_DETAIL_SYSTEM_POOL_HPP_INCLUDED


#if defined( BOOST_DISABLE_THREADS ) && !defined( BOOST_SP_ENABLE_THREADS ) && !defined( BOOST_DISABLE_WIN32 )
# include <boost/detail/system_pool_nt.hpp>

#elif defined( __GNUC__ ) && ( defined( __i386__ ) || defined( __x86_64__ ) ) && !defined(__PATHSCALE__)
# include <boost/detail/system_pool_gcc_x86.hpp>

#else
# include <boost/detail/system_pool_spin.hpp>

#endif

#endif  // #ifndef BOOST_DETAIL_SYSTEM_POOL_HPP_INCLUDED
