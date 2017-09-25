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
	GFAT();
	unsigned int GetFileSize(const char * fileName);
	unsigned short GetS(unsigned short Total_Sectors);
	unsigned short FreeSecCnt(const char * JDisk);
	unsigned short Read_Link(int link);
	void Write_Link(int link, unsigned short val);
	void Flush_Links(); //(checks the flags and calls jdisk_write().

	char * Jbuff;
	char * Fbuff;

	int filed[250];
	FILE * fp[250];
};

GFAT::GFAT()
{
	Jbuff = (char *)malloc(sizeof(char)*1024);
	Fbuff = (char *)malloc(sizeof(char)*1024);
	

}

unsigned short GFAT::FreeSecCnt(const char * JDisk)
{
	
}
unsigned short GFAT::GetS(unsigned short total)
{
	unsigned short rs = 1, D = 0, shortsNded = 0, currentBytes = 0, N = 0;


	while(1)
	{

		D = (total - rs);
		//printf("D: %ld\n",(long int)D);

		shortsNded = ((D+1))*2;

		//printf("Bytes needed %ld\n", (long int)shortsNded);

		currentBytes = rs*SEC_SZ;
		
		//printf("currentBytes %ld\n", (long int)currentBytes);

		if(rs*SEC_SZ >= shortsNded)
		{
			
		//	printf("rs is %ld\n",(long int)rs);
			break;

		}
		else rs++;

	}

	printf("rs is %ld\n", (long int)rs);


	return rs; 
}

//unsigned int GetFileSize(int fd)
unsigned int GFAT::GetFileSize(const char * fileName)
{

	int fd = (int)open(fileName, O_RDWR);
	
	if(fd < 0) 
	{
		printf("error opening file %s\n", fileName);
		exit(0);
	}

	unsigned int size = (unsigned int)lseek(fd,0,SEEK_END);
	lseek(fd,0,SEEK_SET);
	//printf("the offset is now at %u\n",(int)lseek(fd,0,SEEK_CUR));
	printf("returning %u\n",size);
	close(fd);
	return size;	
}

int main (int argc, char ** argv)
{

	//system("ls");

	//printf("the size of a short is %d bytes or %d bits\n",(int)sizeof(unsigned short), (int)sizeof(unsigned short)*8);

	GFAT * GF = new GFAT;

	unsigned int size = 0, D = 0;;

	if( (argc != 4 && argc != 5 ) || (strcmp(argv[2], "import") != 0 && strcmp(argv[2],"export") != 0) )
	{
		printf("usage: ./a.out JdiskFile import InputFileName\n");
		printf("usage: ./a.out JdiskFile export starting_block OutputFileName\n");
		exit(0);
	}


	//need to check format of jdisk is correct 
	
	size = (unsigned int)GF->GetFileSize(argv[1]);

	if( size % SEC_SZ == 0 ) printf("the size of the jdisk is %u\n", size);
	else printf("Issue with jdisk format\n");

	D = size/SEC_SZ;

	printf("D is or there are %u sectors on this disk\n", D);

	unsigned int S = GF->GetS(D);

	printf("the first %u sectors are the FAT\n",S);

	//get the file size of needed file
	if(argc == 4) 
	{
		size = (int)GF->GetFileSize(argv[3]);
		printf("the file %s is %d bytes in size\n", argv[3],size);
	}
	else if(argc == 5) 
	{
		size = (int)GF->GetFileSize(argv[4]);
		printf("the file %s is %d bytes in size\n", argv[4], size);
	}


	//check how many bytes the file needs
	
	unsigned int needed = size/SEC_SZ;

	if(needed > 1) printf("The file needs %u sectors\n",needed);
	else  printf("The file only needs %u sector\n",1);


	/*now check to see if the selected disk is big has enough space?*/

	return 0;
}



