#include "minemu/boot.h"
#include "minemu/trap.h"
#include "minemu/trace.h"
#include "minemu/platform.h"

// Helper function to clean up code and ensure that character is sent when tx status is ready
void uart_putc(char c) {
// While loop to continously check TX_READY status
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
        //wait
    }
        
    //Send the character
    MINEMU_UART0->tx_data = (uint32_t)c;
}

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->system_rom_base != UINT32_C(0x08000000) ||
        boot_info->direct_map_vaddr != UINT32_C(0xc0000000) ||
        boot_info->direct_map_paddr != UINT32_C(0x40000000) ||
        boot_info->direct_map_size != UINT32_C(0x04000000)) {
        minemu_trace_event(UINT32_C(0xb007bad0));
        minemu_fail_stop();
    }
    // HW#1 - Task 1
    uart_putc('h');
    uart_putc('e');
    uart_putc('l');
    uart_putc('l');
    uart_putc('o');
    uart_putc(' ');
    uart_putc('w');
    uart_putc('o');
    uart_putc('r');
    uart_putc('l');
    uart_putc('d');

    minemu_trace_event(1);
    minemu_fail_stop();
}
