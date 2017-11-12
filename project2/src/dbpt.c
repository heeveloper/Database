#include "dbpt.h"

/* Find the leaf page which have the receord containging
 * the searched key.
 */
offsetType find_leaf(int64_t key){
	offsetType offset;
	InternalPage * c = (InternalPage*)malloc(PAGE_SIZE);
	
	load_page((Page*)c, headerPage.rootPage);
	offset = headerPage.rootPage;

	while(c->header.isLeaf == false){
		int i = 0;
		for(i=0; i<c->header.numKeys; i++){
			if(c->entries[i].key > key){
				break;
			}	
		}

		if(i == 0) 
			offset = c->header.oneMorePage;
		else 
			offset = c->entries[i-1].subPage;

		load_page((Page*)c, offset);
	}
	free(c);
	return offset;
}

/* Return value of record which have corresponding key
 * in the given leaf page using find_leaf.
 */
char * find(int64_t key){

	offsetType leafOffset = find_leaf(key);
	LeafPage * leaf = (LeafPage*)malloc(PAGE_SIZE);
	load_page((Page*)leaf, leafOffset);

	// It should be free out of this function.
	char * ret = (char*)malloc(sizeof(char) * 120);
	int i = 0;
	for(i=0; i< leaf->header.numKeys; i ++){
		if(leaf->records[i].key == key){
			strcpy(ret, leaf->records[i].value);
			return ret;
		}
	}
	free(ret);
	return NULL;
}

/* In the given internal page,
 * find out the position of entry which contains
 * the given offset.
 * If it is oneMorePage, then return -1
 */
int get_left_idx(InternalPage * page, offsetType offset){
	if(page->header.oneMorePage == offset) return -1;
	int i;
	for(i=0; i<page->header.numKeys; i++){
		if(page->entries[i].subPage == offset)
			return i;
	}
}

/* If there is a room for input,
 * just insert the pair of key and value 
 * in the right leaf page.
 */
int insert_into_leaf(LeafPage * leaf, offsetType leafOffset, int64_t key, char * value){
	int i, input_idx=0;
	for(i=0;i<leaf->header.numKeys; i++){
		if(leaf->records[i].key > key)
			break;
		input_idx++;
	}
	for(i=leaf->header.numKeys; i>input_idx; i--){
		leaf->records[i].key = leaf->records[i-1].key;
		strcpy(leaf->records[i].value, leaf->records[i-1].value);
	}
	leaf->records[input_idx].key = key;
	strcpy(leaf->records[input_idx].value, value);
	leaf->header.numKeys++;

	pwrite(fd, leaf, PAGE_SIZE, leafOffset);
	free(leaf);	
	//printf("write on %lld\n",leafOffset);
	return 0;
}

int insert_into_new_root(Page * left, int64_t leftOffset, int key, Page * right, int64_t rightOffset){
	
	InternalPage * root = (InternalPage*)malloc(PAGE_SIZE);
	root->header.parent = 0;
	root->header.isLeaf = 0;
	root->header.numKeys = 1;
	root->header.oneMorePage = leftOffset;
	root->entries[0].key = key;
	root->entries[0].subPage = rightOffset;
	
	offsetType rootOffset = write_new_page((Page*)root);
	headerPage.rootPage = rootOffset;
	
	InternalPage * left_child = (InternalPage*)left;
	InternalPage * right_child = (InternalPage*)right;
	left_child->header.parent = rootOffset;
	right_child->header.parent = rootOffset;

	pwrite(fd, &headerPage, PAGE_SIZE, 0);
	pwrite(fd, root, PAGE_SIZE, rootOffset);
	pwrite(fd, left_child, PAGE_SIZE, leftOffset);
	pwrite(fd, right_child, PAGE_SIZE, rightOffset);
	free(left);
	free(right);
	free(root);

	return 0;
}

/* If there is a room for new key,
 * just insert the pair of key and value
 * int the right internal page.
 */
