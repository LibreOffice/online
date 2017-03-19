#!/bin/sh

TEMPDIR=`mktemp -d`
SRCDIR=`pwd`
cd $TEMPDIR
tar xzf $SRCDIR/loolwsd-master.tar.gz
cp -a $SRCDIR/debian loolwsd-master
mkdir -p loolwsd-master/loleaflet
cd loolwsd-master/loleaflet
tar xzf $SRCDIR/loleaflet/loleaflet-master.tar.gz --strip-components=1
cd $TEMPDIR
tar czf $SRCDIR/loolwsd_master.orig.tar.gz loolwsd-master
cd $SRCDIR
rm -rf $TEMPDIR
