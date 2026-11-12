# Implement a Linker

I am revisiting one of the labs from my Operating Systems class (NYU Fall 2024, Professor Hubertus Franke) for fun and to brush up on my C++.

This lab involves implementing a two-pass linker.

The input is a text file containing information about the modules, the symbols defined or used in each module, and the instructions within each module.

In the first pass, we build a symbol table and determine the base address of each module based on its size. 

In the second pass, we rewrite some of the instructions, relocating relative addresses and resolving external symbol references. See `notes.md` for more details.

The lab is quite far from how a real linker works, but I think it's still a good exercise.

## Usage

Compile with C++17 and run with an input file.

```bash
clang++ -std=c++17 linker.cpp -o linker
./linker input/input-1
```

The results are printed to `stdout`. Reference outputs are available in the `output/` directory.

There are scripts for grading.

```bash
mkdir myout

# write results to myout
./script/runit.sh myout

# compare myout with reference output
./script/gradeit.sh . myout
```

Expected result:

```
RES 20 of 20:  1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1

LABRESULTS = 40 + 60.0000 - DEDUCTION = 100.0000
```