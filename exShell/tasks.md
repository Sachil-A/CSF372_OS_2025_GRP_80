# Shell Assignment #0

This assignment involves enhancing the Pintos shell functionality with several improvements. Complete the following tasks:

- [Fix echo Command](#fix-echo-command)
- [Create touch Command](#create-touch-command)]
- [Add Output Redirection](#add-output-redirection)

## Fix echo Command

**Current Behavior:**
When you run the command `echo hi`, the shell prints `echo hi` (includes the command name in the output).

**Required Fix:**
Modify the echo command implementation to only print the arguments, not the command name itself. After your fix, running `echo hi` should only print `hi` to the console.

**Implementation Tips:**
- Locate the echo command implementation in the source code
- Modify the output logic to skip the command name
- Test with various inputs to ensure proper functionality

## Create touch Command

**Task Requirements:**
1. Implement a new `touch` command that creates empty files
2. The command should accept a filename as an argument: `touch filename.txt`
3. If the file already exists, the command should update the file's timestamp
4. If the file doesn't exist, create a new empty file

**Implementation Steps:**
1. Create a new source file for the touch command
2. Implement the file creation/modification functionality
3. Add the touch command to the build system (Makefile)
4. Compile and add the touch command to the filesystem using:
   `pintos -p path/to/touch -a touch -- -q`

## Add Output Redirection

**Feature Description:**
Implement output redirection for the shell using the `>` operator.

**Expected Behavior:**
- Running `echo hi > filename.txt` should redirect the output "hi" to the file `filename.txt` instead of displaying it on the console
- If the file already exists, it should be overwritten
- If the file doesn't exist, it should be created

**Implementation Guidelines:**
1. Modify the shell's command parsing to detect the `>` operator
2. Extract the filename that follows the `>` operator
3. Redirect the standard output of the command to the specified file
4. Ensure proper error handling (file permissions, invalid paths, etc.)
5. Test with various commands to verify functionality

**Testing Strategy:**
- Test with different commands like `echo`, `ls`, etc.
- Verify file contents after redirection
- Test edge cases such as multiple redirections or invalid syntax
