GIT_HASH := $(shell git describe --always --dirty)
BUILD_DATE := $(shell date -u +'%Y%m%d%H%M %Y-%m-%d %H:%M:%S UTC')
COMPILE_TIME := $(wordlist 2,4,$(BUILD_DATE))
COMPILE_TS := $(firstword $(BUILD_DATE))
GIT_BRANCH := $(shell git rev-parse --abbrev-ref HEAD)
export VERSION_FLAGS := -DGIT_HASH="\"$(GIT_HASH)\"" -DCOMPILE_TIME="\"$(COMPILE_TIME)\"" -DGIT_BRANCH="\"$(GIT_BRANCH)\""


# Folder structure:
OBJ := obj
OBJARM := objarm
SRC := src
INC := include
EXE := exe
BIN := bin/$(GIT_HASH)_$(COMPILE_TS)

# Compilers:
ROOT_CONFIG := $(shell command -v "$(if $(ROOTSYS),$(ROOTSYS)/bin/,)root-config" 2>/dev/null)
ifeq ($(ROOT_CONFIG),)
	CXX = g++
	CC = gcc
	F77 = gfortran
else
	CXX = $(shell "$(ROOT_CONFIG)" --cxx)
	CC  = $(shell "$(ROOT_CONFIG)" --cc)
	F77 = $(shell "$(ROOT_CONFIG)" --f77)
	ROOTCFLAGS := $(shell "$(ROOT_CONFIG)" --cflags)
	ROOTLIBS   := $(shell "$(ROOT_CONFIG)" --libs)
	ROOTGLIBS  := $(shell "$(ROOT_CONFIG)" --glibs)
endif

UNAME_S := $(shell uname -s)

# DE10 specific:
CROSS_PATH ?=
CROSS_PREFIX ?= arm-linux-gnueabihf
CROSS_COMPILE ?= $(if $(CROSS_PATH),$(CROSS_PATH)/bin/)$(CROSS_PREFIX)
CCARM ?= "$(CROSS_COMPILE)-g++"
LDARM ?= "$(CROSS_COMPILE)-g++"
ALT_DEVICE_FAMILY ?= soc_cv_av
HWLIBS_ROOT ?= $(if $(SOCEDS_DEST_ROOT),$(SOCEDS_DEST_ROOT)/ip/altera/hps/altera_hps/hwlib)

# Flags and includes:
INCLUDE := -I$(INC)
INCLUDE += $(ROOTCFLAGS)
INCLUDEARM += -I$(INC)

DEBUGFLAGS := #-g -fsanitize=address -fstack-protector
CFLAGS := -Wall -Wextra -pthread -std=c++11 $(DEBUGFLAGS)
LDFLAGS := -Wall -Wextra -pthread $(DEBUGFLAGS)

CPPFLAGS := $(CFLAGS) $(INCLUDE) 
CFLAGSARM := $(CFLAGS) $(INCLUDEARM) -D$(ALT_DEVICE_FAMILY)
ifneq ($(strip $(HWLIBS_ROOT)),)
CFLAGSARM += -I"$(HWLIBS_ROOT)/include" -I"$(HWLIBS_ROOT)/include/$(ALT_DEVICE_FAMILY)"
endif

OCAOPTFLAG := -g
HPSOPTFLAG := -g
# OCAOPTFLAG := -O3
# HPSOPTFLAG := -O2

# Objects and sources:
OBJECTS=$(OBJ)/main.o $(OBJ)/de10_silicon_base.o $(OBJ)/tcpclient.o $(OBJ)/daqserver.o $(OBJ)/tcpserver.o $(OBJ)/utility.o $(OBJ)/daqConfig.o $(OBJ)/paperoConfig.o $(OBJ)/anyoption.o $(OBJ)/makaClient.o $(OBJ)/udpSocket.o

OBJECTSTEST=$(OBJ)/maintest.o $(OBJ)/daqclient.o $(OBJ)/tcpclient.o $(OBJ)/utility.o
OBJECTSSTART=$(OBJ)/mainstart.o $(OBJ)/daqclient.o $(OBJ)/tcpclient.o $(OBJ)/utility.o
OBJECTSSTOP=$(OBJ)/mainstop.o $(OBJ)/daqclient.o $(OBJ)/tcpclient.o $(OBJ)/utility.o

OBJECTSMAKA=$(OBJ)/tcpclient.o $(OBJ)/tcpserver.o $(OBJ)/utility.o $(OBJ)/anyoption.o $(OBJ)/maka.o $(OBJ)/makaMerger.o $(OBJ)/udpSocket.o

