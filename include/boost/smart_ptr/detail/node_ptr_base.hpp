/**
    @file
    Boost QNodePtrBase.hpp header file.

    @note
    Copyright (c) 2003-2008 Phil Bouchard <pbouchard8@gmail.com>.
    Copyright (c) 2001-2007 Peter Dimov

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_DETAIL_NODE_PTR_BASE_HPP
#define BOOST_DETAIL_NODE_PTR_BASE_HPP


#include <boost/smart_ptr/detail/classof.hpp>
#include <boost/smart_ptr/detail/node_base.hpp>


namespace Qt
{

using namespace boost;

namespace smart_ptr
{

namespace detail
{


/**
    Smart pointer optimized for speed and memory usage.
    
    This class represents a basic smart pointer interface.
*/

template <typename T>
    class QNodePtrCommon
    {
        template <typename> friend class QNodePtrCommon;

        // Borland 5.5.1 specific workaround
        typedef QNodePtrCommon<T> this_type;

    protected:
        typedef T value_type;

        value_type * po_;

    public:
        QNodePtrCommon() 
        : po_(0)
        {
        }

        ~QNodePtrCommon()
        {
            if (po_)
            {
                header()->release();
            }
        }

        template <typename V, typename PoolAllocator>
            QNodePtrCommon(QNode<V, PoolAllocator> * p) 
            : po_(p->element())
            {
            }

        template <typename V>
            QNodePtrCommon(QNodePtrCommon<V> const & p) 
            : po_(p.share())
            {
            }

            QNodePtrCommon(QNodePtrCommon<value_type> const & p) 
            : po_(p.share())
            {
            }

        template <typename V, typename PoolAllocator>
            QNodePtrCommon & operator = (QNode<V> * p)
            {
                reset(p->element());
                return * this;
            }
            
        template <typename V>
            QNodePtrCommon & operator = (QNodePtrCommon<V> const & p)
            {
                if (p.po_ != po_)
                {
                    reset(p.share());
                }
                return * this;
            }

            QNodePtrCommon & operator = (QNodePtrCommon<value_type> const & p)
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
        QNodeBase * header() const
        {
            return static_cast<QNodeBase *>
            (
                (typename QNodeElement<value_type>::classof)
                (
                    static_cast<value_type *>
                    (
                        rootof<is_polymorphic<value_type>::value>::get
                        (
                            po_
                        )
                    )
                )
            );
        }
    };


template <typename T>
    class QNodePtrBase : public QNodePtrCommon<T>
    {
        typedef QNodePtrCommon<T> base;
        typedef typename base::value_type value_type;
        
    protected:
        using base::po_;

    public:
        QNodePtrBase() 
        : base()
        {
        }

        template <typename V, typename PoolAllocator>
            QNodePtrBase(QNode<V, PoolAllocator> * p) 
            : base(p)
            {
            }

        template <typename V>
            QNodePtrBase(QNodePtrBase<V> const & p) 
            : base(p)
            {
            }

            QNodePtrBase(QNodePtrBase<value_type> const & p) 
            : base(p)
            {
            }

        template <typename V, typename PoolAllocator>
            QNodePtrBase & operator = (QNode<V, PoolAllocator> * p)
            {
                return static_cast<QNodePtrBase &>(base::operator = (p));
            }

        template <typename V>
            QNodePtrBase & operator = (QNodePtrBase<V> const & p)
            {
                return static_cast<QNodePtrBase &>(base::operator = (p));
            }

            QNodePtrBase & operator = (QNodePtrBase<value_type> const & p)
            {
                return static_cast<QNodePtrBase &>(base::operator = (p));
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
template <typename T, size_t N>
    class QNodePtrBase<T [N]> : public QNodePtrCommon<T [N]>
    {
        typedef QNodePtrCommon<T [N]> base;
        typedef typename base::value_type value_type;

    protected:
        using base::po_;

    public:
        QNodePtrBase() 
        : base()
        {
        }

        template <typename V, typename PoolAllocator>
            QNodePtrBase(QNode<V, PoolAllocator> * p) 
            : base(p)
            {
            }

        template <typename V>
            QNodePtrBase(QNodePtrBase<V> const & p) 
            : base(p)
            {
            }

            QNodePtrBase(QNodePtrBase<value_type> const & p) 
            : base(p)
            {
            }

        template <typename V, typename PoolAllocator>
            QNodePtrBase & operator = (QNode<V, PoolAllocator> * p)
            {
                return static_cast<QNodePtrBase &>(base::operator = (p));
            }

        template <typename V>
            QNodePtrBase & operator = (QNodePtrBase<V> const & p)
            {
                return static_cast<QNodePtrBase &>(base::operator = (p));
            }

            QNodePtrBase & operator = (QNodePtrBase<value_type> const & p)
            {
                return static_cast<QNodePtrBase &>(base::operator = (p));
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


template <>
    class QNodePtrBase<void> : public QNodePtrCommon<void>
    {
        typedef QNodePtrCommon<void> base;
        typedef typename base::value_type value_type;

    protected:
        using base::po_;

    public:
        QNodePtrBase() 
        : base()
        {
        }

        template <typename V, typename PoolAllocator>
            QNodePtrBase(QNode<V, PoolAllocator> * p) 
            : base(p)
            {
            }

        template <typename V>
            QNodePtrBase(QNodePtrBase<V> const & p) 
            : base(p)
            {
            }

            QNodePtrBase(QNodePtrBase<value_type> const & p) 
            : base(p)
            {
            }

        template <typename V, typename PoolAllocator>
            QNodePtrBase & operator = (QNode<V, PoolAllocator> * p)
            {
                return static_cast<QNodePtrBase &>(base::operator = (p));
            }

        template <typename V>
            QNodePtrBase & operator = (QNodePtrBase<V> const & p)
            {
                return static_cast<QNodePtrBase &>(base::operator = (p));
            }

            QNodePtrBase & operator = (QNodePtrBase<value_type> const & p)
            {
                return static_cast<QNodePtrBase &>(base::operator = (p));
            }
    };


} // namespace detail

} // namespace smart_ptr

} // namespace Qt


#endif
