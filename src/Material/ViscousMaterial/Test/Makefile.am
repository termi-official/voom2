bin_PROGRAMS 	= TestViscousMaterial
INCLUDES =		-I./../					\
			-I./../../				\
			-I./../../../				\
	 		-I./../../../VoomMath/ 			\
			-I/usr/include/eigen3
AM_LDFLAGS 	= 	-L./../	                                \
			-L./../../				\
	         	-L./../../../VoomMath/ 			\
			-L./../ViscousMaterial
LDADD   = -lVoomMath -lViscousMaterial  
TestViscousMaterial_SOURCES = TestViscousMaterial.cc
