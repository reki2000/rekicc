#include "types.h"
#include "rsys.h"
#include "devtool.h"
#include "rstring.h"
#include "vec.h"

#include "file.h"
#include "macro.h"

#define NUM_MACROS 1000

macro_t macros[NUM_MACROS];
int macro_len = 0;

void add_macro(const char *name, int start_pos, int end_pos, char_p_vec *vars) {
    if (macro_len >= NUM_MACROS) {
        error("too many macros");
    }
    macro_t *m = &macros[macro_len++];
    m->name = strdup(name);
    m->src = src;
    m->start_pos = start_pos;
    m->end_pos = end_pos;
    m->vars = vars;

    debug("added macro: %s as |%s|" , name, dump_file(src->id, start_pos, end_pos));
    for (int i=0; i<m->vars->len; i++) {
        debug(" args:%d %s", i, m->vars->items[i],i);
    }
}

macro_t *find_macro(const char *name) {
    for (int i=0; i<macro_len; i++) {
        if (strcmp(macros[i].name, name) == 0) {
            return &macros[i];
        }
    }
    return 0;
}

extern int src_file_len;
extern int src_file_stack_top;
extern src_t src_files[];
extern int src_file_id_stack[];
extern bool exit_file();
extern src_t *get_current_file();

VEC_HEADER(macro_t *, macro_p_vec)
VEC_BODY(macro_t *, macro_p_vec)

typedef struct {
    macro_t *m;
    macro_p_vec *args;
} macro_frame_t;

VEC_HEADER(macro_frame_t *, macro_frame_p_vec)
VEC_BODY(macro_frame_t *, macro_frame_p_vec)

macro_frame_p_vec *macro_frames = NULL;

bool is_in_expanding(const char *name) {
    for (int i=0; i<macro_frames->len; i++) {
        if (strcmp(macro_frames->items[i]->m->name, name) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

void read_macro_arg(bool is_last_arg) {
    char_vec *paren_stack = char_vec_new();
    for(;;) {
        int c = ch();
        //debug("read: %c %d %c %c %c", c, paren_stack->len, paren_stack->items[0], paren_stack->items[1], paren_stack->items[2]);

        if (c == '[' || c == '(') {
            char_vec_push(paren_stack, c);
        } else if (c == ']') {
            if (paren_stack->len == 0 || char_vec_pop(paren_stack) != '[') error("stray ']' in macro args");
        } else if (c == ')') {
            if (paren_stack->len == 0 && is_last_arg) break;
            if (paren_stack->len == 0 || char_vec_pop(paren_stack) != '(') error("stray ')' in macro args");
        } else if (c == ',') {
            if (paren_stack->len == 0 && !is_last_arg) break;
        } else if (c == '"') {
            for (;;) {
                if (c == '"') break;
                if (c == '\\') {
                    next(); // skip '"' after '\\'
                }
                next();
            }
        } else if (is_eof()) {
            error("stray eof while parsing macro args");
        }
        next();
    }
}

void enter_macro_file(macro_t *m) {
    if (src_file_len >= 100) {
        error("too much include files");
    }

    src_t *s = &src_files[src_file_len];

    s->filename = m->name;

    s->id = src_file_len;
    s->body = m->src->body;

    s->pos = m->start_pos;
    s->len = m->end_pos + 1;
    s->line = 1;
    s->column = 1;

    s->prev_line = s->line;
    s->prev_column = s->column;
    s->prev_pos = s->pos;

    src_file_len++;

    src_file_id_stack[src_file_stack_top] = s->id;
    src_file_stack_top++;
    src = get_current_file();
}

void build_macro_env(macro_t *m) {

    macro_frame_t *frame = (macro_frame_t *)malloc(sizeof(macro_frame_t));
    frame->m = m;
    frame->args = macro_p_vec_new();

    if (m->vars->len > 0) {
        debug("scanning macro's actual args...");
        if (ch() != '(') error("macro %s requires actual arguments in (...)", m->name);
        next();

        // parse args
        for (int i=0; i<m->vars->len; i++) {
            int spos = src->pos;
            read_macro_arg(i==m->vars->len-1);
            int epos = src->pos-1;

            // store the macro's actual argument value into macro_t 
            macro_t *arg = (macro_t *)malloc(sizeof(macro_t));
            arg->name = strdup(m->vars->items[i]);
            arg->src = src;
            arg->start_pos = spos;
            arg->end_pos = epos;
            arg->vars = char_p_vec_new();
            macro_p_vec_push(frame->args, arg);

            debug("scanned args: %s as |%s|" , arg->name, dump_file(src->id, spos, epos));
            next();
        }
    }

    macro_frame_p_vec_push(macro_frames, frame);

    enter_macro_file(m);

    debug("entering macro: %s", src->filename);
}

bool enter_macro_arg(const char *name) {
    if (is_in_expanding(name)) return FALSE;

    if (macro_frames->len == 0) return FALSE;

    macro_frame_t *frame = macro_frame_p_vec_top(macro_frames);
    for (int i=0; i<frame->args->len; i++) {
        macro_t *arg = frame->args->items[i];
        if (strcmp(arg->name, name) == 0) {
            debug("expanding macro arg: %s as %s", name, dump_file(arg->src->id, arg->start_pos, arg->end_pos));
            build_macro_env(arg);
            return TRUE;
        }
    }
    return FALSE;
}

bool exit_macro_arg() {
    return exit_macro(); // for this time, the same code
}

bool enter_macro(const char *name) {
    if (macro_frames == NULL) {
        macro_frames = macro_frame_p_vec_new();
    }

    macro_t *m = find_macro(name);
    if (!m) return FALSE;

    if (is_in_expanding(name)) { // do not expand current macro again
        return FALSE;
    }

    build_macro_env(m);
    return TRUE;
}

bool exit_macro() {
    macro_frame_p_vec_pop(macro_frames);
    return exit_file();
}
