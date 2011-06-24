// Example ROSE Translator: used within ROSE/tutorial

#include "rose.h"
#include <boost/algorithm/string.hpp>

using namespace std;

// Class declaration
class visitorTraversal : public AstSimpleProcessing
   {
     public:
          virtual void visit(SgNode* n);
   };

void visitorTraversal::visit(SgNode* n)
   {
  // On each node look for any comments of CPP directives
     SgLocatedNode* locatedNode = isSgLocatedNode(n);
     if (locatedNode != NULL)
        {
          AttachedPreprocessingInfoType* comments = locatedNode->getAttachedPreprocessingInfo();

          if (comments != NULL)
             {
               printf ("Found attached preproc info (to IR node at %p of type: %s): \n",locatedNode,locatedNode->class_name().c_str());
               int counter = 0;
               AttachedPreprocessingInfoType::iterator i;
               for (i = comments->begin(); i != comments->end(); i++)
                  {
		 			switch((*i)->getTypeOfDirective()) {
						case PreprocessingInfo::CpreprocessorDefineDeclaration:
						case PreprocessingInfo::CpreprocessorUndefDeclaration:
						case PreprocessingInfo::CpreprocessorIncludeDeclaration: {
						
							std::string text = (*i)->getString();
							boost::algorithm::replace_all(text, "\\\n", " ");
							boost::algorithm::erase_all(text, "\n");
						
	                    	printf ("          Attached preproc info #%d in file %s (relativePosition=%s): classification %s :\n%s\n",
		                         counter++,(*i)->get_file_info()->get_filenameString().c_str(),
		                         ((*i)->getRelativePosition() == PreprocessingInfo::before) ? "before" : "after",
		                         PreprocessingInfo::directiveTypeName((*i)->getTypeOfDirective()).c_str(),
		                         text.c_str());
						};
						break;
						default: ;
					}
                  }
             }
            else
             {
               //printf ("No attached comments (at %p of type: %s): \n",locatedNode,locatedNode->sage_class_name());
             }
        }
   }

int main( int argc, char * argv[] )
   {
  // Build the AST used by ROSE
     SgProject* project = frontend(argc,argv);

  // Build the traversal object
     visitorTraversal exampleTraversal;

  // Call the traversal starting at the project node of the AST
  // Traverse all header files and source file (the -rose:collectAllCommentsAndDirectives 
  // commandline option controls if comments and CPP directives are separately extracted 
  // from header files).
     exampleTraversal.traverse(project,preorder);

     return 0;
   }

