#include "dynArr.h"
#include "log.h"
#include <malloc.h>

void DynArr_Init(DynArr* arr, size_t element_size) {
    arr->size = 0;
    arr->element_size = element_size;
    arr->items = nullptr;
}

// frees the item array, make sure to free the DynArr* itself yourself
void DynArr_Free(DynArr* arr) {
    free(arr->items);
}

int DynArr_Append(DynArr* arr, void* element) {
    void** newItems = (void**) realloc(arr->items, (arr->size+1)*arr->element_size);
    if (!newItems) return 1;
    newItems[arr->size] = element;
    arr->size++;
    arr->items = newItems;
    return 0;
}

// remove first instance of element from array
void DynArr_RemoveElement(DynArr* arr, void* element) {
    bool foundElement = false;
    for (size_t i=0; i < arr->size; i++) {
        if (foundElement) {
            arr->items[i-1] = arr->items[i];
        }

        if (arr->items[i] == element) {
            foundElement = true;
        }
    }

    if (foundElement) {
        arr->size--;
        void** newItems = (void**) realloc(arr->items, (arr->size)*arr->element_size);
        arr->items = newItems;
    }

}

bool DynArr_Contains(DynArr* arr, void* element) {
    if (!arr || !arr->items) return false;

    for (size_t i=0; i < arr->size; i++) {
        if (arr->items[i] == element) {
            log_debug("loop\n");
            return true;
        }
    }
    return false;
}
