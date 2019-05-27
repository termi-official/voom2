## Makefile.am -- Process this file with automake to produce Makefile.in

bin_PROGRAMS = TestVTKOutput

INCLUDES     = 	-I./ 							\
		-I./../							\
		-I./../../						\
	 	-I./../../VoomMath	 				\
		-I./../../Quadrature					\
		-I./../../Shape						\
		-I./../../Material					\
		-I./../../Element					\
		-I./../../Mesh 						\
		-I/usr/include/eigen3	\
		-I./../../Geometry -I./../HalfEdgeMesh			\
		-I./../../Model						\
		-I/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/include/vtk-8.1		\
		-I./../Output

AM_LDFLAGS   =  -L/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/lib			\
		-L./                                                            \
                -L./../                                                         \
		-L./../../							\
                -L./../../Model                                                 \
                -L./../../Mesh                                                  \
                -L./../../Element                                               \
                -L./../../Material                                              \
                -L./../../Quadrature                                            \
                -L./../../Shape                                                 \
                -L./../../VoomMath                                              \
                -L./../../Geometry                                              \
                -L./../../HalfEdgeMesh						\
		-L./../Output

LDADD	     = -lvtkCommonDataModel-8.1 -lvtkIOLegacy-8.1 -lvtkCommonCore-8.1 -lvtksys-8.1 -lOutput

TestVTKOutput_SOURCES = TestVTKOutput.cc
