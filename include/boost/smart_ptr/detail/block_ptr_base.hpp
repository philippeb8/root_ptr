/**
    @file
    Boost block_ptr_base.hpp header file.

    @note
    Copyright (c) 2003 - 2008 Phil Bouchard <pbouchard8@gmail.com>.
    Copyright (c) 2001 - 2007 Peter Dimov

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_DETAIL_BLOCK_PTR_BASE_HPP
#define BOOST_DETAIL_BLOCK_PTR_BASE_HPP


#include <boost/smart_ptr/detail/classof.hpp>
#include <boost/smart_ptr/detail/block_base.hpp>


namespace boost
{

namespace detail
{

namespace bp
{


/**
    Smart pointer optimized for speed and memory usage.
    
    This class represents a basic smart pointer interface.
*/

template <typename T, typename UserPool = system_pool<system_pool_tag, sizeof(char)> >
    class block_ptr_common
    {
        template <typename, typename> friend class block_ptr_common;

        // Borland 5.5.1 specific workaround
        typedef block_ptr_common<T, UserPool> this_type;

    protected:
        typedef T value_type;
        //typedef block<value_type, UserPool> element_type;

        value_type * po_;

        block_ptr_common(T * p) : po_(p)
        {
        }

        block_ptr_common & operator = (T * p)
        {
            po_ = p;
            return * this;
        }
            
    public:
        block_ptr_common() : po_(0)
        {
        }

        ~block_ptr_common()
        {
            if (po_)
            {
                header()->release();
            }
        }

        template <typename V>
            block_ptr_common(block<V, UserPool> * p) : po_(p->element())
            {
            }

        template <typename V>
            block_ptr_common(block_ptr_common<V, UserPool> const & p) : po_(p.share())
            {
            }

            block_ptr_common(block_ptr_common<value_type, UserPool> const & p) : po_(p.share())
            {
            }

        template <typename V>
            block_ptr_common & operator = (block<V, UserPool> * p)
            {
                reset(p->element());
                return * this;
            }
            
        template <typename V>
            block_ptr_common & operator = (block_ptr_common<V, UserPool> const & p)
            {
                if (p.po_ != po_)
                {
                    reset(p.share());
                }
                return * this;
            }

            block_ptr_common & operator = (block_ptr_common<value_type, UserPool> const & p)
            {
                return operator = <value_type>(p);
            }

        value_type * get() const
        {
            return po_;
        }

        value_type * share() const
        {
            if (po_)
            {
                header()->add_ref_copy();
            }
            return po_;
        }

        void reset(value_type * p = 0)
        {
            if (po_)
            {
                header()->release();
            }
            po_ = p;
        }

#if ( defined(__SUNPRO_CC) && BOOST_WORKAROUND(__SUNPRO_CC, < 0x570) ) || defined(__CINT__)
        operator bool () const
        {
            return po_ != 0;
        }
#elif defined( _MANAGED )
        static void unspecified_bool( this_type*** )
        {
        }

        typedef void (*unspecified_bool_type)( this_type*** );

        operator unspecified_bool_type() const // never throws
        {
            return po_ == 0? 0: unspecified_bool;
        }
#elif \
        ( defined(__MWERKS__) && BOOST_WORKAROUND(__MWERKS__, < 0x3200) ) || \
        ( defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ < 304) ) || \
        ( defined(__SUNPRO_CC) && BOOST_WORKAROUND(__SUNPRO_CC, <= 0x590) )

        typedef value_type * (this_type::*unspecified_bool_type)() const;
        
        operator unspecified_bool_type() const // never throws
        {
            return po_ == 0? 0: &this_type::get;
        }
#else 
        typedef value_type * this_type::*unspecified_bool_type;

        operator unspecified_bool_type() const // never throws
        {
            return po_ == 0? 0: &this_type::po_;
        }
#endif

        // operator! is redundant, but some compilers need it

        bool operator! () const // never throws
        {
            return po_ == 0;
        }

        long use_count() const // never throws
        {
            return header()->use_count();
        }

    protected:
        block_base * header() const
        {
            return (block<value_type, UserPool> *) (typename block<value_type, UserPool>::classof) static_cast<value_type *>(rootof<is_polymorphic<value_type>::value>::get(po_));
        }
    };


