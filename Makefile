# Speeds up make by disabling implicit rules
MAKEFLAGS += --no-builtin-rules
.SUFFIXES :

.SECONDEXPANSION :

# Utility variables
empty :=
space := $(empty) $(empty)
space2 := $(space)$(space)

CXXFLAGS := -DGLM_FORCE_RADIANS
 
CXXV := $(shell $(CXX) --version | head -n 1)

helper_lib_src := $(wildcard openGLHelper/*.cpp)
helper_lib_headers := $(wildcard openGLHelper/*.h)
helper_lib_objs := $(notdir $(helper_lib_src:.cpp=.o))

img_lib_src := $(wildcard vendor/imageIO/*.cpp)
img_lib_headers := $(wildcard vendor/imageIO/*.h)
img_lib_objs := $(notdir $(img_lib_src:.cpp=.o))

headers := $(helper_lib_headers) $(img_lib_headers)
objs := main.o $(helper_lib_objs) $(img_lib_objs)

OPT := -O3 -std=c++17

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

ifndef RCOASTER_DEBUG
	CXXFLAGS += -DNDEBUG
endif 

$(info rcoaster build info: )
$(info $(space2)CXXFLAGS: $(CXXFLAGS))
$(info $(space2)LDFLAGS: $(CXXFLAGS))
$(info $(space2)CXX: $(CXXV))

all : main aabb.o

main : $(objs) spline.o
	$(CXX) $(LDFLAGS) $^ $(OPT) $(LIB) -o $@

main.o : main.cpp $(headers) types.hpp spline.hpp
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

aabb.o : aabb.cpp aabb.hpp
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

spline.o : spline.cpp spline.hpp types.hpp
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

$(helper_lib_objs) : %.o : openGLHelper/%.cpp $(helper_lib_headers)
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

$(img_lib_objs) : %.o : vendor/imageIO/%.cpp $(img_lib_headers)
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

clean:
	rm -rf *.o main