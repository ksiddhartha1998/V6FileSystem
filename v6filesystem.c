//Filename: 	  v6filesystem.c
//Team Members:   Siddhartha Kasturi and Manideep Reddy Miryala
//UTD_ID:         2021471985 and 2021492783
//NetID:          sxk180149 and mxm190033
//Class:          OS 5348.001
//Project:        Project 3

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define BLOCK_SIZE 1024			//Size of each block in v6 filesystem
#define MAX_FILE_SIZE 4194304 	// 4GB of file size
#define FREE_ARRAY_SIZE 124 	// free and inode array size
#define INODE_SIZE 64			// inode size

/*************** super block structure**********************/
typedef struct {
        unsigned int isize; 					//Number of blocks given to i-list
        unsigned int fsize;						// First block not available for alloctaion
        unsigned int nfree;						// Number of Free Blocks available in free list
        unsigned int free[FREE_ARRAY_SIZE];		// list of block which are free
        unsigned int ninode;					//Number of free inodes
        unsigned int inode[FREE_ARRAY_SIZE];	// inode list which contains free inode blocks
        unsigned short flock;					//Flags in the core copy of the file system
        unsigned short ilock;
        unsigned int fmod;						//Flag to indicate that super block is changed and should be copied
        unsigned int time[2];					//Specifies the last time when the file system was changed
} super_block;

/****************inode structure ************************/
typedef struct {
        unsigned short flags; 	//Its is used to represent the type of file and its read/write permission
        unsigned short nlinks;  //Number of links to file
        unsigned short uid;		//User ID of owner
        unsigned short gid;		//Group ID of owner
        unsigned int size; 		//Size of the File
        unsigned int addr[11];  //Data Blocks Associated with file
        unsigned int actime;	//Time of Last Access
        unsigned int modtime;	//Time of last modification
} Inode;

typedef struct
{
        unsigned int inode;		//inode associated with file
        char filename[12];		//Name of the file as string
}dEntry;

super_block super;				//Declaring Super Block 
int fd;							//file descriptor 
char pwd[100];					//present working directory
unsigned int curINodeNumber;	// Current directory inode
char fileSystemPath[100];		//path for the v6 file system


void writeToBlock (unsigned int, void *, unsigned int);							// This Function is used to write data to a block in filesystem
void writeToBlockOffset(unsigned int , unsigned int , void * , unsigned int);	// This Function is used to write data to a block in filesystem with offset
void readFromBlockOffset(unsigned int, unsigned int , void * , unsigned int);	// This Function is used to read data from a block in filesystem with offset
void addFreeBlock(unsigned int);	// This Function is used to add free block to the filesystem
unsigned int getFreeBlock();	// This Function is used to get free block from the filesystem
void addFreeInode(unsigned int);	// This Function is used to add free inodes to the filesystem
unsigned int getFreeInode();	// This Function is used to get free inode from the filesystem
void writeInode(unsigned int, Inode);	// This Function is used to write file description to a inode in the filesystem
Inode getInode(unsigned int);	// This Function is used to get a specific inode from the filesystem
void initfs(char*, unsigned int, unsigned int);	// This Function is to intialize V6 Filesystem
void createRootDirectory();	// This Function will create a root directory during initalization
void copyIn(char* , char*);	// This Function is used to copy data from a external file to V6 Filesystem UPTO (4 GB)
void copyOut(char*,char*);	// This Function is used to copy data from a V6 file to external file
void makedir(char*);	// This Function is used to create a new directory
void rm(char*);	// This Function is used to remove a file in V6 Filesystem
void ls();	// This Function is used to list all the files and directories in current working directory
void p_working_directory();	// This Function is used display present working directory
void changedir(char*);	// This Function is used to change current working directory
void removedir(char*);	// This Function is used to remove directory in filesystem
void file_open(char*);	// This Function is used to open an other V6 File
void quit();	// This Function is used to	quit and save all the changes

void writeToBlock (unsigned int blockNumber, void * buffer, unsigned int nbytes)		
{
        lseek(fd,BLOCK_SIZE * blockNumber, SEEK_SET);
        write(fd,buffer,nbytes);
}


void writeToBlockOffset(unsigned int blockNumber, unsigned int offset, void * buffer, unsigned int nbytes)
{
        lseek(fd,(BLOCK_SIZE * blockNumber) + offset, SEEK_SET);
        write(fd,buffer,nbytes);
}

