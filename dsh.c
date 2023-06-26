/*
Authors:	Jagtesh Chadha
Date:		14th November, 2006
Title:		Dhanu-sh - A simple Unix Shell
Version:	0.1
*/

#include <stdio.h>	  /* basic I/O */
#include <string.h>	  /* used for strtok the string tokenizer */
#include <stdlib.h>	  /* used for exit() */
#include <unistd.h>	  /* for STDIN_FILENO, STDOUT_FILENO definitions */
#include <fcntl.h>	  /* for O_RDONLY definition */
#include <dirent.h>	  /* for the dirent structur */
#include <sys/stat.h> /* for the stat structure */
#include <signal.h>	  /* for SIG_INT, SIG_IGN definition */
#include "dsh.h"

/* debug mode at 1, more descriptive than normal mode */
#define DEBUG
/* assumed size for an input buffer */
#define BUFSIZE 1024
/* assumed size for max arguments */
#define ARGNUM 200
/* default delimiter in input */
#define DELIMS "\n\r "
#define CMDNUM 15
#ifdef DEBUG
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif
#define ERROR(...) fprintf(stderr, __VA_ARGS__)
#define ENVVAR(var) (char *)getenv(var)

/* state types definition (list of all commands) */
enum
{
	BUILTIN_EXIT = 0,
	BUILTIN_SYS,
	BUILTIN_LS,
	BUILTIN_ECHO,
	BUILTIN_PWD,
	BUILTIN_CLEAR,
	BUILTIN_CD,
	BUILTIN_CAT,
	BUILTIN_MKDIR,
	BUILTIN_RMDIR,
	BUILTIN_HELP,
	BUILTIN_RM,
	BUILTIN_CHROOT,
	BUILTIN_MV,
	BUILTIN_CP
};

char *cmd[] = {"exit", "sys", "ls", "echo", "pwd", "clear", "cd", "cat", "mkdir", "rmdir", "help", "rm", "chroot", "mv", "cp"};
char *desc[] = {
	"Exit from the shell.",
	"Call an external command. Used especially in case an ambiguity arises between internal and external command names.",
	"List files in a directory.",
	"Print a string to standard output.",
	"Shows the present working directory.",
	"Clears the screen.",
	"Change to a directory.",
	"Concatenate files.",
	"Create one or more directories.",
	"Remove one or more directories.",
	"List all commands.",
	"Remove one or more files.",
	"Change the root directory.",
	"Move a file or a directory.",
	"Copy a file to another localtion"};
char *usage[] = {
	"exit",
	"sys (command)",
	"ls [dir list]",
	"echo [string]",
	"pwd",
	"clear",
	"cd [dir]",
	"cat (file list)",
	"mkdir (dir list)",
	"rmdir (dir list)",
	"help",
	"rm (file list)",
	"chroot (dir)",
	"mv (src) (dest)",
	"cp (src file) (dest file)"};

int state = -1;							   /* state variable, defines the current state */
int fd[2] = {STDIN_FILENO, STDOUT_FILENO}; /* file descriptors for input and output */
/* print the prompt */
void set_prompt()
{
	char buf[BUFSIZE + 1];
	char pwd[BUFSIZE + 1];

	LOG("set_prompt 1\n");
	/* produce a prompt like user@domain:/present/directory$ */
	getcwd(pwd, BUFSIZE);
	LOG("pwd: %s\n", pwd);

	char *hostname = getenv("HOSTNAME");
	LOG("set_prompt 2\n");
	if (hostname == NULL)
	{
		hostname = "localhost";
	}

	char prompt_symbol = (strcmp(ENVVAR("USER"), "root") == 0) ? '#' : '$';

	LOG("set_prompt 3\n");
	sprintf(buf, "dsh %s@%s %s %c ", ENVVAR("USER"), hostname, pwd, prompt_symbol);
	write(STDOUT_FILENO, buf, strlen(buf));
	LOG("set_prompt 4\n");
}

