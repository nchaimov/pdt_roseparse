/*
 * PRAGMA Item
 *
 *  pr#[pragmaID] <name_of_pragma>
 *  ploc		<fileID> <line> <column>	     # location
 *  pkind		<first-word-after-name>              # kind [optional]
 *  ppos		<start_loc> <end_loc>
 *  ptext		<string_giving_text_of_pragma>	     # text of pragma
 *	
 */

#ifndef __PRAGMA_H__
#define __PRAGMA_H__

#include <iostream>
#include <sstream>
#include <string>

#include "rose.h"
#include "pdtutil.h"

class Pragma {
public:
	int id;
	SourceLocation * ploc;
	SourceLocation * ppos_start;
	SourceLocation * ppos_end;
	std::string ptext;
	
	Pragma(int i = -1, SourceLocation * l = NULL, SourceLocation * ps = NULL, 
		  SourceLocation * pe = NULL, std::string t = "") 
			: id(i), ploc(l), ppos_start(ps), ppos_end(pe), ptext(t) {};

	const std::string pragmaString(void) const {
		std::stringstream s;
		
		s << "pr#" << id << "\n";
		
		s << "ploc ";
		if(ploc != NULL) {
			s << (*ploc) << "\n";
		} else {
			s << "NULL 0 0\n";
		}
		
		s << "ppos ";
		if(ppos_start != NULL) {
			s << (*ppos_start) << " ";
		} else {
			s << "NULL 0 0 ";
		}
		if(ppos_end != NULL) {
			s << (*ppos_end) << "\n";
		} else {
			s << "NULL 0 0\n";
		}
		
			
		
		s << "ptext " << ptext << "\n";
		
		s << "\n";
		return s.str();
	}
	
	friend std::ostream & operator<<(std::ostream & out, const SourceFile & loc);
};

std::ostream & operator<<(std::ostream & out, const Pragma & pragma) {
	out << pragma.pragmaString();
	return out;
}


#endif