int insert_into_internal(InternalPage * page, offsetType pageOffset, int left_idx, int64_t key, offsetType rightOffset){
	int i, input_idx = left_idx+1;

	for(i=page->header.numKeys; i>input_idx; i--){
		page->entries[i].key = page->entries[i-1].key;
		page->entries[i].subPage = page->entries[i-1].subPage;	
	}
	page->entries[input_idx].key = key;
	page->entries[input_idx].subPage = rightOffset;
	page->header.numKeys++;

	//printf("page->header.numKeys:%d\n",page->header.numKeys);
	pwrite(fd, page, PAGE_SIZE, pageOffset);
	free(page);

	return 0;
}

/* If there is no room for new key,
 * split the internal page, then add new entry
 * in its parent.
 */
int insert_into_internal_after_splitting(InternalPage * page, offsetType pageOffset, int left_idx, int key, offsetType rightOffset){
	int i, j;
	Entry * tmp = (Entry*)malloc(sizeof(Entry)*INTERNAL_ORDER);

	for(i=0,j=0; i<page->header.numKeys; i++, j++){
		if(j==left_idx+1) j++;
		tmp[j].key = page->entries[i].key;
		tmp[j].subPage = page->entries[i].subPage;
	}
	tmp[left_idx+1].key = key;
	tmp[left_idx+1].subPage = rightOffset;

	page->header.numKeys = 0;
	int split = INTERNAL_ORDER/2;
	int new_key = tmp[split].key;
	for(i=0; i<split; i++){
		page->entries[i].key = tmp[i].key;
		page->entries[i].subPage = tmp[i].subPage;
		page->header.numKeys++;
	}

	InternalPage * child = (InternalPage*)malloc(PAGE_SIZE);
	InternalPage * right_internal = (InternalPage*)malloc(PAGE_SIZE);
	right_internal->header.parent = page->header.parent;
	right_internal->header.numKeys = 0;
	right_internal->header.isLeaf = 0;
	right_internal->header.oneMorePage = tmp[split].subPage;
	offsetType rightInternalOffset = write_new_page((Page*)right_internal);
	
	load_page((Page*)child, tmp[split].subPage);
	child->header.parent = rightInternalOffset;
	pwrite(fd, child, PAGE_SIZE, tmp[split].subPage);

	for(i=split+1; i<INTERNAL_ORDER; i++){
		right_internal->entries[i-split-1].key = tmp[i].key;
		right_internal->entries[i-split-1].subPage = tmp[i].subPage;
		load_page((Page*)child, right_internal->entries[i-split-1].subPage);
		child->header.parent = rightInternalOffset;
		pwrite(fd, child, PAGE_SIZE, right_internal->entries[i-split-1].subPage);
		right_internal->header.numKeys++;
	}
	free(child);
	
	//printf("left.numKeys:%d right.numKeys:%d \n", page->header.numKeys, right_internal->header.numKeys);
	
	pwrite(fd, page, PAGE_SIZE, pageOffset);
	pwrite(fd, right_internal, PAGE_SIZE, rightInternalOffset);
	
	return insert_into_parent((Page*)page, pageOffset, new_key, (Page*)right_internal, rightInternalOffset);	

}

/* Insert new page right to original one and its offset 
 * into its parent.
 */
int insert_into_parent(Page * left, offsetType leftOffset, int64_t key, Page * right, offsetType rightOffset){

	InternalPage * left_child = (InternalPage*)left;

	// If left page is already root, make new root.
	if(left_child->header.parent == 0){
		return insert_into_new_root(left, leftOffset, key, right, rightOffset);
	}

	// Get parent.
	InternalPage * parent = (InternalPage*)malloc(PAGE_SIZE);
	offsetType parentOffset = left_child->header.parent;
	load_page((Page*)parent, parentOffset);	

	// Get index of leftOffset in the parent.
	int left_idx = get_left_idx(parent, leftOffset);

	// Insert the right offset(new offset) into parent.
	if(parent->header.numKeys < INTERNAL_ORDER-1){
		// !!! IT CAN BE REMMOVED !!!
		free(left);
		free(right);
		return insert_into_internal(parent, parentOffset, left_idx, key, rightOffset); 
	}
	else{
		free(left);
		free(right);
		return insert_into_internal_after_splitting(parent, parentOffset, left_idx, key, rightOffset);
	}
}

