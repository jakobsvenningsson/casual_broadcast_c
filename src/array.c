#include "array.h"
#include <stddef.h>
#include<stdio.h>
#include <stdlib.h>

void initArray(Array *a, int initialSize) {
      a->array = (struct pending_message *)malloc(initialSize * sizeof(pending_message));
      a->size = 0;
      a->cap = initialSize;
}

void insertArray(Array *a, struct pending_message element) {
    //a->used is the number of used entries, because a->array[a->used++] updates a->used only *after* the array has been accessed.
    // Therefore a->used can go up to a->size 
    if (a->size == a->cap) {
        a->cap *= 2;
        a->array = (struct pending_message *)realloc(a->array, a->cap * sizeof(struct pending_message));
    }
        a->array[a->size++] = element;
}

void deleteArray(Array *a, struct pending_message element) {
    for(int i = 0; i < a->size; ++i) {
        if(a->array[i].seq == element.seq && a->array[i].src == element.src) {
            a->size--;
            for(int j = i; j < a->size; ++j) {
                a->array[j] = a->array[j+1];
            }
        }
    }
}

void freeArray(Array *a) {
    free(a->array);
    a->array = NULL;
    a->size = a->cap = 0;
}
