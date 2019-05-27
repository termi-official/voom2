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
		-I/usr/include/eigen3	\
		-I./../Geometry						\
		-I/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/boost-1.59.0-6mvivlutt6d2ngomszwekq3ihz3veibx/include \
                -I/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/include/vtk-8.1

lib_LIBRARIES = libSolver.a
libSolver_a_SOURCES = EigenNRsolver.cc LBFGSB.cc lbfgsb-routines.f blas.f linpack.f timer.f
