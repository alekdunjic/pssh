Project 1
Aleksandar Dunjic
14317753

How To Compile & Run
====================
$ make 
$ ./pssh

Description
===========
Once the simple shell is run, it prompts the user for commands.
For each command, a child process is created, and the file
descriptors are modified accordingly to allow for piping 
and input and output file redirection. Furthermore, a built-in
which command is implemented that gives the path of a command
name that is passed as an argument.
