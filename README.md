Init
----

- LLVM    : cd llvm && ./init.sh
- Z3      : cd deps/z3 && ./init.sh

Build
-----

- For first time buildi   : ./llvm.py build -c
- For later build         : ./llvm.py build
- For debug build         : ./llvm.py build -c -d <item>

Test
----

- Set test env            : ./llvm.py work
- Run on bitcode file     : opt -symf <path-to-output> <path-to-bitcode>
- Exit test env           : Ctrl + D or exit

Kernel
------

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
