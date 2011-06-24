/*
 *  	 na#[namespaceID]	<name_of_namespace>
 *  	nloc		<fileID> <line> <column>	     # location
 *  	nnspace		<parent_namespaceID>		     # parent namespace
 *  	nmem [...]	<typeID|routineID|groupID	     # IDs of members
 *  			|templateID|namespaceID>
 *  	nalias		<alias_name>			     # alias of namespace
 *  	npos		<namespace_token> <last_token_before_"{"> <"{"> <"}">
 *
 */

#ifndef __NAMESPACE_H__
#define __NAMESPACE_H__

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <stdlib.h>

#include "pdtutil.h"

class NamespaceMember {
public:
    enum NamespaceMemberKind {
        NS_NA, NS_TYPE, NS_ROUTINE, NS_GROUP, NS_TEMPL, NS_NS
    };
    
    NamespaceMemberKind kind;
    int id;
    std::string name;

    NamespaceMember(int i = -1, NamespaceMemberKind k = NS_NA) : kind(k), id(i) {};
};

class Namespace {
public:
    int id;
    std::string name;
    SourceLocation * nloc;
    SourceLocation * ns_token;
    SourceLocation * ns_tokenEnd;
    SourceLocation * ns_blockStart;
    SourceLocation * ns_blockEnd;
    int nnspace; // parent namespace
    std::vector<NamespaceMember*> nmems; // namespace members
    int nalias; // this namespace is an alias of nalias

    SgNamespaceDeclarationStatement * nsSgStmt;
	SgNamespaceAliasDeclarationStatement * nsAliasSgStmt;

    Namespace(int i, std::string n) : id(i), name(n), nloc(NULL), ns_token(NULL), ns_tokenEnd(NULL), 
                                      ns_blockStart(NULL), ns_blockEnd(NULL), nnspace(-1), nalias(-1),
                                      nsSgStmt(NULL), nsAliasSgStmt(NULL) {};

    friend std::ostream & operator<<(std::ostream & out, const Namespace & r);
            
    const std::string namespaceString(void) const {
        std::stringstream s;

        s << "na#" << id << " " << name << "\n";
        
        s << "nloc ";
        if(nloc != NULL) {
            s << *(nloc) << "\n";
        } else {
            s << "NULL 0 0\n";
        }

        if(nnspace > 0) {
            s << "nnspace na#" << nnspace << "\n";
        }

        for(std::vector<NamespaceMember*>::const_iterator it = nmems.begin(); it != nmems.end(); ++it) {
            s << "nmem ";
            switch((*it)->kind) {
                case NamespaceMember::NS_TYPE:      s << "ty#";     break;
                case NamespaceMember::NS_ROUTINE:   s << "ro#";     break;
                case NamespaceMember::NS_GROUP:     s << "gr#";     break;
                case NamespaceMember::NS_TEMPL:     s << "te#";     break;
                case NamespaceMember::NS_NS:        s << "na#";     break;
                default: std::cerr << "WARNING: Unknown namespace member kind encountered." << std::endl;
            }
            s << ((*it)->id) << "\n";
        }

        if(nalias > 0) {  
            s << "nalias na#" << nalias << "\n";
        }

        s << "npos ";
        if(ns_token != NULL) {
            s << *(ns_token) << " ";
        } else { 
            s << "NULL 0 0 ";
        }

        if(ns_tokenEnd != NULL ) {
            s << *(ns_tokenEnd) << " ";
        } else {
            s << "NULL 0 0 ";
        }

        if(ns_blockStart != NULL) {
            s << *(ns_blockStart) << " ";
        } else {
            s << "NULL 0 0 ";
        }

        if(ns_blockEnd != NULL) {
            s << *(ns_blockEnd);
        } else {
            s << "NULL 0 0";
        }

        s << "\n";

        s << std::endl;

        return s.str();
    };


};

std::ostream & operator<<(std::ostream & out, const Namespace & r) {
    out << r.namespaceString();
    return out;
}

#endif
