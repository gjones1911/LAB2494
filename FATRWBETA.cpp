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
	unsigned short GetS(unsigned short Total_Sectors);
	
	//checks how many sections are left on the given disk
	unsigned short FreeSecCnt(FILE * fp);
	
	
	//reads the link and returns the appropriate link number
	unsigned short Read_Link(int link);
	
	unsigned int GETLBA(unsigned int dn, int mode);
	int WriteToDisk(unsigned int &startBlock, unsigned int &numOblocks);

	//
	void Write_Link(int link, unsigned short val);
	
	
	void Flush_Links(); //(checks the flags and calls jdisk_write().

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
	
	void * dp;;

	int filed[250];
	FILE * fp[250];
};

//give this the start block in the fat and the number of blocks needed to write
//will write the currently pointed to file to the given disk, for a total
//number of numOblocks disk writes
int GFAT::WriteToDisk(unsigned int &startBlock, unsigned int &numOblocks)
{

	unsigned int i = 0, disk = GETLBA(startBlock,1), red = 0, nextone = startBlock, lil, big, cnt,lba = 0, fatid = startBlock; 
	unsigned int oldone = 0;

	for(i = 0; i < numOblocks;i++)
	{
		red = fread(Fbuff,1,SEC_SZ,FP);

		printf("I just READ %u bytes from the file\n",red);

		printf("the next disk block is %u\n", nextone);


		//put jdisk pointer to appropriate disk
		fseek(JP,0,disk);


		fwrite(Fbuff,1,SEC_SZ,JP);

		//get next disk

			lil = FAT[nextone];

			if(lil > 0xc0) 
			{
				lil -= 0xffffff00;
			}
			
			big = FAT[nextone+1];
			if(big > 0xc0) 
			{
				big -= 0xffffff00;
			}


			//printf("lil: %x big %x\n",lil,big);

			//convert the hex number to a decimal
			//remeber that the short is stored in lil endian
			//i.e.  lil,big, number is actually big,lil
			//nextone = lil + big*256;
			nextone = big*256 + lil;
			printf("the next one is %u\n", nextone);

			nextone = (nextone)*2;

			printf("actual %u\n",nextone);

			cnt++;
		
		disk = GETLBA(nextone,1);

	}

	//now fix fat to point to the next free block
	fseek(JP,0,SEEK_SET);

	fwrite(FAT+nextone,1,1,JP);
	fwrite(FAT+nextone + 1,1,1,JP);

	fseek(JP,0,SEEK_SET);


	//now do reporting
	//system(reads);
	//system(writes);

	return 0;
}


unsigned int GFAT::FreeCount( void * d, unsigned int lba, unsigned int need)
{

	unsigned int cnt = 0, lil = 0, big= 0;
	//start at fat[0]

	printf("looking at %u first\n",lba);
/*
	jdisk_read(dp,0,(void *)Jbuff);
	
	unsigned int ll = Jbuff[0];
	unsigned int bb = Jbuff[1];

	unsigned int lba = bb*256 + ll;

	printf("one is %x %x\n",(char)ll, (char)bb);
	printf("or as a number: %u \n",lba);
*/
	unsigned int secnum = lba*2/SEC_SZ;

	printf("the 1st sector number is %u\n", secnum);

	unsigned int offset = (lba*2)%SEC_SZ;
	printf("the offset is %u\n", offset);

	unsigned int next = lba;

	printf("-----------------next starts as %u\n",next);

	while(next != 0 && cnt < need)
	{
		ToChange[cnt] = next;
//		printf("next is %u\n",next*2);

		if(secnum > S) 
		{
//			printf("secnum is too big\n");
			exit(0);
		}

		if(fatbuff[secnum] == NULL)
		{
//			printf("have to load sector %u\n",secnum);
			Jbuff = (unsigned char *)malloc(sizeof(unsigned char)*SEC_SZ);
			jdisk_read(dp,(secnum),(void *)Jbuff);

			fatbuff[secnum] = Jbuff;
		}
		else 
		{
//			printf("sector %u is already loaded\n",secnum);
			Jbuff = fatbuff[secnum];
		}

		olel = lil;
		oleb = big;

		lil = (unsigned int)Jbuff[offset];
		
		big = (unsigned int)Jbuff[offset + 1];
		
		last1 = Jbuff[offset];
		
		last2 = Jbuff[offset + 1];
		

//		printf("the link is %x %x\n",big, lil);

		next = big*256 + lil;

//		printf("The next number is %u\n",next);
		secnum = next*2/SEC_SZ;
		offset = (next*2)%SEC_SZ;

//		printf("secnum %u, off %u\n", secnum, offset);

		cnt++;
	}

	if(cnt == need) 
	{
		//last = next;
		printf("-------the old lil and big is %x %x \n",olel,oleb);
		printf("the new last is %x %x \n",last1,last2);
		printf("there are %u blocks free\n",cnt);
	}
	else printf("there are not enough free blocks\n");
	if(cnt < need) return -1;
	else 

	return cnt;
}


