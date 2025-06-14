#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements. See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership. The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the License for the
# specific language governing permissions and limitations
# under the License.
#

AC_PREREQ(2.65)
AC_CONFIG_MACRO_DIR([./aclocal])

AC_INIT([thrift], [0.22.0])

AC_CONFIG_AUX_DIR([.])

AM_INIT_AUTOMAKE([1.13 subdir-objects tar-ustar foreign])
PKG_PROG_PKG_CONFIG

# Modified configure.ac for custom DPDK installation

# Check for DPDK
AC_ARG_WITH([dpdk],
    [AS_HELP_STRING([--with-dpdk=PATH],
        [path to DPDK installation @<:@default=no@:>@])],
    [with_dpdk=$withval],
    [with_dpdk=no])

# Check for pkg-config first
PKG_PROG_PKG_CONFIG

AS_IF([test "x$with_dpdk" != xno],
    [AS_IF([test "x$with_dpdk" != xyes],
        # Custom path provided
        [DPDK_CFLAGS="-I$with_dpdk/include/dpdk"
         PKG_CONFIG_PATH="$with_dpdk/lib/x86_64-linux-gnu/pkgconfig:$with_dpdk/lib64/pkgconfig:$with_dpdk/lib/pkgconfig:$PKG_CONFIG_PATH"
         DPDK_LIBS="`pkg-config --libs libdpdk`"] # -lrte_net_mlx5 -lrte_common_mlx5"],
        # No path provided, try pkg-config
        [PKG_CHECK_MODULES([DPDK], [libdpdk],
            [],
            [AC_MSG_ERROR([DPDK installation not found. Please specify path using --with-dpdk=PATH])])])
    ])

