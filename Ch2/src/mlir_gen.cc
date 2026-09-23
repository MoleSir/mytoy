#include "mlir_gen.hh"

#include "ast.hh"
#include "dialect.hh"
#include "lexer.hh"

#include "mlir/IR/Block.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/Verifier.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopedHashTable.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <numeric>
#include <optional>
#include <vector>

namespace toy {

namespace {

using namespace mlir::toy;

using llvm::ArrayRef;
using llvm::cast;
using llvm::dyn_cast;
using llvm::isa;
using llvm::ScopedHashTableScope;
using llvm::SmallVector;
using llvm::StringRef;

class MlirGen {
public:
    MlirGen(mlir::MLIRContext &context) : builder_(&context) {}

    mlir::ModuleOp mlir_gen(Module &module) {
        module_ = mlir::ModuleOp::create(builder_.getUnknownLoc());

        for (Function &function : module)
            mlir_gen(function);

        if (failed(mlir::verify(module_))) {
            module_.emitError("module verification error");
            return nullptr;
        }

        return module_;
    }

private:
    mlir::ModuleOp module_;
    mlir::OpBuilder builder_;
    llvm::ScopedHashTable<StringRef, mlir::Value> symbol_table_;

    mlir::Location loc(const Location &location) {
        return mlir::FileLineColLoc::get(builder_.getStringAttr(*location.file), location.line,
                                         location.col);
    }

    llvm::LogicalResult declare(StringRef var, mlir::Value value) {
        if (symbol_table_.count(var))
            return mlir::failure();
        symbol_table_.insert(var, value);
        return mlir::success();
    }

    mlir::toy::FuncOp mlir_gen(Prototype &proto) {
        auto location = loc(proto.loc());

        SmallVector<mlir::Type, 4> arg_types(proto.get_args().size(), get_type(VarType{}));
        auto func_type = builder_.getFunctionType(arg_types, {});
        return mlir::toy::FuncOp::create(builder_, location, proto.get_name(), func_type);
    }

    mlir::toy::FuncOp mlir_gen(Function &func) {
        ScopedHashTableScope<StringRef, mlir::Value> var_scope(symbol_table_);

        builder_.setInsertionPointToEnd(module_.getBody());
        mlir::toy::FuncOp function = mlir_gen(*func.get_proto());
        if (!function)
            return nullptr;

        mlir::Block &entry_block = function.front();
        auto proto_args = func.get_proto()->get_args();

        for (const auto name_value : llvm::zip(proto_args, entry_block.getArguments())) {
            if (failed(declare(std::get<0>(name_value)->get_name(), std::get<1>(name_value))))
                return nullptr;
        }

        builder_.setInsertionPointToStart(&entry_block);

        if (mlir::failed(mlir_gen(*func.get_body()))) {
            function.erase();
            return nullptr;
        }

        ReturnOp return_op;
        if (!entry_block.empty())
            return_op = dyn_cast<ReturnOp>(entry_block.back());
        if (!return_op) {
            ReturnOp::create(builder_, loc(func.get_proto()->loc()));
        } else if (return_op.hasOperand()) {
            function.setType(builder_.getFunctionType(function.getFunctionType().getInputs(),
                                                      get_type(VarType{})));
        }

        return function;
    }

    mlir::Value mlir_gen(BinaryExpr &binop) {
        mlir::Value lhs = mlir_gen(*binop.get_lhs());
        if (!lhs)
            return nullptr;
        mlir::Value rhs = mlir_gen(*binop.get_rhs());
        if (!rhs)
            return nullptr;
        auto location = loc(binop.loc());

        switch (binop.get_op()) {
        case '+':
            return AddOp::create(builder_, location, lhs, rhs);
        case '*':
            return MulOp::create(builder_, location, lhs, rhs);
        }

        mlir::emitError(location, "invalid binary operator '") << binop.get_op() << "'";
        return nullptr;
    }

    mlir::Value mlir_gen(VariableExpr &expr) {
        if (auto variable = symbol_table_.lookup(expr.get_name()))
            return variable;

        mlir::emitError(loc(expr.loc()), "error: unknown variable '") << expr.get_name() << "'";
        return nullptr;
    }

    llvm::LogicalResult mlir_gen(ReturnExpr &ret) {
        auto location = loc(ret.loc());

        mlir::Value expr = nullptr;
        if (ret.get_expr().has_value()) {
            if (!(expr = mlir_gen(**ret.get_expr())))
                return mlir::failure();
        }

        ReturnOp::create(builder_, location, expr ? ArrayRef(expr) : ArrayRef<mlir::Value>());
        return mlir::success();
    }

