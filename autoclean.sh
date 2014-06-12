rm -rf aclocal.m4 autom4te.cache stamp-h1 libtool configure config.* Makefile.in Makefile
rm -rf config/*
rm -rf src/Makefile.in
rm -rf src/Makefile
for a in ./src/libs/*; do
	rm -rf $a/Makefile 2>&1
	rm -rf $a/Makefile.in 2>&1
done