# Verify DPDK headers are available
AS_IF([test "x$with_dpdk" != xno],
    [save_CPPFLAGS="$CPPFLAGS"
     DPDK_ROOT="/home/chumeike/dpdk-24.07"
     CPPFLAGS="$CPPFLAGS $DPDK_CFLAGS \ 
            -I$DPDK_ROOT/drivers/net/mlx5 \
            -I$DPDK_ROOT/drivers/common/mlx5 \
            -I$DPDK_ROOT/build/drivers/common/mlx5 \
            -I$DPDK_ROOT/drivers/common/mlx5/linux \
            -I$DPDK_ROOT/drivers/bus/pci \
            -I$DPDK_ROOT/drivers/common/dpaax \
            -I$DPDK_ROOT/lib/eal/include \
            -I$DPDK_ROOT/lib/ethdev \
            -I$DPDK_ROOT/drivers/common \
            -I$DPDK_ROOT/lib/eal/common \
            -I$DPDK_ROOT/lib/eal/linux/include \
            -I$DPDK_ROOT/lib/eal/x86/include \
            -I$DPDK_ROOT/config \
            -I$DPDK_ROOT/lib/eal \
            -I$DPDK_ROOT"
     AC_CHECK_HEADERS([rte_eal.h rte_ethdev.h rte_mbuf.h rte_mempool.h],
        [],
        [AC_MSG_ERROR([Required DPDK headers not found])],
        [[#include <rte_config.h>]])
     CPPFLAGS="$save_CPPFLAGS"
    ])

# Export variables
AC_SUBST([DPDK_LIBS])
AC_SUBST([DPDK_CFLAGS])

# Define HAVE_DPDK
AC_DEFINE([HAVE_DPDK], [1], [Define if you have DPDK])

AC_ARG_VAR([PY_PREFIX], [Prefix for installing Python modules.
                         (Normal --prefix is ignored for Python because
                         Python has different conventions.)
                         Default = "/usr"])
AS_IF([test "x$PY_PREFIX" = x], [PY_PREFIX="/usr"])

AC_ARG_VAR([JAVA_PREFIX], [Prefix for installing the Java lib jar.
                           Default = "/usr/local/lib"])
AS_IF([test "x$JAVA_PREFIX" != x], [JAVA_PREFIX="$JAVA_PREFIX/usr/local/lib"],
      [test "x$PREFIX" != x], [JAVA_PREFIX="$PREFIX/usr/local/lib"],
      [JAVA_PREFIX="/usr/local/lib"])

AC_ARG_VAR([RUBY_PREFIX], [Prefix for installing Ruby modules.
                           (Normal --prefix is ignored for Ruby because
                           Ruby has different conventions.)
                           Default = none, let ruby setup decide])

AC_ARG_VAR([PHP_PREFIX], [Prefix for installing PHP modules.
                         (Normal --prefix is ignored for PHP because
                         PHP has different conventions.)
                         Default = "/usr/lib/php"])
AS_IF([test "x$PHP_PREFIX" = x], [PHP_PREFIX="/usr/lib/php"])

AC_ARG_VAR([PHP_CONFIG_PREFIX],
           [Prefix for installing PHP extension module .ini file.
            (Normal --prefix is ignored for PHP because PHP has
             different conventions.)
           Default = "/etc/php.d"])
AS_IF([test "x$PHP_CONFIG_PREFIX" = x], [PHP_CONFIG_PREFIX="/etc/php.d"])

AC_ARG_VAR([INSTALLDIRS], [When installing Perl modules, specifies which
                           of the sets of installation directories
                           to choose: perl, site or vendor.
                           Default = "vendor"])
AS_IF([test "x$INSTALLDIRS" = x], [INSTALLDIRS="vendor"])

AC_ARG_VAR([PERL_PREFIX], [Prefix for installing Perl modules.
                           (Normal --prefix is ignored for Perl because
                           Perl has different conventions.)
                           Ignored, when INSTALLDIRS set to site or vendor.
                           Default = "/usr/local/lib"])
AS_IF([test "x$PERL_PREFIX" = x], [PERL_PREFIX="/usr/local"])

AC_ARG_VAR([D_IMPORT_PREFIX], [Prefix for installing D modules.
                           [INCLUDEDIR/d2]])
AS_IF([test "x$D_IMPORT_PREFIX" = x], [D_IMPORT_PREFIX="${includedir}/d2"])

AC_ARG_VAR([DMD_LIBEVENT_FLAGS], [DMD flags for linking libevent (auto-detected if not set).])
AC_ARG_VAR([DMD_OPENSSL_FLAGS], [DMD flags for linking OpenSSL (auto-detected if not set).])

AC_ARG_VAR([THRIFT], [Path to the thrift tool (needed for cross-compilation).])
AS_IF([test "x$THRIFT" = x], [THRIFT=`pwd`/compiler/cpp/thrift])

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_PROG_BISON(2.5)
AC_PROG_YACC
AC_PROG_LEX
AM_PROG_LEX
AC_PROG_LN_S
AC_PROG_MKDIR_P
AC_PROG_AWK
AC_PROG_RANLIB

AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])

AM_EXTRA_RECURSIVE_TARGETS([style])
AC_SUBST(CPPSTYLE_CMD, 'find . -type f \( -iname "*.h" -or -iname "*.cpp" -or -iname "*.cc" -or -iname "*.tcc" \) -printf "Reformatting: %h/%f\n" -exec clang-format -i {} \;')
# '
# The above comment is to fix editor syntax highlighting

AC_ARG_ENABLE([libs],
  AS_HELP_STRING([--enable-libs], [build the Apache Thrift libraries [default=yes]]),
  [], enable_libs=yes
)
have_libs=yes
if test "$enable_libs" = "no"; then
  have_libs="no"
  with_cpp="no"
  with_c_glib="no"
  with_cl="no"
  with_java="no"
  with_kotlin="no"
  with_python="no"
  with_py3="no"
  with_ruby="no"
  with_haxe="no"
  with_netstd="no"
  with_perl="no"
  with_php="no"
  with_php_extension="no"
  with_dart="no"
  with_erlang="no"
  with_go="no"
  with_d="no"
  with_nodejs="no"
  with_nodets="no"
  with_lua="no"
  with_rs="no"
  with_swift="no"
fi

AX_THRIFT_LIB(cpp, [C++], yes)
have_cpp=no
if test "$with_cpp" = "yes";  then
  AX_BOOST_BASE([1.56.0])
  if test "x$succeeded" = "xyes" ; then
    AC_SUBST([BOOST_LIB_DIR], [$(echo "$BOOST_LDFLAGS" | sed -e 's/^\-L//')])
    AC_SUBST([BOOST_CHRONO_LDADD], [$(echo "$BOOST_LIB_DIR/libboost_chrono.a")])
    AC_SUBST([BOOST_FILESYSTEM_LDADD], [$(echo "$BOOST_LIB_DIR/libboost_filesystem.a")])
    AC_SUBST([BOOST_SYSTEM_LDADD], [$(echo "$BOOST_LIB_DIR/libboost_system.a")])
    AC_SUBST([BOOST_TEST_LDADD], [$(echo "$BOOST_LIB_DIR/libboost_unit_test_framework.a")])
    AC_SUBST([BOOST_THREAD_LDADD], [$(echo "$BOOST_LIB_DIR/libboost_thread.a")])
    have_cpp="yes"
  fi

  AX_LIB_EVENT([2.0])
  have_libevent=$success

  AX_LIB_ZLIB([1.2.3])
  have_zlib=$success

  AX_THRIFT_LIB(qt5, [Qt5], yes)
  have_qt5=no
  qt_reduce_reloc=""
  if test "$with_qt5" = "yes";  then
    PKG_CHECK_MODULES([QT5], [Qt5Core >= 5.0, Qt5Network >= 5.0],
                      [have_qt5=yes;qt_reduce_reloc=`$PKG_CONFIG --variable=qt_config Qt5Core | grep "reduce_relocations"`],
                      [have_qt5=no])
  fi
  if test "$have_qt5" = "yes"; then
    AC_PATH_PROGS([QT5_MOC], [moc-qt5 moc], "fail")
    if test "$QT5_MOC" = "fail"; then
      have_qt5=no
    fi
  fi
fi
AM_CONDITIONAL([WITH_CPP], [test "$have_cpp" = "yes"])
AM_CONDITIONAL([AMX_HAVE_LIBEVENT], [test "$have_libevent" = "yes"])
AM_CONDITIONAL([AMX_HAVE_ZLIB], [test "$have_zlib" = "yes"])
AM_CONDITIONAL([AMX_HAVE_QT5], [test "$have_qt5" = "yes"])
AM_CONDITIONAL([QT5_REDUCE_RELOCATIONS], [test "x$qt_reduce_reloc" != "x"])

AX_THRIFT_LIB(c_glib, [C (GLib)], yes)
if test "$with_c_glib" = "yes"; then
  PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.0], have_glib2=yes, have_glib2=no)
  PKG_CHECK_MODULES([GOBJECT], [gobject-2.0 >= 2.0], have_gobject2=yes, have_gobject2=no)
  if test "$have_glib2" = "yes" -a "$have_gobject2" = "yes" ; then
    AC_PATH_PROG([GSETTINGS], [gsettings])
    have_c_glib="yes"
  fi
fi
AM_CONDITIONAL(WITH_C_GLIB, [test "$have_glib2" = "yes" -a "$have_gobject2" = "yes"])

# echo "OpenSSL check"
if test "$have_cpp" = "yes" -o "$have_c_glib" = "yes";  then
  # echo "Have cpp or c so we check for OpenSSL"
  AX_CHECK_OPENSSL()
fi


AX_THRIFT_LIB(java, [Java], yes)
if test "$with_java" = "yes";  then
  AX_JAVAC_AND_JAVA
  AC_PATH_PROG([ANT], [ant])
  AC_PATH_PROG([GRADLE], [gradle])
  AX_CHECK_ANT_VERSION($ANT, 1.7)
  AC_SUBST(CLASSPATH)
  AC_SUBST(ANT_FLAGS)
  AC_SUBST(GRADLE_OPTS)
  if test "x$JAVA" != "x" && test "x$JAVAC" != "x" && test "x$ANT" != "x" && test "x$GRADLE" != "x" ; then
    have_java="yes"
  fi
