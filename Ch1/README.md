# Ch1

这里的代码和 MLIR 本身没有什么关系，只是定义了一门很简单的高级语言 toy。编写一个 cli 将输入 toy 源码输出为 AST。

## 编译

```bash
cmake --build build --target toyc-ch1
```

## 验证

```bash
/home/molesir/development/llvm-project/build/bin/llvm-lit -v build/test/Ch1
./build/bin/toyc-ch1 test/Ch1/ast.toy -emit=ast
```