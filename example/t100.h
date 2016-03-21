/**
	@file
	T100 brain kernel.

	@note
	Copyright (c) 2008-2016 Phil Bouchard <pbouchard8@gmail.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef T100_HPP_INCLUDED
#define T100_HPP_INCLUDED


#include <set>
#include <list>
#include <string>
#include <iostream>
#include <boost/regex.hpp>
#include <boost/smart_ptr/root_ptr.hpp>
#include "indent_facet.hpp"


namespace boost
{

struct neuron_base
{
    friend std::ostream & operator << (std::ostream & out, neuron_base const & n);
    friend bool operator < (neuron_base const & n1, neuron_base const & n2);
    friend bool operator == (neuron_base const & n1, neuron_base const & n2);

    typedef boost::node_ptr<neuron_base> pointer;

    enum sense_t {sight, sound, touch, smell, taste};

    boost::node_proxy const & x_;
    boost::regex exp_;
    std::list<std::list<pointer> > sub_;

    virtual std::string parse_state(std::string const & input, int e)
    {
        static boost::regex exp[] = {boost::regex("(.*)\\[\\-(.*)\\-\\] \\{\\+(.*)\\+\\}(.*)"), boost::regex("(.*)\\{\\+(.*)\\+\\}(.*)"), boost::regex("(.*)\\[\\-(.*)\\-\\](.*)")};

        std::string res;
        boost::match_results<std::string::const_iterator> what;

        if (boost::regex_match(input, what, exp[e], boost::match_default | boost::match_partial))
        {
            if (what[0].matched)
            {
                for (unsigned i = 1; i < what.size(); ++ i)
                {
                    if (what[i].matched)
                    {
                        switch (e)
                        {
                        case 0:
                            switch (i)
                            {
                            case 1: res += what[i].str(); break;
                            case 2: 
                                res += "(.*";
                                sub_.push_front(std::list<pointer>());
                                
                                if (node_ptr<neuron_base> p = search_exact(input))
                                    sub_.front().push_front(p);
                                else
                                    sub_.front().push_front(make_node<neuron_base>(x_, x_, what[i].str())); 
                                
                                break;
                            case 3: 
                                res += ")"; 
                                
                                if (node_ptr<neuron_base> p = search_exact(input))
                                    sub_.front().push_front(p);
                                else
                                    sub_.front().push_front(make_node<neuron_base>(x_, x_, what[i].str())); 
                                
                                break;
                            case 4: res += what[i].str(); break;
                            }
                            break;
                            
                        case 1:
                        case 2:
                            switch (i)
                            {
                            case 1: res += what[i].str(); break;
                            case 2: 
                                res += "(.*)?"; 
                                sub_.push_front(std::list<pointer>());
                                
                                if (node_ptr<neuron_base> p = search_exact(input))
                                    sub_.front().push_front(p);
                                else
                                    sub_.front().push_front(make_node<neuron_base>(x_, x_, what[i].str())); 
                                
                                break;
                            case 3: res += what[i].str(); break;
                            }
                            break;
                        }
                    }
                }
                
                return parse_state(res, e);
            }
        }
        
        return input;
    }
    
    node_ptr<neuron_base> search_exact(std::string const & input)
    {
        boost::match_results<std::string::const_iterator> what;

        if (sub_.size() == 0 && exp_.str() == input)
            return make_node<neuron_base>(x_, * this);
        else if (boost::regex_match(input, what, exp_, boost::match_default | boost::match_partial))
            if (what[0].matched)
            {
                for (std::list<std::list<neuron_base::pointer> >::const_iterator i = sub_.begin(); i != sub_.end(); ++ i)
                    for (std::list<neuron_base::pointer>::const_iterator j = i->begin(); j != i->end(); ++ j)
                        if (node_ptr<neuron_base> p = (* j)->search_exact(input))
                            return p;
            }
        
        return node_ptr<neuron_base>(x_);
    }

public:
    neuron_base(boost::node_proxy const & x, std::string const & s = "") : x_(x), exp_(s) 
    {
    }

    neuron_base(neuron_base const & n) : x_(n.x_), exp_(n.exp_) , sub_(n.sub_)
    {
    }

    virtual ~neuron_base() 
    {
    };

    std::string parse(std::string const & input)
    {
        return parse_state(parse_state(parse_state(input, 0), 1), 2);
    }
    
    node_ptr<neuron_base> search(std::string const & input)
    {
        boost::match_results<std::string::const_iterator> what;

        if (sub_.size() == 0 && exp_.str().find(input) != std::string::npos)
            return make_node<neuron_base>(x_, * this);
        else if (boost::regex_match(input, what, exp_, boost::match_default | boost::match_partial))
            if (what[0].matched)
            {
                node_ptr<neuron_base> res = make_node<neuron_base>(x_, x_, exp_.str());
                res->sub_.push_front(std::list<pointer>());
                
                for (std::list<std::list<neuron_base::pointer> >::const_iterator i = sub_.begin(); i != sub_.end(); ++ i)
                    for (std::list<neuron_base::pointer>::const_iterator j = i->begin(); j != i->end(); ++ j)
                        if (node_ptr<neuron_base> p = (* j)->search(input))
                            res->sub_.front().push_front(p);
                        
                if (res->sub_.front().size() > 0)
                    return res;
            }
        
        return node_ptr<neuron_base>(x_);
    }
    
    neuron_base & sort()
    {
        sub_.sort();
        
        for (std::list<std::list<neuron_base::pointer> >::iterator i = sub_.begin(); i != sub_.end(); ++ i)
        {
            i->sort();
            
            for (std::list<neuron_base::pointer>::const_iterator j = i->begin(); j != i->end(); ++ j)
                (* j)->sort();
        }
        
        return * this;
    }

    neuron_base & unique()
    {
        sub_.unique();

        for (std::list<std::list<neuron_base::pointer> >::iterator i = sub_.begin(); i != sub_.end(); ++ i)
        {
            i->unique();
            
            for (std::list<neuron_base::pointer>::const_iterator j = i->begin(); j != i->end(); ++ j)
                (* j)->unique();
        }
        
        return * this;
    }
    
    std::string id() const
    {
        std::string res = exp_.str();
        
        for (std::list<std::list<neuron_base::pointer> >::const_iterator i = sub_.begin(); i != sub_.end(); ++ i)
            for (std::list<neuron_base::pointer>::const_iterator j = i->begin(); j != i->end(); ++ j)
                res += (* j)->id();
        
        return res;
    }
};


inline std::ostream & operator << (std::ostream & out, neuron_base const & n)
{
    std::ios_base::sync_with_stdio(false);

    out << "- " << n.exp_ << std::endl;
    
    out << indent_manip::push;
   
    for (std::list<std::list<neuron_base::pointer> >::const_iterator i = n.sub_.begin(); i != n.sub_.end(); ++ i)
    {
        for (std::list<neuron_base::pointer>::const_iterator j = i->begin(); j != i->end(); ++ j)
        {
            if (i->size() > 1)
                out << "| ";
            
            out << ** j;
        }
    }
    
    out << indent_manip::pop;
    
    return out;
}


inline bool operator < (neuron_base const & n1, neuron_base const & n2)
{
    return n1.id() < n2.id();
}


inline bool operator < (node_ptr<neuron_base> const & p1, node_ptr<neuron_base> const & p2)
{
    return * p1 < * p2;
}


inline bool operator == (neuron_base const & n1, neuron_base const & n2)
{
    return n1.id() == n2.id();
}


inline bool operator == (node_ptr<neuron_base> const & p1, node_ptr<neuron_base> const & p2)
{
    return * p1 == * p2;
}


#if 0
/**
    Core brain kernel.
    
    @note
    The brain kernel will have a thread running in the background to sort out 
    all confused neurons that didn't take any decisions yet.  This should be 
    linked with the imaginative section of the brain which guesses over and 
    over (trials & errors) when to many regex occurences are present.
*/

