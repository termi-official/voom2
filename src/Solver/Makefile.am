## Makefile.am -- Process this file with automake to produce Makefile.in
INCLUDES     = 	-I./ 							\
		-I./../							\
	 	-I./../VoomMath	 					\
		-I./../Quadrature					\
		-I./../Shape						\
		-I./../Material						\
		-I./../Material/MechanicsMaterial			\
		-I./../Element						\
		-I./../Mesh 						\
		-I./../Geometry 					\
		-I./../HalfEdgeMesh					\
		-I./../Body						\
		-I./../Model						\
		-I./../Geometry					\
		-I./Results						\
		-I/usr/include/eigen3	\
		-I/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/include/vtk-8.1

AM_LDFLAGS  =	-L/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/lib
LDADD 	    =	-lvtkIO -lvtkGraphics -lvtkGenericFiltering -lvtkFiltering -lvtkCommon -lvtksys -ldl -lpthread -lvtkzlib -lvtkDICOMParser -lvtkNetCDF -lvtkmetaio -lvtkNetCDF_cxx -lvtksqlite -lvtkpng -lvtkjpeg -lvtktiff -lvtkexpat -lvtkverdict

lib_LIBRARIES = libSolver.a
libSolver_a_SOURCES = EigenNRsolver.cc LBFGSB.cc lbfgsb-routines.f blas.f linpack.f timer.f
