# Friendly Intro to Lab 1

Let's take a look at the sample input. 

```
1 xy 2
2 z xy
5 R 1004  I 5678  E 2000  R 8002  E 7001
0
1 z
6 R 8001  E 1000  E 1000  E 3000  R 1002  A 1010
0
1 z
2 R 5001  E 4000
1 z 2
2 xy z
3 A 8000  E 1001  E 2000
```

Below is the annotated version for better readability. 

```
<Module 0>

1 symbol defined
xy=2 (Here, 2 is the relative address of xy)

2 symbols used 
z, xy

5 instructions
R 1004  I 5678  E 2000  R 8002  E 7001

<Module 1>

0 symbol defined

1 symbol used
z

6 instructions
R 8001  E 1000  E 1000  E 3000  R 1002  A 1010

<Module 2>

0 symbol defined

1 symbol used
z

2 instructions
R 5001  E 4000

<Module 3>

1 symbol defined 
z=2

2 symbols used 
xy z

3 instructions
A 8000  E 1001  E 2000
```

There are three modules. 

A module is a unit of code that helps organize and encapsulate components, such as functions, classes, and variables, into a separate, reusable, and maintainable structure, i.e., each cpp file is a module.

Each module can define a new symbol (functions or variables), use existing symbols, and have instructions. 

When a compiler compiles a module, it does not resolve external references. For instance, take a look at module 1. Module 1 uses symbol z, which is defined in module 3. When a compiler compiles module 1, it does not check whether z exists in some other module. The compiler assumes that z is defined somewhere else. 

**This is where the linker comes into play.** Linker is responsible for resolving external references, and throwing errors if external references do not exist.

Our linker is a two pass linker. Let's see what happens in each pass.

### Pass 1
In pass1, we do two things. 

1. Make a module base table
2. Make a symbol table

A module base table maps a module to its base address. The size of the module (and its base address) is determined by the number of instructions.

```
Module    Base
0         0
1         5    (Module 0 has 5 instructions)
2         11   (Module 1 has 6 instructions)
3         13   (Module 2 has 2 instructions)
```

A symbol table maps a symbol to its global (absolute) address.
```
Symbol    Absolute address
xy        2
z         15

xy = 0+2 (base address of module 0 + relative address of xy)
z = 13+2 (base address of module 3 + relative address of z)
```

### Pass 2

Now that we have the module base table and the symbol table, we can resolve external references. The output will be the following.

```
Symbol Table
xy=2
z=15

Memory Map
000: 1004
001: 5678
002: 2015
003: 8002
004: 7002
005: 8006
006: 1015
007: 1015
008: 3015
009: 1007
010: 1010
011: 5012
012: 4015
013: 8000
014: 1015
015: 2002
```

Take a look at the third line in memory map. The original instruction was `E 2000`. The instruction points to the symbol z, thus the instruction is resolved to `2015` where 15 is the global address of z.
