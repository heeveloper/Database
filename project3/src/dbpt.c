#include "dbpt.h"

int cut(int length){
	if(length % 2 == 0)
		return length / 2;
	else
		return (length / 2) + 1;
}


/* Traces the path from the root to a leaf 
 * corrsponding given key and then read the page 
 * from the disk onto the buffer.
 * Return the buffer pointer.
 */

Buffer * find_leaf(int table_id, int64_t key){
	int i;
	Buffer * header_buffer;
	HeaderPage * header_page;
	printf("find_leaf check1\n");
	header_buffer = get_buf(table_id, HEADERPAGE_OFFSET);
	header_page = (HeaderPage*)header_buffer->page;
	printf("find_leaf check2\n");

	Buffer * buffer = get_buf(table_id, header_page->root_page);
	printf("root offset: %ld\n",header_page->root_page);
	InternalPage * c = (InternalPage*)buffer->page;

	printf("find_leaf check2\n");
	while(!c->header.is_leaf){
		//printf("offset: %ld\n",buffer->page_offset);
		release_pincount(buffer);
		i=0;
		while(i < c->header.num_keys){
			if(key >= c->entries[i].key)
				i++;
			else
				break;
		}
		//printf("i:%d\n",i);
		if(i == 0){
			buffer = get_buf(table_id, c->header.one_more_page);
			c = (InternalPage*)buffer->page;
		}
		else{
			buffer = get_buf(table_id, c->entries[i-1].child);
			c = (InternalPage*)buffer->page;
		}
	}
	printf("find_leaf check3\n");

	release_pincount(header_buffer);
	return buffer;
}

/* Find the value corrsponding given key,
 * and return it.
 */
char * find(int table_id, int64_t key){
	int i;
	Buffer * buffer = find_leaf(table_id, key);
	LeafPage * leaf = (LeafPage*)buffer->page;
	release_pincount(buffer);
	if(buffer->page_offset == 0) 
		return NULL;

	for(i = 0; i< leaf->header.num_keys; i++){
		if(leaf->records[i].key == key) break;	
	}
	// Could not find.
	if(i == leaf->header.num_keys)
		return NULL;

	return (char*)leaf->records[i].value;
}

/* Insert Utility functions
 */

/* Insert a new pair of key and value into a node
 * without violating the B+ tree properties.
 */
int insert_into_leaf(Buffer * buffer, int64_t key, char * value){
	printf("insert_into_leaf : %ld\n", key);

	int i, insert_idx;
	LeafPage * leaf = (LeafPage*)buffer->page;

	insert_idx = 0;
	while(insert_idx < leaf->header.num_keys && leaf->records[insert_idx].key < key)
		insert_idx++;

	for(i = leaf->header.num_keys; i > insert_idx; i--){
		leaf->records[i].key = leaf->records[i-1].key;
		strcpy(leaf->records[i].value, leaf->records[i-1].value);
	}
	leaf->records[insert_idx].key = key;
	strcpy(leaf->records[insert_idx].value, value);
	
	leaf->header.num_keys++;

	// Make the updated buffer dirty.
	mark_dirty(buffer);
	release_pincount(buffer);

	return 0;
}

/* Insert a new pair of key and value into a node
 * so as to the node's size to exceed the order,
 * causing the node to split into two.
 */

