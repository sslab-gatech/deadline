# toolchain 
CC := clang
CK := opt 

# files
SRCS := $(wildcard *.c) 
BCFS := $(patsubst %.c, %.bc, $(SRCS))
SYMS := $(patsubst %.c, %.sym, $(SRCS))

# paths
VERSION = 4.12.5

LINUX_WKS := ../../../code
LINUX_SRC := $(LINUX_WKS)/srcs/linux-stable-$(VERSION)
LINUX_OBJ := $(LINUX_WKS)/objs/linux-stable-$(VERSION)
LINUX_BIN := $(LINUX_WKS)/bins/linux-stable-$(VERSION)

# basic flags
FLAGS := -std=gnu89 -I$(PWD)/../

# includes
FLAGS += -nostdinc \
				 -isystem /usr/lib/gcc/x86_64-linux-gnu/5.4.0/include \
				 -I$(LINUX_SRC)/arch/x86/include \
				 -I$(LINUX_OBJ)/arch/x86/include \
				 -I$(LINUX_OBJ)/arch/x86/include/generated \
				 -I$(LINUX_OBJ)/arch/x86/include/generated/uapi \
				 -I$(LINUX_SRC)/include \
				 -I$(LINUX_OBJ)/include \
				 -I$(LINUX_OBJ)/include/generated \
				 -I$(LINUX_OBJ)/include/generated/uapi \
				 -I$(LINUX_BIN)/include \
				 -I$(LINUX_BIN)/include/uapi \
				 -include $(LINUX_SRC)/include/linux/kconfig.h

# defines
FLAGS += -D__KERNEL__ \
				 -DCONFIG_X86_X32_ABI \
				 -DCONFIG_AS_CFI=1 \
				 -DCONFIG_AS_CFI_SIGNAL_FRAME=1 \
				 -DCONFIG_AS_CFI_SECTIONS=1 \
				 -DCONFIG_AS_FXSAVEQ=1 \
				 -DCONFIG_AS_SSSE3=1 \
				 -DCONFIG_AS_CRC32=1 \
				 -DCONFIG_AS_AVX=1 \
				 -DCONFIG_AS_AVX2=1 \
				 -DCONFIG_AS_SHA1_NI=1 \
				 -DCONFIG_AS_SHA256_NI=1 \
				 -DCC_USING_FENTRY \
				 -DMODULE \
				 -D"KBUILD_STR(s)=\#s" \
				 -D"KBUILD_BASENAME=KBUILD_STR(prog)"  \
				 -D"KBUILD_MODNAME=KBUILD_STR(prog)"

# quite warnings from llvm
FLAGS += -Wno-address-of-packed-member \
				 -Wno-pointer-sign \
				 -Wno-gnu-variable-sized-type-not-at-end \
				 -Wno-unused-result

# rules 
all: build check

%.bc: %.c
	$(CC) $(FLAGS) -S -emit-llvm -o $@ $<

%.sym: %.bc
	@echo $<
	@$(CK) -symf $@ $< >/dev/null
	@echo ""

$(SYMS): .FORCE

.FORCE:

# targets
build: $(BCFS)

check: $(SYMS)

clean:
	rm -rf *.o *.bc *.sym

.PHONY: build check clean all .FORCE
