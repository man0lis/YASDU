#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xts_crypto.h"

//OMFG THANKS  https://github.com/ihaveamac/switchfs FOR THE SOURCE
//since some people keep their shit closed source and all...

#define NAND_SECTROR_SIZE 0x4000
#define DEC_NAND_SIZE 0x4000
#define KEY_SIZE 32
#define KEY_COUNT 4



static void usage(void) {
    fprintf(stderr,"Usage : \n--i=[file]\n--o=[output]\n--k=[key number]\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	
	printf("/--------------------------------------------------------------------\\ \n\r");
	printf("|                           DacoTaco's R.N.D                          | \n\r");
	printf("|                         Random Nand Decryptor                       | \n\r");
	printf("\\--------------------------------------------------------------------/ \n\r");
	
	unsigned char encryptionKey[KEY_COUNT][KEY_SIZE] = {0};
	unsigned char tweakKey[KEY_COUNT][KEY_SIZE] = {0};
	int chosen_key = 0;
	FILE* fp = NULL;
	
	char inputFilePath[512] = {0};
	char outputFilePath[512] = {0};
	
	for(int i = 1;i < argc;i++)
	{
		if(strlen(argv[i]) < 4 || strlen(argv[i]) > 64)
		{
			usage();
		}
		
		char argument[8] = {0};
		memcpy(argument,argv[i],4);
		
		if(strncmp(argument,"--k=",4) == 0)
		{
			char key_string[2] = { 0 };
			key_string[0] = *argument + 4;
			chosen_key = (int)strtol(key_string,NULL,10);
			if(chosen_key > KEY_COUNT || chosen_key < 0)
			{
				usage();
			}
			//printf("key : %d\n\r",chosen_key);
		}
		else if(strncmp(argument,"--i=",4) == 0)
		{
			memcpy(inputFilePath,argv[i]+4,strlen(argv[i]+4));
			printf("input : %s\n",inputFilePath);
		}
		else if(strncmp(argument,"--o=",4) == 0)
		{
			memcpy(outputFilePath,argv[i]+4,strlen(argv[i]+4));
			printf("output : %s\n",outputFilePath);
		}
		else
		{
			usage();
		}
	}
	
	if(argc < 4 || strlen(outputFilePath) == 0 || strlen(inputFilePath) == 0)
	{
		usage();
	}
	printf("reading keys...\n");
	
	fp = fopen("biskeydump.txt","r");
	if(fp == NULL)
	{
		printf("unable to open the biskey dump file!\n\r");
		return -EFAULT;
	}
	
	char line[512];
	size_t len = 511;
	int key_nr = 0;
	while(fgets(line,len,fp) != NULL)//getline(line,len,fp) >= 0)
	{
		char crypt[33];
		char tweak[33];
		
		crypt[32] = 0;
		tweak[32] = 0;
		
		//printf("%s\n",line);
		if(strncmp(line,"BIS KEY ",8) == 0)
		{
			char* split = line;
			for(int i = 0;i <= 1;i++)
			{
				split = split+10-i;
				strncpy(crypt,split,32);
			}
			
			if(fgets(line,len,fp) != NULL)
			{
				char* split = line;
				for(int i = 0;i <= 1;i++)
				{
					split = split+10-i;
					strncpy(tweak,split,32);
				}
			}
			
			/*printf("crypt(%d) : %s\n\r",key_nr,crypt);
			printf("tweak(%d) : %s\n\r",key_nr,tweak);*/
			
			//convert keys
			for(unsigned int y = 0;y*2 < strlen(crypt);y++)
			{
				char number[3] = { 0 };
				memcpy(number,crypt+(y*2),2);
				encryptionKey[key_nr][y] = (unsigned char)strtol(number,NULL,16);

				memset(number,0,3);
				memcpy(number,tweak+(y*2),2);
				tweakKey[key_nr][y] = (unsigned char)strtol(number,NULL,16);
			}
			
			key_nr++;
			
			//we read all keys
			if(key_nr >= KEY_COUNT)
				break;
		}
	}
	fclose(fp);
	
	if(key_nr < KEY_COUNT)
	{
		printf("did not get all keys!");
		return -EFAULT;
	}
	
	printf("\n\nopening %s & %s...\n",inputFilePath,outputFilePath);
	
	unsigned char* inBuf = NULL;
	unsigned int fileSize = 0;
	size_t sectors = 0;	
	FILE* in_fd = fopen(inputFilePath,"rb");
	FILE* out_fd = fopen(outputFilePath,"wb");
	
	if(in_fd == NULL || out_fd == NULL)
	{
		fprintf(stderr, "error opening files\n");
		exit(EXIT_FAILURE);
	}
	
	fseek(in_fd,0,SEEK_END);
	fileSize = ftell(in_fd);
	fseek(in_fd,0,SEEK_SET);
	
	inBuf = (unsigned char*)malloc(fileSize);
	
	fread(inBuf,1,fileSize,in_fd);
	
	sectors = (fileSize) / NAND_SECTROR_SIZE;
	if(fileSize % NAND_SECTROR_SIZE != 0)
	{
		//there is a remainder, so add a sector
		sectors++;
	}
	printf("detected size : 0x%X , 0x%X sectors\n",fileSize,sectors);
	
	xts_crypto crypto (encryptionKey[chosen_key], tweakKey[chosen_key], NAND_SECTROR_SIZE);
	printf("decrypting...\n");
	unsigned int decrypted = 0x0;
	
	for (size_t i = 0; i < sectors; i++) 
	{
		unsigned char* ptr = inBuf + decrypted;
        crypto.decrypt(ptr, i);
		decrypted +=NAND_SECTROR_SIZE;
		fwrite(ptr,1,NAND_SECTROR_SIZE,out_fd);
		int decryptSize = (decrypted + NAND_SECTROR_SIZE > fileSize)?fileSize - decrypted:NAND_SECTROR_SIZE;
		printf("\rdecrypted sector %u | 0x%X bytes of 0x%0X | 0x%x",(int)i,decrypted,fileSize,decryptSize);
    }
	
	
	printf("\n");
	 
	free(inBuf);
	fclose(in_fd);
	fclose(out_fd);

    
    return EXIT_SUCCESS;
}