template <neuron_base::sense_t>
    class neuron : public neuron_base
    {
        // disable non-"node<neuron>" allocations:
        void * operator new (size_t);

    public:
        typedef boost::node<neuron> pointee;
        
        /** 
            The following should be one of these multimap indexed by:
            - regular expression string
            - weight
            - emotional state weight (normal, angry, or scared)
        */
        
        typedef std::map<std::string, pointer> map_sn_t;
        
        static map_sn_t search_;
    
        neuron(node_proxy & x, std::string const & s) : neuron_base(x, s)
        {
            /// FIXME
            //search_[s] = (pointee *) (typename pointee::roofof) static_cast<neuron *>(rootof<is_polymorphic<neuron>::value>::get(this));

            //if (p1) sub_[0].second = p1;
            //if (p2) sub_[1].second = p2;
            //if (p3) sub_[2].second = p3;
        }

        double operator () (std::string const & input)
        {
            boost::match_results<std::string::const_iterator> what;

            if (! boost::regex_match(input, what, exp_, boost::match_default | boost::match_partial))
                return 0;
            
            if (! what[0].matched)
                return 0;

            // ponderate
            double accuracy = what.size() > 1 ? 0 : 1;
            for (unsigned i = 1; i < what.size(); i ++)
            {
                if (what[i].matched)
                {
                    sub_[i - 1].first = (* sub_[i - 1].second)(what[i].str()) / (what.size() - 1);
                    accuracy += sub_[i - 1].first;
                }
            }

/*
            if (accuracy < .7)
                return accuracy;
*/

            // learn if sounds equitable, God tells you to or "energy" spent is still low
            for (unsigned i = 1; i < what.size(); i ++)
                if (what[i].matched)
                {
                    if (sub_[i - 1].first == 0)
                    {
                        typename map_sn_t::iterator j = search_.find(what[i].str());
                        
                        if (j != search_.end())
                        {
                            /**
                                What we should do here is to:
                                - cummulate suggestions
                                - calculate difference between all proposals
                                - create new regular expression when demand is too high
                            */
                            sub_[i - 1].second = j->second;
                        }
                        else
                        {
                            /**
                                Learn
                            */
                            sub_[i - 1].second->exp_ = sub_[i - 1].second->exp_.str() + "|" + input;
                        }
                    }
                }
            
            return accuracy;
        }
    };

template <neuron_base::sense_t I>
    typename neuron<I>::map_sn_t neuron<I>::search_;


typedef neuron<neuron_base::sight> neuron_sight;
typedef neuron<neuron_base::sound> neuron_sound;
typedef neuron<neuron_base::touch> neuron_touch;
typedef neuron<neuron_base::smell> neuron_smell;
typedef neuron<neuron_base::taste> neuron_taste;
#endif


} // namespace boost


#endif
