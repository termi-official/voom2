bin_PROGRAMS 	= TestQuadrature
INCLUDES =	 	-I./ 					\
			-I./../					\
			-I./../../				\
	 		-I./../../VoomMath/ 			\
			-I/usr/include/eigen3
TestQuadrature_LDFLAGS = -L./../	                        \
	                 -L./../../VoomMath/ 
TestQuadrature_LDADD   = -lQuadrature -lVoomMath    
TestQuadrature_SOURCES = TestQuadrature.cc
