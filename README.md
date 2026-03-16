# IHTC2024 Multi-Neighborhood Hybrid

This repository contains the source code for the UGent Mippets Team for IHTC2024. Follow the instructions below to compile and run the source code. This is the source code used for the following publication:

Garcia Tercero, L., Van Bulck, D., Devriesere, K., Bakker, S., & Goossens, D. (2026). A multi-neighborhood hybrid approach for the integrated healthcare timetabling competition 2024. Computers & Operations Research, 107453. https://doi.org/10.1016/j.cor.2026.107453

## Requirements
- Linux system
- `make` installed
- g++
- Bash shell
- Gurobi 12.0 installed. Note: Gurobi is free for academics, via the following [link](https://www.gurobi.com/features/academic-named-user-license/).
- Boost installed
- (Cliquer program, this is included in our source code and will be compiled automatically.)


### 1A CMake
_Make sure CMake can find Boost: https://cmake.org/cmake/help/latest/module/FindBoost.html_

Set up the cmake lists for gurobi
```bash
$ cd ./IHTC
$ cd ./cmake
```

Tell cmake where to find Gurobi. Add in FindGUROBI.cmake the paths to gurobi in find_path, find_library, and find_library. Please make sure to use Gurobi 12.0 for speed reasons!

Compile with cmake:

```bash
$ cd IHTC
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build . --config Release -j 8
```


### 1B Make
Ignore this step if you completed 1A. This is an alternative to using CMAKE.

Install the required additional packages.
- DLIB installed, see [https://dlib.net/](https://dlib.net/compile.html). The source code for dlib is already included. It should suffice to run the following commands from dlib directory to get dlib installed.

```bash
$ cd IHTC
$ cd dlib
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build . --config Release
```

Adapt the Makefile as needed. (Make sure to use the right Makefile, not the one made by Cmake. Redownload as necessary.)

- Put the flag for the right release mode (USER) to 1, put all others to zero.
- Make sure the STATUS flag is set to RELEASE, not DEBUG. If set correctly, you should see minimal printing during running.
- Set paths to gurobi 12.0 for your name.

```
else ifeq ($(ANDREA), 1)
        GUROBI  = YOUR/PATH/TO/INSTALL/FOLER
        INCLUDEGUROBI = -I${GUROBI}/include/
        LINKGUROBI = -L${GUROBI}/lib
else
```
### 2 Compile

To compile the algorithm, run the following command in the terminal:

```bash
$ make
```

## Running

To run the software on i30, please use the following command:

```bash
$ ./ihtc --instance /PATHTOINSTANCES/Instances/ihtc2024_competition_instances/i30.json
```

If you additionally want to provide the filename for the solution file, use:
```bash
--solution mysol.sol
```
