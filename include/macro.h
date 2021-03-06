typedef struct {
    char *name;
    src_t *src;
    int start_pos;
    int end_pos;
    char_p_vec vars;
} macro_t;

void add_macro(const char *name, int start_pos, int end_pos, char_p_vec vars);
void delete_macro(const char *name);
macro_t *find_macro(const char *name);

bool enter_macro(const char *name);
bool exit_macro();
void extract_macro(char *buf);
