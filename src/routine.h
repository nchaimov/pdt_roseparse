/*
 *   ROUTINE Item
 *
 *  ro#[routineID]  <name_of_routine>
 *  rloc		<fileID> <line> <column>	     # location
 *  [
 *  rgroup		<groupID>			     # parent group
 *  racs		<pub|prot|priv>			     # access
 *    OR
 *  rnspace		<namespaceID>			     # parent namespace (c++)
 *    OR
 *  rroutine	<routineID>			     # parent routine (f90)
 *  racs		<pub|priv>			     # access (f90)
 *    OR
 *  racs		<pub|priv>			     # access for interfaces (f90)
 *  ]
 *  ralias		<routineID>			     # alias (via interface) (f90)
 *  rsig		<typeID>                             # signature
 *  rlink		<no|internal|C++|C|fint|f90>         # linkage
 *  rkind		<ext|stat|auto|NA|asm|tproto	     # storage class
 *  		|fext|fprog|fbldat|fintrin	     # (f90)
 *  		|fint|fstfn|fmproc|funspec	     # (f90)
 *  		|falias>			     # (f90)
 *  rimpl [...]	<routineID>			     # for "rkind falias" (f90)
 *  rstatic		<boolean>			     # is static function? (c/c++)
 *  rskind		<ctor|dtor|conv|op>		     # special kind
 *  rvirt		<no|virt|pure>                       # virtuality (c++)
 *  rcrvo		<boolean>                            # is covariant return
 *  						     # virtual override? (c++)
 *  rinline		<boolean>			     # is inline? (c/c++)
 *  rcgen		<boolean>                            # is compiler generated? (c++)
 *  rexpl		<boolean>                            # is explicit ctor? (c++)
 *  rtempl		<templateID>			     # ID if template instance; (c++)
 *  rspecl		<boolean>                            # is specialized? (c++)
 *  rarginfo	<boolean>			     # explicit interface defined? (f90)
 *  rrec		<boolean>			     # is declared recursive? (f90)
 *  riselem		<boolean>			     # is elemental? (f90)
 *  rstart		<fileID> <line> <column>	     # location of first
 *  						     # executable statement (c/f90)
 *  rcall [...]	<routineID> <no|virt> <fileID> <line> <column>
 *  						     # callees
 *  rret [...] 	<fileID> <line> <column>	     # location of return (f90)
 *  rstop [...]	<fileID> <line> <column>	     # location of stop (f90)
 *
 *  rpos		<start_of_return_type> <last_token_before_"{"> <"{"> <"}">
 *
 *  rstmt [...]   	<id> <kind> <start_loc> <end_loc> <next_st> <down> [<extra>]
 *  		where id = st#<no>
 *  		kind = <switch|case|init|return|if|empty|for|goto|continue|
 *  			break|label|block|asm|expr|assign|throw|while|do|
 *                          try|catch|decl|set_vla_size|vla_decl|vla_dealloc|
 *
 *  			# fortran only
 *
 *  			fallocate|fassign|fio|fdo|fdeallocate|freturn|fif|
 *  			fsingle_if|fgoto|fstop|flabel|fexit|fcycle|farithif|
 *                          fentry|fpause|flabelassign|fpointerassign|fselect|
 *                          fcase|fwhere|fforall|fcall
 *  			>
 *  		start_loc = <fileID> <line> <column> |  NULL 0 0
 *  		end_loc = <fileID> <line> <column> | NULL 0 0
 *  		next_st = st#<id>  | NA
 *  		down    = st#<id> | NA
 *  		extra   = <for_init | else_stmt | catch_stmt |
 *  		           target_stmt | break_stmt | goto_target | for_stmt>
 *     		for_init = st#<id> | NA  (associated with for)
 *  		else_stmt = st#<id> | NA  (associated with if, fwhere)
 *  		catch_stmt = st#<id> | NA (associated with try)
 *  		target_stmt = st#<id> | NA (associated with goto, break, continue)
 *  		break_stmt = st#<id> | NA (associated with case)
 *
 *  		# goto_targets are the flabel statement associated with a fortran goto
 *  		goto_target = st#<id> | NA (associated with fgoto)
 *
 *  		# fexit and fcycle statements <extra> stmt points to the associated loop
 *  		for_stmt = st#<id> | NA (associated with fexit, fcycle)
 *
 *
 *
 *  rbody [...]     <statement_block>
 *  		where statement_block = st#<id>       # entry point 
 *    
 */

#ifndef __ROUTINE_H__
#define __ROUTINE_H__

#include "pdtutil.h"
#include "statement.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class RoutineCall {
public:
    int id;
    SourceLocation * loc;
    SgFunctionDefinition * sgRoutine;
	bool virt;

    RoutineCall(int i = -1, SourceLocation * l = NULL, SgFunctionDefinition * d = NULL) :
        id(i), loc(l), sgRoutine(d), virt(false) {};
};

class Routine {
public:

    SgFunctionDefinition * node;
	bool fortran;

	int id;
	std::string name;
	SourceLocation * rloc;
	//int rgroup;
	//accessType access;
	int rnspace;
	int rsig;
	
	int stmtId;
	
	enum Linkage {
		NO, INTERNAL, CPP, C, FORTRAN, FINT
	};
	Linkage rlink;
	
	enum StorageClass {
		NA, EXT, STAT, AUTO, ASM, TPROTO, FINTRIN, FEXT, FPROG, FBLDAT
	};
	StorageClass rkind;
	
	bool rstatic;
	
	enum SpecialKind {
		NONE, CTOR, DTOR, CONV, OP
	};
	SpecialKind rskind;
	
	enum VirtualType {
		VIRT_NO, VIRT, PURE
	};
	VirtualType rvirt;
	