void readFromBlockOffset(unsigned int blockNumber, unsigned int offset, void * buffer, unsigned int nbytes)
{
        lseek(fd,(BLOCK_SIZE * blockNumber) + offset, SEEK_SET);
        read(fd,buffer,nbytes);
}
	
void addFreeBlock(unsigned int blockNumber)
{
		unsigned int i;
        if(super.nfree == FREE_ARRAY_SIZE)			//When free array is full we are cloning the list into incoming free block and stroring it in free[0]
        {
        	unsigned int buffer[BLOCK_SIZE/4]={0};
            for(i=0;i<FREE_ARRAY_SIZE;i++)
		       	buffer[i]=super.free[i];
            lseek(fd,(BLOCK_SIZE * blockNumber), SEEK_SET);
            writeToBlock(blockNumber, buffer, BLOCK_SIZE);
            super.nfree = 0;
            for(i=0;i<FREE_ARRAY_SIZE;i++)
                super.free[i]=0;
		}
        super.free[super.nfree] = blockNumber;
        super.nfree++;
}
unsigned int getFreeBlock()
{
        if(super.nfree == 0)		// When free list is left with only one block we extract the list stored in the free[0] and replenish the free list 
		{
                unsigned int blockNumber = super.free[0];
                lseek(fd,BLOCK_SIZE * blockNumber , SEEK_SET); 
                read(fd,super.free, FREE_ARRAY_SIZE * 4);
                super.nfree = FREE_ARRAY_SIZE;
                return blockNumber;
        }
        super.nfree--;
        if(super.nfree==0)
        {
        	return getFreeBlock();
		}
		else
		{	
        return super.free[super.nfree];
    	}
}

void addFreeInode(unsigned int iNumber)
{
        if(super.ninode == FREE_ARRAY_SIZE)
            return;
        super.inode[super.ninode] = iNumber;
        super.ninode++;
}

unsigned int getFreeInode()
{
        super.ninode--;
        return super.inode[super.ninode];
}

void writeInode(unsigned int INumber, Inode inode)
{
        unsigned int blockNumber = (INumber * (INODE_SIZE / BLOCK_SIZE))+2;
        unsigned int offset = (INumber * INODE_SIZE) % BLOCK_SIZE;
        writeToBlockOffset(blockNumber, offset, &inode, sizeof(Inode));
}

Inode getInode(unsigned int INumber)
{
        Inode iNode;
        unsigned int blockNumber = (INumber * (INODE_SIZE / BLOCK_SIZE))+2;
        unsigned int offset = (INumber * INODE_SIZE) % BLOCK_SIZE;
        lseek(fd,(BLOCK_SIZE * blockNumber) + offset, SEEK_SET);
        read(fd,&iNode,INODE_SIZE);
        return iNode;
}

void initfs(char* path, unsigned int total_blocks, unsigned int total_inodes)
{
    printf("\n Filesystem Intialization Started \n");
    char emptyBlock[BLOCK_SIZE] = {0};
    unsigned int no_of_bytes,i,blockNumber,iNumber;
    if(((total_inodes*INODE_SIZE)%BLOCK_SIZE) == 0) 			// In this conditional statement we are performing ceil to inodes_per_block and storing it as isize in superblock
        super.isize = (total_inodes*INODE_SIZE)/BLOCK_SIZE;	
    else
        super.isize = (total_inodes*INODE_SIZE)/BLOCK_SIZE+1;
    super.fsize = total_blocks;		//File size is assigned to number of blocks in the file
    if((fd = open(path,O_RDWR|O_CREAT,0600))== -1)		 // opening the given file path and storing the starting address in fileDescriptor pointer
    {
        printf("\n File Opening Error [%s]\n",strerror(errno));
        return;
    }
    strcpy(fileSystemPath,path);
    writeToBlock(total_blocks-1,emptyBlock,BLOCK_SIZE); // writing empty block to last block to fix the size of the V6 file
    super.nfree = 0;
    for (blockNumber= 2+super.isize; blockNumber< total_blocks; blockNumber++)	// adding all blocks to the free list
        addFreeBlock(blockNumber);
    super.ninode = 0;
    for (iNumber=1; iNumber < total_inodes ; iNumber++)		// add free Inodes to inode array
        addFreeInode(iNumber);
    super.flock = 'f';
    super.ilock = 'i';
    super.fmod = 'f';
    super.time[0] = 0;
    super.time[1] = 0;
    writeToBlock (1,&super,BLOCK_SIZE);		//write Super Block to the disk
    for (i=0; i < super.isize; i++)			//allocate empty space for i-nodes
        writeToBlock(i+2,emptyBlock,BLOCK_SIZE);
    createRootDirectory();
}

