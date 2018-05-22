#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"
#include <memory.h>

/*
 * definition of error codes
 *
 */

#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
#define DPLIST_INVALID_ERROR 2 //error due to a list operation applied on a NULL list

#ifdef DEBUG
#define DEBUG_PRINTF(...) 									         \
        do {											         \
            fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	 \
            fprintf(stderr,__VA_ARGS__);								 \
            fflush(stderr);                                                                          \
                } while(0)
#else
#define DEBUG_PRINTF(...) (void)0
#endif


#define DPLIST_ERR_HANDLER(condition, err_code)\
    do {                                    \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");    \
            assert(!(condition));                                    \
        } while(0)

/*
 * The real definition of struct node
 */

struct dplist_node {
    dplist_node_t *prev, *next;
    void *element;
};

/*
 * The real definition of struct list
 */

struct dplist {
    dplist_node_t *head;

    void *(*element_copy)(void *src_element);

    void (*element_free)(void **element);

    int (*element_compare)(void *x, void *y);
};

dplist_t *dpl_sort(dplist_t *list);

// Returns a pointer to a newly-allocated and initialized list.

dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_MEMORY_ERROR);
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element) {
    // Every list node of the list needs to be deleted (free memory)
    // If free_element == true : call element_free() on the element of the list node to remove
    // If free_element == false : don't call element_free() on the element of the list node to remove
    // The list itself also needs to be deleted (free all memory)
    // '*list' must be set to NULL.
    // Extra error handling: use assert() to check if '*list' is not NULL at the start of the function.

    assert(*list != NULL);

    while ((*list)->head != NULL) {
        // Delete all the nodes untill the list is empty
        //free(dpl_get_reference_at_index(*list,0));
        dpl_remove_at_index(*list, 0, free_element);
    }

    // Free the list itself
    free(*list);
    *list = NULL;
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {
    // Function requirements
    // Inserts a new list node containing an 'element' in the list at position 'index' and returns a pointer to the new list.
    // If insert_copy == true : use element_copy() to make a copy of 'element' and use the copy in the new list node
    // If insert_copy == false : insert 'element' in the new list node without taking a copy of 'element' with element_copy()
    // Remark: the first list node has index 0.
    // If 'index' is 0 or negative, the list node is inserted at the start of 'list'.
    // If 'index' is bigger than the number of elements in the list, the list node is inserted at the end of the list.

    // Check if the list is NULL
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    // Initialize the reference and list node
    dplist_node_t *ref_at_index = NULL, *list_node = malloc(sizeof(dplist_node_t));
    memset(list_node, 0, sizeof(dplist_node_t));
    DPLIST_ERR_HANDLER(list_node == NULL, DPLIST_MEMORY_ERROR);

    // Check if needed to make a deep copy of the element being inserted
    if (insert_copy == true) {
        // Make a copy
        list_node->element = list->element_copy(element);
    }
        // Insert without making a copy
    else list_node->element = element;

    //
    if (list->head == NULL) { // List is empty, just insert the node
        list_node->prev = NULL;
        list_node->next = NULL;
        list->head = list_node;
    } else if (index <= 0)
        //  List not empty
    { // Index negative or 0, insert at the beginning
        list_node->prev = NULL;
        list_node->next = list->head;
        list->head->prev = list_node;
        list->head = list_node;
    } else { // Index positive
        // Get the reference at index
        ref_at_index = dpl_get_reference_at_index(list, index);  //Get the index element in list
        assert(ref_at_index != NULL);

        if (index < dpl_size(list)) { // Index inside range(smaller than the list size)
            list_node->prev = ref_at_index->prev;
            list_node->next = ref_at_index;
            ref_at_index->prev->next = list_node;
            ref_at_index->prev = list_node;
        } else { // Index larger than list size, insert at end
            assert(ref_at_index->next == NULL);
            list_node->next = NULL;
            list_node->prev = ref_at_index;
            ref_at_index->next = list_node;
        }
    }
    return list;
}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {
    // Removes the list node at index 'index' from the list.
    // If free_element == true : call element_free() on the element of the list node to remove
    // If free_element == false : don't call element_free() on the element of the list node to remove
    // The list node itself should always be freed
    // If 'index' is 0 or negative, the first list node is removed.
    // If 'index' is bigger than the number of elements in the list, the last list node is removed.
    // If the list is empty, return the unmodified list

    // Check if the list is NULL
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    // Declare variables to store reference at index and list node
    dplist_node_t *list_node, *ref_at_index;

    if (list->head == NULL) { // Empty list, nothing to remove, return the unmodified list
        return list;
    }

    // Get the reference at index
    ref_at_index = dpl_get_reference_at_index(list, index);
    assert(ref_at_index != NULL);

    // Check if it's necessary to free the element copy
    if (free_element == true) {
        void *pointer_to_element = &(ref_at_index->element);
        list->element_free(pointer_to_element);
    }
    // List only have one element, just remove it, regardless of the index
    if (dpl_size(list) == 1) {
        list->head = NULL;
        free(ref_at_index);
        return list;
    }

    // Index negative or 0
    if (index <= 0) {
        // covers case 3----Remove the first element
        list->head = ref_at_index->next;
        list->head->prev = NULL;
        free(ref_at_index);
    } else {
        if (index <= dpl_size(list)) {
            // covers case 4----Remove at index
            dplist_node_t *prev_node;
            dplist_node_t *next_node;
            prev_node = dpl_get_reference_at_index(list, index - 1);
            next_node = dpl_get_reference_at_index(list, index + 1);
            prev_node->next = ref_at_index->next;
            next_node->prev = ref_at_index->prev;

            free(ref_at_index);
        } else if (index > dpl_size(list)) {
            // covers case 5----Remove the last element
            ref_at_index->prev = NULL;
            list_node = dpl_get_reference_at_index(list, (dpl_size(list) - 2));
            list_node->next = NULL;

            free(ref_at_index);
        }
    }

    return list;
}

int dpl_size(dplist_t *list) {
    // Returns the number of elements in the list.
    int size = 0;
    dplist_node_t *dummy = NULL;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    for (dummy = list->head; dummy != NULL; dummy = dummy->next, size++) {};
    return size;
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {
    // Returns a reference to the list node with index 'index' in the list.
    // If 'index' is 0 or negative, a reference to the first list node is returned.
    // If 'index' is bigger than the number of list nodes in the list, a reference to the last list node is returned.
    // If the list is empty, NULL is returned.

    int count = 0;
    dplist_node_t *dummy = NULL;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    if (list->head == NULL) return NULL;

        // Index negative or 0, return the first reference
    else if (index <= 0) dummy = dpl_get_first_reference(list);

        // Index larger than size, return the last reference
    else if (index > dpl_size(list)) dummy = dpl_get_last_reference(list);

    else {

        // Return the reference at index
        for (dummy = list->head, count = 0; dummy->next != NULL; dummy = dummy->next, count++) {
            if (count >= index) return dummy;
        }
    }
    return dummy;
}

void *dpl_get_element_at_index(dplist_t *list, int index) {
    // Returns the list element contained in the list node with index 'index' in the list.
    // Remark: return is not returning a copy of the element with index 'index', i.e. 'element_copy()' is not used.
    // If 'index' is 0 or negative, the element of the first list node is returned.
    // If 'index' is bigger than the number of elements in the list, the element of the last list node is returned.
    // If the list is empty, (void *)0 is returned.
    dplist_node_t *dummy = NULL;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    dummy = dpl_get_reference_at_index(list, index);
    return dummy ? dummy->element : NULL;
}

int dpl_get_index_of_element(dplist_t *list, void *element) {
    // Returns an index to the first list node in the list containing 'element'.
    // Use 'element_compare()' to search 'element' in the list
    // A match is found when 'element_compare()' returns 0
    // If 'element' is not found in the list, -1 is returned.
    int index = 0, size = dpl_size(list);
    dplist_node_t *dummy = NULL;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    for (dummy = list->head, index = 0; index < size; dummy = dummy->next, index++) {
        // If found return the result
        if (list->element_compare(dummy->element, element) == 0) return index;
    }
    // Else return -1
    return -1;
}

// HERE STARTS THE EXTRA SET OF OPERATORS //

// ---- list navigation operators ----//

dplist_node_t *dpl_get_first_reference(dplist_t *list) {
    // Returns a reference to the first list node of the list.
    // If the list is empty, NULL is returned.

    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    if (list->head == NULL) return NULL;
    else return list->head;
}

dplist_node_t *dpl_get_last_reference(dplist_t *list) {
    // Returns a reference to the last list node of the list.
    // If the list is empty, NULL is returned.

    dplist_node_t *dummy = list->head;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    if (list->head == NULL) return NULL;
    while (dummy->next != NULL) {
        dummy = dummy->next;
    }
    return dummy;
}

dplist_node_t *dpl_get_next_reference(dplist_t *list, dplist_node_t *reference) {
    // Returns a reference to the next list node of the list node with reference 'reference' in the list.
    // If the list is empty, NULL is returned
    // If 'reference' is NULL, NULL is returned.
    // If 'reference' is not an existing reference in the list, NULL is returned.

    dplist_node_t *dummy = NULL;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    if (list->head == NULL) return NULL;
    if (reference == NULL) return NULL;
    for (dummy = list->head; dummy->next != NULL; dummy = dummy->next) {
        if (dummy == reference) {
            return reference->next;
        }
    }
    return NULL;
}

dplist_node_t *dpl_get_previous_reference(dplist_t *list, dplist_node_t *reference) {
    // Returns a reference to the previous list node of the list node with reference 'reference' in 'list'.
    // If the list is empty, NULL is returned.
    // If 'reference' is NULL, a reference to the last list node in the list is returned.
    // If 'reference' is not an existing reference in the list, NULL is returned.

    dplist_node_t *dummy = NULL;
    int index = 0, size = dpl_size(list);
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    if (list->head == NULL) return NULL;
    if (reference == NULL) return dpl_get_last_reference(list);
    for (index = 0, dummy = list->head; index < size; index++, dummy = dummy->next) {
        if (dummy == reference) {
            return reference->prev;
        }
    }
    return NULL;
}

// ---- search & find operators ----//

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {
    // Returns the element contained in the list node with reference 'reference' in the list.
    // If the list is empty, NULL is returned.
    // If 'reference' is NULL, the element of the last element is returned.
    // If 'reference' is not an existing reference in the list, NULL is returned.

    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    void *element = NULL;
    if (list->head == NULL) return NULL;
    if (reference == NULL) return dpl_get_last_reference(list)->element;
    dplist_node_t *dummy = list->head;
    while (dummy != NULL) {
        if (dummy == reference) return reference->element;
        dummy = dummy->next;
    }
    return element;
}

dplist_node_t *dpl_get_reference_of_element(dplist_t *list, void *element) {
    // Returns a reference to the first list node in the list containing 'element'.
    // If the list is empty, NULL is returned.
    // If 'element' is not found in the list, NULL is returned.

    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    if (list->head == NULL) return NULL;
    dplist_node_t *dummy = list->head;
    while (dummy != NULL) {
        if (list->element_compare(dummy->element, element) == 0) return dummy;
        dummy = dummy->next;
    }
    return NULL;
}

int dpl_get_index_of_reference(dplist_t *list, dplist_node_t *reference) {
    // Returns the index of the list node in the list with reference 'reference'.
    // If the list is empty, -1 is returned.
    // If 'reference' is NULL, the index of the last element is returned.
    // If 'reference' is not an existing reference in the list, -1 is returned.

    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    if (reference == NULL) return (dpl_size(list) - 1);
    if (list->head == NULL) return -1;
    dplist_node_t *dummy = list->head;
    int index = 0;
    while (dummy != NULL) {
        if (dummy == reference) return index;
        else {
            dummy = dummy->next;
            index++;
        }
    }
    return -1;
}

// ---- extra insert & remove operators ----//

dplist_t *dpl_insert_at_reference(dplist_t *list, void *element, dplist_node_t *reference, bool insert_copy) {
    // Inserts a new list node containing an 'element' in the list at position 'reference'  and returns a pointer to the new list.
    // If insert_copy == true : use element_copy() to make a copy of 'element' and use the copy in the new list node
    // If insert_copy == false : insert 'element' in the new list node without taking a copy of 'element' with element_copy()
    // If 'reference' is NULL, the element is inserted at the end of 'list'.
    // If 'reference' is not an existing reference in the list, 'list' is returned.

    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    int index = dpl_get_index_of_reference(list, reference);
    if (index != -1 && list->head == NULL) dpl_insert_at_index(list, element, 0, insert_copy);
    if (index == -1) return list;
    dpl_insert_at_index(list, element, index, insert_copy);
    return list;
}

dplist_t *dpl_insert_sorted(dplist_t *list, void *element, bool insert_copy) {
    // Inserts a new list node containing 'element' in the sorted list and returns a pointer to the new list.
    // The list must be sorted before calling this function.
    // The sorting is done in ascending order according to a comparison function.
    // If two members compare as equal, their order in the sorted array is undefined.
    // If insert_copy == true : use element_copy() to make a copy of 'element' and use the copy in the new list node
    // If insert_copy == false : insert 'element' in the new list node without taking a copy of 'element' with element_copy()

    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    if (list->head == NULL) dpl_insert_at_index(list, element, 0, insert_copy);

    dplist_node_t *dummy = list->head;
    while (dummy != NULL) {
        if (list->element_compare(element, dummy->element) == -1) {
            list = dpl_insert_at_reference(list, element, dummy, insert_copy);
            return list;
        } else if (dummy->next == NULL) {
            list = dpl_insert_at_index(list, element, dpl_size(list) + 1, insert_copy);
            return list;
        } else dummy = dummy->next;
    }
    return list;
}

dplist_t *dpl_remove_at_reference(dplist_t *list, dplist_node_t *reference, bool free_element) {
    // Removes the list node with reference 'reference' in the list.
    // If free_element == true : call element_free() on the element of the list node to remove
    // If free_element == false : don't call element_free() on the element of the list node to remove
    // The list node itself should always be freed
    // If 'reference' is NULL, the last list node is removed.
    // If 'reference' is not an existing reference in the list, 'list' is returned.
    // If the list is empty, return the unmodifed list

    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    int index = dpl_get_index_of_reference(list, reference);
    if (index == -1) return list;
    else {
        dpl_remove_at_index(list, index, free_element);
        return list;
    }
}

dplist_t *dpl_remove_element(dplist_t *list, void *element, bool free_element) {
    // Finds the first list node in the list that contains 'element' and removes the list node from 'list'.
    // If free_element == true : call element_free() on the element of the list node to remove
    // If free_element == false : don't call element_free() on the element of the list node to remove
    // If 'element' is not found in 'list', the unmodified 'list' is returned.

    int index = dpl_get_index_of_element(list, element);
    if (index == -1) return list;
    else {
        dpl_remove_at_index(list, index, free_element);
        return list;
    }
}


