#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <ctype.h>

#include "seashell.h"
#include "execution.h"
#include "ncurses_proxy.h"
#include "arguments_manager.h"
#include "autocompletion.h"

#define BUFFER_LEN  256

#define CHAR_BS     0x08 // back space '\b'
#define CHAR_TAB    0x09 // horizontal tab '\t'
#define CHAR_NL     0x0A // new line '\n'
#define CHAR_CR     0x0D // carriage return '\r'
#define CHAR_ESC    0x1B // escape
#define CHAR_SB     0x5B // [
#define CHAR_DELETE 0x7E // delete key
#define CHAR_DEL    0x7F // DEL

#define CHAR_A      0x41 // [ A ETX
#define CHAR_B      0x42 // [ B ETX
#define CHAR_C      0x43 // [ C ETX
#define CHAR_D      0x44 // [ D ETX

#define CHAR_SOH    0x01 // start of heading - ctrl-a
#define CHAR_ENQ    0x05 // enquiery - ctrl-e
#define CHAR_ETB    0x17 // end of trans. blk - ctrl-w
#define CHAR_FF     0x0C // form feed - ctrl-l

/////////////////////////////////////////////////////////////////////

/* move cursor one space to the left */
static void cursor_left(struct shell *ctx)
{
    if (ctx->pos_x > 0) {
        nc_cursor_left();
        ctx->pos_x -= 1;
    }
}

/* move cursor from n space to the left */
static int cursor_n_left(struct shell *ctx, unsigned int n)
{
    int nb = -1;
    int size = ctx->pos_x - n;

    if (size < 0)
        size = 0;

    if (size >= 0 && n > 0) {
        nb = fprintf(stdout, "\x1B[%dD", n);
        fflush(stdout);
    }

    return nb;
}

/* move cursor from one space to the right */
static void cursor_right(struct shell *ctx)
{
    if (ctx->pos_x < ctx->line_size) {
        nc_cursor_right();
        ctx->pos_x += 1;
    }
}

/////////////////////////////////////////////////////////////////////

static int print_fct(const char *fmt, ...)
{
    int ret = -1;
    char str[BUFFER_LEN] = {0};
    va_list arg;

    va_start(arg, fmt);
    ret = vsnprintf(str, BUFFER_LEN, fmt, arg);
    va_end(arg);

    return write(1, str, ret);
}

static void print_prompt(const char *prompt)
{
    print_fct(prompt);
}

static void print_line(const struct shell *ctx, const char *line)
{
    nc_delete_line();
    print_fct("\r%s%s", ctx->prompt, line);
}

/////////////////////////////////////////////////////////////////////

static int insert_char(char *buffer, char c, unsigned int pos)
{
    size_t len = strlen(buffer);

    if (len == 0) {
        buffer[0] = c;
        return 0;
    }

    while (len > pos) {
        buffer[len + 1] = buffer[len];
        len--;
    }

    buffer[len + 1] = buffer[len];
    buffer[len] = c;

    return 0;
}

