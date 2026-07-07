#include "process.h"
#include "../mm/heap.h"
#include "../userspace/exe.h"

#define KERNEL_STACK_SIZE 8192

process_t *proc_list = NULL;
process_t *curr_proc = NULL;
static uint32_t next_pid = 1;

extern void enter_usermode(uint32_t entry, uint32_t user_stack, uint32_t pd_phys);
extern void tss_set_kernel_stack(uint32_t esp0);

process_t *process_create(const char *path, int *err_out) {
    process_t *proc = (process_t *)kmalloc(sizeof(process_t));

    if (!proc) {
        if (err_out)
            *err_out = -1;
        
        return NULL;
    }

    int r = exec_load(path, proc);

    if (r) {
        if (err_out)
            *err_out = r;

        return NULL;
    }

    uint32_t kstack = kmalloc(KERNEL_STACK_SIZE);

    if (!kstack) {
        if (err_out)
            *err_out = -2;

        return NULL;
    }

    proc->pid = next_pid++;
    proc->kstack = kstack;
    proc->esp0 = kstack + KERNEL_STACK_SIZE;
    proc->state = PROC_READY;
    proc->next = proc_list;
    proc_list = proc;

    return proc;
}

void process_run(process_t *proc) {
    curr_proc = proc;
    proc->state = PROC_RUNNING;

    tss_set_kernel_stack(proc->esp0);
    enter_usermode(proc->entry_point, proc->user_stack_top, proc->pd_phys);
}