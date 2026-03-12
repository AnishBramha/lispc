# lispc
Single-pass Lisp subset compiler

## Platforms Supported

- Darwin ARM64
- GAS (GNU Assembler) x86-64

## Usage

`lispc <file>.lisp [-gnu|-clean|-run|-compile]`

- Default target is Darwin
- For GAS, uncomment the compiler variables in the Makefile
- Options:
    * `-gnu`: Compiles to GAS x86-64 for Linux
    * `-clean`: Produces no intermediate files
    * `-run`: Runs the produced executable
    * `-compile`: Compiles to Assembly, but does not link to form executable


