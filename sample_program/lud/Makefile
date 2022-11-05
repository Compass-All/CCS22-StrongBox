# [NOTE]: Setup your Arm OpenCL path here
OPENCL_INC= 
OPENCL_LIB= 
OPENCL_LIN= 

#C compiler
CC = aarch64-linux-android-clang++
CC_FLAGS = -g -O3 -Wall

SRC = lud.cpp common.c ../strongbox_lib.c ../aes128-armv8-aarch64.S ../sha256-armv8-aarch64.S

EXE = test_lud.out

INC = $(OPENCL_INC) -I..
LIB = $(OPENCL_LIB)
LIN = $(OPENCL_LIN)

all:
	$(CC) $(SRC) $(CC_FLAGS) $(INC) $(LIB) $(LIN) -o $(EXE) -march=armv8-a+crypto -Wno-unused-result

clean:
	rm -f ./$(EXE) ./$(CLEXE)
