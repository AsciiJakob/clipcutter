#include "dynArr.h"
#include "log.h"
#include <cstring>
#include <malloc.h>

void DynArr_Init(DynArr* arr, size_t elementSize, size_t initialCapacity) {
    arr->size = 0;
    arr->elementSize = elementSize;
    arr->capacity = initialCapacity;
    arr->items = (void**) malloc(initialCapacity * elementSize);
}

// frees the item array, make sure to free the DynArr* itself yourself
void DynArr_Free(DynArr* arr) {
    free(arr->items);
    arr->items = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

int DynArr_Append(DynArr* arr, void* element) {
    if (arr->size == arr->capacity) {
        size_t newCapacity = arr->capacity == 0 ? 1 : arr->capacity * 2;
        void** newItems = (void**) realloc(arr->items, newCapacity * arr->elementSize);
        if (!newItems) return 1;
        arr->items = newItems;
        // log_debug("expanded arr of %d to %d", arr->capacity, newCapacity);
        arr->capacity = newCapacity;
    }
    char* dest = (char*)arr->items + arr->size * arr->elementSize;
    memcpy(dest, element, arr->elementSize);
    arr->size++;
    return 0;
}

void* DynArr_Get(DynArr* arr, size_t index) {
    if (!arr || !arr->items || index >= arr->size) return NULL;
    return (char*)arr->items + index * arr->elementSize;
}
 
int DynArr_Set(DynArr* arr, size_t index, const void* element) {
    if (!arr || !arr->items || index >= arr->size) return 1;
    memcpy((char*)arr->items + index * arr->elementSize, element, arr->elementSize);
    return 0;
}


void DynArr_RemoveAt(DynArr* arr, size_t index) {
    if (!arr || !arr->items || index >= arr->size) return;
    char* base = (char*)arr->items;
    size_t elemSize = arr->elementSize;
    if (index < arr->size - 1) {
        memmove(base + index * elemSize,
                base + (index + 1) * elemSize,
                (arr->size - index - 1) * elemSize);
    }
    arr->size--;
}

size_t DynArr_IndexOf(DynArr* arr, const void* element) {
    if (!arr || !arr->items) return -1;
    for (size_t i = 0; i < arr->size; i++) {
        void* slot = (char*)arr->items + i * arr->elementSize;
        if (memcmp(slot, element, arr->elementSize) == 0) {
            log_debug("loop\n");
            return i;
        }
    }
    return -1;
}


// remove first instance of element from array
void DynArr_RemoveElement(DynArr* arr, const void* element) {
    int index = DynArr_IndexOf(arr, element);
    if (index != -1) {
        DynArr_RemoveAt(arr, index);
    }
}
