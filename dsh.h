#include <signal.h> /* for SIG_INT, SIG_IGN definition */

void set_prompt();
void list_dir(int, char *tokens[]);
void cat(int n, char *tokens[]);
void create_dir(int n, char *tokens[]);
void remove_dir(int n, char *tokens[]);
void remove_file(int n, char *tokens[]);
void change_root(int n, char *tokens[]);
void copy_file(int n, char *tokens[]);
void move(int n, char *tokens[]);
void ignore_line();
char **tokensenize(char *buffer);
void *handle_signal(int signal_code);
int main(int argc, char *argv[]);
