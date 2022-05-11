# ConfInLog Src 

A tool to infer configuration constraints from log messages in source code.

`ConfInLog` is built based on LibTooling APIs of Clang/LLVM framework.



## LLVM build commands

```bash
cmake -DLLVM_TARGETS_TO_BUILD="X86,ARM" -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release ../
```

## `ConfInLog` build commands

```bash
cd <path/to/ConfInLog/src>
mkdir build && cd build
cmake ../
make
```


## `ConfInLog` run commands
```bash
cd <path/to/ConfInLog/src>
./scripts/confinlog.py -[a|p|f|i|e|m] <target_software_name>
```