fi
AM_CONDITIONAL([WITH_JAVA], [test "$have_java" = "yes"])

AX_THRIFT_LIB(kotlin, [Kotlin], yes)
if test "$with_kotlin" = "yes";  then
  AX_JAVAC_AND_JAVA
  AC_PATH_PROG([GRADLE], [gradle])
  AC_SUBST(CLASSPATH)
  AC_SUBST(GRADLE_OPTS)
  if test "x$JAVA" != "x" && test "x$JAVAC" != "x" && test "x$GRADLE" != "x" ; then
    have_kotlin="yes"
  fi
fi
AM_CONDITIONAL([WITH_KOTLIN], [test "$have_kotlin" = "yes"])

AX_THRIFT_LIB(erlang, [Erlang], yes)
if test "$with_erlang" = "yes";  then
  AC_ERLANG_PATH_ERL
  AC_ERLANG_PATH_ERLC
  AC_PATH_PROG([REBAR], [rebar3])
  if test -n "$ERLC" ; then
    AC_ERLANG_SUBST_LIB_DIR
    # Install into the detected Erlang directory instead of $libdir/erlang/lib
    ERLANG_INSTALL_LIB_DIR="$ERLANG_LIB_DIR"
    AC_ERLANG_SUBST_INSTALL_LIB_SUBDIR(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
  fi
  if test -n "$ERL" -a -n "$ERLC" && test "x$REBAR" != "x" ; then
    have_erlang="yes"

  fi
fi
AM_CONDITIONAL(WITH_ERLANG, [test "$have_erlang" = "yes"])

AX_THRIFT_LIB(nodejs, [Nodejs], yes)
have_nodejs=no
if test "$with_nodejs" = "yes"; then
  AC_PATH_PROGS([NODEJS], [nodejs node])
  AC_PATH_PROG([NPM], [npm])
  if test "x$NODEJS" != "x" -a "x$NPM" != "x"; then
    have_nodejs="yes"
  fi
fi
AM_CONDITIONAL(WITH_NODEJS, [test "$have_nodejs" = "yes"])
AM_CONDITIONAL(HAVE_NPM, [test "x$NPM" != "x"])

AX_THRIFT_LIB(nodets, [Nodets], yes)
have_nodets=no
if test "$with_nodets" = "yes"; then
  AC_PATH_PROGS([NODETS], [nodets node])
  AC_PATH_PROG([NPM], [npm])
  if test "x$NODETS" != "x" -a "x$NPM" != "x"; then
    have_nodets="yes"
  fi
fi
AM_CONDITIONAL(WITH_NODETS, [test "$have_nodets" = "yes"])
AM_CONDITIONAL(HAVE_NPM, [test "x$NPM" != "x"])

AX_THRIFT_LIB(lua, [Lua], yes)
have_lua=no
if test "$with_lua" = "yes"; then
  AX_PROG_LUA(5.2,, have_lua="yes", have_lua="no")
  if test "$have_lua" = "yes"; then
    AX_LUA_HEADERS(, have_lua="no")
    AX_LUA_LIBS(, have_lua="no")
  fi
fi
AM_CONDITIONAL(WITH_LUA, [test "$have_lua" = "yes"])

# Find python regardless of with_python value, because it's needed by make cross
AM_PATH_PYTHON(2.6,, :)
AX_THRIFT_LIB(python, [Python], yes)
if test "$with_python" = "yes";  then
  if test -n "$PYTHON"; then
    have_python="yes"
  fi
  AC_PATH_PROG([TRIAL], [trial])
  if test -n "$TRIAL"; then
    have_trial="yes"
  fi
fi
AM_CONDITIONAL(WITH_PYTHON, [test "$have_python" = "yes"])
AM_CONDITIONAL(WITH_TWISTED_TEST, [test "$have_trial" = "yes"])

# Find "python3" executable.
# It's distro specific and far from ideal but needed to cross test py2-3 at once.
# TODO: find "python2" if it's 3.x
have_py3="no"
AX_THRIFT_LIB(py3, [Py3], yes)
if test "$with_py3" = "yes"; then
  # if $PYTHON is 2.x then search for python 3. otherwise, $PYTHON is already 3.x
  if $PYTHON --version 2>&1 | grep -q "Python 2"; then
    AC_PATH_PROGS([PYTHON3], [python3 python3.8 python38 python3.7 python37 python3.6 python36 python3.5 python35 python3.4 python34])
    if test -n "$PYTHON3"; then
      have_py3="yes"
    fi
  elif $PYTHON --version 2>&1 | grep -q "Python 3"; then
    have_py3="yes"
    PYTHON3=$PYTHON
  fi
fi
AM_CONDITIONAL(WITH_PY3, [test "$have_py3" = "yes"])

AX_THRIFT_LIB(perl, [Perl], yes)
if test "$with_perl" = "yes"; then
  AC_PATH_PROG([PERL], [perl])
  if test -n "$PERL" ; then
    AX_PROG_PERL_MODULES([Bit::Vector],[success="yes"],[success="no"])
    have_perl_bit_vector="$success"
    AX_PROG_PERL_MODULES([Class::Accessor],[success="yes"],[success="no"])
    have_perl_class_accessor="$success"
  fi
  if test -n "$PERL" -a "$have_perl_bit_vector" = "yes" ; then
    if test -n "$PERL" -a "$have_perl_class_accessor" = "yes" ; then
      have_perl="yes"
    fi
  fi
fi
AM_CONDITIONAL(WITH_PERL, [test "$have_perl" = "yes"])

AX_THRIFT_LIB(php, [PHP], yes)
if test "$with_php" = "yes"; then
  AC_PATH_PROG([PHP], [php])
  if test -n "$PHP" ; then
    have_php="yes"
  fi
fi
AM_CONDITIONAL(WITH_PHP, [test "$have_php" = "yes"])

AX_THRIFT_LIB(php_extension, [PHP_EXTENSION], yes)
if test "$with_php_extension" = "yes"; then
  if test -f "lib/php/src/ext/thrift_protocol/configure"; then
    AC_PATH_PROG([PHP_CONFIG], [php-config])
    if test -n "$PHP_CONFIG" ; then
      AC_CONFIG_SUBDIRS([lib/php/src/ext/thrift_protocol])
      have_php_extension="yes"
    fi
  fi
fi
AM_CONDITIONAL(WITH_PHP_EXTENSION, [test "$have_php_extension" = "yes"])

AX_THRIFT_LIB(dart, [DART], yes)
if test "$with_dart" = "yes"; then
  AC_PATH_PROG([DART], [dart])
  AC_PATH_PROG([DARTPUB], [pub])
  if test "x$DART" != "x" -a "x$DARTPUB" != "x"; then
    have_dart="yes"
  fi
fi
AM_CONDITIONAL(WITH_DART, [test "$have_dart" = "yes"])

AX_THRIFT_LIB(ruby, [Ruby], yes)
have_ruby=no
if test "$with_ruby" = "yes"; then
  AC_PATH_PROG([RUBY], [ruby])
  AC_PATH_PROG([BUNDLER], [bundle])
  if test "x$RUBY" != "x" -a "x$BUNDLER" != "x"; then
    have_ruby="yes"
  fi
fi
AM_CONDITIONAL(WITH_RUBY, [test "$have_ruby" = "yes"])
AM_CONDITIONAL(HAVE_BUNDLER, [test "x$BUNDLER" != "x"])

AX_THRIFT_LIB(go, [Go], yes)
if test "$with_go" = "yes";  then
  AC_PATH_PROG([GO], [go])
  if [[ -x "$GO" ]] ; then
    AS_IF([test -n "$GO"],[
      ax_go_version="1.4"
      ax_go17_version="1.7"
      ax_go118_version="1.18"

      AC_MSG_CHECKING([for Go version])
      golang_version=`$GO version 2>&1 | $SED -e 's/\(go \)\(version \)\(go\)\(@<:@0-9@:>@.@<:@0-9@:>@.@<:@0-9@:>@\)\(@<:@\*@:>@*\).*/\4/'`
      AC_MSG_RESULT($golang_version)
      AC_SUBST([golang_version],[$golang_version])
      AX_COMPARE_VERSION([$ax_go_version],[le],[$golang_version],[
      :
        have_go="yes"
      ],[
      :
        have_go="no"
      ])
      AX_COMPARE_VERSION([$golang_version],[lt],[$ax_go17_version],[
      :
        go_version_lt_17="yes"
      ],[
      :
        go_version_lt_17="no"
      ])
      AX_COMPARE_VERSION([$golang_version],[ge],[$ax_go118_version],[
      :
        go_version_ge_118="yes"
      ],[
      :
        go_version_ge_118="no"
      ])
    ],[
      AC_MSG_WARN([could not find Go ])
      have_go="no"
    ])
  fi
fi
AM_CONDITIONAL(WITH_GO, [test "$have_go" = "yes"])
AM_CONDITIONAL([GOVERSION_LT_17], [test "$go_version_lt_17" = "yes"])
AM_CONDITIONAL([GOVERSION_GE_118], [test "$go_version_ge_118" = "yes"])

AX_THRIFT_LIB(swift, [Swift], yes)
have_swift="no"
if test "$with_swift" = "yes"; then
  AC_PATH_PROG([SWIFT], [swift])
  if test "x$SWIFT" != "x" -a "x$SWIFT" != "x"; then
    have_swift="yes"
  fi
fi
AM_CONDITIONAL([WITH_SWIFT], [test "$have_swift" = "yes"])

AX_THRIFT_LIB(rs, [Rust], yes)
have_rs="no"
if test "$with_rs" = "yes";  then
  AC_PATH_PROG([CARGO], [cargo])
  AC_PATH_PROG([RUSTC], [rustc])
  if [[ -x "$CARGO" ]] && [[ -x "$RUSTC" ]]; then
      min_rustc_version="1.13"

      AC_MSG_CHECKING([for rustc version])
      rustc_version=`$RUSTC --version 2>&1 | $SED -e 's/\(rustc \)\([0-9]\)\.\([0-9][0-9]*\)\.\([0-9][0-9]*\).*/\2.\3/'`
      AC_MSG_RESULT($rustc_version)
      AC_SUBST([rustc_version],[$rustc_version])

      AX_COMPARE_VERSION([$min_rustc_version],[le],[$rustc_version],[
      :
        have_rs="yes"
      ],[
      :
        have_rs="no"
      ])
  fi
fi
AM_CONDITIONAL(WITH_RS, [test "$have_rs" = "yes"])

AX_THRIFT_LIB(cl, [Common Lisp], yes)
have_cl="no"
if test "$with_cl" = "yes";  then
  AC_PATH_PROG([SBCL], [sbcl])
  if test "x$SBCL" != "x"; then
    have_cl="yes"
  fi
fi
AM_CONDITIONAL(WITH_CL, [test "$have_cl" = "yes"])

AX_THRIFT_LIB(haxe, [Haxe], yes)
if test "$with_haxe" = "yes";  then
  AC_PATH_PROG([HAXE], [haxe])
  if [[ -x "$HAXE" ]] ; then
    AX_PROG_HAXE_VERSION( [4.2.1], have_haxe="yes", have_haxe="no")
  fi
fi
AM_CONDITIONAL(WITH_HAXE, [test "$have_haxe" = "yes"])


AX_THRIFT_LIB(netstd, [.NET Core], yes)
if test "$with_netstd" = "yes";  then
  AC_PATH_PROG([DOTNETCORE], [dotnet])
  if [[ -x "$DOTNETCORE" ]] ; then
    AX_PROG_DOTNETCORE_VERSION( [7.0.0], have_netstd="yes", have_netstd="no")
  fi
fi
AM_CONDITIONAL(WITH_DOTNET, [test "$have_netstd" = "yes"])


AX_THRIFT_LIB(d, [D], yes)
if test "$with_d" = "yes";  then
  AX_DMD
  AC_SUBST(DMD)
  if test "x$DMD" != "x"; then
    have_d="yes"
  fi
fi

# Determine actual name of the generated D library for use in the command line
# when compiling tests. This is needed because the -l<lib> syntax doesn't work
# with OPTLINK (Windows).
lib_prefix=lib
lib_suffix=a
case "$host_os" in
  cygwin* | mingw* | pw32* | cegcc*)
    lib_prefix=""
    lib_suffix=lib
    ;;
