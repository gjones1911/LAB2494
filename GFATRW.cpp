#include <iostream>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
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
	GFAT(const char * filename, const char * jdisk);
	~GFAT();
	//mode tells it if it is working with a jdisk or regular file
	unsigned int GetFileSize(const char * fileName, int mode);
	
	//Calculates the necessary S value. also sets D
	unsigned short GetS(unsigned short Total_Sectors);
	
	//checks how many sections are left on the given disk
	unsigned short FreeSecCnt(FILE * fp);
	
	
	//reads the link and returs the appropriate link number
	unsigned short Read_Link(int link);
	
	//
	void Write_Link(int link, unsigned short val);
	
	
	void Flush_Links(); //(checks the flags and calls jdisk_write().

	//a buffer set to the size of a disk sector
	//used for reading disks a sector at a time
	char * Jbuff;
	char * Fbuff;


	unsigned int D,S, fileSize, disksize;;

	FILE * JP;		//jdisk pointer 
	FILE * FP;		//file pointer

	char * FAT;

	int filed[250];
	FILE * fp[250];
};


string MakeCommand(vector<string> words)
{

	string command, word;

	word.clear();
	command.clear();


	for(int i = 0; i < words.size();i++)
	{
		word.clear();
		word = words[i];

		for(int j = 0; j < word.size();j++)
		{
			command.push_back(word[j]);
		}

		command.push_back(' ');
	}

	printf("the command is %s\n",command.c_str());
	return command;
}



GFAT::GFAT(const char * fileName, const char * JDisk)
{
	unsigned int total = 0, i=0, tbytes = 0;
	

	//get the size of the file and the jdisk
	fileSize = GetFileSize(fileName,0);
	disksize = GetFileSize(JDisk,1);

	//need to check format of jdisk is correct , it should be some multiple of sec_sz(1024)
//	size = (unsigned int)GF->GetFileSize(argv[1],1);

	if( disksize % SEC_SZ == 0 ) printf("the size of the jdisk is %u\n", disksize);
	else printf("Issue with jdisk format\n");
	
	printf("the file is %u bytes\n", fileSize);


	total = disksize/SEC_SZ;


	printf("There are %u sectors on this disk\n", total);

	GetS(total);
	//D = total - S;

	tbytes = (D+1)*2;

	printf("the fat is the first %u bytes of jdisk\n", tbytes);

	FAT = (char *)malloc(sizeof(char) * tbytes);

	printf("there are %u sectors on this disk's FAT, and %u data sections\n",S,D);
	printf("----There are disks from byte %u*DNUM to %u\n",(S*SEC_SZ),(S*SEC_SZ+SEC_SZ-1));

	unsigned int needed = fileSize/SEC_SZ;

	if(needed > 1) printf("The file needs %u sectors\n",needed);
	else  printf("The file only needs %u sector\n",1);

	//check if there is enough space for the input file
	
	//set up the buffers
	Jbuff = (char *)malloc(sizeof(char)*SEC_SZ);
	Fbuff = (char *)malloc(sizeof(char)*SEC_SZ);

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


	//count free sectors
	while( nextone != 0)
	{
		
		nextone = FAT[nextone] + FAT[nextone+1]*256;
		printf("the next one is %u\n", nextone);
		
		nextone = (nextone)*2;
		
		printf("actual %u\n",nextone);

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


	printf("returning %u\n",size);
	return size;	
}

int main (int argc, char ** argv)
{

	//system("ls");
	//
	//

	FILE * output;
	FILE * outfile;

	string command, myfile = " myfile.txt", report, create, attach,reads,writes,fat;
	string jdisk, file;
	fat = "./FAT";
	report = "report";
	create = "create";
	attach = "attach";
	reads = "reads";
	writes = "writes";


	command.push_back('.');
	command.push_back('/');
	command.push_back('F');
	command.push_back('A');
	command.push_back('T');
	command.push_back(' ');
	//command.push_back(argv[1]);
	unsigned int i = 0;
	while( argv[1][i] != '\0' )
	{
		jdisk.push_back(argv[1][i]);
		command.push_back(argv[1][i]);
		i++;
	}

	i = 0;

	if(argc == 4)
	{
		while( argv[3][i] != '\0' )
		{
			file.push_back(argv[3][i]);
			i++;
		}

	
	}
	else if(argc == 5)
	{
		while( argv[4][i] != '\0' )
		{
			file.push_back(argv[4][i]);
			i++;
		}


	}

	printf("jdisk: %s ,file: %s\n", jdisk.c_str(), file.c_str());

	printf("the name is %u long\n", i);
	command.push_back(' ');
	command.push_back('r');
	command.push_back('e');
	command.push_back('p');
	command.push_back('o');
	command.push_back('r');
	command.push_back('t');
	command.push_back('>');
	command.push_back(' ');

	for(int k = 0; k < myfile.size();k++)
	{
		command.push_back(myfile.at(k));
	}

	printf("the command is \n%s\n", command.c_str());

	//put output of command into myfile,txt
	system(command.c_str());

	output = fopen("outfile.txt","w");
	outfile = fopen("myfile.txt","r");



	if(output == NULL)
	{
		printf("couldn't open outfile\n");
		exit(0);
	}
	
	if(outfile == NULL)
	{
		printf("couldn't open myfile.txt\n");
		exit(0);
	}

	unsigned int t=0,d=0,s=0;

	char buff[256];
	fscanf(outfile,"%s %s %u%s  %s %s %u%s  %s %s %u%s\n%s %s %s %s %s ",buff,buff,&t,buff,buff,buff,&d,buff,buff,buff,&s,buff,buff,buff,buff,buff,buff); 
	
	unsigned int num = 0, fcnt = 0;;

	printf("thge buff %s\n",buff);
	//printf("fscan return %d\n", fscanf(outfile," %u",&num));
	//printf("num is %u\n",num);

	//printf("fscan return %d\n", fscanf(outfile," %u",&num));
	//printf("num is %u\n",num);

	fscanf(outfile," %u",&num);

	printf("There are %u disks free\n",num);

	//fprintf(stdout, "%.2f %s\n",3.24,"bob");

	printf("igot %u, %u, %u\n", t,d,s);
	


	//printf("the size of a short is %d bytes or %d bits\n",(int)sizeof(unsigned short), (int)sizeof(unsigned short)*8);

	GFAT * GF;

	unsigned int size = 0,total = 0;;

	if( (argc != 4 && argc != 5 ) || (strcmp(argv[2], "import") != 0 && strcmp(argv[2],"export") != 0) )
	{
		printf("usage: ./a.out JdiskFile import InputFileName\n");
		printf("usage: ./a.out JdiskFile export starting_block OutputFileName\n");
		exit(0);
	}


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
		GF = new GFAT(argv[3],argv[1]);
		//size = (int)GF->GetFileSize(argv[3],0);
		//printf("the file %s is %d bytes in size\n", argv[3],size);
	}
	else if(argc == 5) 
	{
		GF = new GFAT(argv[4],argv[1]);
		//size = (int)GF->GetFileSize(argv[4],0);
		//printf("the file %s is %d bytes in size\n", argv[4], size);
	}


	//check how many bytes the file needs

	unsigned int needed = size/SEC_SZ;



	/*now check to see if the selected disk is big has enough space?*/

	return 0;
}