int insert_into_leaf_after_splitting(int table_id, Buffer * buf, int64_t key, char * value){
	
	printf("insert_into_leaf_after_splitting: %ld\n", key);
	
	int insert_idx, split, new_key, i, j;
	LeafPage * leaf, *new_leaf;
	HeaderPage * header_page;
	Buffer * new_buf;
	Buffer * header_buf;

	Record tmp[LEAF_ORDER];

	header_buf = get_buf(table_id, HEADERPAGE_OFFSET);
	header_page = (HeaderPage*)header_buf->page;
	release_pincount(header_buf);

	new_buf = get_buf(table_id, header_page->free_page);
	new_leaf = (LeafPage*)new_buf->page;

	insert_idx = 0;
	leaf = (LeafPage*)buf->page;

	while(insert_idx < LEAF_ORDER - 1 && leaf->records[insert_idx].key < key)
		insert_idx++;

	for(i=0, j=0; i<leaf->header.num_keys; i++, j++){
		if(j == insert_idx) j++;
		tmp[j].key = leaf->records[i].key;
		strcpy(tmp[j].value, leaf->records[i].value);
	}

	tmp[insert_idx].key = key;
	strcpy(tmp[insert_idx].value, value);

	leaf->header.num_keys = 0;
	new_leaf->header.num_keys = 0;

	split = cut(LEAF_ORDER);

	for(i=0; i<split; i++){
		leaf->records[i].key = tmp[i].key;
		strcpy(leaf->records[i].value, tmp[i].value);
		leaf->header.num_keys++;
	}

	for(i = split, j = 0; i < LEAF_ORDER; i++, j++){
		new_leaf->records[j].key = tmp[i].key;
		strcpy(new_leaf->records[j].value, tmp[i].value);
		new_leaf->header.num_keys++;
	}

	leaf->header.right = new_buf->page_offset;
	new_leaf->header.parent = leaf->header.parent;
	new_leaf->header.is_leaf = true;
	new_key = new_leaf->records[0].key;

	// Make the updated buffers dirty.
	mark_dirty(buf);
	mark_dirty(new_buf);

	printf("new_key: %d\n",new_key);
	return insert_into_parent(table_id, buf, new_key, new_buf);
}

/* Used in insert_into_parent,
 * find out the index of entry of parent which points the page 
 * corrsponding to the given buffer.
 */

int get_left_index(Buffer * parent_buf, Buffer * left_buf){
	
	InternalPage * parent_page;
	int left_idx;
	parent_page = (InternalPage*)parent_buf->page;

	if(parent_page->header.one_more_page == left_buf->page_offset)
		return 0;

	left_idx = 0;
	while(left_idx <= parent_page->header.num_keys && parent_page->entries[left_idx].child != left_buf->page_offset)
		left_idx++;

	return left_idx + 1;
}

/* Insert a entry to newly allocated node into its parent.
 */

int insert_into_parent(int table_id, Buffer * left_buf, int64_t key, Buffer * right_buf){
	int left_idx;
	InternalPage * parent, *left;
	HeaderPage * hp;
	Buffer * b, *hb;
	printf("left_buf : %ld right_buf : %ld\n",left_buf->page_offset, right_buf->page_offset);
	left = (InternalPage*)left_buf->page;
	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (HeaderPage*)hb->page;
	release_pincount(hb);

	if(left_buf->page_offset == hp->root_page)
		return insert_into_new_root(table_id, left_buf, key, right_buf);

	b = get_buf(table_id, left->header.parent);
	left_idx = get_left_index(b, left_buf);
	release_pincount(left_buf);

	printf("left idx: %d\n", left_idx);

	parent = (InternalPage*)b->page;
	if(parent->header.num_keys < INTERNAL_ORDER - 1)
		return insert_into_internal(b, left_idx, key, right_buf);

	return insert_into_internal_after_splitting(table_id, b, left_idx, key, right_buf);
}

/* Create a new root to take given two nodes
 * and insert the appropriate key into the new root node.
 */

