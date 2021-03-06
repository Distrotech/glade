#!/bin/bash

# Build revision, always starts from 1 for each version
REV=1

# Location of the GTK+ runtime
GTKBUNDLE="/d/bin/Python27/Lib/site-packages/gtk-2.0/runtime/"
# Or instead of using the gtk+-bundle included in the pygtk all-in-one
# installer you could use a gtk+-bundle from ftp.gnome.org.
# Both are identical.
# GTKBUNDLE="/d/dev/gnome.org/gtk+-bundle_2.22.0-20101016_win32"

# List Python interperer installation directories for wich
# you'll build glade with Python support
INTERPRETERS="/d/bin/Python26 /d/bin/Python27"

OLD_CWD=`pwd`
OLD_PATH=${PATH}
OLD_PKG_CONFIG_PATH=${PKG_CONFIG_PATH}

SCRIPT=`dirname "$0"`
SRCDIR=`readlink -f "$SCRIPT/../../"`
BUILDDIR="${SRCDIR}/build/mswindows/build"
DISTDIR="${SRCDIR}/build/mswindows/dist"

MOD=glade3
MAJOR=`grep m4_define\\(glade_major_version ${SRCDIR}/configure.ac | gawk '{print gensub(/^.*?glade_major_version,[[:space:]]([[:digit:]]).*?$/,"\\\\1","g")}'`
MINOR=`grep m4_define\\(glade_minor_version ${SRCDIR}/configure.ac | gawk '{print gensub(/^.*?glade_minor_version,[[:space:]]([[:digit:]]).*?$/,"\\\\1","g")}'`
MICRO=`grep m4_define\\(glade_micro_version ${SRCDIR}/configure.ac | gawk '{print gensub(/^.*?glade_micro_version,[[:space:]]([[:digit:]]).*?$/,"\\\\1","g")}'`
VER="${MAJOR}.${MINOR}.${MICRO}"
ARCH=win32

#################################################################
# Build Glade without Python support                            #
#################################################################
export PATH=${GTKBUNDLE}/bin:${OLD_PATH}
export PKG_CONFIG_PATH=${GTKBUNDLE}/lib/pkgconfig/:${OLD_PKG_CONFIG_PATH}

THIS=${MOD}_${VER}-${REV}_${ARCH}
MFT=${THIS}.mft
BUILD=${THIS}.sh
RUNZIP=${THIS}.zip
DEVZIP=${MOD}-dev_${VER}-${REV}_${ARCH}.zip

PREFIX=${BUILDDIR}/${THIS}
LOG=${PREFIX}/src/glade3/${THIS}.log

if test -d "${PREFIX}"; then
    rm -rf "${PREFIX}"
fi

mkdir -p "${PREFIX}/manifest"
mkdir -p "${PREFIX}/src/glade3/"
mkdir -p "${DISTDIR}"

