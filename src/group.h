/*
 *  GROUP Item
 *
 *  gr#[groupID] 	<name_of_group>
 *  gloc		<fileID> <line> <column>	     # location
 *  [
 *  ggroup		<groupID>			     # parent group
 *  gacs		<pub|prot|priv>			     # access
 *    OR
 *  gnspace		<namespaceID>			     # parent namespace (c++)
 *  ]
 *  gkind		<class|struct|union|tproto	     # kind
 *  		|fderived|fmodule>		     # (f90)
 *  gtempl		<templateID>			     # template ID (c++)
 *  gspecl		<boolean>                            # is specialized? (c++)
 *  gsparam [...]	<type> <typeID|groupID>		     # specialization (c++)
 *  		OR <ntype> <constant>		     # template arguments
 *  		OR <templ> <templateID>
 *  gbase [...]	<virt|no> <NA|pub|prot|priv> <direct base groupID>
 *  		  <fileID> <line> <column>
 *  						     # direct base groups (c++)
 *  gfrgroup [...]  <groupID> <fileID> <line> <column>   # friend groups (c++)
 *  gfrfunc [...]	<routineID> <fileID> <line> <column> # friend functions (c++)
 *  gfunc [...]	<member_routineID> <fileID> <line> <column>
 *  						     # member functions
 *  gmem [...]	<name_of_non-function_member>	     # other members
 *    gmloc		<fileID> <line> <column>	     # location
 *    gmgroup	<groupID>			     # parent group (f90) 
 *    gmacs		<pub|prot|priv>			     # access
 *    gmkind	<type|statvar|var|templ>	     # kind:  type or group,
 *  						     # static or non-static
 *  						     # variable, template
 *    [
 *    if (gmkind == type):
 *    gmtype	<typeID|groupID>		     # type of member
 *
 *    if (gmkind == statvar):
 *    gmtype	<typeID|groupID>		     # type of member
 *    gmtempl	<templateID>			     # ID if template static (c++)
 *  						     # data member
 *    gmspecl	<boolean>			     # is specialized? (c++)
 *    gmconst	<boolean>			     # was initializer
 *  						     # specified at its
 *  						     # declaration within the
 *  						     # group definition? (c++)
 *
 *    if (gmkind == var):
 *    gmtype	<typeID|groupID>		     # type of member
 *    gmisbit	<boolean>                            # is bit field?
 *    gmmut		<boolean>			     # is mutable? (c++)
 *    gmconst	<boolean>			     # is constant? (f90)
 *
 *    if (gmkind == templ):
 *    gmtempl	<templateID>			     # template ID (c++)
 *    ]
 *  gpos		<group_token> <last_token_before_"{"> <"{"> <"};">
 *   
 */

#ifndef __GROUP_H__
#define __GROUP_H__

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <stdlib.h>

#include "pdtutil.h"

class Group;

class BaseGroup {
    public:
        bool virt;
        bool pub;
        bool prot;
        bool priv;
        int id;
        SgClassDeclaration * sgClass;
        SourceLocation * loc;
        std::string name;

        BaseGroup(int i = -1, SourceLocation * l = NULL, bool vrt = false, bool b = false,
                  bool t = false, bool v = false, std::string n = "") : virt(vrt), pub(b), prot(t), priv(v),
                                                                        id(i), sgClass(NULL), loc(l),
                                                                        name(n) {};

};

class MemberFunction {
    public:
        int id;
        SgFunctionDeclaration * sgFunction;
        std::string name;
        SourceLocation * loc;

        MemberFunction(int i = -1, SourceLocation * l = NULL, std::string n = "") : id(i), sgFunction(NULL),
                                                                                      name(n), loc(l) {};
};

class Member {
    public:
        std::string name;
        SourceLocation * gmloc;
        
        enum MemberAccess {
            GMACS_NA, GMACS_PUB, GMACS_PROT, GMACS_PRIV
        };
        MemberAccess gmacs;

