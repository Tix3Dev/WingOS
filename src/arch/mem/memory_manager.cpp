#include "memory_manager.h"
#include <arch/lock.h>
#include <arch/process.h>
#include <stddef.h>
memory_map_children *heap = nullptr;
uint64_t heap_length;
// so this work with 512mo of data each time
lock_type memory_lock = {0};
void init_mm()
{
    log("memory manager", LOG_DEBUG) << "loading mm";
    heap = (memory_map_children *)pmm_alloc(MM_BIG_BLOCK_SIZE / 4096);
    heap->code = 0xf2ee;
    heap->length = MM_BIG_BLOCK_SIZE - sizeof(memory_map_children);
    heap->is_free = true;
    heap->next = nullptr;
}
void *addr_from_header(memory_map_children *target)
{
    return (void *)((uint64_t)target + sizeof(memory_map_children));
}
void increase_mmap()
{
    log("memory manager", LOG_INFO) << "increasing memory manager map";
    memory_map_children *current = heap;
    for (uint64_t i = 0; current != nullptr; i++)
    {
        current = current->next;
        if (current->next == nullptr)
        {

            current->next = (memory_map_children *)pmm_alloc(MM_BIG_BLOCK_SIZE / 4096);
            current->next->code = 0xf2ee;
            current->next->length = MM_BIG_BLOCK_SIZE - sizeof(memory_map_children);
            current->next->is_free = true;
            current->next->next = nullptr;
            return;
        }
    }
}
void insert_new_mmap_child(memory_map_children *target, uint64_t length)
{
    uint64_t t = (uint64_t)addr_from_header(target);
    t += length;
    uint64_t previous_next = (uint64_t)target->next;
    uint64_t previous_length = target->length;
    target->next = (memory_map_children *)t;
    target->next->next = (memory_map_children *)previous_next;
    target->next->length = target->length - (length + sizeof(memory_map_children));
    target->length = length;
    target->next->code = 0xf2ee;
    target->next->is_free = true;
}
void *malloc(uint64_t length)
{
    lock(&memory_lock);
    lock_process();
    if (length < 16)
    {
        length = 16;
    }
    if (heap == nullptr)
    {
        init_mm();
    }
    memory_map_children *current = heap;
    for (uint64_t i = 0; current != nullptr; i++)
    {
        if (current->is_free == true)
        {
            if (current->length > length)
            {

                current->is_free = false;
                insert_new_mmap_child(current, length);
                current->length = length;
                current->code = 0xf2ee;
                unlock_process();
                unlock(&memory_lock);

                return addr_from_header(current);
            }
            else if (current->length == length)
            {

                current->is_free = false;
                current->code = 0xf2ee;
                unlock_process();
                unlock(&memory_lock);
                return addr_from_header(current);
            }
        }
        current = current->next;
    }
    increase_mmap();
    unlock_process();
    unlock(&memory_lock);

    return malloc(length);
}
void free(void *addr)
{
    lock(&memory_lock);
    lock_process();
    memory_map_children *current = (memory_map_children *)((uint64_t)addr - sizeof(memory_map_children));
    if (current->code != 0xf2ee)
    {
        log("memory manager", LOG_ERROR) << "trying to free an invalid address" << current->code;
        return;
    }
    else if (current->is_free == true)
    {
        log("memory manager", LOG_ERROR) << "address is already free";
        return;
    }

    current->is_free = true;
    unlock_process();
    unlock(&memory_lock);
}