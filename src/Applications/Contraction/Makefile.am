bin_PROGRAMS = TestStrip EllipsoidLV CheckingConsistency MaterialPoint EllipsoidLVSpring StripSpringBC MaterialPoint_PurelyUniaxial RabbitLV CylindricalStrip \
	       TestEllipsoid

INCLUDES =		-I./							\
			-I./../							\
			-I./../../						\
			-I./../../Mesh						\
			-I./../../Solver  					\
	 		-I./../../VoomMath					\
			-I./../../Shape						\
			-I./../../Quadrature					\
			-I./../../Model						\
			-I./../../Potential					\
			-I./../../Material 					\
			-I./../../Material/MechanicsMaterial			\
			-I./../../Material/ViscousMaterial			\
			-I./../../Element 					\
			-I/usr/include/eigen3	\
		    	-I/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/boost-1.59.0-6mvivlutt6d2ngomszwekq3ihz3veibx/include		\
			-I/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/include/vtk-8.1

AM_LDFLAGS = 		-L./							\
			-L./../							\
			-L./../../VoomMath					\
			-L./../../Model           				\
             		-L./../../Element					\
			-L./../../Shape						\
			-L./../../Quadrature     				\
	     		-L./../../VoomMath					\
			-L./../../Material					\
			-L./../../Mesh						\
			-L./../../Solver  					\
	     		-L./../../Potential					\
	     		-L./../../Material/MechanicsMaterial			\
			-L./../../Material/ViscousMaterial			\
			-L/home/termi/Repos/spack/opt/spack/linux-opensuse_leap15-x86_64/gcc-8.2.1/vtk-8.1.2-ahw6y34tf2ylqla7ozn47nvwvs2pfta3/lib

LDADD = 		-lSolver						\
			-lModel							\
			-lMesh							\
			-lElement 				             	\
			-lShape							\
			-lQuadrature						\
			-lVoomMath                				\
			-lPotentials						\
			-lMaterials						\
			-lMechanicsMaterial					\
			-lViscousMaterial					\
			-lgfortran						\
			-lvtkCommonDataModel-8.1 -lvtkIOXML-8.1 -lvtkIOLegacy-8.1 -lvtkCommonCore-8.1 -lvtksys-8.1

TestStrip_SOURCES = TestStrip.cc
TestEllipsoid_SOURCES = TestEllipsoid.cc
EllipsoidLV_SOURCES = EllipsoidLV.cc
CheckingConsistency_SOURCES = CheckingConsistency.cc
MaterialPoint_SOURCES = MaterialPoint.cc
EllipsoidLVSpring_SOURCES = EllipsoidLVSpring.cc
StripSpringBC_SOURCES = StripSpringBC.cc
MaterialPoint_PurelyUniaxial_SOURCES = MaterialPoint_PurelyUniaxial.cc
RabbitLV_SOURCES = RabbitLV.cc
CylindricalStrip_SOURCES = CylindricalStrip.cc