void createRootDirectory()
{
    unsigned int blockNumber = getFreeBlock();
    dEntry directory[2];
    directory[0].inode = 0;
    strcpy(directory[0].filename,".");
    directory[1].inode = 0;
    strcpy(directory[1].filename,"..");
    writeToBlock(blockNumber, directory, 2*sizeof(dEntry));
    Inode root;
    root.flags = 1<<14 | 1<<15; // This indicate file is allocated and it is a directory
    root.nlinks = 1;
    root.uid = 0;
    root.gid = 0;
    root.size = 2*sizeof(dEntry);
    root.addr[0] = blockNumber;
    root.actime = time(NULL);
    root.modtime = time(NULL);
    writeInode(0,root);
    curINodeNumber = 0;
    strcpy(pwd,"/");
}

void copyIn(char* sourcePath, char* filename)
{
    unsigned int source,blockNumber;
    if((source = open(sourcePath,O_RDWR|O_CREAT,0600))== -1)
    {
        printf("\n File Opening Error [%s]\n",strerror(errno));
        return;
    }
    unsigned int iNumber = getFreeInode();
    Inode file;
    file.flags = 1<<15; // Indicates inode is allocated
    file.nlinks = 1;
    file.uid = 0;
    file.gid = 0;
    file.size = 0;
    file.actime = time(NULL);
    file.modtime = time(NULL);
    unsigned int bytesRead = BLOCK_SIZE;
    char buffer[BLOCK_SIZE] = {0};
    unsigned int i = 0;
	unsigned int k,l,m,temp1,temp2,temp3;
    while(bytesRead == BLOCK_SIZE)
	{
		if(i!=10)			//Small File upto 10 KB
		{
            bytesRead = read(source,buffer,BLOCK_SIZE);
            file.size += bytesRead;
            blockNumber = getFreeBlock();
			file.addr[i++] = blockNumber;
            writeToBlock(blockNumber, buffer, bytesRead);
		}
		else			//Large File upto 4 GB
		{
			blockNumber = getFreeBlock();
			file.addr[i] = blockNumber;
			file.flags = 1<<15 | 1<<12;		//specifying that this file is a large file
			for(k=0;k<BLOCK_SIZE/4;k++)		//Triple Indirect Block 
			{
				temp1=getFreeBlock();
				writeToBlockOffset(blockNumber,k*4,&temp1,4);
				for(l=0;l<BLOCK_SIZE/4;l++)
				{
					temp2=getFreeBlock();
					writeToBlockOffset(temp1,l*4,&temp2,4);
					for(m=0;m<BLOCK_SIZE/4;m++)
					{
						if(bytesRead == BLOCK_SIZE)
						{
						temp3=getFreeBlock();
						writeToBlockOffset(temp2,m*4,&temp3,4);
						bytesRead = read(source,buffer,BLOCK_SIZE);
                				file.size += bytesRead;
						writeToBlock(temp3, buffer, bytesRead);
						}
						else
						{
							k=BLOCK_SIZE/4;
							l=BLOCK_SIZE/4;
							m=BLOCK_SIZE/4;
						}
					}
				}	
			}	
		}
    }
    writeInode(iNumber,file);		// Writing the stored file's inode to the disk
    Inode curINode = getInode(curINodeNumber);	// Updating the parent directory
    blockNumber = curINode.addr[0];
    dEntry newFile;
    newFile.inode = iNumber ;
    strcpy(newFile.filename,filename);
    writeToBlockOffset(blockNumber,curINode.size,&newFile,sizeof(dEntry));
    curINode.size += sizeof(dEntry);
    writeInode(curINodeNumber,curINode);
}

