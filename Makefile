GIT_HASH=`git describe --always --dirty`
COMPILE_TIME=`date -u +'%Y-%m-%d %H:%M:%S UTC'`
COMPILE_TS=`date -u +'%Y%m%d%H%M'`
GIT_BRANCH=`git branch | grep "^\*" | sed 's/^..//'`
export VERSION_FLAGS=-DGIT_HASH="\"$(GIT_HASH)\"" -DCOMPILE_TIME="\"$(COMPILE_TIME)\"" -DGIT_BRANCH="\"$(GIT_BRANCH)\""


# Folder structure:
OBJ := obj
OBJARM := objarm
SRC := src
INC := include
EXE := exe
BIN := bin/$(GIT_HASH)_$(COMPILE_TS)/

# Compilers:
ifndef ROOTSYS
	CXX = g++
	CC = gcc
	F77 = gfortran
else
	CXX = $(shell root-config --cxx)
	CC  = $(shell root-config --cc)
	F77 = $(shell root-config --f77)
endif

UNAME_S := $(shell uname -s)

CROSS_COMPILE = arm-linux-gnueabihf
CCARM = $(CROSS_COMPILE)-g++
LDARM = $(CROSS_COMPILE)-g++

# Root specific (unused for now):
ifdef ROOTSYS
	ROOTCFLAGS    =
	ROOTLIBS      =
	ROOTGLIBS     =
else
	ROOTCFLAGS    = $(shell root-config --cflags)
	ROOTLIBS      = $(shell root-config --libs)
	ROOTGLIBS     = $(shell root-config --glibs)
endif

# DE10 specific:
ALT_DEVICE_FAMILY ?= soc_cv_av
#SOCEDS_DEST_ROOT = /home/depa/intelFPGA/20.1/embedded
HWLIBS_ROOT = $(SOCEDS_DEST_ROOT)/ip/altera/hps/altera_hps/hwlib

# Flags and includes:
INCLUDE := -I$(INC)
ifdef ROOTSYS
	INCLUDE += -I$(ROOTSYS)/include
endif
INCLUDEARM += $(INCLUDE)

CFLAGS := -Wall -Wextra -pthread #-fsanitize=address
LDFLAGS := -Wall -Wextra -pthread #-fsanitize=address

CPPFLAGS := $(CFLAGS) -std=c++11 $(INCLUDE)
CFLAGSARM := $(CFLAGS) $(INCLUDEARM) -I$(HWLIBS_ROOT)/include -I$(HWLIBS_ROOT)/include/$(ALT_DEVICE_FAMILY) -D$(ALT_DEVICE_FAMILY)

OCAOPTFLAG := -g
HPSOPTFLAG := -g
# OCAOPTFLAG := -O3
# HPSOPTFLAG := -O2

# Objects and sources:
OBJECTS=$(OBJ)/main.o $(OBJ)/de10_silicon_base.o $(OBJ)/tcpclient.o $(OBJ)/daqserver.o $(OBJ)/tcpServer.o $(OBJ)/utility.o
OBJECTSTEST=$(OBJ)/maintest.o $(OBJ)/daqclient.o $(OBJ)/tcpclient.o $(OBJ)/utility.o

OBJECTSHPS := $(OBJARM)/server.o $(OBJARM)/hpsServer.o $(OBJARM)/highlevelDriversFPGA.o $(OBJARM)/lowlevelDriversFPGA.o $(OBJARM)/tcpServer.o

# Executables:
PAPERO := $(EXE)/PAPERO
OCADAQ := $(EXE)/OCA
OCATEST := $(EXE)/testOCA

# Rules:
all: clean $(OCADAQ) $(OCATEST) $(PAPERO)

oca: cleanoca $(OCADAQ) $(OCATEST)

papero: cleanpapero $(PAPERO)

$(OCADAQ): $(OBJECTS)
	@echo Linking $^ to $@
	@mkdir -pv $(EXE)
	@mkdir -pv $(BIN)
	$(CXX) $(CPPFLAGS) $^ -o $@ $(ROOTGLIBS)
	@cp -v $(OCADAQ) $(BIN)/

$(OCATEST): $(OBJECTSTEST)
	@echo Linking $^ to $@
	@mkdir -pv $(EXE)
	@mkdir -pv $(BIN)
	$(CXX) $(CPPFLAGS) $^ -o $@ $(ROOTGLIBS)
	@cp -v $(OCATEST) $(BIN)/

$(PAPERO): $(OBJECTSHPS)
ifeq ($(UNAME_S),Darwin)
	@echo Compilation under MacOs not possibile
else
	@echo Linking $^ to $@
	@mkdir -pv $(EXE)
	@mkdir -pv $(BIN)
	$(LDARM) $(LDFLAGS) $^ -o $(PAPERO)
	@cp -v $(PAPERO) $(BIN)/
endif

##SUMMARY: $(TOP)/TakeData/summary.o  $(TOP)lib/libamswire.a
#SUMMARY: $(TOP)/TakeData/summary.o
#	@echo Linking $@ ...
##	$(CXX) $(CFLAGS) $(ROOTCFLAGS) $(CPPFLAGS) -o $@ $(TOP)/TakeData/summary.o -L$(TOP)lib/ -lamswire $(ROOTLIBS)
#	$(CXX) $(CFLAGS) $(ROOTCFLAGS) $(CPPFLAGS) -o $@ $(TOP)/TakeData/summary.o $(ROOTLIBS)
#	ln -fs SUMMARY SUMMARY_MUTE

$(OBJ)/%.o: $(SRC)/%.cpp
	@echo Compiling $< ...
	@mkdir -pv $(OBJ)
	$(CXX) $(CPPFLAGS) $(OCAOPTFLAG) $(VERSION_FLAGS) -c -o $@ $<

#Objects
$(OBJARM)/%.o: $(SRC)/%.c
ifeq ($(UNAME_S),Darwin)
	@echo Compilation under MacOs not possibile
else
	@echo Compiling $< ...
	@mkdir -pv $(OBJARM)
	$(CCARM) $(CFLAGSARM) $(HPSOPTFLAG) $(VERSION_FLAGS) -c -o $@ $<
endif

clean:
	@echo " Cleaning all..."
	@$(RM) -Rfv $(OBJ)
	@$(RM) -Rfv $(OCADAQ)
	@$(RM) -Rfv $(OBJARM)
	@$(RM) -Rfv $(EXE)

cleanoca:
	@echo " Cleaning OCA..."
	@$(RM) -Rfv $(OBJ)
	@$(RM) -Rfv $(OCADAQ)

cleanpapero:
	@echo " Cleaning PAPERO..."
	@$(RM) -Rfv $(OBJARM)
	@$(RM) -Rfv $(PAPERO)


.PHONY: clean cleanoca cleanpapero
