AC_INIT(9x, 0.0.7, https://github.com/cleanbaja/CrecentOS/issues, ninex)
AC_COPYRIGHT([Copyright (c) 2022 cleanbaja, All Rights Reserved.])

# Search for the programs we need...
AC_PROG_INSTALL
AC_CHECK_PROG([NASM_LOCATED], [nasm], [yes])
if ! test "x$NASM_LOCATED" = "xyes"; then
    AC_MSG_ERROR([NASM (Netwide Assembler) is not present on this system, please install before running!])
fi

# Kernel features that can be enabled/disabled
#----------------------------------------------#
AC_ARG_ENABLE(earlyconsole,
    AS_HELP_STRING([--disable-earlyconsole], [disables the use of the stivale2 terminal as a early logging output, required if not using limine]),
    LIMINE_EARLYCONSOLE="$enableval")

# If we're on aarch64, disable the LIMINE_EARLYCONSOLE, even if the flag isn't passed
if test "x$ARCH" = "xaarch64"; then
    LIMINE_EARLYCONSOLE=""
fi
if test "x$LIMINE_EARLYCONSOLE" = "xno"; then
    LIMINE_EARLYCONSOLE=""
else
    LIMINE_EARLYCONSOLE="yes"
fi
AC_SUBST(LIMINE_EARLYCONSOLE, [$LIMINE_EARLYCONSOLE])

#----------------------------------------------#

# Kernel arguments
AC_ARG_VAR(ARCH, [Kernel CPU Architecture [x86_64]])
AC_ARG_VAR(KERNEL_CFLAGS, [Kernel C Flags [-O2 -g]])
AC_ARG_VAR(KERNEL_LDFLAGS, [Kernel Linker Flags [empty]])
test "x$ARCH" = "x" && ARCH='x86_64'
test "x$KERNEL_CFLAGS" = "x" && KERNEL_CFLAGS='-O2 -g'
test "x$KERNEL_LDFLAGS" = "x" && KERNEL_LDFLAGS=' '

# Find the tools
AC_CHECK_TOOL(CC, gcc, :)
AC_CHECK_TOOL(LD, ld, :)
AC_CHECK_TOOL(AR, ar, :)

# Then finish the remaining parts of configuration
AC_PREFIX_DEFAULT(/boot)
AC_CONFIG_FILES(GNUmakefile config.h)
AC_OUTPUT