esac
D_LIB_NAME="${lib_prefix}thriftd.${lib_suffix}"
AC_SUBST(D_LIB_NAME)
D_EVENT_LIB_NAME="${lib_prefix}thriftd-event.${lib_suffix}"
AC_SUBST(D_EVENT_LIB_NAME)
D_SSL_LIB_NAME="${lib_prefix}thriftd-ssl.${lib_suffix}"
AC_SUBST(D_SSL_LIB_NAME)

if test "$have_d" = "yes"; then
  AX_CHECK_D_MODULE(deimos.event2.event)
  have_deimos_event2=$success

  with_d_event_tests="no"
  if test "$have_deimos_event2" = "yes"; then
    if test "x$DMD_LIBEVENT_FLAGS" = "x"; then
      if test "$dmd_optlink" = "yes"; then
        AC_MSG_WARN([D libevent interface found, but cannot auto-detect \
linker flags for OPTLINK. Please set DMD_LIBEVENT_FLAGS manually.])
      else
        AX_LIB_EVENT([2.0])
        if test "$success" = "yes"; then
          DMD_LIBEVENT_FLAGS=$(echo "-fuse-ld=gold $LIBEVENT_LDFLAGS $LIBEVENT_LIBS" | \
            sed -e 's/^ *//g;s/ *$//g;s/^\(.\)/-L\1/g;s/  */ -L/g')
          with_d_event_tests="yes"
        else
          AC_MSG_WARN([D libevent interface present, but libevent library not found.])
        fi
      fi
    else
      with_d_event_tests="yes"
    fi
  fi

  AX_CHECK_D_MODULE(deimos.openssl.ssl)
  have_deimos_openssl=$success

  with_d_ssl_tests="no"
  if test "$have_deimos_openssl" = "yes"; then
    if test "x$DMD_OPENSSL_FLAGS" = "x"; then
      if test "$dmd_optlink" = "yes"; then
        AC_MSG_WARN([D OpenSSL interface found, but cannot auto-detect \
linker flags for OPTLINK. Please set DMD_OPENSSL_FLAGS manually.])
      else
        AX_CHECK_OPENSSL([with_d_ssl_tests="yes"])
        if test "$with_d_ssl_tests" = "yes"; then
          DMD_OPENSSL_FLAGS=$(echo "-fuse-ld=gold $OPENSSL_LDFLAGS $OPENSSL_LIBS" | \
            sed -e 's/^ *//g;s/ *$//g;s/^\(.\)/-L\1/g;s/  */ -L/g')
        else
          AC_MSG_WARN([D OpenSSL interface present, but OpenSSL library not found.])
        fi
      fi
    else
      with_d_ssl_tests="yes"
    fi
  fi
