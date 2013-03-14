/*          
 *  roseparse
 *  This program uses the ROSE compiler framework to read in C, C++, UPC or
 *  Fortran code, generates an abstract syntax tree, and uses ROSE to
 *  traverse the AST, extract the data needed to produce a PDB (Program
 *  Database, used by PDT) file.
 */

#include "rose.h"

enum Language {
	LANG_NONE, LANG_C, LANG_CPP, LANG_C_CPP, LANG_FORTRAN, LANG_JAVA, LANG_MULTI, LANG_UPC
};

#include "taurose.h"
#include "pdtutil.h"
#include "routine.h"
#include "type.h"
#include "sourcefile.h"
#include "statement.h"
#include "group.h"
#include "namespace.h"
#include "template.h"
#include "macro.h"
#include "pragma.h"

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::vector;
using std::stringstream;
using std::fstream;
using std::ifstream;

const int PDB_VERSION = 3;
const int UPC_PDB_VERSION = 4;
const string PDT_ATTRIBUTE = "PDT_ATTRIBUTE";

// Different types of entries the PDB file are numbered separately.
// Per PDB format specification, these are not guaranteed to be
// contiguous or to start at zero (which here they don't; they start
// at one.)
int nextFunctionID = 1;

// To be consistent with cparse and cxxparse, type and group
// ID numbers are drawn from the same pool, thereby avoiding
// a type and a group with the same ID.
int nextTypeID = 1;

int nextNamespaceID = 1;
int nextTemplateID = 1;
int nextMacroID = 1;
int nextPragmaID = 1;

// Groups are just an alternate form of type notated gr#NNN instead
// of ty#NNN. 
class TypeID {
public:
    int id;
    bool group;
    Type * type;
    
    TypeID(int i = -1, bool g = false, Type * t = NULL) : id(i), group(g), type(t) {};
};

// We maintain maps from the unique name ROSE produces for each entity in the
// code to the object we generate to represent its entry in the PDB file.
// This is because when we are traversing the AST, we may come across
// a reference to a type, function, etc. which has not been processed and
// therefore doesn't have an ID. We save a reference to these objects so
// we can look them up later and fill in missing IDs.
map<string, TypeID> typeMap;
map<string, Routine*> routineMap;
map<string, Group*> groupMap;
map<string, Namespace*> namespaceMap;
map<string, Template*> templateMap;

// We maintain vectors of objects representing each possible type of entry
// in the PDB file. When we are done processing, we iterate through the
// arrays, printing them into the PDB file.
vector<Routine*> routines;
vector<Type*> types;
vector<RoutineCall*> calls; // In PDT, calls are separate from statements.
vector<Group*> groups;
vector<Namespace*> namespaces;
vector<Template*> templates;
vector<Macro*> macros;
vector<Pragma*> pragmas;

// Keep track of the statement processed before the current one.
Statement * prevStmt = NULL;

// Which language is the project written in?
Language lang;

// ROSE's Top-down Bottom-up processing allows us to pass attributes down
// the AST as we traverse it, as well as back up the tree as we return
// up it.
class VisitorTraversal : public AstTopDownBottomUpProcessing<InheritedAttribute, SynthesizedAttribute> {
public:
	virtual InheritedAttribute evaluateInheritedAttribute(SgNode * n, InheritedAttribute inheritedAttribute);
    virtual SynthesizedAttribute evaluateSynthesizedAttribute(SgNode * n, InheritedAttribute inheritedAttribute, SubTreeSynthesizedAttributes synthesizedAttributeList);
};


string normalizeTypeName(const string & name) {
    string result(name);    
    boost::algorithm::replace_all(result, "(", " (");
    boost::algorithm::replace_all(result, " )", ")");
    boost::algorithm::replace_all(result, " ,", ",");
    return result;
}

inline std::string getUniqueTypeName(SgType * type) {
    return type->get_mangled().str() + normalizeTypeName(type->unparseToString());
}

// handletype()
// If we've already handled this type before, we return the ID of the
// previously generated PDB TYPE entry. Otherwise, we generate an entry
// with the appropriate fields filled in and store it for future reference.
TypeID handleType(SgType * type, Namespace * parentNamespace, bool isGroup = false) {
	string st;
	
	// Create a name for this type. Types in ROSE can be named types
	// (classes with a user-supplied name: Class, Enum, and Typedef)
	// in which case we use the fully-qualified user-supplied name;
	// or ordinary types (everything else) in which case we unparse
	// the type.
	// We can't just unparse named types because we won't get namespace
	// or parent class information in the name.
	SgNamedType * namedType = isSgNamedType(type);
	if(namedType != NULL) {
		st = namedType->get_name();
	} else {
		st = normalizeTypeName(type->unparseToString());
	}
		
	std::string mangledName = getUniqueTypeName(type);
	
	// Find out if we've already handled this type...
	if(typeMap.count(mangledName) != 0) {
        // If so, look up the previously generated ID...
		TypeID prevtid = typeMap[mangledName];
		return typeMap[mangledName];
		
	// ... otherwise handle it now.
	} else {
		int id = nextTypeID++;
		Type * t = new Type(id, st); // (id number, name)
        if(SgProject::get_verbose() > 5) {
            std::cerr << "Handling type ty#" << id << " " << st << " for " << type->sage_class_name() << std::endl;
        }
		t->fortran = (lang == LANG_FORTRAN);
        TypeID typeID(id, isGroup, t);    // (id number, is a group)

		// TYPE REFERENCE TYPES (modifiers)
		// Determine if the type is declared const, volatile, or restrict.
		// In ROSE, modifiers to types are expressed by wrapping the type
		// in an SgModifierType, which holds a reference to, confusingly enough,
		// an SgTypeModifier.
		SgModifierType * modType = isSgModifierType(type);
		if(modType != NULL) {
			t->ykind = Type::TREF;
			const SgTypeModifier & typeMod = modType->get_typeModifier();
			const SgConstVolatileModifier & constMod = typeMod.get_constVolatileModifier();
            const SgUPC_AccessModifier & upcMod = typeMod.get_upcModifier();
            // const
			if(constMod.isConst()) {
				t->yqual = true;
			}
            // volatile
			if(constMod.isVolatile()) {
				t->yqual_volatile = true;
			}
            // restrict
			if(typeMod.isRestrict()) {
				t->yqual_restrict = true;
			}
            // UPC shared
            if(upcMod.get_isShared()) {
                t->yshared = true;
                t->yblocksize = SageInterface::getUpcSharedBlockSize(type);
                t->ystrict = upcMod.isUPC_Strict();
                t->yrelaxed = upcMod.isUPC_Relaxed();
            }
            // UPC strict
            if(upcMod.isUPC_Strict()) {
                t->ystrict = true;
            }
            // UPC relaxed
            if(upcMod.isUPC_Relaxed()) {
                t->yrelaxed = true;
            }
			// Now that we've processed the modifiers, we want to continue
			// with the type that it wraps.
			TypeID baseTypeID = handleType(modType->get_base_type(), parentNamespace);
			t->ytref = baseTypeID.id;
			t->ytref_group = baseTypeID.group;
		
		// TYPEDEF TYPE (tref)
		// Handle this before float types, because ROSE will say that a typedef
		// to a float is a float type (but it won't do this for integer types, oddly.)
		} else if(isSgTypedefType(type)) {
			t->ykind = Type::TREF;
			SgTypedefType * typedefType = isSgTypedefType(type);
			TypeID trefType = handleType(typedefType->get_base_type(), parentNamespace);
			t->ytref = trefType.id;
			t->ytref_group = trefType.group;
			if(parentNamespace != NULL) {
                NamespaceMember * nm = new NamespaceMember(typeID.id, NamespaceMember::NS_TYPE);
                parentNamespace->nmems.push_back(nm);
				t->ynspace = parentNamespace->id;
            }
		
		// FUNCTION TYPE
        // Functions that are actually defined in the code are handled in the
        // handleFunctionType() function below. This case is for function
        // types that don't correspond to an actual function defined in the code,
        // as in the base type of a function pointer.
		} else if(isSgFunctionType(type)) {
			SgFunctionType * fnType = isSgFunctionType(type);
			SgType * fnRetType = fnType->get_return_type();
			t->ykind = Type::FUNC;
			TypeID retID = handleType(fnRetType, parentNamespace);
			t->yrett = retID.id;
			t->yrett_group = retID.group;
			const SgTypePtrList & argTypes = fnType->get_arguments();
			for(SgTypePtrList::const_iterator it = argTypes.begin(); it != argTypes.end(); it++) {
				SgType * argType = (*it);
				if(isSgTypeEllipse(argType)) {
					t->yellip = true;
					continue;
				}
				TypeID argID = handleType(argType, parentNamespace);
                // This type doesn't correspond to a particular function, so
                // arguments don't have names.
				t->yargts.push_back(new ArgumentType(argID.id, argID.group, "-", NULL));
			}
		
		// INTEGER TYPES
		} else if(type->isIntegerType()) {
			t->ykind = Type::INT;
			if(isSgTypeSignedChar(type)) {
				t->yikind = Type::INT_SCHAR;
			} else if(isSgTypeUnsignedChar(type)) {
				t->yikind = Type::INT_UCHAR;
			} else if(isSgTypeChar(type) || isSgTypeBool(type)) {
                // Note that the Fortran character type is SgCharType
                // if it is of length 1, but SgStringType if it is
                // of any other length.
				t->yikind = Type::INT_CHAR;
			} else if(isSgTypeShort(type)) {
				t->yikind = Type::INT_SHORT;
			} else if(isSgTypeUnsignedShort(type)) {
				t->yikind = Type::INT_USHORT;
			} else if(isSgTypeInt(type)) {
				t->yikind = Type::INT_INT;
			} else if(isSgTypeUnsignedInt(type)) {
				t->yikind = Type::INT_UINT;
			} else if(isSgTypeLong(type)) {
				t->yikind = Type::INT_LONG;
			} else if(isSgTypeUnsignedLong(type)) {
				t->yikind = Type::INT_ULONG;
			} else if(isSgTypeLongLong(type)) {
				t->yikind = Type::INT_LONGLONG;
			} else if(isSgTypeUnsignedLongLong(type)) {
				t->yikind = Type::INT_ULONGLONG;
			} else if(isSgTypeWchar(type)) {
				t->yikind = Type::INT_WCHAR;
			} else {
					std::cerr	<< "WARNING: Unknown integer type " << type->sage_class_name() 
								<< " encountered." << endl;
			}
			
		// FLOAT TYPES
		} else if(type->isFloatType()) {
			t->ykind = Type::FLOAT;
			if(isSgTypeFloat(type)) {
				t->yfkind = Type::FLOAT_FLOAT;
			} else if(isSgTypeDouble(type)) {
				t->yfkind = Type::FLOAT_DBL;
			} else if(isSgTypeLongDouble(type)) {
				t->yfkind = Type::FLOAT_LONGDBL;
			} else {
					std::cerr	<< "WARNING: Unknown floating point type " << type->sage_class_name() 
							<< " encountered." << endl;
			}
			
		// POINTER TO MEMBER FUNCTION
		// This has to come before SgPointerType since SgPointerMemberType is
		// a subclass of SgPointerType. For some reason, calling the dereference
		// function on SgPointerMemberType results in the same type being returned,
		// causing an infinite loop if we do so and then handle that type.
        // We have to use get_base_type() instead.
		} else if(isSgPointerMemberType(type)) {
			t->ykind = Type::PTRMEM;
			SgPointerMemberType * memType = isSgPointerMemberType(type);
			
			TypeID classType = handleType(memType->get_class_type(), parentNamespace);
			t->ympgroup = classType.id;
			TypeID baseType = handleType(memType->get_base_type(), parentNamespace);
			t->ymptype = baseType.id;
			t->ymptype_group = baseType.group;
        	
			
		// POINTER TYPE
		} else if(isSgPointerType(type)) {
			t->ykind = Type::PTR;
			TypeID ptrType = handleType(type->dereference(), parentNamespace);
			t->yptr = ptrType.id;
            t->yptr_group = ptrType.group;
			
		// REFERENCE TYPE
		} else if(isSgReferenceType(type)) {
			t->ykind = Type::REF;
			TypeID refType = handleType(type->dereference(), parentNamespace);
			t->yref = refType.id;
            t->yref_group = refType.group;
			
		// DEFAULT TYPE (handle as if it were void)
		} else if(isSgTypeDefault(type)) {
			return handleType(SageBuilder::buildVoidType(), parentNamespace);
		
		// ELLIPSE TYPE (ignore)
        // This is handled as part of a function type.
		} else if(isSgTypeEllipse(type)) {
			t->id = -6;
			typeID.id = -6;
			return typeID;

        // ENUM TYPE
        } else if(isSgEnumType(type)) {
           t->ykind = Type::ENUM;
           // The enum type doesn't carry information on the values of the enum;
           // the declaration has that, so we can't fill out the rest of the
           // fields until we encounter the declaration.
		
		// ARRAY TYPE
		} else if(isSgArrayType(type)) {
			SgArrayType * arr = isSgArrayType(type);
			t->ykind = Type::ARRAY;
			TypeID arrayBaseType = handleType(arr->get_base_type(), parentNamespace);
			t->yelem = arrayBaseType.id;
			t->yelem_group = arrayBaseType.group;
			
            // getArrayElementCount crashes in Fortran code.
			if(lang != LANG_FORTRAN) {
				t->ynelem = SageInterface::getArrayElementCount(arr);
			} else {
				t->yrank = arr->get_rank();
			}
						
        // TPARAM
        } else if(isSgTemplateType(type)) {
            t->ykind = Type::TPARAM;
		
		// VOID TYPE
		} else if(isSgTypeVoid(type)) {
			t->ykind = Type::VOID;
        
        // CLASS TYPE
        // These are for class types not associated with a class definition,
        // which are handled separately below.
		} else if(isSgClassType(type)) {
			typeID.group = true;
			if(!isGroup) {
				SgClassType * ct = isSgClassType(type);
				SgDeclarationStatement * declStmt = ct->get_declaration();
				SgClassDeclaration * classDec = NULL;
				if(declStmt != NULL) {
					classDec = isSgClassDeclaration(declStmt);
				}
				std::string name = "-";
				if(classDec != NULL) {
					name = classDec->get_name().getString();
				}
				Group * group = new Group(typeID.id, name, new SourceLocation(declStmt->get_startOfConstruct()));
	            groups.push_back(group);
	            groupMap[classDec->get_mangled_name().getString()] = group;
				if(classDec != NULL) {
					switch(classDec->get_class_type()) {
		                case SgClassDeclaration::e_class: 
		                    group->gkind = Group::GKIND_CLASS; break;
		                case SgClassDeclaration::e_struct:
		                    group->gkind = Group::GKIND_STRUCT; break;
		                case SgClassDeclaration::e_union:
		                    group->gkind = Group::GKIND_UNION; break;
		                case SgClassDeclaration::e_template_parameter:
		                    group->gkind = Group::GKIND_TPROTO; break;
		                default:
		                    group->gkind = Group::GKIND_NA;
		            }
				}
			}
            // Don't keep the class type in the list of types; groups
            // are stored separately.
            t->ykind = Type::NA;
        } else {
				std::cerr << "WARNING: Unhandled type " << type->sage_class_name() << " encountered." << endl;
		}
		
		// ROSE uses SgTypeChar for a FORTRAN character type of length
		// one, but a SgTypeString for a FORTRAN character type of
		// some other length.
		if(t->fortran && isSgTypeChar(type)) {
			t->ykind = Type::FCHAR;
			t->yclen = 1;
		}
		
		if(t->fortran && isSgTypeString(type)) {
			t->ykind = Type::FCHAR;
			SgExpression * lenExpr = isSgTypeString(type)->get_lengthExpression();
			if(isSgValueExp(lenExpr)) {
				t->yclen = SageInterface::getIntegerConstantValue(isSgValueExp(lenExpr));
			}
		}
		
 		typeMap[mangledName] = typeID;
        if(t->ykind != Type::NA) {
		    types.push_back(t);
            if(SgProject::get_verbose() > 5) {
                std::cerr << "Added a type ty#" << t->id << " " << t->name << std::endl;
            }
        } else {
			delete t;
		}
		return typeID;
	}
}

