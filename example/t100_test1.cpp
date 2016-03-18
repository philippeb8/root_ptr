/**
	@file
	t100_test1.cpp

	@note
	Copyright (c) 2008 Phil Bouchard <phil@fornux.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
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
using boost::detail::sh::neuron_sight;

void foo()
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
        boost::algorithm::trim_if(line, ! boost::algorithm::is_alpha());

        text.push_back(line);
    }
    
    set<string> mind;
    
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

            struct
            {
                string operator () (string const & input, int e)
                {
                    static boost::regex exp[] = {boost::regex("(.*)\\[\\-(.*)\\-\\] \\{\\+(.*)\\+\\}(.*)"), boost::regex("(.*)\\{\\+(.*)\\+\\}(.*)"), boost::regex("(.*)\\[\\-(.*)\\-\\](.*)")};

                    string res;
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
                                        case 2: res += "(.*)"; break;
                                        case 3: break;
                                        case 4: res += what[i].str(); break;
                                        }
                                        break;
                                        
                                    case 1:
                                    case 2:
                                        switch (i)
                                        {
                                        case 1: res += what[i].str(); break;
                                        case 2: res += "(.*)"; break;
                                        case 3: res += what[i].str(); break;
                                        }
                                        break;
                                    }
                                }
                            }
                            
                            return operator () (res, e);
                        }
                    }
                    
                    return input;
                }
            } parse;
            
            mind.insert(parse(parse(parse(output, 0), 1), 2));
        }
        cout << distance(text.begin(), i) * 100 / distance(text.begin(), text.end()) << "%..." << endl;
    }
    
    for (set<string>::iterator i = mind.begin(); i != mind.end(); ++ i)
        cout << *i << endl;
}


int main(int argv, char * argc[])
{
    foo();
    
    root_ptr<neuron_sight> t100;
    t100 = new node<neuron_sight>(t100, "I eat ([a-z]+) then drink ([a-z]+)");
    t100->sub_[0].second = new node<neuron_sight>(t100, "beef|chicken");
    t100->sub_[1].second = new node<neuron_sight>(t100, "vodka|water");

    cout << (* t100)("I eat beef then drink vodka") << endl;
    cout << (* t100)("I eat beef then drink wine") << endl;
    cout << (* t100)("I eat fish then drink wine") << endl;
    cout << (* t100)("I eat fish then drink beer") << endl;
}
