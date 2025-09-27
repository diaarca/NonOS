# Copyright (c) 2011 Vincent Danjean <Vincent.Danjean@imag.fr>

.PHONY: all clean depend print build check mips-progs bin

GIT_INIT_COMMIT:=$(shell git rev-list --max-parents=0 HEAD)
ifeq ($(GIT_INIT_COMMIT),a8550946e865bff4dd452841e71b23387243de51)
else
# gitlab CI does checkouts with a depth of 50...
ifeq ($(shell git rev-parse --is-shallow-repository),true)
$(warning partial git checkout (shallow repository). Assuming ok)
else
$(error Your git does not seem to be correctly setup. Aborting.)
endif
endif

# By default, build the check target so that we check that initial
# nachos code still compile
check:

all: build

build: bin
	$(MAKE) -C $@
	@echo "******* Success: 2023 Nachos project compiled"

.PHONY: bin
bin:
	$(MAKE) -C bin

mips-progs:
	$(MAKE) -C build $@

check: build

clean:
	@set -e; \
	for i in build bin; do \
		$(MAKE) -C $$i $@; \
	done

## LPR	:=	echo
LPR	:=	enscript --columns=2  --borders -r --landscape \
		--media=A4 --verbose --pretty-print --toc 

print:
	-for dir in machine threads userprog filesys network vm test; \
	do \
		ls -f \
		$$dir/Makefile* \
		$${dir}/*.h \
		$${dir}/*.cc $${dir}/*.S \
		$${dir}/*.c; \
	done > list
	$(LPR) `cat list` 

INDENT	:= clang-format -style="{BasedOnStyle: llvm, IndentWidth: 4}" -i
DIRS_INDENT	:= threads userprog test

indent:
	-for dir in machine $(DIRS_INDENT) test; do \
		ls -f \
		$${dir}/*.h \
		$${dir}/*.cc \
		$${dir}/*.c; \
	done > list
	for file in `cat list`; do \
		echo $${file}; $(INDENT) $${file}; \
	done