template <typename T, typename UserPool = system_pool<system_pool_tag, sizeof(char)> >
    class block_ptr_base : public block_ptr_common<T, UserPool>
    {
        typedef block_ptr_common<T, UserPool> base;
        typedef typename base::value_type value_type;
        
    protected:
        using base::po_;

        block_ptr_base(value_type * p) : base(p)
        {
        }

        block_ptr_base & operator = (value_type * p)
        {
            return static_cast<block_ptr_base &>(base::operator = (p));
        }

    public:
        block_ptr_base() : base()
        {
        }

        template <typename V>
            block_ptr_base(block<V, UserPool> * p) : base(p)
            {
            }

        template <typename V>
            block_ptr_base(block_ptr_base<V, UserPool> const & p) : base(p)
            {
            }

            block_ptr_base(block_ptr_base<value_type, UserPool> const & p) : base(p)
            {
            }

        template <typename V>
            block_ptr_base & operator = (block<V, UserPool> * p)
            {
                return static_cast<block_ptr_base &>(base::operator = (p));
            }

        template <typename V>
            block_ptr_base & operator = (block_ptr_base<V, UserPool> const & p)
            {
                return static_cast<block_ptr_base &>(base::operator = (p));
            }

            block_ptr_base & operator = (block_ptr_base<value_type, UserPool> const & p)
            {
                return static_cast<block_ptr_base &>(base::operator = (p));
            }

        value_type & operator * () const
        {
            return * po_;
        }

        value_type * operator -> () const
        {
            return po_;
        }
    };


#if !defined(_MSC_VER)
template <typename T, size_t N, typename UserPool>
    class block_ptr_base<T [N], UserPool> : public block_ptr_common<T [N], UserPool>
    {
        typedef block_ptr_common<T [N], UserPool> base;
        typedef typename base::value_type value_type;

    protected:
        using base::po_;

        block_ptr_base(value_type * p) : base(p)
        {
        }

        block_ptr_base & operator = (value_type * p)
        {
            return static_cast<block_ptr_base &>(base::operator = (p));
        }

    public:
        block_ptr_base() : base()
        {
        }

        template <typename V>
            block_ptr_base(block<V, UserPool> * p) : base(p)
            {
            }

        template <typename V>
            block_ptr_base(block_ptr_base<V, UserPool> const & p) : base(p)
            {
            }

            block_ptr_base(block_ptr_base<value_type, UserPool> const & p) : base(p)
            {
            }

        template <typename V>
            block_ptr_base & operator = (block<V, UserPool> * p)
            {
                return static_cast<block_ptr_base &>(base::operator = (p));
            }

        template <typename V>
            block_ptr_base & operator = (block_ptr_base<V, UserPool> const & p)
            {
                return static_cast<block_ptr_base &>(base::operator = (p));
            }

            block_ptr_base & operator = (block_ptr_base<value_type, UserPool> const & p)
            {
                return static_cast<block_ptr_base &>(base::operator = (p));
            }

        T & operator [] (std::size_t n)
        {
            return * (* po_ + n);
        }

        T const & operator [] (std::size_t n) const
        {
            return * (* po_ + n);
        }
    };
#endif


template <typename UserPool>
    class block_ptr_base<void, UserPool> : public block_ptr_common<void, UserPool>
    {
        typedef block_ptr_common<void, UserPool> base;
        typedef typename base::value_type value_type;

    protected:
        using base::po_;

        block_ptr_base(value_type * p) : base(p)
        {
        }

        block_ptr_base & operator = (value_type * p)
        {
            return static_cast<block_ptr_base &>(base::operator = (p));
        }

    public:
        block_ptr_base() : base()
        {
        }

        template <typename V>
            block_ptr_base(block<V, UserPool> * p) : base(p)
            {
            }

        template <typename V>
            block_ptr_base(block_ptr_base<V, UserPool> const & p) : base(p)
            {
            }

            block_ptr_base(block_ptr_base<value_type, UserPool> const & p) : base(p)
            {
            }

        template <typename V>
            block_ptr_base & operator = (block<V, UserPool> * p)
            {
                return static_cast<block_ptr_base &>(base::operator = (p));
            }

        template <typename V>
            block_ptr_base & operator = (block_ptr_base<V, UserPool> const & p)
            {
                return static_cast<block_ptr_base &>(base::operator = (p));
            }

            block_ptr_base & operator = (block_ptr_base<value_type, UserPool> const & p)
            {
                return static_cast<block_ptr_base &>(base::operator = (p));
            }
    };


} // namespace bp

} // namespace detail

} // namespace boost


#endif