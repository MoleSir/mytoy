# MLIR Toy 教程 —— 出树学习环境

这是从 `llvm-project/mlir/examples/toy/` 搬出来的 Toy 教程，**独立编译**，不碰 llvm-project 源码树。

源码逐字复制自上游（`README-upstream.md` 是上游原文），只新增了本目录的构建/测试脚手架。

- 上游源码版本：`llvm-project` @ `f28f0baf4`
- 依赖的宿主 MLIR/LLVM 构建：`/home/molesir/development/llvm-project/build`（Debug + assertions）
- 测试用例来自 `mlir/test/Examples/Toy/`，复制到本目录 `test/`

## 目录结构

```
CMakeLists.txt        出树构建入口（替换了上游的 add_toy_chapter 宏）
Ch1 .. Ch7            教程各章，源码逐字复制自上游，CMakeLists 未改动
test/Ch1 .. Ch7       lit 测试（56 个用例）
README-upstream.md    上游原始 README
build/                CMake 构建目录（可整个删掉重来）
```

## 构建

```bash
cmake -S . -B build \
  -DMLIR_DIR=/home/molesir/development/llvm-project/build/lib/cmake/mlir \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build                 # 全部七章
cmake --build build --target toyc-ch5   # 只编某一章
```

`MLIR_DIR` 指向宿主 MLIR 的 **build 树**即可（`lib/cmake/mlir`），不需要先 `ninja install`。

**必须是 Debug**：宿主 LLVM/MLIR 是 Debug + `LLVM_ENABLE_ASSERTIONS=ON` 构建的。用 Release 编译本项目会造成 `NDEBUG` 与断言不一致，链接期或运行期出现难以排查的问题。

## 测试

```bash
cmake --build build --target check-toy
```

当前 56/56 通过。lit 配置在 `test/lit.cfg.py`，`FileCheck` / `not` / `mlir-opt` 从宿主 build 树借用。

## 各章用法

```bash
./build/bin/toyc-ch1 input.toy                     # Ch1 只解析，会提示需要 -emit
./build/bin/toyc-ch5 input.toy -emit=ast           # 打印 AST
./build/bin/toyc-ch5 input.toy -emit=mlir          # 生成 Toy dialect
./build/bin/toyc-ch5 input.toy -emit=mlir-affine   # 降到 Affine
./build/bin/toyc-ch7 input.toy -emit=llvm          # 降到 LLVM dialect
./build/bin/toyc-ch7 input.toy -emit=jit           # 直接 JIT 执行
```

可用的 `-emit=` 值随章节递增，直接 `./build/bin/toyc-ch7 --help` 看全量。

## 动手改代码

改 `Ch*/` 下的源码后重新 `cmake --build build` 即可，只有改动的那一章会重编。
每章是**独立的完整编译器**（Ch5 不依赖 Ch3 的产物），可以对照着看增量演进 —— 这正是教程的设计意图。

## 已知无害警告

编译时会有大量 `-Wdeprecated-declarations`，来源是 `mlir-tblgen` 生成的 `Ops.cpp.inc`，
指向上游教程的 ODS 定义还在用旧版 builder 签名（未跟进 MLIR 的 Properties 迁移）。
属上游示例问题，不影响功能。

## 磁盘占用

约 2.8G，几乎全是 `build/bin/toyc-ch{6,7}` 各 1.1G —— 这两章链接了 JIT/native codegen，
在 Debug 构建下体积很大，属正常。不需要时可 `cmake --build build --target toyc-ch5` 只留前面几章，
或直接 `rm -rf build` 全清（重新配置 + 编译约 5 分钟）。
