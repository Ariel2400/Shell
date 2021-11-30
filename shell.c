

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//error codes
#define EXEC_FAILED 1
#define FORK_FAILED 2
#define CHDIR_FAILED 3
#define CD_ARGUMENT_ERROR 4
#define GENRAL_ERROR 5

//max length
#define MAX_LENGTH 100

#define DELIM " "
#define BACKGROUND_FLAG "&"
#define PROMPT "$ "

//store the data of a generic command
typedef struct
{
    char command_string[MAX_LENGTH];
    pid_t command_pid;
    bool command_is_running;
} Command;

//global variables. store data about the current command
char command_as_string[MAX_LENGTH];
char *command_as_args[MAX_LENGTH];
bool should_parent_wait = true;

//a stack of all commands
Command history[MAX_LENGTH];
int history_i = 0;

//buffer that stores the last dir. for recovery when typed 'cd -'
char recently_used_dir[MAX_LENGTH];

//prints an error message.
void print_error_msg(int error_code)
{
    switch (error_code)
    {
    case EXEC_FAILED:
        printf("exec failed\n");
        break;
    case FORK_FAILED:
        printf("fork failed\n");
        break;
    case CHDIR_FAILED:
        printf("chdir failed\n");
        break;
    case CD_ARGUMENT_ERROR:
        printf("Too many arguments\n");
        break;
    default:
        printf("An error occurred\n");
    }
}

//reads the command from the input
void read_command()
{
    fgets(command_as_string, MAX_LENGTH, stdin);

    int pos = 0;
    bool is_blank_space = true;
    // removes '\n' and blank lines, making the string proessable
    while (command_as_string[pos] != '\0')
    {
        //reached the end of the
        if (command_as_string[pos] == '\n')
        {
            if (is_blank_space)
            {
                //get another input
                read_command();
                return;
            }
            //end the string in the EOL
            command_as_string[pos] = '\0';
            break;
        }
        else if (is_blank_space && command_as_string[pos] != ' ')
        {
            is_blank_space = false;
        }

        pos++;
    }
}

//parses the command from a string to an array of arguments
void parse_command()
{
    char *token;
    int pos = 0;

    token = strtok(command_as_string, DELIM);
    while (token != NULL)
    {
        command_as_args[pos] = token;
        pos++;
        token = strtok(NULL, DELIM);
    }

    //checks if the command sholud run in the back ground
    if (strcmp(command_as_args[pos - 1], BACKGROUND_FLAG) == 0)
    {
        pos--;
        should_parent_wait = false;
    }
    else
    {
        should_parent_wait = true;
    }
    command_as_args[pos] = NULL;
}

//adds the current command to the history
void add_to_history()
{
    strcpy(history[history_i].command_string, command_as_args[0]);
    int i = 1;
    while (command_as_args[i] != NULL)
    {
        strcat(history[history_i].command_string, DELIM);
        strcat(history[history_i].command_string, command_as_args[i]);
        i++;
    }
}

// //implementation for the 'jobs' command
void execute_command_jobs()
{
    int i;
    for (i = 0; i < history_i; i++)
    {
        //check if it ran in the background
        if (history[i].command_is_running)
            //check if the process still runs
            if (waitpid(history[i].command_pid, NULL, WNOHANG) == 0)
            {
                printf("%s\n", history[i].command_string);
            }
            else
            {
                //update the is_running
                history[i].command_is_running = false;
            }
    }
}

//implementation for the 'history' command
void execute_command_history()
{
    int i;
    for ( i = 0; i < history_i; i++)
    {
        //if it ran in the back ground
        if (history[i].command_is_running)
        //chechk if it's still running
            if (waitpid(history[i].command_pid, NULL, WNOHANG) != 0)
            {
                history[i].command_is_running = false;
            }
        printf("%s ", history[i].command_string);
        fflush(stdout);
        history[i].command_is_running ? printf("RUNNING\n") : printf("DONE\n");
    }
    //you to hard-code the history itself.
    printf("history RUNNING\n");
}

