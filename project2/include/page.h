#ifndef __PAGE_H
#define __PAGE_H 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef char BYTE;
typedef int64_t offsetType;

/* Structure representing page on disk.
 * Its size is set to 4096 bytes
 * as required by the specification.
 *
 * There are 4 types of page.
 * 1. Header page
 * 2. Free page
 * 3. Internal page
 * 4. Leaf page 
 */

#define true 1
#define false 0

#define PAGE_SIZE           (4096)
#define PAGE_HEADER_SIZE    (128)
#define KEY_SIZE            (8)
#define VALUE_SIZE          (120)

#define LEAF_ORDER	((PAGE_SIZE - PAGE_HEADER_SIZE) / sizeof(Record)) + 1
#define INTERNAL_ORDER	((PAGE_SIZE - PAGE_HEADER_SIZE) / (KEY_SIZE + sizeof(offsetType))) + 1

//#define LEAF_ORDER 4
//#define INTERNAL_ORDER 4


typedef struct Page{
	BYTE byte[PAGE_SIZE];
} Page;


/* Header page must be first page in file.
 * When we open the data file,
 * we can initialize disk-based b+ tree
 * using this header page.
 */
#pragma pack(push, 1)
typedef struct HeaderPage{
	offsetType freePageList;
	offsetType rootPage;
	int64_t numPages;
	int64_t reserved[PAGE_SIZE - sizeof(int64_t) - sizeof(offsetType)*2];
} HeaderPage;
#pragma pack(pop)

/* Free page is managed by linked list
 * using freePageList pointed by 
 * the header page.
 */
#pragma pack(push, 1)
typedef struct FreePage{
	offsetType nextFreePage;
	BYTE reserved[PAGE_SIZE - sizeof(offsetType)];
} FreePage;
#pragma pack(pop)

/* Structure representing header of page.
 * 
 * First 128 bytes of leaf and internal page 
 * will be used as a page header for other 
 * types of page.
 */

typedef struct LeafPageHeader{
	offsetType parent;
	int isLeaf;
	int numKeys;
	BYTE reserved[PAGE_HEADER_SIZE - (sizeof(int) * 2) - (sizeof(offsetType) * 2)];
	offsetType right;
} LeafPageHeader;

typedef struct InternalPageHeader{
	offsetType parent;
	int isLeaf;
	int numKeys;
	BYTE reserved[PAGE_HEADER_SIZE - (sizeof(int) * 2) - (sizeof(offsetType) * 2)];
	offsetType oneMorePage;
} InternalPageHeader;

/* Structure representing key and value
 */
typedef struct Record{
	int64_t key;
	BYTE value[VALUE_SIZE];
} Record;

typedef struct Entry{
	int64_t key;
	offsetType subPage;
} Entry;

/* Leaf page contains the key/value records.
 * Keys are sorted in the page.
 *
 * One record size is 128 bytes and we contain 
 * maximum 31 records per one data page.
 * 
 * First 128 bytes will be used as a page header 
 * for other types of page.
 * 
 * Branch factor (order) = 32.
 */
#pragma pack(push, 1)
typedef struct LeafPage{
	LeafPageHeader header;
	Record records[LEAF_ORDER-1];
} LeafPage;
#pragma pack(pop)

/* Internal page is similar to leaf page, but
 * instead of containing 120 bytes of values,
 * it contains 8 bytes of another page offset.
 */
#pragma pack(push, 1)
typedef struct InternalPage{
	InternalPageHeader header;
	Entry entries[INTERNAL_ORDER-1];
} InternalPage;
#pragma pack(pop)

/************************************************
 * Functions
 ************************************************/

int open_db(const char * filepath);
int load_page(Page * page, offsetType offset);
void create_free();
offsetType write_new_page(Page * page);
int init_page();

/************************************************
 * Global variables
 ************************************************/

int fd;
HeaderPage headerPage;

#endif
