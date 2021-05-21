/**********************************************************************
 * 							PS2 APA header injection
 * 
 * 							alexparrado (2021)  
 * hddosd-headers.h
 * ********************************************************************/

#ifndef __HDDOSD_HEADERS_H__
#define __HDDOSD_HEADERS_H__

//Number of sectors to perform injection
#define NSECTORS 256

//Enumeration data type for category files
typedef enum
{
  SYSTEM_CNF,
  ICON_SYS,
  LIST_ICO
} category_t;

//Struct data type for user function argument
typedef struct
{

  char *systemCnf;
  char *iconSys;
  char *listIco;
  char *partition;

} header_info_t;

//User function
int WriteAPAHeader(header_info_t info);

#endif