(
    set -x
    cd "${SRCDIR}"

    # Copied from tml@iki.fi's build scripts:
    # Don't do any relinking and don't use any wrappers in libtool. It
    # just causes trouble, IMHO.
    sed -e "s/need_relink=yes/need_relink=no # no way --tml/" \
        -e "s/wrappers_required=yes/wrappers_required=no # no thanks --tml/" \
        <ltmain.sh >ltmain.temp && mv ltmain.temp ltmain.sh

    lt_cv_deplibs_check_method="pass_all" \
    CC="gcc -mthreads" \
    CFLAGS=-O2 \
    ./configure \
    --enable-debug=yes \
    --disable-static \
    --disable-gnome \
    --disable-gtk-doc \
    --disable-python \
    --disable-static \
    --prefix="${PREFIX}" &&

    make -j3 install &&

    # No thanks
    rm -rf ${PREFIX}/lib/*.dll.a &&
    rm -rf ${PREFIX}/lib/*.la &&
    rm -rf ${PREFIX}/lib/glade3/modules/*.dll.a &&
    rm -rf ${PREFIX}/lib/glade3/modules/*.la &&

    touch "${PREFIX}/manifest/${MFT}" &&
    cp "${SRCDIR}/build/mswindows/build_glade.sh" "${PREFIX}/src/glade3/${BUILD}"
) 2>&1 | tee "${LOG}"

# Write manifest and create zip archives...
cd "${PREFIX}" &&
find -type f -print | sed "s/\.\///g" > "${PREFIX}/manifest/${MFT}" &&
zip -r "${DISTDIR}/${RUNZIP}" "bin" "lib/glade3" "lib/locale" "share/applications" "share/glade3" "share/icons" &&
zip -r "${DISTDIR}/${DEVZIP}" "include" "lib/pkgconfig" "manifest" "share/gtk-doc" "src"

#################################################################
# Build Glade with Python support                               #
#################################################################
for INTERPRETER in ${INTERPRETERS}; do
    export PATH=${INTERPRETER}:${INTERPRETER}/Scripts:${GTKBUNDLE}/bin:${OLD_PATH}
    export PKG_CONFIG_PATH=${INTERPRETER}/Lib/pkgconfig/:${GTKBUNDLE}/lib/pkgconfig/:${OLD_PKG_CONFIG_PATH}

    len=${#INTERPRETER}
    INTERPRETER_VERSION=$(echo "$INTERPRETER"|cut -c"$((len-1))"-"$len")
    THIS=${MOD}_${VER}-${REV}_${ARCH}-py${INTERPRETER_VERSION}
    MFT=${THIS}.mft
    BUILD=${THIS}.sh
    RUNZIP=${THIS}.zip
    DEVZIP=${MOD}-dev_${VER}-${REV}_${ARCH}-py${INTERPRETER_VERSION}.zip

    PREFIX=${BUILDDIR}/${THIS}
    LOG=${PREFIX}/src/glade3/${THIS}.log

    if test -d "${PREFIX}"; then
        rm -rf "${PREFIX}"
    fi

    mkdir -p "${DISTDIR}"
    mkdir -p "${PREFIX}/src/glade3/"
    mkdir -p "${PREFIX}/manifest"

    (
        set -x      
        cd "${SRCDIR}"

        # Copied from tml@iki.fi's build scripts:
        # Don't do any relinking and don't use any wrappers in libtool. It
        # just causes trouble, IMHO.
        sed -e "s/need_relink=yes/need_relink=no # no way --tml/" \
            -e "s/wrappers_required=yes/wrappers_required=no # no thanks --tml/" \
            <ltmain.sh >ltmain.temp && mv ltmain.temp ltmain.sh

        lt_cv_deplibs_check_method="pass_all" \
        CC="gcc -mthreads" \
        CFLAGS=-O2 \
        PYTHON_INCLUDES="-I${INTERPRETER}/include/" \
        PYTHON_LIBS="-L${INTERPRETER}/libs/ -lpython${INTERPRETER_VERSION}" \
        PYTHON_LIB_LOC="${INTERPRETER}/libs/" \
        ./configure \
        --enable-debug=yes \
        --disable-static \
        --disable-gnome \
        --disable-gtk-doc \
        --enable-python \
        --disable-static \
        --prefix="${PREFIX}" &&

        make -j3 install &&

        # No thanks
        rm -rf ${PREFIX}/lib/*.dll.a &&
        rm -rf ${PREFIX}/lib/*.la &&
        rm -rf ${PREFIX}/lib/glade3/modules/*.dll.a &&
        rm -rf ${PREFIX}/lib/glade3/modules/*.la &&

        touch "${PREFIX}/manifest/${MFT}" &&
        cp "${SRCDIR}/build/mswindows/build_glade.sh" "${PREFIX}/src/glade3/${BUILD}" &&

        # Write a .exe.manifest file...
        echo "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>
<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">
  <assemblyIdentity version=\"1.0.0.0\"
                    name=\"glade-3.exe\"
                    type=\"win32\" />
  <dependency>
    <dependentAssembly>
      <assemblyIdentity
        type=\"win32\"
        name=\"Microsoft.VC90.CRT\"
        version=\"9.0.21022.8\"
        processorArchitecture=\"*\"
        publicKeyToken=\"1fc8b3b9a1e18e3b\" />
    </dependentAssembly>
  </dependency>
</assembly>
" > "${PREFIX}/bin/glade-3.exe.manifest"

    ) 2>&1 | tee "${LOG}"

    # Write manifest and create zip archives...
    cd "${PREFIX}" &&
    find -type f -print | sed "s/\.\///g" > "${PREFIX}/manifest/${MFT}" &&
    zip -r "${DISTDIR}/${RUNZIP}" "bin" "lib/glade3" "lib/locale" "share/applications" "share/glade3" "share/icons" &&
    zip -r "${DISTDIR}/${DEVZIP}" "include" "lib/pkgconfig" "manifest" "share/gtk-doc" "src"

done
