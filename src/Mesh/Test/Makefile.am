bin_PROGRAMS 	= TestMesh
INCLUDES = -I./../					\
	   -I./../../					\
	   -I./../../VoomMath/ 				\
	   -I./../../Element -I./../../Quadrature	\
	   -I./../../Shape				\
	   -I./../../HalfEdgeMesh			\
           -I/usr/include/eigen3\
	  -I./../../Geometry 
AM_LDFLAGS = -L./../ -L./../../VoomMath -L./../../Shape	\
	     -L./../../Quadrature -L ./../../Element -L./../../HalfEdgeMesh -L./../../Geometry
LDADD      = -lMesh -lElement -lShape -lQuadrature -lVoomMath -lHEMesh -lGeometry
TestMesh_SOURCES = TestMesh.cc
