/*
 * 		@author Heejun Lee (zaqq1414@gmail.com)
 * 		@file	page.h
 * 		@brief	Disk-based B+ tree with buffer
 * 		@since	2017-11-09
 */

#ifndef __PAGE_H
#define __PAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

typedef char BYTE;
typedef int64_t offsetType;

#define true 1
#define false 0

#define PAGE_SIZE			(4096)
#define PAGE_HEADER_SIZE	(128)
#define KEY_SIZE			(8)
#define VALUE_SIZE			(120)
#define TABLE_SIZE			(10)
#define BUFFER_SIZE			(1000)

//#define LEAF_ORDER			(32)
//#define INTERNAL_ORDER		(249)
#define LEAF_ORDER		(4)
#define INTERNAL_ORDER	(4)

#define HEADERPAGE_OFFSET	(0)

/* Types representing page on disk.
 * Its size is set to 4096 bytes
 * as required by the specification.
 * 
 * There are 4 types of page.
 * 1. Header page
 * 2. Free page
 * 3. Internal page
 * 4. Leaf page 
 */

#pragma pack(push, 1)
typedef struct Page {
	BYTE byte[PAGE_SIZE];
} Page;

typedef struct HeaderPage{
	offsetType free_page;
	offsetType root_page;
	int64_t num_pages;
	BYTE reserved[PAGE_SIZE - sizeof(int64_t) - sizeof(offsetType)*2];
} HeaderPage;

typedef struct FreePage{
	offsetType next_page;
	BYTE reserved[PAGE_SIZE - sizeof(offsetType)];
} FreePage;

/* Types representing header of page.
 *
 * First 128 bytes of leaf and internal page 
 * will be used as a page header for other
 * types of page.
 */

typedef struct LeafPageHeader{
	offsetType parent;
	int is_leaf;
	int num_keys;
	BYTE reserved[PAGE_HEADER_SIZE - sizeof(int)*2 - sizeof(offsetType)*2];
	offsetType right;
} LeafPageHeader;

typedef struct InternalPageHeader{
	offsetType parent;
	int is_leaf;
	int num_keys;
	BYTE reserved[PAGE_HEADER_SIZE - sizeof(int)*2 - sizeof(offsetType)*2];
	offsetType one_more_page;
} InternalPageHeader;

/* Types representing key and value 
 */

typedef struct Record{
	int64_t key;
	BYTE value[VALUE_SIZE];
} Record;

typedef struct Entry{
	int64_t key;
	offsetType child;
} Entry;

/* Leaf page contains the key/value records.
 * Keys are sorted in the page.
 *
 * maximum 31 records per one data page.
 * 
 * First 128 bytes will be used as a page header 
 * for other types of page.
 *
 * Branch factor (order) = 32.
 */

typedef struct LeafPage{
	LeafPageHeader header;
	Record records[LEAF_ORDER - 1];
} LeafPage;

/* Internal page is similar to leaf page, but
 * instead of containing 120 bytes of values,
 * it contains 8 bytes of another page offset.
 */

typedef struct InternalPage{
	InternalPageHeader header;
	Entry entries[INTERNAL_ORDER - 1];
} InternalPage;

/* Types representing buffer in memory.
 */
typedef struct LRU LRU;

typedef struct Buffer{
	Page * page;
	int table_id;	// table_id is (fd - 2)
	offsetType page_offset;
	bool is_dirty;
	int pin_count;
	bool in_LRU;
	LRU * lru;
} Buffer;

/* Utility types for following LRU policy.
 * Buffers will be managed through this linked-list.
 */

struct LRU{
	Buffer * buffer;
	LRU * pre;
	LRU * next;
};

typedef struct LRU_List{
	LRU * head;
	LRU * tail;
	int num_lru;
} LRU_List;

#pragma pack(pop)

/*************************************************
 * Global variables
 ************************************************/

int fd;
Buffer ** buf;
int num_buf;
LRU_List * LRU_list;

/*************************************************
 * Functions
 ************************************************/

/* 1. Initialization
 */
int open_table(const char * pathname);
int close_table(int table_id);
int shutdown_db(void);

/* 2. Buffer management
 */
int init_db(int num_buf);
int update_LRU(Buffer * buf);
void drop_victim(void);
Buffer * init_headerpage(int table_id);
void mark_dirty(Buffer * buf);
void release_pincount(Buffer * buffer);
void init_buf(int table_id);
Buffer * get_buf(int table_id, offsetType offset);
Buffer * find_buf(int table_id, offsetType offset);
Buffer * set_buf(int table_id, offsetType offset);
void read_page(int table_id, Page * page, offsetType offset);
void write_page(int table_id, Page * page, offsetType offset);

#endif
