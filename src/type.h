/*
 *	TYPE Item
 *
 *   ty#[typeID]	<name_of_type>
 *   yloc		<fileID> <line> <column>	     # location
 *   [
 *   ygroup		<groupID>			     # parent group
 *   yacs		<pub|prot|priv>			     # access
 *     OR
 *   ynspace		<namespaceID>			     # parent namespace (c++)
 *     OR
 *   yacs		<pub|priv>			     # access (f90)
 *   ]
 *   ykind		<err|void|int|float|ptr|ref|func     # kind
 *   		|array|tref|ptrmem|tparam
 *   		|enum|wchar|bool
 *   		|ferr|fvoid|fint|flogic|ffloat	     # (f90)
 *   		|ffunc|fchar|farray|fcmplx	     # (f90)
 *   		|funspecfunc|fbldat|fmod|fptr	     # (f90)
 *   		|NA
 *
 *   [
 *   if (ykind == int|enum|wchar|bool|fint|flogic|fchar):
 *   yikind		<char|schar|uchar|wchar		     # integer kind
 *   		|short|ushort|int|uint
 *   		|long|ulong|longlong|ulonglong>
 *   ysigned		<boolean>			     # is explicitly signed?
 *
 *   if (ykind == enum):
 *   yenum [...]	<name1> <value1>                     # name-value pairs
 *
 *   if (ykind == fchar):				     # (f90)
 *   yclen		<number_of_characters>		     # *: unspecified
 *   						     # else constant
 *
 *   if (ykind == float|ffloat|fcmplx):
 *   yfkind		<float|dbl|longdbl>		     # float kind
 *
 *   if (ykind == ptr|fptr):
 *   yptr		<typeID|groupID>		     # type pointed to
 *
 *   if (ykind == ref):
 *   yref		<typeID|groupID>		     # type referred to
 *
 *   if (ykind == func|ffunc):
 *   yrett		<typeID|groupID>		     # return type
 *   yargt [...]	<typeID|groupID> <-|name> <src> <def|in|out|opt>*
 *   		where src = NA 0 0 | so#<id> <line> <col>
 *   		where the position represents the beginning of type 
 *   						     # argument type
 *   						     # argument name
 *   						     # has default value?
 *   						     # intent in? (f90)
 *   						     # intent out? (f90)
 *   						     # is optional? (f90)
 *   yellip          <boolean>			     # has ellipsis?
 *   yqual		<const>				     # qualifier
 *   yexcep [...]	<typeID|groupID>		     # absence of attribute
 *   						     # indicates any exception
 *   						     # may be thrown;
 *   						     # NULL indicates no excep-
 *   						     # tions will be thrown
 *
 *   if (ykind == array):
 *   yelem		<typeID|groupID>		     # type of array element
 *   ystat		<boolean>			     # (C99) is static?  i.e.,
 *   						     # is array tagged with C99
 *   						     # keyword "static"?
 *   ynelem		<number_of_elements>		     # -2: template dependent size array;
 *   						     # -1: variable length array;
 *   						     # 0: [] incomplete-type case;
 *   						     # else number of elements
 *
 *   if (ykind == farray):				     # (f90)
 *   yelem		<typeID|groupID>		     # type of array element
 *   yshape		<explicit|asmdsize|asmdshape	     # shape of array
 *   		|deferred>
 *   yrank		<number_of_dimensions>		     # number of dimensions
 *   ydim [...]	<lower_bound> <upper_bound>	     # bounds of dimensions
 *   						     # *: if upper bound for asmdsize;
 *   						     # NA: if not constant;
 *   						     # else constant value
 *
 *   if (ykind == tref):
 *   ytref		<typeID|groupID>		     # typedef type
 *   yqual		<const|volatile|restrict>*	     # qualifiers
 *
 *   if (ykind == ptrmem):
 *   ympgroup  	<groupID>			     # type of group to which
 *   						     # member pointed to belongs
 *   ymptype		<typeID|groupID>		     # type of member pointed to
 *   ]
 *
 */

#ifndef __TYPE_H__
#define __TYPE_H__

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <stdlib.h>

#include "pdtutil.h"

class Type;

class ArgumentType {
public:	
	int id;
    bool group;
	std::string name;
	SourceLocation * location;
	
	ArgumentType(int i, bool g, std::string n, SourceLocation * l = NULL) : id(i), group(g), name(n), location(l) {};
};

class EnumEntry {
public:
	std::string name;
	int value;
	
	EnumEntry(std::string n, int v) : name(n), value(v) {};
};

class Type {
public:
	int id;
	bool fortran;
	std::string name;
	SourceLocation * yloc;
	//ygroup
	//yacs
	int ynspace;
	
	enum TypeKind {
		NA, ERR, VOID, INT, FLOAT, PTR, REF, FUNC, ARRAY, TREF, PTRMEM, TPARAM,
		ENUM, WCHAR, BOOL, FUNSPECFUNC, FCMPLX, FBLDAT, FMOD, FCHAR
	};
	TypeKind ykind;
	