fi

AM_CONDITIONAL(WITH_D, [test "$have_d" = "yes"])
AM_CONDITIONAL(DMD_OPTLINK, [test "$dmd_optlink" = "yes"])
AC_SUBST(DMD_OF_DIRSEP, "$dmd_of_dirsep")
AM_CONDITIONAL(HAVE_DEIMOS_EVENT2, [test "$have_deimos_event2" = "yes"])
AM_CONDITIONAL(WITH_D_EVENT_TESTS, [test "$with_d_event_tests" = "yes"])
AC_SUBST(DMD_LIBEVENT_FLAGS)
AM_CONDITIONAL(HAVE_DEIMOS_OPENSSL, [test "$have_deimos_openssl" = "yes"])
AM_CONDITIONAL(WITH_D_SSL_TESTS, [test "$with_d_ssl_tests" = "yes"])
AC_SUBST(DMD_OPENSSL_FLAGS)

AC_ARG_ENABLE([tests],
  AS_HELP_STRING([--enable-tests], [build tests [default=yes]]),
  [], enable_tests=yes
)
have_tests=yes
if test "$enable_tests" = "no"; then
  have_tests="no"
fi
AM_CONDITIONAL(WITH_TESTS, [test "$have_tests" = "yes"])

AC_ARG_ENABLE([tutorial],
  AS_HELP_STRING([--enable-tutorial], [build tutorial [default=yes]]),
  [], enable_tutorial=yes
)
have_tutorial=yes
if test "$enable_tutorial" = "no"; then
  have_tutorial="no"
fi
AM_CONDITIONAL(WITH_TUTORIAL, [test "$have_tutorial" = "yes"])

AM_CONDITIONAL(MINGW, false)
case "${host_os}" in
*mingw*)
  mingw32_support="yes"
  AC_CHECK_HEADER(windows.h)
  AM_CONDITIONAL(MINGW, true)
  ;;
*)
  AC_SEARCH_LIBS([strerror],[cposix])
  ;;
esac

AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_C_RESTRICT

