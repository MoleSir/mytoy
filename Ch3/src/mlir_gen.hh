#pragma once

#include <memory>

namespace mlir {

class MLIRContext;
class ModuleOp;
template <typename OpTy>
class OwningOpRef;

}

namespace toy {

class Module;

mlir::OwningOpRef<mlir::ModuleOp> mlir_gen(mlir::MLIRContext &context, Module &module);

}
