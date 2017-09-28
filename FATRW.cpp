#include "jdisk.h"
#include <iostream>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h>
#include <cstring>

#define SEC_SZ  1024

using namespace std;

struct GFAT
{
	//sets up the FAT variables
	GFAT(char * filename, char * jdisk, unsigned int mde);
	~GFAT();
	
	//mode tells it if it is working with a jdisk or regular file
	unsigned int GetFileSize(const char * fileName, int mode);
	
	//Calculates the necessary S value. also sets D
	unsigned short GetS(unsigned int Total_Sectors);
	
	unsigned int FreeCount( void * d, unsigned int lba, unsigned int need);

	//a buffer set to the size of a disk sector
	//used for reading disks a sector at a time
	unsigned char * Jbuff;
	unsigned char * Fbuff;

	unsigned char ** fatbuff;

	unsigned int D,S, fileSize, disksize, last1, last2, olel, oleb;;

	FILE * JP;		//jdisk pointer 
	FILE * FP;		//file pointer

	char * FAT;

	unsigned int * ToChange;

	string jdisk;
	string file;

	//will store a pointer to the jdisk
	void * dp;;

};



//this will see if the given disk d has enough free blocks(need) , the lba is the first link on the 
//free list, as it goes along free list it stores the neccessary parts of the FAT in
//the fat buffer
unsigned int GFAT::FreeCount( void * d, unsigned int lba, unsigned int need)
{

	unsigned int cnt = 0, lil = 0, big= 0;
	//start at fat[0]

	//convert the given link into a sector number
	unsigned int secnum = lba*2/SEC_SZ;

	//get the offset into the found disk section
	unsigned int offset = (lba*2)%SEC_SZ;

	unsigned int next = lba;

	while(next != 0 && cnt < need)
	{

		//mark the disk next to be loaded into
		ToChange[cnt] = next;

		if(secnum > S) 
		{
			printf("secnum is too big %u\n",secnum);
			exit(0);
		}

		//check the fatbuffer for the fat section secnum
		//if not loaded load it, otherwise
		//use the fatbuff to get a pointer to the fat section
		if(fatbuff[secnum] == NULL)
		{

			Jbuff = (unsigned char *)malloc(sizeof(unsigned char)*SEC_SZ);
			
			jdisk_read(dp,(secnum),(void *)Jbuff);

			fatbuff[secnum] = Jbuff;
		}
		else 
		{
			Jbuff = fatbuff[secnum];
		}

		//save the ole lil and big
		olel = lil;
		oleb = big;

		//calculate the new lil and big from secnum section of fat at offset
		lil = (unsigned int)Jbuff[offset];
		
		big = (unsigned int)Jbuff[offset + 1];
		
		last1 = Jbuff[offset];
		
		last2 = Jbuff[offset + 1];
		
		next = big*256 + lil;

		secnum = next*2/SEC_SZ;
		offset = (next*2)%SEC_SZ;

		cnt++;
	}
	return cnt;
}