void copyOut(char* destinationPath,char* filename)
{
    unsigned int dest,blockNumber,x=0,i,flag=1,k,l,m;
    char buffer[BLOCK_SIZE] = {0};
    if((dest = open(filename,O_RDWR|O_CREAT,0600))== -1)
    {
        printf("\n File Opening Error [%s]\n",strerror(errno));
        return;
    }
    Inode curINode = getInode(curINodeNumber);
    blockNumber = curINode.addr[0];
    dEntry directory[100];
    readFromBlockOffset(blockNumber,0,directory,curINode.size);
    for(i = 0; i < curINode.size/sizeof(dEntry); i++)
    {
        if(strcmp(destinationPath,directory[i].filename)==0)
		{
            Inode file = getInode(directory[i].inode);
			unsigned int* source = file.addr;
            if(file.flags ==(1<<15)||file.flags ==(1<<15 | 1<<12))
			{
                while(flag)
				{
					if(x!=10)
					{
						blockNumber = source[x];
						if(x==file.size/BLOCK_SIZE)
                            {
                            	readFromBlockOffset(blockNumber, 0, buffer, file.size%BLOCK_SIZE);
                            	write(dest,buffer,file.size%BLOCK_SIZE);
                            	flag=0;
                            }
                            else
                            {
                                readFromBlockOffset(blockNumber, 0, buffer, BLOCK_SIZE);
                                write(dest,buffer,BLOCK_SIZE);
							}
							x++;
					}
					else
					{
						unsigned int b1[BLOCK_SIZE/4]={0};
						readFromBlockOffset(source[10], 0, b1, BLOCK_SIZE);
						for(k=0;k<BLOCK_SIZE/4;k++)
						{
							unsigned int b2[BLOCK_SIZE/4]={0};
							readFromBlockOffset(b1[k], 0, b2, BLOCK_SIZE);
							for(l=0;l<BLOCK_SIZE/4;l++)
							{
								unsigned int b3[BLOCK_SIZE/4]={0};
								readFromBlockOffset(b2[l], 0, b3, BLOCK_SIZE);
								for(m=0;m<BLOCK_SIZE/4;m++)
								{
									blockNumber = b3[m];
									if(x==file.size/BLOCK_SIZE)
		                            {
		                                readFromBlockOffset(blockNumber, 0, buffer, file.size%BLOCK_SIZE);
		                                write(dest,buffer,file.size%BLOCK_SIZE);
		                                flag=0;
		                                k=BLOCK_SIZE/4;
										l=BLOCK_SIZE/4;
										m=BLOCK_SIZE/4;
		                            }
		                            else
		                            {
		                                readFromBlockOffset(blockNumber, 0, buffer, BLOCK_SIZE);
		                                write(dest,buffer,BLOCK_SIZE);
									}
									x++;
								}
							}	
						}	
					}
				}
  				printf("File Copied Out\n");
            }
            else
			{
                printf("\n%s\n","NOT A FILE!");
            }
            return;
        }
    }
}

void makedir(char* dirName)
{
        unsigned int blockNumber = getFreeBlock(); // block to store directory table
        unsigned int iNumber = getFreeInode(); // inode number for new directory
        dEntry directory[2];
        directory[0].inode = iNumber;
        strcpy(directory[0].filename,".");
        directory[1].inode = curINodeNumber;
        strcpy(directory[1].filename,"..");
        writeToBlock(blockNumber, directory, 2*sizeof(dEntry));		// write directory i node to the given block
        Inode dir;
        dir.flags = 1<<14 | 1<<15; // This indicate file is allocated and it is a directory
        dir.nlinks = 1;
        dir.uid = 0;
        dir.gid = 0;
        dir.size = 2*sizeof(dEntry);
        dir.addr[0] = blockNumber;
        dir.actime = time(NULL);
        dir.modtime = time(NULL);
        writeInode(iNumber,dir);
        Inode curINode = getInode(curINodeNumber);		// updating the parent directory and inode
        blockNumber = curINode.addr[0];
        dEntry newDir;
        newDir.inode = iNumber ;
        strcpy(newDir.filename,dirName);
        unsigned int i;
        writeToBlockOffset(blockNumber,curINode.size,&newDir,sizeof(dEntry));
        curINode.size += sizeof(dEntry);
        writeInode(curINodeNumber,curINode);
}

