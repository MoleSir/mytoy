# Ch2

先拷贝一份 Ch1，此时还是只能将输入 toy 文件“编译”为 AST 的程序。

1. 编写 ops.td，声明式地描述 Toy 方言：方言叫什么、有哪些 Op、每个 Op 的输入输出是什么类型、文本格式长什么样、要挂哪些接口。它是生成下面那些 C++ 类的输入，本身不是 C++ 代码。

2. 利用 tablegen 将 ops.td 导出为四个 C++ 文件：
    - dialect.h.inc：ToyDialect 类声明；
    - dialect.cpp.inc：ToyDialect 的构造/析构函数实现（构造函数内部会调用下面手写的 initialize()）；
    - ops.h.inc：Toy 的 Op 类声明；
    - ops.cpp.inc：Toy 的 Op 类实现 —— 但只包含 tablegen 生成的那部分，见第 3 条。

3. 补充 ToyDialect 和 ToyOps 的必要函数实现：
    - dialect.hh：这个文件是 include 两个生成的头文件，一个是 Dialect 类声明，另一个是各种 Op 的类声明；
    - dialect.cc：
        1. #include "output/dialect.cpp.inc"，导入自动生成的 Dialect 类函数实现；
        2. 实现 ToyDialect::initialize，给我们 Toy 方言插入所有的 Op（利用 GET_OP_LIST 宏包含 ops.cpp.inc，得到的是 Op 类名的列表，喂给 addOperations）；
        3. 所有 Op 的 build、parse、print 等，取决于 td 文件的声明，本质上是为我们的 Op 增加构造方法，重载解析输出格式，使得 mlir 框架可以认识我们自定义格式；
        4. 开启 GET_OP_CLASSES，#include "output/ops.cpp.inc"，这将导入 op 自动生成的函数实现；

    **哪些函数要自己写，完全由 .td 里的字段决定：**

    | .td 里写的 | 结果 |
    | --- | --- |
    | `assemblyFormat = "..."` | tblgen 生成 parse 和 print，进 ops.cpp.inc |
    | `hasCustomAssemblyFormat = 1` | parse/print 完全不生成，必须手写 |
    | `hasVerifier = 1` | tblgen 只在 verifyInvariants() 里调用你的 verify()，verify() 本体手写 |
    | `builders` 里 OpBuilder 带内联 body | tblgen 生成 |
    | `builders` 里 OpBuilder 只有签名 | 必须手写 |

    另外：不管 builders 写不写，tblgen 都会生成一整套默认 builder，builders 是往里追加。

    验证方法（ConstantOp 手写、PrintOp 声明式）：
    `grep -c 'ConstantOp::parse' ops.cpp.inc` 得 0，`grep -c 'PrintOp::parse' ops.cpp.inc` 得 2。

4. 以上都是为了实现 Toy 方言的 C++ 类。到这里我们就导出了一套可以嵌入 MLIR 中的 C++ 类，来表示 Toy 方言。

5. mlir_gen.hh/mlir_gen.cc：实现从 AST 数据结构转为 Toy 方言对象。所以到这里就可以搞一个从 toy 文件 -> AST -> Toy 方言对象 的路径，得到可以被 MLIR 框架操作的一堆类对象！同时支持导出为文本格式。