int insert_into_new_root(int table_id, Buffer * left_buf, int64_t key, Buffer * right_buf){
	
	printf("insert_into_new_root : %ld\n", key);

	Buffer * root_buf, *header_buf;
	InternalPage * root, *left, *right;
	HeaderPage * header_page;

	header_buf = get_buf(table_id, HEADERPAGE_OFFSET);
	header_page = (HeaderPage*)header_buf->page;

	root_buf = get_buf(table_id, header_page->free_page);
	root = (InternalPage*)root_buf->page;

	left = (InternalPage*)left_buf->page;
	right = (InternalPage*)right_buf->page;

	root->header.one_more_page = left_buf->page_offset;
	root->entries[0].key = key;
	root->entries[0].child = right_buf->page_offset;
	root->header.num_keys++;
	root->header.parent = 0;
	//root->header.is_leaf = false;
	
	left->header.parent = root_buf->page_offset;
	right->header.parent = root_buf->page_offset;
	header_page->root_page = root_buf->page_offset;

	// Make the updated buffers dirty.
	mark_dirty(header_buf);
	mark_dirty(root_buf);
	mark_dirty(left_buf);
	mark_dirty(right_buf);

	release_pincount(header_buf);
	release_pincount(root_buf);
	release_pincount(left_buf);
	release_pincount(right_buf);

	return 0;
}

/* Called in insert_into_leaf if there is a room in its parent,
 * just insert the pair of key and value into the appropriate
 * internal node.
 */
int insert_into_internal(Buffer * parent_buf, int insert_idx, int64_t key, Buffer * right_buf){
	
	printf("insert_into_internal : %ld\n", key);
	
	int i;
	InternalPage * parent_page;
	parent_page = (InternalPage*)parent_buf->page;

	for(i = parent_page->header.num_keys; i > insert_idx; i--){
		parent_page->entries[i].key = parent_page->entries[i-1].key;
		parent_page->entries[i].child = parent_page->entries[i-1].child;
	}
	
	parent_page->entries[insert_idx].key = key;
	parent_page->entries[insert_idx].child = right_buf->page_offset;
	parent_page->header.num_keys++;

	// Make the updated buffer dirty.
	mark_dirty(parent_buf);

	release_pincount(parent_buf);
	release_pincount(right_buf);

	return 0;
}

/* Called in insert_into_parent if there is no room 
 * in its parent so as to exceed the internal order,
 * causing the page split into two node.
 */
int insert_into_internal_after_splitting(int table_id, Buffer * parent_buf, int insert_idx, int64_t key, Buffer * right_child_buf){
	
	int i, j, split, k_prime;
	HeaderPage * header_page;
	InternalPage * parent_page, * new_page, * new_child_page;
	Buffer * header_buf, * new_buf, * new_child_buf;
	Entry tmp[INTERNAL_ORDER];

	header_buf = get_buf(table_id, HEADERPAGE_OFFSET);
	header_page = (HeaderPage*)header_buf->page;
	release_pincount(header_buf);

	parent_page = (InternalPage*)parent_buf->page;
	printf("parent page: %ld\n",parent_page->entries[0].key);
	for(i = 0, j = 0; i < parent_page->header.num_keys; i++, j++){
		if(j == insert_idx) j++;
		tmp[j].key = parent_page->entries[i].key;
		tmp[j].child = parent_page->entries[i].child;
	}
	tmp[insert_idx].key = key;
	tmp[insert_idx].child = right_child_buf->page_offset;

	split = cut(INTERNAL_ORDER);

	new_buf = get_buf(table_id, header_page->free_page);
	new_page = (InternalPage*)new_buf->page;
	new_page->header.num_keys = 0;
	parent_page->header.num_keys = 0;

	for(i = 0; i < split; i++){
		printf("%d ",i);
		parent_page->entries[i].key = tmp[i].key;
		parent_page->entries[i].child = tmp[i].child;
		parent_page->header.num_keys++;
	}

	new_page->header.one_more_page = tmp[i].child;
	k_prime = tmp[i].key;

	for(++i, j = 0; i < INTERNAL_ORDER; i++, j++){
		new_page->entries[j].key = tmp[i].key;
		new_page->entries[j].child = tmp[i].child;
		new_page->header.num_keys++;
	}

	new_page->header.parent = parent_page->header.parent;
	
	new_child_buf = get_buf(table_id, new_page->header.one_more_page);
	new_child_page = (InternalPage*)new_child_buf->page;
	new_child_page->header.parent = new_buf->page_offset;
	mark_dirty(new_child_buf);
	release_pincount(new_child_buf);

	for(i = 0; i < new_page->header.num_keys; i++){
		new_child_buf = get_buf(table_id, new_page->entries[i].child);
		new_child_page = (InternalPage*)new_child_buf->page;
		new_child_page->header.parent = new_buf->page_offset;
		mark_dirty(new_child_buf);
		release_pincount(new_child_buf);
	}

	mark_dirty(parent_buf);
	mark_dirty(new_buf);

	release_pincount(right_child_buf);
	
	printf("k_prime : %d\n",k_prime);
	return insert_into_parent(table_id, parent_buf, k_prime, new_buf);
}

