# ShellForge

ShellForge is a modular Unix shell developed as an Operating Systems and Systems Programming (OSSP) project.

It implements command parsing, process creation, pipelines, redirection, environment variables, shell built-ins, command history, background jobs, signals, process groups, and terminal job control using POSIX system calls.

## Requirements

- Linux / WSL
- GCC
- GNU Make
- Python 3 (required for the interactive test suite)

## Build

From the project root:

```bash
make
```

This creates the `shellforge` executable.

## Run

Start ShellForge with:

```bash
./shellforge
```

You should see:

```text
shellforge$
```

You can then use commands such as:

```text
pwd
ls
echo hello
cd ..
export NAME=ShellForge
echo "$NAME"
```

Exit the shell with:

```text
exit
```

## Supported Features

- Interactive command input
- Command history and arrow-key navigation
- Dynamic input buffers
- Tokenization and parsing
- Single and double quotes
- Escape sequences
- Environment variable expansion
- `cd`
- `pwd`
- `export`
- `history`
- `exit`
- PATH-based command execution
- Input/output redirection
- Append redirection
- stderr redirection
- Pipelines
- Multiple-command pipelines
- Background jobs using `&`
- `jobs`
- `fg`
- Process groups
- Foreground/background terminal control
- SIGINT (`Ctrl+C`)
- SIGTSTP (`Ctrl+Z`)
- Child-process management using `waitpid()`
- POSIX process creation and execution
- Error handling and recovery

## Testing

Run the complete automated test suite with:

```bash
make test
```

The test suite includes:

- Command and parser validation
- Pipeline and redirection tests
- Shell state and recovery tests
- Interactive PTY input tests
- History tests
- Process-group tests
- Job-control tests
- Terminal-control tests

## System Call Tracing

ShellForge can be inspected with `strace`:

```bash
strace -f ./shellforge
```

This allows process creation, execution, waiting, signal, pipe, file-descriptor, and terminal-control system calls to be observed.

## Clean Build

To remove the generated executable:

```bash
make clean
```

Then rebuild with:

```bash
make
```

## Project Structure

```text
ShellForge/
├── Makefile
├── README.md
├── docs/
│   ├── ARCHITECTURE.md
│   ├── DEMO.md
│   ├── DESIGN_DECISIONS.md
│   ├── PROJECT_STRUCTURE.md
│   ├── SYSCALLS.md
│   └── TESTING.md
├── include/
│   ├── history.h
│   ├── input.h
│   ├── parser.h
│   ├── shell.h
│   └── tokenizer.h
├── src/
│   ├── builtins.c
│   ├── executor.c
│   ├── history.c
│   ├── input.c
│   ├── jobs.c
│   ├── main.c
│   ├── parser.c
│   ├── shell.c
│   ├── signals.c
│   └── tokenizer.c
└── tests/
    ├── basic_shell_tests.sh
    └── interactive_shell_tests.py
```

## Quick Start

Build and run:

```bash
make clean
make
./shellforge
```

Run the verification suite:

```bash
make test
```

ShellForge is intended for Linux/WSL environments and demonstrates operating-system concepts through actual POSIX process, signal, pipe, file-descriptor, and terminal-control mechanisms.