//implementation for the 'cd' command
void execute_command_cd()
{
    //general guidelines:when success, update the 'recently_used_dir' to cwd
    //when failure, print error and do nothing
    char cwd[MAX_LENGTH];
    getcwd(cwd, MAX_LENGTH);
    char *home = getenv("HOME");

    // 'cd' is equivelent to 'cd ~'
    if (command_as_args[1] == NULL)
    {
        if (chdir(home) == -1)
        {
            print_error_msg(CHDIR_FAILED);
        }
        else
        {
            strcpy(recently_used_dir, cwd);
        }
        return;
    }

    //check number of arguments
    if (command_as_args[2] != NULL)
    {
        print_error_msg(CD_ARGUMENT_ERROR);
        return;
    }

    //handels the case 'cd -'
    if (strcmp(command_as_args[1], "-") == 0)
    {
        if (chdir(recently_used_dir) == -1)
        {
            print_error_msg(CHDIR_FAILED);
        }
        else if (strcmp(recently_used_dir, cwd) != 0)
        {
            strcpy(recently_used_dir, cwd);
        }
        return;
    }

    //generic case+use of ~
    char path[MAX_LENGTH] = "\0";
    if (command_as_args[1][0] == '~')
    {
        //append the home directory, then the rest of the path(if there's any)
        strcat(path, home);
        if(strlen(command_as_args[1])>1){
            strcat(path, command_as_args[1] + 1);
        }
    }
    else
    {
        strcat(path, command_as_args[1]);
    }

    if (chdir(path) == -1)
    {
        print_error_msg(CHDIR_FAILED);
    }
    else
    {
        strcpy(recently_used_dir, cwd);
    }
}

//implementation for the 'exit' command
int execute_command_exit()
{
    exit(0);
}

// execute a generic command, that requires a child process
void execute_nonbulitin_command()
{
    //handels the echo command and the string which is printed
    if (strcmp(command_as_args[0], "echo") == 0)
    {
        int len=0, i;
        for (i = 0; command_as_args[i] != NULL; i++)
        {
            char *tmp = command_as_args[i];
            len = strlen(tmp);
            //if an argument start and ends with brackets
            if (tmp[0] == '\"' && tmp[0] == tmp[len - 1])
            {
                //a buffer with length of temp, minus 2 brackets, plus '\0'
                char buff[len - 1];
                strncpy(buff, tmp + 1, len - 2);
                buff[len - 2] = '\0';
                strcpy(tmp, buff);
            }
        }
    }

    //creating a child process and executing the command there
    pid_t pid = fork();

    if (pid == 0)
    {
        //child process
        if (execvp(command_as_args[0], command_as_args) < 0)
        {
            print_error_msg(EXEC_FAILED);
            exit(1);
        }
        exit(0);
    }
    else if (pid == -1)
    {
        //fork failed
        print_error_msg(FORK_FAILED);
    }
    else
    {
        //parent process
        history[history_i].command_pid = pid;
        if (should_parent_wait)
        {
            waitpid(pid, NULL, 0); // wait for it to finish
            history[history_i].command_is_running = false;
        }
        else
        {
            history[history_i].command_is_running = true;
        }
    }
}

//handles the executing of the parsed command
void execute_command()
{
    //adds command to history
    add_to_history();

    //all implementations are preformed in the parnet process and in the foreground
    if (strcmp(command_as_args[0], "exit") == 0)
    {

        history[history_i].command_is_running = false;
        history[history_i].command_pid = 0;
        execute_command_exit();
    }
    else if (strcmp(command_as_args[0], "cd") == 0)
    {
        history[history_i].command_is_running = false;
        history[history_i].command_pid = 0;
        execute_command_cd();
    }
    else if (strcmp(command_as_args[0], "history") == 0)
    {
        history[history_i].command_is_running = false;
        history[history_i].command_pid = 0;
        execute_command_history();
    }
    else if (strcmp(command_as_args[0], "jobs") == 0)
    {
        history[history_i].command_is_running = false;
        history[history_i].command_pid = 0;
        execute_command_jobs();
    }
    else
    {
        execute_nonbulitin_command();
    }
    //pushed the command to the history stack, update pointer to top
    history_i++;
}

//main loop
int main()
{
    getcwd(recently_used_dir, MAX_LENGTH);
    do
    {
        printf(PROMPT);
        fflush(stdout);
        read_command();
        parse_command();
        execute_command();
        
    } while (1);

    return 0;
}