/* Master insertion function.
 * Inserts a key and an associated value into
 * the B+ tree, causing the tree to be adjusted
 * however necessary to maintain the B+ tree
 * properties.
 */

int insert(int table_id, int64_t key, char * value){
	Buffer * buffer;
	LeafPage * leaf;

	// duplicate
	if(find(table_id, key) != NULL){
		printf("Duplicate\n");
		return 0;
	}
	
	buffer = find_leaf(table_id, key);
	leaf = (LeafPage*)buffer->page;

	// Case : leaf has room for key.
	if(leaf->header.num_keys < LEAF_ORDER -1){
		return insert_into_leaf(buffer, key, value);
	}

	// Case : leaf has no room for key.
	return insert_into_leaf_after_splitting(table_id, buffer, key, value);
}

/* Deletion Utility functions.
 */

/* Remove record or entry in the page containing given key.
 */
Buffer * remove_entry_from_page(Buffer * buf, int64_t key){
	printf("remove entry from page: %ld\n",key);
	int i;
	InternalPage * c;
	LeafPage * leaf;

	c = (InternalPage*)buf->page;

	// Case : Deleted page is leaf.
	if(c->header.is_leaf){
		leaf = (LeafPage*)buf->page;

		// Remove the record corrsponding to the given key,
		// then shift other records.
		i=0;
		while(i < leaf->header.num_keys && leaf->records[i].key != key)
			i++;

		for(++i; i < leaf->header.num_keys; i++){
			leaf->records[i-1].key = leaf->records[i].key;
			strcpy(leaf->records[i-1].value, leaf->records[i].value);
		}
		leaf->header.num_keys--;
		mark_dirty(buf);
	}
	// Case : Deleted page is internal.
	else{
		i=0;
		while(c->entries[i].key != key)
			i++;
		for(++i; i < c->header.num_keys; i++){
			c->entries[i - 1].key = c->entries[i].key;
			c->entries[i - 1].child = c->entries[i].child;
		}
		c->header.num_keys--;
		mark_dirty(buf);
	}

	return buf;
}

/* Called in delete_entry if the deleted page is root.
 */
int adjust_root(int table_id, Buffer * buf){
	printf("adjust_root\n");
	Buffer * new_root_buf, * header_buf;
	InternalPage * new_root;
	HeaderPage * header_page;
	InternalPage * root_page;

	root_page = (InternalPage*)buf->page;

	/* Case : If the root page containing deleted entry 
	 * is nonempty, just remain.
	 */
	if(root_page->header.num_keys > 0){
		release_pincount(buf);
		return 0;
	}

	/* Case : Empty root
	 */

	/* If empty root has a child which must be only and
	 * the leftmost child page of root, promote the
	 * child as new root.
	 */
	header_buf = get_buf(table_id, HEADERPAGE_OFFSET);
	header_page = (HeaderPage*)header_buf->page;

	if(!root_page->header.is_leaf){
		header_page->root_page = root_page->header.one_more_page;
		new_root_buf = get_buf(table_id, root_page->header.one_more_page);
		new_root = (InternalPage*)new_root_buf->page;
		new_root->header.parent = 0;
		buf->page_offset = -1;

		mark_dirty(header_buf);
		mark_dirty(buf);
		mark_dirty(new_root_buf);

		release_pincount(header_buf);
		release_pincount(buf);
		release_pincount(new_root_buf);
	}

	return 0;
}

