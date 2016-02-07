/**
	@file
	Boost intrusive_list.hpp header file.

	@note
	Copyright (c) 2008 Phil Bouchard <phil@fornux.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_INTRUSIVE_LIST_HPP_INCLUDED
#define BOOST_INTRUSIVE_LIST_HPP_INCLUDED


#include <boost/detail/roofof.hpp>


namespace boost
{

namespace detail
{

namespace bp
{


struct intrusive_list_node
{
	intrusive_list_node * next;
	intrusive_list_node * prev;
	
	intrusive_list_node() : next(this), prev(this)
	{
	}

	void insert(intrusive_list_node * const p)
	{
		p->next = this;
		p->prev = prev;
		
		prev->next = p;
		prev = p;
	}

    void erase()
	{
		prev->next = next;
		next->prev = prev;
		
		next = this;
		prev = this;
	}
	
	~intrusive_list_node()
	{
		erase();
	}
};


class intrusive_list_base
{
protected:
	intrusive_list_node impl;

	void clear()
	{
		impl.next = & impl;
		impl.prev = & impl;
	}
};


/**
	Static list.
	
	Rewritten list template with explicit access to internal nodes.  This 
	allows usages of tags already part of an object, used to group objects 
	together without the need of any memory allocation.
*/

class intrusive_list : protected intrusive_list_base
{
	typedef intrusive_list_base						base;

public:
	typedef intrusive_list_node						node;
	typedef intrusive_list_node *					pointer;
	template <typename T, intrusive_list_node T::* P> 
		struct iterator;

protected:
	using base::impl;

public:
	pointer begin() 								{ return impl.next; }
	pointer end() 									{ return & impl; }

	bool empty() const 								{ return impl.next == & impl; }
	
	void push_front(pointer i)
	{
		begin()->insert(i);
	}
	
	void push_back(pointer i)
	{
		end()->insert(i);
	}
	
	void merge(intrusive_list& x)
	{
		if (! x.empty())
		{
			x.impl.prev->next = impl.next;
			impl.next->prev = x.impl.prev;
			
			impl.next = x.impl.next;
			x.impl.next->prev = & impl;

			x.clear();
		}
	}
};


template <typename T, intrusive_list_node T::* P>
	struct intrusive_list::iterator
	{
		typedef iterator           						self_type;
		typedef intrusive_list_node               		node_type;

		iterator() : node_() 							{}
		iterator(intrusive_list::pointer __x) : node_(__x) {}

		T & operator * () const 						{ return * roofof(P, node_); }
		T * operator -> () const						{ return roofof(P, node_); }

		self_type & operator ++ ()
		{
			node_ = node_->next;
			return * this;
		}

		self_type & operator -- ()
		{
			node_ = node_->prev;
			return * this;
		}

		bool operator == (const self_type & x) const 	{ return node_ == x.node_; }
		bool operator != (const self_type & x) const 	{ return node_ != x.node_; }

		node_type * node_;
	};


} // namespace bp

} // namespace detail

} // namespace boost


#endif // #ifndef BOOST_INTRUSIVE_LIST_HPP_INCLUDED
