#include "page.h"

/* Initialize buffer array
 * used for openning database file.
 */
void init_buf(int table_id){
	int i;
	buf = (Buffer**)malloc(sizeof(Buffer*) * TABLE_SIZE);
	buf[table_id] = (Buffer*)malloc(sizeof(Buffer) * BUFFER_SIZE);

	for(i = 0; i < BUFFER_SIZE; i++){
		buf[table_id][i].page = (Page*)malloc(sizeof(Page));
		buf[table_id][i].table_id = table_id;
		buf[table_id][i].page_offset = -1;
		buf[table_id][i].is_dirty = false;
		buf[table_id][i].pin_count = 0;
		buf[table_id][i].in_LRU = false;
		buf[table_id][i].lru = (LRU*)malloc(sizeof(LRU));	
	}
}

/* Initialize header page buffer.
 * Then return it.
 */
Buffer * init_headerpage(int table_id){
	Buffer * hb;
	hb = &buf[table_id][0];
	hb->table_id = table_id;
	hb->page_offset = HEADERPAGE_OFFSET;
	hb->pin_count = 0;
	hb->is_dirty = true;
	return hb;
}

/* Read a page from disk to the given page object.
 */
void read_page(int table_id, Page * page, offsetType offset){
	pread(table_id + 2, page, PAGE_SIZE, offset);
}

/* Write a page from buffer to disk
 */
void write_page(int table_id, Page * page, offsetType offset){
	pwrite(table_id + 2, page, PAGE_SIZE, offset);
}

/* Mark the buffer dirty
 */
void mark_dirty(Buffer * buffer){
	buffer->is_dirty = true;
}

/* Select the victim in LRU_list,
 * Then remove on the buffer with updating on the disk.
 */
void drop_victim(){
	Buffer * victim;
	LRU * delete_lru;
		
	// Searching the victim from tail to head in LRU_list.
	delete_lru = LRU_list->tail->pre;
	while(delete_lru->buffer->pin_count != 0 && delete_lru != LRU_list->head){
		delete_lru = delete_lru->pre;
	}
	

	// If the victim is the head,
	// It is error.
	if(delete_lru == LRU_list->head){
		printf("Error: Nobody can victim.\n");
		return;
	}
	
	delete_lru->pre->next = delete_lru->next;
	delete_lru->next->pre = delete_lru->pre;
	
	
	/* Update on the disk within the deleted buffer
	 * if the buffer is dirty.
	 */
	
	victim = delete_lru->buffer;
	if(victim->is_dirty){
		write_page(victim->table_id, victim->page, victim->page_offset);
	}
    
	victim->is_dirty = false;
	victim->in_LRU = false;
	
	LRU_list->num_lru--;

	return;
}

/* Update LRU linked-list within new buffer.
 * If success, return 0. Otherwise, return non_zero value.
 */
int update_LRU(Buffer * buffer){
	if(!buffer->in_LRU  && LRU_list->num_lru >= num_buf){
		drop_victim();
	}

	buffer->lru->buffer = buffer;

	/* If the page already exists in buffer,
	 * move the buffer to the head of LRU_list
	 * which means most recently used.
	 */
	if(buffer->in_LRU){
		buffer->lru->pre->next = buffer->lru->next;
		buffer->lru->next->pre = buffer->lru->pre;
		LRU_list->num_lru--;
	}
	buffer->lru->next = LRU_list->head->next;
	LRU_list->head->next->pre = buffer->lru;
	buffer->lru->pre = LRU_list->head;
	LRU_list->head->next = buffer->lru;

	LRU_list->num_lru++;

	if(LRU_list->num_lru > num_buf){
		printf("Error: update_LRU()");
		return -1;
	}

	buffer->in_LRU = true;
	buffer->pin_count++;
	
	return 0;
}

/* Release pin_count of the buffer, 
 * which is called when someone ends up using the Buffer
 * such as after the update_LRU().
 */

void release_pincount(Buffer * buffer){
	buffer->pin_count--;
}

/* Open the given file path or create new one if not existed.
 * If success, return 0. Otherwise, return -1
 */

int open_table(const char * pathname){
	int fd;
	int table_id;
	HeaderPage * header_page;
	Buffer * header_buffer; 

	header_page = (HeaderPage*)malloc(sizeof(HeaderPage));

	/* Case 1: The file exists
	 */
	if(access(pathname, 0) == 0){
		if((fd = open(pathname, O_RDWR | O_SYNC, 0644)) == -1){
			printf("Open Error\n");
			return -1;
		}
		else{
		/*	Success to access the existing file.
		 *	Load header page from disk.
		 */
			printf("open check1\n");
			table_id = fd - 2;
			init_buf(table_id);
			printf("open check2\n");
			header_buffer = init_headerpage(table_id);
			printf("open check3\n");
			// Read header page from disk to buffer's member.
			read_page(table_id, header_buffer->page, HEADERPAGE_OFFSET);
			
			header_page = (HeaderPage*)header_buffer->page;
			printf("open check4\n");
			update_LRU(header_buffer);
			printf("open check5\n");
			release_pincount(header_buffer);
			return table_id;
		}
	}
	/* Case 2: The file did not exist.
	 * Create a new one and initialize its header page and 
	 * update into disk.
	 */
	else{
		if((fd = open(pathname, O_CREAT | O_RDWR | O_SYNC, 0644)) == -1){
			printf("Create Error\n");
			return -1;
		}
		else{
			table_id = fd - 2;
			init_buf(table_id);
			header_buffer = init_headerpage(table_id);
			header_page = (HeaderPage*)header_buffer->page;
			header_page->free_page = PAGE_SIZE;
			header_page->root_page = 0;	// NULL
			header_page->num_pages = 1;	// itself, header page.
			update_LRU(header_buffer);
		
			// Create root page.
			Buffer * buffer = get_buf(table_id, header_page->free_page);
			LeafPage * root = (LeafPage*)buffer->page;
			root->header.parent = 0;
			root->header.is_leaf = true;
			root->header.num_keys = 0;
			root->header.right = 0;
		
			header_page->root_page = buffer->page_offset;
			header_page->num_pages++;	
			// Update page on disk.
			mark_dirty(buffer);
			mark_dirty(header_buffer);

			release_pincount(buffer);
			release_pincount(header_buffer);
			return table_id;
		}
	}
}