AC_HEADER_STDBOOL
AC_HEADER_STDC
AC_HEADER_TIME
AC_HEADER_SYS_WAIT
AC_TYPE_SIGNAL
AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([libintl.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([malloc.h])
AC_CHECK_HEADERS([netdb.h])
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([openssl/rand.h])
AC_CHECK_HEADERS([openssl/ssl.h])
AC_CHECK_HEADERS([openssl/x509v3.h])
AC_CHECK_HEADERS([poll.h])
AC_CHECK_HEADERS([pthread.h])
AC_CHECK_HEADERS([sched.h])
AC_CHECK_HEADERS([signal.h])
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([strings.h])
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADERS([sys/param.h])
AC_CHECK_HEADERS([sys/poll.h])
AC_CHECK_HEADERS([sys/resource.h])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([sys/un.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([wchar.h])

AC_CHECK_LIB(pthread, pthread_create)
dnl NOTE(dreiss): I haven't been able to find any really solid docs
dnl on what librt is and how it fits into various Unix systems.
dnl My best guess is that it is where glibc stashes its implementation
dnl of the POSIX Real-Time Extensions.  This seems necessary on Linux,
dnl and we haven't yet found a system where this is a problem.
AC_CHECK_LIB(rt, clock_gettime)
AC_CHECK_LIB(socket, setsockopt)

AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t], [], [echo "ptrdiff_t not found or g++ not installed - cannot continue" && exit 1])

AC_STRUCT_TM

dnl NOTE(dreiss): AI_ADDRCONFIG is not defined on OpenBSD.
AC_CHECK_DECL([AI_ADDRCONFIG], [],
              [AC_DEFINE([AI_ADDRCONFIG], 0,
                         [Define if the AI_ADDRCONFIG symbol is unavailable])],
              [
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
])

AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_STRERROR_R
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([strtoul])
AC_CHECK_FUNCS([bzero])
AC_CHECK_FUNCS([ftruncate])
AC_CHECK_FUNCS([gethostbyname])
AC_CHECK_FUNCS([gethostbyname_r])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([mkdir])
AC_CHECK_FUNCS([realpath])
AC_CHECK_FUNCS([select])
AC_CHECK_FUNCS([setlocale])
AC_CHECK_FUNCS([socket])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([sqrt])
dnl The following functions are optional.
AC_CHECK_FUNCS([alarm])
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_FUNCS([sched_get_priority_min])
AC_CHECK_FUNCS([sched_get_priority_max])
AC_CHECK_FUNCS([inet_ntoa])
AC_CHECK_FUNCS([pow])

if test "$cross_compiling" = "no" ; then
  AX_SIGNED_RIGHT_SHIFT
fi

dnl autoscan thinks we need this macro because we have a member function
dnl called "error".  Invoke the macro but don't run the check so autoscan
dnl thinks we are in the clear.  It's highly unlikely that we will ever
dnl actually use the function that this checks for.
if false ; then
  AC_FUNC_ERROR_AT_LINE
fi

# --- Coverage hooks ---

AC_ARG_ENABLE(coverage,
              [  --enable-coverage      turn on -fprofile-arcs -ftest-coverage],
              [case "${enableval}" in
                yes) ENABLE_COVERAGE=1 ;;
                no) ENABLE_COVERAGE=0 ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-cov) ;;
              esac],
              [ENABLE_COVERAGE=2])

if test "x[$]ENABLE_COVERAGE" = "x1"; then
  AC_MSG_WARN(enable coverage)
  GCOV_CFLAGS="`echo \"[$]CFLAGS\" | perl -pe 's/-O\d+//g;'` -fprofile-arcs -ftest-coverage"
  GCOV_CXXFLAGS="`echo \"[$]CXXFLAGS\" | perl -pe 's/-O\d+//g;'` -fprofile-arcs -ftest-coverage"
  GCOV_LDFLAGS="-XCClinker -fprofile-arcs -XCClinker -ftest-coverage"
fi

AC_SUBST(ENABLE_COVERAGE)
AC_SUBST(GCOV_CFLAGS)
AC_SUBST(GCOV_CXXFLAGS)
AC_SUBST(GCOV_LDFLAGS)

AC_CONFIG_HEADERS(config.h:config.hin)
AC_CONFIG_HEADERS(lib/cpp/src/thrift/config.h:config.hin)
AC_CONFIG_HEADERS(lib/c_glib/src/thrift/config.h:config.hin)
# guard against pre defined config.h
AH_TOP([
#ifndef CONFIG_H
#define CONFIG_H
])
AH_BOTTOM([
#endif
])


AC_CONFIG_FILES([
  Makefile
  compiler/cpp/Makefile
  compiler/cpp/src/Makefile
  compiler/cpp/test/Makefile
  lib/Makefile
  lib/cl/Makefile
  lib/cpp/Makefile
  lib/cpp/test/Makefile
  lib/cpp/thrift-nb.pc
  lib/cpp/thrift-z.pc
  lib/cpp/thrift-qt5.pc
  lib/cpp/thrift.pc
  lib/c_glib/Makefile
  lib/c_glib/thrift_c_glib.pc
  lib/c_glib/test/Makefile
  lib/d/Makefile
  lib/d/test/Makefile
  lib/erl/Makefile
  lib/go/Makefile
  lib/go/test/Makefile
  lib/go/test/fuzz/Makefile
  lib/haxe/test/Makefile
  lib/java/Makefile
  lib/js/Makefile
  lib/js/test/Makefile
  lib/json/Makefile
  lib/json/test/Makefile
  lib/kotlin/Makefile
  lib/netstd/Makefile
  lib/nodejs/Makefile
  lib/nodets/Makefile
  lib/perl/Makefile
  lib/perl/t/Makefile
  lib/php/Makefile
  lib/php/test/Makefile
  lib/dart/Makefile
  lib/py/Makefile
  lib/rb/Makefile
  lib/rs/Makefile
  lib/rs/test/Makefile
  lib/rs/test_recursive/Makefile
  lib/rs/test_recursive/src/Makefile
  lib/rs/test_recursive/src/maintenance/Makefile
  lib/rs/test_recursive/src/transit/Makefile
  lib/rs/test_recursive/src/transit/light/Makefile
  lib/rs/test_recursive/src/transit/services/Makefile
  lib/lua/Makefile
  lib/swift/Makefile
  lib/ts/Makefile
  lib/xml/Makefile
  lib/xml/test/Makefile
  test/Makefile
  test/features/Makefile
  test/c_glib/Makefile
  test/cl/Makefile
  test/cpp/Makefile
  test/erl/Makefile
  test/go/Makefile
  test/haxe/Makefile
  test/lua/Makefile
  test/netstd/Makefile
  test/php/Makefile
  test/dart/Makefile
  test/perl/Makefile
  test/py/Makefile
  test/py.twisted/Makefile
  test/py.tornado/Makefile
  test/rb/Makefile
  test/rs/Makefile
  test/swift/Makefile
  test/swift/CrossTests/Makefile
  tutorial/Makefile
  tutorial/c_glib/Makefile
  tutorial/cl/Makefile
  tutorial/cpp/Makefile
  tutorial/d/Makefile
  tutorial/go/Makefile
  tutorial/haxe/Makefile
  tutorial/java/Makefile
  tutorial/js/Makefile
  tutorial/netstd/Makefile
  tutorial/nodejs/Makefile
  tutorial/dart/Makefile
  tutorial/perl/Makefile
  tutorial/php/Makefile
  tutorial/py/Makefile
  tutorial/py.twisted/Makefile
  tutorial/py.tornado/Makefile
  tutorial/rb/Makefile
  tutorial/rs/Makefile
  tutorial/swift/Makefile
])

