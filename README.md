![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white) ![Visual Studio Code](https://img.shields.io/badge/Visual%20Studio%20Code-0078d7.svg?style=for-the-badge&logo=visual-studio-code&logoColor=white)

# smallShell
A small interactive shell written in C by Chris Peterman

## Description
This is a lightweight mock verison of shell written in the C language. It supports running foreground and background processes, file redirection, and three internal commands, status, cd, and exit.
This was a class project that synthesized what I learned in the C language, as well as the process of forking, signal interrupts, directory navigation, and file redirection. It was challenging learning all the new concepts at once and implementing them in such a project, but I am extremely proud of the accomplishment and what I've created.

## Installation
Download smallsh.c and the corresponding Makefile and place them in the same directory.

## Running the program

### Using the GCC Compiler (Recommended)

Compile and run with the following commands
```
gcc -std=gnu99 -g -Wall -o smallsh smallsh.c

./smallsh
```

## Usage
The smallShell supports all bash commands as well as its own internal commands. When the shell is running you will be prompted with : to indicate a command can be put on the line.

### cd
Change directory is an internal command that is modified in the following ways.

Default cd will navigate to the $HOME directory on your system.

```c
: cd

```

Entering a name will cause the program to search for a directory with that name from PATH, it will return an error if it is not found.

```c
: cd myDirectory

```

Entering a / or ./ in front of the directory name will search the current directory for the folder name it will return an error if it is not found.

```c
: cd ./myDirectory
: cd /myDirectory

```

### status
Typing in Status will return the exit status of the last foreground process. If the foreground process exited normally it will return 0, otherwise it will return 1 for abnormal termination.

```c
: status
exit value 0
: 
```

### exit
Exit will terminate all foreground and background processes and exit the program.

```c
: exit

```

### Foreground Processes
Entering a legal bash command will start it running in the foreground. The shell will be paused until the currently running foreground process is completed.

```c
: echo Hello World
: ls -al
: sleep(2)

```

### Running a background process
Adding a & at the end of the command will signal to smallShell that the process should be run in the background. When started the background process will signal with the pid and a message. Once the next command is entered after a background process has completed, smallShell will signal the user with a message and the corresponding pid that completed.

```c
: sleep 5
Starting Background Process for id: 23540
: echo hello
hello
background pid 23540 terminated: signal 0
: 

```

### Signal interrupts - Ctrl-c
Entering SIGINT or Ctrl-c on the keyboard will terminate the current foreground process. The parent process and all background processes will ignore this signal.

```c
: sleep 4
^Cpid 23620 terminated: signal 2
: 
```

### Signal interrupts - Ctrl-z
Entering SIGTSTP or Ctrl-z will cause the program to enter and leave foreground only mode. It will ignore all inputs for starting a background process with & and run every command in foreground mode. Entering and exiting foreground only mode will show a message.

```c
:^ZForeground mode activated.
echo hello
hello
:sleep 2 &
:^ZForeground mode disabled.

```

## License

MIT License

Copyright (c) [2022] [Chris Peterman]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.