bin_PROGRAMS = TestEigen TestSingleCube TestDoubleCube TestContractingCube
INCLUDES = -I./							\
	   -I./../ 						\
	   -I./../../  						\
	   -I./../../Body 						\
	   -I./../../VoomMath					\
	   -I./../../Solver/Results				\
	   -I./../../Model					\
	   -I./../../Quadrature					\
	   -I./../../Shape 					\
	   -I./../../Material 					\
	   -I./../../Material/MechanicsMaterial			\
	   -I./../../Material/PairMaterial			\
	   -I./../../Element					\
	   -I./../../Mesh					\
           -I./../../Geometry 					\
	   -I./../../HalfEdgeMesh				\
	   -I/usr/include/eigen3\	\
	   -I/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/include/vtk-8.1	

AM_LDFLAGS = 	-L./ 								\
	     	-L./../ 							\
		-L./../../Model							\
	     	-L./../../Body					 		\
		-L./../../Mesh							\
		-L./../../Element 						\
		-L./../../Material 						\
		-L./../../Material/MechanicsMaterial				\
		-L./../../Material/PairMaterial					\
		-L./../../Quadrature 						\
		-L./../../Shape 						\
		-L./../../VoomMath 						\
		-L./../../Geometry						\
		-L./../../HalfEdgeMesh						\
		-L/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/lib

LDADD   = 	-lSolver -lModel -lBody -lMesh -lElement -lQuadrature -lShape -lMaterials -lVoomMath  	\
		-lGeometry -lHEMesh -lMechanicsMaterial	-lPairMaterial -lgfortran			\
	 	-lvtkIO -lvtkGraphics -lvtkGenericFiltering -lvtkFiltering -lvtkCommon 			\
		-lvtksys -ldl -lpthread -lvtkzlib -lvtkDICOMParser -lvtkNetCDF -lvtkmetaio 		\
		-lvtkNetCDF_cxx -lvtksqlite -lvtkpng -lvtkjpeg -lvtktiff -lvtkexpat -lvtkverdict	

TestEigen_SOURCES = TestEigen.cc
TestSingleCube_SOURCES = TestSingleCube.cc
TestDoubleCube_SOURCES = TestDoubleCube.cc
TestContractingCube_SOURCES = TestContractingCube.cc
# TestLB_SOURCES = TestLB.cc

