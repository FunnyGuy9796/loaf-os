#include "process.h"
#include "../misc/mem.h"
#include "../misc/util.h"
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

static void copy_to_user(uint32_t pd_phys, uint32_t dest_vaddr, const void *src, uint32_t len) {
    __asm__ volatile ("cli");

    uint32_t saved_cr3;

    __asm__ volatile ("mov %%cr3, %0" : "=r"(saved_cr3));

    paging_switch_directory(pd_phys);
    memcpy((void *)dest_vaddr, src, len);

    __asm__ volatile ("mov %0, %%cr3" :: "r"(saved_cr3));
    __asm__ volatile ("sti");
}

static uint32_t setup_user_argv(uint32_t pd_phys, uint32_t user_stack_top, int argc, char *const argv[]) {
    uint32_t str_len = 0;

    for (int i = 0; i < argc; i++)
        str_len += strlen(argv[i]) + 1;
    
    uint32_t ptr_block = (argc + 1) * 4;
    uint32_t block_size = 4 + ptr_block + str_len;

    block_size = (block_size + 15) & ~15;

    uint8_t *kbuf = (uint8_t *)kmalloc(block_size);

    memset(kbuf, 0, block_size);

    uint32_t esp = user_stack_top - block_size;
    uint32_t str_off = 4 + ptr_block;
    uint32_t *argv_ptrs = (uint32_t *)(kbuf + 4);
    uint32_t cursor = str_off;

    for (int i = 0; i < argc; i++) {
        uint32_t len = strlen(argv[i]) + 1;

        memcpy(kbuf + cursor, argv[i], len);

        argv_ptrs[i] = esp + cursor;
        cursor += len;
    }

    argv_ptrs[argc] = 0;
    *(uint32_t *)kbuf = (uint32_t)argc;

    copy_to_user(pd_phys, esp, kbuf, block_size);
    kfree((uint32_t)kbuf);

    return esp;
}

static void set_proc_name(process_t *proc, const char *path) {
    const char *base = path;

    for (const char *p = path; *p; p++) {
        if (*p == '/')
            base = p + 1;
    }

    strncpy(proc->name, base, MAX_PROC_NAME - 1);

    proc->name[MAX_PROC_NAME - 1] = '\0';
}

void set_proc_cwd(process_t *proc, const char *cwd) {
    if (!cwd || cwd[0] == '\0') {
        proc->cwd[0] = '/';
        proc->cwd[1] = '\0';

        return;
    }

    strncpy(proc->cwd, cwd, sizeof(proc->cwd) - 1);

    proc->cwd[sizeof(proc->cwd) - 1] = '\0';
}

process_t *process_create(const char *path, int argc, char *const argv[], int *err_out) {
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
    proc->user_stack_top = setup_user_argv(proc->pd_phys, proc->user_stack_top, argc, argv);

    init_fake_stack(proc);

    for (int i = 0; i < MAX_FDS_PER_PROC; i++)
        proc->fds[i] = -1;
    
    for (int i = 0; i < MAX_DIRS_PER_PROC; i++)
        proc->dirs[i].in_use = 0;
    
    proc->exit_code = -1;
    proc->state = PROC_READY;
    proc->block = PROC_NONE;

    set_proc_name(proc, path);
    set_proc_cwd(proc, NULL);

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

void process_cleanup(process_t *proc) {
    for (int i = 0; i < MAX_FDS_PER_PROC; i++) {
        if (proc->fds[i] != -1) {
            file_close(proc->fds[i]);

            proc->fds[i] = -1;
        }
    }

    for (int i = 0; i < MAX_DIRS_PER_PROC; i++)
        proc->dirs[i].in_use = 0;

    if (proc->pd_phys) {
        paging_destroy_address_space(proc->pd_phys);

        proc->pd_phys = 0;
    }

    if (proc->kstack) {
        kfree(proc->kstack);

        proc->kstack = 0;
    }
}

void proc_unlink(process_t *proc) {
    if (proc_list == proc) {
        proc_list = proc->next;

        return;
    }

    for (process_t *p = proc_list; p != NULL; p = p->next) {
        if (p->next == proc) {
            p->next = proc->next;
            
            return;
        }
    }
}