#include <stdint.h>
#include "minemu/user_abi.h"

#define LINE_MAX 64U

static void uart_write(const char *buffer, uint32_t length) {
    if (length == 0U) {
        return;
    }

    (void)ioctl(
        MINEMU_FD_UART_OUT,
        MINEMU_IOCTL_UART_WRITE,
        (void *)buffer,
        length
    );
}

static void uart_puts(const char *s) {
    uint32_t length = 0;

    while (s[length] != '\0') {
        length++;
    }

    uart_write(s, length);
}

static char uart_getc(void) {
    char c;

    while (ioctl(
        MINEMU_FD_UART_IN,
        MINEMU_IOCTL_UART_READ,
        &c,
        1U
    ) != 1) {
    }

    return c;
}

static void process_command(char *line, uint32_t length) {
    uint32_t i = 0;
    uint32_t command_start;
    uint32_t command_end;

    while (i < length && line[i] == ' ') {
        i++;
    }

    if (i == length) {
        return;
    }

    command_start = i;

    while (i < length && line[i] != ' ') {
        i++;
    }

    command_end = i;

    if (command_end - command_start == 4U &&
        line[command_start] == 'e' &&
        line[command_start + 1U] == 'c' &&
        line[command_start + 2U] == 'h' &&
        line[command_start + 3U] == 'o') {

        char output[LINE_MAX + 1U];
        uint32_t output_length = 0;
        uint32_t pending_space = 0;

        while (i < length && line[i] == ' ') {
            i++;
        }

        while (i < length) {
            char c = line[i++];

            if (c == ' ') {
                if (output_length > 0U) {
                    pending_space = 1U;
                }
            } else {
                if (pending_space) {
                    output[output_length++] = ' ';
                    pending_space = 0U;
                }

                output[output_length++] = c;
            }
        }

        output[output_length++] = '\n';
        uart_write(output, output_length);

    } else {
        uart_puts("command not found: ");

        uart_write(
            line + command_start,
            command_end - command_start
        );

        uart_puts("\n");
    }
}

void minemu_user_main(void) {
    char input_buffer[LINE_MAX + 1U];

    uint32_t input_length = 0;
    uint32_t overflow_count = 0;

    uart_puts("msh> ");

    for (;;) {
        char c = uart_getc();

        if (c == '\n') {

            if (overflow_count == 0U) {
                input_buffer[input_length] = '\0';
                process_command(input_buffer, input_length);
            }

            input_length = 0;
            overflow_count = 0;

            uart_puts("msh> ");

        } else if (c == '\b' || c == 0x7f) {

            if (overflow_count > 0U) {
                overflow_count--;
            } else if (input_length > 0U) {
                input_length--;
            }

        } else if (input_length < LINE_MAX) {

            input_buffer[input_length++] = c;

        } else {
            overflow_count++;
        }
    }
}