if test "$have_cpp" = "yes" ; then MAYBE_CPP="cpp" ; else MAYBE_CPP="" ; fi
AC_SUBST([MAYBE_CPP])
if test "$have_c_glib" = "yes" ; then MAYBE_C_GLIB="c_glib" ; else MAYBE_C_GLIB="" ; fi
AC_SUBST([MAYBE_C_GLIB])
if test "$have_d" = "yes" -a "$have_deimos_event2" = "yes" -a "$have_deimos_openssl" = "yes"; then MAYBE_D="d" ; else MAYBE_D="" ; fi
AC_SUBST([MAYBE_D])
if test "$have_java" = "yes" ; then MAYBE_JAVA="java" ; else MAYBE_JAVA="" ; fi
AC_SUBST([MAYBE_JAVA])
if test "$have_kotlin" = "yes" ; then MAYBE_KOTLIN="kotlin" ; else MAYBE_KOTLIN="" ; fi
AC_SUBST([MAYBE_KOTLIN])
if test "$have_python" = "yes" ; then MAYBE_PYTHON="py" ; else MAYBE_PYTHON="" ; fi
AC_SUBST([MAYBE_PYTHON])
if test "$have_py3" = "yes" ; then MAYBE_PY3="py3" ; else MAYBE_PY3="" ; fi
AC_SUBST([MAYBE_PY3])
if test "$have_ruby" = "yes" ; then MAYBE_RUBY="rb" ; else MAYBE_RUBY="" ; fi
AC_SUBST([MAYBE_RUBY])
if test "$have_perl" = "yes" ; then MAYBE_PERL="perl" ; else MAYBE_PERL="" ; fi
AC_SUBST([MAYBE_PERL])
if test "$have_php" = "yes" ; then MAYBE_PHP="php" ; else MAYBE_PHP="" ; fi
AC_SUBST([MAYBE_PHP])
if test "$have_dart" = "yes" ; then MAYBE_DART="dart" ; else MAYBE_DART="" ; fi
AC_SUBST([MAYBE_DART])
if test "$have_go" = "yes" ; then MAYBE_GO="go" ; else MAYBE_GO="" ; fi
AC_SUBST([MAYBE_GO])
if test "$have_nodejs" = "yes" ; then MAYBE_NODEJS="nodejs" ; else MAYBE_NODEJS="" ; fi
AC_SUBST([MAYBE_NODEJS])
if test "$have_nodets" = "yes" ; then MAYBE_NODETS="nodets" ; else MAYBE_NODETS="" ; fi
AC_SUBST([MAYBE_NODETS])
if test "$have_erlang" = "yes" ; then MAYBE_ERLANG="erl" ; else MAYBE_ERLANG="" ; fi
AC_SUBST([MAYBE_ERLANG])
if test "$have_lua" = "yes" ; then MAYBE_LUA="lua" ; else MAYBE_LUA="" ; fi
AC_SUBST([MAYBE_LUA])
if test "$have_rs" = "yes" ; then MAYBE_RS="rs" ; else MAYBE_RS="" ; fi
AC_SUBST([MAYBE_RS])
if test "$have_swift" = "yes" ; then MAYBE_SWIFT="swift" ; else MAYBE_SWIFT="" ; fi
AC_SUBST([MAYBE_SWIFT])
if test "$have_netstd" = "yes" ; then MAYBE_NETSTD="netstd" ; else MAYBE_NETSTD="" ; fi
AC_SUBST([MAYBE_NETSTD])
if test "$have_cl" = "yes" ; then MAYBE_CL="cl" ; else MAYBE_CL="" ; fi
AC_SUBST([MAYBE_CL])

AC_OUTPUT


echo
echo "$PACKAGE $VERSION"
echo
echo "Building C (GLib) Library .... : $have_c_glib"
echo "Building C++ Library ......... : $have_cpp"
echo "Building Common Lisp Library.. : $have_cl"
echo "Building D Library ........... : $have_d"
echo "Building Dart Library ........ : $have_dart"
echo "Building .NET Standard Library : $have_netstd"
echo "Building Erlang Library ...... : $have_erlang"
echo "Building Go Library .......... : $have_go"
echo "Building Haxe Library ........ : $have_haxe"
echo "Building Java Library ........ : $have_java"
echo "Building Kotlin Library ...... : $have_kotlin"
echo "Building Lua Library ......... : $have_lua"
echo "Building NodeJS Library ...... : $have_nodejs"
echo "Building Perl Library ........ : $have_perl"
echo "Building PHP Library ......... : $have_php"
echo "Building Python Library ...... : $have_python"
echo "Building Py3 Library ......... : $have_py3"
echo "Building Ruby Library ........ : $have_ruby"
echo "Building Rust Library ........ : $have_rs"
echo "Building Swift Library ....... : $have_swift"

