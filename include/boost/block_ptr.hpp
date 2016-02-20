/**
    @file
    Boost block_ptr.hpp header file.

    @author
    Copyright (c) 2008 Phil Bouchard <pbouchard8@gmail.com>.

    @note
    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.


    Thanks to: Steven Watanabe <watanabesj@gmail.com>
*/


#ifndef BOOST_BLOCK_PTR_INCLUDED
#define BOOST_BLOCK_PTR_INCLUDED


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4355 )

#include <new.h>
#endif

#include <iostream>
#include <boost/pool/pool_alloc.hpp>
#include <boost/type_traits/add_pointer.hpp>
#ifndef BOOST_DISABLE_THREADS
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#endif

#include <boost/detail/intrusive_list.hpp>
#include <boost/detail/intrusive_stack.hpp>
#include <boost/detail/roofof.hpp>
#include <boost/detail/block_ptr_base.hpp>
#include <boost/detail/system_pool.hpp>


namespace boost
{

namespace detail
{

namespace bp
{


struct block_base;


/**
    Set header.
    
    Proxy object used to count the number of pointers from the stack are referencing pointee objects belonging to the same @c block_proxy .
*/

struct block_proxy
{
    long count_;								/**< Count of the number of pointers from the stack referencing the same @c block_proxy .*/
    mutable block_proxy * redir_;					/**< Redirection in the case of an union multiple sets.*/

    bool destroy_;									/**< Destruction sequence initiated. */
    intrusive_list::node tag_;						/**< Tag used to enlist to @c block_proxy::includes_ . */

    intrusive_list includes_;						/**< List of all sets of an union. */
    intrusive_list elements_;						/**< List of all pointee objects belonging to a @c block_proxy . */

#ifndef BOOST_DISABLE_THREADS
    static mutex & static_mutex()					/**< Main global mutex used for thread safety */
    {
        static mutex mutex_;
        
        return mutex_;
    }
#endif

    static fast_pool_allocator<block_proxy> & static_pool() /**< Pool where all sets are allocated. */
    {
        static fast_pool_allocator<block_proxy> pool_;
        
        return pool_;
    }

    /**
        Initialization of a single @c block_proxy .
    */
    
    block_proxy() : count_(1), redir_(this), destroy_(false)
    {
		includes_.push_back(& tag_);
    }

    
    /**
        Release of a @c block_proxy with possible destruction of all its elements and other sets unified to it.
        
        @return		True if the @c block_proxy was released.
    */
    
    bool release()
    {
        if (-- count_ == 0)
        {
            destroy_ = true;
            
            for (intrusive_list::iterator<block_base, & block_base::block_tag_> i; i = elements_.begin(), i != elements_.end(); )
                delete &* i;
                
            destroy_ = false;
            
            for (intrusive_list::iterator<block_proxy, & block_proxy::tag_> i = includes_.begin(), j; j = i, i != includes_.end(); i = j)
            { 
                ++ j;
                
                if (&* i != this)
                    delete &* i;
            }

            return true;
        }

        return false;
    }

    
    /**
        Search for the @c block_proxy header of an union.
        
        @return		@c block_proxy responsible for managing the counter of an union.
    */
    
    block_proxy * redir() const
    {
        while (redir_ != redir_->redir_)
            redir_ = redir_->redir_;
        
        return redir_;
    }
    
    
    /**
        Unification with a new @c block_proxy .
        
        @param	p	New @c block_proxy to unify with.
    */

    void redir(block_proxy * p)
    {
        if (redir_ != p)
        {
            redir_ = p;
            redir_->includes_.merge(includes_);
            redir_->elements_.merge(elements_);
            redir_->count_ += count_;
        }
    }

    
    /**
        Allocates a new @c block_proxy using the fast pool allocator.
        
        @param	s	Size of the @c block_proxy .
        @return		Pointer of the new memory block.
    */
    
    void * operator new (size_t s)
    {
        return static_pool().allocate(s);
    }
    
    
    /**
        Placement new.
        
        @param	s	Size of the @c block_proxy .
        @param	p	Address to construct the @c block_proxy on.
        @return		Address to construct the @c block_proxy on.
    */
    
    void * operator new (size_t s, block_proxy * p)
    {
        return p;
    }

    
    /**
        Deallocates a @c block_proxy from the fast pool allocator.
        
        @param	p	Address of the @c block_proxy to deallocate.
    */
    
