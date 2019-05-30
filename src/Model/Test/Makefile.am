bin_PROGRAMS = TestModel
INCLUDES = -I./							\
	   -I./../ 						\
           -I./../../						\
	   -I./../../VoomMath					\
	   -I./../../Solver/Results				\
	   -I./../../Quadrature					\
	   -I./../../Shape 					\
	   -I./../../Material 					\
	   -I./../../Material/MechanicsMaterial			\
	   -I./../../Element					\
	   -I./../../Mesh					\
	   -I./../../Body					\
	   -I./../../Geometry					\
	   -I./../../HalfEdgeMesh				\
		-I/usr/include/eigen3		\
		-I/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/include/vtk-8.1
AM_LDFLAGS = 	-L./ 								\
	     	-L./../ 							\
		-L./../../VoomMath					 	\
		-L./../../Quadrature 						\
		-L./../../Shape 						\
		-L./../../Material 						\
		-L./../../Material/MechanicsMaterial				\
		-L./../../Element 						\
		-L./../../Mesh							\
		-L./../../Body							\
		-L./../../Geometry						\
		-L./../../HalfEdgeMesh						\
		-L/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/lib
LDADD   = 	-lModel	-lBody -lMesh -lElement -lQuadrature -lShape -lMaterials -lVoomMath  	\
		-lGeometry -lHEMesh -lMechanicsMaterial					\
	 	-lvtkIO -lvtkGraphics -lvtkGenericFiltering -lvtkFiltering -lvtkCommon 			\
		-lvtksys -ldl -lpthread -lvtkzlib -lvtkDICOMParser -lvtkNetCDF -lvtkmetaio 		\
		-lvtkNetCDF_cxx -lvtksqlite -lvtkpng -lvtkjpeg -lvtktiff -lvtkexpat -lvtkverdict
TestModel_SOURCES = TestModel.cc
