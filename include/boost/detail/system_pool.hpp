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


#if 0 //defined( __GNUC__ ) && ( defined( __i386__ ) || defined( __x86_64__ ) ) && !defined(__PATHSCALE__)
# include <boost/detail/system_pool_gcc_x86.hpp>

#elif 0 //defined( WIN32 ) || defined( _WIN32 ) || defined( __WIN32__ ) || defined(__CYGWIN__)
# include <boost/detail/system_pool_w32.hpp>

#else
# include <boost/detail/system_pool_spin.hpp>

#endif

#endif  // #ifndef BOOST_DETAIL_SYSTEM_POOL_HPP_INCLUDED
