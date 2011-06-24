#ifndef __PDTUTIL_H__
#define __PDTUTIL_H__

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>


#include "rose.h"

#include "sourcefile.h"

class SourceLocation;

std::vector<SourceFile*> files;
std::map<int, SourceLocation*> foundLocations;

class SourceLocation {
public:
	int fileId;
	int line;
	int column;
	bool cgen;
		
	SourceLocation(int f, int l, int c) : fileId(f), line(l), column(c), cgen(false) {};
	
	SourceLocation(Sg_File_Info const * file) : fileId(file->get_file_id() + 1), 
												line(file->get_raw_line()), 
												column(file->get_raw_col()),
												cgen(file->isCompilerGenerated()) {
													if(foundLocations.count(fileId) == 0) {
														// First time we've seen this file.
														foundLocations[fileId] = this;
														files.push_back(new SourceFile(fileId, file->get_raw_filename()));
													}
												};
	
	const std::string locationString(void) const {
		std::stringstream s;
		if(this == NULL || cgen) {
			s << "NULL 0 0";
		} else {
			s << "so#" << fileId << " " << line << " " << column;
		}
		return s.str();
	}
	
	friend std::ostream & operator<<(std::ostream & out, const SourceLocation & loc);
};

std::ostream & operator<<(std::ostream & out, const SourceLocation & loc) {
	out << loc.locationString();
	return out;
}

#endif
