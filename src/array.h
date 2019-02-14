#ifndef ARRAY_INCLUDED
#define ARRAY_INCLUDED


struct pending_message {
     int src;
     int seq;
     int *clock;
}pending_message;

typedef struct {
    struct pending_message *array;
    int size;
    int cap;
} Array;

void initArray(Array *a, int initialSize);
void insertArray(Array *a, struct pending_message element);
void deleteArray(Array *a, struct pending_message element);
void freeArray(Array *a);

#endif  
