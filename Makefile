# Speeds up make by disabling implicit rules
MAKEFLAGS += --no-builtin-rules
.SUFFIXES :

.SECONDEXPANSION :

CXXFLAGS := -DGLM_FORCE_RADIANS

helper_lib_src := $(wildcard openGLHelper/*.cpp)
helper_lib_headers := $(wildcard openGLHelper/*.h)
helper_lib_objs := $(notdir $(helper_lib_src:.cpp=.o))

img_lib_src := $(wildcard vendor/imageIO/*.cpp)
img_lib_headers := $(wildcard vendor/imageIO/*.h)
img_lib_objs := $(notdir $(img_lib_src:.cpp=.o))

headers := $(helper_lib_headers) $(img_lib_headers)
objs := rcoaster.o $(helper_lib_objs) $(img_lib_objs)

OPT := -O3

uname_s := $(shell uname -s)
ifeq ($(uname_s),Linux)
  	INCLUDE := -Ivendor/glm/ -IopenGLHelper -Ivendor/imageIO
  	LIB := -lGLEW -lGL -lglut -ljpeg
  	LDFLAGS :=
else
	INCLUDE := -Ivendor/glm/ -IopenGLHelper -Ivendor/imageIO -Ivendor/jpeg-9a-mac/include
  	LIB := -framework OpenGL -framework GLUT vendor/jpeg-9a-mac/lib/libjpeg.a
  	CXXFLAGS += -Wno-deprecated-declarations
  	LDFLAGS := -Wl,-w
endif

rcoaster : $(objs)
	$(CXX) $(LDFLAGS) $^ $(OPT) $(LIB) -o $@

rcoaster.o : rcoaster.cpp $(headers)
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

$(helper_lib_objs) : %.o : openGLHelper/%.cpp $(helper_lib_headers)
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

$(img_lib_objs) : %.o : vendor/imageIO/%.cpp $(img_lib_headers)
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

clean:
	rm -rf *.o rcoaster