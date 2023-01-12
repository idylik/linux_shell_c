#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/wait.h>

#define true 1
#define false 0
#define bool int

typedef int error_code;

#define ERROR (-1)
#define HAS_ERROR(code) ((code) < 0)
#define NULL_TERMINATOR '\0'

enum op {
    BIDON, NONE, OR, AND   //BIDON is just to make NONE=1, BIDON is unused
};

typedef struct Command Command;
struct Command {
    char **call;
    enum op operator;
    struct Command *next;
};

//Linked list container
typedef struct CommandList CommandList;
struct CommandList {
    struct Command *firstCommand;
    bool background;
};

void freeCommandList(CommandList *commandList) {

    Command* currentCommand = commandList->firstCommand;
    Command* previousCommand;

    while (currentCommand != NULL) {
        previousCommand = currentCommand;
        currentCommand = currentCommand->next;
        free(previousCommand->call);
        free(previousCommand);
    }

    free(commandList);

}


error_code readline(char **out) {
    size_t size = 10;                       // size of the char array
    char *line = malloc(sizeof(char) * size);       // initialize a ten-char line
    if (line == NULL) return ERROR;   // if we can't, terminate because of a memory issue

    for (int at = 0; 6; at++) {

        //Double size of array if at >= size
        if (at >= size) {
            size *= 2;
            line = realloc(line, sizeof(char)*size);
            if (line == NULL) return ERROR;
        }

        int ch = getchar(); //[fixed: use int instead of char]
        if (ch == '\n') {        // if we get a newline
            line[at] = NULL_TERMINATOR;    // finish the line with return 0
            break;
        }
        line[at] = ch; // sets ch at the current index and increments the index
    }

    *out = line;
    return 0;
}


error_code parseLine(char **pointerLine, CommandList **out) {

    CommandList *commandList = malloc(sizeof(*commandList)); //linked list control structure

    Command *currentCommandObject = malloc(sizeof(*currentCommandObject)); //first command of the chain
    if (commandList == NULL || currentCommandObject == NULL) exit(-1);

    commandList->firstCommand = currentCommandObject;
    currentCommandObject->operator = NONE;

    char* line = *pointerLine;

    int sizeofLine = strlen(line);
    currentCommandObject->call = malloc(sizeof(char*)*sizeofLine); //first call;
    if (currentCommandObject->call == NULL) exit(-1);
    bool txtChar = false;

    int i = 0; //line index
    int j = 0; //call index

    while (true) {

        //AND OR
        if ((line[i] == '&' && line[i+1] == '&')
            ||  (line[i] == '|' && line[i+1] == '|')) {

            //Create new node
            Command *newCommandObject = malloc(sizeof(*newCommandObject)); //Address of Next node object
            if (newCommandObject == NULL) exit(-1);

            newCommandObject->call = malloc(sizeof(char*)*sizeofLine);
            if (newCommandObject->call == NULL) exit(-1);

            currentCommandObject->next = newCommandObject; //Address of the next node
            newCommandObject->operator = (line[i] == '&') ? AND : OR;

            //Close current node
            currentCommandObject->call[j] = NULL; //last index must be null
            line[i] = '\0';
            txtChar = false;

            //Make new node current node
            currentCommandObject = newCommandObject;

            i++; //Skip one more character
            j = 0; //Reset call index

        //END OF LINE or BACKGROUND
        } else if ((line[i] == '\0') || (line[i] == '&' && (line[i+1] == '\0' || line[i+1] == ' '))) {

            //Complete this as last node
            currentCommandObject->call[j] = NULL; //last index must be null
            currentCommandObject->next = NULL;

            if (line[i] == '&') commandList->background = true;

            break;

        }  else if (txtChar == false && line[i] != ' ') {
            txtChar = true;
            currentCommandObject->call[j] = &line[i]; //New pointer
            j++;

        } else if (line[i] == ' ') {
            line[i] = '\0';
            txtChar = false;
        }

        i++; //Next char
    }

    *out = commandList;
    return 0;

}

int checkRN(char **call) {

    int nbRepetitions = 1;
    int p = '(';
    char* pointerP = strchr(call[0], p);

    if (call[0][0] == 'r' && pointerP) {

        //Get number of repetitions
        pointerP[0] = '\0';
        char* numStr = &(call[0][1]);
        nbRepetitions = atoi(numStr);

        //Point to start of inner command
        call[0] = &(pointerP[1]);

        //S'il y a des espaces après '(' marche pas car les args sont décalés de 1

        //Find last argument and replace ')' with \0
        int i=0;
        while (call[i+1] != NULL) {
            i++;
        }
        //Erase last ')'
        int p = ')';
        char* pointerP2 = strchr(call[i], p);
        if (pointerP2 != NULL) *pointerP2 = '\0';

    }

    return nbRepetitions;
}


//Returns success=0 or success=1 of call
int executeCommand(char** call) {

    int success = 1;
    int nbRepetitions = checkRN(call);

    for (int i=0; i < nbRepetitions; i++) {

        pid_t pid;
        pid = fork();

        if (pid < 0) { //error
            printf("Fork failed");
            exit(-1);
        } else if (pid == 0) { //Child

            execvp(call[0], call);
            printf("%s: ", call[0]);
            printf("command not found\n");
            exit(-1);
        } else { //parent
            int status;
            waitpid(pid, &status, NULL);

            if (WEXITSTATUS(status) == 255) {
                success = 0;
            }
        }
    }

    return success;
}

error_code executeCommandList(CommandList* commandList) {

    Command* command = commandList->firstCommand;
    enum op operator;
    int successPrevious;

    while(command != NULL) {

        operator = command->operator;

        if ((operator == NONE)
            || (operator == AND && successPrevious)
            || (operator == OR && successPrevious == 0)) {
            successPrevious = executeCommand(command->call);
        } else if ((operator == AND && successPrevious == 0)
            ||(operator == OR && successPrevious)){
            //Do nothing
        }

        command = command->next;
    }

}



int main (void) {    

    char *line;

    while(1) {

        //READ LINE
        error_code read = readline(&line);

        //If error, exit
        if (read == ERROR) {  //to do what about error_code?
            free(line);
            exit(-1);
        }

        if(strcmp(line, "exit") == 0) {
            free(line);
            //If there are children processes running, wait for them to finish
            wait(NULL);
            exit(0);
        }

        //PARSE LINE:
        //if line empty
        if (strlen(line) == 0) {
            continue;
        }

        CommandList* commandList;
        parseLine(&line, &commandList);

        //If command is set to run in the background
        if (commandList->background) {
            pid_t pid;
            pid = fork();
            if (pid < 0) { //error
                printf("Fork failed");
                exit(-1);
            } else if (pid == 0) {
                //Execute line in child
                executeCommandList(commandList);
                freeCommandList(commandList);
                free(line);
                exit(0);
            } else { //Parent must also free parsed commandList before fork
                freeCommandList(commandList);
            }

        } else {

            //EXECUTE LINE normally
            executeCommandList(commandList);
            freeCommandList(commandList);
        }

        free(line);
    }

}