/* If there is no room for input,
 * split the leaf page, then add new entry
 * in its parent.
 */
int insert_into_leaf_after_splitting(LeafPage * leaf, offsetType leafOffset, int64_t key, char * value){
	int i, j, input_idx, split;
	int64_t new_key, rightOffset;

	Record * tmp = (Record*)malloc(sizeof(Record)*(LEAF_ORDER));
	
	input_idx = 0;	
	while(input_idx < leaf->header.numKeys && leaf->records[input_idx].key < key)
		input_idx++;

	for(i=0,j=0; i<leaf->header.numKeys; i++, j++){
		if(j==input_idx) j++;
		tmp[j].key = leaf->records[i].key;
		strcpy(tmp[j].value, leaf->records[i].value);
	}
	tmp[input_idx].key = key;
	strcpy(tmp[input_idx].value, value);

	// Split into two leaf pages.
	split = LEAF_ORDER/2;

	leaf->header.numKeys = 0;
	for(i=0; i<split; i++){
		leaf->records[i].key = tmp[i].key;
		strcpy(leaf->records[i].value, tmp[i].value);
		leaf->header.numKeys++;
	}
	
	// Create new leaf page right after existing one.
	LeafPage * right = (LeafPage*)malloc(PAGE_SIZE);
	right->header.isLeaf = 1;
	right->header.numKeys = 0;
	right->header.parent = leaf->header.parent;
	right->header.right = leaf->header.right;
	for(i=split; i<LEAF_ORDER; i++){
		right->records[i-split].key = tmp[i].key;
		strcpy(right->records[i-split].value, tmp[i].value);
		right->header.numKeys++;
	}
	new_key = right->records[0].key;

	rightOffset = write_new_page((Page*)right);
	leaf->header.right = rightOffset;
	pwrite(fd, leaf, PAGE_SIZE, leafOffset);
	pwrite(fd, right, PAGE_SIZE, rightOffset);
	
	//printf("write on left:%lld right:%lld\n", leafOffset, rightOffset);
	return insert_into_parent((Page*)leaf, leafOffset, new_key, (Page*)right, rightOffset);
}


/* Master insertion function.
 * Inserts a key and an associated value into
 * the B+ tree, causing the tree to be adjusted
 * however necessary to maintain the B+ tree
 * properties.
 * If success, return 0. Otherwise, return non-zero
 */

 int insert(int64_t key, char * value){
 
	 	if(find(key) != NULL){
			printf("duplicate!\n");
			return 0;
		}

		offsetType leafOffset = find_leaf(key);
		LeafPage * leaf = (LeafPage*)malloc(PAGE_SIZE);
		load_page((Page*)leaf, leafOffset);

		if(leaf->header.numKeys < LEAF_ORDER - 1){
		// There is a room for input.
			return insert_into_leaf(leaf, leafOffset, key, value);
		}
		else{
			return insert_into_leaf_after_splitting(leaf, leafOffset, key, value);
		}
 }

/* Master deletion function.
 * Delete the record that has a input key.
 * If success, return 0. Otherwise, return non-zero
 */
int delete(int64_t key){

	LeafPage * leaf = (LeafPage*)malloc(PAGE_SIZE);
	offsetType leafOffset = find_leaf(key);
	load_page((Page*)leaf, leafOffset);

	int i, del_idx=-1;
	for(i=0; i<leaf->header.numKeys; i++)
		if(leaf->records[i].key == key){
			del_idx = i;
			break;
		}

	if(del_idx == -1) return -1;

	for(i=del_idx; i<leaf->header.numKeys-1; i++){
		leaf->records[i].key = leaf->records[i+1].key;
		strcpy(leaf->records[i].value, leaf->records[i+1].value);
	}
	leaf->header.numKeys--;
	pwrite(fd, leaf, PAGE_SIZE, leafOffset);

	free(leaf);
	return 0;
}