/* Return the index of neighbor page of given page Buffer
 * in its parent page.
 */
int get_neighbor_index(int table_id, Buffer * buf){
	int i;
	InternalPage * c, * parent_page;
	Buffer * parent_buf;

	c = (InternalPage*)buf->page;
	parent_buf = get_buf(table_id, c->header.parent);
	parent_page = (InternalPage*)parent_buf->page;
	release_pincount(parent_buf);

	if(parent_page->header.one_more_page == buf->page_offset)
		return -2;

	for(i = 0; i <= parent_page->header.num_keys; i++)
		if(parent_page->entries[i].child == buf->page_offset)
			return i - 1;
}

/* Coalesce a page with its neighbor page if the 
 * number of left records or entries is too small
 * to remain and the sum of both is larger than 
 * given order after deletion.
 */
int coalesce_pages(int table_id, Buffer * buf, Buffer * neighbor_buf, int neighbor_idx, int64_t k_prime){
	printf("coalesce pages k_prime: %ld\n", k_prime);
	int i, j, neighbor_insert_idx, neighbor_end_idx;
	HeaderPage * header_page;
	InternalPage * ci, *ni, *child_page;
	LeafPage * cl, *nl;
	Buffer * tmp, * child_buf, *parent_buf, *header_buf;

	header_buf = get_buf(table_id, HEADERPAGE_OFFSET);
	header_page = (HeaderPage*)header_buf->page;

	/* If the page is the leftmost child of its parent 
	 * and its neighbor page is right to it,
	 * swap these positions.
	 */
	if(neighbor_idx == -2){
		tmp = buf;
		buf = neighbor_buf;
		neighbor_buf = tmp;
	}

	ni = (InternalPage*)neighbor_buf->page;
	parent_buf = get_buf(table_id, ni->header.parent);
	
	neighbor_insert_idx = ni->header.num_keys;

	/* Case : Coalesced pages are internal page.
	 * Append k_prime and the following page to neighbor
	 * and update its childs' parent pointer.
	 */
   if(!ni->header.is_leaf){
	   ci = (InternalPage*)buf->page;
	   ni->entries[neighbor_insert_idx].key = k_prime;
	   ni->entries[neighbor_insert_idx].child = ci->header.one_more_page;
	   ni->header.num_keys++;

	   neighbor_end_idx = ci->header.num_keys;

	   for(i = neighbor_insert_idx + 1, j = 0; j < neighbor_end_idx; i++, j++){
		   ni->entries[i].key = ci->entries[j].key;
		   ni->entries[i].child = ci->entries[j].child;
		   ni->header.num_keys++;
		   ci->header.num_keys--;
	   }

	   child_buf = get_buf(table_id, ni->header.one_more_page);
	   child_page = (InternalPage*)child_buf->page;
	   child_page->header.parent = neighbor_buf->page_offset;
	   mark_dirty(child_buf);
	   release_pincount(child_buf);
	   for(i = 0; i < ni->header.num_keys; i++){
		   child_buf = get_buf(table_id, ni->entries[i].child);
		   child_page = (InternalPage*)child_buf->page;
		   child_page->header.parent = neighbor_buf->page_offset;
		   mark_dirty(child_buf);
		   release_pincount(child_buf);
	   }

	   ci->header.parent = 0;
	   header_page->num_pages--;
	   buf->page_offset = 0;
   }
   /* Case : Coalesced pages are leaf page.
	* Append records to neighbor page.
	*/
   else{
		cl = (LeafPage*)buf->page;
		nl = (LeafPage*)neighbor_buf->page;

		for(i = neighbor_insert_idx, j = 0; j < cl->header.num_keys; i++, j++){
			nl->records[i].key = cl->records[j].key;
			strcpy(nl->records[i].value, cl->records[j].value);
			nl->header.num_keys++;
		}
		nl->header.right = cl->header.right;

		cl->header.parent = 0;
		header_page->num_pages--;
		buf->page_offset = 0;
   }   

   mark_dirty(header_buf);
   mark_dirty(buf);
   mark_dirty(neighbor_buf);

   release_pincount(header_buf);
   release_pincount(buf);
   release_pincount(neighbor_buf);

   return delete_entry(table_id, parent_buf, k_prime);
}

