# Ch3

拷贝一份 Ch2。

Ch3 相比 Ch2 只做了一件事：**给方言加上优化能力**，让 canonicalizer 能在 Toy IR 上做模式匹配和重写。

1. ops.td 补两个声明（决定了后面能写什么）：
    - `[Pure]` trait 加到 AddOp / MulOp / ReshapeOp / TransposeOp 上（Ch2 里只有 ConstantOp 有）。Pure 表示"无副作用、结果只由输入决定"，DCE 和 canonicalizer 才敢重写这些 Op。
    - `let hasCanonicalizer = 1;` 加到 ReshapeOp / TransposeOp 上。这会在生成的 C++ 类里**声明** `getCanonicalizationPatterns()`，我们才能在 combine.cc 里**定义**它 —— 不加则编译报 "out-of-line definition does not match any declaration"。

2. combine.td：用 DRR（Declarative Rewrite Rules）声明式地写重写规则（新增文件，是第 5 个 tablegen 输入）
    用一个新的 generator 导出成**一个** inc：
    `mlir-tblgen -gen-rewriters combine.td -o output/combine.inc`

    里面三条规则：

    | 规则 | 含义 | 用到什么 |
    | --- | --- | --- |
    | ReshapeReshapeOptPattern | `reshape(reshape(x))` → `reshape(x)` | 最基础的 Pat |
    | FoldConstantReshapeOptPattern | `reshape(constant(x))` → `constant(reshape(x))` | NativeCodeCall 内联 C++ |
    | RedundantReshapeOptPattern | 输入输出类型相同时 `reshape(x)` → `x` | Constraint 加前置条件 |

3. combine.cc：把规则注册到 Op 上（新增文件）
    - `#include "output/combine.inc"`（放在匿名 namespace 里）拿到 DRR 生成的那三个 pattern 类；
    - 手写一个 C++ pattern：`transpose(transpose(x))` → `x`，自己实现 `matchAndRewrite`；
    - 实现 `TransposeOp::getCanonicalizationPatterns` 和 `ReshapeOp::getCanonicalizationPatterns`，把上面这些 pattern 注册给自己的 Op。

    DRR 和 C++ pattern 的区别：前者写在 .td 里由 tablegen 展开成匹配代码，后者手写。能表达成 DRR 的优先用 DRR。

4. main.cc 加 `-opt`：
    - 拆成 `load_mlir()`（只负责拿到 module）+ `dump_mlir()`（优化后打印），因为"从 toy 生成"和"从 .mlir 解析"两条路都要能接优化；
    - `mlir::PassManager` + `pm.addNestedPass<toy::FuncOp>(mlir::createCanonicalizerPass())`；
    - 注意 canonicalizer 是 **MLIR 现成的 pass**，我们做的只是把 pattern 挂到自己的 Op 上 —— 这就是"语言特定优化"接入 MLIR 的方式。

5. CMake 相对 Ch2 多了：combine.td → combine.inc 的生成规则、src/combine.cc、链接库 MLIRPass（PassManager 在里面）。

## 编译

```bash
cmake --build build --target toyc-ch3
```

## 验证

```bash
/home/molesir/development/llvm-project/build/bin/llvm-lit -v build/test/Ch3
./build/bin/toyc-ch3 test/Ch3/transpose_transpose.toy -emit=mlir -opt
./build/bin/toyc-ch3 test/Ch3/trivial_reshape.toy -emit=mlir -opt
```

## 已知

`scalar.toy -emit=mlir -opt` 会崩（断言失败），上游也一样：DRR 里的 FoldConstantReshapeOptPattern 没检查元素个数，标量常量是 1 个元素、目标类型是 4 个元素时 `DenseElementsAttr::reshape` 直接断言。所以 scalar.toy 的 RUN 行只测 `-emit=mlir`。