    mlir::Value mlir_gen(LiteralExpr &lit) {
        auto type = get_type(lit.get_dims());

        std::vector<double> data;
        data.reserve(llvm::product_of(lit.get_dims()));
        collect_data(lit, data);

        mlir::Type element_type = builder_.getF64Type();
        auto data_type = mlir::RankedTensorType::get(lit.get_dims(), element_type);

        auto data_attribute = mlir::DenseElementsAttr::get(data_type, llvm::ArrayRef(data));

        return ConstantOp::create(builder_, loc(lit.loc()), type, data_attribute);
    }

    void collect_data(Expr &expr, std::vector<double> &data) {
        if (auto *literal = dyn_cast<LiteralExpr>(&expr)) {
            for (auto &value : literal->get_values())
                collect_data(*value, data);
            return;
        }

        assert(isa<NumberExpr>(expr) && "expected literal or number expr");
        data.push_back(cast<NumberExpr>(expr).get_value());
    }

    mlir::Value mlir_gen(CallExpr &call) {
        StringRef callee = call.get_callee();
        auto location = loc(call.loc());

        SmallVector<mlir::Value, 4> operands;
        for (auto &expr : call.get_args()) {
            auto arg = mlir_gen(*expr);
            if (!arg)
                return nullptr;
            operands.push_back(arg);
        }

        if (callee == "transpose") {
            if (call.get_args().size() != 1) {
                mlir::emitError(location, "MLIR codegen encountered an error: toy.transpose "
                                          "does not accept multiple arguments");
                return nullptr;
            }
            return TransposeOp::create(builder_, location, operands[0]);
        }

        return GenericCallOp::create(builder_, location, callee, operands);
    }

    llvm::LogicalResult mlir_gen(PrintExpr &print) {
        auto arg = mlir_gen(*print.get_arg());
        if (!arg)
            return mlir::failure();

        PrintOp::create(builder_, loc(print.loc()), arg);
        return mlir::success();
    }

    mlir::Value mlir_gen(NumberExpr &num) {
        return ConstantOp::create(builder_, loc(num.loc()), num.get_value());
    }

    mlir::Value mlir_gen(Expr &expr) {
        switch (expr.get_kind()) {
        case ExprKind::Binary:
            return mlir_gen(cast<BinaryExpr>(expr));
        case ExprKind::Variable:
            return mlir_gen(cast<VariableExpr>(expr));
        case ExprKind::Literal:
            return mlir_gen(cast<LiteralExpr>(expr));
        case ExprKind::Call:
            return mlir_gen(cast<CallExpr>(expr));
        case ExprKind::Number:
            return mlir_gen(cast<NumberExpr>(expr));
        default:
            mlir::emitError(loc(expr.loc()))
                << "MLIR codegen encountered an unhandled expr kind '"
                << static_cast<int>(expr.get_kind()) << "'";
            return nullptr;
        }
    }

    mlir::Value mlir_gen(VarDeclExpr &var_decl) {
        auto *init = var_decl.get_init_val();
        if (!init) {
            mlir::emitError(loc(var_decl.loc()), "missing initializer in variable declaration");
            return nullptr;
        }

        mlir::Value value = mlir_gen(*init);
        if (!value)
            return nullptr;

        if (!var_decl.get_type().shape.empty()) {
            value = ReshapeOp::create(builder_, loc(var_decl.loc()), get_type(var_decl.get_type()),
                                      value);
        }

        if (failed(declare(var_decl.get_name(), value)))
            return nullptr;
        return value;
    }

    llvm::LogicalResult mlir_gen(ExprList &block) {
        ScopedHashTableScope<StringRef, mlir::Value> var_scope(symbol_table_);
        for (auto &expr : block) {
            if (auto *var_decl = dyn_cast<VarDeclExpr>(expr.get())) {
                if (!mlir_gen(*var_decl))
                    return mlir::failure();
                continue;
            }
            if (auto *ret = dyn_cast<ReturnExpr>(expr.get()))
                return mlir_gen(*ret);
            if (auto *print = dyn_cast<PrintExpr>(expr.get())) {
                if (mlir::failed(mlir_gen(*print)))
                    return mlir::success();
                continue;
            }

            if (!mlir_gen(*expr))
                return mlir::failure();
        }
        return mlir::success();
    }

    mlir::Type get_type(ArrayRef<int64_t> shape) {
        if (shape.empty())
            return mlir::UnrankedTensorType::get(builder_.getF64Type());

        return mlir::RankedTensorType::get(shape, builder_.getF64Type());
    }

    mlir::Type get_type(const VarType &type) { return get_type(type.shape); }
};

}

mlir::OwningOpRef<mlir::ModuleOp> mlir_gen(mlir::MLIRContext &context, Module &module) {
    return MlirGen(context).mlir_gen(module);
}

}
