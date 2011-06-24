// ROSE is a tool for building preprocessors, this file is an example preprocessor built with ROSE.
// rose.C: Example (default) ROSE Preprocessor: used for testing ROSE infrastructure

#include "rose.h"
#include <iostream>

using std::cout;
using std::endl;

class PreAndPostOrderTraversal : public AstPrePostProcessing {
        public:
                virtual void preOrderVisit(SgNode* n);
                virtual void postOrderVisit(SgNode* n);
};


void PreAndPostOrderTraversal::preOrderVisit(SgNode* n) {

        SgFunctionDeclaration * dec = isSgFunctionDeclaration(n);
        if (dec != NULL) {
                cout << "Found function declaration " << dec->get_name().getString();
                Sg_File_Info * start = dec->get_startOfConstruct();
                Sg_File_Info * end = dec->get_endOfConstruct();
                if(start->isCompilerGenerated()) {
                        cout << ", which is compiler-generated" << endl;
                } else {
                        cout << " in file " << start->get_raw_filename() << ", " << start->get_file_id() << " from line " << 
                            start->get_line() << ", col " << start->get_col() << " to line " << 
                            end->get_line() << ", col " << end->get_col() << endl;
                }
                SgFunctionType * type = dec->get_type();
                SgType * retType = type->get_return_type();
        cout << "Return type: " << retType->unparseToString() << endl;
        SgFunctionParameterList * params = dec->get_parameterList();
        SgInitializedNamePtrList & ptrList = params->get_args();
        if(!ptrList.empty()) {
            cout << "Parameter types: ";
            for(SgInitializedNamePtrList::iterator j = ptrList.begin(); j != ptrList.end(); j++) {
                SgType * pType = (*j)->get_type();
                cout << pType->unparseToString() << " ";
            }
            cout << endl;
        }
        cout << "Linkage: " << dec->get_linkage() << endl;
               cout << endl;
        }


        SgFunctionDefinition * def = isSgFunctionDefinition(n);
        if (def != NULL) {
                cout << "Found function definition " << def->get_declaration()->get_name().getString();
                Sg_File_Info * start = def->get_startOfConstruct();
                Sg_File_Info * end = def->get_endOfConstruct();
                if(start->isCompilerGenerated()) {
                        cout << ", which is compiler-generated" << endl;
                } else {
                        cout << " in file " << start->get_raw_filename() << " from line " << start->get_line() << ", col " << start->get_col() << " to line " << end->get_line() << ", col " << end->get_col() << endl;
                SgBasicBlock * body = def->get_body();
        Sg_File_Info * bodyStart = body->get_startOfConstruct();
        Sg_File_Info * bodyEnd =   body->get_endOfConstruct();
        cout << "Function body from line " << bodyStart->get_line() << ", col " << bodyStart->get_col() << " to line " << bodyEnd->get_line() << ", col " << bodyEnd->get_col() << endl; 
 }
        cout << endl;
        }
}

void PreAndPostOrderTraversal::postOrderVisit(SgNode* n) {
        SgFunctionDefinition * def = isSgFunctionDefinition(n);
        if (def != NULL) {
                //cout << "Leaving function definition" << endl;
        }
}


int main ( int argc, char* argv[] ) {
        if (SgProject::get_verbose() > 0)
                printf ("In prePostTraversal.C: main() \n");

        SgProject* project = frontend(argc,argv);
        ROSE_ASSERT (project != NULL);

        // Build the traversal object
        PreAndPostOrderTraversal exampleTraversal;

        // Call the traversal starting at the project node of the AST
        exampleTraversal.traverse(project);

        return 0;
}
