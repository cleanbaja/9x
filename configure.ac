AC_INIT(9x, m4_esyscmd(./version.sh), https://github.com/cleanbaja/9x/issues, ninex)
AC_CONFIG_SRCDIR([LICENSE.md])
AC_CONFIG_AUX_DIR([build-aux])
AC_PREREQ(2.69)

# Build Arch
m4_define([DEFAULT_ARCH], [x86_64])
AC_ARG_VAR(ARCH, [Target Architecture @<:@]DEFAULT_ARCH[@:>@])
test "x$ARCH" = "x" && ARCH="DEFAULT_ARCH"

# Build Variables
BUILDDIR="$(pwd -P)"
AC_SUBST(BUILDDIR, [$BUILDDIR])
AC_ARG_VAR(CFLAGS, [C Compiler Flags [-O2 -g]])
AC_ARG_VAR(KERNEL_CC, [Kernel C Compiler [clang]])
AC_ARG_VAR(KERNEL_LD, [Kernel ELF Linker [ld.lld]])

# Defaults
test "x$CFLAGS" = "x" && CFLAGS='-O2 -g'
test "x$KERNEL_CC" = "x" && KERNEL_CC='clang'
test "x$KERNEL_LD" = "x" && KERNEL_LD='ld.lld'

# Build Tools
AC_PROG_INSTALL
AC_CHECK_TOOL(CC_LOCATED, $KERNEL_CC, [yes])
AC_CHECK_TOOL(LD_LOCATED, $KERNEL_LD, [yes])
AC_CHECK_TOOL(AR_LOCATED, ar, [yes])
AC_CHECK_PROG([NASM_LOCATED], [nasm], [yes])

# Affirm tools are present
if test "x$CC_LOCATED" = "xyes"; then
    AC_MSG_ERROR([$KERNEL_CC (C Compiler) is not present on this system, please install before running!])
fi
if test "x$LD_LOCATED" = "xyes"; then
    AC_MSG_ERROR([$KERNEL_LD (Object Linker) is not present on this system, please install before running!])
fi
if test "x$AR_LOCATED" = "xyes"; then
    AC_MSG_ERROR([ar (Object Archiver) is not present on this system, please install before running!])
fi
if ! test "x$NASM_LOCATED" = "xyes"; then
    AC_MSG_ERROR([NASM (Netwide Assembler) is not present on this system, please install before running!])
fi

AC_PREFIX_DEFAULT()
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

