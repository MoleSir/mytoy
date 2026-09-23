#include "./dialect.hh"

#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/Value.h"
#include "mlir/Interfaces/FunctionImplementation.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include <algorithm>
#include <string>

using namespace mlir::toy;

#include "output/dialect.cpp.inc"

void ToyDialect::initialize() {
    this->addOperations<
#define GET_OP_LIST
#include "output/ops.cpp.inc"
    >();
}

//=============================================================================//
// Toy Operations
//=============================================================================//

static auto parseBinaryOp(mlir::OpAsmParser &parser, mlir::OperationState &result) -> mlir::ParseResult {
    auto operands = mlir::SmallVector<mlir::OpAsmParser::UnresolvedOperand, 2>{};
    auto operandsLoc = parser.getCurrentLocation();
    auto type = mlir::Type{};
    if (parser.parseOperandList(operands, 2) ||
        parser.parseOptionalAttrDict(result.attributes) ||
        parser.parseColonType(type))
    {
        return mlir::failure();    
    }

    if (auto funcType = llvm::dyn_cast<mlir::FunctionType>(type)) {
        if (parser.resolveOperands(operands, funcType.getInputs(), operandsLoc, result.operands)) {
            return mlir::failure();
        }
        result.addTypes(funcType.getResults());
        return mlir::success();
    }

    if (parser.resolveOperands(operands, type, result.operands)) {
        return mlir::failure();
    }
  
    result.addTypes(type);
    return mlir::success();
}

static void printBinaryOp(mlir::OpAsmPrinter &printer, mlir::Operation *op) {
    printer << " " << op->getOperands();
    printer.printOptionalAttrDict(op->getDiscardableAttrDictionary());
    printer << " : ";

    auto resultType = *op->result_type_begin();
    if (llvm::all_of(op->getOperandTypes(), [=](mlir::Type type) { return type == resultType; })) {
        printer << resultType;
        return;
    }

    printer.printFunctionalType(op->getOperandTypes(), op->getResultTypes());
}

//=============================================================================//
// ConstantOp
//=============================================================================//

/*
    let builders = [
        OpBuilder<(ins "DenseElementsAttr":$value), [{build($_builder, $_state, value.getType(), value);}]>, 
        OpBuilder<(ins "double":$value)>
    ];

    声明两个额外的 build 重载（默认的 build 已自动生成：
    (OpBuilder&, OperationState&, Type resultType, DenseElementsAttr value)

    - 第一个：
        build(builder, state, DenseElementsAttr value)。
        后面那段 [{ ... }] 是内联 C++ 实现，$_builder/$_state 会被替换成实际参数名。它调用的是默认那个四参数 build（因为 value.getType() 拿到了结果类型）。所以这个重载只是"省得调用方自己传 resultType"。
    
    - 第二个：
        build(builder, state, double value)。只声明不写实现，所以生成的只是签名，实现要手写。

*/
/// td 文件声明了需要一个输入是 "double":$value 的构造，在这里实现
void ConstantOp::build(mlir::OpBuilder& builder, mlir::OperationState& state, double value) {
    // const op 最后输出的是 F64Tensor，但输入只有 double，需要从 double -> Tensor
    // RankedTensorType：输入 shape 为 {} 因为是 scalar，类型是 F64
    auto dataType = RankedTensorType::get({}, builder.getF64Type());
    // DenseElementsAttr::get(type, value) 是 MLIR 提供的便捷重载：元素类型匹配时直接塞一个标量。
    // 因为 const op 的 attr 是需要一个 F64ElementsAttr
    auto dataAttribute = DenseElementsAttr::get(dataType, value);
    ConstantOp::build(builder, state, dataType, dataAttribute);
}

