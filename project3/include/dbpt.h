/*
 * 		@author Heejun Lee (zaqq1414@gmail.com)
 * 		@file	dbpt.h
 * 		@brief	Disk-based B+ tree with buffer
 * 		@since	2017-11-09
 */

#ifndef __DBPT_H
#define __DBPT_H

#include "page.h"

int cut(int length);

// FIND 
Buffer * find_leaf(int table_id, int64_t key);
char * find(int table_id, int64_t key);

// Insert
int insert(int table_id, int64_t key, char * value);
int get_left_index(Buffer * buffer, Buffer * left_buffer);
int insert_into_new_root(int table_id, Buffer * left_buf, int64_t key, Buffer * right_buf);
int insert_into_leaf(Buffer * buffer, int64_t key, char * value);
int insert_into_leaf_after_splitting(int table_id, Buffer * buffer, int64_t key, char * value);
int insert_into_parent(int table_id, Buffer * left_buffer, int64_t key, Buffer * right_buffer);
int insert_into_internal(Buffer * buffer, int left_idx, int64_t key, Buffer * right_buffer);
int insert_into_internal_after_splitting(int table_id, Buffer * buffer, int left_idx, int64_t key, Buffer * right_buffer);

// Delete
int delete(int table_id, int64_t key);
int delete_entry(int table_id, Buffer * buffer, int64_t key);
Buffer * remove_entry_from_page(Buffer * buffer, int64_t key);
int adjust_root(int table_id, Buffer * buffer);
int get_neighbor_index(int table_id, Buffer * buffer);
int coalesce_pages(int table_id, Buffer * buffer, Buffer * neighbor_buffer, int neighbor_index, int64_t k_prime);
int redistribute_pages(int table_id, Buffer * buffer, Buffer * neighbor_buffer, int neighbor_index, int k_prime_index, int k_prime);

#endif
