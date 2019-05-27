## Makefile.am -- Process this file with automake to produce Makefile.in
AM_CPPFLAGS = 	-I ./						\
	 	-I ./../ 					\
		-I ./../VoomMath				\
		-I./../Quadrature				\
		-I./../Shape					\
		-I/usr/include/eigen3\
		-I./../Geometry -I./../HalfEdgeMesh
lib_LIBRARIES        = libElement.a
libElement_a_SOURCES = FEgeomElement.cc FEgeomElement1D.cc LMEgeomElement.cc \
		       RKPMgeomElement.cc LoopShellElement.cc
