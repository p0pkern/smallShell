# smallShell
A small interactive shell written in C

## Description
This is a lightweight mock verison of shell written in the C language. It supports running foreground and background processes, file redirection, and three internal commands, status, cd, and exit.
This was a class project that synthesized what I learned in the C language, as well as the process of forking, signal interrupts, directory navigation, and file redirection. It was challenging learning all the new concepts at once and implementing them in such a project, but I am extremely proud of the accomplishment and what I've created.

## Installation
Download smallsh.c and the corresponding Makefile and place them in the same directory.

## Running the program

### Using the GCC Compiler (Recommended)

Compile with the following command
```
gcc -std=gnu99 -g -Wall -o smallsh smallsh.c

./smallsh
```

## Usage
The smallShell supports all bash commands as well as its own internal commands. When the shell is running you will be prompted with : to indicate a command can be put on the line.

### Change Directory

