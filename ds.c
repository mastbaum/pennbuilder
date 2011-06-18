#include <stdio.h>
#include <stdlib.h>
#include "ds.h"

void pmtbundle_print(PMTBundle* p)
{
    printf("PMTBundle at %p:\n", p);
    printf("  word1 =  %u:\n", p->word1);
    printf("  word2 =  %u:\n", p->word2);
    printf("  word3 =  %u:\n", p->word3);
}

void event_clear(Event* e)
{
    int i;
    for(i=0; i<NPMTS; i++)
        if(e->pmt[i]) {
            free(e->pmt[i]);
            e->pmt[i] = NULL;
        }
}

Buffer* buffer_alloc(Buffer** pb)
{
    int sz = sizeof(Buffer);
    *pb = malloc(sz);
    if(*pb)
    {
        printf("Initializing buffer: keys[%d] (%d)\n", BUFFER_SIZE, sz);
        (*pb)->size = BUFFER_SIZE;
        (*pb)->end = 0;
        (*pb)->start  = 0;
        (*pb)->offset = 0;

        int i;
        for(i=0; i<(*pb)->size; i++) {
            Event* e = malloc(sizeof(Event));
            int j;
            for(j=0;j<10000;j++) {
                PMTBundle* p = malloc(sizeof(PMTBundle));
                p->word1 = i*100;
                e->pmt[j] = p;
            }
            (*pb)->keys[i] = e;
        }
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
 
int buffer_push(Buffer* b, Event* key)
{
    int full = buffer_isfull(b);
    if(!full)
    {
        b->keys[b->end] = key;
        b->end++;
        b->end %= b->size;
    }
    return !full;
}
 
int buffer_pop(Buffer* b, Event* pk)
{
    int empty = buffer_isempty(b);
    if(!empty)
    {
        pk = b->keys[b->start];
        pthread_mutex_t m;
        if(pk) {
            m = b->keys[b->start]->mutex;
            pthread_mutex_lock(&m);
        }
        free(b->keys[b->start]);
        b->keys[b->start] = NULL;
        if(pk)
            pthread_mutex_unlock(&m);
        b->start++;
        b->start %= b->size;
    }
    return !empty;
}

void buffer_status(Buffer* b)
{
    printf("write: %lu, read: %lu, full: %d, empty: %d\n", b->end,
                                                           b->start,
                                                           buffer_isfull(b),
                                                           buffer_isempty(b));
}

void buffer_clear(Buffer* b)
{
    int i;
    for(i=0; i<b->size; i++)
        if(b->keys[i]) {
            pthread_mutex_t m = b->keys[i]->mutex;
            pthread_mutex_lock(&m);
            event_clear(b->keys[i]);
            free(b->keys[i]);
            b->keys[i] = NULL;
            pthread_mutex_unlock(&m);
        }
    b->end = 0;
    b->start = 0;
}

// random access
int buffer_at(Buffer* b, unsigned int id, Event** pk)
{
    int keyid = (id - b->offset) % b->size;
    if (keyid < b->size) {
        (*pk) = b->keys[keyid];
        return pk == NULL ? 1 : 0;
    }
    else
        return 1;
}

int buffer_insert(Buffer* b, unsigned int id, Event* pk)
{
    int keyid = (id - b->offset) % b->size;
    if (!b->keys[keyid] && (keyid < b->size)) {
        pthread_mutex_t m = b->keys[keyid]->mutex;
        pthread_mutex_lock(&m);
        b->keys[keyid] = pk;
        pthread_mutex_unlock(&m);
        return 0;
    }
    else
        return 1;
}

