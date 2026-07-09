#include "process.h"
#include "../mm/paging.h"
#include "../mm/heap.h"
#include "../userspace/exe.h"

#define KERNEL_STACK_SIZE 8192

static uint32_t next_pid = 0;

extern void enter_usermode(uint32_t entry, uint32_t user_stack, uint32_t pd_phys);
extern void tss_set_kernel_stack(uint32_t esp0);
extern void isr_return();
extern void context_switch(uint32_t *old_esp_store, uint32_t new_esp);

static void init_fake_stack(process_t *proc) {
    uint32_t *sp = (uint32_t *)(proc->kstack + KERNEL_STACK_SIZE);

    *--sp = 0x23;                  // ss
    *--sp = proc->user_stack_top;  // esp
    *--sp = 0x202;                 // eflags (IF set, reserved bit 1 set)
    *--sp = 0x1b;                  // cs
    *--sp = proc->entry_point;     // eip

    *--sp = 0; // err_code
    *--sp = 0; // int_no

    *--sp = 0; // eax
    *--sp = 0; // ecx
    *--sp = 0; // edx
    *--sp = 0; // ebx
    *--sp = 0; // esp
    *--sp = 0; // ebp
    *--sp = 0; // esi
    *--sp = 0; // edi

    *--sp = 0x23; // ds
    *--sp = 0x23; // es
    *--sp = 0x23; // fs
    *--sp = 0x23; // gs

    *--sp = (uint32_t)isr_return;

    *--sp = 0;  // ebp
    *--sp = 0;  // edi
    *--sp = 0;  // esi
    *--sp = 0;  // ebx

    proc->saved_esp = (uint32_t)sp;
}

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
    proc->ppid = -1;
    proc->target_pid = -1;
    proc->next = NULL;
    proc->kstack = kstack;
    proc->esp0 = kstack + KERNEL_STACK_SIZE;
    proc->resume_tick = (uint32_t)-1;

    init_fake_stack(proc);

    for (int i = 0; i < MAX_FDS_PER_PROC; i++)
        proc->fds[i] = -1;
    
    proc->exit_code = -1;
    proc->state = PROC_READY;

    return proc;
}

void process_run(process_t *proc) {
    curr_proc = proc;
    curr_proc->state = PROC_RUNNING;

    tss_set_kernel_stack(curr_proc->esp0);
    paging_switch_directory(curr_proc->pd_phys);
    context_switch(&(uint32_t){0}, proc->saved_esp);
}

process_t *process_find(uint32_t pid) {
    process_t *proc = proc_list;

    while (proc) {
        if (proc->pid == pid)
            return proc;
        
        proc = proc->next;
    }

    return NULL;
}