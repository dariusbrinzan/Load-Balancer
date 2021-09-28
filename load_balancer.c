/* Copyright 2021 <> */
#include <stdlib.h>
#include <string.h>

#include "load_balancer.h"
#include "utils.h"

#define MAX_SERVERS 3

// hash function for unsigned integer
unsigned int hash_function_servers(void *a) {
    unsigned int uint_a = *((unsigned int *)a);

    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = (uint_a >> 16u) ^ uint_a;
    return uint_a;
}

// hash function for string
unsigned int hash_function_key(void *a) {
    unsigned char *puchar_a = (unsigned char *) a;
    unsigned int hash = 5381;
    int c;

    while ((c = *puchar_a++))
        hash = ((hash << 5u) + hash) + c;

    return hash;
}

// functia pentru realizarea unui load_balancer
load_balancer* init_load_balancer() {
    load_balancer* lb = malloc(sizeof(load_balancer));
    DIE(lb == NULL, "couldn't initialize load balancer");
    lb->size = 0;
    return lb;
}
// functie care realizeaza o cautare binara
int binary_search(const load_balancer* main, const unsigned int hash) {
    int left = 0;
    int right = main->size - 1;
    while (left <= right) {
        int mid = (left + right) / 2;  // can't overflow bcs MAX SIZE is 3e+5
        if (hash < main->hashring[mid].hash) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    return left;
}
// functia de load
void loader_store(load_balancer* main, char* key, char* value, int* server_id) {
    int ind = binary_search(main, hash_function_key(key));
    if (ind == main->size) { ind = 0; }
    *server_id = main->hashring[ind].server_id;
    server_store(main->hashring[ind].hashtable, key, value);
}
// functia de retrieve
char* loader_retrieve(load_balancer* main, char* key, int* server_id) {
    int ind = binary_search(main, hash_function_key(key));
    if (ind == main->size) { ind = 0; }
    *server_id = main->hashring[ind].server_id;
	return server_retrieve(main->hashring[ind].hashtable, key);
}
// functia de add
void loader_add_server(load_balancer* main, int server_id) {
    server_memory* hashtable = init_server_memory();
    for (int i = 0; i < MAX_SERVERS; ++i) {
        unsigned int tag = i * MAX + server_id;
        unsigned int hash_server = hash_function_servers(&tag);
        int ind = binary_search(main, hash_server);

        // shifting hashring to right to create space for new server
        for (int j = main->size; j > ind; --j) {
            main->hashring[j] = main->hashring[j - 1];
        }
        main->hashring[ind].hashtable = hashtable;
        main->hashring[ind].hash = hash_server;
        main->hashring[ind].server_id = server_id;
        ++main->size;

        if (main->size == 1) { continue; }

        int ind_next = (ind + 1 == main->size) ? 0 : ind + 1;
        server_memory* server = main->hashring[ind_next].hashtable;
        for (int j = 0; j < CAPACITY; ++j) {
            ht_node* head = server->buckets[j];
            while (head) {
                unsigned int hash_item = hash_function_key(head->item->key);
                int ind2 = binary_search(main, hash_item);
                if (ind2 == main->size) { ind2 = 0; }
                if (main->hashring[ind2].server_id == server_id) {
                    server_store(hashtable, head->item->key, head->item->value);
                    char* key_to_be_deleted = head->item->key;
                    head = head->next;
                    server_remove(server, key_to_be_deleted);
                    continue;
                }
                head = head->next;
            }
        }
    }
}

// functia de remove a unui server
void loader_remove_server(load_balancer* main, int server_id) {
    server_memory* memory_ptr = NULL;
    for (int cnt = 0, i = main->size - 1; i >= 0; --i) {
        if (main->hashring[i].server_id == server_id) {
            memory_ptr = main->hashring[i].hashtable;
            memmove(main->hashring + i,
                    main->hashring + i + 1,
                    (main->size - i - 1) * sizeof(main->hashring[0]));
            --main->size;
            if (++cnt == MAX_SERVERS) { break; }
        }
    }
    DIE(memory_ptr == NULL, "couldn't remove server, it doesn't exist");
    for (int i = 0; i < CAPACITY; ++i) {
        for (ht_node* head = memory_ptr->buckets[i]; head; head = head->next) {
            int ind = binary_search(main, hash_function_key(head->item->key));
            if (ind == main->size) { ind = 0; }
            server_store(main->hashring[ind].hashtable,
            head->item->key, head->item->value);
        }
    }

    free_server_memory(memory_ptr);
}
// functia de free a balancerului
void free_load_balancer(load_balancer* main) {
    char* is_deleted = calloc(MAX, 1);
    DIE(is_deleted == NULL,
    "free load balancer error, calloc call returned NULL");
    for (int i = 0; i < main->size; ++i) {
        if (!is_deleted[main->hashring[i].server_id]) {
            free_server_memory(main->hashring[i].hashtable);
            is_deleted[main->hashring[i].server_id] = 1;
        }
    }
    free(is_deleted);
    free(main);
}