/// let hasCustomAssemblyFormat = 1;
/*

MLIR 框架有规定所有 op 必须满足格式：
```
%results = "dialect.op"(%operands) {attrs} : (inputTypes) -> outputTypes loc(...)
```
MLIR 框架解析一个 op 大概会用这个一个结构体
```
struct OperationState {
    std::string name;              // "toy.constant"
    SmallVector<Value> operands;   // 输入操作数（SSA 值）
    SmallVector<Type> types;       // 结果类型
    NamedAttrList attributes;      // 属性
    Location location;
    // 还有 regions、successors、trait 相关等
};
```
这只是针对格式的解析，还不需要理解 op 的含义。而我们的 parse 既然自定义格式，mlir 无法按照原来的格式一个一个解析出这些字段。
所以他构造一个空的 OperationState（仅仅初始化 name，这个总是必先先得到的，才能知道 op 给谁构造）。
交给我们自己的 parse，要求 parse 自己处理后面还没解析的字段。

流程是这样的：

1. 通用解析器读到 toy.constant，识别出 dialect 和操作名。
2. 它查表知道 toy.constant 有自定义 assembly（因为 td 里 hasCustomAssemblyFormat = 1）。
3. 它构造一个 OperationState，已经设好 name = "toy.constant"。
4. 调用 ConstantOp::parse(parser, result)，把 state 传进来。
5. 你的 parse 负责：从当前位置开始，把还没解析的部分（属性、结果类型等）填进 result。
6. parse 返回后，框架继续处理剩下的通用部分（比如 loc、以及把结果 SSA 名绑定），最后 Operation::create(result)。

MLIR 有通用 Op 格式，任何 Op 都能这样表示。为了人类可读，Op 可以声明自定义 assembly。

在这个 API 里，success() 的 bool 是 false，failure() 的 bool 是 true。

*/
auto ConstantOp::parse(mlir::OpAsmParser& parser, mlir::OperationState& result) -> mlir::ParseResult {
    // %0 = toy.constant dense<5.500000e+00> : tensor<f64>
    
    // 准备一个空的 DenseElementsAttr，待会儿解析结果填进去。
    /*
        parseOptionalAttrDict:
        - 如果当前位置是 { → 解析一个属性字典，存进 result.attributes。
        - 如果当前位置不是 { → 什么都不做，也算成功。
        - 只有当它看到 { 但里面语法错了 → 才算失败。

        parseAttribute(value, "value", result.attributes):
        1 value（第一个参数）：一个 DenseElementsAttr& 的输出参数。解析出来的属性值会写进这个局部变量。之后你能用 value 拿到 dense<1.0>。
        2 "value"（第二个参数）：这个属性在 Op 里的名字。它是字符串，因为 td 里你声明了 F64ElementsAttr:$value，名字就是 "value"。这个名字用于存进 attributes 字典。
        3 result.attributes（第三个参数）：存到哪里。解析到的属性会以 "value" -> 值 的形式存进 OperationState 的 attributes 里。

        从当前位置解析一个属性（比如 dense<1.000000e+00>），把值写进 value，同时把它作为名为 "value" 的属性存进 result.attributes。这正好和 print 里 printer << getValue() 对称。

        1 是"给框架用的"——最终 Op 的 attributes 里必须有 value，才能构造出正确的 Operation。
        3 是"给你自己用的"——你后面要用 value.getType() 拿结果类型

    */
    auto value = mlir::DenseElementsAttr{};
    if (parser.parseOptionalAttrDict(result.attributes) || parser.parseAttribute(value, "value", result.attributes)) {
        return mlir::failure();
    } 

    result.addTypes(value.getType());
    return mlir::success();
} 

/// let hasCustomAssemblyFormat = 1;
void ConstantOp::print(mlir::OpAsmPrinter& printer) {
    printer << " ";
    printer.printOptionalAttrDict((*this)->getDiscardableAttrDictionary());
    printer << getValue();
}

/// verifier 在 Op 已经通过 Operation::create 造好之后、被 MLIR 框架调用
auto ConstantOp::verify() -> LogicalResult {
    /*

        - this->getResult()：Op 的第 0 个结果（Value）
        - getType()：这个结果的类型
        - dyn_cast<RankedTensorType>(...)：尝试把它当作 RankedTensorType。
    
    */
    auto resultType = llvm::dyn_cast<mlir::RankedTensorType>(this->getResult().getType());
    if (!resultType) {
        return mlir::success();
    }

    auto attrType = llvm::cast<mlir::RankedTensorType>(getValue().getType());
    if (attrType.getRank() != resultType.getRank()) {
        return emitOpError("return type must match the one of the attached value attribute: ") 
            << attrType.getRank() << " != " << resultType.getRank();
    }

    for (int dim = 0, dimE = attrType.getRank(); dim < dimE; ++dim) {
        if (attrType.getShape()[dim] != resultType.getShape()[dim]) {
            return emitOpError("return type shape mismatches its attribute at dimension ")
                << dim << ": " << attrType.getShape()[dim]
                << " != " << resultType.getShape()[dim];
        }
    }

    return mlir::success();
}

//=============================================================================//
// AddOp
//=============================================================================//

void AddOp::build(mlir::OpBuilder &builder, mlir::OperationState &state, mlir::Value lhs, mlir::Value rhs) {
    state.addTypes(UnrankedTensorType::get(builder.getF64Type()));
    state.addOperands({lhs, rhs});
}