// This handles functions declared in the code. This needs to be handled
// separately from other types because SgFunctionType does not carry the names
// of parameters with it, but we want to output such names in the PDB file.
// As such, we need an SgFunctionParameterList as well, from which we
// can get these names.
int handleFunctionType(SgFunctionType * type, SgFunctionParameterList * params, bool cgen, Namespace * parentNamespace) {
	string st = normalizeTypeName(type->unparseToString());
	int id = nextTypeID++;
    if(SgProject::get_verbose() > 5) {
        std::cerr << "Handling function type ty#" << id << " " << st << " for " << type->sage_class_name() <<  std::endl;
    }
	Type * t = new Type(id, st);
    TypeID fnTypeID(id, false, t);
	t->ykind = Type::FUNC;
	TypeID tid = typeMap[getUniqueTypeName(type->get_return_type())];
	t->yrett = tid.id;
    t->yrett_group = tid.group;
	SgInitializedNamePtrList & ptrList = params->get_args();
	if(!ptrList.empty()) {
		for(SgInitializedNamePtrList::iterator j = ptrList.begin(); j != ptrList.end(); j++) {
			SgType * pType = (*j)->get_type();
			if(isSgTypeEllipse(pType)) {
				t->yellip = true;
				continue;
			}
			string typeName = getUniqueTypeName(pType);
			SourceLocation * loc = new SourceLocation( (*j)->get_file_info() );
			std::string paramName;
			if(!cgen) {
				paramName = (*j)->get_name().getString();
			}
			if(cgen || paramName.empty()) {
				paramName = "-";
			}
			TypeID paramId = typeMap[typeName];
			t->yargts.push_back(new ArgumentType(paramId.id, paramId.group, paramName, loc));
		}
	}
	typeMap.insert( std::pair<string,TypeID>(type->get_mangled().str(),fnTypeID) );
	types.push_back(t);
	return id;
	
}

Template * handleTemplate(SgTemplateDeclaration * tDecl, Namespace * parentNamespace) {
		std::string mangledName = tDecl->get_mangled_name().getString();
		std::string qualifiedName = tDecl->get_name().getString();
		std::string uniqueName = SageInterface::generateUniqueName(tDecl, true);
		Template * templ = new Template(nextTemplateID++, qualifiedName);
				
		// Check if we previously encountered a forward declaration.
		if(templateMap.count(mangledName) != 0) {
			templateMap[mangledName]->tdef = templ->id;
			templ->tdecl = templateMap[mangledName]->id;
		}
		
		templateMap[mangledName] = templ;
		templates.push_back(templ);
		
		// If template was declared in a namespace, make note of this.
		if(parentNamespace != NULL) {
            templ->tnspace = parentNamespace->id;       
            NamespaceMember * nm = new NamespaceMember(templ->id, NamespaceMember::NS_TEMPL);
            parentNamespace->nmems.push_back(nm);
        }

		templ->tloc = new SourceLocation(tDecl->get_startOfConstruct());
		// TODO Is there any way to get the first character location for
		// tpos_templateToken? There isn't an obvious one.
		// Probably don't have to worry about tpos_tokenEnd,
		// as cxxparse omits it.
		templ->tpos_templateStart = new SourceLocation(tDecl->get_startOfConstruct());
		templ->tpos_templateEnd = new SourceLocation(tDecl->get_endOfConstruct());
		

		switch(tDecl->get_template_kind()) {
			case SgTemplateDeclaration::e_template_none: {
					std::cerr << "WARNING: ROSE template declaration has no type.\n" << tDecl->unparseToString() << std::endl;
				templ->tkind = Template::TKIND_NA;
			};
			break;
			
			case SgTemplateDeclaration::e_template_class: 		
				templ->tkind = Template::TKIND_CLASS; 		break;
			case SgTemplateDeclaration::e_template_m_class: 	
				templ->tkind = Template::TKIND_MEMCLASS; 	break;
			case SgTemplateDeclaration::e_template_function: 	
				templ->tkind = Template::TKIND_FUNC; 		break;
			case SgTemplateDeclaration::e_template_m_function: 	
				templ->tkind = Template::TKIND_MEMFUNC;		break;
			case SgTemplateDeclaration::e_template_m_data:
				templ->tkind = Template::TKIND_STATMEM;		break;
				
			default: {
					std::cerr << "WARNING: Unknown ROSE template declaration type encountered.\n" << tDecl->unparseToString() << std::endl;
			};
		}
				
		// This function doesn't return a reference to a list, unlike
		// every other ptrList-returning function I've encountered in ROSE.
		SgTemplateParameterPtrList pList = tDecl->get_templateParameters();
				
		for(SgTemplateParameterPtrList::const_iterator it = pList.begin(); it != pList.end(); ++it) {
			SgTemplateParameter * sgParam = *it;
			TemplateParameter * tparam = new TemplateParameter();
			templ->tparams.push_back(tparam);
			
			switch(sgParam->get_parameterType()) {
				case SgTemplateParameter::parameter_undefined: {
					tparam->tparam_kind = TemplateParameter::TPARAM_NA;
                    std::cerr << "WARNING: ROSE template parameter had no type.\n" << sgParam->unparseToString() << std::endl;
				};
				break;
				
				case SgTemplateParameter::type_parameter: {
					tparam->tparam_kind = TemplateParameter::TPARAM_TYPE;
					SgType * pType = sgParam->get_type();
					SgType * defType = sgParam->get_defaultTypeParameter();
					if(pType != NULL) {
						TypeID pTypeID = handleType(pType, parentNamespace);
						tparam->id = pTypeID.id;
						tparam->id_group = pTypeID.group;
					}
					TypeID defTypeID = handleType(defType, parentNamespace);
					if(defType != NULL) {
						TypeID defTypeID = handleType(defType, parentNamespace);
						tparam->defaultId = defTypeID.id;
						tparam->defaultId_group = defTypeID.group;
					}
				};
				break;
				
				case SgTemplateParameter::nontype_parameter: {
					tparam->tparam_kind = TemplateParameter::TPARAM_NTYPE;
					SgType * pType = sgParam->get_type();
					if(pType != NULL) {
						TypeID pTypeID = handleType(pType, parentNamespace);
						tparam->id = pTypeID.id;
						tparam->id_group = pTypeID.group;
					}
					SgExpression * pExpr = sgParam->get_expression();
					SgExpression * defExpr = sgParam->get_defaultExpressionParameter();
					if(pExpr != NULL) {
						tparam->name = pExpr->unparseToString();
					} else {
						tparam->name = "-";
					}
					if(defExpr != NULL) {
						tparam->defaultValue = defExpr->unparseToString();
					} else {
						tparam->defaultValue = "";
					}
				};
				break;
				
				case SgTemplateParameter::template_parameter: {
					tparam->tparam_kind = TemplateParameter::TPARAM_TEMPL;                     
					SgTemplateDeclaration * defTempl = isSgTemplateDeclaration(sgParam->get_defaultTemplateDeclarationParameter());
					if(defTempl != NULL) {
						if(templateMap.count(defTempl->get_mangled_name().getString()) != 0) {
							tparam->id = templateMap[defTempl->get_mangled_name().getString()]->id;
						}
					}
				};
				break;
				
				default: {
						std::cerr << "WARNING: Unknown ROSE template parameter type encountered." << sgParam->unparseToString() << std::endl;
				}
			}
		}
		
		templ->ttext = tDecl->get_string().getString();
		boost::algorithm::replace_all(templ->ttext, "\\\n", " ");
		boost::algorithm::replace_all(templ->ttext, "\n", " ");
		
        return templ;
		
}

