## Makefile.am -- Process this file with automake to produce Makefile.in
INCLUDES     = 	-I./ 							\
		-I./../							\
	 	-I./../VoomMath/					\
		-I./../Shape						\
		-I./../Element						\
		-I./../Quadrature					\
		-I./../Mesh						\
		-I./../Material						\
		-I./../Material/MechanicsMaterial			\
		-I./../Model						\
		-I/u/local/apps/eigen/3.2.4/gcc-4.4.7/include/eigen3	\
		-I./../Geometry						\
                -I/u/local/apps/vtk/5.8.0/include/vtk-5.8

lib_LIBRARIES = libSolver.a
libSolver_a_SOURCES = EigenNRsolver.cc LBFGSB.cc lbfgsb-routines.f blas.f linpack.f timer.f