	// INTEGER
	enum IntKind {
		INT_NA, INT_CHAR, INT_SCHAR, INT_UCHAR, INT_WCHAR,
		INT_SHORT, INT_USHORT, INT_INT, INT_UINT,
		INT_LONG, INT_ULONG, INT_LONGLONG, INT_ULONGLONG
	};
	IntKind yikind;
	
	bool ysigned;
	
	// ENUM
	std::vector<EnumEntry*> yenums;
	int lastEnumValue;
	
	// FLOAT
	enum FloatKind {
		FLOAT_NA, FLOAT_FLOAT, FLOAT_DBL, FLOAT_LONGDBL
	};
	FloatKind yfkind;
	
	// POINTER
	int yptr;
    bool yptr_group;
	
	// REFERENCE
	int yref;
    bool yref_group;
	
	// FUNCTION
	int yrett;
    bool yrett_group;
	std::vector<ArgumentType*> yargts;
	bool yellip;
	bool yqual;
	//yexcep
	
	// ARRAY
	int yelem;
	bool yelem_group;
	bool ystat;
	ssize_t ynelem;
	
	// FORTRAN ARRAY
	enum ArrayShape {
		AS_NA, AS_EXPLICIT, AS_MDSIZE, AS_MDSHAPE, AS_DEFERRED
	};
	ArrayShape yshape;
	int yrank;
	std::string ydim;
	
	// TYPE REFERENCE
	int ytref;
	bool ytref_group;
	bool yqual_volatile;
	bool yqual_restrict;
	
	// POINTER TO MEMBER
	int ympgroup;
	int ymptype;
	bool ymptype_group;
	
	// FORTRAN CHARACTER
	int yclen;
	
    // UPC SHARED TYPES
    bool yshared;
    int yblocksize;
    bool ystrict;
    bool yrelaxed;

	friend std::ostream & operator<<(std::ostream & out, const Type & t);

	Type(int i, std::string n) : id(i), fortran(false), name(n), yloc(NULL), ynspace(-1), ykind(NA), yikind(INT_NA), ysigned(false),
								 lastEnumValue(0), yfkind(FLOAT_NA), yptr(-1), yref(-1), yrett(-1), yellip(false),
								 yqual(false), yelem(-1), yelem_group(false), ystat(false), ynelem(-3),
								 yshape(AS_NA), yrank(-1), ydim(""),
								 ytref(-1), ytref_group(false), yqual_volatile(false), yqual_restrict(false),
								 ympgroup(-1), ymptype(-1), ymptype_group(false), yclen(-1),
                                 yshared(false), yblocksize(-1), ystrict(false), yrelaxed(false) {};
	
