# Solver makefile

EXE = teststub
THIRDPARTY ?= ../thirdparty

BUILD ?= Debug

MPICXX ?= mpicxx
CXX=$(MPICXX)

####################

SRC = $(addprefix src/, teststub.cpp)
OBJ = $(patsubst %.cpp,%.o,$(SRC))

CXXFLAGS += -Wall -Wextra -std=c++11 -rdynamic
ifeq ($(BUILD),Release)
  CXXFLAGS += -O3 -g
else
  CXXFLAGS += -O0 -g
endif
CXXFLAGS += -I$(THIRDPARTY)/yaml-cpp.bin/include -I$(THIRDPARTY)/argsparser.bin
CXXFLAGS += -Isrc -Isrc/dbg

LDFLAGS += -L$(THIRDPARTY)/yaml-cpp.bin/lib -Wl,-rpath -Wl,$(THIRDPARTY)/yaml-cpp.bin/lib -lyaml-cpp
LDFLAGS += -L$(THIRDPARTY)/argsparser.bin -Wl,-rpath -Wl,$(THIRDPARTY)/argsparser.bin -largsparser
LDFLAGS += -ldl

all: $(EXE)

$(EXE): $(OBJ)
	$(CXX) -rdynamic -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(EXE) $(OBJ)

deps:
	g++ -c $(CXXFLAGS) -MM $(SRC) > Makefile.deps

-include Makefile.deps

$(OBJ): Makefile

ifneq ($(filter-out clean,$(MAKECMDGOALS)),)
ifeq ($(findstring clean,$(MAKECMDGOALS)),clean)
$(error `clean` rule cannot be combined with other rules)
endif
endif

ifneq ($(filter-out deps,$(MAKECMDGOALS)),)
ifeq ($(findstring deps,$(MAKECMDGOALS)),deps)
$(error `deps` rule cannot be combined with other rules)
endif
endif
