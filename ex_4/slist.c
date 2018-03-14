//
//  list.c
//  ex_1
//
//  Created by Eliyah Weinberg on 6.11.2017.
//  Copyright © 2017 Eliyah Weinberg. All rights reserved.
//

#include "slist.h"
#include <stdlib.h>
#include <stdio.h>

#define SUCESS 0
#define FAILURE -1

/*-----------------------PRIVATE FUNCTIONS DECLARATION------------------------*/

/** Create single node for linked list
 \param data - the data to place in the list
 \allocates memory
 \return a pointer to the node, or NULL if failed */
slist_node_t *create_node(void* data);

/** Checking that argument isn't NULL
 \param arg for argument to check
 \return 0 if argument is not NULL otherwise -1 */
int is_not_null(void *arg);

/*----------------------------------------------------------------------------*/

/** Initialize a single linked list
 \param list - the list to initialize */
void slist_init(slist_t * list){
    if (is_not_null(list) == FAILURE)
        return;

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

/** Destroy and de-allocate the memory hold by a list
 \param list - a pointer to an existing list
 \param state flag that indicates whether stored data should also be de-allocated */
void slist_destroy(slist_t *list,slist_destroy_t state){
    if (is_not_null(list) == FAILURE)
        return;

    slist_node_t *node = list->head;
    slist_node_t *next;
    for ( ;node != NULL; node = next) {
        if (state == SLIST_FREE_DATA) {
            if (slist_data(node) != NULL)
            free(slist_data(node));
        }

        next = slist_next(node);
        free(node);
        list->size--;
    }
    list->head = list->tail = NULL;


}

/** Pop the first element in the list
 \param list - a pointer to a list
 \return a pointer to the data of the element, or NULL if the list is empty */
void *slist_pop_first(slist_t *list){
    if (is_not_null(list) == FAILURE)
        return NULL;

    slist_node_t *head_to_pop;
    void *data;
    if (slist_size(list) == 0) {
        return NULL;
    }
    head_to_pop = slist_head(list);
    if (slist_size(list) == 1){
        list->head = NULL;
        list->tail = NULL;
    } else
        list->head = head_to_pop->next;

    data = head_to_pop->data;

    free(head_to_pop);
    list->size--;

    return data;
}

/** Append data to list (add as last node of the list)
 \param list - a pointer to a list
 \param data - the data to place in the list
 \return 0 on success, or -1 on failure */
int slist_append(slist_t *list,void *data){
    if (is_not_null(list) == FAILURE)
        return FAILURE;

    slist_node_t *new_node = create_node(data);
    if (new_node == NULL)  //node allocation failured
        return FAILURE;

    if (slist_size(list) == 0)
        list->head = new_node;
    else
        list->tail->next = new_node;

    list->tail = new_node;
    list->size++;
    return SUCESS;
}

/** Prepend data to list (add as first node of the list)
 \param list - a pointer to list
 \param data - the data to place in the list
 \return 0 on success, or -1 on failure
 */
int slist_prepend(slist_t *list,void *data){
    if (is_not_null(list) == FAILURE)
        return FAILURE;

    slist_node_t *new_node = create_node(data);
    if (new_node == NULL)  //node allocation failured
        return FAILURE;

    if (slist_size(list) == 0)
        list->tail = new_node;
    else
        new_node->next = list->head;
    list->head = new_node;
    list->size++;
    return SUCESS;
}

/** \brief Append elements from the second list to the first list, use the slist_append function.
 you can assume that the data of the lists were not allocated and thus should not be deallocated in destroy
 (the destroy for these lists will use the SLIST_LEAVE_DATA flag)
 \param to a pointer to the destination list
 \param from a pointer to the source list
 \return 0 on success, or -1 on failure
 */
int slist_append_list(slist_t *to, slist_t *from){
    if ((is_not_null(to) == FAILURE) || (is_not_null(from) == FAILURE) )
        return FAILURE;

    slist_node_t *source;
    int flag = SUCESS;
    //condition of two empty list appending or source list empty
    if ((slist_size(to) == 0 && slist_size(from)==0)
        || slist_size(from)==0)
        return SUCESS;
    source = slist_head(from);
    for (; source != NULL && flag != FAILURE; source = slist_next(source)) {
        flag = slist_append(to, slist_data(source));
    }
    return flag;
}

//----------------------------------------------------------------------------//
slist_node_t *create_node(void* data){
    slist_node_t *node = (slist_node_t*)malloc(sizeof(slist_node_t));
    node->data = data;
    node->next = NULL;
    return node;
}

//----------------------------------------------------------------------------//
int is_not_null(void *arg){
    if (arg == NULL)
        return FAILURE;

    return SUCESS;
}