	const std::string typeString(void) const {
		std::stringstream s;

		s << "ty#" << id << " " << name << " \n";
		
		if(ynspace > 0) {
			s << "ynspace na#" << ynspace << "\n";
		}
		
		s << "ykind ";
		if(fortran) {
			s << "f";
		}
		switch(ykind) {
			case NA: 			s << "NA"; break;
			case ERR: 			s << "err"; break;
			case VOID: 			s << "void"; break;
			case INT: 			s << "int"; break;
			case FLOAT: 		s << "float"; break;
			case PTR: 			s << "ptr"; break;
			case REF: 			s << "ref"; break;
			case FUNC: 			s << "func"; break;
			case ARRAY: 		s << "array"; break;
			case TREF: 			s << "tref"; break;
			case PTRMEM: 		s << "ptrmem"; break;
			case TPARAM: 		s << "tparam"; break;
			case ENUM: 			s << "enum"; break;
			case WCHAR: 		s << "wchar"; break;
			case BOOL: 			s << (fortran ? "logic" : "bool"); break;
			case FUNSPECFUNC: 	s << "unspecfunc"; break;
			case FCMPLX: 		s << "cmplx"; break;
			case FBLDAT: 		s << "bldat"; break;
			case FMOD: 			s << "mod"; break;
			case FCHAR: 		s << "fchar"; break;
            default:        std::cerr << "WARNING: Unknown kind encountered." << std::endl;
		}
		s << "\n";

		switch(ykind) {
			case NA:
			case ERR:
			case VOID:
			break;
			
			case INT:
			case ENUM:
			case WCHAR:
			case BOOL:
			case FCHAR: {
				if(yikind != INT_NA) {
					s << "yikind ";
					switch(yikind) {
						case INT_CHAR     : s << "char"; break;
						case INT_SCHAR    : s << "schar"; break;
						case INT_UCHAR    : s << "uchar"; break;
						case INT_WCHAR    : s << "wchar"; break;
						case INT_SHORT    : s << "short"; break;
						case INT_USHORT   : s << "ushort"; break;
						case INT_INT      : s << "int"; break;
						case INT_UINT     : s << "uint"; break;
						case INT_LONG     : s << "long"; break;
						case INT_ULONG    : s << "ulong"; break;
						case INT_LONGLONG : s << "longlong"; break;
						case INT_ULONGLONG: s << "ulonglong"; break;
                        default: std::cerr << "WARNING: Unknown PDB integer kind encountered;" << std::endl;
					}
					s << "\n";
					if(ysigned) {
						s << "ysigned T\n";
					}
				}
				if(ykind == ENUM) {
					for(std::vector<EnumEntry*>::const_iterator i = yenums.begin(); i != yenums.end(); i++) {
						s << "yenum " << (*i)->name << " " << (*i)->value << "\n";
					}
				}
				if(ykind == FCHAR) {
					if(yclen > 0) {
						s << "yclen " << yclen << "\n";
					} else {
						s << "yclen *\n";
					}
				}
			}
			break;
			
			case FLOAT: {
				if(yfkind != FLOAT_NA) {
					s << "yfkind ";
					switch(yfkind) {
						case FLOAT_FLOAT: s << "float"; break;
						case FLOAT_DBL: s << "dbl"; break;
						case FLOAT_LONGDBL: s << "longdbl"; break;
                        default: std::cerr << "WARNING: Unknown PDB float kind encountered;" << std::endl;
					}
					s << "\n";
				}
			}
			break;
			
			case PTR: {
                std::string yptr_flag = yptr_group ? "gr#" : "ty#";
				s << "yptr " << yptr_flag << yptr << "\n"; 
			}
			break;
			
			case REF: {
                std::string yref_flag = yref_group ? "gr#" : "ty#";
				s << "yref " << yref_flag << yref << "\n";
			}
			break;
			
			case FUNC: {
				if(yrett > 0) {
                    std::string yrett_flag = yrett_group ? "gr#" : "ty#";
					s << "yrett " << yrett_flag << yrett << "\n";
				}
				for(std::vector<ArgumentType*>::const_iterator it = yargts.begin(); it!=yargts.end(); ++it) {
                    std::string yargt_flag = (*it)->group ? "gr#" : "ty#";
					s << "yargt " << yargt_flag << (*it)->id << " " << (*it)->name << " " << *(*it)->location << "\n";
				}
				if(yellip) {
					s << "yellip T\n";
				}
				if(yqual) {
					s << "yqual T\n";
				}
			}
			break;
			
			case ARRAY: {
				if(yelem > 0) {
					std::string yelem_flag = yelem_group ? "gr#" : "ty#";
					s << "yelem " << yelem_flag << yelem << "\n";
				}
				if(!fortran) {
					if(ynelem > -3) {
						s << "ynelem " << ynelem << "\n";
					}
				} else {
					switch(yshape) {
						case AS_NA: break;
						case AS_EXPLICIT: 	s << "yshape explicit\n"; break;
						case AS_MDSIZE: 	s << "yshape asmdsize\n"; break;
						case AS_MDSHAPE: 	s << "yshape asmdshape\n"; break;
						case AS_DEFERRED: 	s << "yshape deferred\n"; break;
						default: std::cerr << "WARNING: Unknown array shape encountered." << std::endl;
					}
					
					if(yrank > -1) {
						s << "yrank " << yrank << "\n";
					}
					
					if(!ydim.empty()) {
						s << "ydim " << ydim << "\n";
					}
				}
			}
			break;
			
			case TREF: {
				if(ytref > 0) {
					std::string ytref_flag = ytref_group ? "gr#" : "ty#";
					s << "ytref " << ytref_flag << ytref << "\n";
					if(yqual || yqual_volatile || yqual_restrict) {
						s << "yqual";
						if(yqual) {
							s << " const";
						}
						if(yqual_volatile) {
							s << " volatile";
						}
						if(yqual_restrict) {
							s << " restrict";
						}
						s << "\n";
					}
				}
			}
			break;
			
			case PTRMEM: {
				if(ympgroup > 0) {
					s << "ympgroup gr#" << ympgroup << "\n";
				}
				
				if(ymptype > 0) {
					std::string ymptype_flag = ymptype_group ? "gr#" : "ty#";
					s << "ymptype " << ymptype_flag << ymptype << "\n";
				}
			}
			break;
			
			// TPARAM
            default: 
				std::cerr << "WARNING: Unknown PDB type kind encountered!\n";
				std::cerr << "Type: " << ykind << "\n";
				std::cerr << "ID: " << id << "\n";
				std::cerr << "Name: " << name << "\n";
				std::cerr << std::endl;
		}

        if(yshared) {
            s << "yshared T\n";
        }

        if(yblocksize >= 0) {
            s << "yblocksize " << yblocksize << "\n";
        }

        if(ystrict) {
            s << "ystrict T\n";
        }

        if(yrelaxed) {
            s << "yrelaxed T\n";
        }

		s << "\n";
		
		return s.str();
	}
};

std::ostream & operator<<(std::ostream & out, const Type & t) {
	out << t.typeString();
	return out;
}

#endif