/* Redistribute a page with its neighbor page if the 
 * number of left records or entries is too small
 * to remain and the sum of both is less or eqaul 
 * than given order after deletion.
 */
int redistribute_pages(int table_id, Buffer * buf, Buffer * neighbor_buf, int neighbor_idx, int k_prime_idx, int k_prime){
	printf("redistribute pages k_prime: %d\n", k_prime);
	int i;
	InternalPage * ci, *ni, *parent_page;
	LeafPage *cl, *nl;
	Buffer * tmp, *parent_buf;

	ci = (InternalPage*)buf->page;
	parent_buf = get_buf(table_id, ci->header.parent);
	parent_page = (InternalPage*)parent_buf->page;

	/* Case : Given page's neighbor page is left to the page.
	 * Move the neighbor pages' rightmost record or entry 
	 * to the leftmost of the given page.
	 */
	if(neighbor_idx != -2){
		// Case : pages corrsponding to given buffers are leaf.
		if(ci->header.is_leaf){
			cl = (LeafPage*)buf->page;
			nl = (LeafPage*)neighbor_buf->page;

			for(i = cl->header.num_keys; i > 0; i--){
				cl->records[i].key = cl->records[i - 1].key;
				strcpy(cl->records[i].value, cl->records[i - 1].value);
			}
			cl->records[0].key = nl->records[nl->header.num_keys - 1].key;
			strcpy(cl->records[0].value, nl->records[nl->header.num_keys - 1].value);
			
			parent_page->entries[k_prime_idx].key = cl->records[0].key;
			
			nl->records[nl->header.num_keys - 1].key = -1;
			cl->header.num_keys--;
			nl->header.num_keys++;
		}
		// Case : pages corrsponding to given buffers are internal.
		else{
			ni = (InternalPage*)neighbor_buf->page;
			for(i = cl->header.num_keys; i > 0; i--){
				ci->entries[i].key = ci->entries[i - 1].key;
				ci->entries[i].child = ci->entries[i - 1].child;
			}
			ci->entries[0].key = k_prime;
			ci->entries[0].child = ci->header.one_more_page;
			ci->header.one_more_page = ni->entries[ni->header.num_keys - 1].child;
			parent_page->entries[k_prime_idx].key = ni->entries[ni->header.num_keys - 1].key;

			ci->header.num_keys++;
			ni->header.num_keys--;
		}
	}
	/* Case : Given page's neighbor page is right to the page.
	 * Move the neighbor pages' leftmost record or entry
	 * to the rightmost of the given page.
	 */
	else{
		// Case : pages corrsponding to given buffers are leaf.
		if(ci->header.is_leaf){
			cl = (LeafPage*)buf->page;
			nl = (LeafPage*)neighbor_buf->page;

			cl->records[cl->header.num_keys].key = nl->records[0].key;
			strcpy(cl->records[cl->header.num_keys].value, nl->records[0].value);

			parent_page->entries[k_prime_idx].key = nl->records[1].key;

			for(i = 0; i < nl->header.num_keys - 1; i++){
				nl->records[i].key = nl->records[i + 1].key;
				strcpy(nl->records[i].value, nl->records[i + 1].value);
			}
			cl->header.num_keys++;
			nl->header.num_keys--;
		}
		// Case : pages corrsponding to given buffers are internal.
		else{
			ni = (InternalPage*)neighbor_buf->page;
			ci->entries[ci->header.num_keys].key = k_prime;
			ci->entries[ci->header.num_keys].child = ni->header.one_more_page;
			parent_page->entries[k_prime_idx].key = ni->entries[0].key;
			ni->header.one_more_page = ni->entries[0].child;

			for(i = 0; i < ni->header.num_keys - 1; i++){
				ni->entries[i].key = ni->entries[i + 1].key;
				ni->entries[i].child = ni->entries[i + 1].child;
			}
			ci->header.num_keys++;
			ni->header.num_keys--;
		}
	}

	mark_dirty(buf);
	mark_dirty(neighbor_buf);
	mark_dirty(parent_buf);

	release_pincount(buf);
	release_pincount(neighbor_buf);
	release_pincount(parent_buf);

	return 0;
}

