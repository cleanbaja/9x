AC_INIT([9x],[m4_esyscmd(./version.sh)],[https://github.com/cleanbaja/9x/issues],[ninex])
AC_CONFIG_SRCDIR([LICENSE.md])
AC_CONFIG_AUX_DIR([build-aux])
AC_PREREQ([2.71])

# Build Variables
BUILDDIR="$(pwd -P)"
AC_SUBST(BUILDDIR, [$BUILDDIR])
AC_ARG_VAR(CFLAGS, [C Compiler Flags [-Os -pipe -g]])
AC_ARG_VAR(KERNEL_CC, [Kernel C Compiler [clang]])
AC_ARG_VAR(KERNEL_LD, [Kernel ELF Linker [ld.lld]])

# Defaults
test "x$CFLAGS" = "x" && CFLAGS='-Os -pipe -g'
test "x$KERNEL_CC" = "x" && KERNEL_CC='clang'
test "x$KERNEL_LD" = "x" && KERNEL_LD='ld.lld'

# Runtime sanitizers
AC_ARG_WITH([sanitizers],
  [AS_HELP_STRING([--with-sanitizers],[use runtime kernel sanitizers (ubsan, kasan and lksan)])],
  [sanitizers=$withval], [sanitizers=""])

# Build Arch
m4_define([DEFAULT_ARCH], [x86_64])
AC_ARG_VAR(ARCH, [Target Architecture @<:@]DEFAULT_ARCH[@:>@])
test "x$ARCH" = "x" && ARCH="DEFAULT_ARCH"

# Build Tools
AC_PROG_INSTALL
AC_CHECK_TOOL(CC_LOCATED, $KERNEL_CC, [yes])
AC_CHECK_TOOL(LD_LOCATED, $KERNEL_LD, [yes])
AC_CHECK_TOOL(AR_LOCATED, ar, [yes])
AC_CHECK_PROG([NASM_LOCATED], [nasm], [yes])

# Confirm tools are present
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

# Apply the requested sanitizers
if test "$sanitizers" = "all"; then
    sanitizers="ubsan,kasan,lksan"
fi
idx=1
while iter=$(echo "$sanitizers,"|cut -d\, -f$idx) ; test -n "$iter"; do
  case $iter in
    ubsan)
      CFLAGS="$CFLAGS -fsanitize=undefined -DUBSAN=1"
      ;;
    kasan)
      CFLAGS="$CFLAGS -fasan-shadow-offset=0xdfffe00000000000 -fsanitize=kernel-address -DKASAN=1"
      if ! test "$KERNEL_CC" = *"gcc"*; then
         AC_MSG_ERROR(kasan currently only works with GCC!)
      fi
      ;;
    lksan)
      CFLAGS="$CFLAGS -DSANITIZE_LOCKS=1"
      ;;
    *)
      AC_MSG_ERROR(unknown sanitizer $iter!)
      ;;
  esac

  idx=$((idx+1))
done

# Link the arch-specific include dir into .tmpinc
cur_src_dir=`readlink -f $srcdir`
rm -rf $BUILDDIR/.tmpinc && mkdir $BUILDDIR/.tmpinc
ln -sf $cur_src_dir/include/arch/$ARCH $BUILDDIR/.tmpinc/arch

# Finally, finish up configuration
AC_PREFIX_DEFAULT()
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

