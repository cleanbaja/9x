#!/bin/sh

rm -f $1
printf "#include <stdint.h>\n\nstruct kernel_symbol" > $1
printf " {\n\tuint64_t addr;\n\tchar* name;\n};\n\n" >> $1
printf "const struct kernel_symbol ksym_table[] = {\n" >> $1
nm $2 | grep " T " | awk '{ print "    { .addr = 0x"$1", .name = \""$3"\" }," }' | sort >> $1
printf "    { .addr = UINT64_MAX, .name = \"\" }\n};\n\n" >> $1
printf "const int ksym_elem_count = sizeof(ksym_table) / sizeof(*ksym_table);\n" >> $1

