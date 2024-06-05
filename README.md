# Spellchecker Program

This spellchecker program uses a multi-threaded approach to process multiple text files concurrently, checking for spelling errors against a given dictionary. It also logs the most common misspelled words and can output results to a file.

## Table of Contents
1. [Features](#features)
2. [Requirements](#requirements)
4. [Files and Functions](#files-and-functions)
5. [Program Flow](#program-flow)
6. [Thread Management](#thread-management)
7. [Building and Running the Program](#building-and-running-the-program)
8. [Additional Information](#additional-information)

## Features
- Multi-threaded spellchecking for efficient processing of multiple files.
- HashSet data structure for fast dictionary lookups.
- Aggregate and report the most common misspelled words.
- Option to log results to a file.
- Python integration for preprocessing input files.

## Requirements
- GCC for compiling the C program.
- Python 3 for running the preprocessing script.
- POSIX-compliant operating system (Linux, macOS).

## Files and Functions
- **spellchecker.c**: Main source file containing the spellchecker program.
- **replaceApostrophe.py**: Python script used to preprocess input files.

### Main Functions
- `main()`: Entry point of the program.
- `spellprocessing()`: Thread function to process a single task.
- `createHashSet()`, `freeHashSet()`, `insertHashSet()`, `containsHashSet()`: Functions to manage the HashSet.
- `checkSpellingAndCountMisspellings()`: Check spelling and count misspelled words in a file.
- `callPythonScript()`: Function to call the Python preprocessing script.
- `printTopMisspellings()`: Print the most common misspelled words.
- `addCurrentThread()`, `removeCurrentThread()`: Manage active threads.
- `wordCheck()`, `hasDigit()`, `toLowerCase()`: Helper functions for word processing.

## Program Flow
1. **Initialization**: 
    - Parse command-line arguments.
    - Initialize global variables and mutexes.

2. **User Interaction**:
    - Present a menu to the user.
    - Based on user input, start a new spellchecking task or exit the program.

3. **Task Processing**:
    - For each task, create a new thread to process the input file.
    - Threads use a HashSet to load the dictionary and check spelling.

4. **Output Results**:
    - Print results to the console or log to a file based on user choice.
    - Display the top 3 most common misspelled words.

## Thread Management
- **Thread Creation**: New threads are created for each spellchecking task, up to a maximum limit defined by `THREAD_LIMIT`.
- **Thread Synchronization**: 
    - Mutexes ensure thread-safe access to shared resources such as the list of misspelled words and the count of active tasks.
    - Condition variables manage the state of threads and ensure proper synchronization.

## Building and Running the Program
### Compilation
To compile the program, use the following command:
```sh
make
```

### Running the Program
To run the program, use:
```sh
./checker
```
To enable logging to a file:
```sh
./checker -l
```

## Additional Information
### HashSet Implementation
The HashSet is used to store the dictionary words for fast lookup. It uses a simple hash function and linked lists to handle collisions.

### Python Integration
The `replaceApostrophe.py` script is called to preprocess input files, replacing apostrophes and other preprocessing tasks. The script's output is read and used in the main program.

### Logging
If logging is enabled (`-l` flag), the results of each task are appended to `username_O.out`.

### Error Handling
The program includes robust error handling for file operations, memory allocation, and thread management to ensure stability and provide useful error messages.

### Future Improvements
- Enhance dictionary management (e.g., allowing dynamic resizing).
- Improve user interface and input validation.
- Extend functionality to support additional languages and custom dictionaries.
