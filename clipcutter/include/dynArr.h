#ifndef DYNARR_H
#define DYNARR_H
#include <cstddef>

struct DynArr {
    size_t capacity; // how many elements can fit
    size_t size; // how many elements are in use
    size_t elementSize;
    void* items;
};

void DynArr_Init(DynArr* arr, size_t elementSize, size_t initialCapacity);
void DynArr_Free(DynArr* arr);
int DynArr_Append(DynArr* arr, void* element);
void* DynArr_Get(DynArr* arr, size_t index);
int DynArr_Set(DynArr* arr, size_t index, const void* element);
void DynArr_RemoveAt(DynArr* arr, size_t index);
void DynArr_RemoveElement(DynArr* arr, const void* element);
size_t DynArr_IndexOf(DynArr* arr, const void* element);
bool DynArr_Contains(DynArr* arr, void* element);

#endif

