/**
    @file
    Boost sh_utility.h header file.

    @note
    Root Pointer - Deterministic Memory Manager.

    Copyright (c) 2003-2017 Phil Bouchard <pbouchard8@gmail.com>.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef BOOST_DETAIL_ROOFOF_HPP_INCLUDED
#define BOOST_DETAIL_ROOFOF_HPP_INCLUDED


#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_volatile.hpp>
#include <boost/type_traits/is_polymorphic.hpp>
#include <boost/type_traits/type_with_alignment.hpp>


namespace Qt
{
    
using namespace boost;

namespace smart_ptr
{

namespace detail
{


/**
    Block address helper.

    Returns the absolute address of a non-polymorphic object.
    
    @note
    Expects template value given by @c is_polymorphic::value.
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
    T * classof(U T::* q, U * p)
    {
        typedef typename remove_const<typename remove_volatile<U>::type>::type unqualified_type;
    
        return static_cast<T *>
        (
            static_cast<void *>
            (
                static_cast<char *>
                (
                    static_cast<void *>
                    (
                        const_cast<unqualified_type *>(p)
                    )
                ) 
                - 
                ptrdiff_t
                (
                    static_cast<char *>
                    (
                        static_cast<void *>
                        (
                            const_cast<unqualified_type *>(& ((T *)(0)->* q))
                        )
                    ) 
                    - 
                    (char *)(0)
                )
            )
        );
    }


} // namespace detail

} // namespace smart_ptr

} // namespace Qt


#endif // #ifndef BOOST_DETAIL_ROOFOF_HPP_INCLUDED