	bool rcrvo;
	bool rinline;
	bool rcgen;
	bool rexpl;

	
	int rtempl;
	bool rspecl;

	bool rarginfo;
	bool rrec;
	bool riselem;

	SourceLocation * rstart;

    std::vector<RoutineCall *> rcalls;

	SourceLocation * rpos_rtype;
	SourceLocation * rpos_endDecl;
	SourceLocation * rpos_startBlock;
	SourceLocation * rpos_endBlock;
	
	std::vector<Statement*> rstmts;
	int rbody;
	

	Routine(int i, SgFunctionDefinition * nd, std::string n) : node(nd), fortran(false), id(i), name(n), rloc(NULL), rnspace(-1), rsig(-1), 
                                                               stmtId(0), rlink(NO), rkind(NA), rstatic(false),
                                                               rskind(NONE), rvirt(VIRT_NO), rcrvo(false),
                                                               rinline(false), rcgen(false), rexpl(false), 
															   rtempl(-1), rspecl(false), rarginfo(false), rrec(false), 
															   riselem(false), rstart(NULL), rpos_rtype(NULL),
                                                               rpos_endDecl(NULL), rpos_startBlock(NULL), rpos_endBlock(NULL),
                                                               rstmts(), rbody(-1) {};
	
	friend std::ostream & operator<<(std::ostream & out, const Routine & r);
	
	const std::string routineString(void) const {
		std::stringstream s;
		s << "ro#" << id << " " << name << "\n";
		if(rloc != NULL) {
			s << "rloc " << *rloc << "\n";
		}

        if(rnspace > 0) {
            s << "rnspace na#" << rnspace << "\n";
        }

		if(rsig > 0) {
			s << "rsig ty#" << rsig << "\n";
		}
		
		s << "rlink ";
		switch(rlink) {
			case NO: s << "no"; break;
			case INTERNAL: s << "internal"; break;
			case CPP: s << "C++"; break;
			case C: s << "C"; break;
			case FORTRAN: s << "f90"; break;
			case FINT: s << "fint"; break;
		}
		s << "\n";
		
		s << "rkind ";
		switch(rkind) {
			case EXT: s << "ext"; break;
			case STAT: s << "stat"; break;
			case AUTO: s << "auto"; break;
			case NA: s << "NA"; break;
			case ASM: s << "asm"; break;
			case TPROTO: s << "tproto"; break;
			case FINTRIN: s << "fintrin"; break;
			case FEXT: s << "fext"; break;
			case FPROG: s << "fprog"; break;
			case FBLDAT: s << "fbldat"; break;
		}
		s << "\n";
		
		if(!fortran) {
			s << "rvirt ";
			switch(rvirt) {
				case VIRT_NO: 	s << "no"; break;
				case VIRT: 		s << "virt"; break;
				case PURE:		s << "pure"; break;
			}
			s << "\n";
		}
		
		if(rstatic) {
			s << "rstatic T\n";
		}
		
		if(rskind != NONE) {
			s << "rskind ";
			switch(rskind) {
				case CTOR: s << "ctor"; break;
				case DTOR: s << "dtor"; break;
				case CONV: s << "conv"; break;
				case OP: s << "op"; break;
                default: ;
			}
			s << "\n";
		}
		
		if(rcrvo) {
			s << "rcrvo T\n";
		}
		
		if(rinline) {
			s << "rinline T\n";
		}
		
		if(rcgen) {
			s << "rcgen T\n";
		}
		
		if (rexpl) {
			s << "rexpl T\n";
		}
		
		if(rtempl > 0) {
			s << "rtempl te#" << rtempl << "\n";
		}
		
		if(rspecl) {
			s << "rspecl T\n";
		}
		
		if(rarginfo) {
			s << "rarginfo T\n";
		}
		
		if(rrec) {
			s << "rrec T\n";
		}
		
		if(riselem) {
			s << "riselem T\n";
		}
		
		if(fortran) {
			s << "rstart ";
			if(rstart != NULL) {
				s << (*rstart) << "\n";
			} else {
				s << "NULL 0 0\n";
			}
		}
		
        for(std::vector<RoutineCall*>::const_iterator it = rcalls.begin(); it!=rcalls.end(); ++it) {
            RoutineCall * rcall = *it;
            SourceLocation * rcloc = rcall->loc;
            if(rcall->id < 0) {
                //std::cerr << "WARNING: rcall has invalid id" << std::endl;
            } else {
	            s << "rcall ro#" << rcall->id << " ";
				if(rcall->virt) {
					s << "virt ";
				} else {
					s << "no ";
				}
	            if(rcloc == NULL) {
	                s << "NULL 0 0";
	            } else {
	                s << *rcloc;
	            }
	            s << "\n";
			}
        }

		for(std::vector<Statement*>::const_iterator it = rstmts.begin(); it!=rstmts.end(); ++it) {
			s << *(*it);
		}
		
		if(rbody >= 0) {
			s << "rbody st#" << rbody << "\n";
		}
		
		if(rpos_rtype != NULL) {
			s << "rpos " << (*rpos_rtype);
            if(rpos_endDecl != NULL) {
			    s << " " << (*rpos_endDecl);
            } else {
                s << " NULL 0 0";
            }
            if(rpos_startBlock != NULL) {
			    s << " " << (*rpos_startBlock);
            } else {
                s << " NULL 0 0";
            }
            if(rpos_endBlock != NULL) {
			    s << " " << (*rpos_endBlock);
            } else {
                s << " NULL 0 0";
            }
			s << "\n";
		}
		
		s << "\n";
		return s.str();
	}
	
};

std::ostream & operator<<(std::ostream & out, const Routine & r) {
	out << r.routineString();
	return out;
}

#endif
