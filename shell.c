// Matthew Kim CSS503 A
// Modified: 13/04/22
// ------------------------------------------
// Implements a bash shell that is able to process one >, <, or |
// command. Includes ability to process command with & at the end.
// --------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h> // fork, wait 
#include <sys/wait.h>  // wait 
#include <fcntl.h> 

#define MAX_LINE 80 /* The maximum length command */
#define MAX_HISTORY 100 //maximum number of command lines saved in history

//Reads input from console.
//Return: NULL if input is blank or empty line. Otherwise, returns pointer to
//the input string.
char* readInput() {
  char* line = NULL;
  size_t len = 0;
  int lineSize = 0;
  lineSize = getline(&line, &len, stdin);

  if (line != NULL && line[lineSize - 1] == '\n') { 
    char* temp = line;
    line = (char*) malloc(lineSize);
    strncpy(line, temp, lineSize - 1);
    line[lineSize - 1] = '\0';
    lineSize--;
    free(temp);
  }
  
  if (line == NULL || lineSize == 0) { //if empty
    free(line);
    return NULL;
  } else {
    return line;
  }
}

//Frees all dynamically allocated stored in the args[] array.
//Parameter: args is the array of dynamically allocated strings.
//Postcondition: args[] is filled with NULL.
void freeArgs(char** args) {
  int i = 0;
  while (args[i] != NULL && i < (MAX_LINE/2 + 1)) {
    free(args[i]);
    args[i] = NULL;
    i++;
  }
}

//Parses a string into the args[] array.
//Parameter: argLine is the string to be parsed. args is the array of
//dynamically allocated strings.
void parseArg(char* argLine, char** args) {
  int lineSize = strlen(argLine);
  char line[lineSize + 1]; //temp line
  strncpy(line, argLine, lineSize + 1);
  int pipeLocation = -1;

  char* token = strtok(line, " "); //grabs first token
  int index = 0;
  
  while (token != NULL) {
    int tokenSize = strlen(token);
    args[index] = (char*) malloc(tokenSize + 1);
    strncpy(args[index], token, tokenSize + 1);
    index++;
    token = strtok(NULL, " "); //grabs next token
  }
}

//Checks if the user command ends with "&".
//Return: True if the user command ends with "&" else false.
bool checkAmper(char** args) {
  int argCount = 0;
  char* word = args[argCount];
  while(word != NULL) {
    argCount++;
    word = args[argCount];
  }
  if (strcmp(args[argCount - 1], "&") == 0) {
    free(args[argCount - 1]);
    args[argCount - 1] = NULL;
    return true;
  } else {
    return false;
  }
}

//Returns the type of command in args[] array. Returns 0 for regular command
//with no redirects. Returns 1 for redirect output. Returns 2 for redirect
//input. Returns 3 for pipe. Returns -1 if command is not valid.
int cmdType(char** args, char** leftArgs, char** rightArgs) {
  int cmdType = 0;
  int splitLoc = -1;
  int i = 0;
  char* word = args[i];

  while(word != NULL && i < 10) {
    bool isTypeZero = cmdType == 0;
    bool isRedirOut = strcmp(word, ">") == 0;
    bool isRedirIn = strcmp(word, "<") == 0;
    bool isPipe = strcmp(word, "|") == 0;
    if (!isTypeZero && (isRedirOut || isRedirIn || isPipe)) {
      cmdType = -1;
    } else if (isRedirOut) {
      cmdType = 1;
      splitLoc = i;
    } else if (isRedirIn) {
      cmdType = 2;
      splitLoc = i;
    } else if (isPipe) {
      cmdType = 3;
      splitLoc = i;
    }
    i++;
    word = args[i];
  }

  if (cmdType == 1 || cmdType == 2 || cmdType == 3) {
    for(int j = 0; j < splitLoc; j++) {
      leftArgs[j] = args[j];
    }
    for(int k = splitLoc + 1; k < i; k++) {
      rightArgs[k - splitLoc - 1] = args[k];
    }
  }
  return cmdType;
}