//give this the number in the fat non converted(in decimal not hex)
//if mode == 1 converting from FAT to disk number, otherwise
//converting from disk number to FATID
unsigned int GFAT::GETLBA(unsigned int num, int mode)
{

	if(mode)
	{
		printf("returning disk number %u\n", S+num-1);
		printf("or byte %u\n", (S+num-1)*SEC_SZ);
		return (S + num - 1)*SEC_SZ;
	}
	else
	{
		printf("returning FAT number %u\n", num - S +1);
		return num - S + 1;
	}
}
//creates a GFAT using the given file and jdisk names
GFAT::GFAT(char * fileName, char * JDisk, unsigned int mode)
{
	unsigned int total = 0, i=0, tbytes = 0;

	//get a pointer to this jdisk
	dp = jdisk_attach(JDisk);


	if(mode == 0) 
	{
		//get the size of the file and the jdisk
		//alos set a pointer to the file called FP
		fileSize = GetFileSize(fileName,0);

		//FILE * DKP = fopen(JDisk,"r+");

		disksize = jdisk_size(dp);

		//disksize = GetFileSize(JDisk,1);
		//disksize = t * SEC_SZ;

		//check if there is enough space
		unsigned int needed = fileSize/SEC_SZ;
		if(needed == 0) needed = 1;
		unsigned int possible = disksize/SEC_SZ;
		printf("the file needs %u sectors of space\n", needed);
		printf("the disk is of size %u\n", disksize);
		printf("the disk is %u sectors long\n",possible);

		//count free sectors

		unsigned int freenum = 0;

		//printf("the disk has %u sectors left\n",free);

		if(needed > possible) 
		{
			fprintf(stderr,"There is not enough space on disk %s\n",JDisk);
			exit(0);
		}
		printf("the load is possible.,.......\n");


		total = possible;

		ToChange = (unsigned int*)malloc(sizeof(unsigned int)*needed);

		//need to check format of jdisk is correct , it should be some multiple of sec_sz(1024)
		//	size = (unsigned int)GF->GetFileSize(argv[1],1);

		if( disksize % SEC_SZ == 0 ) printf("the size of the jdisk is %u\n", disksize);
		else printf("Issue with jdisk format need to format\n");


		total = disksize/SEC_SZ;


		printf("There are %u sectors on this disk\n", total);

		GetS(total);
		//D = total - S;

		//D = d;
		//S = s;


		//make fatbuff
		fatbuff = (unsigned char **)malloc(sizeof(char*)*S);


		for(unsigned int k = 0; k < S; k++)
		{
			fatbuff[k] = NULL;
		}

		//set up the buffers
		Jbuff = (unsigned char *)malloc(sizeof(unsigned char)*SEC_SZ);
		Fbuff = (unsigned char *)malloc(sizeof(unsigned char)*SEC_SZ);

		//grab the first FAT sector 
		//to look at the first element on the free list
		jdisk_read(dp,0,(void *)Jbuff);

		unsigned int ll = Jbuff[0];
		unsigned int bb = Jbuff[1];

		unsigned int lba = bb*256 + ll;

		printf("one is %x %x\n",(char)ll, (char)bb);
		printf("or as a number: %u \n",lba);

		unsigned int secnum = lba*2/SEC_SZ;

		printf("the sector number is %u\n", secnum);

		unsigned int offset = (lba*2)%SEC_SZ;
		printf("the offset is %u\n", offset);


		fatbuff[0] = Jbuff;

		unsigned int have = FreeCount(dp,lba,needed);

		if(have ==1) 
		{
			olel = ll;
			oleb = bb;
			printf("old iss  %x %x\n",(char)olel, (char)oleb);


		}

		if( needed > have)
		{
			printf("there are not enough free blocks\n");
			printf("needed %u, have %u\n",needed,have);
			exit(0);
		}

		//	exit(0);

		unsigned int idx, disknumber, secn;;


		printf("needed %u, have %u\n",needed,have);

		//if doing an import

		for(idx  =0; idx < needed; idx++)
		{
			disknumber = ToChange[idx];
			printf("the val is %u\n", disknumber);
			secn = S + disknumber - 1;
			printf("secnumber %u\n", secn);

			fread(Fbuff,1,SEC_SZ,FP);


			jdisk_write(dp,secn,Fbuff);

		}

		printf("------the last disk number to be changed is %x\n",disknumber);
		printf("------the last disk number to be changed is %u\n",disknumber);

		secn = disknumber*2/SEC_SZ;
		unsigned int ost = disknumber*2%SEC_SZ;

		printf("---------sec number %u, off set %u\n",secn, ost);

		//go to the last disk and set its link to itself in our copy of the fat
		Jbuff = fatbuff[secn];



		Jbuff[ost] = (unsigned short) disknumber;

		printf("------sec number %u, off set %u\n",secn, ost);
		printf("------it is now %x\n", disknumber);

		jdisk_write(dp,secn,Jbuff);
		//update the FAT
		//
		Jbuff = fatbuff[0];
		printf("last1 %x, last2 %x\n", last1,last2);
		Jbuff[0] = last1;
		Jbuff[1] = last2;

		jdisk_write(dp,0,Jbuff);
		printf("New file starts at %u\n",(S+lba-1) );


	}
	else
	{

	}

	printf("Reads: %ld\n",(long int)jdisk_reads(dp));
	printf("Writes: %ld\n",(long int)jdisk_writes(dp));

	/*
	   tbytes = (D+1)*2;

	   printf("the fat is the first %u bytes of jdisk\n", tbytes);

	   FAT = (char *)malloc(sizeof(char) * tbytes);

	   printf("there are %u sectors on this disk's FAT, and %u data sections\n",S,D);
	   printf("----There are disks from byte %u*DNUM to %u\n",(S*SEC_SZ),(S*SEC_SZ+(SEC_SZ-1)*D));


	   if(needed > 1) printf("The file needs %u sectors\n",needed);
	   else  printf("The file only needs %u sector\n",1);

	//check if there is enough space for the input file


	//now read the FAT into the FAT buffer
	unsigned int igot = fread(FAT, 1, tbytes, JP); 

	printf("Just read %u bytes into FAT\n", igot);


	unsigned int nextone = (unsigned int)FAT[0] + (unsigned int)FAT[1]*256;
	unsigned int cnt = 0, first_open = 0;;


	printf("\n\n-------The first disk on free list is %u\n\n",nextone);

	if(nextone == 0) 
	{
	printf("the jdisk is full\n");
	exit(0);
	}


	nextone = (nextone)*2;

	printf("next starts at %u\n", nextone);
	first_open = nextone;

	unsigned int count = 0;

	//count free sectors
	//while( nextone != 0 && cnt < 10)
	while( nextone != 0 )
	{
	unsigned int lil = FAT[nextone];

	//if(lil > 0xc0) 
	//{
	//	lil -= 0xffffff00;
	//	}
	unsigned int big = FAT[nextone+1];
	//	if(big > 0xc0) 
	//	{
	//		big -= 0xffffff00;
	//	}


	printf("lil: %x big %x\n",lil,big);

	nextone = lil + big*256;
	//printf("the next one is %u\n", nextone);

	nextone = (nextone)*2;

	//printf("actual %u\n",nextone);

	cnt++;
	}

	printf("there are %u disks on the free list\n", cnt);
	printf("or there are at least %u bytes left of space\n", cnt*SEC_SZ);

	if( cnt*SEC_SZ < fileSize)
	{
		printf("There is not enough space on the disk\n");
		printf("file needs %u and disk only has %u\n", fileSize, cnt*SEC_SZ);
		exit(0);
	}
	else printf("the disk has enough space starting at %u\n",first_open);

	//WriteToDisk(first_open,needed);
	*/

		/*

		   for(i= 0; i < tbytes;i +=2)
		   {
		//printf("---->IN hex yo %x %x\n",(long int)Jbuff[nrl], (long long)Jbuff[nrl-1]);
		printf("FAT[%u]:%x, FAT[%u]:%x\n",i,(char)FAT[i],i+1,(char)FAT[i+1]);

		unsigned int lil = 0, big = 0;

		lil = (unsigned int)FAT[i];
		big = (unsigned int)FAT[i+1]*256;

		printf("lil = %u, big = %u\n", lil, big);

		unsigned int next = lil + big;

		printf("-----next is %u\n",next);
		}
		*/


}