//creates a GFAT using the given file and jdisk names and perfoms the 
//action given on the command line
//import if mode == 0, export if mode is some start block
GFAT::GFAT(char * fileName, char * JDisk, unsigned int mode)
{
	unsigned int total = 0, i=0, tbytes = 0;

	//get a pointer to this jdisk
	dp = jdisk_attach(JDisk);

	//get the size of the jdisk
	//and calculate how many sectors are available on the disk
	disksize = jdisk_size(dp);
	total = disksize/SEC_SZ;
	
	//set D, and S using the GetS function and the total disk size
	GetS(total);

	//set up the buffers
	Jbuff = (unsigned char *)malloc(sizeof(unsigned char)*SEC_SZ);
	Fbuff = (unsigned char *)malloc(sizeof(unsigned char)*SEC_SZ);
	
	//make fatbuff used to hold pointers to sections of the FAT
	//if that secions is not loaded yet it's value will be NULL
	fatbuff = (unsigned char **)malloc(sizeof(char*)*S);


	for(unsigned int k = 0; k < S; k++)
	{
		fatbuff[k] = NULL;
	}

	//if mode ==0 the command is an import command
	if(mode == 0) 
	{
		//get the size of the file and the jdisk
		//alos set a pointer to the file called FP
		fileSize = GetFileSize(fileName,0);


		if(fileSize > disksize) 
		{
			fprintf(stderr,"this file is too damn big\n");
			exit(0);
		}
	
		//check if there is enough space
		unsigned int needed = (fileSize + SEC_SZ-1)/SEC_SZ;
		if(needed == 0) needed = 1;

		if(needed > total) 
		{
			fprintf(stderr,"There is not enough space on disk %s\n",JDisk);
			exit(0);
		}

		//used to remember which links change, 
		ToChange = (unsigned int*)malloc(sizeof(unsigned int)*needed);

		//check for proper formating of jdisk
		if( disksize % SEC_SZ == 0 ) ;//printf("the size of the jdisk is %u\n", disksize);
		else 
		{
			printf("Issue with jdisk format need to format\n");
			exit(0);
		}

		//grab the first FAT sector 
		//to look at the first element on the free list
		jdisk_read(dp,0,(void *)Jbuff);

		//grab first link on free list 
		//as two bytes
		unsigned int ll = Jbuff[0];
		unsigned int bb = Jbuff[1];

		//convert it to an actual int
		unsigned int lba = bb*256 + ll;

		unsigned int secnum = lba*2/SEC_SZ;

		unsigned int offset = (lba*2)%SEC_SZ;

		if(lba == 0)
		{
			printf("There are not enough free disks\n");
			printf("needed %u, have %u\n",needed,0);
			exit(0);
			
		}


		fatbuff[0] = Jbuff;

		//see how many free disks there are
		unsigned int have = FreeCount(dp,lba,needed);


		if(have < needed)
		{
			fprintf(stderr,"There are not enough free disks\n");
			exit(0);
		}

		if(have == 1) 
		{
			olel = ll;
			oleb = bb;

		}

		unsigned int idx, disknumber, secn, numrd = 0;;

		int arr[S];


		for(unsigned int k = 0; k < S; k++)
		{
			arr[k] = -1;
		}

		for(idx = 0; idx < needed; idx++)
		{
			//actually a fat number
			disknumber = ToChange[idx];
		
			//find the actual disk sector
			secn = S + disknumber - 1;

			//read a disk sized section from the file
			//into the fbuffer
			numrd = fread(Fbuff,1,SEC_SZ,FP);

			//if its the last load check how much of the section is taken up
			if(idx == needed -1 ) 
			{
				//grab the last link to put in 00

				unsigned int ui = disknumber * 2/SEC_SZ;

				unsigned int oui = (disknumber %(SEC_SZ/2))*2;


				if(fatbuff[ui] == NULL)
				{
					printf("ui in fat load %u\n",ui);
					Jbuff = (unsigned char *)malloc(sizeof(char)*SEC_SZ);
					jdisk_read(dp,ui,(void *)Jbuff);

					fatbuff[ui] =  Jbuff;
				}

				//look inside fat for next link for this last one
				Jbuff = fatbuff[ui];

				arr[ui] = 1;

				unsigned int l = Jbuff[oui];
				unsigned int b = Jbuff[oui + 1];

				unsigned int rm = b*256 + l;

				Jbuff = fatbuff[0];

				arr[0] = 1;
				Jbuff[0] = l;
				Jbuff[1] = b;


				if(numrd < SEC_SZ) 
				{
					if(numrd == SEC_SZ-1) Fbuff[SEC_SZ-1] = 0xff;
					else
					{

						((unsigned short *) Fbuff)[(SEC_SZ-2)/2] = (unsigned short)numrd;

					}

					//write the chunk of file to disk secn
					jdisk_write(dp,secn,(void *)Fbuff);

					Fbuff = fatbuff[ui];
					arr[ui] = 1;
					((unsigned short *)Fbuff)[oui/2] = (unsigned short)disknumber;
					//jdisk_write(dp,ui,(void *)Fbuff);
					//
					for(unsigned int p = 0; p < S;p++)
					{
						if(arr[p] == 1)
						{

							jdisk_write(dp,p,(void *)fatbuff[p]);
						}
					}
				
				
				}
				else if(numrd == SEC_SZ) 
				{

					//write the chunk of file to disk secn
					jdisk_write(dp,secn,(void *)Fbuff);

					arr[ui] = 1;
					Fbuff = fatbuff[ui];
					Fbuff[oui] = 0;
					Fbuff[oui+1] = 0;

					//write adjusted fat
					//jdisk_write(dp,ui,(void *)Fbuff);
					for(unsigned int p = 0; p < S;p++)
					{
						if(arr[p] == 1)
						{

							jdisk_write(dp,p,(void *)fatbuff[p]);
						}
					}

				}

		
			}
			else
			{

				//write a chunk of file to the disk secn
				jdisk_write(dp,secn,Fbuff);

			}

		}


		printf("New file starts at %u\n",(S+lba-1) );


	}
	else
	{
		//create and open oput file for writing
		FP = fopen(fileName,"w");

		if(FP == NULL) 
		{
			fprintf(stderr,"issue opening file %s\n", fileName);
			exit(0);
		}

		unsigned int oldl, newl = 0, sec, lil, big,sn, ofst;

		oldl = -1;
		newl = mode;


		while(oldl != newl && newl != S-1)
		{

			//read the file from the jdisk into the buffer
			jdisk_read(dp,newl,(void *)Fbuff);


			//save the old disc section
			oldl = newl;

			//figure out the fat link number
			sec = oldl - S + 1;

			//figure out the disk number and offset into that disk
			sn = sec*2/SEC_SZ;
			ofst = (sec%(SEC_SZ/2))*2;


			//if you dont have that part of the fat loadeded do so
			if(fatbuff[sn] == NULL)
			{
				Jbuff = (unsigned char *)malloc(sizeof(unsigned char)*SEC_SZ);
				jdisk_read(dp,sn,(void *)Jbuff);

				fatbuff[sn] = Jbuff;
			}
			else 
			{
				Jbuff = fatbuff[sn];

			}

			lil = Jbuff[ofst];
			big = Jbuff[ofst+1];


			//calculate the new link from the value in this part of the FAT
			newl = S + (big*256 + lil) - 1;

			//if the new link is not the  old link
			//or if this is the last part of the file
			if( newl != oldl || newl == S-1)
			{
				fwrite(Fbuff,1,SEC_SZ,FP);
				
			}
			else
			{


				if(Fbuff[SEC_SZ-1] == 0xff)
				{

					fwrite(Fbuff,1,SEC_SZ-1,FP);
				}
				//if the last part of the file is not 0xff
				else
				{

					unsigned int amount = Fbuff[SEC_SZ-2] + Fbuff[SEC_SZ-1]*256;
					fwrite(Fbuff,1,amount,FP);
					
				}
		
			}

		}

	} 


	fclose(FP);

	printf("Reads: %ld\n",(long int)jdisk_reads(dp));
	printf("Writes: %ld\n",(long int)jdisk_writes(dp));


}


