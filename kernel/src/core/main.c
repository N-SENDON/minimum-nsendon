#include <stdint.h>
#include "minemu/irq.h"
#include "minemu/boot.h"
#include "minemu/trap.h"
#include "minemu/trace.h"
#include "minemu/platform.h"

#define LINE_MAX 20

static volatile char input_buffer[LINE_MAX + 1];
static volatile uint32_t input_length;
static volatile uint32_t overflow_count;
static volatile uint32_t line_ready;

// Helper function to clean up code and ensure that character is sent when tx status is ready
void uart_putc(char c) {
// While loop to continously check TX_READY status
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
        //wait
    }
        
    //Send the character
    MINEMU_UART0->tx_data = (uint32_t)c;
}

// Send string one character at a time
static void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s);
        ++s;
    }
}

// IRQ dispatcher based on irq-context-switch example
struct minemu_trap_frame *
minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    uint32_t source = (uint32_t)frame->exception_id;

    if (source == MINEMU_IRQ_UART0) {

        // Read every byte before EOI
        while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
            char c = (char)MINEMU_UART0->rx_data;

            // Reads byte even if the previous line has not been processed
            // This is to drain UART
            if (line_ready) {
                continue;
            }

            // Newline ends the command
            if (c == '\n') {
                input_buffer[input_length] = '\0';
                line_ready = 1;
            }

            // Both 0x08 and 0x7f are backspace.
            else if (c == 0x08 || c == 0x7f) {
                if (overflow_count > 0) {
                    --overflow_count;
                }
                else if (input_length > 0) {
                    --input_length;
                }
            }

            // Stores the input while under the 20-byte limit
            else if (input_length < LINE_MAX) {
                input_buffer[input_length] = c;
                ++input_length;
            }

            // For characters past the limit
            else {
                ++overflow_count;
            }
        }
    }

    MINEMU_INTERRUPT->eoi = source;

    return frame;
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
 
     // Allows UART to generate RX interrupt
    MINEMU_UART0->control =
        MINEMU_UART_CONTROL_RX_IRQ_ENABLE;

    // Enables UART in the interrupt controller
    MINEMU_INTERRUPT->enable =
        UINT32_C(1) << MINEMU_IRQ_UART0;

    // Enables IRQs globally
    minemu_irq_enable();

    uart_puts("msh> ");

    for (;;) {
        if (line_ready) {
            char line[LINE_MAX + 1];

            uint32_t length;
            uint32_t i;
            uint32_t command_start;
            uint32_t command_end;

            // Shared state used by both main and IRQ handler
            // Disable IRQs while copying and resetting
            minemu_irq_disable();

            length = input_length;

            for (i = 0; i < length; ++i) {
                line[i] = input_buffer[i];
            }

            line[length] = '\0';

            input_length = 0;
            overflow_count = 0;
            line_ready = 0;

            minemu_irq_enable();

            // Ignore spaces before command
            i = 0;

            while (i < length && line[i] == ' ') {
                ++i;
            }

            // Case for empty or all-space line
            if (i == length) {
            }

            else {
                // Save the beginning of the command
                command_start = i;

                // Find end of first word
                while (i < length && line[i] != ' ') {
                    ++i;
                }

                command_end = i;

                // Check if first word is "echo"
                if ((command_end - command_start) == 4 &&
                    line[command_start] == 'e' &&
                    line[command_start + 1] == 'c' &&
                    line[command_start + 2] == 'h' &&
                    line[command_start + 3] == 'o') {

                    // Spaces after echo count as a single separator
                    while (i < length && line[i] == ' ') {
                        ++i;
                    }

                    // Print everything after echo
                    while (i < length) {
                        uart_putc(line[i]);
                        ++i;
                    }

                    uart_putc('\n');
                }

                // Handle for unknown command
                else {
                    uart_puts("command not found: ");

                    for (i = command_start;
                         i < command_end;
                         ++i) {
                        uart_putc(line[i]);
                    }

                    uart_putc('\n');
                }
            }

            uart_puts("msh> ");
        }

        __asm__ volatile("nop");
    }

}
