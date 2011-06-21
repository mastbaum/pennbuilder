#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ds.h"
#include "jemalloc/jemalloc.h"

void pmtbundle_print(PMTBundle* p)
{
    printf("PMTBundle at %p:\n", p);
    printf("  pmtid =  %i:\n", p->pmtid);
    printf("  gtid  =  %i:\n", p->gtid);
    int i;
    for(i=0; i<3; i++)
        printf("  word%i =  %u:\n", i, p->word[i]);
}

Buffer* buffer_alloc(Buffer** pb)
{
    int sz = sizeof(Buffer);
    *pb = malloc(sz);
    if(*pb)
    {
        printf("Initializing buffer: keys[%d] (%d)\n", BUFFER_SIZE, sz);
        memset(*pb, 0, sizeof(Buffer));
        (*pb)->size = BUFFER_SIZE;
        (*pb)->end = 0;
        (*pb)->start  = 0;
        (*pb)->offset = 0;
        (*pb)->mutex = PTHREAD_MUTEX_INITIALIZER;
    }
    return *pb;
}
 
int buffer_isfull(Buffer* b)
{
    return (((b->end + 1) % b->size) == b->start);
}
 
int buffer_isempty(Buffer* b)
{
    return (b->start == b->end);
}
 
int buffer_push(Buffer* b, RecordType type, void* key)
{
    int full = buffer_isfull(b);
    if(!full)
    {
        pthread_mutex_lock(&(b->mutex));
        b->keys[b->end] = key;
        b->type[b->end] = type;
        b->end++;
        b->end %= b->size;
        pthread_mutex_unlock(&(b->mutex));
    }
    return !full;
}

int buffer_pop(Buffer* b, RecordType* type, void** pk)
{
    int empty = buffer_isempty(b);
    if(!empty)
    {
        (*pk) = b->keys[b->start];
        pthread_mutex_lock(&(b->mutex));
        if(*pk) {
            (*type) = b->keys[b->start];
            free(b->keys[b->start]);
            b->keys[b->start] = NULL;
        }
        b->start++; // note: you can pop a NULL pointer off the end
        b->start %= b->size;
        pthread_mutex_unlock(&(b->mutex));
    }
    return !empty;
}

void buffer_status(Buffer* b)
{
    printf("Buffer at %p:\n", b);
    printf("  write: %lu, read: %lu, full: %d, empty: %d\n", b->end,
                                                             b->start,
                                                             buffer_isfull(b),
                                                             buffer_isempty(b));
}

void buffer_clear(Buffer* b)
{
    pthread_mutex_lock(&(b->mutex));
    int i;
    for(i=0; i<b->size; i++)
        if(b->keys[i]) {
            free(b->keys[i]);
            b->keys[i] = NULL;
        }
    b->end = 0;
    b->start = 0;
    pthread_mutex_unlock(&(b->mutex));
}

int buffer_at(Buffer* b, unsigned int id, RecordType* type, void** pk)
{
    int keyid = (id - b->offset) % b->size;
    if (keyid < b->size) {
        *type = b->type[keyid];
        *pk = b->keys[keyid];
        return pk == NULL ? 1 : 0;
    }
    else
        return 1;
}

int buffer_insert(Buffer* b, unsigned int id, RecordType type, void* pk)
{
    int keyid = (id - b->offset) % b->size;
    if (!b->keys[keyid] && (keyid < b->size)) {
        pthread_mutex_lock(&(b->mutex));
        b->type[keyid] = type;
        b->keys[keyid] = pk;
        if(keyid > b->end) {
            b->end = keyid;
            b->end %= b->size;
        }
        if(keyid < b->start) {
            printf("buffer_insert: got record with id %i < read position %i\n", keyid, b->start);
            // received data for already-shipped event, do something
        }
        pthread_mutex_unlock(&(b->mutex));
        return 0;
    }
    else
        return 1;
}