// This is where we actually gather data from the AST. This function is
// called on each node as we do a depth-first traversal of the AST. Whatever
// we store in the InheritedAttribute is passed down to children of this node.
InheritedAttribute VisitorTraversal::evaluateInheritedAttribute(SgNode* n, InheritedAttribute inheritedAttribute) {
	// Grab information about our parent.
	Routine * parentRoutine = inheritedAttribute.routine;
    Statement * parentStatement = inheritedAttribute.statement;
    Group * parentGroup = inheritedAttribute.group;
    Namespace * parentNamespace = inheritedAttribute.ns;
    Statement * switchCase = inheritedAttribute.switchCase;
    Statement * afterSwitch = inheritedAttribute.afterSwitch;
	Type * parentEnum = inheritedAttribute.parentEnum;
	Template * parentTemplate = inheritedAttribute.parentTemplate;
    SgTemplateFunctionDefinition * templateFunctionDefinition = inheritedAttribute.templateFunctionDefinition;
    PDTAttribute * pdtAttr = new PDTAttribute();

    Sg_File_Info * s = n->get_startOfConstruct();
    Sg_File_Info * e = n->get_endOfConstruct();

    if(SgProject::get_verbose() > 5) {
        std::cerr << "Now processing: " << n->class_name() << " parent routine: " << parentRoutine << " " << (parentRoutine != NULL ? parentRoutine->name : std::string()) << "          " << n->unparseToString() << std::endl;
    }

	// MACROS and COMMENTS
	SgLocatedNode * locatedNode = isSgLocatedNode(n);
	if(locatedNode != NULL) {
		// Macros and comments aren't directly represented in the AST.
		// Rather, they are attached to the AST node nearest to which
		// they occur.
		AttachedPreprocessingInfoType* preproc = locatedNode->getAttachedPreprocessingInfo();
        if(preproc != NULL) {
			for(AttachedPreprocessingInfoType::iterator it = preproc->begin(); it != preproc->end(); it++ ) {
				switch((*it)->getTypeOfDirective()) {
					
					// MACROS
					case PreprocessingInfo::CpreprocessorDefineDeclaration:
					case PreprocessingInfo::CpreprocessorUndefDeclaration: {
						std::string text = (*it)->getString();
						boost::algorithm::replace_all(text, "\\\n", " ");
						boost::algorithm::erase_all(text, "\n");
						
						Macro * macro = new Macro(nextMacroID++, new SourceLocation((*it)->get_file_info()), 
										(*it)->getTypeOfDirective() == PreprocessingInfo::CpreprocessorUndefDeclaration,
										text);
						macros.push_back(macro);
						
					};
					break;
					
					// COMMENTS
					case PreprocessingInfo::C_StyleComment:        
					case PreprocessingInfo::CplusplusStyleComment: 
					case PreprocessingInfo::FortranStyleComment:   
					case PreprocessingInfo::F90StyleComment: {
						std::string text = (*it)->getString();
						boost::algorithm::replace_all(text, "\\\n", " ");
						boost::algorithm::erase_all(text, "\n");
						int fileID = (*it)->get_file_info()->get_file_id() + 1;
						if(sourceFileMap.count(fileID) != 0) {
							SourceFile * sourceFile = sourceFileMap[fileID];
							if(sourceFile != NULL) {
								Comment * com = new Comment(sourceFile->nextCommentID++);
								switch((*it)->getTypeOfDirective()) {
									case PreprocessingInfo::C_StyleComment:
										com->lang = LANG_C;
										break;
									case PreprocessingInfo::CplusplusStyleComment: 
										com->lang = LANG_CPP;
										break;
									case PreprocessingInfo::FortranStyleComment:   
									case PreprocessingInfo::F90StyleComment:
										com->lang = LANG_FORTRAN;
										break;
									default: ;
								}
								SourceLocation * loc = new SourceLocation((*it)->get_file_info());
								com->start = loc->locationString();
								com->end = loc->locationString();
								com->text = text;
								sourceFile->scoms.push_back(com);
							}
						}
					}
                    break;

					
                   // case PreprocessingInfo::CpreprocessorIncludeDeclaration: {
                   //     std::cerr << "found include" << std::endl;
                   //     PreprocessingInfo::rose_include_directive * incDir = (*it)->get_include_directive();                                                         
                   //     std::cerr << incDir->directive.get_value() << std::endl;
                   // }  
                   // break;

					default: ; // Ignore other types of preproc info
				}
			}
		}
	}


	SgFunctionDeclaration * dec = isSgFunctionDeclaration(n);
	
	
    // *** FUNCTIONS / METHODS / ROUTINES ***
	SgFunctionDefinition * def = isSgFunctionDefinition(n);
	if( (def != NULL || dec != NULL) && !isSgEntryStatement(dec) ) {
		if(dec == NULL && def != NULL) {
			dec = def->get_declaration();
		} else if(dec != NULL && def == NULL){
			def = dec->get_definition();
		}
				
        if(SgProject::get_verbose() > 5) {
            std::cerr << "Encountering routine: " << dec->get_mangled_name().getString() << std::endl;
        }

		// First check to see if we've handled this function already.
		if(routineMap.count(dec->get_mangled_name().getString()) == 0) { 
			Routine * r = new Routine(nextFunctionID++, def, dec->get_name().getString());
			r->fortran = (lang == LANG_FORTRAN);
				
	        routineMap[dec->get_mangled_name().getString()] = r;
	        pdtAttr->routine = r;
	        parentRoutine = r;

			if(isSgTemplateInstantiationFunctionDecl(dec)) {
				SgTemplateInstantiationFunctionDecl * instDecl = isSgTemplateInstantiationFunctionDecl(dec);
				SgTemplateFunctionDeclaration * tDecl = instDecl->get_templateDeclaration();
				std::string templateName = tDecl->get_mangled_name().getString();
				if(templateMap.count(templateName) != 0) {
					r->rtempl = templateMap[templateName]->id;
				} 
			}
			
			if(dec->isSpecialization()) {
				r->rspecl = true;
			}

			r->rcgen = dec->get_file_info()->isCompilerGenerated();
			if(!r->rcgen) {
				r->rloc = new SourceLocation(s);
			}
			// Type
			SgFunctionType * type = dec->get_type();
	        SgType * retType = type->get_return_type();
	        SgFunctionParameterList * params = dec->get_parameterList();
	        SgInitializedNamePtrList & ptrList = params->get_args();

			handleType(retType, parentNamespace);
		
			if(!ptrList.empty()) {
	            for(SgInitializedNamePtrList::iterator j = ptrList.begin(); j != ptrList.end(); j++) {
	                SgType * pType = (*j)->get_type();
					handleType(pType, parentNamespace);
	            }
	        }

	        // Namespace
	        if(parentNamespace != NULL) {
	            r->rnspace = parentNamespace->id;
	        }

			r->rsig = handleFunctionType(type, params, r->rcgen, parentNamespace);
		
			// Linkage
			const string & linkage = dec->get_linkage();
			if(linkage.empty()) {
					switch(lang) {
                        case LANG_UPC:      // fallthrough
						case LANG_C:  		r->rlink = Routine::C; 		 break;
						case LANG_CPP: 		r->rlink = Routine::CPP;	 break;
						case LANG_FORTRAN: 	r->rlink = Routine::FORTRAN; break;
						default: if(SgProject::get_verbose() > 0) {
							std::cerr << "Unknown linkage type encountered" << std::endl;
						}
					}
			} else {
				if(linkage.compare("C++") == 0) {
					r->rlink = Routine::CPP;
				} else if(linkage.compare("C") == 0) {
					r->rlink = Routine::C;
				} else if(linkage.compare("FORTRAN") == 0
				          ||  linkage.compare("f90") == 0) {
					r->rlink = Routine::FORTRAN;
				}
			}
		
			// Storage Modifiers
			const SgDeclarationModifier & decMod = dec->get_declarationModifier();
			const SgStorageModifier & storeMod = decMod.get_storageModifier();
			const SgTypeModifier & typeMod = decMod.get_typeModifier();
			
			if(lang != LANG_FORTRAN) {
				if(storeMod.isExtern()) {
					r->rkind = Routine::EXT;
				} else if(storeMod.isStatic()) {
					r->rkind = Routine::STAT;
				} else if(storeMod.isAuto()) {
					r->rkind = Routine::AUTO;
				} else if(storeMod.isAsm()) {
					r->rkind = Routine::ASM;
				} else {
					r->rkind = Routine::NA;
				}
			} else {
				if(typeMod.isIntrinsic()) {
					r->rlink = Routine::FINT;
					r->rkind = Routine::FINTRIN;
				} else if(isSgProgramHeaderStatement(dec)) {
					r->rkind = Routine::FPROG;
				} else {
					r->rkind = Routine::FEXT;
				}
			}
						
	        // Special kind
	        const SgSpecialFunctionModifier & specMod = dec->get_specialFunctionModifier();
	        if(specMod.isConstructor()) {
	            r->rskind = Routine::CTOR;
	        } else if(specMod.isDestructor()) {
	            r->rskind = Routine::DTOR;
	        } else if(specMod.isConversion()) {
	            r->rskind = Routine::CONV;
	        } else if(specMod.isOperator()) {
	            r->rskind = Routine::OP;
	        } else {
	            r->rskind = Routine::NONE;
	        }


			// Virtual
			const SgFunctionModifier & funcMod = dec->get_functionModifier();
			if(funcMod.isPure()) {
				r->rvirt = Routine::PURE;
			} else if(funcMod.isVirtual()) {
				r->rvirt = Routine::VIRT;	
			} else {
				r->rvirt = Routine::VIRT_NO;	
			}
        
	        // explicit modifier
	        if(funcMod.isExplicit()) {
				if(lang != LANG_FORTRAN) {
	            	r->rexpl = true;
				} else {
					r->rarginfo = true;
				}
	        }
	
			if(funcMod.isElemental()) {
				r->riselem = true;
			}
			
			if(funcMod.isRecursive()) {
				r->rrec = true;
			}
		
			// rpos
			if(def != NULL) {
				Sg_File_Info * decStart = dec->get_startOfConstruct();
		        Sg_File_Info * decEnd = dec->get_endOfConstruct();
				SgBasicBlock * body = def->get_body();
				Sg_File_Info * bodyStart = body->get_startOfConstruct();
		        Sg_File_Info * bodyEnd = body->get_endOfConstruct();

				r->rpos_rtype = new SourceLocation(decStart);
				r->rpos_endDecl = new SourceLocation(decEnd);
				r->rpos_startBlock = new SourceLocation(bodyStart);
				r->rpos_endBlock = new SourceLocation(bodyEnd);
			} else if (dec != NULL) {
				Sg_File_Info * decStart = dec->get_startOfConstruct();
		        Sg_File_Info * decEnd = dec->get_endOfConstruct();
				r->rpos_rtype = new SourceLocation(decStart);
				r->rpos_endDecl = new SourceLocation(decEnd);
	        }
		
			// body of function
			if(def != NULL) {
				SgBasicBlock * body = def->get_body();
				Sg_File_Info * bodyStart = body->get_startOfConstruct();
		        Sg_File_Info * bodyEnd = body->get_endOfConstruct();
				if(lang != LANG_FORTRAN) {
					Statement * stmt = new Statement(r->stmtId++, def, Statement::STMT_BLOCK);
					stmt->start = new SourceLocation(bodyStart);
					stmt->end = new SourceLocation(bodyEnd);
		            const SgStatementPtrList & l = body->get_statements();
		            if(l.size() > 0) {
		                stmt->downSgStmt = l.front();
		            }
					r->rstmts.push_back(stmt);
					r->rbody = stmt->id;
				} else {
					// FORTRAN doesn't use blocks
					r->rbody = 0;
					r->rstart = NULL;
				}
			}
		
			routines.push_back(r);

	        if(parentNamespace != NULL) {
	            NamespaceMember * nm = new NamespaceMember(r->id, NamespaceMember::NS_ROUTINE);
	            nm->name = dec->get_mangled_name().getString();
	            parentNamespace->nmems.push_back(nm);
	        }
		} else {

            if(SgProject::get_verbose() > 5) {
                std::cerr << "Already processed this routine: " << dec->get_mangled_name().getString() << std::endl;    
            }

            parentRoutine = routineMap[dec->get_mangled_name().getString()]; 
            ROSE_ASSERT(parentRoutine != NULL);

			// If this is a defining declaration, reset rpos with the definition's position
			if(def != NULL) {
				Sg_File_Info * decStart = dec->get_startOfConstruct();
		        Sg_File_Info * decEnd = dec->get_endOfConstruct();
				SgBasicBlock * body = def->get_body();
				Sg_File_Info * bodyStart = body->get_startOfConstruct();
		        Sg_File_Info * bodyEnd = body->get_endOfConstruct();

                if(parentRoutine->rpos_rtype != NULL) {
                    delete parentRoutine->rpos_rtype;
                }
				parentRoutine->rpos_rtype = new SourceLocation(decStart);

                if(parentRoutine->rloc != NULL) {
                    delete parentRoutine->rloc;
                }
                parentRoutine->rloc = new SourceLocation(decStart);

                if(parentRoutine->rpos_endDecl != NULL) {
                    delete parentRoutine->rpos_endDecl;
                }
				parentRoutine->rpos_endDecl = new SourceLocation(decEnd);
                
                if(parentRoutine->rpos_startBlock != NULL) {
                    delete parentRoutine->rpos_startBlock;
                }
                parentRoutine->rpos_startBlock = new SourceLocation(bodyStart);
                
                if(parentRoutine->rpos_endBlock != NULL) {
                    delete parentRoutine->rpos_endBlock;
                }
				parentRoutine->rpos_endBlock = new SourceLocation(bodyEnd);
			
                parentRoutine->node = def;
            }
		
			// body of function
			if(parentRoutine->rbody < 0 && def != NULL) {
				SgBasicBlock * body = def->get_body();
				Sg_File_Info * bodyStart = body->get_startOfConstruct();
		        Sg_File_Info * bodyEnd = body->get_endOfConstruct();
				if(lang != LANG_FORTRAN) {
					Statement * stmt = new Statement(parentRoutine->stmtId++, def, Statement::STMT_BLOCK);
					stmt->start = new SourceLocation(bodyStart);
					stmt->end = new SourceLocation(bodyEnd);
		            const SgStatementPtrList & l = body->get_statements();
		            if(l.size() > 0) {
		                stmt->downSgStmt = l.front();
		            }
					parentRoutine->rstmts.push_back(stmt);
					parentRoutine->rbody = stmt->id;
				} else {
					// FORTRAN doesn't use blocks
					parentRoutine->rbody = 0;
					parentRoutine->rstart = NULL;
				}
			}
        }

    // *** STATEMENTS ***    
	} else if(templateFunctionDefinition == NULL && isSgStatement(n)) {
        if(parentRoutine != NULL) {
            Statement * stmt = new Statement(-1, isSgStatement(n)); 
            stmt->depth = inheritedAttribute.depth;
            stmt->start = new SourceLocation(s);
            stmt->end = new SourceLocation(e);
			stmt->fortran = (lang == LANG_FORTRAN);

            // DECL
            SgVariableDeclaration * varDec = isSgVariableDeclaration(n);

            // VARIABLE DECLARATION (DECL)
            if(varDec != NULL) {
				if(lang != LANG_FORTRAN) {
                	stmt->kind = Statement::STMT_DECL;
				} else {
					// Don't care about variable declarations in FORTRAN
					stmt->kind = Statement::STMT_IGNORE;
				}
				const SgInitializedNamePtrList & varList = varDec->get_variables();
				for(SgInitializedNamePtrList::const_iterator j = varList.begin(); j != varList.end(); j++) {
					SgInitializedName * namedVar = (*j);
					if(namedVar != NULL) {
						SgType * varType = namedVar->get_type();
						if(varType != NULL) {
							handleType(varType, parentNamespace);
						} else {
								std::cerr << "WARNING: Declared variable had null type" << std::endl;
						}
					} else {
							std::cerr << "WARNING: Declared variable was null" << std::endl;
					}
				}
	           

            // PARAMETER LIST (ignore)
            } else if(isSgFunctionParameterList(n))  {
                // SgFunctionParameterLists are technically declarations, but we already
                // handled this when we handled functions, so we want to ignore it now.
                stmt->kind = Statement::STMT_IGNORE;

			// Don't care about initializer lists, since those don't show up as statements.
			} else if(isSgCtorInitializerList(n)) {
				stmt->kind = Statement::STMT_IGNORE;
			
            
            // EXPRESSION STATEMENT (plain EXPR, and EMPTY, THROW and ASSIGN)
            } else if(isSgExprStatement(n)) {
                SgExprStatement * exprStmt = isSgExprStatement(n);
                SgExpression * cExpr = exprStmt->get_expression();

				if(cExpr == NULL) {
						std::cerr << "WARNING: Expression inside expression statement was null" << std::endl;
				} else {
					SgType * exprType = cExpr->get_type();
					if(exprType == NULL) {
							std::cerr << "WARNING: Expression had null type" << std::endl;
					} else {
						handleType(exprType, parentNamespace);
					}
				}
				
                // ASSIGN
				// Use this utility method from SageInterface instead of a class
				// type check because the various assignment operators aren't
				// subclasses of SgAssignOp, even though it seems like they
				// should be.
                if(SageInterface::isAssignmentStatement(cExpr)) {
                  	stmt->kind = Statement::STMT_ASSIGN;

				// FORTRAN ASSIGN
				// In FORTRAN, we have to deal with different types of assignment:
				// fassign, fpointerassign, and flabelassign.
				// However, ROSE does not support label assignment.
				// If you try, you get:
				// "Assign statement not implemented (very old langauge feature)"
				} else if(isSgPointerAssignOp(cExpr)) {
					stmt->kind = Statement::STMT_FPOINTERASSIGN;
			
                // EMPTY
                } else if (isSgNullExpression(cExpr)) {
                    stmt->kind = Statement::STMT_EMPTY;

                // THROW
                } else if(isSgThrowOp(cExpr)) {
                    stmt->kind = Statement::STMT_THROW;

                // FUNCTION CALL
                } else if(isSgFunctionCallExp(cExpr)) {
                    // These are handled as rcalls, not rstmts, in C/C++.
                    // In FORTRAN, these are regular statements.
					if(lang == LANG_FORTRAN) {
						stmt->kind = Statement::STMT_FCALL;
					} else {
                        stmt->kind = Statement::STMT_EXPR;
                    }

                // EXPR    
                } else {
					if(lang != LANG_FORTRAN) {
                    	stmt->kind = Statement::STMT_EXPR;
					} else {
						// PDB doesn't include non-assignment exprs in
						// FORTRAN output
						stmt->kind = Statement::STMT_IGNORE;
					}
                }

            // BLOCK
            } else if(isSgBasicBlock(n)) {
                if(inheritedAttribute.routine != NULL && inheritedAttribute.routine->node != NULL 
                        && n == inheritedAttribute.routine->node->get_body()) {
                    //Skipping basic block because it is the function body (already processed)
                    stmt->kind = Statement::STMT_IGNORE;
                } else if(switchCase != NULL) {
                    // Skipping because ROSE considers each group of statements in a switch
                    // to be a block, whereas PDT does not.
                    stmt->kind = Statement::STMT_IGNORE;
				} else if(lang == LANG_FORTRAN) {
					// Skipping because PDB doesn't have block entries for
					// FORTRAN code.
					stmt->kind = Statement::STMT_IGNORE;
				
                } else {
                    stmt->kind = Statement::STMT_BLOCK;    
                    SgBasicBlock * blk = isSgBasicBlock(n);
                    const SgStatementPtrList & l = blk->get_statements();
                    if(l.size() > 0) {
                        stmt->downSgStmt = l[0];
                    }
                }

            // RETURN
            } else if(isSgReturnStmt(n)) {
                stmt->kind = Statement::STMT_RETURN;
                  // Attempted workaround for missing return statements
//                if(stmt->end->fileId < 0) {
//                    SgStatement * prev = SageInterface::getPreviousStatement(isSgReturnStmt(n));
//                    if(prev != NULL) {
//                        Sg_File_Info * prevEnd = prev->get_endOfConstruct();
//                        stmt->start->fileId = prevEnd->get_file_id();
//                        if(stmt->start->fileId <= 0 && parentRoutine != NULL && parentRoutine->rpos_endBlock != NULL) {
//                            stmt->start->fileId = parentRoutine->rpos_endBlock->fileId;    
//                        } else {
//                            stmt->start->cgen = true;
//                        }
//                        stmt->start->line = prevEnd->get_raw_line();
//                        stmt->start->column = prevEnd->get_raw_col() + 1;
//                    } else {
//                        stmt->start->cgen = true;
//                    }
//                    if(parentRoutine != NULL && parentRoutine->rpos_endBlock != NULL) {
//                        stmt->end->fileId = parentRoutine->rpos_endBlock->fileId;
//                        stmt->end->line = parentRoutine->rpos_endBlock->line;
//                        stmt->end->column = parentRoutine->rpos_endBlock->column - 1;
//                    } else {
//                        stmt->end->cgen = true;
//                    }
//                }
            


            // FOR
            } else if(isSgForStatement(n)) {
                SgForStatement * forStmt = isSgForStatement(n);
                stmt->kind = Statement::STMT_FOR;
                // DOWN should point to the body of the loop.
                stmt->downSgStmt = forStmt->get_loop_body();
				if(lang == LANG_FORTRAN && isSgBasicBlock(stmt->downSgStmt)) {
					const SgStatementPtrList & bodyStmts = isSgBasicBlock(stmt->downSgStmt)->get_statements();
					if(!bodyStmts.empty()) {
						stmt->downSgStmt = bodyStmts.front();
					}
				}
                // EXTRA should point to the initializer.
                stmt->extraSgStmt = forStmt->get_for_init_stmt();
				if(lang == LANG_FORTRAN && isSgBasicBlock(stmt->extraSgStmt)) {
					const SgStatementPtrList & bodyStmts = isSgBasicBlock(stmt->extraSgStmt)->get_statements();
					if(!bodyStmts.empty()) {
						stmt->extraSgStmt = bodyStmts.front();
					}
				}

            
            // UPC FORALL
            } else if(isSgUpcForAllStatement(n)) {
                SgUpcForAllStatement * forStmt = isSgUpcForAllStatement(n);
                stmt->kind = Statement::STMT_UPC_FORALL;
                // DOWN should point to the body of the loop.
                stmt->downSgStmt = forStmt->get_loop_body();
                // EXTRA should point to the initializer.
                stmt->extraSgStmt = forStmt->get_for_init_stmt();
                // AFFINITY should point to the affinity expression.
                // Affinity is a bare expression, not an expression statement,
                // so it will ordinarily not have an entry in the PDB file;
                // we generate one for it here.
                stmt->affinitySgExpr = forStmt->get_affinity(); 
                if(stmt->affinitySgExpr != NULL && !isSgNullExpression(stmt->affinitySgExpr)) {
                    Statement * affinityStmt = new Statement(parentRoutine->stmtId++, NULL);
                    affinityStmt->kind = Statement::STMT_EXPR;
                    affinityStmt->start = new SourceLocation(stmt->affinitySgExpr->get_startOfConstruct());
                    affinityStmt->end   = new SourceLocation(stmt->affinitySgExpr->get_endOfConstruct());
                    parentRoutine->rstmts.push_back(affinityStmt);
                    stmt->affinity = affinityStmt->id;
                }
   
            // For initialization statement (treat as BLOCK)
            } else if(isSgForInitStatement(n)) {
                SgForInitStatement * forInit = isSgForInitStatement(n);
                stmt->kind = Statement::STMT_BLOCK;
                stmt->start = NULL;
                stmt->end = NULL;
                const SgStatementPtrList & l = forInit->get_init_stmt();
                if(l.size() > 0) {
                    stmt->downSgStmt = l.front();
                }

            // IF
            } else if(isSgIfStmt(n)) {
                SgIfStmt * ifStmt = isSgIfStmt(n);
                stmt->kind = Statement::STMT_IF;
                // DOWN points to the 'then' clause, EXTRA to the 'else' clause
                stmt->downSgStmt = ifStmt->get_true_body();
				stmt->extraSgStmt = ifStmt->get_false_body();
                
				if(lang == LANG_FORTRAN && isSgBasicBlock(stmt->downSgStmt)) {
					const SgStatementPtrList & bodyStmts = isSgBasicBlock(stmt->downSgStmt)->get_statements();
					if(!bodyStmts.empty()) {
						stmt->downSgStmt = bodyStmts.front();
					}
				}
				if(lang == LANG_FORTRAN && isSgBasicBlock(stmt->extraSgStmt)) {
					const SgStatementPtrList & bodyStmts = isSgBasicBlock(stmt->extraSgStmt)->get_statements();
					if(!bodyStmts.empty()) {
						stmt->extraSgStmt = bodyStmts.front();
					}
				}
				
            
            // SWITCH
            } else if(isSgSwitchStatement(n)) {
                SgSwitchStatement * switchStmt = isSgSwitchStatement(n);
                stmt->kind = Statement::STMT_SWITCH;
                
                // Create a label to point to the statement after the loop body.
                const int labelAfterId = parentRoutine->stmtId++;
                Statement * labelAfter = new Statement(labelAfterId, NULL, Statement::STMT_LABEL);
                if(isSgIfStmt(switchStmt->get_scope())) {
                    // getNextStatement aborts if the parent scope is an If statement.
                    // This only occurs when the body of the If statement is not enclosed
                    // in a block, in which case there is no next statement.
                    labelAfter->nextSgStmt = NULL;
                } else {
                    labelAfter->nextSgStmt = SageInterface::getNextStatement(switchStmt);
                }
                stmt->next = labelAfterId;
                parentRoutine->rstmts.push_back(labelAfter);
                afterSwitch = labelAfter;

                // DOWN points to the block inside the switch statement.
                stmt->downSgStmt = switchStmt->get_body();

            
            // CASE and DEFAULT
            } else if(isSgCaseOptionStmt(n) || isSgDefaultOptionStmt(n)) {
                SgCaseOptionStmt * opt = isSgCaseOptionStmt(n);
                SgDefaultOptionStmt * def = isSgDefaultOptionStmt(n);
                switchCase = stmt;
                stmt->kind = Statement::STMT_CASE;
                SgStatement * optBody = NULL;
                
                if(opt != NULL) {   
                    optBody = opt->get_body();
                } else if(def != NULL) {
                    optBody = def->get_body();
                } else {
                    cerr << "WARNING: Somehow this case or default statement is neither a case nor default statement!" << endl;
                }

                if(optBody != NULL) {
                    SgBasicBlock * optBlk = isSgBasicBlock(optBody);
                    if(optBlk != NULL) {
                        const SgStatementPtrList & blkStmts = optBlk->get_statements();
                        if(blkStmts.size() > 0) {
                            stmt->downSgStmt = blkStmts.front();
                            SgStatement * last = blkStmts.back();
                            if(!isSgBreakStmt(last)) {
                                // If we don't break, we need to insert a LABEL and GOTO
                                SgStatement * nextStmt;
                                if(opt != NULL) {
                                    nextStmt = SageInterface::getNextStatement(opt);  
                                } else if(def != NULL) {
                                    nextStmt = SageInterface::getNextStatement(def);
                                }
                                if(nextStmt != NULL) {
                                    int labelId = parentRoutine->stmtId++;
                                    int gotoId = parentRoutine->stmtId++;
                                    Statement * label = new Statement(labelId, opt);
                                    label->kind = Statement::STMT_LABEL;
                                    label->nextSgStmt = nextStmt;
                                    parentRoutine->rstmts.push_back(label);
                                    Statement * gotoStmt = new Statement(gotoId, opt);
                                    gotoStmt->kind = Statement::STMT_GOTO;
                                    gotoStmt->extra = labelId;
                                    parentRoutine->rstmts.push_back(gotoStmt);
                                    AstAttribute * attr = last->getAttribute(PDT_ATTRIBUTE);
                                    PDTAttribute * pdtAttr = NULL;
                                    if(attr != NULL) {
                                        pdtAttr = dynamic_cast<PDTAttribute *>(attr);
                                    }
                                    if(pdtAttr == NULL) {
                                        pdtAttr = new PDTAttribute();
                                        last->setAttribute(PDT_ATTRIBUTE, pdtAttr); 
                                    }
                                    pdtAttr->gotoStmt = gotoStmt;
                                } else if(def != NULL) {
                                    // If the last statement of a DEFAULT case is not a break, we
                                    // need to insert one.
                                    int breakId = parentRoutine->stmtId++;
                                    Statement * breakStmt = new Statement(breakId, def);
                                    breakStmt->kind = Statement::STMT_LABEL;
                                    if(afterSwitch != NULL) {
                                        breakStmt->extra = afterSwitch->id;
                                    }
                                    parentRoutine->rstmts.push_back(breakStmt);
                                }
                            }
                        }
                    }
                }
            
            // BREAK
            } else if(isSgBreakStmt(n)) {
                stmt->kind = Statement::STMT_BREAK;
                if(switchCase != NULL) {
                    switchCase->extra = parentRoutine->stmtId;
                }
                if(afterSwitch != NULL) {
                    stmt->extra = afterSwitch->id;
                }
                // Use in loops

            // LABEL
            } else if(isSgLabelStatement(n)) {
                stmt->kind = Statement::STMT_LABEL;
                stmt->nextSgStmt = SageInterface::getNextStatement(isSgLabelStatement(n));
                                                  
            // GOTO
            } else if(isSgGotoStatement(n)) {
                stmt->kind = Statement::STMT_GOTO;
                stmt->extraSgStmt = isSgGotoStatement(n)->get_label();
            
            // CONTINUE 
            } else if(isSgContinueStmt(n)) {
                stmt->kind = Statement::STMT_CONTINUE;
                // Extra needs to point to label where we jump to.
            

            // WHILE
            } else if(isSgWhileStmt(n)) {
                stmt->kind = Statement::STMT_WHILE;
                stmt->downSgStmt = isSgWhileStmt(n)->get_body();
            
            // DO-WHILE    
            }  else if(isSgDoWhileStmt(n)) {
                stmt->kind = Statement::STMT_DO;
                stmt->downSgStmt = isSgDoWhileStmt(n)->get_body();
            
            // ASM
            } else if(isSgAsmStmt(n)) {
                stmt->kind = Statement::STMT_ASM;
            
            // TRY
            } else if(isSgTryStmt(n)) {
                SgTryStmt * tryStmt = isSgTryStmt(n);
                stmt->kind = Statement::STMT_TRY;

                // EXTRA points to first catch statement
                SgCatchStatementSeq * seq = tryStmt->get_catch_statement_seq_root();
                const SgStatementPtrList & ptrList = seq->get_catch_statement_seq();
                if(ptrList.size() > 0) {
                    stmt->extraSgStmt = ptrList.front();
                }
                stmt->extraSgStmt = tryStmt->get_catch_statement_seq_root();
                
                // DOWN points to body of the try statement
                stmt->downSgStmt = tryStmt->get_body();

            // CATCH SEQUENCE
            // We're only interested in the catch blocks inside this.
            } else if(isSgCatchStatementSeq(n)) {
                stmt->kind = Statement::STMT_IGNORE;
            
            // CATCH
            } else if(isSgCatchOptionStmt(n)) {
                SgCatchOptionStmt * catchStmt = isSgCatchOptionStmt(n);
                stmt->kind = Statement::STMT_CATCH;
                stmt->downSgStmt = catchStmt->get_body();
                // In a PDB file, a catch statement's source location is the end of its block,
                // not the end of the statement.
                delete stmt->end;
                stmt->end = new SourceLocation(catchStmt->get_body()->get_endOfConstruct());

                // NEXT needs to point to the next CATCH statement, if there is one.
                SgTryStmt * tryStmt = catchStmt->get_trystmt();
                if(tryStmt != NULL) {
                    SgCatchStatementSeq * stmtSeq = tryStmt->get_catch_statement_seq_root();
                    if(stmtSeq != NULL) {
                        const SgStatementPtrList & ptrList = stmtSeq->get_catch_statement_seq();
                        const SgStatementPtrList::const_iterator & begin = ptrList.begin();
                        const SgStatementPtrList::const_iterator & end = ptrList.end();
                        SgStatementPtrList::const_iterator found = std::find(begin, end, catchStmt);
                        ++found;
                        if(found != end) {
                            stmt->nextSgStmt = (*found);
                        }
                    }
                }
            
			// FORTRAN ALLOCATE 
			} else if(isSgAllocateStatement(n)) {
				stmt->kind = Statement::STMT_FALLOCATE;
				
			// FORTRAN DEALLOCATE
			} else if(isSgDeallocateStatement(n)) {
				stmt->kind = Statement::STMT_FDEALLOCATE;

			// FORTRAN DO STATEMENT
			} else if(isSgFortranDo(n)) {
				stmt->kind = Statement::STMT_DO;
				const SgStatementPtrList & bodyStmts = isSgFortranDo(n)->get_body()->get_statements();
				if(!bodyStmts.empty()) {
					stmt->downSgStmt = bodyStmts.front();
				}
			
			// FORTRAN IO STATEMENTS
			// Thankfully these all are subclasses of a common parent.
			} else if(isSgIOStatement(n)) {
				stmt->kind = Statement::STMT_FIO;
			
			// FORTRAN STOP/PAUSE
			} else if(isSgStopOrPauseStatement(n)) {
				switch(isSgStopOrPauseStatement(n)->get_stop_or_pause()) {
					case SgStopOrPauseStatement::e_unknown: 
							std::cerr << "WARNING: Unknown stop/pause type" << std::endl;
						break;
					case SgStopOrPauseStatement::e_stop:
						stmt->kind = Statement::STMT_FSTOP;
						break;
					case SgStopOrPauseStatement::e_pause:
						stmt->kind = Statement::STMT_FPAUSE;
						break;
					default:
							std::cerr << "WARNING: Unrecognized stop/pause type" << std::endl;
				}
			
			// FORTRAN ARITHMETIC IF
			} else if(isSgArithmeticIfStatement(n)) {
				stmt->kind = Statement::STMT_FARITHIF;
				
			// FORTRAN WHERE/ELSEWHERE
			} else if(isSgWhereStatement(n)) {
				stmt->kind = Statement::STMT_FWHERE;
				SgWhereStatement * where = isSgWhereStatement(n);
				SgBasicBlock * downBlock = where->get_body();
				if(downBlock != NULL) {
					const SgStatementPtrList & bodyStmts = downBlock->get_statements();
					if(!bodyStmts.empty()) {
						stmt->downSgStmt = bodyStmts.front();
					}
				}
				
				SgElseWhereStatement * elsewhere = where->get_elsewhere();
				if(elsewhere != NULL) {
					SgBasicBlock * elseBlock = elsewhere->get_body();
					if(elseBlock != NULL) {
						const SgStatementPtrList & bodyStmts = elseBlock->get_statements();
						if(!bodyStmts.empty()) {
							stmt->extraSgStmt = bodyStmts.front();
						}
					}
				}
			
			// FORTRAN FORALL STATEMENT	
			} else if(isSgForAllStatement(n)) {
				stmt->kind = Statement::STMT_FFORALL;
				const SgStatementPtrList & bodyStmts = isSgForAllStatement(n)->get_body()->get_statements();
				if(!bodyStmts.empty()) {
					stmt->downSgStmt = bodyStmts.front();
				}
			
			// FORTRAN ENTRY STATEMENT
			} else if(isSgEntryStatement(n)) {
				stmt->kind = Statement::STMT_FENTRY;
			
			
            // UPC BARRIER
            } else if(isSgUpcBarrierStatement(n)) {
                stmt->kind = Statement::STMT_UPC_BARRIER;
                SgExpression * expr = isSgUpcBarrierStatement(n)->get_barrier_expression();
                if(expr != NULL) {
                    if(stmt->end != NULL) {
                        delete stmt->end;
                    }
                    stmt->end = new SourceLocation(expr->get_endOfConstruct());
                    // We want the end column to point at the semicolon.
                    ++stmt->end->column;
                } else if (stmt->start->line == stmt->end->line && stmt->start->column == stmt->end->column){
                    // Work around a bug in ROSE where upc_barrier has the wrong end location.
                    std::string barrierStr = n->unparseToString();
                    size_t found = barrierStr.find(";");
                    if(found != std::string::npos) {
                        stmt->end->column += found - 1;
                    }
                }
                
                
            
            // UPC FENCE
            } else if(isSgUpcFenceStatement(n)) {
                stmt->kind = Statement::STMT_UPC_FENCE;
                if (stmt->start->line == stmt->end->line && stmt->start->column == stmt->end->column){
                    // Work around a bug in ROSE where upc_fence has the wrong end location.
                    std::string barrierStr = n->unparseToString();
                    size_t found = barrierStr.find(";");
                    if(found != std::string::npos) {
                        stmt->end->column += found - 1;
                    }
                }

            // UPC NOTIFY
            } else if(isSgUpcNotifyStatement(n)) {
                stmt->kind = Statement::STMT_UPC_NOTIFY;
                SgExpression * expr = isSgUpcNotifyStatement(n)->get_notify_expression();
                if(expr != NULL) {
                    if(stmt->end != NULL) {
                        delete stmt->end;
                    }
                    stmt->end = new SourceLocation(expr->get_endOfConstruct());
                    // We want the end column to point at the semicolon.
                    ++stmt->end->column;
                }
                if (stmt->start->line == stmt->end->line && stmt->start->column == stmt->end->column){
                    // Work around a bug in ROSE where upc_barrier has the wrong end location.
                    std::string barrierStr = n->unparseToString();
                    size_t found = barrierStr.find(";");
                    if(found != std::string::npos) {
                        stmt->end->column += found - 1;
                    }
                }

            // UPC WAIT    
            } else if(isSgUpcWaitStatement(n)) {
                stmt->kind = Statement::STMT_UPC_WAIT;
                SgExpression * expr = isSgUpcWaitStatement(n)->get_wait_expression();
                if(expr != NULL) {
                    if(stmt->end != NULL) {
                        delete stmt->end;
                    }
                    stmt->end = new SourceLocation(expr->get_endOfConstruct());
                    // We want the end column to point at the semicolon.
                    ++stmt->end->column;
                }
                if (stmt->start->line == stmt->end->line && stmt->start->column == stmt->end->column){
                    // Work around a bug in ROSE where upc_barrier has the wrong end location.
                    std::string barrierStr = n->unparseToString();
                    size_t found = barrierStr.find(";");
                    if(found != std::string::npos) {
                        stmt->end->column += found - 1;
                    }
                }

            // EMPTY
            } else if(isSgNullStatement(n)) {
                stmt->kind = Statement::STMT_EMPTY;
                // Due to a bug in ROSE, the end of empty statements are not correct.
                // (They indicate an end location of (0,0))
                if(stmt->end != NULL) {
                    delete stmt->end;
                }
                stmt->end = new SourceLocation(n->get_startOfConstruct());

			// PRAGMA
			// Despite being preprocessor directives, these are statements
			// in ROSE, whereas other preprocessor directives are not
			// in the AST but are attached to AST nodes.
			} else if(isSgPragmaDeclaration(n)) {
					SgPragmaDeclaration * pragDecl = isSgPragmaDeclaration(n);
					SgPragma * pragma = pragDecl->get_pragma();
					if(pragma != NULL) {
						Pragma * p = new Pragma(nextPragmaID++, new SourceLocation(pragma->get_startOfConstruct()),
											    new SourceLocation(pragDecl->get_startOfConstruct()),
												new SourceLocation(pragDecl->get_endOfConstruct()));
						std::string pragText = pragma->get_pragma();
						boost::algorithm::replace_all(pragText, "\\\n", " ");
						boost::algorithm::replace_all(pragText, "\n", " ");
						p->ptext = pragText;
						pragmas.push_back(p);
					}
				}
			

            if(stmt->kind != Statement::STMT_NONE && stmt->kind != Statement::STMT_IGNORE) {
                
                // We haven't given the next statement an ID yet, so save a pointer
                // to the next statement so that we can go back and do so once
                // we've processed the whole tree.
                SgStatement * ss = isSgStatement(n);
                // Trying to find the next statement of one of the types in the if statement
                // results, unhelpfully, in an assert(0) instead of an exception, so we'd best
                // avoid those cases.
                if(stmt->nextSgStmt ==  NULL && isSgBasicBlock(ss->get_scope())
                        && !isSgForInitStatement(ss) && !isSgBasicBlock(ss)
                        && !isSgClassDefinition(ss) && !isSgFunctionDefinition(ss) 
                        && !isSgFunctionParameterList(ss) && !isSgCatchOptionStmt(ss)) {
                    stmt->nextSgStmt = SageInterface::getNextStatement(ss);
					// FORTRAN doesn't use blocks, so if we get a block
					// as a next, redirect to the first statement in the block.
					if(lang == LANG_FORTRAN && stmt->nextSgStmt != NULL) {
						SgBasicBlock * block = isSgBasicBlock(stmt->nextSgStmt);
						if(block != NULL) {
							const SgStatementPtrList & bodyStmts = block->get_statements();
							if(!bodyStmts.empty()) {
								stmt->nextSgStmt = bodyStmts.front();
							}
						}
					}
                }
                if(stmt->nextSgStmt == NULL) {
                    AstAttribute * attr = ss->getAttribute(PDT_ATTRIBUTE);
                    if(attr != NULL) {
                        PDTAttribute * pdtAttr = dynamic_cast<PDTAttribute *>(attr);
                        if(pdtAttr != NULL && pdtAttr->gotoStmt != NULL) {
                            stmt->next = pdtAttr->gotoStmt->id;        
                        }
                    }
                }

				// In FORTRAN, the start of a routine is the same as the
				// start of the first statement in the routine.
				if(lang == LANG_FORTRAN && parentRoutine->rstmts.empty()) {
					parentRoutine->rstart = stmt->start;
				}
				
                stmt->id = parentRoutine->stmtId++;
                parentRoutine->rstmts.push_back(stmt);
                
                if(SgProject::get_verbose() > 2) {
                    std::cerr << "Added a statement " << (*stmt) << " for " << n->unparseToString() << std::endl;
                }

                pdtAttr->statement = stmt;
                parentStatement = stmt;
                prevStmt = stmt;

            } else if(stmt->kind == Statement::STMT_IGNORE) {
                delete stmt;
            } else if(SgProject::get_verbose() > 0) {
                std::cerr << "WARNING: Unhandled statement type " << n->class_name() << std::endl;   
            }
        }


        
    // FUNCTION CALLs
    // (Handled as rcalls)
    } else if(templateFunctionDefinition == NULL && isSgFunctionCallExp(n))  {
        if(lang != LANG_FORTRAN) {
            if(parentRoutine == NULL) {
                std::cerr << "BUG: function call without parent routine!" << std::endl;
            } else {                                                            
                SgFunctionCallExp * fcall = isSgFunctionCallExp(n);
                SgFunctionDeclaration * fdecl = fcall->getAssociatedFunctionDeclaration();
                if(fdecl != NULL) {
                    int routineId = -1;
                    if(routineMap.count(fdecl->get_mangled_name().getString()) != 0) {
                        routineId = routineMap[fdecl->get_mangled_name().getString()]->id;
                    }
                    RoutineCall * rc = new RoutineCall();
                    rc->sgRoutine = fdecl->get_definition();
                    rc->loc = new SourceLocation(s);
                    rc->id = routineId;
                    parentRoutine->rcalls.push_back(rc);
                    calls.push_back(rc);
                }
            }
        }
    

    // *** INITIALIZERS (INIT) ***
    // (SgInitializers are not a type of SgStatement, but we want to treat
    // them as statements since they're represented by rstmts.)
    } else if(isSgInitializer(n)) {
        SgAssignInitializer * init = isSgAssignInitializer(n);
        if(init != NULL && parentRoutine != NULL && parentStatement != NULL) {
            Statement * stmt = new Statement(-1, NULL); 
            stmt->depth = inheritedAttribute.depth;
            stmt->start = new SourceLocation(s);
            stmt->end = new SourceLocation(e);
            stmt->kind = Statement::STMT_INIT;

            stmt->id = parentRoutine->stmtId++;

            parentRoutine->rstmts.push_back(stmt);
            pdtAttr->statement = parentStatement;
            parentStatement = stmt;
            prevStmt = stmt;
        } else if (parentEnum != NULL) {
			// ignore; we handle enums separately
		} else {
            //cerr << "WARNING: Unhandled initializer type " << n->class_name() << endl;
            // 			cerr << n->unparseToString() << endl;
        }
    
    // *** CLASSES (GROUPS) ***
    } 
    if(isSgClassDeclaration(n)) {
        SgClassDeclaration * classDec = isSgClassDeclaration(n);
        SgClassDefinition * classDef = classDec->get_definition();
        if(classDec != NULL) {

			// Don't reprocess a group we've already seen before
			// (e.g. forward declaration)
			Group * group = NULL;
			if(groupMap.count(classDec->get_mangled_name().getString()) == 0) {
				// Passing in parentNamespace will set ynspace in the generated type.
	            TypeID tid = handleType(classDec->get_type(), parentNamespace, true);
	            group = new Group(tid.id, classDec->get_name().getString(), new SourceLocation(s));
	            groups.push_back(group);
	            groupMap[classDec->get_mangled_name().getString()] = group;
			} else {
				group = groupMap[classDec->get_mangled_name().getString()];
			}

            if(parentGroup != NULL) {
                group->ggroup = parentGroup->id;
            }
            
            if(parentNamespace != NULL) {
				group->gnspace = parentNamespace->id;
	            NamespaceMember * nm = new NamespaceMember(group->id, NamespaceMember::NS_GROUP);
				parentNamespace->nmems.push_back(nm);
            }

            parentGroup = group;
            pdtAttr->group = group;

            if(classDef != NULL) {
                classDef->setAttribute(PDT_ATTRIBUTE, pdtAttr);    
            }

            // gkind
            switch(classDec->get_class_type()) {
                case SgClassDeclaration::e_class: 
                    group->gkind = Group::GKIND_CLASS; break;
                case SgClassDeclaration::e_struct:
                    group->gkind = Group::GKIND_STRUCT; break;
                case SgClassDeclaration::e_union:
                    group->gkind = Group::GKIND_UNION; break;
                case SgClassDeclaration::e_template_parameter:
                    group->gkind = Group::GKIND_TPROTO; break;
                default:
                    group->gkind = Group::GKIND_NA;
            }

			// Is this group a template instantiation?
			if(isSgTemplateInstantiationDecl(classDec)) {
				SgTemplateInstantiationDecl * instDec = isSgTemplateInstantiationDecl(classDec);
				SgTemplateClassDeclaration * templDec = instDec->get_templateDeclaration();
				if(templDec != NULL) {
					std::string mangledTemplName = templDec->get_mangled_name().getString();
					if(templateMap.count(mangledTemplName) != 0) {
						group->gtempl = templateMap[mangledTemplName]->id;
					}
					// gsparam
				}
			}
			
			if(isSgModuleStatement(classDec)) {
				group->gkind = Group::GKIND_FMODULE;
			} else if(isSgDerivedTypeStatement(classDec)) {
				group->gkind = Group::GKIND_FDERIVED;
			} 
			

            // gbase
            if(classDef != NULL) {
                const SgBaseClassPtrList & ptrList = classDef->get_inheritances();
                for(SgBaseClassPtrList::const_iterator it = ptrList.begin(); it != ptrList.end(); ++it) {
                    SgBaseClass * base = *it;
                    const SgBaseClassModifier & baseMod = base->get_baseClassModifier();
                    const SgAccessModifier & accMod = baseMod.get_accessModifier();
                    BaseGroup * baseGroup = new BaseGroup();
                    baseGroup->virt = baseMod.isVirtual();
                    baseGroup->pub = accMod.isPublic();
                    baseGroup->prot = accMod.isProtected();
                    baseGroup->priv = accMod.isPrivate();
                    baseGroup->sgClass = base->get_base_class();
                    baseGroup->name = baseGroup->sgClass->get_mangled_name().getString();
                    group->gbases.push_back(baseGroup); 
                }
            }

            
            // Class Members
            if(classDef != NULL) {
                const SgDeclarationStatementPtrList & declStmts = classDef->get_members();
                for(SgDeclarationStatementPtrList::const_iterator it = declStmts.begin(); it != declStmts.end(); ++it) {
                    SgDeclarationStatement * memDecl = *it;
                    const SgDeclarationModifier & memDeclMod = memDecl->get_declarationModifier();
                    const SgTypeModifier & memTypeMod = memDeclMod.get_typeModifier();
                    const SgAccessModifier & memAccMod = memDeclMod.get_accessModifier();
                    const SgStorageModifier & memStorMod = memDeclMod.get_storageModifier();
                    
                    // gfunc & gfrfunc
                    if(isSgFunctionDeclaration(memDecl)) {
                        SgFunctionDeclaration * memFunDecl = isSgFunctionDeclaration(memDecl);
                        MemberFunction * memFun = new MemberFunction();
                        memFun->loc = new SourceLocation(memFunDecl->get_startOfConstruct());
                        memFun->name = memFunDecl->get_mangled_name().getString();
                        if(memDeclMod.isFriend()) {
                            group->gfrfuncs.push_back(memFun);
                        } else {
                            group->gfuncs.push_back(memFun);
                        }
                    
                    // gfrgroup
                    } else if(isSgClassDeclaration(memDecl)) {
                        SgClassDeclaration * friendClass = isSgClassDeclaration(memDecl);
                        BaseGroup * gfrgroup = new BaseGroup();
                        gfrgroup->sgClass = friendClass;
                        gfrgroup->name = friendClass->get_mangled_name().getString();
                        gfrgroup->loc = new SourceLocation(friendClass->get_startOfConstruct());
                        group->gfrgroups.push_back(gfrgroup);  

                    } else if(isSgUsingDeclarationStatement(memDecl)) {
                        if(SgProject::get_verbose() > 5) {
                            std::cerr << "Skipping using declaration as class member: " << memDecl->unparseToString() << std::endl;
                        }

                    // gmem (data member)
                    } else {
                        Member * member = new Member(SageInterface::get_name(memDecl), new SourceLocation(memDecl->get_startOfConstruct()));

                        if(SgProject::get_verbose() > 5) {
                            std::cerr << "Adding class member: " << member->name << " " << memDecl->class_name() << std::endl;
                        }

                        // access modifier
                        if(memAccMod.isPublic()) {
                            member->gmacs = Member::GMACS_PUB;
                        } else if(memAccMod.isProtected()) {
                            member->gmacs = Member::GMACS_PROT;
                        } else if(memAccMod.isPrivate()) {
                            member->gmacs = Member::GMACS_PRIV;
                        }

                        // type members
                        // typedef
                        if(isSgTypedefDeclaration(memDecl)) {
                            member->gmkind = Member::GMKIND_TYPE;
                            TypeID t = handleType(isSgTypedefDeclaration(memDecl)->get_type(), parentNamespace);
                            member->gmtype = t.id;
                            member->gmtype_group = t.group;

                        // enum
                        } else if(isSgEnumDeclaration(memDecl)) {
                            member->gmkind = Member::GMKIND_TYPE;
                            TypeID t = handleType(isSgEnumDeclaration(memDecl)->get_type(), parentNamespace);
                            member->gmtype = t.id;
                            member->gmtype_group = t.group;

                        // template member
                        } else if(isSgTemplateDeclaration(memDecl)) {
                            member->gmkind = Member::GMKIND_TEMPL;
                            std::string mangledName = isSgTemplateDeclaration(memDecl)->get_mangled_name().getString();
                            if(templateMap.count(mangledName) > 0) {
                                member->gmtempl = templateMap[mangledName]->id;
                            } else {
                                Template * templ = handleTemplate(isSgTemplateDeclaration(memDecl), parentNamespace);
                                member->gmtempl = templ->id;
                            }
                        // static var member
                        } else if(memStorMod.isStatic()) {
                            member->gmkind = Member::GMKIND_STATVAR;
                        // var member
                        } else {
                            member->gmkind = Member::GMKIND_VAR;
                        }

                        if(memTypeMod.get_constVolatileModifier().isConst()) {
                            member->gmconst = true;
                        }

                        if(member->gmkind != Member::GMKIND_TEMPL && isSgVariableDeclaration(memDecl)) {
                            SgVariableDeclaration * varDecl = isSgVariableDeclaration(memDecl);
                            const SgInitializedNamePtrList & ptrList = varDecl->get_variables();
                            if(ptrList.size() > 0) {
                                SgInitializedName * name = ptrList[0];
                                TypeID t = handleType(name->get_type(), parentNamespace);
                                member->name = name->get_name();
                                member->gmtype = t.id;
                                member->gmtype_group = t.group;
                                SgVariableDefinition * varDefn = varDecl->get_definition(name);
                                if(varDefn != NULL) {
                                    SgUnsignedLongVal * bitfield = varDefn->get_bitfield();
                                    if(bitfield != NULL && bitfield->get_value() > 0) {
                                        member->gmisbit = true;
                                    }
                                }
                            } else {
	                                std::cerr << "WARNING: Variable declaration had no variables." << std::endl;    
                            }
                        }
                        group->gmems.push_back(member);
                    }
                }
                
            }
        }
    } // end groups

    // NAMESPACES
    // Note that these ROSE classes are named differently from others:
    // we have SgFunctionDeclaration, SgClassDeclaration, etc.,
    // but SgNamespaceDeclarationStatement for namespaces.
    if(isSgNamespaceDeclarationStatement(n)) {
        SgNamespaceDeclarationStatement *nsDecl = isSgNamespaceDeclarationStatement(n);
        //SgNamespaceDefinitionStatement *nsDefn = nsDecl->get_definition();
        
        Namespace * ns = NULL;
        const std::string mangledName = nsDecl->get_mangled_name().getString();
        if(namespaceMap.count(mangledName) > 0) {
            ns = namespaceMap[mangledName];
        } else {
            ns = new Namespace(nextNamespaceID++, nsDecl->get_name().getString());
            namespaces.push_back(ns);
            namespaceMap[nsDecl->get_mangled_name().getString()] = ns;
            ns->nloc = new SourceLocation(nsDecl->get_startOfConstruct());
            ns->ns_tokenEnd = new SourceLocation(nsDecl->get_startOfConstruct());
            ns->ns_blockEnd = new SourceLocation(nsDecl->get_endOfConstruct());
            ns->nsSgStmt = nsDecl;

            if(parentNamespace != NULL) {
                ns->nnspace = parentNamespace->id;       
                NamespaceMember * nm = new NamespaceMember(ns->id, NamespaceMember::NS_NS);
                nm->name = nsDecl->get_mangled_name().getString();
                parentNamespace->nmems.push_back(nm);
            }
        }

        parentNamespace = ns;
        
    } // end namespace
	
	// ROSE handles namespace aliases separately from aliases, and an SgNamespaceAliasDeclarationStatement
	// is not a subclass of SgNamespaceDeclarationStatement, nor vice-versa.
	if(isSgNamespaceAliasDeclarationStatement(n)) {
		SgNamespaceAliasDeclarationStatement *nsAliasDecl = isSgNamespaceAliasDeclarationStatement(n);
		// Namespaces have qualified names in ROSE, but namespace aliases don't for some reason.
		Namespace * ns = new Namespace(nextNamespaceID++, nsAliasDecl->get_name().getString());
        namespaces.push_back(ns);
        namespaceMap[nsAliasDecl->get_mangled_name().getString()] = ns;
		ns->nloc = new SourceLocation(nsAliasDecl->get_startOfConstruct());
        ns->ns_tokenEnd = new SourceLocation(nsAliasDecl->get_startOfConstruct());
        ns->ns_blockEnd = new SourceLocation(nsAliasDecl->get_endOfConstruct());
        ns->nsAliasSgStmt = nsAliasDecl;
		if(parentNamespace != NULL) {
            ns->nnspace = parentNamespace->id;       
            NamespaceMember * nm = new NamespaceMember(ns->id, NamespaceMember::NS_NS);
            nm->name = nsAliasDecl->get_mangled_name().getString();
            parentNamespace->nmems.push_back(nm);
        }
		// Find the target of the alias.
		SgNamespaceDeclarationStatement * aliasTarget = nsAliasDecl->get_namespaceDeclaration();
		if(namespaceMap.count(aliasTarget->get_mangled_name().getString()) != 0) {
			Namespace * nsTarget = namespaceMap[aliasTarget->get_mangled_name().getString()];
			ns->nalias = nsTarget->id;
		} else {
				std::cerr << "WARNING: No target found for namespace alias." << std::endl;
		}
	} // end namespace alias
	
	// Enums are types in PDB, but ROSE considers them to be declarations
	// (Technically there is a type as well in ROSE, but you can't extract
	// the names and values of the enum's contents from SgEnumType.)
	if(isSgEnumDeclaration(n)) {
		SgEnumDeclaration * enumDecl = isSgEnumDeclaration(n);
		SgEnumType * enumType = enumDecl->get_type();
		std::string enumName = enumType->get_name();
		std::string mangledName = getUniqueTypeName(enumType);
		
        Type * t = NULL;
		// If we haven't processed this type already, make a TypeID for it.
		if(typeMap.count(mangledName) == 0) {
			int id = nextTypeID++;
			t = new Type(id, enumName);
	        TypeID typeID(id, false, t);
			t->yloc = new SourceLocation(enumDecl->get_startOfConstruct());
			t->ykind = Type::ENUM;
			t->yikind = Type::INT_INT;
            typeMap.insert( std::pair<string,TypeID>(mangledName,typeID) );
            types.push_back(t);
        } else {
            t = typeMap[mangledName].type;
            ROSE_ASSERT( t != NULL);
        }
			
        // The initialized names store the name and value of each entry.
        const SgInitializedNamePtrList & enumerators = enumDecl->get_enumerators();
        int curValue = 0;
        for(SgInitializedNamePtrList::const_iterator j = enumerators.begin(); j != enumerators.end(); j++) {
            SgInitializedName * initName = (*j);
            std::string qualName = initName->get_name().getString();
            SgInitializer * enumInit = initName->get_initializer();
            if(enumInit != NULL) {
                SgAssignInitializer * assignInit = isSgAssignInitializer(enumInit);
                if(assignInit != NULL) {
                    SgExpression * assignExpr = assignInit->get_operand();
                    SgValueExp * valueExpr = isSgValueExp(assignExpr);
                    if(valueExpr != NULL) {
                        switch(valueExpr->variantT()) {
                            case V_SgCharVal:
                            case V_SgUnsignedCharVal:
                            case V_SgShortVal:
                            case V_SgUnsignedShortVal:
                            case V_SgIntVal:
                            case V_SgUnsignedIntVal:
                            case V_SgLongIntVal:
                            case V_SgUnsignedLongVal:
                            case V_SgLongLongIntVal:
                            case V_SgUnsignedLongLongIntVal:
                                curValue = SageInterface::getIntegerConstantValue(valueExpr);
                            default:
                                ; // Do nothing
                        }
                    }
                }
            }
            t->yenums.push_back(new EnumEntry(qualName, curValue++));
        }
        parentEnum = t;
	} // end enum
	
	// TEMPLATES
	if(isSgTemplateDeclaration(n)) {
        parentTemplate = handleTemplate(isSgTemplateDeclaration(n), parentNamespace);
	} 
    
    if(isSgTemplateFunctionDefinition(n)) {
        templateFunctionDefinition = isSgTemplateFunctionDefinition(n);
    } // end templates

    n->setAttribute(PDT_ATTRIBUTE, pdtAttr);
    return InheritedAttribute(inheritedAttribute.depth+1, parentRoutine, parentStatement, switchCase,
            afterSwitch, parentGroup, parentNamespace, parentEnum, parentTemplate, templateFunctionDefinition);
}

