#ifndef __SIMULATOR_H__
#define __SIMULATOR_H__

#include "slang/ast/Compilation.h"

namespace slang {
class Simulator {
private:
    ast::Compilation &CompiledDesign;
public:
    Simulator(ast::Compilation &CompiledDesign) : CompiledDesign(CompiledDesign) {
        // Ensure elaboration has taken place.
        CompiledDesign.getRoot();
    }

    bool simulate() {
        return true;
    }

};

}
#endif