void rm(char* filename)
{
    unsigned int blockNumber,x=0,i,flag=1,k,l,m;
    Inode curINode = getInode(curINodeNumber);
    blockNumber = curINode.addr[0];
    dEntry directory[100];
    readFromBlockOffset(blockNumber,0,directory,curINode.size);
    for(i = 0; i < curINode.size/sizeof(dEntry); i++)
    {
        if(strcmp(filename,directory[i].filename)==0)	// Checking for the specific file in the directory
		{
            Inode file = getInode(directory[i].inode);
            if(file.flags ==(1<<15)||file.flags ==(1<<15 | 1<<12))
			{
                unsigned int* source = file.addr;
                while(flag)
				{
					if(x!=10)
					{
						blockNumber = source[x];
						if(x==file.size/BLOCK_SIZE)
                            flag=0;
						addFreeBlock(blockNumber);
						x++;
					}
					else
					{
						unsigned int b1[BLOCK_SIZE/4]={0};
						readFromBlockOffset(source[10], 0, b1, BLOCK_SIZE);
						for(k=0;k<BLOCK_SIZE/4 && flag;k++)
						{
							unsigned int b2[BLOCK_SIZE/4]={0};
							readFromBlockOffset(b1[k], 0, b2, BLOCK_SIZE);
							for(l=0;l<BLOCK_SIZE/4 && flag;l++)
							{
								unsigned int b3[BLOCK_SIZE/4]={0};
								readFromBlockOffset(b2[l], 0, b3, BLOCK_SIZE);
								for(m=0;m<BLOCK_SIZE/4 && flag;m++)
								{
									blockNumber = b3[m];
								    if(x==file.size/BLOCK_SIZE)
		                                flag=0;
		                            addFreeBlock(blockNumber);
		                            x++;	
								}
								addFreeBlock(b2[l]);
							}
							addFreeBlock(b1[k]);							
						}
						addFreeBlock(source[10]);		
					}
				}
				addFreeInode(directory[i].inode);			// updating the parent directory
                directory[i]=directory[(curINode.size/sizeof(dEntry))-1];
                curINode.size-=sizeof(dEntry);
                writeToBlock(curINode.addr[0],directory,curINode.size);
                writeInode(curINodeNumber,curINode);
                printf("File Removed\n");
            }
            else
			{
                printf("\n%s\n","NOT A FILE!");
            }
            return;
        }
    }
}

void ls()
{                                                             
        Inode curINode = getInode(curINodeNumber);		// Extracting inode of the current working directory
        unsigned int blockNumber = curINode.addr[0];
        dEntry directory[100];
        unsigned int i;
        readFromBlockOffset(blockNumber,0,directory,curINode.size);
        for(i = 0; i < curINode.size/sizeof(dEntry); i++)		// printing all the files and directories in the current working directory
        {
            printf("%s\n",directory[i].filename);
        }
}

void p_working_directory()
{
	printf("Path of Current Working Directory %s\n",pwd);
}

void changedir(char* dirName)
{
    Inode curINode = getInode(curINodeNumber);			// Extracting inode of the current working directory
    unsigned int blockNumber = curINode.addr[0];
    dEntry directory[100];
    unsigned int i;
    readFromBlockOffset(blockNumber,0,directory,curINode.size);
    for(i = 0; i < curINode.size/sizeof(dEntry); i++)
    {
        if(strcmp(dirName,directory[i].filename)==0)	// checking for the given directory
		{
            Inode dir = getInode(directory[i].inode);
            if(dir.flags ==( 1<<14 | 1<<15))			// checking given filename is a directory or not
			{
                curINodeNumber=directory[i].inode;
                if(strcmp(dirName,"..")==0)				//storing parents inode in the child directory for going back to parent
                {
                    int j=strlen(pwd)-1;
                    char ch;
                    while((ch=pwd[j])!='/')
                    j--;
                    pwd[j]='\0';
				}
				else if(strcmp(dirName,".")==0);
				else
				{
                   strcat(pwd,"/");
                    strcat(pwd,dirName);	//updating the present working directory 
                }
                printf("Directory Changed\n");
            }
            else
			{
                printf("\n%s\n","NOT A DIRECTORY!");
            }
            return;
        }
    }
}

void removedir(char* filename)
{
    unsigned int blockNumber,x,i;
    Inode curINode = getInode(curINodeNumber);			// Extracting inode of the current working directory
    blockNumber = curINode.addr[0];
    dEntry directory[100];
    readFromBlockOffset(blockNumber,0,directory,curINode.size);
    for(i = 0; i < curINode.size/sizeof(dEntry); i++)
    {
        if(strcmp(filename,directory[i].filename)==0)
		{
            Inode dir = getInode(directory[i].inode);	// checking for the given directory
            if(dir.flags ==(1<<14 | 1<<15))
			{
                if(dir.size>2*sizeof(dEntry))
                {
                    printf("Directory is not Empty cannot be Removed");
				}
				else
				{
                    addFreeInode(directory[i].inode);		//freeing the inode
                    directory[i]=directory[(curINode.size/sizeof(dEntry))-1];		//updating the parent directory
                    curINode.size-=sizeof(dEntry);
                    writeToBlock(curINode.addr[0],directory,curINode.size);
                    writeInode(curINodeNumber,curINode);
                    printf("Direcotry Removed\n");
                }
            }
            else
			{
                printf("\n%s\n","NOT A Directory!");
            }
            return;
        }
    }
}