GFAT::~GFAT()
{

}


unsigned short GFAT::FreeSecCnt(FILE * fp)
{
	//sector cover sz*sn---->(sn*sz) + (sz-1)
	printf("reading sector 0 into the buffer\n");

	int igot = fread(Jbuff,1,SEC_SZ,fp);

	printf("%d bytes were read\n",igot);

	//now read the first element in the FAT
	printf("showing 2301 due to little endian\n");
	//	printf("%d %d %d %d is the first data sector on the free list\n",(int)Jbuff[2], (int)Jbuff[3],(int)Jbuff[0], (int)Jbuff[1]);
	//	printf("%c %c%c %c is the first data sector on the free list\n",Jbuff[2], Jbuff[3],Jbuff[0], Jbuff[1]);


	unsigned int num = (unsigned int)Jbuff[1]+(unsigned int)Jbuff[0]*10+(unsigned int)Jbuff[3]*100+(unsigned int)Jbuff[2]*1000;

	printf("the link is %u\n", num);

	unsigned int link = (S + num - 1);
	printf("Need to go to %u\n", link);


	return 0;

}
unsigned short GFAT::GetS(unsigned short total)
{
	unsigned short rs = 1, d = 0, shortsNded = 0, currentBytes = 0, N = 0;


	while(1)
	{

		d = (total - rs);
		//printf("D: %ld\n",(long int)D);

		shortsNded = ((d+1))*2;

		//printf("Bytes needed %ld\n", (long int)shortsNded);

		currentBytes = rs*SEC_SZ;

		//printf("currentBytes %ld\n", (long int)currentBytes);

		//as soon as we have enough space break
		if(rs*SEC_SZ >= shortsNded)
		{

			//	printf("rs is %ld\n",(long int)rs);
			break;

		}
		else rs++;

	}

	//SET d AND s IN THE FAT 
	D = (total - rs);
	S = rs;
	printf("D is %u\n",D);
	printf("S is %u\n", S);


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
	//int fd = (int)open(fileName, O_RDWR);

	if(fp == NULL) 
	{
		printf("error opening file %s\n", fileName);
		exit(0);
	}
	printf("opened file %s\n", fileName); 

	//calculate the file size of given file
	fseek (fp, 0 , SEEK_END);
	unsigned int size = ftell (fp);
	rewind (fp);

	printf("file %s is %u bytes\n", fileName, size);

	/*
	   if(mode)
	   {

	//if want idx r of FAT look at buff[r*2+1] = big, and buff[(r*2+1)-1]


	printf("reading sector 0 into the buffer\n");

	int igot = fread(Jbuff,1,SEC_SZ,fp);

	printf("%d bytes were read\n",igot);

	unsigned long  lil = 0, big = 0, rl = 16, nrl = rl*2+1;

		printf("-----new rl %ld, n-1: %ld\n", nrl, nrl-1);

		lil = (unsigned long)Jbuff[4];
		big = (unsigned long)Jbuff[5];

		printf("big %ld, lil %ld\n",big,lil);

		unsigned long llil = (unsigned long)Jbuff[nrl];
		if(llil > 0xc0 ) llil -= 0xffffff00;
		//unsigned int lbig = (unsigned int)Jbuff[nrl-1] - 0xffffff00;;
		unsigned int lbig = (unsigned int)Jbuff[nrl-1];
		if(lbig > 0xc0 ) lbig -= 0xffffff00;
		printf("---->IN hex yo %x %x\n",(long int)Jbuff[nrl], (long long)Jbuff[nrl-1]);

		printf("llil %u, lbig %u\n",llil,lbig);
		printf("the number is %u \n",llil*256 + lbig);

		//convert the hex to a decimal number
		unsigned int nextL = llil*256 + lbig;
		printf("the next s is %u\n",nextL);
		printf("that is disk sector %u\n", 10+nextL-1);

		//now read the first element in the FAT
		printf("showing 2301 due to little endian\n");
		//printf("%x %x %x %x IS the first data sector on the free list\n",(char)Jbuff[2], (char)Jbuff[3],(char)Jbuff[0], (char)Jbuff[1]);
		//printf("%d %x %x %x IS the first data sector on the free list\n",(int)Jbuff[6],(int)Jbuff[3], (int)Jbuff[0],(int)Jbuff[1]);
		printf("1%c %c  IS the first data sector on the free list\n",Jbuff[1], (char)Jbuff[0]);
		printf("2%c %x  IS the first data sector on the free list\n",(char)Jbuff[6], Jbuff[6]);
		printf("3%c %d  IS the first data sector on the free list\n",(char)Jbuff[6], Jbuff[6]);
		printf("4%d %d  IS the first data sector on the free list\n",(int)Jbuff[1],(int)Jbuff[0]);
		//printf("%c %c%c %c is the first data sector on the free list\n",(int)Jbuff[2], (char)Jbuff[3],(char)Jbuff[0], (char)Jbuff[1]);


		unsigned int num = (unsigned int)Jbuff[1]+(unsigned int)Jbuff[0]*10+(unsigned int)Jbuff[3]*100+(unsigned int)Jbuff[2]*1000;

		printf("the link is %u\n", num);

		unsigned int link = (S + num - 1);
		printf("Need to go to %u\n", link);
	}
*/

	/*
	   unsigned int size = (unsigned int)lseek(fd,0,SEEK_END);
	   lseek(fd,0,SEEK_SET);
	//printf("the offset is now at %u\n",(int)lseek(fd,0,SEEK_CUR));
	printf("returning %u\n",size);
	close(fd);
	*/


	//printf("returning %u\n",size);
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
	//system("ls");

	if(argc == 4) file = (argv[3]);
	else if(argc == 5) file = (argv[4]);
	
	printf("jdisk: %s ,file: %s\n", jdisk.c_str(), file.c_str());

	//use report command to get disk info and put output of command into myfile.txt

	//printf("the size of a short is %d bytes or %d bits\n",(int)sizeof(unsigned short), (int)sizeof(unsigned short)*8);

	GFAT * GF;

	//need to check format of jdisk is correct , it should be some multiple of sec_sz(1024)
	//size = (unsigned int)GF->GetFileSize(argv[1],1);

	//if( size % SEC_SZ == 0 ) printf("the size of the jdisk is %u\n", size);
	//else printf("Issue with jdisk format\n");

	//calculate total number of sectors on this disk
	//total = size/SEC_SZ;

	//printf("D is or there are %u sectors on this disk\n", total);

	//calculate the total size of the FAT(S)
	//unsigned int S = GF->GetS(total);



	//printf("\n---the first %u sectors are the FAT\n",S);
	//printf("----,or the first %u sectors are the FAT\n",GF->S);


	//get the file size of needed file
	if(argc == 4) 
	{
		GF = new GFAT(argv[3],argv[1],0);
		//size = (int)GF->GetFileSize(argv[3],0);
		//printf("the file %s is %d bytes in size\n", argv[3],size);
	}
	else if(argc == 5) 
	{
		unsigned int start = (unsigned int) atol(argv[3]);

		printf("the start block is %u\n",start);

		GF = new GFAT(argv[4],argv[1],start);
		//size = (int)GF->GetFileSize(argv[4],0);
		//printf("the file %s is %d bytes in size\n", argv[4], size);
	}


	/*now check to see if the selected disk is big has enough space?*/

	return 0;
}



