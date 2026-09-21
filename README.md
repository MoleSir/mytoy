# MLIR Toy


## 构建

```bash
cmake -S . -B build \
  -DMLIR_DIR=/home/molesir/development/llvm-project/build/lib/cmake/mlir \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build                 # 全部七章
cmake --build build --target toyc-ch5   # 只编某一章
```