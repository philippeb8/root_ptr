/**
	@file
	Boost sh_utility.h header file.

	@note
	Copyright (c) 2008 Phil Bouchard <phil@fornux.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_DETAIL_ROOFOF_HPP_INCLUDED
#define BOOST_DETAIL_ROOFOF_HPP_INCLUDED


#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_volatile.hpp>
#include <boost/type_traits/is_polymorphic.hpp>
#include <boost/type_traits/type_with_alignment.hpp>


namespace boost
{

namespace detail
{

namespace bp
{


/**
	Block address helper.

	Returns the absolute address of a non-polymorphic object.
	
	@note
	Expects template value given by @sa is_polymorphic<>::value.
*/

template <bool>
	struct rootof
	{
		template <typename U>
			static void * get(U * a_p)
			{
				typedef typename remove_const<typename remove_volatile<U>::type>::type unqualified_type;

				return static_cast<void *>(const_cast<unqualified_type *>(a_p));
			}
	};


/**
	Block address helper.

	Returns the absolute address of a polymorphic object.
*/

template <>
	struct rootof<true>
	{
		template <typename U>
			static void * get(U * a_p)
			{
				typedef typename remove_const<typename remove_volatile<U>::type>::type unqualified_type;

				return dynamic_cast<void *>(const_cast<unqualified_type *>(a_p));
			}
	};

	
/**
	Class member upshift.
	
	Finds the address of a class given member credentials.
*/

template <typename T, typename U>
	T * roofof(U T::* q, U * p)
	{
		typedef typename remove_const<typename remove_volatile<U>::type>::type unqualified_type;
	
		return static_cast<T *>(static_cast<void *>(static_cast<char *>(static_cast<void *>(const_cast<unqualified_type *>(p))) - ptrdiff_t(static_cast<char *>(static_cast<void *>(const_cast<unqualified_type *>(& ((T *)(0)->* q)))) - (char *)(0))));
	}


} // namespace bp

} // namespace detail

} // namespace boost


#endif // #ifndef BOOST_DETAIL_ROOFOF_HPP_INCLUDED
