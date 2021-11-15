# Deadline
Deadline provide a formal and precise definition of double-fetch bugs and then implement a static analysis system to automatically detect double-fetch bugs in OS kernels. Deadline uses static program analysis techniques to systematically find multi-reads throughout the kernel and employs specialized symbolic checking to vet each multi-read for double-fetch bugs. We apply Deadline to Linux and FreeBSD kernels and find 23 new bugs in Linux and one new bug in FreeBSD.

This repository is provided under the terms of the MIT license.

## Init
- LLVM    : cd llvm && ./init.sh
- Z3      : cd deps/z3 && ./init.sh

## Build
- For first time buildi   : ./llvm.py build -c
- For later build         : ./llvm.py build
- For debug build         : ./llvm.py build -c -d <item>

## Test
- Set test env            : ./llvm.py work
- Run on bitcode file     : opt -symf <path-to-output> <path-to-bitcode>
- Exit test env           : Ctrl + D or exit

## Kernel
(In the case of Linux kernel)

- Setup submodule         : git submodule update --init -- app/linux-stable
- Checkout a version      : ./main.py checkout
- Config                  : ./main.py config
- Build w/gcc (3 hours)   : ./main.py build
- Parse build procedure   : ./main.py parse
- Build w/llvm            : ./main.py irgen
- Group into modules      : ./main.py group
- Optimize and LTO        : ./main.py trans
- Run the checker         : ./main.py check
- Check failure/timeouts  : ./main.py stat

## Reference
https://ieeexplore.ieee.org/abstract/document/8418630
```
  @inproceedings{xu2018precise,
  title={Precise and scalable detection of double-fetch bugs in OS kernels},
  author={Xu, Meng and Qian, Chenxiong and Lu, Kangjie and Backes, Michael and Kim, Taesoo},
  booktitle={2018 IEEE Symposium on Security and Privacy (SP)},
  pages={661--678},
  year={2018},
  organization={IEEE}
}
```
