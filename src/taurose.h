#ifndef __TAUROSE_H__
#define __TAUROSE_H__

#include "pdtutil.h"
#include "routine.h"
#include "statement.h"
#include "group.h"
#include "namespace.h"
#include "template.h"
#include "type.h"

class InheritedAttribute {
    public:
            int depth;  // Depth in AST
            Routine * routine; // Routine we're inside of, if any
            Statement * statement; // Statement we're inside of, if any 
            Statement * switchCase; 
            Statement * afterSwitch;
            Group * group;
            Namespace * ns;
			Type * parentEnum;
			Template * parentTemplate;
            SgTemplateFunctionDefinition * templateFunctionDefinition;

            InheritedAttribute (int d, Routine * r = NULL, Statement * s = NULL, Statement * sc = NULL,
                                Statement * as = NULL, Group * g = NULL, Namespace * n = NULL, Type * pe = NULL,
								Template * pt = NULL, SgTemplateFunctionDefinition * tfd = NULL) :
                                    depth(d), routine(r), statement(s), switchCase(sc), afterSwitch(as),
                                    group(g), ns(n), parentEnum(pe), parentTemplate(pt),
                                    templateFunctionDefinition(tfd) {}; 

            InheritedAttribute (const InheritedAttribute & X) : depth(X.depth), routine(X.routine), 
                                                                statement(X.statement), switchCase(X.switchCase),
                                                                afterSwitch(X.afterSwitch), group(X.group),
                                                                ns(X.ns), parentEnum(X.parentEnum),
 																parentTemplate(X.parentTemplate),
                                                                templateFunctionDefinition(X.templateFunctionDefinition){}; 
};

class SynthesizedAttribute {
    public:
            Statement * next;
                Statement * down;

                    SynthesizedAttribute(Statement * n = NULL, Statement * d = NULL) : next(n), down(d) {}; 
                        SynthesizedAttribute(const SynthesizedAttribute & X) : next(X.next), down(X.down) {}; 
};

class PDTAttribute : public AstAttribute {
    public:
            Statement * statement;
            Routine * routine;
            Statement * gotoStmt;
            Group * group;

            PDTAttribute(Statement * s = NULL, Routine * r = NULL, Statement * g = NULL,
                         Group * gr = NULL) : statement(s), routine(r), gotoStmt(NULL), group(gr) {}; 
            PDTAttribute(const PDTAttribute & X) : statement(X.statement), routine(X.routine), 
                                                   gotoStmt(X.gotoStmt), group(X.group) {}; 
};

#endif

