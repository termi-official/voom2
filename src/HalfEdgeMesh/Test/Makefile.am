bin_PROGRAMS = test

INCLUDES = -I./	-I./../	-I./../../			\
	   -I./../../VoomMath -I./../../Quadrature	\
	   -I/usr/include/eigen3
test_LDFLAGS = -L./../ -L./../../VoomMath/ -L./../../Quadrature
test_LDADD   = -lHEMesh -lVoomMath -lQuadrature
test_SOURCES = test.cc