auto AddOp::parse(mlir::OpAsmParser &parser, mlir::OperationState &result) -> mlir::ParseResult {
    return parseBinaryOp(parser, result);
}

void AddOp::print(mlir::OpAsmPrinter &p) { 
    printBinaryOp(p, *this); 
}

//==============================================================================//
// GenericCallOp
//==============================================================================//

void GenericCallOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                          StringRef callee, ArrayRef<mlir::Value> arguments) 
{
    state.addTypes(UnrankedTensorType::get(builder.getF64Type()));
    state.addOperands(arguments);
    state.addAttribute("callee", mlir::SymbolRefAttr::get(builder.getContext(), callee));
}

//==============================================================================//
// FuncOp
//==============================================================================//

void FuncOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   llvm::StringRef name, mlir::FunctionType type,
                   llvm::ArrayRef<mlir::NamedAttribute> attrs) 
{
    buildWithEntryBlock(builder, state, name, type, attrs, type.getInputs());
}

auto FuncOp::parse(mlir::OpAsmParser &parser, mlir::OperationState &result) -> mlir::ParseResult {
    auto buildFuncType = [] (
        mlir::Builder &builder, llvm::ArrayRef<mlir::Type> argTypes,
        llvm::ArrayRef<mlir::Type> results,
        mlir::function_interface_impl::VariadicFlag, std::string &
    ) -> mlir::FunctionType { 
        return builder.getFunctionType(argTypes, results); 
    };

    return mlir::function_interface_impl::parseFunctionOp(
        parser, result, /*allowVariadic=*/false,
        getFunctionTypeAttrName(result.name), buildFuncType,
        getArgAttrsAttrName(result.name), getResAttrsAttrName(result.name)
    );
}

void FuncOp::print(mlir::OpAsmPrinter &p) {
    mlir::function_interface_impl::printFunctionOp(
        p, *this, /*isVariadic=*/false, getFunctionTypeAttrName(),
        getArgAttrsAttrName(), getResAttrsAttrName()
    );
}

//==============================================================================//
// MulOp
//==============================================================================//

void MulOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                  mlir::Value lhs, mlir::Value rhs) 
{
    state.addTypes(UnrankedTensorType::get(builder.getF64Type()));
    state.addOperands({lhs, rhs});
}

mlir::ParseResult MulOp::parse(mlir::OpAsmParser &parser, mlir::OperationState &result) {
    return parseBinaryOp(parser, result);
}

void MulOp::print(mlir::OpAsmPrinter &p) { 
    printBinaryOp(p, *this); 
}

//==============================================================================//
// ReturnOp
//==============================================================================//

llvm::LogicalResult ReturnOp::verify() {
    auto function = cast<FuncOp>((*this)->getParentOp());

    if (getNumOperands() > 1) {
        return emitOpError() << "expects at most 1 return operand";
    }

    const auto &results = function.getFunctionType().getResults();
    if (getNumOperands() != results.size()) {
        return emitOpError() << "does not return the same number of values ("
                             << getNumOperands() << ") as the enclosing function ("
                             << results.size() << ")";
    }

    if (!hasOperand()) {
        return mlir::success();
    }

    auto inputType = *operand_type_begin();
    auto resultType = results.front();
    
    if (inputType == resultType || 
        llvm::isa<mlir::UnrankedTensorType>(inputType) ||
        llvm::isa<mlir::UnrankedTensorType>(resultType)) 
    {
        return mlir::success();
    }

    return emitError()  << "type of return operand (" << inputType
                        << ") doesn't match function result type (" << resultType
                        << ")";
}

//==============================================================================//
// TransposeOp
//==============================================================================//

void TransposeOp::build(mlir::OpBuilder &builder, mlir::OperationState &state, mlir::Value value) {
    state.addTypes(UnrankedTensorType::get(builder.getF64Type()));
    state.addOperands(value);
}

llvm::LogicalResult TransposeOp::verify() {
    auto inputType = llvm::dyn_cast<RankedTensorType>(getOperand().getType());
    auto resultType = llvm::dyn_cast<RankedTensorType>(getType());
    if (!inputType || !resultType) {
        return mlir::success();
    }

    auto inputShape = inputType.getShape();
    if (!std::equal(inputShape.begin(), inputShape.end(), resultType.getShape().rbegin())) {
        return emitError() << "expected result shape to be a transpose of the input";
    }
    return mlir::success();
}

#define GET_OP_CLASSES
#include "output/ops.cpp.inc"