# To use custom paths, put USER to 1
USER = 0

APP = ihtc
DEBUG = -O1  -g  
RELEASE = -Ofast -frename-registers -funroll-loops -march=native -DCLIQUE -DNDEBUG -DMINIMALPRINT
STATUS = $(RELEASE)

CC = g++
LD = g++

ifeq ($(USER), 1)
	GUROBI  = YOUR/PATH/TO/INSTALL/FOLER
	INCLUDEGUROBI = -I${GUROBI}/include/
	LINKGUROBI = -L${GUROBI}/lib
else 
	# Default example settings
	GUROBI  = /opt/gurobi952/linux64/
	INCLUDEGUROBI = -I${GUROBI}/include/
	LINKGUROBI = -L${GUROBI}/lib
endif

# Use all threads to compile in parallel
MAKEFLAGS += -j 8

MODULES   := $(shell ls ./source/)
SRC_DIR   := $(addprefix source/,$(MODULES))
BUILD_DIR := $(addprefix build/,$(MODULES))

SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
SRC       += $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cc))
SRC       += $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ       := $(patsubst source/%.cpp,build/%.o,$(SRC))
INCLUDES  := $(addprefix -I,$(SRC_DIR))
INCLUDES += -Idlib

CFLAGS += -std=c++17 ${INCLUDES} -fopenmp
LDFLAGS += -lpthread -std=c++11 -ldl ${INCLUDES} -fopenmp

CFLAGS += ${INCLUDEGUROBI} 
LDFLAGS += ${LINKGUROBI} -lgurobi_c++ 

ifeq ($(USER), 1)
	LDFLAGS += -lgurobi120 -lm 
else
	LDFLAGS += -lgurobi95 -lm 
endif

vpath %.cpp $(SRC_DIR)

define make-goal
$1/%.o: %.cpp
	@echo "[CC] $<"
	$(CC) $(CFLAGS) $(STATUS) -c $$< -o $$@
endef

.PHONY: all checkdirs clean

all: checkdirs build/$(APP)
build/$(APP): $(OBJ)
	@echo "[LD] $<"
	$(LD) $(ALLPADBis)  $^ $(LDFLAGS) -o $(APP)

checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD_DIR)
	@rm $(APP)

$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))
