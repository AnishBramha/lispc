# lispc
Single-pass Lisp subset compiler

## Platforms Supported

- Darwin ARM64
- GAS (GNU Assembler) x86-64

## Usage

`lispc <file>.lisp [-gnu|-clean|-run|-compile|-help]`

- Options:
    * `-clean`: Produce no intermediate files
    * `-run`: Run the produced executable
    * `-compile`: Compile to Assembly, but do not link to form executable

- No options starts an REPL
