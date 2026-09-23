# MLIR Toy

重写 MLIR 提供的 Toy 语言，学习 MLIR。

- [x] ch1
- [x] ch2
- [ ] ch3
- [ ] ch4
- [ ] ch5
- [ ] ch6
- [ ] ch7

## 构建

```bash
cmake -S . -B build \
  -DMLIR_DIR=/home/molesir/development/llvm-project/build/lib/cmake/mlir \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build # 编译全部
cmake --build build --target toyc-chx # 编译 chx
```