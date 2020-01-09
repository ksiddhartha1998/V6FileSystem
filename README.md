# V6 FileSystem

We've implemeted V6 Filesytem in Linux environment which can store files with size upto __4GB__. V6 file system is highly restrictive. A modification has been done: Block size is 1024 Bytes, i-node size is 64 Bytes and i-node’s structure has been modified as well.

## Getting Started

The following functionalities are given to the file system :-

* __initfs fname n1 n2__ :-
fname is the name of the (external) file in your Unix machine that represents the V6 file system.
n1 is the number of blocks in the disk (fsize) and n2 is the total number of i-nodes.
This command initializes the file system. All data blocks are in the free list (except for one data blosk that is allocated to the root /. An example is: initfs sid 8000 300.

* __cpin externalfile v6-file__ :-
Creat a new file called v6-file in the v6 file system and fill the contents of the newly created file with the contents of the externalfile.

* __cpout v6-file externalfile__ :-
If the v6-file exists, create externalfile and make the externalfile's contents equal to v6-file.

* __mkdir v6-dir__ :-
If v6-dir does not exist, create the directory and set its first two entries . and ..

* __rm v6-file__ :-
If v6-file exists, delete the file, free the i-node, remove the file name from the (parent) directory that has this file and add all data blocks of this file to the free list.

* __ls__ :-
List the contents of the current directory.

* __pwd__ :-
List the fill pathname of the current directory.

* __cd dirname__ :-
change current (working) directory to the dirname.

* __rmdir dir__ :-
Remove the directory specified (dir, in this case).

* __open filename__ :-
Open the external file filename, which has a v6 filesystem installed.

* __q__ :-
Save all changes and quit.



