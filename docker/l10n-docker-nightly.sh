#! /bin/bash
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# -- Available env vars --
# * DOCKER_HUB_REPO - which Docker Hub repo to use
# * DOCKER_HUB_TAG  - which Docker Hub tag to create
# * LIBREOFFICE_BRANCH  - which branch to build in core
# * LIBREOFFICE_ONLINE_BRANCH - which branch to build in online
# * LIBREOFFICE_BUILD_TARGET - which make target to run (in core repo)
# * ONLINE_EXTRA_BUILD_OPTIONS - extra build options for online
# * NO_DOCKER_IMAGE - if set, don't build the docker image itself, just do all the preps

# check we can sudo without asking a pwd
echo "Trying if sudo works without a password"
echo
echo "If you get a password prompt now, break, and fix your setup using 'sudo visudo'; add something like:"
echo "yourusername ALL=(ALL) NOPASSWD: /sbin/setcap"
echo
sudo echo "works"

# Check env variables
if [ -z "$DOCKER_HUB_REPO" ]; then
  DOCKER_HUB_REPO="libreoffice/online"
fi;
if [ -z "$DOCKER_HUB_TAG" ]; then
  DOCKER_HUB_TAG="master"
fi;
echo "Using Docker Hub Repository: '$DOCKER_HUB_REPO' with tag '$DOCKER_HUB_TAG'."

if [ -z "$LIBREOFFICE_BRANCH" ]; then
  LIBREOFFICE_BRANCH="master"
fi;
echo "Building core branch '$LIBREOFFICE_BRANCH'"

if [ -z "$LIBREOFFICE_ONLINE_BRANCH" ]; then
  LIBREOFFICE_ONLINE_BRANCH="master"
fi;
echo "Building online branch '$LIBREOFFICE_ONLINE_BRANCH'"

if [ -z "$LIBREOFFICE_BUILD_TARGET" ]; then
  LIBREOFFICE_BUILD_TARGET=""
fi;
echo "LibreOffice build target: '$LIBREOFFICE_BUILD_TARGET'"

# do everything in the builddir
SRCDIR=$(realpath `dirname $0`)
INSTDIR="$SRCDIR/instdir"
BUILDDIR="$SRCDIR/builddir"

mkdir -p "$BUILDDIR"
cd "$BUILDDIR"

rm -rf "$INSTDIR" || true
mkdir -p "$INSTDIR"

##### cloning & updating #####

# libreoffice repo
if test ! -d libreoffice ; then
    git clone https://git.libreoffice.org/core libreoffice || exit 1
fi

( cd libreoffice && git fetch --all && git checkout $LIBREOFFICE_BRANCH && ./g pull -r ) || exit 1

# online repo
if test ! -d online ; then
    git clone https://git.libreoffice.org/online online || exit 1
fi

( cd online && git fetch --all && git checkout -f $LIBREOFFICE_ONLINE_BRANCH && git clean -f -d && git pull -r ) || exit 1

##### LibreOffice #####

# build LibreOffice
cat > libreoffice/autogen.input << EOF
--disable-cups
--disable-dbus
--disable-dconf
--disable-epm
--disable-evolution2
--disable-ext-nlpsolver
--disable-ext-wiki-publisher
--disable-firebird-sdbc
--disable-gio
--disable-gstreamer-0-10
--disable-gstreamer-1-0
--disable-gtk
--disable-gtk3
--disable-qt5
--disable-kde5
--disable-odk
--disable-online-update
--enable-pdfimport
--disable-postgresql-sdbc
--disable-report-builder
--disable-scripting-beanshell
--disable-scripting-javascript
--disable-sdremote
--disable-sdremote-bluetooth
--enable-extension-integration
--enable-mergelibs
--enable-python=internal
--enable-release-build
--with-external-dict-dir=/usr/share/hunspell
--with-external-hyph-dir=/usr/share/hyphen
--with-external-thes-dir=/usr/share/mythes
--with-fonts
--with-galleries=no
--with-lang=ALL
--with-linker-hash-style=both
--with-system-dicts
--with-system-zlib
--with-theme=colibre
--without-branding
--without-help
--without-java
--without-junit
--with-myspell-dicts
--without-package-format
--without-system-cairo
--without-system-jars
--without-system-jpeg
--without-system-libpng
--without-system-libxml
--without-system-openssl
--disable-poppler
--without-system-postgresql
EOF

( cd libreoffice && ./autogen.sh ) || exit 1
( cd libreoffice && make $LIBREOFFICE_BUILD_TARGET ) || exit 1

# copy stuff
mkdir -p "$INSTDIR"/opt/
cp -a libreoffice/instdir "$INSTDIR"/opt/libreoffice

# FIXME fix RPATH of libcairo
chrpath -r '$ORIGIN' "$INSTDIR"/opt/libreoffice/program/libcairo.so.2

##### loolwsd & loleaflet #####

# build
( cd online && ./autogen.sh ) || exit 1
( cd online && ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --enable-silent-rules --with-lokit-path="$BUILDDIR"/libreoffice/include --with-lo-path="$INSTDIR"/opt/libreoffice $ONLINE_EXTRA_BUILD_OPTIONS) || exit 1
( cd online/loleaflet/po && ../../scripts/downloadpootle.sh )
( cd online/loleaflet && make l10n) || exit 1
( cd online && scripts/locorestrings.py "$BUILDDIR"/online "$BUILDDIR"/libreoffice/translations )
( cd online && scripts/unocommands.py --update "$BUILDDIR"/online "$BUILDDIR"/libreoffice )
( cd online && scripts/unocommands.py --translate "$BUILDDIR"/online "$BUILDDIR"/libreoffice/translations )
( cd online && make -j 8) || exit 1

# copy stuff
( cd online && DESTDIR="$INSTDIR" make install ) || exit 1

# Create new docker image
if [ -z "$NO_DOCKER_IMAGE" ]; then
  cd "$SRCDIR"
  docker build --no-cache -t $DOCKER_HUB_REPO:$DOCKER_HUB_TAG . || exit 1
  docker push $DOCKER_HUB_REPO:$DOCKER_HUB_TAG || exit 1
else
  echo "Skipping docker image build"
fi;
