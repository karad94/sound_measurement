#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#include "startup.h"

// Deklaration Struct for Ringbuffer
struct ringbuffer_handle{
    uint writeIndex;
    uint readIndex;
    uint32_t *sensValue;
    int size;
    bool full;
};
typedef struct ringbuffer_handle ringbuffer_handle_t;

// Function to initialiaze the buffer
ringbuffer_handle_t *init_buffer(uint size);

// Function to write data to buffer
void write_to_buffer(ringbuffer_handle_t *buffer, uint32_t data);

// Read data from buffer
uint32_t read_from_buffer(ringbuffer_handle_t *buffer);

// Check if Buffer is full
bool is_full(ringbuffer_handle_t *buffer);

// Free allocated Memory
void free_buffer(ringbuffer_handle_t *buffer);

#endif