static int remove_char(char *buffer, unsigned int pos)
{
    size_t len = strlen(buffer);

    if (pos >= len) {
        buffer[len - 1] = '\0';
        return 0;
    }

    while (pos < len) {
        buffer[pos] = buffer[pos + 1];
        pos++;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////

static void set_cursor_pos(struct shell *ctx, unsigned int nb)
{
    ctx->pos_x = nb;
    ctx->line_size = nb;
}

static void backspace_key(struct shell *ctx, char *buffer)
{
    if (ctx->pos_x > 0) {
        cursor_left(ctx);
        remove_char(buffer, ctx->pos_x);
        nc_save_cursor();
        print_line(ctx, buffer);
        nc_restore_cursor();
        ctx->line_size -= 1;
    }
}

static void delete_key(struct shell *ctx, char *buffer)
{
    if (ctx->pos_x < ctx->line_size) {
        nc_delete_char();
        remove_char(buffer, ctx->pos_x);
        ctx->line_size -= 1;
    }
}

static int read_arrow_key(struct shell *ctx, const char c)
{
    switch (c) {
        case CHAR_A: // arrow up
            hist_set_next_entry(ctx->history_hdl);
            return 1;
        case CHAR_B: // arrow down
            hist_set_prev_entry(ctx->history_hdl);
            return 1;
        case CHAR_C: // arrow right
            cursor_right(ctx);
            break;
        case CHAR_D: // arrow left
            cursor_left(ctx);
            break;
    }

    return 0;
}

/* M A I N   L O O P */
static int read_keyboard(struct shell *ctx, const char *buffer, ssize_t bytes_read)
{
    // DEBUG
    // printf("[%d][%d][%d][%d]\n", buffer[0], buffer[1], buffer[2], buffer[3]);
    static char line[BUFFER_LEN] = {0};

    /* loop over every received characters (printable and special characters) */
    int i;
    for (i = 0; i < bytes_read; i++) {

        /* split the processing between printable and special characters for one input character */
        if (isprint(buffer[i]) != 0) {

            /* insert char in buffer line */
            insert_char(line, buffer[i], ctx->pos_x);
            /* write the char on STDOUT */
            write(1, &buffer[i], 1);
            /* set usefull variable */
            ctx->pos_x += 1;
            ctx->line_size = strlen(line);

        } else {

            /* handle special characters */
            switch (buffer[i]) {
                case CHAR_BS:
                    printf("BS\n");
                    break;
                case CHAR_DEL: /* backspace keycode */
                    backspace_key(ctx, line);
                    break;
                case CHAR_CR: /* enter keycode */
                    write(1, "\r\n", 2);
                    if (strcmp("exit", line) == 0) {
                        ctx->exit = 1;
                        return EXIT_SUCCESS;
                    }

                    if (strcmp(line, "\0") != 0) {
                        hist_add_entry(ctx->history_hdl, line);
                        /* execute the command */
                        execution(line);
                    }

                    print_prompt(ctx->prompt);

                    /* set usefull variable */
                    memset(line, 0, BUFFER_LEN);
                    set_cursor_pos(ctx, 0);
                    hist_set_head_entry(ctx->history_hdl);
                    break;
                case CHAR_ESC:
                    if (buffer[i + 3] == CHAR_DELETE) { /* delete keycode */
                        delete_key(ctx, line);
                        /* delete escape sequence is 4 characters long then add 4 to the iterator */
                        i += 4;
                        break;
                    }
                    if (read_arrow_key(ctx, buffer[i + 2]) != 0) {
                        hist_get_current_entry(ctx->history_hdl, line, BUFFER_LEN);
                        set_cursor_pos(ctx, strlen(line));
                        print_line(ctx, line);
                    }
                    /* arrow key escape sequence is 3 characters long then add 3 to the iterator */
                    i += 3;
                    break;
                case CHAR_TAB: /* tab keycode */
                    write(1, "\r\n", 2);
                    if (*line != '\0') {
                        char *ptr = line;
                        char *pos = strrchr(line, ' ');
                        if (pos)
                            ptr = pos + 1;
                        if (*ptr != ' ')
                            autocompletion(ptr, ctx->envp);

                    }
                    print_line(ctx, line);
                    set_cursor_pos(ctx, strlen(line));
                    break;
                case CHAR_SOH: /* ctrl-a */
                    cursor_n_left(ctx, ctx->pos_x);
                    ctx->pos_x = 0;
                    break;
                case CHAR_ENQ: /* ctrl-e */
                    print_line(ctx, line);
                    set_cursor_pos(ctx, strlen(line));
                    break;
                case CHAR_ETB: /* ctrl-w */
                    memmove(&line[0], &line[ctx->pos_x], ctx->line_size);
                    print_line(ctx, line);
                    ctx->pos_x = 0;
                    ctx->line_size = strlen(line);
                    cursor_n_left(ctx, ctx->line_size);
                    break;
                case CHAR_FF: /* ctrl-l */
                    nc_clear_screen();
                    print_prompt(ctx->prompt);
                    memset(line, 0, BUFFER_LEN);
                    break;
                default:
                    break;
            } /* end switch special character processing */
        } /* end handling for one input (printable and special character) */
    } /* end for loop */

    return 0;
}

/////////////////////////////////////////////////////////////////////

// ctrl + c handler for clean up
// check if there is better solution than global variable
// keep global_save to clean the context in case of SIGINT
static struct shell *global_save;
static int shell_deinit(struct shell *ctx);
static void signal_handler(__attribute__((unused)) int signum)
{
    write(1, "^C\r\n", 4);
    shell_deinit(global_save);
    exit(EXIT_SUCCESS);
}

/////////////////////////////////////////////////////////////////////

static struct shell *shell_init(struct arguments *args, char **envp)
{
    args = args;

    // 1) set signal handler: only for SIGINT atm, see later to extend it
    struct sigaction action = {
        .sa_handler = signal_handler,
        .sa_flags = 0
    };

    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("sigaction");
        goto clean;
    }

    // 2) create a new user shell context
    struct shell *new = calloc(1, sizeof(struct shell));
    if (!new) {
        perror("calloc");
        goto clean;
    }

    // 3) set the user prompt
    memcpy(new->prompt, "Cli>", PROMPT_LEN);

    // 4) initialize history
    new->history_hdl = hist_init(".seashell_history");
    if (!new->history_hdl) {
        fprintf(stderr, "hist_init failed\n");
        goto clean_shell;
    }

    // 5) save the old terminal configuration to be able to reuse it
    // when leaving the seashell
    if (nc_get_terminal(&(new->saved_cfg)) == -1)
        goto clean_history;

    // 6) initialize current terminal session
    if (nc_init_terminal() == -1)
        goto clean_terminal;

    // 7) initialize terminfo database
    if (nc_init_terminal_data() != 1)
        goto clean_terminal;

    // TODO: do the clear_screen at the begining optional
    // do the configuration from cfg file ?
    //nc_clear_screen();
    nc_enter_insert_mode();
    set_cursor_pos(new, 0);

    new->envp = envp;

    // 8) keep global_save to clean the context in case of SIGINT
    // and retrieve the terminal context
    global_save = new;

    return new;

clean_terminal:
    nc_set_terminal(&(new->saved_cfg));
clean_history:
    hist_deinit(new->history_hdl);
clean_shell:
    free(new);
clean:
    return NULL;
}

static int shell_start(struct shell *ctx)
{
    char buffer[BUFFER_LEN] = {0};
    ssize_t bytes_read = 0;

    print_prompt(ctx->prompt);
    while (ctx->exit != 1) {

        memset(buffer, '\0', BUFFER_LEN);

        bytes_read = read(STDIN_FILENO, buffer, BUFFER_LEN);
        if (bytes_read == -1)
            return -1;

        read_keyboard(ctx, buffer, bytes_read);
    }

    return 0;
}

static int shell_deinit(struct shell *ctx)
{
    if (!ctx)
        return 0;

    nc_exit_insert_mode();

    hist_deinit(ctx->history_hdl);

    // reset the terminal as it was before launching seashell
    if (nc_set_terminal(&(ctx->saved_cfg)) == -1)
        return -1;

    free(ctx);

    return 0;
}

int start_shell(int argc, char *argv[], char **envp)
{
    struct arguments shell_args;
    args_get_arguments(argc, argv, &shell_args);

    struct shell *ctx = shell_init(&shell_args, envp);
    if (!ctx) {
        fprintf(stderr, "shell_init:failed\n");
        goto clean;
    }

    if (shell_start(ctx) == -1) {
        fprintf(stderr, "shell_start:failed\n");
        goto clean_shell;
    }

    if (shell_deinit(ctx) == -1) {
        fprintf(stderr, "shell_deinit:failed\n");
        goto rough_clean;
    }

    return 0;

rough_clean:
    free(ctx);
clean_shell:
    shell_deinit(ctx);
clean:
    return -1;
}

