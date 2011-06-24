/*
 *   MACRO Item
 *
 *  ma#[macroID] 	<name_of_macro>
 *  mloc		<fileID> <line> <column>	     # location
 *  mkind		<def|undef>			     # kind
 *  mtext		<string_giving_text_of_macro>	     # text of macro
 *  
 */

#ifndef __MACRO_H__
#define __MACRO_H__

#include <iostream>
#include <sstream>
#include <string>

#include "rose.h"
#include "pdtutil.h"

class Macro {
public:
	int id;
	SourceLocation * mloc;
	bool mkind; // false = def, true = undef
	std::string mtext;
	
	Macro(int i = -1, SourceLocation * l = NULL, bool k = false, 
		  std::string t = "") : id(i), mloc(l), mkind(k), mtext(t) {};
	// Macro(int i = -1, SourceLocation * l = NULL, bool k = false, 
	// 	  const std::string & t = "") : id(i), mloc(l), mkind(k), mtext(t) {};
	

	
	const std::string macroString(void) const {
		std::stringstream s;
		
		s << "ma#" << id << "\n";
		
		s << "mloc ";
		if(mloc != NULL) {
			s << (*mloc) << "\n";
		} else {
			s << "NULL 0 0\n";
		}
		
		if(mkind) {
			s << "mkind undef\n";
		} else {
			s << "mkind def\n";
		}
		
		s << "mtext " << mtext << "\n";
		
		s << "\n";
		return s.str();
	}
	
	friend std::ostream & operator<<(std::ostream & out, const SourceFile & loc);
};

std::ostream & operator<<(std::ostream & out, const Macro & macro) {
	out << macro.macroString();
	return out;
}


#endif
