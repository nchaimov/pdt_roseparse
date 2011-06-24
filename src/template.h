#ifndef __TEMPLATE_H__
#define __TEMPLATE_H__

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <stdlib.h>

#include "rose.h"
#include "pdtutil.h"

class TemplateParameter {
public:
	enum TemplateParameterKind {
		TPARAM_NA, TPARAM_TYPE, TPARAM_NTYPE, TPARAM_TEMPL
	};
	
	TemplateParameterKind tparam_kind;
	int id;
	bool id_group;
	std::string name;

	std::string defaultValue;
	int defaultId;
	bool defaultId_group;
	
	TemplateParameter(int i = -1, bool ig = false, TemplateParameterKind k = TPARAM_NA, std::string n = "-", 
					  int d = -1, bool dg = false, std::string dv = "") : tparam_kind(k),
					  id(i), id_group(ig), name(n), defaultValue(dv), defaultId(d), defaultId_group(dg) {};
};

class TemplateSpecializationParameter {
public:
	enum TemplateSpecializationParameterKind {
		TSPARAM_NA, TSPARAM_TYPE, TSPARAM_NTYPE, TSPARAM_TEMPL
	};
	
	TemplateSpecializationParameterKind tsparam_kind;
	int id;
	bool id_group;
	std::string constant;
	
	TemplateSpecializationParameter(int i, TemplateSpecializationParameterKind k,
									bool g, std::string c) : tsparam_kind(k), id(i),
									id_group(g), constant(c) {};
};

class Template {
	
public:
	int id;
	std::string name;
	
	SgTemplateDeclaration * sgTemplateDeclaration;
	
	SourceLocation * tloc;
	
	int tgroup;
	enum TemplateGroupAccess {
        T_ACS_NA, T_ACS_PUB, T_ACS_PROT, T_ACS_PRIV
    };
    TemplateGroupAccess tacs;
	
	int tnspace;
	
	int tdecl;
	int tdef;
	
	enum TemplateKind {
		TKIND_NA, TKIND_CLASS, TKIND_FUNC, TKIND_MEMFUNC, TKIND_STATMEM,
		TKIND_MEMCLASS, TKIND_TTPARAM
	};
	TemplateKind tkind;
	
	std::vector<TemplateParameter *> tparams;
	std::vector<TemplateSpecializationParameter *> tsparams;
	
	int tproto;
	int ttype;
	bool ttype_group;
	
	std::string ttext;
	
	SourceLocation * tpos_templateToken;
	SourceLocation * tpos_tokenEnd;
	SourceLocation * tpos_templateStart;
	SourceLocation * tpos_templateEnd;
	
	Template(int i = -1, std::string n = "-") 
		: id(i), name(n), sgTemplateDeclaration(NULL), tgroup(-1), tacs(T_ACS_NA),
		  tnspace(-1), tdecl(-1), tdef(-1), tkind(TKIND_NA), tproto(-1), ttype(-1), ttype_group(false),
		  ttext(""), tpos_templateToken(NULL), tpos_tokenEnd(NULL), tpos_templateStart(NULL),
		  tpos_templateEnd(NULL) {};
			
    
	
	friend std::ostream & operator<<(std::ostream & out, const Group & r);
    
