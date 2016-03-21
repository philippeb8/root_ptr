/**
	@file
	t100_test1.cpp

	@note
	Copyright (c) 2008-2016 Phil Bouchard <pbouchard8@gmail.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
	
	@note Depends on: libpstreams-dev, wdiff, lynx and pdftotext
*/

#include <pstreams/pstream.h>
#include <set>
#include <list>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iterator>
#include <boost/regex.hpp>
#include <boost/smart_ptr/root_ptr.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "t100.h"

using namespace std;
using namespace redi;
using namespace boost;


int main(int argv, char * argc[])
{
    pstream page(string("lynx --dump http://www.unifiedfieldtheoryfinite.com/files/experiment.pdf | pdftotext - -"), pstreams::pstdout);
    
    list<string> text;

    for (string line; getline(page.out(), line, '.');)
    {
        boost::algorithm::replace_all(line, "\n", " ");
        boost::algorithm::trim_all(line);
        boost::algorithm::to_lower(line);
        boost::algorithm::replace_all(line, "'", "\\'");
        boost::algorithm::replace_all(line, "\"", "\\\"");
        boost::algorithm::replace_all(line, "(", "\\(");
        boost::algorithm::replace_all(line, ")", "\\)");
        boost::algorithm::trim_if(line, ! boost::algorithm::is_alpha());

        text.push_back(line);
    }
    
    root_ptr<neuron_base> t100;
    t100 = make_root<neuron_base>(t100, "(.*)");
    t100->sub_.push_front(std::list<neuron_base::pointer>());
    
    for (list<string>::iterator i = text.begin(); i != text.end(); ++ i)
    {
        for (list<string>::iterator j = text.begin(); j != text.end(); ++ j)
        {
            if (i == j)
                continue;
            
            pstream proc(string("bash -c \"wdiff <(echo '") + *i + string("' ) <(echo '") + *j + string("')\""), pstreams::pstdout);
            
            string output;
            for (string line; getline(proc.out(), line);)
                output += line;

            node_ptr<neuron_base> n = make_node<neuron_base>(t100, t100);
            n->exp_ = n->parse(output);
            t100->sub_.front().push_back(n);
        }
        
        cout << "\r" << distance(text.begin(), i) * 100 / distance(text.begin(), text.end()) << "%...";
        cout.flush();
    }
    cout << endl;
    
    cout << "Mind dump:" << endl;
    cout << * t100 << endl;
    
    cout << "Searching for: \"einstein\"" << endl;
    if (node_ptr<neuron_base> p = t100->search("einstein"))
        cout << p->sort().unique() << endl;
    
    cout << "Searching for: \"graviton\"" << endl;
    if (node_ptr<neuron_base> p = t100->search("graviton"))
        cout << p->sort().unique() << endl;
    
    return 0;
}
