/* Kaylan Mettus
CS 370
Project 2

This is a simple shell that reads input from the user, executes commands, and performs other tasks. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/types.h>

//helper function declarations
void printPrompt();
void readCommand(char **, char **, int *, int *);
void execCommand(char **);
void pipeCommand(char **, int *);
void mergeCommand(char **);
void addHistory(char **, char *, int *, int *);
void clearHistory(char **);
int killShell();

//byte values for keyboard keys
static const int ARROWBYTEONE = 27;
static const int ARROWBYTETWO = 91;
static const int UP = 65;
static const int DOWN = 66;
static const int BACKSP = 8;
static const int DEL = 127;

static const int BUFSIZE = 256;
static const int HISTORYSIZE = 10;

int main()
{
	//get the original terminal configuration
	struct termios origConfig;
	tcgetattr(0, &origConfig);
	
	//start with a copy of the original configuration and
	//adjust it
	struct termios newConfig = origConfig;
	//enable special characters, disable echo
	newConfig.c_lflag &= ~(ICANON | ECHO); 
	newConfig.c_cc[VMIN] = 10;
	newConfig.c_cc[VTIME] = 2;
	
	//set new configuration
	tcsetattr(0, TCSANOW, &newConfig);
	
	//string array for tokens
	char **args = (char **)calloc(BUFSIZE, sizeof(char *));
	//string array for command history
	int newest = 0, histCount = 0;
	//allocates a circular array for command history
	char **history = (char**)calloc(HISTORYSIZE, sizeof(char *));
	
	//loops until shell is exited
	while (1) 
	{
		printPrompt();
		readCommand(args, history, &newest, &histCount);

		//if cd command is entered, change the current directory
		if (strcmp(args[0],"cd") == 0)
		{
			putchar('\n');
			if (chdir(args[1]) == -1)
				{perror(NULL);}
		}
		//check for merge command and execute
		else if (strcmp(args[0], "merge") == 0)
			{mergeCommand(args);}
		//if exit command is entered, quit the shell
		else if (strcmp(args[0],"exit") == 0)
		{
			if (killShell())
				break;
		}
		//otherwise execute the command the user has entered
		else
			{execCommand(args);}
		
	}

	clearHistory(history);
	free(args);
	
	//restore input configs
	tcsetattr(0, TCSANOW, &origConfig);

	return 0;
}

//prints the current working directory followed by '>' character to the screen
void printPrompt()
{
	char *buf = (char *)calloc(BUFSIZE, sizeof(char));
	//get current working directory
	getcwd(buf, BUFSIZE);
	printf("%s->", buf);
	free(buf);
}

//reads and tokenizes user input.  Detects if a pipe command has been
//entered and executes it
void readCommand(char **argArr, char **histArr, int *new, int *count)
{
	int pos = 0;
	int curr = *new;
	int tempCount = *count;
	//buffer for user input
	char str[BUFSIZE];
	char *buf = str;
	
	char in = getchar();	
	
	while (in == '\n')
	{
		putchar('\n');
		printPrompt();
		in = getchar();
	}
	
	//read user input
	while (in != '\n')
	{
		if (in == ARROWBYTEONE)
		{
			//handle arrow keys
			in = getchar();
			if (in != ARROWBYTETWO)
				//handle esc key
			in = getchar();
			if (in == UP)
			{
				//move backward through the history
				if ((tempCount) > 0)
				{
					//clear chars from screen and buffer
					while (pos > 0)
					{
						pos--;
						putchar('\b');
					}
					printf("%s", histArr[curr]);
					//place history command in buffer and reset pos
					strcpy(buf, histArr[curr]);
					//buf = strdup(histArr[curr]); //check this
					pos = strlen(histArr[curr]) - 1;
					curr = (curr - 1) % HISTORYSIZE;
					tempCount--;
				}
			}
			else
			{
				//move forward through the history
				if (tempCount < (*count))
				{
					//clear chars from screen and buffer
					while (pos > 0)
					{
						pos--;
						putchar('\b');
					}
					curr = (curr + 1) % HISTORYSIZE;
					printf("%s", histArr[curr]);
					//place history command in buffer and reset pos
					strcpy(buf, histArr[curr]); //check this
					pos = strlen(histArr[curr]) - 1;
					tempCount++;
				}
			}
		}
		else if (in == BACKSP || in == DEL)
		{
			//handle backspace and delete
			if (pos > 0)
			{
				pos--;
				putchar('\b');
			}
		}
		else
		{
			//handle all other keys
			putchar(in);
			buf[pos] = in;
			pos++;
			//add bounds check
		}
		in = getchar();
	}
	buf[pos] = '\0';
	
	//add string to history
	//addHistory(histArr, buf, new, count);
	*new = (*new + 1) % HISTORYSIZE;
	histArr[*new] = strdup(buf);
	if (*count < HISTORYSIZE)
		(*count)++;
		
	int argC = 0;
	//flag for pipe command
	int pipe = 0;
	//get input tokens and store as arguments
	char *tok = strtok(buf, " \n");
	while (tok != NULL)
	{
		if (strcmp(tok, "|") == 0)
		{
			pipe = 1;
			pos = argC;
		}
		argArr[argC] = strdup(tok);
		argC++;
		tok = strtok(NULL, " \n");
		//NOTE: add bounds check
	}
	//null terminate the string array
	argArr[argC] = NULL;
	
	if (pipe == 1)
		pipeCommand(argArr, &pos);
}

//runs the user command in a new process and waits for completion
void execCommand(char **args)
{
	int pid, status;
	//create a new process and save the process id
	putchar('\n');
	pid = fork();

	//if pid is greater than 0, it is the parent process
	if (pid > 0)
	{
		//wait for child process to complete
		waitpid(pid, &status, WUNTRACED);
	}
	//if pid is 0, it is the child process
	else if (pid == 0)
	{
		//execute the command, print error if unsuccessful
		if (execvp(args[0], args) == -1)
		{
			perror(NULL);
			exit(1);
		}

		printPrompt();
	}
	//if pid is negative, an error has occurred
	else
	{
		perror(NULL);
	}
}

//executes two commands, using the output of the first as the
//input of the second
void pipeCommand(char **argArr, int *pos)
{
	int pid1, pid2, status;
	char **cmd1 = (char **)calloc(BUFSIZE, sizeof(char *));
	char **cmd2 = (char **)calloc(BUFSIZE, sizeof(char *));
	int i = 0;
	//file descriptor, [0] for input and [1] for output
	int fd[2];
	
	//split array into the two commands
	while (i < (*pos))
	{
		cmd1[i] = strdup(argArr[i]);
		i++;
	}
	i++; //skip | operator
	while (argArr[i] != NULL)
	{
		cmd2[i] = strdup(argArr[i]);
		i++;
		//add bounds check
	}
	
	//create a child process for one command
	putchar('\n');
	pid1 = fork();

	//if pid is greater than 0, it is the parent process
	if (pid1 > 0)
	{
		//wait for child process to complete
		waitpid(pid1, &status, WUNTRACED);
	}
	//if pid is 0, it is the child process
	else if (pid1 == 0)
	{
		//create a second child process for second command
		pid2 = fork();
	
		//grandchild process executes the second command
		if (pid2 == 0)
		{
			close(0);
			dup(fd[0]);
			close(fd[1]);
			execvp(cmd2[0], cmd2);
		}
		//child process executes the first command
		else if (pid2 > 0)
		{
			close(1);
			dup(fd[1]);
			close(fd[0]);
			execvp(cmd1[0], cmd1);
		}
		else
		{
			perror(NULL);
		}
	}
	//if pid is negative, an error has occurred
	else
	{
		perror(NULL);
	}
	
	free(cmd1);
	free(cmd2);
}

//the merge command combines two text files into a third
//expected format is "src1.txt src2.txt > dest.txt"
void mergeCommand(char **argArr)
{
	char *cmd = "cat";
	
	//error checks
	//if ( != 5)
		//printf("Error: invalid number of arguments.");
	if (strstr(argArr[1], ".txt") == NULL)
		printf("Error: argument one must be a text file.");
	else if (strstr(argArr[2], ".txt") == NULL)
		printf("Error: argument two must be a text file.");
	else if (strcmp(argArr[3], ">") != 0)
		printf("Error: argument three must be > operator.");
	else if (strstr(argArr[4], ".txt") == NULL)
		printf("Error: argument four must be a text file.");
	else
	{
		//cat argArr[1] argArr[2] > argArr[4]
		argArr[0] = strdup(cmd);
		execCommand(argArr);
	}
}

//adds a string to the history, overrwriting the oldest string and
//updates beginning and end markers
void addHistory(char **circArr, char *str, int *new, int *count)
{
	*new = (*new + 1) % HISTORYSIZE;
	circArr[*new] = strdup(str);
	if (*count < HISTORYSIZE)
		(*count)++;
}

//clears out history and deallocates memory
void clearHistory(char **circArr)
{
	for (int i = 0; i < HISTORYSIZE; i++)
		circArr[i] = NULL;
		
	free(circArr);
	circArr = NULL;
}

//Verifies that user wants to exit shell, returns 1 if yes and 0 if no
int killShell()
{
	char in = '\0';
	
	do{
	printf("\nDo you want to exit the shell?\nEnter Y for yes or N for no.\n");
	in = getchar();
	in = toupper(in);
	}while (in != 'Y' && in != 'N');
	
	return in == 'Y' ? 1 : 0;
}