        enum MemberKind {
            GMKIND_NA, GMKIND_TYPE, GMKIND_STATVAR, GMKIND_VAR, GMKIND_TEMPL
        };
        MemberKind gmkind;

        bool gmtype_group;
        int gmtype;

        //gmspecl

        bool gmconst;

        bool gmisbit;
        bool gmmut;

        int gmtempl;

        Member(std::string n, SourceLocation * loc) : name(n), gmloc(loc), gmacs(GMACS_NA),
                                                      gmkind(GMKIND_NA), gmtype_group(false), 
                                                      gmtype(-1), gmconst(false), gmisbit(false),
                                                      gmmut(false), gmtempl(-1) {};
};

class Group {
public:
	int id;
	std::string name;
	SourceLocation * gloc;

    int ggroup; // parent group
    int gnspace;

    enum GroupAccess {
        ACS_NA, ACS_PUB, ACS_PROT, ACS_PRIV
    };
    GroupAccess gacs;
    
    
    enum GroupKind {
        GKIND_NA, GKIND_CLASS, GKIND_STRUCT, GKIND_UNION, GKIND_TPROTO, GKIND_FDERIVED, GKIND_FMODULE
    };
    GroupKind gkind;


    int gtempl;
    //bool gspecl;
    //gsparam

    std::vector<BaseGroup *> gbases;
    std::vector<BaseGroup *> gfrgroups;
    std::vector<MemberFunction *> gfrfuncs;
    std::vector<MemberFunction *> gfuncs;
    std::vector<Member *> gmems;

    SourceLocation * gpos_groupToken;
    SourceLocation * gpos_tokenEnd;
    SourceLocation * gpos_blockStart;
    SourceLocation * gpos_blockEnd;
    
    Group(int i, std::string n, SourceLocation * l = NULL) : id(i), name(n), gloc(l), ggroup(-1), gnspace(-1), 
                                                             gacs(ACS_NA), gkind(GKIND_NA), gtempl(-1), gpos_groupToken(NULL),
                                                             gpos_tokenEnd(NULL), gpos_blockStart(NULL),
                                                             gpos_blockEnd(NULL) {};

    friend std::ostream & operator<<(std::ostream & out, const Group & r);
            
