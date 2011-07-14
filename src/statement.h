/* 
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
 */

#ifndef __STATEMENT_H__
#define __STATEMENT_H__

#include "pdtutil.h"
#include "taurose.h"

#include <iostream>
#include <sstream>
#include <string>

class Statement {
public:

    SgStatement * node;

	int id;
    int depth;

	bool fortran;
	
	enum StatementType {
		// C, C++
		STMT_NONE, STMT_IGNORE, STMT_SWITCH, STMT_CASE, STMT_INIT, STMT_RETURN, STMT_IF, STMT_EMPTY,
		STMT_FOR, STMT_GOTO, STMT_CONTINUE, STMT_BREAK, STMT_LABEL, STMT_BLOCK,
		STMT_ASM, STMT_EXPR, STMT_ASSIGN, STMT_THROW, STMT_WHILE, STMT_DO, STMT_TRY,
		STMT_CATCH, STMT_DECL,
		
		// C99
		STMT_SET_VLA_SIZE, STMT_VLA_DECL, STMT_VLA_DEALLOC,
		
		// FORTRAN
		STMT_FALLOCATE, STMT_FIO, STMT_FDEALLOCATE, STMT_FSINGLE_IF, STMT_FSTOP,
		STMT_FARITHIF, STMT_FENTRY, STMT_FPAUSE, STMT_FLABELASSIGN, STMT_FPOINTERASSIGN,
		STMT_FWHERE, STMT_FFORALL, STMT_FCALL,

        // UPC
        STMT_UPC_FORALL, STMT_UPC_BARRIER, STMT_UPC_FENCE, STMT_UPC_NOTIFY, STMT_UPC_WAIT
	};
	
	StatementType kind;
	
	SourceLocation * start;
	SourceLocation * end;
	int next;
    int down;
    int extra;

    SgStatement * nextSgStmt;
    SgStatement * downSgStmt;
    SgStatement * extraSgStmt;
	
	Statement(int i, SgStatement * n, StatementType k = STMT_NONE) : node(n), id(i), depth(-1), fortran(false), kind(k), start(NULL), end(NULL),
                                                                     next(-1), down(-1), extra(-1), nextSgStmt(NULL), 
                                                                     downSgStmt(NULL), extraSgStmt(NULL) {};
	
	friend std::ostream & operator<<(std::ostream & out, const Statement & s);
	
	const std::string statementString(void) const {
		std::stringstream s;
        if(kind == STMT_IGNORE) {
            return s.str();
        }
        if(id < 0) {
            std::cerr << "WARNING: rstmt has invalid id" << std::endl;
        }
		s << "rstmt st#" << id << " ";
		if(fortran) {
			s << "f";
		}
		switch(kind) {
			case STMT_NONE				: s << "NA"; 								break;
			case STMT_SWITCH			: s << (fortran ? "select" : "switch");		break;
			case STMT_CASE				: s << "case"; 								break;
			case STMT_INIT				: s << "init"; 								break;
			case STMT_RETURN			: s << "return";							break;
			case STMT_IF				: s << "if"; 								break;
			case STMT_EMPTY				: s << "empty"; 							break;
			case STMT_FOR				: s << "for"; 								break;
			case STMT_GOTO				: s << "goto"; 								break;
			case STMT_CONTINUE			: s << (fortran ? "cycle" : "continue"); 	break;
			case STMT_BREAK				: s << (fortran ? "exit"  : "break"); 		break;
			case STMT_LABEL				: s << "label"; 							break;
			case STMT_BLOCK				: s << "block"; 							break;
			case STMT_ASM				: s << "asm"; 								break;
			case STMT_EXPR				: s << "expr"; 								break;
			case STMT_ASSIGN			: s << "assign"; 							break;
			case STMT_THROW				: s << "throw"; 							break;
			case STMT_WHILE				: s << "while"; 							break;
			case STMT_DO				: s << "do"; 								break;
			case STMT_TRY				: s << "try"; 								break;
			case STMT_CATCH				: s << "catch"; 							break;
			case STMT_DECL				: s << "decl"; 								break;
			case STMT_SET_VLA_SIZE		: s << "set_vla_size"; 						break;
			case STMT_VLA_DECL			: s << "vla_decl"; 							break;
			case STMT_VLA_DEALLOC		: s << "vla_dealloc"; 						break;
			case STMT_FALLOCATE 		: s << "allocate"; 							break;
			case STMT_FIO 				: s << "io"; 								break;
			case STMT_FDEALLOCATE		: s << "deallocate"; 						break;
			case STMT_FSINGLE_IF		: s << "single_if"; 						break;
			case STMT_FSTOP				: s << "stop"; 								break;
			case STMT_FARITHIF			: s << "arithif"; 							break;
			case STMT_FENTRY			: s << "entry"; 							break;
			case STMT_FPAUSE			: s << "pause"; 							break;
			case STMT_FLABELASSIGN		: s << "labelassign"; 						break;
			case STMT_FPOINTERASSIGN	: s << "pointerassign";						break;
			case STMT_FWHERE			: s << "where"; 							break;
			case STMT_FFORALL			: s << "forall"; 							break;
			case STMT_FCALL				: s << "call"; 								break;
            case STMT_UPC_FORALL        : s << "upc_forall";                        break;
            case STMT_UPC_BARRIER       : s << "upc_barrier";                       break;
            case STMT_UPC_FENCE         : s << "upc_fence";                         break;
            case STMT_UPC_NOTIFY        : s << "upc_notify";                        break;
            case STMT_UPC_WAIT          : s << "upc_wait";                          break;
            default						: std::cerr << "WARNING: Unknown statement type encountered." << std::endl;
		} 
		s << " ";
		
		if(start == NULL) {
			s << "NULL 0 0";
		} else {
			s << *start;
		}
		s << " ";
		
		if(end == NULL) {
			s << "NULL 0 0";
		} else {
			s << *end;
		}
		s << " ";
		
		if(next < 0) {
			s << "NA";
		} else {
			s << "st#" << next;
		}
		s << " ";
		
		if(down < 0) {
			s << "NA";
		} else {
			s << "st#" << down;
		}
		s << " ";
		
		if(!fortran) {
			switch(kind) {
				case STMT_FOR:
				case STMT_IF:
				case STMT_TRY:
				case STMT_GOTO:
				case STMT_BREAK:
				case STMT_CONTINUE:
				case STMT_CASE:
	            case STMT_DECL:
                case STMT_UPC_FORALL:
					if(extra < 0) {
						s << "NA";
					} else {
						s << "st#" << extra;
					}
					s << " ";
					break;
	            default:
	                ; // Do nothing
			}
		}
		
		if(fortran && kind == STMT_FWHERE) {
			if(extra < 0) {
				s << "NA";
			} else {
				s << "st#" << extra;
			}
			s << " ";
		}
		
		s << "\n";
		return s.str();
	}
	
};

std::ostream & operator<<(std::ostream & out, const Statement & s) {
	out << s.statementString();
	return out;
}

#endif

