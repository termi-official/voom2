## Makefile.am -- Process this file with automake to produce Makefile.in
INCLUDES     = 	-I./ 					\
		-I./../					\
	 	-I./../VoomMath 			\
		-I./../Solver/Results			\
		-I./../Quadrature			\
		-I./../Shape				\
		-I./../Element				\
		-I/usr/include/eigen3\
		-I./../Geometry				\
		-I./../HalfEdgeMesh

lib_LIBRARIES=libMesh.a
libMesh_a_SOURCES = Mesh.cc FEMesh.cc # LoopShellMesh.cc
