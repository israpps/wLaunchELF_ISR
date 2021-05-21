/**********************************************************************
 * 							PS2 APA header injection
 * 
 * 							alexparrado (2021)  
 * hddosd-headers.c
 * ********************************************************************/

//Header files
#include <stdio.h>
#include <errno.h>
#include <hdd-ioctl.h>
#include <fileXio_rpc.h>
#include <string.h>
#include <malloc.h>
#include "hddosd-headers.h"

//Size of buffer for HDD dump
#define HDD_BLOCK_SIZE (NSECTORS * 512)

//Sector where partition starts
int partitionSector = -1;

//Buffer for I/O devctl operations on HDD
uint8_t IOBuffer[512 + sizeof(hddAtaTransfer_t)] __attribute__((aligned(64)));

//Buffer for HDD dump
uint8_t dumpBuffer[HDD_BLOCK_SIZE] __attribute__((aligned(64)));

//Magic string for PS2ICON3D
static const char *HDL_HDR0 = "PS2ICON3D";

//Dumps buffer from HDD
int readBufferfromHDD()
{
	int i, result;

	//For each sector
	for (i = 0; i < NSECTORS; i++)
	{
		//Performs read operation for one sector
		((hddAtaTransfer_t *)IOBuffer)->lba = partitionSector + i;
		((hddAtaTransfer_t *)IOBuffer)->size = 1;
		if ((result = fileXioDevctl("hdd0:", APA_DEVCTL_ATA_READ, IOBuffer, sizeof(hddAtaTransfer_t), dumpBuffer + 512 * i, 512)) < 0)	
			break;
	}

	return result;
}

//Dumps buffer to HDD
int writeBuffertoHDD()
{
	int i, result;

	//For each sector
	for (i = 0; i < NSECTORS; i++)
	{
		//Performs write operation for one sector
		((hddAtaTransfer_t *)IOBuffer)->lba = partitionSector + i;
		((hddAtaTransfer_t *)IOBuffer)->size = 1;
		//Copies 512 bytes from dump buffer to IOBuffer
		memcpy(((hddAtaTransfer_t *)IOBuffer)->data, dumpBuffer + 512 * i, 512);
		if ((result = fileXioDevctl("hdd0:", APA_DEVCTL_ATA_WRITE, IOBuffer, 512 + sizeof(hddAtaTransfer_t), NULL, 0)) < 0)
			break;
	}

	return result;
}

//Gets sector where partition starts
int getPartitionSector(char *partition)
{
	iox_stat_t statHDD;
	int result;
	if ((result = fileXioGetStat(partition, &statHDD)) >= 0)
	{
		partitionSector = statHDD.private_5;
		//scr_printf("Partition sector: %d \n", partitionSector);
	}

	return result;
}

//Function to write file to APA header
int WriteFileToAPAHeader(char *fileName, char *partition, category_t category)
{

	int result;
	unsigned int size;
	iox_stat_t statFile;
	uint32_t dataOffset, sizeOffset, offsetOffset, offset;

	FILE *file;

	//Computes offsets from file category
	switch (category)
	{

	case SYSTEM_CNF:
		dataOffset = 0x1200;
		sizeOffset = 0x1014;
		offsetOffset = 0x1010;
		break;

	case ICON_SYS:
		dataOffset = 0x1400;
		sizeOffset = 0x101C;
		offsetOffset = 0x1018;
		break;

	case LIST_ICO:
		dataOffset = 0x1800;
		sizeOffset = 0x1024;
		offsetOffset = 0x1020;
		break;
	}
	//Offset for data offset
	offset = dataOffset - 0x1000;

	//If file stat can be done
	if ((result = fileXioGetStat(fileName, &statFile)) >= 0)
	{
		size = statFile.size;
		//If file can be opened
		if ((file = fopen(fileName, "rb")) != NULL)
		{
			//If file can be read
			if (fread(dumpBuffer + dataOffset, 1, size, file) == size)
			{
				//Stores size and offset
				memcpy(dumpBuffer + sizeOffset, &size, sizeof(uint32_t));
				memcpy(dumpBuffer + offsetOffset, &offset, sizeof(uint32_t));

				//Same data for del.ico
				if (category == LIST_ICO)
				{
					sizeOffset = 0x102C;
					offsetOffset = 0x1028;
					memcpy(dumpBuffer + sizeOffset, &size, sizeof(uint32_t));
					memcpy(dumpBuffer + offsetOffset, &offset, sizeof(uint32_t));
				}
			}
			else
				result = -EIO;
			//Closes file
			fclose(file);
		}
		else
			result = -EIO;
	}
	return result;
}

//User function to write APA headers
int WriteAPAHeader(header_info_t info)
{

	int result;

	//IF HDD is connected and formatted
	if ((result = fileXioDevctl("hdd0:", HDIOC_STATUS, NULL, 0, NULL, 0)) == 0)
	{
		//IF partition start sector can be retrieved
		if ((result = getPartitionSector(info.partition)) >= 0)
		{
			//IF HDD dump operation is succesful
			if ((result = readBufferfromHDD()) >= 0)
			{
				//Writes magic string to buffer
				memcpy(dumpBuffer + 0x001000, HDL_HDR0, strlen(HDL_HDR0));

				//If system.cnf buffer injection is succesful 
				if ((result = WriteFileToAPAHeader(info.systemCnf, info.partition, SYSTEM_CNF)) >= 0)
				{
					//If icon.sys buffer injection is succesful 
					if ((result = WriteFileToAPAHeader(info.iconSys, info.partition, ICON_SYS)) >= 0)
					{
						//If list.ico buffer injection is succesful
						if ((result = WriteFileToAPAHeader(info.listIco, info.partition, LIST_ICO)) >= 0)
						{
							//If above operations succeeded we can write buffer into HDD, not before!!!
							result = writeBuffertoHDD();
						}
					}
				}
			}
		}
	}
	else
		result = -EIO;

	return result;
}
