# lispc
Single-pass Lisp subset compiler

## Platforms Supported

- Darwin ARM64
- NASM x86_64

## Usage

`lispc <file>.lisp [-nasm]`

- Default target is Darwin
- Memory leaks (if displayed) can be ignored
- For NASM, uncomment the compiler variables in the Makefile