OBJECTSHPS := $(OBJARM)/papero.o $(OBJARM)/hpsDataServer.o $(OBJARM)/hpsServer.o $(OBJARM)/tcpserver.o $(OBJARM)/utility.o $(OBJARM)/fpgaDriver.o $(OBJARM)/axiFifo.o

# Executables:
PAPERO := $(EXE)/PAPERO
OCADAQ := $(EXE)/OCA
OCATEST := $(EXE)/testOCA
OCASTART := $(EXE)/startOCA
OCASTOP := $(EXE)/stopOCA
MAKA := $(EXE)/MAKA

# Rules:
all: $(OCADAQ) $(OCATEST) $(MAKA) $(PAPERO) $(OCASTART) $(OCASTOP)

oca: $(OCADAQ) $(OCATEST)

maka: $(MAKA)

startstop : $(OCASTART) $(OCASTOP)

papero: $(PAPERO)

$(OCADAQ): $(OBJECTS)
	@echo Linking $^ to $@
	@mkdir -pv $(EXE)
	@mkdir -pv "$(BIN)"
	$(CXX) $(CPPFLAGS) $^ -o $@ $(ROOTGLIBS)
	@cp -v "$@" "$(BIN)/"

$(OCATEST): $(OBJECTSTEST)
	@echo Linking $^ to $@
	@mkdir -pv $(EXE)
	@mkdir -pv "$(BIN)"
	$(CXX) $(CPPFLAGS) $^ -o $@ $(ROOTGLIBS)
	@cp -v "$@" "$(BIN)/"

$(MAKA): $(OBJECTSMAKA)
	@echo Linking $^ to $@
	@mkdir -pv $(EXE)
	@mkdir -pv "$(BIN)"
	$(CXX) $(CPPFLAGS) $^ -o $@ $(ROOTGLIBS)
	@cp -v "$@" "$(BIN)/"
	
$(OCASTART): $(OBJECTSSTART)
	@echo Linking $^ to $@
	@mkdir -pv $(EXE)
	@mkdir -pv "$(BIN)"
	$(CXX) $(CPPFLAGS) $^ -o $@ $(ROOTGLIBS)
	@cp -v "$@" "$(BIN)/"	

$(OCASTOP): $(OBJECTSSTOP)
	@echo Linking $^ to $@
	@mkdir -pv $(EXE)
	@mkdir -pv "$(BIN)"
	$(CXX) $(CPPFLAGS) $^ -o $@ $(ROOTGLIBS)
	@cp -v "$@" "$(BIN)/"	

# Link statically for compatibility with the DE10 HPS root filesystem.
$(PAPERO): $(OBJECTSHPS)
ifeq ($(UNAME_S),Darwin)
	@echo Compilation under MacOs not possibile
else
	@echo Linking $^ to $@
	@mkdir -pv $(EXE)
	@mkdir -pv "$(BIN)"
	$(LDARM) $(LDFLAGS) -static $^ -o $@
	@cp -v "$@" "$(BIN)/"
endif

##SUMMARY: $(TOP)/TakeData/summary.o  $(TOP)lib/libamswire.a
#SUMMARY: $(TOP)/TakeData/summary.o
#	@echo Linking $@ ...
##	$(CXX) $(CFLAGS) $(ROOTCFLAGS) $(CPPFLAGS) -o $@ $(TOP)/TakeData/summary.o -L$(TOP)lib/ -lamswire $(ROOTLIBS)
#	$(CXX) $(CFLAGS) $(ROOTCFLAGS) $(CPPFLAGS) -o $@ $(TOP)/TakeData/summary.o $(ROOTLIBS)
#	ln -fs SUMMARY SUMMARY_MUTE

$(OBJ)/%.o: $(SRC)/%.cpp Makefile
	@echo Compiling $< ...
	@mkdir -pv $(OBJ)
	$(CXX) $(CPPFLAGS) $(OCAOPTFLAG) $(VERSION_FLAGS) -c -o $@ $<

$(OBJARM)/%.o: $(SRC)/%.cpp Makefile
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
	@$(RM) -Rfv $(MAKA)
	@$(RM) -Rfv $(EXE)

cleanoca:
	@echo " Cleaning OCA..."
	@$(RM) -Rfv $(OBJ)
	@$(RM) -Rfv $(OCADAQ)

cleanmaka:
	@echo " Cleaning MAKA..."
	@$(RM) -Rfv $(OBJ)
	@$(RM) -Rfv $(MAKA)

cleanpapero:
	@echo " Cleaning PAPERO..."
	@$(RM) -Rfv $(OBJARM)
	@$(RM) -Rfv $(PAPERO)


.PHONY: all oca maka startstop papero clean cleanoca cleanpapero cleanmaka