    void operator delete (void * p)
    {
        static_pool().deallocate(static_cast<block_proxy *>(p), sizeof(block_proxy));
    }
};


#define TEMPLATE_DECL(z, n, text) BOOST_PP_COMMA_IF(n) typename T ## n
#define ARGUMENT_DECL(z, n, text) BOOST_PP_COMMA_IF(n) T ## n const & t ## n
#define PARAMETER_DECL(z, n, text) BOOST_PP_COMMA_IF(n) t ## n

#define BEFRIEND_MAKE_BLOCK(z, n, text)																			    	\
    template <typename V, BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>										                    \
        friend block_ptr<V, UserPool> text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0));

#define CONSTRUCT_MAKE_BLOCK(z, n, text)																			    \
    template <typename V, BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0), typename UserPool = system_pool<system_pool_tag, sizeof(char)> >										                    \
        block_ptr<V, UserPool> text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0))															\
        {																												\
            return block_ptr<V, UserPool>(new block<V, UserPool>(BOOST_PP_REPEAT(n, PARAMETER_DECL, 0)));									\
        }


/**
    Deterministic memory manager of constant complexity.
    
    Complete memory management utility on top of standard reference counting.
*/

template <typename T, typename UserPool = system_pool<system_pool_tag, sizeof(char)> >
    class block_ptr : public block_ptr_base<T, UserPool>
    {
        template <typename, typename> friend class block_ptr;

        typedef block_ptr_base<T, UserPool> base;
        
        using base::share;
        using base::po_;

        union
        {
            block_proxy * ps_;						/**< Pointer to the @c block_proxy node @c block_ptr<> belongs to. */
            intrusive_stack::node pn_;				/**< Tag used for enlisting a pointer on the heap to later share the @c block_proxy it belongs to. */
        };
        
    public:
        /**
            Initialization of a pointer living on the stack or proper enlistment if living on the heap.
            
            @param	p	New pointee object to manage.
        */
        
        template <typename V>
            block_ptr(block<V, UserPool> * p) : base(p)
            {
				if (! pool<UserPool>::is_from(this))
                {
                    ps_ = new block_proxy();
                    init(p);
                }
                else
                {
                    pool<UserPool>::top(this)->ptrs_.push(& pn_);
                    pool<UserPool>::top(this)->inits_.merge(p->inits_);
                }
            }

        
        /**
            Assignment & union of 2 sets if the pointee resides a different @c block_proxy.
            
            @param	p	New pointee object to manage.
        */
        
        template <typename V>
            block_ptr & operator = (block<V, UserPool> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                mutex::scoped_lock scoped_lock(block_proxy::static_mutex());
#endif

                release(false);

                init(p);

                base::operator = (p);

                return * this;
            }

        template <typename V>
            void reset(block<V, UserPool> * p)
            {
                operator = <T>(p);
            }
            
        template <typename V>
            friend block_ptr<V, UserPool> make_block();

        BOOST_PP_REPEAT_FROM_TO(1, 10, BEFRIEND_MAKE_BLOCK, make_block)

    public:
        typedef T                           value_type;
        typedef block<value_type, UserPool> element_type;


        /**
            Initialization of a pointer living on the stack or proper enlistment if living on the heap.
        */
        
        block_ptr() : ps_(0)
        {
            if (!pool<UserPool>::is_from(this))
            {
                ps_ = new block_proxy();
            }
            else
            {
                pool<UserPool>::top(this)->ptrs_.push(&pn_);
            }
        }

        
        /**
            Initialization of a pointer living on the stack or proper enlistment if living on the heap.
            
            @param	p	New pointer to manage.
        */

        template <typename V>
            block_ptr(block_ptr<V, UserPool> const & p) : base(p), ps_(p.ps_->redir())
            {
#ifndef BOOST_DISABLE_THREADS
                mutex::scoped_lock scoped_lock(block_proxy::static_mutex());
#endif

				if (!pool<UserPool>::is_from(this))
                    ++ ps_->redir()->count_;
            }

        
        /**
            Initialization of a pointer living on the stack or proper enlistment if living on the heap.
            
            @param	p	New pointer to manage.
        */

            block_ptr(block_ptr<T, UserPool> const & p) : base(p), ps_(p.ps_->redir())
            {
#ifndef BOOST_DISABLE_THREADS
                mutex::scoped_lock scoped_lock(block_proxy::static_mutex());
#endif

				if (!pool<UserPool>::is_from(this))
                    ++ ps_->redir()->count_;
            }


        /**
            Assignment & union of 2 sets if the pointee resides a different @c block_proxy.
            
            @param	p	New pointer to manage.
        */
            
        template <typename V>
            block_ptr & operator = (block_ptr<V, UserPool> & p)
            {
#ifndef BOOST_DISABLE_THREADS
                mutex::scoped_lock scoped_lock(block_proxy::static_mutex());
#endif
				
				if (!pool<UserPool>::is_from(this) && pool<UserPool>::is_from(& p))
				{
					if (ps_->redir() != p.ps_->redir())
						release(false);
				}
				else if (pool<UserPool>::is_from(this) && !pool<UserPool>::is_from(& p))
				{
					if (ps_->redir() != p.ps_->redir())
						p.release(false);
				}
				else
				{
					if (ps_->redir() != p.ps_->redir())
					{
						// unify & order proxies
						if (ps_->redir() < p.ps_->redir())
							ps_->redir()->redir(p.ps_->redir());
						else
							p.ps_->redir()->redir(ps_->redir());
					}
				}

				base::operator = (p);

                return * this;
            }


        /**
            Assignment & union of 2 sets if the pointee resides a different @c block_proxy.
            
            @param	p	New pointer to manage.
        */

        block_ptr & operator = (block_ptr<T, UserPool> & p)
        {
            return operator = <T>(p);
        }

		block_ptr & operator = (int i)
		{
			reset();

			return *this;
		}

		void reset()
        {
#ifndef BOOST_DISABLE_THREADS
            mutex::scoped_lock scoped_lock(block_proxy::static_mutex());
#endif

            release(false);
        }
        
        template <typename V>
            void reset(block_ptr<V, UserPool> const & p)
            {
                operator = <T>(p);
            }
        
        bool cyclic() const
        {
            return ps_->redir()->destroy_;
        }

        ~block_ptr()
        {
            if (cyclic())
                base::po_ = 0;
            else
                release(true);
        }

    private:
        /**
            Release of the pointee object with or without destroying the entire @c block_proxy it belongs to.
            
            @param	d	Destroy (true) or reuse (false) the @c block_proxy it is releasing.
        */
        
        void release(bool d)
        {
            base::reset();
            
            if (! pool<UserPool>::is_from(this))
            {
                block_proxy * p = ps_->redir();

                if (p->release())
                {
					if (! d)
                    {
                        p->~block_proxy();
                        ps_ = new (p) block_proxy();
                    }
                    else
                        delete p;
                }
                else 
                {
                    if (! d)
                        ps_ = new block_proxy();
                }
            }
        }

        
        /**
            Enlist & initialize pointee objects belonging to the same @c block_proxy .  This initialization occurs when a pointee object is affected to the first pointer living on the stack it encounters.
            
            @param	p	Pointee object to initialize.
        */
        
        void init(block_base * p)
        {
            if (p->init_)
                return;

            block_proxy * q = ps_->redir();
        
            // iterate memory blocks
            for (intrusive_list::iterator<block_base, & block_base::init_tag_> i = p->inits_.begin(); i != p->inits_.end(); ++ i)
            {
                i->init_ = true;
                q->elements_.push_back(& i->block_tag_);

                // iterate block_ptr elements
                for (intrusive_stack::iterator<block_ptr, & block_ptr::pn_> j = i->ptrs_.begin(), k; k = j, j != i->ptrs_.end(); j = k)
                {
                    ++ k;
                    j->ps_ = ps_;
                }
            }
        }

#if 0 //defined(BOOST_HAS_RVALUE_REFS)
    public:
        block_ptr(block_ptr<T> && p): base(p.po_), ps_(p.ps_)
        {
            p.po_ = 0;
        }

        template<class Y>
            block_ptr(block_ptr<Y> && p): base(p.po_), ps_(p.ps_)
            {
                p.po_ = 0;
            }

        block_ptr<T> & operator = (block_ptr<T> && p)
        {
            std::swap(po_, p.po_);
            std::swap(ps_, p.ps_);
            
            return *this;
        }

        template<class Y>
            block_ptr & operator = (block_ptr<Y> && p)
            {
                std::swap(po_, p.po_);
                std::swap(ps_, p.ps_);
                
                return *this;
            }
#endif
    };

template <typename V, typename UserPool = system_pool<system_pool_tag, sizeof(char)> >
    block_ptr<V, UserPool> make_block()
    {
        return block_ptr<V, UserPool>(new block<V, UserPool>());
    }

template <typename T, typename UserPool>
	bool operator == (block_ptr<T, UserPool> const &a1, block_ptr<T, UserPool> const &a2)
	{
		return a1.get() == a2.get();
	}

template <typename T, typename UserPool>
	bool operator != (block_ptr<T, UserPool> const &a1, block_ptr<T, UserPool> const &a2)
	{
		return a1.get() != a2.get();
	}


BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_MAKE_BLOCK, make_block)

} // namespace bp

} // namespace detail

using detail::bp::block_ptr;
using detail::bp::block;
using detail::bp::make_block;
using detail::bp::operator ==;
using detail::bp::operator !=;

} // namespace boost


#if defined(_MSC_VER)
#pragma warning( pop )
#endif


#endif // #ifndef BOOST_BLOCK_PTR_INCLUDED
