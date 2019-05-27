bin_PROGRAMS 	= TestMaterial
INCLUDES =		-I./../					\
			-I./../../				\
			-I./../MechanicsMaterial		\
			-I./../ViscousMaterial			\
	 		-I./../../VoomMath/ 			\
			-I./../../Potential/			\
			-I./../../Geometry/			\
			-I/usr/include/eigen3
AM_LDFLAGS 	= 	-L./../	                                \
			-L./../../Potential/			\
	         	-L./../../VoomMath/ 			\
			-L./../MechanicsMaterial		\
			-L./../ViscousMaterial			\
			-L./../../Geometry
LDADD   = -lMaterials -lMechanicsMaterial -lViscousMaterial -lPotentials -lVoomMath -lGeometry  
TestMaterial_SOURCES = TestMaterial.cc
