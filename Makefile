linker: linker.cpp
	clang++ --std=c++17 linker.cpp -o linker

clean:
	rm linker