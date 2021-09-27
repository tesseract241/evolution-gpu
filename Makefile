CXX=g++

EXE=evolution-gpu
EXE_DEBUG=evolution-gpu_debug
SOURCES=main.cpp
SOURCES += evo-devo-gpu/source/evo-devo-gpu.cpp evo-devo-gpu/glad/src/glad.c genetic-algorithm--/source/genetic-algorithm.cpp 
OBJ_DIR=./build
OBJS=$(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(basename $(notdir $(SOURCES)))))
## Using OpenGL loader: gl3w [default]
OBJS_DEBUG=$(addprefix $(OBJ_DIR)/, $(addsuffix .ob, $(basename $(notdir $(SOURCES)))))

CXXFLAGS=-Iinclude -Ievo-devo-gpu/glad/include -Ievo-devo-gpu/include -Igenetic-algorithm--/include -ldl
## Using OpenGL loader: gl3w [default]
CXXFLAGS+= -g -Wall -Wformat -Wextra -pthread
LIBS=$(wildcard evo-devo-gpu/libs/*.a)
##---------------------------------------------------------------------
## BUILD FLAGS PER PLATFORM
##---------------------------------------------------------------------
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"

	CXXFLAGS += -I/usr/local/include -I/opt/local/include
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
	ECHO_MESSAGE = "MinGW"
	LIBS += -lglfw3 -lgdi32 -lopengl32 -limm32

	CXXFLAGS += `pkg-config --cflags glfw3`
	CFLAGS = $(CXXFLAGS)
endif

##---------------------------------------------------------------------
## VECTOR OPERATIONS BUILD FLAGS
##---------------------------------------------------------------------
CXXFLAGS_DEBUG:=$(CXXFLAGS)
CXXFLAGS+= -O2 -msse4 
CXXFLAGS_DEBUG+= -ggdb
##---------------------------------------------------------------------
## BUILD RULES
##---------------------------------------------------------------------

##---------------------------------------------------------------------
## RELEASE
##---------------------------------------------------------------------
$(OBJ_DIR)/%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: evo-devo-gpu/source/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: evo-devo-gpu/glad/src/%.c
	$(CXX) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: genetic-algorithm--/source/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: $(EXE)
	@echo $(ECHO_MESSAGE) Build complete

$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

##---------------------------------------------------------------------
## DEBUG
##---------------------------------------------------------------------
$(OBJ_DIR)/%.ob:%.cpp
	$(CXX) $(CXXFLAGS_DEBUG) -c -o $@ $<

$(OBJ_DIR)/%.ob: evo-devo-gpu/source/%.cpp
	$(CXX) $(CXXFLAGS_DEBUG) -c -o $@ $<

$(OBJ_DIR)/%.ob: evo-devo-gpu/glad/src/%.c
	$(CXX) $(CXXFLAGS_DEBUG) -c -o $@ $<

$(OBJ_DIR)/%.ob: genetic-algorithm--/source/%.cpp
	$(CXX) $(CXXFLAGS_DEBUG) -c -o $@ $<

debug: $(EXE_DEBUG)
	@echo $(ECHO_MESSAGE) Debug Build complete
	
$(EXE_DEBUG): $(OBJS_DEBUG)
	$(CXX) -o $@ $^ $(CXXFLAGS_DEBUG) $(LIBS)

.PHONY: clean

clean:
	rm -f $(EXE) $(OBJS) $(EXE_DEBUG) $(OBJS_DEBUG)
