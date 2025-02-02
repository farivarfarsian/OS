#include "kernel.h"
#include "console.h"
#include "string.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "multiboot.h"
#include "pmm.h"

KERNEL_MEMORY_MAP g_kmap;

int get_kernel_memory_map(KERNEL_MEMORY_MAP *kmap, MULTIBOOT_INFO *mboot_info) {
    uint32 i;
    
    if (kmap == NULL) return -1;
    kmap->kernel.k_start_addr = (uint32)&__kernel_section_start;
    kmap->kernel.k_end_addr = (uint32)&__kernel_section_end;
    kmap->kernel.k_len = ((uint32)&__kernel_section_end - (uint32)&__kernel_section_start);

    kmap->kernel.text_start_addr = (uint32)&__kernel_text_section_start;
    kmap->kernel.text_end_addr = (uint32)&__kernel_text_section_end;
    kmap->kernel.text_len = ((uint32)&__kernel_text_section_end - (uint32)&__kernel_text_section_start);

    kmap->kernel.data_start_addr = (uint32)&__kernel_data_section_start;
    kmap->kernel.data_end_addr = (uint32)&__kernel_data_section_end; 
    kmap->kernel.data_len = ((uint32)&__kernel_data_section_end - (uint32)&__kernel_data_section_start);

    kmap->kernel.rodata_start_addr = (uint32)&__kernel_rodata_section_start;
    kmap->kernel.rodata_end_addr = (uint32)&__kernel_rodata_section_end;
    kmap->kernel.rodata_len = ((uint32)&__kernel_rodata_section_end - (uint32)&__kernel_rodata_section_start);

    kmap->kernel.bss_start_addr = (uint32)&__kernel_bss_section_start;
    kmap->kernel.bss_end_addr = (uint32)&__kernel_bss_section_end; 
    kmap->kernel.bss_len = ((uint32)&__kernel_bss_section_end - (uint32)&__kernel_bss_section_start);

    kmap->system.total_memory = mboot_info->mem_low + mboot_info->mem_high;

    for (i = 0; i < mboot_info->mmap_length; i += sizeof(MULTIBOOT_MEMORY_MAP)) {
        MULTIBOOT_MEMORY_MAP *mmap = (MULTIBOOT_MEMORY_MAP *)(mboot_info->mmap_addr + i);
        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;
        // make sure kernel is loaded at 0x100000 by bootloader(see linker.ld)
        if (mmap->addr_low == kmap->kernel.text_start_addr) {
            // set available memory starting from end of our kernel, leaving 1024 bytes
            kmap->available.start_addr = kmap->kernel.k_end_addr + 1024;
            kmap->available.end_addr = mmap->addr_low + mmap->len_low;
            // get availabel memory in bytes
            kmap->available.size = kmap->available.end_addr - kmap->available.start_addr;
            return 0;
        }
    }

    return -1;
}

void display_kernel_memory_map(KERNEL_MEMORY_MAP *kmap) {
    printf("kernel:\n");
    printf("  kernel-start: 0x%x, kernel-end: 0x%x, TOTAL: %d bytes\n", 
            kmap->kernel.k_start_addr, kmap->kernel.k_end_addr, kmap->kernel.k_len);
    printf("  text-start: 0x%x, text-end: 0x%x, TOTAL: %d bytes\n", 
            kmap->kernel.text_start_addr, kmap->kernel.text_end_addr, kmap->kernel.text_len);
    printf("  data-start: 0x%x, data-end: 0x%x, TOTAL: %d bytes\n", 
            kmap->kernel.data_start_addr, kmap->kernel.data_end_addr, kmap->kernel.data_len);
    printf("  rodata-start: 0x%x, rodata-end: 0x%x, TOTAL: %d\n",
            kmap->kernel.rodata_start_addr, kmap->kernel.rodata_end_addr, kmap->kernel.rodata_len);
    printf("  bss-start: 0x%x, bss-end: 0x%x, TOTAL: %d\n",
            kmap->kernel.bss_start_addr, kmap->kernel.bss_end_addr, kmap->kernel.bss_len);

    printf("total_memory: %d KB\n", kmap->system.total_memory);
    printf("available:\n");
    printf("  start_adddr: 0x%x\n  end_addr: 0x%x\n  size: %d\n", 
            kmap->available.start_addr, kmap->available.end_addr, kmap->available.size);
}


void kmain(unsigned long magic, unsigned long addr) {
    MULTIBOOT_INFO *mboot_info;

    gdt_init();
    idt_init();

    console_init(COLOR_WHITE, COLOR_BLACK);
    if(magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        mboot_info = (MULTIBOOT_INFO *)addr;
        memset(&g_kmap, 0, sizeof(KERNEL_MEMORY_MAP));
        if (get_kernel_memory_map(&g_kmap, mboot_info) < 0) {
            printf("error: failed to get kernel memory map\n");
            return;
        }
        //display_kernel_memory_map(&g_kmap);
        printf("total_memory: %d KB\n", g_kmap.system.total_memory);

        // put the memory bitmap at the start of the available memory
        pmm_init(g_kmap.available.start_addr, g_kmap.available.size);

        printf("Max blocks: %d\n", pmm_get_max_blocks());
        // initialize a region of memory of size (4096 * 10), 10 blocks memory
        pmm_init_region(g_kmap.available.start_addr, PMM_BLOCK_SIZE * 10);

        printf("[KERNEL REGION 0-%d] [ALWAYS IN USE]\n\n", pmm_next_free_frame(1) - 1);
        printf("before alloc- next free: %d\n", pmm_next_free_frame(1));

        uint32 *p1 = pmm_alloc_block();
        printf("block allocated at 0x%x, next free: %d\n", p1, pmm_next_free_frame(1));

        uint32 *p2 = pmm_alloc_blocks(3);
        printf("blocks allocated 0x%x, next free: %d\n", p2, pmm_next_free_frame(1));

        uint32 *p3 = pmm_alloc_block();
        printf("block allocated at 0x%x, next free: %d\n", p3, pmm_next_free_frame(1));

        // usage
        printf("usage:-\n");
        memset(p1, 0, PMM_BLOCK_SIZE);
        p1[0] = 123;
        p1[1] = 456;
        p1[2] = 789;
        printf("array:\n");
        printf("  0: %d, 1: %d, 2: %d\n", p1[0], p1[1], p1[2]);

        struct example {
            int id;
            char name[32];
        };

        struct example *e = (struct example *)pmm_alloc_block();
        e->id = 10012;
        strcpy(e->name, "Iron Man");
        printf("struct:\n");
        printf("  id: %d, name: %s\n", e->id, e->name);

        printf("\nfreeing all blocks:\n");
        pmm_free_block(p1);
        pmm_free_blocks(p2, 3);
        pmm_free_block(p3);
        pmm_free_block((void *)e);

        printf("next free: %d\n", pmm_next_free_frame(1));

        pmm_deinit_region(g_kmap.available.start_addr, PMM_BLOCK_SIZE * 10);
    } else {
        printf("error: invalid multiboot magic number\n");
    }
}


