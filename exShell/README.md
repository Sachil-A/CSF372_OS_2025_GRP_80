# Install and setup Pintos
- Follow [Pintos Setup Guide](../wiki/Pintos.md) to install and set up Pintos on your system.


# Pintos Shell Setup Commands
Build the userprog directory:
`cd /home/me/pintos/src/userprog`
`make`

Build the examples directory:
`cd /home/me/pintos/src/examples`
`make`

Change to userprog/build directory:
`cd /home/me/pintos/src/userprog/build`

Create a filesystem with 5MB size:
`pintos-mkdisk filesys.dsk --filesys-size=5`

Format the filesystem:
`pintos -- -f -q`

Copy cat program to the filesystem:
`pintos -p ../../examples/cat -a cat -- -q`

Copy echo program to the filesystem:
`pintos -p ../../examples/echo -a echo -- -q`

Copy shell program to the filesystem:
`pintos -p ../../examples/shell -a shell -- -q`

Run ls to verify files were copied correctly:
`pintos -- -q ls`

Run the shell program on Pintos:
`pintos -- run shell`

