/*
 * Copyright (C) 2017 Heejun Lee.
 * All rights reserved.
 */

#ifndef __DBPT_H__
#define __DBPT_H__

#include "page.h"

int64_t find_leaf(int64_t key);
char * find(int64_t key);
//int delete(int64_t key);

int get_left_idx(InternalPage * page, offsetType offset);

int insert(int64_t key, char * value);
int insert_into_leaf(LeafPage * leaf, offsetType leafOffset, int64_t key, char * value);
int insert_into_new_root(Page * left, int64_t leftOffset, int key, Page * right, int64_t rightOffset);
int insert_into_internal(InternalPage * page, offsetType pageOffset, int left_idx, int64_t key, offsetType rightOffset);
int insert_into_internal_after_splitting(InternalPage * page, offsetType pageOffset, int left_idx, int key, offsetType rightOffset);
int insert_into_parent(Page * left, offsetType leftOffset, int64_t key, Page * right, offsetType rightOffset);
int insert_into_leaf_after_splitting(LeafPage * leaf, offsetType leafOffset, int64_t key, char * value);

#endif