    const std::string groupString(void) const {
        std::stringstream s;

        s << "gr#" << id << " " << name << "\n";

        if(gloc != NULL) {
            s << "gloc " << (*gloc) << "\n";
        }    

        if(ggroup > 0 && gnspace <= 0) {
            s << "ggroup gr#" << ggroup << "\n";
        }

        if(gnspace > 0) {
            s << "gnspace na#" << gnspace << "\n";
        }
        switch(gacs) {
            case ACS_NA:    break;
            case ACS_PUB:   s << "gacs pub\n";  break;
            case ACS_PROT:  s << "gacs prot\n"; break;
            case ACS_PRIV:  s << "gacs priv\n"; break;
            default:        std::cerr << "WARNING: Unknown group access type encountered!" << std::endl;
        }
        switch(gkind) {
            case GKIND_NA:       	break;
            case GKIND_CLASS:    	s << "gkind class\n";  	 break;
            case GKIND_STRUCT:   	s << "gkind struct\n"; 	 break;
            case GKIND_UNION:    	s << "gkind union\n";  	 break;
            case GKIND_TPROTO:   	s << "gkind tproto\n"; 	 break;
			case GKIND_FDERIVED: 	s << "gkind fderived\n"; break;
            case GKIND_FMODULE:     s << "gkind fmodule\n";  break;
            default:           		std::cerr << "WARNING: Unknown group kind encountered!" << std::endl;
        }

		if(gtempl > 0) {
			s << "gtempl te#" << gtempl << "\n";
		}

        for(std::vector<BaseGroup *>::const_iterator it = gbases.begin(); it != gbases.end(); ++it) {
           BaseGroup * base = *it;
           s << "gbase ";
           if(base->virt) {
               s << "virt ";
           } else {
               s << "no ";
           }

           if(base->pub) {
               s << "pub ";
           } else if(base->prot) {
               s << "prot ";
           } else if(base->priv) {
               s << "priv ";
           } else {
               s << "NA ";
           }

           s << "gr#" << base->id << " ";

           if(base->loc != NULL) {
               s << *(base->loc);
           } else {
               s << "NULL 0 0";
           }

           s << "\n";
        }

        for(std::vector<BaseGroup *>::const_iterator it = gfrgroups.begin(); it != gfrgroups.end(); ++it) {
            BaseGroup * base = *it;
            s << "gfrgroup gr#" << base->id << " ";
            if(base->loc != NULL) {
                s << *(base->loc);
            } else {
                s << "NULL 0 0";
            }
            s << "\n";
        }
        
        for(std::vector<MemberFunction *>::const_iterator it = gfrfuncs.begin(); it != gfrfuncs.end(); ++it) {
            MemberFunction * func = *it;
            s << "gfrfunc ro#" << func->id << " ";
            if(func->loc != NULL) {
                s << *(func->loc);
            } else {
                s << "NULL 0 0";
            }
            s << "\n";
        }

        for(std::vector<MemberFunction *>::const_iterator it = gfuncs.begin(); it != gfuncs.end(); ++it) {
            MemberFunction * func = *it;
            s << "gfunc ro#" << func->id << " ";
            if(func->loc != NULL) {
                s << *(func->loc);
            } else {
                s << "NULL 0 0";
            }
            s << "\n";
        }

        for(std::vector<Member *>::const_iterator it = gmems.begin(); it != gmems.end(); ++it) {
            Member * m = *it;
            s << "gmem " << m->name << std::endl;
            if(m->gmloc != NULL) {
                s << "gmloc " << *(m->gmloc) << "\n";
            } else {
                s << "NULL 0 0\n";
            }
            switch(m->gmacs) {
                case Member::GMACS_NA:      break;
                case Member::GMACS_PUB:     s << "gmacs pub\n";     break;
                case Member::GMACS_PROT:    s << "gmacs prot\n";    break;
                case Member::GMACS_PRIV:    s << "gmacs priv\n";    break;
                default:            std::cerr << "WARNING: Unknown group member access qualifier encountered." 
                                              << std::endl;
            }
            switch(m->gmkind) {
                case Member::GMKIND_NA:         break;
                case Member::GMKIND_TYPE:       s << "gmkind type\n";       break;
                case Member::GMKIND_STATVAR:    s << "gmkind statvar\n";    break;
                case Member::GMKIND_VAR:        s << "gmkind var\n";        break;
                case Member::GMKIND_TEMPL:      s << "gmkind templ\n";      break;
                default:                std::cerr << "WARNING: Unknown group member kind encountered."
                                                  << std::endl;
            }

            if(m->gmtype > 0) {
                s << "gmtype ";
                if(m->gmtype_group) {
                    s << "gr#";
                } else {
                    s << "ty#";
                }
                s << m->gmtype << "\n";
            }

            if(m->gmtempl > 0) {
                s << "gmtempl te#" << m->gmtempl << "\n";
            }

            if(m->gmisbit) {
                s << "gmisbit T\n";
            }
            if(m->gmmut) {
                s << "gmmut T\n";
            }
        }
        s << "gpos ";
          
        if(gpos_groupToken != NULL) {
            s << *(gpos_groupToken) << " ";
        } else {
            s << "NULL 0 0";
        }

        if(gpos_tokenEnd != NULL) {
            s << *(gpos_tokenEnd) << " ";
        } else {
            s << " NULL 0 0";
        }
        
        if(gpos_blockStart != NULL) {
            s << *(gpos_blockStart) << " ";
        } else {
            s << " NULL 0 0";
        }

        if(gpos_blockEnd != NULL) {
            s << *(gpos_blockEnd);
        } else {
            s << " NULL 0 0";
        }
        s << "\n";

        s << std::endl;

        return s.str();
    };


};

std::ostream & operator<<(std::ostream & out, const Group & r) {
    out << r.groupString();
    return out;
}

#endif
