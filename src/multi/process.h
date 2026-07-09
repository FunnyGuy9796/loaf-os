#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "../idt/isr.h"
#include "../fs/file.h"

typedef enum {
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_DEAD
} proc_state_t;

typedef struct process {
    uint32_t pid;
    uint32_t ppid;
    uint32_t target_pid;
    uint32_t pd_phys;
    uint32_t esp0;
    uint32_t kstack;
    uint32_t entry_point;
    uint32_t user_stack_top;
    uint32_t saved_esp;
    uint32_t heap_start;
    uint32_t heap_break;
    uint32_t heap_max;
    uint32_t resume_tick;
    int fds[MAX_FDS_PER_PROC];
    int exit_code;
    proc_state_t state;
    struct process *next;
} process_t;

extern process_t *proc_list;
extern process_t *curr_proc;
extern process_t *idle_proc;
extern volatile int scheduler_ready;

process_t *process_create(const char *path, int *err_out);
void process_run(process_t *proc);
process_t *process_find(uint32_t pid);

void scheduler_add(process_t *proc);
void schedule(registers_t *regs);

#endif