	const std::string templateString(void) const {
        std::stringstream s;
		
		s << "te#" << id << " " << name << "\n";
		
		if(tloc != NULL) {
			s << "tloc " << (*tloc) << "\n";
		}
		
		if(tgroup > 0) {
			s << "tgroup gr#" << tgroup << "\n";
		}
		
		switch(tacs) {
			case T_ACS_NA:		break;
			case T_ACS_PUB: 	s << "tacs pub\n"; 	break;
			case T_ACS_PROT: 	s << "tacs prot\n"; break;
			case T_ACS_PRIV: 	s << "tacs priv\n"; break;
			default:        	std::cerr << "WARNING: Unknown template group access type encountered!" << std::endl;
		}
		
		if(tnspace > 0) {
			s << "tnspace na#" << tnspace << "\n";
		}
		
		if(tdecl > 0) {
			s << "tdecl te#" << tdecl << "\n";
		}
		
		if(tdef > 0) {
			s << "tdef te#" << tdef << "\n";
		}
		
		switch(tkind) {
			case TKIND_NA:      	break;
            case TKIND_CLASS:   	s << "tkind class\n";    break;
            case TKIND_FUNC:  		s << "tkind func\n";     break;
            case TKIND_MEMFUNC:		s << "tkind memfunc\n";  break;
            case TKIND_STATMEM:  	s << "tkind statmem\n";  break;
            case TKIND_MEMCLASS:  	s << "tkind memclass\n"; break;
            case TKIND_TTPARAM:  	s << "tkind ttparam\n";  break;
            default:            	std::cerr << "WARNING: Unknown template kind encountered: " << tkind << std::endl;
		}
		
		for(std::vector<TemplateParameter *>::const_iterator it = tparams.begin(); it != tparams.end(); ++it) {
			TemplateParameter * tparam = *it;
			if(tparam->id > 0) {
				s << "tparam ";
			
				switch(tparam->tparam_kind) {
					case TemplateParameter::TPARAM_NA:		break;
					case TemplateParameter::TPARAM_TYPE: {
						if(tparam->id > 0) {
							std::string id_flag = tparam->id_group ? "gr#" : "ty#";
							s << "type " << id_flag << tparam->id;
							if(tparam->defaultId > 0) {
								std::string did_flag = tparam->defaultId_group ? "gr#" : "ty#";
								s << " " << did_flag << tparam->defaultId;
							}
							s << "\n";
						}
					};
					break;
					case TemplateParameter::TPARAM_NTYPE: {
						if(tparam->id > 0) {
							std::string id_flag = tparam->id_group ? "gr#" : "ty#";
							s << "ntype " << id_flag << tparam->id << " " << tparam->name;
							s << " " << tparam->defaultValue << "\n";
						}
					};
					break; 	
					case TemplateParameter::TPARAM_TEMPL: 	{
						if(tparam->id > 0) {
							s << "templ te#" << tparam->id;
							if(tparam->defaultId > 0) {
								s << " te#" << tparam->defaultId;
							}
							s << "\n";
						}
					};
					break;
					default: 			std::cerr << "WARNING: Unknown template parameter kind encountered!" << std::endl;
				}
			}
		}
		
		for(std::vector<TemplateSpecializationParameter *>::const_iterator it = tsparams.begin(); it != tsparams.end(); ++it) {
			TemplateSpecializationParameter * tsparam = *it;
			if(tsparam->id > 0) {
				s << "tsparam ";
			
				switch(tsparam->tsparam_kind) {
					case TemplateSpecializationParameter::TSPARAM_NA:	break;
					case TemplateSpecializationParameter::TSPARAM_TYPE: {
						if(tsparam->id > 0) {
							std::string id_flag = tsparam->id_group ? "gr#" : "ty#";
							s << "type " << id_flag << tsparam->id;
							s << "\n";
						}
					};
					break;
					case TemplateSpecializationParameter::TSPARAM_NTYPE: {
						if(tsparam->id > 0) {
							std::string id_flag = tsparam->id_group ? "gr#" : "ty#";
							s << "ntype " << id_flag << tsparam->id << " " << tsparam->constant << "\n";
						}
					};
					break; 	
					case TemplateSpecializationParameter::TSPARAM_TEMPL: 	{
						if(tsparam->id > 0) {
							s << "templ te#" << tsparam->id;
							s << "\n";
						}
					};
					break;
					default: 			std::cerr << "WARNING: Unknown template specialization parameter kind encountered!" << std::endl;
				}
			}
		}
		
		switch(tkind) {
			case TKIND_NA: break;
			
			case TKIND_FUNC:
			case TKIND_MEMFUNC: {
				if(tproto > 0) {
					s << "tproto ro#" << tproto << "\n";
				}
			};
			break;
			case TKIND_CLASS:
			case TKIND_MEMCLASS: {
				if(tproto > 0) {
					s << "tproto gr#" << tproto << "\n";
				}
			};
			break;
			case TKIND_STATMEM: {
				if(ttype > 0) {
					std::string ttype_flag = ttype_group ? "gr#" : "ty#";
					s << "ttype " << ttype_flag << ttype << "\n";
				}
			};
			break;
			default: 		std::cerr << "WARNING: Unknown template kind encountered: " << tkind << std::endl;
		}
		
		if(!ttext.empty()) {
			s << "ttext " << ttext << "\n";
		}
		
		s << "tpos ";

        if(tpos_templateToken != NULL) {
            s << *(tpos_templateToken) << " ";
        } else {
            s << "NULL 0 0 ";
        }

        if(tpos_tokenEnd != NULL) {
            s << *(tpos_tokenEnd) << " ";
        } else {
            s << "NULL 0 0 ";
        }
        
        if(tpos_templateStart != NULL) {
            s << *(tpos_templateStart) << " ";
        } else {
            s << "NULL 0 0 ";
        }

        if(tpos_templateEnd != NULL) {
            s << *(tpos_templateEnd);
        } else {
            s << "NULL 0 0 ";
        }
        s << "\n";

        s << std::endl;
        
		return s.str();
    }
    
};

std::ostream & operator<<(std::ostream & out, const Template & t) {
    out << t.templateString();
    return out;
}


#endif