/* Remove LRUs corresponding the given table_id in LRU_list.
 * If the buffer is dirty, update on disk.
 * Return 0.
 */
int close_table(int table_id){
	LRU * cur;
	cur = LRU_list->head->next;
	while(cur != LRU_list->tail){
		if(cur->buffer->table_id == table_id){
			Buffer * removed;
			removed = cur->buffer;

			cur->pre->next = cur->next;
			cur->next->pre = cur->pre;

			// If the removed buffer is dirty,
			// update on disk.
			if(removed->is_dirty){
				write_page(table_id, removed->page, removed->page_offset);
			}
			removed->is_dirty = false;
			removed->in_LRU = false;

			LRU_list->num_lru--;
		}
		cur = cur->next;
	}
	return 0;
}

/* Flush all data from buffer and destroy allocated buffer.
 * Implicitly, all buffer in LRU_list is working on memory.
 * So its fd according to its table_id is always open.
 * If success, return 0. Otherwise, return non-zero value.
 */
int shutdown_db(){
	LRU * cur;
	cur = LRU_list->head->next;
	Buffer * removed;
	while(cur != LRU_list->tail){
		removed = cur->buffer;
		cur->pre->next = cur->next;
		cur->next->pre = cur->pre;

		// If the removed buffer is dirty,
		// update on disk.
		if(removed->is_dirty){
			write_page(removed->table_id, removed->page, removed->page_offset);
		}
		removed->is_dirty = false;
		removed->in_LRU = false;

		LRU_list->num_lru--;
	}
	cur = cur->next;

	if(LRU_list->num_lru == 0) return 0;
	else
		return -1;
}

/* Initialize buffer pool and buffer manager with given number.
 * Below the table APIs should be called after init_db() is called.
 */

int init_db(int buf_num){
	num_buf = buf_num;
	LRU_list = (LRU_List*)malloc(sizeof(LRU_List));
	LRU * head = (LRU*)malloc(sizeof(LRU));
	LRU * tail = (LRU*)malloc(sizeof(LRU));
	head->next = tail;
	tail->pre = head;
	LRU_list->head = head;
	LRU_list->tail = tail;
	LRU_list->num_lru = 0;

	return 0;
}

/* If the buffer to find exists, return its pointer.
 * Otherwise, return NULL.
 */
Buffer * find_buf(int table_id, offsetType offset){
	int buf_idx = offset / PAGE_SIZE;
	//printf("offset: %ld, buf_idx: %d\n", offset, buf_idx);
	
	// There is no buffer corresponding given values.
	if(buf[table_id][buf_idx].in_LRU != true || buf[table_id][buf_idx].page_offset == -1){
		return NULL;
	}

	// Update the buffer into LRU_list.
	// Allocate or relocate in LRU_list.
	update_LRU(&buf[table_id][buf_idx]);

	return &buf[table_id][buf_idx];
}

/* Set a new buffer from disk.
 * Case 1: buffer for newly allocated page used in insert.
 * Case 2: buffer for first read page from disk.
 */
Buffer * set_buf(int table_id, offsetType offset){
	
	int buffer_idx;
	FreePage * free_page;
	HeaderPage * header_page;
	Buffer * header_buffer;
	offsetType free_offset;

	buffer_idx = offset / PAGE_SIZE;
	free_offset = offset;

	header_buffer = get_buf(table_id, HEADERPAGE_OFFSET);
	header_page = (HeaderPage*)header_buffer->page;

	// Case 1: buffer for newly allocated page,
	// 		   which means free page allocated.
	if(header_page->free_page = offset){
		free_page = (FreePage*)malloc(sizeof(FreePage));
		read_page(table_id, (Page*)free_page, offset);

		// If there is only one free page left,
		// allocate new free pages.
		if(free_page->next_page == 0){
			while(1){
				free_offset += PAGE_SIZE;
				read_page(table_id, (Page *)free_page, free_offset);
				if(free_page->next_page == 0 && header_page->root_page != free_offset){
					header_page->free_page = free_offset;
					break;
				}
			}	
		}
		// If next free page exists 
		else{
			header_page->free_page = free_page->next_page;
		}
		
		
		header_page->num_pages++;
		mark_dirty(header_buffer);
		release_pincount(header_buffer);
		free(free_page);
	}
	// Case 2: buffer for first read from disk.
	else{
		read_page(table_id, buf[table_id][buffer_idx].page, offset);
	}

	buf[table_id][buffer_idx].page_offset = offset;
	buf[table_id][buffer_idx].table_id = table_id;
	buf[table_id][buffer_idx].is_dirty = false;
	if(update_LRU(&buf[table_id][buffer_idx]) != 0){
		printf("Error : set_buf update_LRU error\n");
	}

	return &buf[table_id][buffer_idx];
}
 

/* If the buffer corresponding given offset page exists,
 * return the buffer.
 * Otherwise, read from disk and assign to a buffer.
 */
Buffer * get_buf(int table_id, offsetType offset){
	Buffer * buf;
	if((buf = find_buf(table_id, offset)) != NULL)
		return buf;

	// If the page corresponding values is not yet read,
	// read from disk into allocated buffer.
	return set_buf(table_id, offset);
}
