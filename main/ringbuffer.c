#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Custom Headerfiles
#include "ringbuffer.h"

// Init of Ringbuffer with Memory allocation. Size of Buffer is set in Parameter size at Function-call
ringbuffer_handle_t *init_buffer(uint size)
{
    ringbuffer_handle_t *buffer;

    // Allocate Memory for Buffer
    buffer = malloc(sizeof(ringbuffer_handle_t));
    if(buffer == NULL)
    {
        return NULL;
    }

    // Allocate Buffer Data
    buffer->sensValue = malloc(sizeof(uint32_t) * size);
    if(buffer->sensValue == NULL)
    {
        free(buffer);
        return NULL;
    }

    // Init Values
    buffer->writeIndex = 0;
    buffer->readIndex = 0,
    buffer->size = size;
    buffer->full = false;

    return buffer;
}

void write_to_buffer(ringbuffer_handle_t *buffer, uint32_t data)
{
    //Check if the ringbuffer is full and set Full-Flag to True
    if(((buffer->writeIndex + 1) % buffer->size) == buffer->readIndex)
    {
        buffer->full = true;
    }

    // Write Data to buffer and move the writeIndex to the next place
    buffer->sensValue[buffer->writeIndex] = data;
    buffer->writeIndex = (buffer->writeIndex + 1) % buffer->size;
}

uint32_t read_from_buffer(ringbuffer_handle_t *buffer)
{
    uint32_t data;

    // Get Data from Buffer an return to Functioncaller
    // Move readIndex to next Position
    data = buffer->sensValue[buffer->readIndex];
    buffer->readIndex = (buffer->readIndex + 1) % buffer->size;

    // If every Value has been read, the Full-Flag will be reseted to allow Function to write new Values to buffer
    if(buffer->readIndex == buffer->writeIndex)
    {
        buffer->full = false;
    }

    return data;
}

bool is_full(ringbuffer_handle_t *buffer)
{
    return buffer->full;
}

void free_buffer(ringbuffer_handle_t *buffer)
{
    free(buffer->sensValue);
    free(buffer);
}