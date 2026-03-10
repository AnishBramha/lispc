# lispc
Single-pass Lisp subset compiler

## Platforms Supported

- Darwin ARM64
- NASM x86-64

## Usage

`lispc <file>.lisp [-nasm|-clean|-run|-compile]`

- Default target is Darwin
- Memory leaks (if displayed) can be ignored
- For NASM, uncomment the compiler variables in the Makefile
- Options:
    * `-nasm`: Compiles to NASM x86_64 for Linux
    * `-clean`: Produces no intermediate files
    * `-run`: Runs the produced executable
    * `-compile`: Compiles to Assembly, but does not link to form executable