/* list files in a directory */
void list_dir(int n, char *tok[ARGNUM])
{
	DIR *dir;
	struct stat s;
	struct dirent *direntry;
	char dirpath[BUFSIZE + 1];

	/* assume pwd on no arguments */
	if (n == 0)
		getcwd(dirpath, BUFSIZE);
	else
		strcpy(dirpath, tok[1]);

#ifdef DEBUG
	fprintf(stderr, "dirpath: %s\n", dirpath);
#endif

	/* dir must exist and must have read permission */
	if ((dir = opendir(dirpath)) == NULL)
	{
		perror("ls");
		_exit(0);
	}

	/* print all the entries in direntry */
	/*	while ((direntry = readdir(dir)) != NULL)
			fprintf(stdout, "%6d %.10s\t", direntry->d_ino, direntry->d_name);

	*/

	while ((direntry = readdir(dir)) != NULL)
		fprintf(stdout, "%s\n", direntry->d_name);
	closedir(dir);
}

/* concatinate files */
void cat(int n, char *tok[])
{
	int i, j; /* i counts the tokens, j counts the file descriptors */
	FILE *file[ARGNUM];
	int ch;

	char errstr[BUFSIZE + 1]; /* for storing and displaying the error string */

	/* print help message if no argument is supplied */
	i = 1;
	j = 0;
	if (n == 0)
	{
		fprintf(stdout, "%s usage: %s\n%s    \n", cmd[BUILTIN_CAT], usage[BUILTIN_CAT], desc[BUILTIN_CAT]);
		return;
	}

	/* open the tokens in file descriptors */
	while (i <= n)
	{
		if ((file[j] = fopen(tok[i], "r")) != NULL)
			j++;
		else
		{
			strcpy(errstr, "cat: ");
			strcat(errstr, tok[i]);
			perror(errstr);
		}
		i++;
	}

	/* read a character from files and display it to stdout */
	i = 0;
	while (i < j) /* j will contain 1 more than the total no. of opened files */
	{
		while ((ch = getc(file[i])) != EOF)
		{
			putc(ch, stdout);
		}
		i++;
	}

	/* close all the files after reading */
	i = 0;
	while (i < j)
	{
		fclose(file[i]);
		i++;
	}
}

/* create one or more directories */
void create_dir(int n, char *tok[])
{
	char errstr[BUFSIZE + 1];

	/* start from the first argument */
	int i = 1;
	if (i == 0)
	{
		fprintf(stdout, "%s usage: %s\n%s    \n", cmd[BUILTIN_MKDIR], usage[BUILTIN_MKDIR], desc[BUILTIN_MKDIR]);
		return;
	}
	while (i <= n)
	{ /* set permission to 755 */
		if (mkdir(tok[i], S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) > 0)
		{
			/* if an error occurs, show the error */
			strcpy(errstr, "mkdir: ");
			strcat(errstr, tok[i]);
			perror(errstr);
		}
		i++;
	}
}

/* remove one or more directories */
void remove_dir(int n, char *tok[])
{
	char errstr[BUFSIZE + 1];

	/* start from the first argument */
	int i = 1;
	if (i == 0)
	{
		fprintf(stdout, "%s usage: %s\n%s    \n", cmd[BUILTIN_MKDIR], usage[BUILTIN_MKDIR], desc[BUILTIN_MKDIR]);
		return;
	}
	while (i <= n)
	{
		if (rmdir(tok[i]) == -1)
		{
			/* if an error occurs, show the error */
			strcpy(errstr, "rmdir: ");
			strcat(errstr, tok[i]);
			perror(errstr);
		}
		i++;
	}
}

/* remove files */
void remove_file(int n, char *tok[])
{
	char errstr[BUFSIZE + 1];

	/* start from the first argument */
	int i = 1;
	if (i == 0)
	{
		fprintf(stdout, "%s usage: %s\n%s    \n", cmd[BUILTIN_RM], usage[BUILTIN_RM], desc[BUILTIN_RM]);
		return;
	}
	while (i <= n)
	{
		if (unlink(tok[i]) == -1)
		{
			/* if an error occurs, show the error */
			strcpy(errstr, "rm: ");
			strcat(errstr, tok[i]);
			perror(errstr);
		}
		i++;
	}
}