if test "$have_c_glib" = "yes" ; then
  echo
  echo "C (glib):"
  echo "   Using glib version ........ : $($GSETTINGS --version)"
fi
if test "$have_cpp" = "yes" ; then
  echo
  echo "C++ Library:"
  echo "   C++ compiler .............. : $CXX"
  echo "   Build TZlibTransport ...... : $have_zlib"
  echo "   Build TNonblockingServer .. : $have_libevent"
  echo "   Build TQTcpServer (Qt5) ... : $have_qt5"
  echo "   C++ compiler version ...... : $($CXX --version | head -1)"
fi
if test "$have_cl" = "yes" ; then
  echo
  echo "Common Lisp Library:"
  echo "   Using Common Lisp ......... : $SBCL"
  echo "   Using Common Lisp version . : $($SBCL --version)"
fi
if test "$have_d" = "yes" ; then
  echo
  echo "D Library:"
  echo "   Using D Compiler .......... : $DMD"
  echo "   Building D libevent tests . : $with_d_event_tests"
  echo "   Building D SSL tests ...... : $with_d_ssl_tests"
  echo "   Using D version ........... : $($DMD --version | head -1)"
fi
if test "$have_dart" = "yes" ; then
  echo
  echo "Dart Library:"
  echo "   Using Dart ................ : $DART"
  echo "   Using Pub ................. : $DARTPUB"
  echo "   Using Dart version ........ : $($DART --version 2>&1)"
fi
if test "$have_netstd" = "yes" ; then
  echo
  echo ".NET Standard Library:"
  echo "   Using dotnet .............. : $DOTNETCORE"
  echo "   Using dotnet version ...... : $DOTNETCORE_VERSION"
fi
if test "$have_erlang" = "yes" ; then
  echo
  echo "Erlang Library:"
  echo "   Using erlc ................ : $ERLC"
  echo "   Using rebar ............... : $REBAR"
  echo "   Using erlc version ........ : $($ERL -eval 'erlang:display(erlang:system_info(otp_release)), halt().' -noshell | tr -d '\"')"
fi
if test "$have_go" = "yes" ; then
  echo
  echo "Go Library:"
  echo "   Using Go................... : $GO"
  echo "   Using Go version........... : $($GO version)"
fi
if test "$have_haxe" = "yes" ; then
  echo
  echo "Haxe Library:"
  echo "   Using Haxe ................ : $HAXE"
  echo "   Using Haxe version ........ : $HAXE_VERSION"
fi
if test "$have_java" = "yes" ; then
  echo
  echo "Java Library:"
  echo "   Using gradle .............. : $GRADLE"
  echo "   Using java ................ : $JAVA"
  echo "   Using javac ............... : $JAVAC"
  echo "   Using Gradle version ...... : $($GRADLE --version --quiet | grep Gradle 2>&1)"
  echo "   Using java version ........ : $($JAVA -version 2>&1 | grep 'version ')"
fi
if test "$have_kotlin" = "yes" ; then
  echo
  echo "Kotlin (Test Only) Library:"
  echo "   Using gradle .............. : $GRADLE"
  echo "   Using java ................ : $JAVA"
  echo "   Using javac ............... : $JAVAC"
  echo "   Using Gradle version ...... : $($GRADLE --version --quiet | grep Gradle 2>&1)"
  echo "   Using java version ........ : $($JAVA -version 2>&1 | grep 'version ')"
fi
if test "$have_lua" = "yes" ; then
  echo
  echo "Lua Library:"
  echo "   Using Lua ................. : $LUA"
  echo "   Using Lua version.......... : $($LUA -v)"
fi
if test "$have_nodejs" = "yes" ; then
  echo
  echo "NodeJS Library:"
  echo "   Using NodeJS .............. : $NODEJS"
  echo "   Using NodeJS version....... : $($NODEJS --version)"
fi
if test "$have_perl" = "yes" ; then
  echo
  echo "Perl Library:"
  echo "   Using Perl ................ : $PERL"
  echo "   Using Perl version ........ : $($PERL -v | grep 'version ')"
fi
if test "$have_php" = "yes" ; then
  echo
  echo "PHP Library:"
  echo "   Using php-config .......... : $PHP_CONFIG"
  echo "   Using php version ......... : $($PHP --version | head -1)"
fi
if test "$have_python" = "yes" ; then
  echo
  echo "Python Library:"
  echo "   Using Python .............. : $PYTHON"
  echo "   Using Python version ...... : $($PYTHON --version 2>&1)"
  if test "$have_py3" = "yes" ; then
  echo "   Using Python3 ............. : $PYTHON3"
  echo "   Using Python3 version ..... : $($PYTHON3 --version)"
  fi
  if test "$have_trial" = "yes"; then
  echo "   Using trial ............... : $TRIAL"
  fi
fi
if test "$have_ruby" = "yes" ; then
  echo
  echo "Ruby Library:"
  echo "   Using Ruby ................ : $RUBY"
  echo "   Using Ruby version ........ : $($RUBY --version)"
fi
if test "$have_rs" = "yes" ; then
  echo
  echo "Rust Library:"
  echo "   Using Cargo................ : $CARGO"
  echo "   Using rustc................ : $RUSTC"
  echo "   Using Rust version......... : $($RUSTC --version)"
fi
if test "$have_swift" = "yes" ; then
  echo
  echo "Swift Library:"
  echo "   Using Swift ............... : $SWIFT"
  echo "   Using Swift version ....... : $($SWIFT --version | head -1)"
fi
echo
echo "If something is missing that you think should be present,"
echo "please skim the output of configure to find the missing"
echo "component.  Details are present in config.log."
echo
