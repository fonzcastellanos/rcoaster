# Speeds up make by disabling implicit rules
MAKEFLAGS += --no-builtin-rules
.SUFFIXES :

.SECONDEXPANSION :

CXXFLAGS := -DGLM_FORCE_RADIANS

LIB_CODE_BASE := openGLHelper
LIB_CODE_SRC := $(wildcard $(LIB_CODE_BASE)/*.cpp)
LIB_CODE_HEADERS := $(wildcard $(LIB_CODE_BASE)/*.h)
LIB_CODE_OBJS := $(notdir $(LIB_CODE_SRC:.cpp=.o))

IMAGE_LIB_SRC := $(wildcard vendor/imageIO/*.cpp)
IMAGE_LIB_HEADERS := $(wildcard vendor/imageIO/*.h)
IMAGE_LIB_OBJS := $(notdir $(IMAGE_LIB_SRC:.cpp=.o))

HEADERS := $(LIB_CODE_HEADERS) $(IMAGE_LIB_HEADERS)
OBJS := rcoaster.o $(LIB_CODE_OBJS) $(IMAGE_LIB_OBJS)

OPT := -O3

uname_s := $(shell uname -s)
ifeq ($(uname_s),Linux)
  	INCLUDE := -Ivendor/glm/ -I$(LIB_CODE_BASE) -Ivendor/imageIO
  	LIB := -lGLEW -lGL -lglut -ljpeg
  	LDFLAGS :=
else
	INCLUDE := -Ivendor/glm/ -I$(LIB_CODE_BASE) -Ivendor/imageIO -Ivendor/jpeg-9a-mac/include
  	LIB := -framework OpenGL -framework GLUT vendor/jpeg-9a-mac/lib/libjpeg.a
  	CXXFLAGS += -Wno-deprecated-declarations
  	LDFLAGS := -Wl,-w
endif

rcoaster : $(OBJS)
	$(CXX) $(LDFLAGS) $^ $(OPT) $(LIB) -o $@

rcoaster.o : rcoaster.cpp $(HEADER)
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

$(LIB_CODE_OBJS) : %.o : $(LIB_CODE_BASE)/%.cpp $(LIB_CODE_HEADERS)
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

$(IMAGE_LIB_OBJS) :%.o : vendor/imageIO/%.cpp $(IMAGE_LIB_HEADERS)
	$(CXX) -c $(CXXFLAGS) $(OPT) $(INCLUDE) $< -o $@

clean:
	rm -rf *.o rcoaster