/* change the root directory */
void change_root(int n, char *tok[])
{
	char errstr[BUFSIZE + 1];

	/* change the root directory only if more than one argument is specified */
	if (n == 0)
	{
		fprintf(stdout, "%s usage: %s\n%s    \n", cmd[BUILTIN_CHROOT], usage[BUILTIN_CHROOT], desc[BUILTIN_CHROOT]);
		return;
	}
	/* change the root directory to the first argument or print the error */
	else if (chroot(tok[1]) == -1)
	{
		/* if an error occurs, show the error */
		strcpy(errstr, "chroot: ");
		perror(errstr);
	}
}

/* copy a file */
void copy_file(int n, char *tok[])
{
	int file[2];
	char errstr[BUFSIZE + 1];
	char buf;
	int i;

	if (n == 0 || n > 2)
	{
		fprintf(stdout, "%s usage: %s\n%s    \n", cmd[BUILTIN_CP], usage[BUILTIN_CP], desc[BUILTIN_CP]);
		return;
	}

	if ((file[0] = open(tok[1], O_RDONLY)) < 0) /* source file cannot be opened */
	{
		perror("cp: ");
	}

	if ((file[1] = open(tok[2], O_WRONLY | O_CREAT | O_EXCL)) < 0) /* destination file cannot be opened */
	{
		perror("cp: ");
	}

	while (buf != EOF)
	{
		if (read(file[0], &buf, 1) > 0) /* read till EOF is not reached */
			write(file[1], &buf, 1);
		else
			break;
	}

	close(file[0]);
	close(file[1]);
}

/* move a file or directory */
void move(int n, char *tok[])
{
	if (n == 0 || n > 2)
	{
		fprintf(stdout, "%s usage: %s\n%s    \n", cmd[BUILTIN_MV], usage[BUILTIN_MV], desc[BUILTIN_MV]);
		return;
	}

	if (rename(tok[1], tok[2]) == -1)
	{
		perror("mv: ");
	}
}

/* ignores current line */
void ignore_line()
{
	fprintf(stdout, "\n");
	set_prompt();
}

char **tokenize(char *buffer)
{
	char **tokens = calloc(ARGNUM, sizeof(char *));
	// int token_count = 0;
	// char *token = NULL;
	// char *quote_start = NULL;
	// char *quote_end = NULL;

	// token = strtok(buffer, " ");
	// while (token != NULL && token_count < ARGNUM)
	// {
	// 	if (token[0] == '\"' || token[0] == '\'')
	// 	{
	// 		token = strtok(buffer, " ");
	// 		quote_end = strchr(quote_start, token[0]);

	// 		if (quote_end != NULL)
	// 		{
	// 			*quote_end = '\0';
	// 			tokens[token_count++] = quote_start;
	// 		}
	// 		else
	// 		{
	// 			fprintf(stderr, "Error: Missing closing quote\n");
	// 			break;
	// 		}
	// 	}
	// 	else
	// 	{
	// 		tokens[token_count++] = token;
	// 	}
	// 	LOG("[%d] %s", token_count, tokenize);

	// 	token = strtok(NULL, " ");
	// }

	// tokens[token_count] = NULL;

	tokens[0] = strtok(buffer, DELIMS);

	int i = 1;

	/* extract the remaining tokens */
	/* sending NULL in subsequent strtok calls will return the remaining tokens */
	while ((tokens[i] = strtok(NULL, DELIMS)) != NULL)
		i++;

	tokens[i] = NULL; /* very important to set the last token as NULL, for specifying the end of tokens */

	// i--; /* remove the extra 1 that is added to n */

	return tokens;
}

void *handle_signal(int signal_code)
{
	char buffer[200] = {'\0'};

	switch (signal_code)
	{
		case SIGQUIT:
			sprintf(buffer, "Exiting..\n");
			perror(buffer);
			exit(0);
			break;
		case SIGINT:
			ignore_line();
			break;
		default:
			perror(sprintf("Signal unhandled: %s", strsignal(signal_code)));
	}
}