void file_open(char* source_path)
{
	writeToBlock (1,&super,BLOCK_SIZE);		//write Super Block to the disk
	if(access(source_path, X_OK) != -1)
    {
        fd = open(source_path,O_RDWR);		// opening the given file path and storing the starting address in fileDescriptor pointer
        lseek(fd,BLOCK_SIZE, SEEK_SET);
        read(fd,&super,BLOCK_SIZE);			//updating the super block
        strcpy(fileSystemPath,source_path);
    }
    else
    {
    	printf("Invalid File\n");
	}
}

void quit()
{
	writeToBlock (1,&super,BLOCK_SIZE);		//write Super Block to the disk
    close(fd);
    exit(0);
}

int main(int argc, char *argv[])
{
    char c;
    printf("\n Clearing screen \n");
    system("clear");
    unsigned int blk_no =0, inode_no=0;
    char *fs_path;
    char *arg1, *arg2;
    char *my_argv, cmd[256];
    while(1)
    {
        printf("\n%s@%s>>>",fileSystemPath,pwd);
        scanf(" %[^\n]s", cmd);
        my_argv = strtok(cmd," ");
        if(strcmp(my_argv, "initfs")==0)
        {
            fs_path = strtok(NULL, " ");
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, " ");
            if(access(fs_path, X_OK) != -1)
            {
                printf("Filesystem Already Exists. \n");
                printf("Same File System will be used\n");
                fd = open(fs_path,O_RDWR);
                lseek(fd,BLOCK_SIZE, SEEK_SET);
        		read(fd,&super,BLOCK_SIZE);
                strcpy(fileSystemPath,fs_path);
            }
            else
            {
                if (!arg1 || !arg2)
                    printf(" Insufficient Arguments to Proceed\n");
                else
            	{
                    blk_no = atoi(arg1);
                    inode_no = atoi(arg2);
                    initfs(fs_path,blk_no, inode_no);
                    printf("File System Initialised\n");
                }
            }
            my_argv = NULL;
        }
        else if(strcmp(my_argv, "cpin")==0)
		{
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, " ");
            if (!arg1 || !arg2)
                printf(" Insufficient Arguments to Proceed\n");
            else
            {
                copyIn(arg1,arg2);
                printf("File Copied In\n");
            }
        }
        else if(strcmp(my_argv, "cpout")==0)
		{
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, " ");
            if (!arg1 || !arg2)
                printf(" Insufficient Arguments to Proceed\n");
            else
                copyOut(arg1,arg2);
        }
        else if(strcmp(my_argv, "mkdir")==0)
		{
            arg1 = strtok(NULL, " ");
            if (!arg1)
            	printf(" Insufficient Arguments to Proceed\n");
            else
            	makedir(arg1);
            printf("Directory Created\n");
        }
        else if(strcmp(my_argv, "rm")==0)
		{
            arg1 = strtok(NULL, " ");
            if (!arg1)
                printf(" Insufficient Arguments to Proceed\n");
            else
                rm(arg1);
        }
        else if(strcmp(my_argv, "ls")==0)
		{
            ls();
        }
        else if(strcmp(my_argv, "pwd")==0)
		{
        	p_working_directory();
        }
        else if(strcmp(my_argv, "cd")==0)
		{
            arg1 = strtok(NULL, " ");
            if (!arg1)
            	printf(" Insufficient Arguments to Proceed\n");
            else
                changedir(arg1);
        }
        else if(strcmp(my_argv, "rmdir")==0)
		{
            arg1 = strtok(NULL, " ");
            if (!arg1)
                printf(" Insufficient Arguments to Proceed\n");
            else
                removedir(arg1);
        }
        else if(strcmp(my_argv, "open")==0)
		{
            arg1 = strtok(NULL, " ");
            if (!arg1)
                printf(" Insufficient Arguments to Proceed\n");
            file_open(arg1);    
        }
        else if(strcmp(my_argv, "q")==0)
		{
            quit();
        }
        else
        {
            printf("Invalid Command\n");
		}
    }
    return 0;
}
 
