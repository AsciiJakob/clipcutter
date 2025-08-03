#ifndef DYNARR_H
#define DYNARR_H

struct DynArr {
    size_t size;
    size_t element_size;
    void** items;
};

void DynArr_Init(DynArr* arr, size_t element_size);
void DynArr_Free(DynArr* arr);
int DynArr_Append(DynArr* arr, void* element);
void DynArr_RemoveElement(DynArr* arr, void* element);
bool DynArr_Contains(DynArr* arr, void* element);

#endif