void init_signals()
{
	struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handle_signal;
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

/* argc, *argv[] assume usual meaning */
int main(int argc, char *argv[])
{
	char buf[BUFSIZE + 1] = {'\0'};	   /* temporary buffer for storing the line input */
	char pwd[BUFSIZE + 1] = {'\0'};	   /* stores the present working directory */
	char errstr[BUFSIZE + 1] = {'\0'}; /* stores the error occured */
	char **tok = NULL;				   /* for storing the tokens */
	int n = 0, i = 0;				   /* n will store the no. of tokens starting from 0 */
	int returnval = 0;				   /* used by wait() */

	init_signals();
	LOG("Checkpoint 1\n");

	while (1)
	{
		/* very important: */
		/* reset the state to -1 */
		state = -1;

		LOG("Checkpoint 2\n");

		/* print the prompt with the pwd */
		set_prompt();
		LOG("Checkpoint 3\n");

		/* read input from STDIN */
		read(STDIN_FILENO, buf, BUFSIZE);
		LOG("buf: %s", buf);
		LOG("Checkpoint 4\n");

		/* extract the first token (the command)*/

		LOG("Checkpoint 5\n");
		tok = tokenize(buf);
		LOG("Checkpoint 6\n");
		int i = 0;
		for (i = 0; tok[i] != NULL; i++)
		{
			printf("token %d: %s\n", i, tok[i]);
		}
		LOG("Checkpoint 7\n");

		/* lookup on the first token */
		for (i = BUILTIN_EXIT; i < CMDNUM; i++)
			if (strcmp(tok[0], cmd[i]) == 0)
				state = i; /* set the state to its command equivalent */

		/* switch to a state (execute a command) based on the token */
		switch (state)
		{
		case BUILTIN_EXIT:
			return 0;

		case BUILTIN_SYS:
			switch (fork()) /* is this the child process? */
			{
			case 0:
				returnval = execvp(tok[1], &tok[1]); /* tok[1] because tok[0] would contain sys */
				exit(returnval);					 /* child always exits with 200 on success */
			default:
				wait(&returnval);
			}
			/* wait till the first child process dies */
			wait(&returnval);
			break;

		case BUILTIN_LS:
			list_dir(n, tok);
			break;

		case BUILTIN_ECHO:
			for (i = 1; i <= n; i++)
				fprintf(stdout, "%s ", tok[i]);
			fprintf(stdout, "\n");
			break;

		case BUILTIN_PWD:
			getcwd(pwd, BUFSIZE);
			fprintf(stdout, "%s\n", pwd);
			break;

		case BUILTIN_CLEAR:
			system(tok[0]);
			break;

		case BUILTIN_CD:
			/* if no argument is given, change to home directory */
			if ((n == 0) && (chdir(ENVVAR("HOME")) == -1))
			{
				{
					perror("cd");
				}
			}
			else if (chdir(tok[1]) == -1)
			{
				{
					perror("cd");
				}
			}
			break;

		case BUILTIN_CAT:
			cat(n, tok);
			break;

		case BUILTIN_MKDIR:
			create_dir(n, tok);
			break;

		case BUILTIN_RMDIR:
			remove_dir(n, tok);
			break;

		case BUILTIN_HELP:
			/* print a list of commands with basic usage help */
			fprintf(stdout, "You are running Dhanu-sh\nType 'help (command)' for more details\n");
			if (n == 0)
			{
				fprintf(stdout, "The following commands are available:\n");
				for (i = 0; i < CMDNUM; i++)
					fprintf(stdout, " %s\n", cmd[i]);
			}
			else
			{
				for (i = 0; i < CMDNUM; i++)
					if (strcmp(cmd[i], tok[1]) == 0)
						fprintf(stdout, " %s usage: %s\n%s    \n", cmd[i], usage[i], desc[i]);
			}
			break;

		case BUILTIN_RM:
			remove_file(n, tok);
			break;

		case BUILTIN_CHROOT:
			change_root(n, tok);
			break;

		case BUILTIN_MV:
			move(n, tok);
			break;

		case BUILTIN_CP:
			copy_file(n, tok);
			break;

		default:
			switch (fork())
			{
			case 0:
				returnval = execvp(tok[0], &tok[0]);
				fprintf(stderr, "%s: No such file or directory\n", tok[0]);
				exit(returnval);
			default:
				wait(&returnval);
			}
		}
		LOG("Checkpoint 8\n");
	}
	return 0;
}
