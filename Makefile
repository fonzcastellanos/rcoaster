# Speeds up make by disabling implicit rules
MAKEFLAGS += --no-builtin-rules
.SUFFIXES :

.SECONDEXPANSION :

# Utility variables
empty :=
space := $(empty) $(empty)
space2 := $(space)$(space)

CXXV := $(shell $(CXX) --version | head -n 1)

CXXFLAGS := -DGLM_FORCE_RADIANS
CXXFLAGS += -std=c++17 -O3
CXXFLAGS += -Wall -Wextra

ifndef RCOASTER_DEBUG
	CXXFLAGS += -DNDEBUG
endif 

helper_lib_src := $(wildcard openGLHelper/*.cpp)
helper_lib_headers := $(wildcard openGLHelper/*.h)
helper_lib_objs := $(notdir $(helper_lib_src:.cpp=.o))

uname_s := $(shell uname -s)
ifeq ($(uname_s),Linux)
	INCLUDE := -Ivendor/glm -IopenGLHelper -Ivendor
	LIBS := -lGLEW -lGL -lglut
else ifeq ($(uname_s),Darwin)
	INCLUDE := -Ivendor/glm -IopenGLHelper -Ivendor
	LIBS := -framework OpenGL -framework GLUT
	# OpenGL is deprecated in macOS, but it's still usable
	CXXFLAGS += -Wno-deprecated-declarations
else
	$(error Only Linux and Darwin are supported.)
endif


$(info rcoaster build info: )
$(info $(space2)CXXFLAGS: $(CXXFLAGS))
$(info $(space2)LDFLAGS: $(CXXFLAGS))
$(info $(space2)CXX: $(CXXV))

all : main

main : main.o $(helper_lib_objs) spline.o aabb.o
	$(CXX) $(CXXFLAGS) $(LIBS) $^ -o $@

main.o : main.cpp $(helper_lib_headers) types.hpp spline.hpp aabb.hpp vendor/stb_image.h vendor/stb_image_write.h
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) $< -o $@

aabb.o : aabb.cpp aabb.hpp types.hpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) $< -o $@

spline.o : spline.cpp spline.hpp types.hpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) $< -o $@

$(helper_lib_objs) : %.o : openGLHelper/%.cpp $(helper_lib_headers)
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) $< -o $@

clean:
	rm -rf *.o main