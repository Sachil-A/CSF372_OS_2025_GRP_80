# Running Pintos

## Start the container
```bash
python pintos_manager.py --start
```
You will enter the docker shell

## Building Pintos
- In the **docker shell**
```bash
cd pintos/src/userprog 
make
cd build
pintos --
``` 

Expected output ( need not match exactly):
```
Pintos hda1
Loading............
Kernel command line:
Pintos booting with 3,968 kB RAM...
367 pages available in kernel pool.
367 pages available in user pool.
Calibrating timer...  32,716,800 loops/s.
Boot complete.
```

**Congratulations!** You have successfully built and run Pintos. You can now start working on the assignments. You can use `Ctrl+C` to exit the Pintos shell.

### Additional references
- [Why are we using docker?](docker_why.md)
- [How did the docker commands work?](https://docs.docker.com/reference/dockerfile/)


## Running Pintos commands
- To run Pintos commands, you need to be in the `pintos/src/threads/build` directory.
- To run pintos commands, add a -- before the command you want to run. For example, to run the help command, you would use:
  ```bash
  pintos -- -h 
  ```
- Anything that comes before the `--` is for the emulator, and anything that comes after is for the Pintos kernel.
- Refer to the [Environment Setup](./assignment_1_environment_setup.md) for more information on Pintos commands.


## Understanding the hierarchy
- We are going to be using three different shells over the course of the assignments.
- The user shell - used to run the `pintos_manager.py` script, or to spawn the docker containers
- The docker shell - used to build pintos 
- The pintos shell - Run inside pintos, which is being emulated by qemu. Refer
to the first assignment to see how to spawn this shell.


## Working with the Source Code
- The code that is build and run by the docker container is located in the `pintos` subvolume/ subfolder of this repo.
- The docker container diretly accesses this folder to build it and run it
- You can use your favourite editor ( not [VSCode](https://code.visualstudio.com/download) ) to edit the sourcecode, and then build it inside the docker shell.
- You need to rebuild pintos using the make command after making any changes to the sourcecode.