/* Remove record in the leaf page corrsponding to
 * given key, then delete entry to this to preserve
 * the B+ three properties.
 */
int delete_entry(int table_id, Buffer * buf, int64_t key){
	printf("delete_entry: %ld\n", key);
	
	int minimum;
	int neighbor_idx, k_prime_idx, capacity;
	int64_t k_prime, nb_offset;
	Buffer * nb, * pb, * hb;
	InternalPage * internal, * parent, * neighbor;
	HeaderPage * hp;

	// Remove key and value from the page.
	buf = remove_entry_from_page(buf, key);
	
	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (HeaderPage*)hb->page;
	release_pincount(hb);
	
	/* Case : Deleted node is root.
	 */
	if(buf->page_offset == hp->root_page)
		return adjust_root(table_id, buf);

	/* Case : Deleted node is below the root.
	 */

	internal = (InternalPage*)buf->page;

	/* Set the minimum size of page possble to maintain
	 * B+ tree properties.
	 */
	minimum = internal->header.is_leaf ? cut(LEAF_ORDER - 1) : cut(INTERNAL_ORDER - 1) - 1;

	/* Case : Deleted page's number of keys is allowable,
	 */
	if(internal->header.num_keys >= minimum){
		release_pincount(buf);
		return 0;
	}

	/* Case : Deleted page's number of keys is less than minimum.
	 * Coalescence or redistribute is needed.
	 */

	pb = get_buf(table_id, internal->header.parent);
	parent = (InternalPage*)pb->page;
	release_pincount(pb);

	neighbor_idx = get_neighbor_index(table_id, buf);
	k_prime_idx = neighbor_idx == -2 ? 0 : neighbor_idx + 1;
	k_prime = parent->entries[k_prime_idx].key;

	if(neighbor_idx == -2){
		nb_offset = parent->entries[0].child;
	}
	else if(neighbor_idx == -1){
		nb_offset = parent->header.one_more_page;
		
	}
	else{
		nb_offset = parent->entries[neighbor_idx].child;
	}

	nb = get_buf(table_id, nb_offset);
	neighbor = (InternalPage*)nb->page;
	capacity = internal->header.is_leaf ? LEAF_ORDER - 1 : INTERNAL_ORDER - 1;

	printf("n.num: %d, i.num: %d\n",neighbor->header.num_keys, internal->header.num_keys);
	// Coalescence
	if(neighbor->header.num_keys + internal->header.num_keys <= capacity)
		return coalesce_pages(table_id, buf, nb, neighbor_idx, k_prime);

	// Redistribution.
	return redistribute_pages(table_id, buf, nb, neighbor_idx, k_prime_idx, k_prime);
}

/* Master deletion function.
 */
int delete(int table_id, int64_t key){
	Buffer * buf;

	if(find(table_id, key) == NULL){
		printf("It does not exist.\n");
		return 0;
	}

	buf = find_leaf(table_id, key);
	return delete_entry(table_id, buf, key);
}
