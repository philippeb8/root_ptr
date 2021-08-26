/**
    \file
    Boost classof.hpp header file.

    \note
    Copyright (C) 2021 Fornux Inc.
    
    Phil Bouchard, Founder & CTO
    Fornux Inc.
    phil@fornux.com
    20 Poirier St, Gatineau, Quebec, Canada, J8V 1A6
    
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/


#ifndef BOOST_DETAIL_ROOFOF_HPP_INCLUDED
#define BOOST_DETAIL_ROOFOF_HPP_INCLUDED


#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_volatile.hpp>
#include <boost/type_traits/is_polymorphic.hpp>
#include <boost/type_traits/type_with_alignment.hpp>


namespace boost
{

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

} // namespace boost


#endif // #ifndef BOOST_DETAIL_ROOFOF_HPP_INCLUDED
