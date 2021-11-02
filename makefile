## Project Setup
TARGET	:= radarapp
PROJ_ROOT := .

BUILD_PATH := build

#SRCS	+= $(wildcard *.cpp)
SRCS	+= bts.cpp

OBJS = $(foreach src,$(SRCS),$(BUILD_PATH)/$(src:.cpp=.o))
BUILD_DIR = $(addprefix build/, $(INC_DIR))
INCS = $(addprefix -I, $(INC_DIR))


INCS += -I$(PROJ_ROOT)
INCS += -I./include ../chilkat/include

## Predefine
CDEFINES :=


## Libraries
LIBS	+= -pthread -lrt -lncurses
LIBS	+= ./lib/libbts24x.so ../chilkat/lib/libchilkat-9.5.0.so 

LIBS_DIR +=

## Compiler Setup
#.SUFFIXES: .cpp .o
CXX		:= g++

##ifeq ($(DEBUG), 1)
DBG_FLAGS := -g -O0 -DDEBUG -D_DEBUG
##else
##DBG_FLAGS := -O2 -DNDEBUG
##endif

#CFLAGS      = -Wall -Wextra $(CLIBS) $(CDEFINES) $(HWOPT) $(DBG_FLAGS)
CFLAGS      := $(DBG_FLAGS) $(CLIBS) $(CDEFINES) $(HWOPT) 
CXXFLAGS    := $(CFLAGS) -std=c++11

CXXFLAGSD	:= $(CXXFLAGS)

CFLAGS 		+= $(INCS)
CXXFLAGS 	+= $(INCS)
 

## 
MKDIR = mkdir -p


## Rule

default : $(TARGET)
	
all : clean $(TARGET)

$(BUILD_PATH)/%.o : %.cpp
	@$(MKDIR) -p $(@D)
	@echo [Compile C++] $<
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET) : $(OBJS)
	@echo --------------------------
	@echo  [Linking] $(TARGET)
	@$(CXX) -o $(TARGET) $^ $(LIB_DIRS) $(LIBS)
	@echo --------------------------
	@echo  [Done]
	@echo ==========================

clean:
	@echo --------------------------
	@echo [Clean Project]
	@rm -rf $(OBJS) $(TARGET) $(BUILD_DIR)
	@echo --------------------------

depend:
	gccmakedep $(SRCS)
 
debug:
	@echo [SRCS] $(SRCS)
	@echo [INCS] $(INCS)
	@echo [LIBS] $(LIBS)
	@echo [CLIBS] $(CLIBS)
	@echo [DEFS] $(CDEFINES)	
	@echo [OPTS] $(CXXFLAGSD)
	@echo [OBJS] $(OBJS)
	@echo [BUILD_DIR] $(BUILD_DIR)
