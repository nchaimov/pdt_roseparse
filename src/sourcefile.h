#ifndef __SOURCEFILE_H__
#define __SOURCEFILE_H__

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>

#include "rose.h"

class SourceFile;
class SourceLocation;

std::map<int, SourceFile*> sourceFileMap;

class Comment {
public:
	int id;
	Language lang;
	std::string start;
	std::string end;
	std::string text;
	
	Comment(int i = -1, Language l = LANG_NONE, std::string s = "NULL 0 0",
			std::string e = "NULL 0 0", std::string t = "") : id(i), lang(l),
			start(s), end(e), text(t) {};
};

class SourceFile {
public:
	int fileId;
	std::string path;
	bool ssys;
	std::vector<int> sinc;
	std::vector<Comment*> scoms;
	
	int nextCommentID;
	
	SourceFile(int f, std::string & p) : fileId(f), path(p), ssys(false), nextCommentID(1) { sourceFileMap[this->fileId] = this;};
	SourceFile(int f, std::string p) : fileId(f), path(p), ssys(false), nextCommentID(1) {sourceFileMap[this->fileId] = this;};
	
	
	SourceFile(SgFile const * file) :      fileId(file->get_file_info()->get_file_id() + 1), 
										   path(file->get_sourceFileNameWithPath()),
										   ssys(false), nextCommentID(1) {
											sourceFileMap[this->fileId] = this;
										   };
	
	const std::string sourceFileString(void) const {
		std::stringstream s;
		s << "so#" << fileId << " " << path << "\n";
		if(ssys) {
			s << "ssys T\n";
		}
		for(std::vector<int>::const_iterator it = sinc.begin(); it != sinc.end(); ++it) {
			s << "sinc " << (*it) << "\n";
		}
		for(std::vector<Comment*>::const_iterator it = scoms.begin(); it != scoms.end(); ++it) {
			Comment * com = (*it);
			s << "scom co#" << com->id << " ";
			switch(com->lang) {
				case LANG_C: s << "c"; break;
				case LANG_CPP: s << "c++"; break;
	            case LANG_C_CPP: s << "c_or_c++"; break;
				case LANG_FORTRAN: s << "fortran"; break;
				case LANG_JAVA: s << "java"; break;
				case LANG_MULTI: s << "multi"; break;
				default: ;
			}
			s << " " << com->start << " " << com->end << " ";
			s << com->text;
			s << "\n";
			
		}
		
		s << "\n";
		return s.str();
	}
	
	friend std::ostream & operator<<(std::ostream & out, const SourceFile & loc);
};

std::ostream & operator<<(std::ostream & out, const SourceFile & loc) {
	out << loc.sourceFileString();
	return out;
}


#endif
