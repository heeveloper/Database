#include "dbpt.h"

#define NUM_COMMAND	1000

int main() {

	int i;
	int num_buf = 16;
	int table_id;
	char string[120];
	char* file = "test.db";
	char* string_set[3] = {
	"val1",
	"val2",
	"va13"
	};
	if (init_db(num_buf) != 0) {
		printf("init_db() error!\n");
		return 0;
	}

	if((table_id = open_table(file)) <= 0) {
		printf("open_db() error!!\n");
		return 0;
	}


	// insert
	
	for (i = 0; i < NUM_COMMAND; i++) {
		if (insert(table_id, i, string_set[i%3])){
			printf("insert(%d) error!\n", i);
		}
		printf("insert(%d, %s)\n", i, string_set[i%3]);
	}


	// find
	for (i = 0; i < NUM_COMMAND; i++) {
		if (find(table_id ,i) == NULL){
			printf("find(%d) fail!\n", i);
			continue;
		}
		strcpy(string, find(table_id, i));
		printf("find(%d) : %s \n", i, string);
	}
	
	// delete
	for (i = 0; i < NUM_COMMAND; i++) {
		if (delete(table_id, i)){
			printf("delete(%d) error!\n", i);
			continue;
		}
		printf("delete(%d) \n", i);
	}	
	
	// find
	for (i = 0; i < NUM_COMMAND; i++) {
		if (find(table_id, i) == NULL){
			printf("find(%d) fail!\n", i);
			continue;
		}
		strcpy(string, find(table_id, i));
		printf("find(%d) : %s \n", i, string);
	}

	if (close_table(table_id) != 0) {
		printf("close_table() error!!!\n");
		return 0;
	}

	return 0;
}
