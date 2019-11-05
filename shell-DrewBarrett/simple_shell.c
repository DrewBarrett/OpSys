#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

char * mygetline(FILE * file, int * size)
{
    char * myline = NULL;
    *size = 0;

    int c;

    while ((c = fgetc(file)) != EOF)
    {
        *size = *size + 1;
        myline = realloc(myline, sizeof(char) * (*size + 1));
        myline[*size - 1] = (c == '\n') ? '\0' : c;
        if (c == '\n') break;
    }
    return *size == 0 ? NULL : myline;
}

char ** parseArguments(char * string) {
    char ** r = NULL;
    char * s;
    s = strtok(string, " ");
    int size = 0;
    if (s == NULL) {
        return NULL;
    }
    do {
        size++;
        r = realloc(r, sizeof(char*) * (size + 1));
        r[size - 1] = s; // insert char * argument to list
        r[size] = NULL; // terminate array with nullptr
    } while ((s = strtok(NULL, " ")) != NULL);
    return r;
}

int main(int argc, char *argv[])
{
    // Check if we were given exactly one argument (command file)
    if (argc < 2) {
        printf("Error: You must provide a command file!\nSample Usage: %s command_file\n",
                argv[0]);
        return 1;
    }
    FILE * commandFile;
    commandFile = fopen(argv[1], "r");
    // Check if file is actually openable
    if (commandFile == NULL) {
        printf("Error: File %s is not readable!\n", argv[1]);
        return 2;
    }

    // Read lines from file
    int size;
    char * line;
    while ((line = mygetline(commandFile, &size)) != NULL){
        // Process command
        char ** args = parseArguments(line);
        if (args == NULL) {
            free(line);
            free(args);
            break;
        }
        pid_t pid = fork();
        if (pid == 0) {
            // we are child, kill ourselves for the greater good.
            int c = execvp(args[0], args);
            free(line);
            free(args);
            fclose(commandFile);
            return c;
        } else {
            // we are parent, wait until our child dies.
            int status;
            pid_t error = wait(&status);
            if (error == -1 || WEXITSTATUS(status) == -1 || WEXITSTATUS(status) == 255) {
                printf("Error: Invalid Command. Exiting...\n");
                free(args);
                free(line);
                fclose(commandFile);
                return 3;
            }
        }
        free(line);
        free(args);
    }

    fclose(commandFile);
    return 0;
}
