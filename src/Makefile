.SILENT:

LLVM_FLAGS=-fuse-ld=lld `llvm-config --cxxflags` `llvm-config --ldflags --libs all bitwriter core support` -lz
WARN=-Wall -Wno-multichar -Wno-c23-extensions -Wno-unqualified-std-cast-call -Wno-unused-function
SANITIZER=-g -g3 -fsanitize=address
OPT=-O3 -fno-rtti -ffast-math -mtune=native -march=native

test:
	clear
	clang++ $(OPT) $(LLVM_FLAGS) $(SANITIZER) $(WARN) -std=c++23 main.cpp
	./a.out
	clang++ $(OPT) main.bc -omain.elf
	./main.elf
	rm a.out main.bc main.elf