// Called on the way back up the tree.
SynthesizedAttribute VisitorTraversal::evaluateSynthesizedAttribute(SgNode * n, InheritedAttribute inheritedAttribute, SubTreeSynthesizedAttributes synthesizedAttributeList) {
    AstAttribute * attr = n->getAttribute(PDT_ATTRIBUTE);
    if(attr != NULL) {
        PDTAttribute * pdtAttr = dynamic_cast<PDTAttribute *>(attr);
        
        // EXTRA for DECL statements (corresponding INIT)
        if(inheritedAttribute.statement != NULL && pdtAttr->statement != NULL && inheritedAttribute.statement->kind == Statement::STMT_INIT && pdtAttr->statement->kind == Statement::STMT_DECL) {
            pdtAttr->statement->extra = inheritedAttribute.statement->id;
        }

    }
    return SynthesizedAttribute();
 }

inline std::string generatePDBFileName(SgFile * f) {
    const std::string & fileName = f->get_file_info()->get_filenameString();
    const std::string & baseName = StringUtility::stripPathFromFileName(fileName);
    const std::string & noExt = StringUtility::stripFileSuffixFromFileName(baseName);
    return noExt + ".pdb";
}

bool isVoidType(SgType* t) {
    if (isSgTypeVoid(t))
        return true;
    if (isSgTypedefType(t))
        return isVoidType(isSgTypedefType(t)->get_base_type());
    if (isSgModifierType(t)) {
        return isVoidType(isSgModifierType(t)->get_base_type());
    }
    return false;
}

