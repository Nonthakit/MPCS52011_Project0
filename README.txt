# Compiling

To compile the source code, make sure you have GNU compiler with C++11 in your environment.
Then run `g++ vm_translator.cpp -o vm_translator` with in directory `src`

# Usage

To use the program run command:

Linux: `./vm_translator <vm_file_path>`

Window: `vm_translator.exe <vm_file_path>`

`file_path` has to have .vm extension. The output will be the same filename with
.asm extension within the same directory as input file.