GFAT::~GFAT()
{
	for(unsigned int i = 0;i < S ; i++)
	{
		if(fatbuff[i] != NULL) free(fatbuff[i]);
	}

	free(Jbuff);
	free(Fbuff);
}


unsigned short GFAT::GetS(unsigned int total)
{
	unsigned int rs = 1, d = 0, shortsNded = 0, currentBytes = 0, N = 0;

	while(1)
	{

		d = (total - rs);

		shortsNded = ((d+1))*2;

		currentBytes = rs*SEC_SZ;

		//as soon as we have enough space break
		if(rs*SEC_SZ >= shortsNded)
		{

			break;

		}
		else rs++;

	}

	//SET d AND s IN THE FAT 
	D = (total - rs);
	S = rs;
	
	return rs; 
}

//used to calculate file size in bytes
//unsigned int GetFileSize(int fd)
//mode: =0,file, =1,jdisk
unsigned int GFAT::GetFileSize(const char * fileName, int mode)
{

	FILE * fp;

	fp = fopen(fileName,"r");

	//Store a pointer to the given file(1 = Jdisk, 0 = file)
	if(mode) JP = fp;
	else FP = fp;

	if(fp == NULL) 
	{
		printf("error opening file %s\n", fileName);
		exit(0);
	}

	//calculate the file size of given file
	fseek (fp, 0 , SEEK_END);
	unsigned int size = ftell (fp);
	rewind (fp);

	return size;	
}

int main (int argc, char ** argv)
{

	unsigned int size = 0,total = 0;;
	unsigned int i = 0;
	FILE * output;
	FILE * outfile;

	string command, myfile = " myfile.txt", report, create, attach,reads,writes,fat;
	string jdisk(argv[1]);
	string file;
	string space, format;
	
	if( (argc != 4 && argc != 5 ) || (strcmp(argv[2], "import") != 0 && strcmp(argv[2],"export") != 0) )
	{
		printf("usage: ./a.out JdiskFile import InputFileName\n");
		printf("usage: ./a.out JdiskFile export starting_block OutputFileName\n");
		exit(0);
	}

	if(argc == 4) file = (argv[3]);
	else if(argc == 5) file = (argv[4]);


	GFAT * GF;


	if(argc == 4) 
	{
		GF = new GFAT(argv[3],argv[1],0);
	}
	else if(argc == 5) 
	{
		unsigned int start = (unsigned int) atol(argv[3]);

		fflush(stdout);

		GF = new GFAT(argv[4],argv[1],start);
	}

	return 0;
}