void insertMissingReturns(SgProject * project) {
    if(project->get_C_only() || project->get_C99_only() || project->get_Cxx_only()) {
        Rose_STL_Container<SgNode*> defns = NodeQuery::querySubTree(project, V_SgFunctionDefinition);
        BOOST_FOREACH(SgNode * node, defns) {
            SgFunctionDefinition * defn = isSgFunctionDefinition(node);
            ROSE_ASSERT(defn != NULL);
            SgFunctionDeclaration * decl = defn->get_declaration();
            ROSE_ASSERT(decl != NULL);
            SgFunctionType * type = decl->get_type();
            ROSE_ASSERT(type != NULL);
            SgType * retType = type->get_return_type();
            ROSE_ASSERT(retType != NULL);
            if(isVoidType(retType)) {
                SgStatement * lastStmt = SageInterface::getLastStatement(defn);
                if(lastStmt != NULL && !isSgReturnStmt(lastStmt)) {
                    SageInterface::insertStatementAfter(lastStmt, SageBuilder::buildReturnStmt());
                }
            }
        }
    }
}


int main ( int argc, char* argv[] ) {
	
	// Parses the input files and generates the AST
	SgProject* project = frontend(argc,argv);
	ROSE_ASSERT (project != NULL);
    AstTests::runAllTests(project);
	
    SgStringList args = project->get_originalCommandLineArgumentList();

    std::string confPath = "./";
    BOOST_FOREACH(string s, args) {
        if( boost::starts_with(s, "-pdtConfDir=") ) {
            confPath = s.substr(12, string::npos);     
            break;
        }
    }
    if( !boost::ends_with(confPath, "/") ) {
        confPath += "/";
    }

    std::string cIncludeName = "rose_c_includes";
    BOOST_FOREACH(string s, args) {
        if( boost::starts_with(s, "-pdtCInc=") ) {
            cIncludeName = s.substr(9, string::npos);
            break;
        }
    }

    std::string cxxIncludeName = "rose_cxx_includes";
    BOOST_FOREACH(string s, args) {
        if( boost::starts_with(s, "-pdtCxxInc=") ) {
            cxxIncludeName = s.substr(11, string::npos);
            break;
        }
    }

    std::string c_includes = confPath + cIncludeName;
    std::string cxx_includes = confPath + cxxIncludeName;

    if( SgProject::get_verbose() > 1 ) {
        std::cerr << "Rose C configuration file: " << c_includes << std::endl;
        std::cerr << "Rose CXX configuration file: " << cxx_includes << std::endl;
    }

    
     Rose_STL_Container<string> Cxx_ConfigIncludeDirs;
     Rose_STL_Container<string> C_ConfigIncludeDirs;

     if(boost::filesystem::exists(c_includes.c_str())) {
        ifstream incfile(c_includes.c_str());
        ROSE_ASSERT(incfile.is_open());
        C_ConfigIncludeDirs.clear();
        string line;
        while( !(getline(incfile, line).eof()) ) {
            if(line != ".") {
                if(boost::starts_with(line, "gcc_HEADERS") || boost::starts_with(line, "g++_HEADERS")) {
                    line = std::string("./include/") + line;
                }
                C_ConfigIncludeDirs.push_back(StringUtility::getAbsolutePathFromRelativePath(line, false));    
            }
            if(SgProject::get_verbose() > 1) {
                std::cerr << "Added C include path from config file: " << line << std::endl;
            }
        } 
        incfile.close();
     }

     if(boost::filesystem::exists(cxx_includes.c_str())) {
        ifstream incfile(cxx_includes.c_str());
        ROSE_ASSERT(incfile.is_open());
        Cxx_ConfigIncludeDirs.clear();
        string line;
        while( !(getline(incfile, line).eof()) ) {
            if( line != "." ) {
                if(boost::starts_with(line, "gcc_HEADERS") || boost::starts_with(line, "g++_HEADERS")) {
                    line = std::string("./include/") + line;
                }
                Cxx_ConfigIncludeDirs.push_back(StringUtility::getAbsolutePathFromRelativePath(line, false));    
            }
            if(SgProject::get_verbose() > 1) {
                std::cerr << "Added CXX include path from config file: " << line << std::endl;
            }
        } 
        incfile.close();
     }

     Rose_STL_Container<string> * sysIncludes = NULL;

	const SgFilePtrList & fileList = project->get_fileList();
    if(fileList.size() <= 0) {
        std::cerr << "ERROR: No input files provided!" << std::endl;
        exit(2);
    }
    std::string outName = project->get_outputFileName();
    if(outName.compare("a.out") == 0) {
        outName = generatePDBFileName(fileList.front());
    }

    fstream outfile;
    outfile.open(outName.c_str(), fstream::out | fstream::trunc);

    // Determine language of the project
	lang = LANG_NONE;
    int version = PDB_VERSION;


	
    if(SageInterface::is_UPC_language()) {
        lang = LANG_UPC;
        version = UPC_PDB_VERSION;
        sysIncludes = &C_ConfigIncludeDirs;
    } else if(project->get_C_only() || project->get_C99_only()) {
		lang = LANG_C;
        sysIncludes = &C_ConfigIncludeDirs;
	} else if(project->get_Cxx_only()) {
		lang = LANG_CPP;
        sysIncludes = &Cxx_ConfigIncludeDirs;
	} else if(project->get_Fortran_only() || project->get_F77_only() || project->get_F90_only()
			  || project->get_F95_only() || project->get_F2003_only()) {
		lang = LANG_FORTRAN;
	} else {
		lang = LANG_MULTI;
        std::cerr << "WARNING: Source language not determined to be UPC, C, C++ or Fortran." << std::endl;
	} // The PDT specification says Java is a possible language, but ROSE
      // doesn't (yet?) support Java.

	
	// Determine which files were part of the project.
	for(SgFilePtrList::const_iterator i = fileList.begin(); i != fileList.end(); ++i) {
		new SourceLocation((*i)->get_file_info());
	}

    //insertMissingReturns(project);
	
	// At the start, we're at depth zero and not contained inside anything.
	InheritedAttribute inheritedAttribute(0, NULL, NULL, NULL );
	VisitorTraversal visitorTraversal;

	// Perform the traversal of the AST
	visitorTraversal.traverse(project, inheritedAttribute);
		

    // *** Print output *** 


    // Start printing PDB formatted output: print version number
	outfile << "<PDB " << version << ".0>\n";
	
    // Print language used
	if(lang != LANG_NONE) {
		outfile << "lang ";
		switch(lang) {
			case LANG_C: outfile << "c"; break;
			case LANG_CPP: outfile << "c++"; break;
            case LANG_C_CPP: outfile << "c_or_c++"; break;
			case LANG_FORTRAN: outfile << "fortran"; break;
			case LANG_JAVA: outfile << "java"; break;
			case LANG_MULTI: outfile << "multi"; break;
            case LANG_UPC:   outfile << "upc"; break;
            default: 
				std::cerr << "WARNING: Unknown language type encountered." << std::endl;
		}
	}
	outfile << "\n\n";
		    
    // Post-processing to make sure we have all the data needed for routines and groups:
    
    // If we saved the next, down or extra statements of a routine for later processing, set them now
    for(std::vector<Routine*>::iterator it = routines.begin(); it != routines.end(); ++it) {
        Routine * r = *it;
        for(std::vector<Statement*>::iterator sit = r->rstmts.begin(); sit != r->rstmts.end(); ++sit) {
           Statement * stmt = *sit;
           if(stmt->next < 0 && stmt->nextSgStmt != NULL) {
                AstAttribute * attr = stmt->nextSgStmt->getAttribute(PDT_ATTRIBUTE);
                if(attr != NULL) {
                    PDTAttribute * pdtAttr = dynamic_cast<PDTAttribute *>(attr);
                    if(pdtAttr != NULL && pdtAttr->statement != NULL) {
                        stmt->next = pdtAttr->statement->id;    
                    }
                } 
           }
           if(stmt->down < 0 && stmt->downSgStmt != NULL) {
                AstAttribute * attr = stmt->downSgStmt->getAttribute(PDT_ATTRIBUTE);
                if(attr != NULL) {
                    PDTAttribute * pdtAttr = dynamic_cast<PDTAttribute *>(attr);
                    if(pdtAttr != NULL && pdtAttr->statement != NULL) {
                        stmt->down = pdtAttr->statement->id;    
                    }
                }
           } 
           if(stmt->extra < 0 && stmt->extraSgStmt != NULL) {
                AstAttribute * attr = stmt->extraSgStmt->getAttribute(PDT_ATTRIBUTE);
                if(attr != NULL) {
                    PDTAttribute * pdtAttr = dynamic_cast<PDTAttribute *>(attr);
                    if(pdtAttr != NULL && pdtAttr->statement != NULL) {
                        stmt->extra = pdtAttr->statement->id;    
                    }
                }
           }
        }
    }

    // Get IDs for called functions
    for(std::vector<RoutineCall*>::iterator it = calls.begin(); it != calls.end(); ++it) {
        RoutineCall * rcall = *it;
        if(rcall->id <= 0 && rcall->sgRoutine != NULL) {
            AstAttribute * attr = rcall->sgRoutine->getAttribute(PDT_ATTRIBUTE);
            if(attr != NULL) {
                PDTAttribute * pdtAttr = dynamic_cast<PDTAttribute *>(attr);
                if(pdtAttr != NULL && pdtAttr->routine != NULL) {
                    rcall->id = pdtAttr->routine->id;
					if(pdtAttr->routine->rvirt != Routine::VIRT_NO) {
						rcall->virt = true;
					}
                }
            }
        }
    }
    
    // Get IDs for everything having to do with groups
    for(std::vector<Group*>::iterator it = groups.begin(); it != groups.end(); ++it) {
        Group * group = *it;
        // Get IDs for base groups.
        for(std::vector<BaseGroup*>::iterator bit = group->gbases.begin(); bit != group->gbases.end(); ++bit) {
            BaseGroup * baseGroup = *bit;
            if(baseGroup != NULL && baseGroup->id <= 0) {
                Group * basePDTGroup = groupMap[baseGroup->name];
                if(basePDTGroup != NULL) {
                    baseGroup->id = basePDTGroup->id;
                }
            }
        }
        // Get IDs for friend groups
        for(std::vector<BaseGroup*>::iterator bit = group->gfrgroups.begin(); bit != group->gfrgroups.end(); ++bit) {
            BaseGroup * baseGroup = *bit;
            if(baseGroup != NULL && baseGroup->id <= 0) {
                Group * basePDTGroup = groupMap[baseGroup->name];
                if(basePDTGroup != NULL) {
                    baseGroup->id = basePDTGroup->id;
                }
            }
        }
        // Get IDs for friend functions
        for(std::vector<MemberFunction*>::iterator bit = group->gfrfuncs.begin(); bit != group->gfrfuncs.end(); ++bit) {
            MemberFunction * memberFunction = *bit;
            if(memberFunction != NULL && memberFunction->id <= 0 && memberFunction->sgFunction != NULL) {
                SgFunctionDefinition * funcDefn = memberFunction->sgFunction->get_definition();
                if(funcDefn != NULL) {
                    AstAttribute * attr = memberFunction->sgFunction->getAttribute(PDT_ATTRIBUTE);
                    if(attr != NULL) {
                        PDTAttribute * pdtAttr = dynamic_cast<PDTAttribute *>(attr);
                        if(pdtAttr != NULL && pdtAttr->routine != NULL) {
                            memberFunction->id = pdtAttr->routine->id;
                        }
                    }   
                }
            }
			if(memberFunction != NULL && memberFunction->id < 0) {
				Routine * routine = routineMap[memberFunction->name];
				if(routine != NULL) {
					memberFunction->id = routine->id;
				}
			}
        }
        // Get IDs for member functions
        for(std::vector<MemberFunction*>::iterator bit = group->gfuncs.begin(); bit != group->gfuncs.end(); ++bit) {
            MemberFunction * memberFunction = *bit;
            if(memberFunction != NULL && memberFunction->id <= 0) {
                Routine * routine = routineMap[memberFunction->name];
                if(routine != NULL) {
                    memberFunction->id = routine->id;
                }
            }
        }
    }
 
    // Fix paths of files to be absolute paths and mark system headers
    for(std::vector<SourceFile*>::iterator it = files.begin(); it != files.end(); ++it) {
        SourceFile * f = (*it);
        f->path = StringUtility::getAbsolutePathFromRelativePath(f->path, false);
        if(sysIncludes != NULL) {
            BOOST_FOREACH(std::string s, *sysIncludes) {
                if(boost::starts_with(f->path, s)) {
                    f->ssys = true;
                    break;
                }
            }
        }

    }

	// Print file entries
	for(std::vector<SourceFile*>::const_iterator it = files.begin(); it!=files.end(); ++it) {
		outfile << *(*it);
	}

    // Print routines to the PDB file
	for(std::vector<Routine*>::const_iterator it = routines.begin(); it!=routines.end(); ++it) {
		outfile << *(*it);
	}
	
	// Print groups to the PDB file
    for(std::vector<Group*>::const_iterator it = groups.begin(); it!=groups.end(); ++it) {
		outfile << *(*it);
	} 

    // Print types to the PDB file
	for(std::vector<Type*>::const_iterator it = types.begin(); it!=types.end(); ++it) {
		outfile << *(*it);
	}

	// Print templates to the PDB file
	for(std::vector<Template*>::const_iterator it = templates.begin(); it!=templates.end(); ++it) {
		outfile << *(*it);
	}
    
    // Print namespaces to the PDB file.
    for(std::vector<Namespace*>::const_iterator it = namespaces.begin(); it != namespaces.end(); ++it) {
        outfile << *(*it);
    }

    // Print macros to the PDB file.
    for(std::vector<Macro*>::const_iterator it = macros.begin(); it != macros.end(); ++it) {
        outfile << *(*it);
    }

    // Print pragmas to the PDB file.
    for(std::vector<Pragma*>::const_iterator it = pragmas.begin(); it != pragmas.end(); ++it) {
        outfile << *(*it);
    }


	
	return 0;
}                                  
