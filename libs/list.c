#include <os/list.h>

/**
 * @brief Add a node to the beginning of the list.
 */
void list_prepend(list_head* head, list_node_t* node) {
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}
/**
 * @brief Add a node to the end of the list.
 */
void list_append(list_head* head, list_node_t* node) {
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;
}
/**
 * @brief Pop a node from the beginning of the list.
 */
void list_shift(list_head* head, list_node_t** node) {
    if (head->next == head) {
        *node = NULL;  // List is empty
        return;
    }
    *node = head->next;
    head->next = (*node)->next;
    (*node)->next->prev = head;
}
/**
 * @brief Pop a node from the end of the list.
 */
void list_pop(list_head* head, list_node_t** node) {
    if (head->prev == head) {
        *node = NULL;  // List is empty
        return;
    }
    *node = head->prev;
    head->prev = (*node)->prev;
    (*node)->prev->next = head;
}

size_t list_size(list_head* head) {
    size_t size = 0;
    list_node_t* current = head->next;
    while (current != head) {
        size++;
        current = current->next;
    }
    return size;
}
