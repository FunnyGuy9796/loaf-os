#include "process.h"
#include "../misc/printf.h"
#include "../mm/paging.h"

process_t *proc_list = NULL;
process_t *curr_proc = NULL;
process_t *idle_proc = NULL;
volatile int scheduler_ready = 0;

extern void tss_set_kernel_stack(uint32_t esp0);
extern void context_switch(uint32_t *old_esp_store, uint32_t new_esp);

void scheduler_add(process_t *proc) {
    proc->next = proc_list;
    proc_list = proc;
}

static process_t *pick_next() {
    if (!curr_proc)
        return proc_list ? proc_list : idle_proc;

    process_t *start = curr_proc->next ? curr_proc->next : proc_list;
    process_t *p = start;

    while (p) {
        if (p->state == PROC_READY)
            return p;

        p = p->next ? p->next : proc_list;

        if (p == start)
            break;
    }

    if (!idle_proc)
        panic("scheduler.c: pick_next() -> idle_proc is NULL\n");

    return idle_proc;
}

void schedule(registers_t *regs) {
    (void)regs;

    if (!scheduler_ready)
        return;

    process_t *prev = curr_proc;
    process_t *next = pick_next();

    if (next == prev)
        return;

    if (prev && prev->state == PROC_RUNNING)
        prev->state = PROC_READY;

    curr_proc = next;
    next->state = PROC_RUNNING;

    tss_set_kernel_stack(next->esp0);
    paging_switch_directory(next->pd_phys);

    if (prev)
        context_switch(&prev->saved_esp, next->saved_esp);
    else
        context_switch(&(process_t){0}.saved_esp, next->saved_esp);
}