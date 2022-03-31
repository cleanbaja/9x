AC_INIT(9x, 0.0.1, https://github.com/cleanbaja/CrecentOS/issues, ninex)
AC_COPYRIGHT([Copyright (c) 2022 cleanbaja, All Rights Reserved.])

# Search for the programs we need...
AC_PROG_INSTALL
AC_CHECK_PROG([NASM_LOCATED], [nasm], [yes])
if ! test "x$NASM_LOCATED" = "xyes"; then
    AC_MSG_ERROR([NASM (Netwide Assembler) is not present on this system, please install before running!])
fi

# Kernel features that can be enabled/disabled
AC_ARG_ENABLE(hardened-kernel,
    AS_HELP_STRING([--enable-hardened-kernel], [enables kernel security features like UBSAN/STK Protector]),
    HARDENED_BUILD="$enableval")
AC_SUBST(HARDENED_BUILD, [$HARDENED_BUILD])

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
