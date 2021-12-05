#!/bin/sh


printf "#include <stdint.h>\n\nstruct kernel_symbol" > $1/ksym.gen.c
printf " {\n\tuint64_t addr;\n\tchar* name;\n};\n\n" >> $1/ksym.gen.c
printf "const struct kernel_symbol ksym_table[] = {\n" >> $1/ksym.gen.c
nm $2 | grep " T " | awk '{ print "    { .addr = 0x"$1", .name = \""$3"\" }," }' | sort >> $1/ksym.gen.c
printf "    { .addr = UINT64_MAX, .name = \"\" }\n};\n\n" >> $1/ksym.gen.c
