#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    PROC_READY,
    PROC_RUNNING,
    PROC_DEAD
} proc_state_t;

typedef struct process {
    uint32_t pid;
    uint32_t pd_phys;
    uint32_t esp0;
    uint32_t kstack;
    uint32_t entry_point;
    uint32_t user_stack_top;
    proc_state_t state;
    struct process *next;
} process_t;

process_t *process_create(const char *path, int *err_out);
void process_run(process_t *proc);

#endif