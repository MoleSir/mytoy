#ifndef TOY_COMBINE
#define TOY_COMBINE

include "mlir/IR/PatternBase.td"
include "ops.td"

//==========================================================================//
// Basic Pattern-Match and Rewrite
//==========================================================================//

def ReshapeReshapeOptPattern : Pat<(ReshapeOp(ReshapeOp $arg)), (ReshapeOp $arg)>;

//==========================================================================//
// Pattern-Match and Rewrite using Native Code Call
//==========================================================================//

def ReshapeConstant : NativeCodeCall<"$0.reshape(::llvm::cast<ShapedType>($1.getType()))">;

def FoldConstantReshapeOptPattern : Pat<
    (ReshapeOp:$res (ConstantOp $arg)),
    (ConstantOp (ReshapeConstant $arg, $res))
>;

//==========================================================================//
// Pattern-Match and Rewrite with Constraints
//==========================================================================//

def TypesAreIdentical : Constraint<CPred<"$0.getType() == $1.getType()">>;

def RedundantReshapeOptPattern : Pat<
    (ReshapeOp:$res $arg), (replaceWithValue $arg),
    [(TypesAreIdentical $res, $arg)]
>;


#endif // TOY_COMBINE