//Executes processes from commands in args[].
void execCmd(int ctype, char** args, char** leftArgs, char** rightArgs, bool isAmper) {
  int fd = 1;
  if (ctype == 1) {
    fd = open(rightArgs[0], O_CREAT | O_TRUNC | O_WRONLY) ;
  } else if (ctype == 2) {
    fd = open(rightArgs[0], O_RDONLY);
  }
  if (fd < 0) {
    printf("File open failed\n");
    return;
  }
  
  int pid = fork();
  if (pid == -1) {
    printf("Failed to create child process\n");
    return;
  } else if (pid == 0) { //child process
    if (ctype == 1) {
      dup2(fd, STDOUT_FILENO);
    } else if (ctype == 2) {
      dup2(fd, STDIN_FILENO);
    }
    
    int success;
    if (ctype == 0) {
      success = execvp(args[0], args);
    } else if (ctype == 1 || ctype == 2) {
      success = execvp(leftArgs[0], leftArgs);
    }

    if (success < 0) {
      dup2(1, STDOUT_FILENO);
      printf("Unable to execute command\n");
      exit(1);
    }
  } else { //parent process
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);
    if (!isAmper) {
      waitpid(pid, &status, 0); //waits for child to exit
      return;
    }
  }
}

//Executes processes from commands in args[] for piped command.
void execPipeCmd(char** leftArgs, char** rightArgs, bool isAmper) {
  int pid = 1;
  int pid2 = 1;
  int status;
  int status1;
  int status2;
  int success1;
  int success2;
  int pipefd[2]; //0 is read, 1 is write
  pid = fork();

  if (pid == -1) {
    printf("Failed to create child process\n");
    exit(1);
  } else if (pid == 0) { //process child1
    pipe(pipefd); //create pipe
    pid2 = fork();

    //process child2
    if (pid2 == -1) {
      printf("Failed to create child process\n");
      success2 = -1;
      exit(1);
    } else if (pid2 == 0) { 
      //run LH command
      close(pipefd[0]); //close read
      dup2(pipefd[1], STDOUT_FILENO); //change stdout to pipe's write
      success2 = execvp(leftArgs[0], leftArgs); //execute LH command and exit
      if (success2 < 0) {
        dup2(1, STDOUT_FILENO);
        printf("Unable to execute command\n");
        exit(1);
      } else {
        dup2(1, STDOUT_FILENO);
        printf("success2 is good\n");
      }
    }

    //run RH command
    if (success2 >= 0) {
      waitpid(pid2, &status2, 0);
      close(pipefd[1]); //close write
      dup2(pipefd[0], STDIN_FILENO); //change stdin to pipe's read
      success1 = execvp(rightArgs[0], rightArgs);
      if (success1 < 0) {
        dup2(1, STDOUT_FILENO);
        printf("Unable to execute command\n");
        exit(1);
      }
    }
  } else { //in parent
    while (waitpid(-1, &status, WNOHANG) > 0);
    if (!isAmper) {
      waitpid(pid, &status1, 0); //wait for child1 to exit
    }
  }
}

int main() {
  char* args[MAX_LINE/2 + 1] = {NULL}; // list of parsed command line arguments
  int should_run = 1; // flag to determine when to exit program
  int pipeLocation = 0;
  char* history = NULL; //pointer to previous command line

  while (should_run) {
    printf("osh>");
    fflush(stdout);
    bool isValidCmd = true;
    char* inputLine = readInput(); //current user command line
    if (inputLine != NULL) { //checks if the user entry is blank
      char* argLine = NULL; //holds the string of arguments to be processed

      if (strcmp(inputLine, "exit") == 0) { //exits when user enters "exit"
        should_run = 0;
        break;
      } else if (strcmp(inputLine, "!!") == 0 && history == NULL) { 
        free(inputLine);
        printf("No commands in history\n");
        isValidCmd = false;
      } else if (strcmp(inputLine, "!!") == 0 && history != NULL) { //recalls history when user enters "!!"
        printf("%s\n", history);
        argLine = history; //update inputLine command
      } else { //when it is a regular command
        history = inputLine;
        argLine = inputLine;
      }
      if (isValidCmd) {
        parseArg(argLine, args); //parse into args[]
      
        bool isAmper = checkAmper(args);
        char* leftArgs[MAX_LINE/2 + 1] = {NULL};
        char* rightArgs[MAX_LINE/2 + 1] = {NULL};
        
        int ctype = cmdType(args, leftArgs, rightArgs);

        if (ctype == -1) {
          printf("Unable to execute command\n");
        } else if (ctype == 0 || ctype == 1 || ctype == 2) {
          execCmd(ctype, args, leftArgs, rightArgs, isAmper);
        } else if (ctype == 3) { //pipe command
          execPipeCmd(leftArgs, rightArgs, isAmper);
        }

        freeArgs(args); //clear out args[]
      }
    }
  }
  return 0;
}