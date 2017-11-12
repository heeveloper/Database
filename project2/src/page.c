#include "page.h"

/* Given the file path,
 * open and initialize the data file.
 */
int open_db(const char * filepath){
	
	int result;
	
	// Check if there is file.
	if((result = access(filepath, F_OK)) == 0){
		if((fd = open(filepath, O_RDWR | O_SYNC, 0644)) < 0){
			printf("open error!\n");
			return -1;
		}
		else{
			// Load header page from disk.
			return load_page((Page*)&headerPage, 0);
		}
	}
	else{
	
		// Create new data file and initialize its header page.
		if((fd = open(filepath, O_RDWR | O_SYNC | O_CREAT, 0644))< 0){
			return -1;
		}
		return init_page();
	}
}

/* Load header page in data file
 * to header page in memory.
 */

int load_page(Page * page, offsetType offset){

	// File was not opened.
	if(fd == 0) return -1;

	return (pread(fd, page, PAGE_SIZE, offset) == PAGE_SIZE);
}

/* When there is only one free page left,
 * create 10 free pages.
 */
void create_free(){
	FreePage * lastFreePage = (FreePage*)malloc(PAGE_SIZE);
	load_page((Page*)lastFreePage, headerPage.freePageList);
	lastFreePage->nextFreePage = headerPage.numPages*PAGE_SIZE;
	
	FreePage ** freepages = (FreePage**)malloc(sizeof(offsetType) * 10);
	int i;
	for(i=0; i<10; i++){
		freepages[i] = (FreePage*)malloc(PAGE_SIZE);
		freepages[i]->nextFreePage = (headerPage.numPages*PAGE_SIZE) + PAGE_SIZE * (i+1);
	}

	pwrite(fd, lastFreePage, PAGE_SIZE,  headerPage.freePageList);
	pwrite(fd, freepages, PAGE_SIZE*10, headerPage.numPages * PAGE_SIZE);
	headerPage.numPages += 10;
	pwrite(fd, &headerPage, PAGE_SIZE, 0);
	free(lastFreePage);
	for(i=0; i<10; i++) free(freepages[i]);
	free(freepages);

	return;
}

/* Write down new page on the first free page
 * pointed by headerPage.
 * Then return the offset.
 */
offsetType write_new_page(Page * page){
	offsetType newOffset = headerPage.freePageList;
	
	FreePage * firstFreePage = (FreePage*)malloc(PAGE_SIZE);
	load_page((Page*)firstFreePage, headerPage.freePageList);
	if(firstFreePage->nextFreePage != 0){
		headerPage.freePageList = firstFreePage->nextFreePage;
	}
	else{
		// Create new free page.
		create_free();
		load_page((Page*)firstFreePage, headerPage.freePageList);
		headerPage.freePageList = firstFreePage->nextFreePage;
	}
	
	pwrite(fd, page, PAGE_SIZE, newOffset);
	free(firstFreePage);
	return newOffset;
}


/* When new file is created,
 * header page must be created and initialized.
 */

int init_page(){
	headerPage.freePageList = PAGE_SIZE*2;
	headerPage.rootPage = PAGE_SIZE;
	headerPage.numPages = 1002; // itself and root leaf page.

	LeafPage * root = (LeafPage*)malloc(PAGE_SIZE);
	
	root->header.parent = 0;
	root->header.isLeaf = 1;
	root->header.numKeys = 0;
	root->header.right = 0;

	FreePage ** freepages = (FreePage**)malloc(sizeof(offsetType) * 1000);
	int i;
	for(i=0;i<1000;i++){
		freepages[i] = (FreePage*)malloc(PAGE_SIZE);
		freepages[i]->nextFreePage = PAGE_SIZE*(i+3);
	}
	freepages[999]->nextFreePage = 0;

	pwrite(fd, &headerPage, PAGE_SIZE, 0);
	pwrite(fd, root, PAGE_SIZE, PAGE_SIZE);
	for(i=0;i<1000;i++)
		pwrite(fd,freepages[i], PAGE_SIZE, PAGE_SIZE*(i+2));
	//pwrite(fd, freepages, PAGE_SIZE*1000, PAGE_SIZE*2);
	
	free(root);
	for(i=0;i<1000;i++) free(freepages[i]);
	free(freepages);
	
	return 0;
}

