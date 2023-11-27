/**
 * @file ringbuffer.h
 * @author Adam Karsten (a.karsten@ostfalia.de)
 * @brief Handler for Ringbuffer
 * @version 0.1
 * @date 2023-11-27
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

/**
 * @brief Struct and Typedef for Ringbuffer
 * 
 */
struct ringbuffer_handle{
    uint writeIndex;
    uint readIndex;
    uint32_t *sensValue;
    int size;
    bool full;
};
typedef struct ringbuffer_handle ringbuffer_handle_t;

/**
 * @brief Initialize and allocate memory for Ringbuffer
 * 
 * @param size Size of Ringbuffer
 * @return ringbuffer_handle_t* Pointer to Ringbuffer
 */
ringbuffer_handle_t *init_buffer(uint size);

/**
 * @brief Write data to Buffer and move Index to next position. Buffer will marked es full, when writeIndex + 1 == readIndex
 * 
 * @param buffer Pointer Buffer to which the Data should be written
 * @param data Data of Type uint32_t
 */
void write_to_buffer(ringbuffer_handle_t *buffer, uint32_t data);

/**
 * @brief Read data from Buffer and move readIndex to next postion. Buffer will marked es empty, when readIndex == writeIndex
 * 
 * @param buffer  Pointer toBuffer from which data should be read
 * @return uint32_t Data from Buffer
 */
uint32_t read_from_buffer(ringbuffer_handle_t *buffer);

/**
 * @brief Check if Buffer is full
 * 
 * @param buffer Pointer to Buffer which should be checked
 * @return bool TRUE if Buffer is full // else FALSE
 * 
 */
bool is_full(ringbuffer_handle_t *buffer);

/**
 * @brief Free Ringbuffer
 * 
 * @param buffer Pointer to Buffer which memory should be free
 */
void free_buffer(ringbuffer_handle_t *buffer);

#endif