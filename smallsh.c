/********************************************************************************
* Program Name: smallsh.c
* Author: Mathew Kagel
* Date: 2018-03-04
* Description: This program runs a small shell by taking command line input from
*   the user and executing the commands. 
********************************************************************************/
#include "CommandLine.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

int forkCount = 0; /* Count of fork calls that are currently running */
int countBG = 0; /* Count of background processes that haven't been reported to */
                 /* the user yet */
pid_t processBG[512]; /* Array of background processes */

bool firstStop = false; /* Used by SIGTSTP to tell the shell to enter foreground only mode */
bool secondStop = false; /* Used by SIGTSTP to tell the shell to leave foreground only mode */

struct statusValues { /* Used by the "status" command to report exit status or */
  int exitStatus;     /* terminate signal, but not both */
  int termSignal;
};

/********************************************************************************
* Description: catchSIGTSTP()
*   This function is the signal handler for SIGTSTP.
********************************************************************************/
void catchSIGTSTP(int signo) {
  if (firstStop == false) {
    char *message = "\nEntering foreground-only mode (& is now ignored)\n";
    write(STDOUT_FILENO, message, 50);
    firstStop = true;
    secondStop = false;
  } else if (secondStop == false) {
    char *message = "\nExiting foreground-only mode\n";
    write(STDOUT_FILENO, message, 31);
    firstStop = false;
    secondStop = true;
  }
}


/********************************************************************************
* Description: catchSIGUSR1()
*   This function is the signal handler for SIGUSR1.
********************************************************************************/
void catchSIGUSR1(int signo) {
  char *message;
  char number[20];
  int n;
  int i;
  int j;
  int maxProcessToClear = 10; /* Can clear 10 background processes at once */
  int processesCleared = 0;  
  int index;
  pid_t childPid = -5;  
  int childExitMethod = -5;
  int exitStatus = -5;
  int termSignal = -5; 

  /* If there are background processes, terminate them one by one */
  if (countBG > 0) {
    do {
      fflush(stdout);
      /* Look for process that has finished and is zombied */
      childPid = waitpid(-1, &childExitMethod, WNOHANG);
  
      /* If a child process has been found then terminate the process */
      if (childPid > 0) {
        /* Match the child PID to an index in processBG[] */
        for (i = 0; i < countBG; i++) {
          if (childPid == processBG[i]) {
            index = i;
            break;
          }
        }
        
        /* Determine exit/terminate condition */
        if (WIFEXITED(childExitMethod) != 0) {
          exitStatus = WEXITSTATUS(childExitMethod);
        } else if (WIFSIGNALED(childExitMethod) != 0) {
          termSignal = WTERMSIG(childExitMethod); 
        }

        /* Print appropriate message for how background process terminated */
        message = "background pid ";
        write(STDOUT_FILENO, message, 15);
        n = sprintf(number, "%d", processBG[index]);
        write(STDOUT_FILENO, number, n);
        message = " is done: ";
        write(STDOUT_FILENO, message, 10);
        if (exitStatus >= 0) {
          message = "exit value ";
          write(STDOUT_FILENO, message, 11);
          n = sprintf(number, "%d", childExitMethod);
          write(STDOUT_FILENO, number, n); 
        } else if (termSignal >= 0) {
          message = "terminated by signal ";
          write(STDOUT_FILENO, message, 21);
          n = sprintf(number, "%d", childExitMethod);
          write(STDOUT_FILENO, number, n);
        }
        message = "\n";
        write(STDOUT_FILENO, message, 1);
        
        /* Remove child PID from processBG[], fill the gap left behind */
        if (index != countBG) {
          for (j = index; j < countBG - 1; j++) {
            processBG[j] = processBG[j + 1];
          }
        }
     
        /* Reset exit/terminate values */
        exitStatus = -5;
        termSignal = -5;

        countBG--; 
      }

      processesCleared++;
      
      /* If a background process cannot be found to terminate, or if the maximum */
      /* number of processes to terimate has been reached, leave the loop */
    } while (childPid > 0 && processesCleared < maxProcessToClear);
  }
}

/********************************************************************************
* Description: CheckResult()
*   This function is a wrapper for perror(relevant info); exit(1). This gets used
*   several times throughout this program.
********************************************************************************/
void CheckResult(int result, char *errorMessage) {
  if (result < 0) {
    perror(errorMessage);
    exit(1);
  } 
}

/********************************************************************************
* Description: ExitSmallSh()
*   This function responds to the command "exit". It terminate all running
*   processes prior to the shell exiting.
********************************************************************************/
int ExitSmallSh(char currentDir[]) {
  int result;
  int i;
  for (i = 0; i < countBG; i++) {
    result = kill(processBG[i], 15); /* Send terminate signal to each background process */
    if (result < 0) {
      return result;
    }
  }
  return 0;
}

/********************************************************************************
* Description: ChangeDir()
*   This function responds to the command "cd". It attempts to change the
*   directory to what the user inputs as an argument.
********************************************************************************/
int ChangeDir(struct command *input, char currentDir[]) {
  int result;
  char tempDir[1024];

  /* Check for command "cd" with no arguments */
  if (input->argCount == 1) {
    memset(currentDir, '\0', 512 * sizeof(char));
    strcpy(currentDir, getenv("HOME")); /* Change directory to HOME path */
    return 0;
  } else { 
    /* Attempt to run chdir() with user input as absolute path */
    result = chdir(input->args[1]);
    /* If absolute doesn't work, try chdir() with relative path */
    if (result != 0) {
      memset(tempDir, '\0', sizeof(tempDir));
      strcpy(tempDir, currentDir);
      strcat(tempDir, "/");
      strcat(tempDir, input->args[1]);
      result = chdir(tempDir);
    }
   
    /* If chdir() works, actually change the directory string that the */
    /* shell is tracking */
    if (result == 0) {
      if (input->args[1][0] == '/') { /* absolute path */
        memset(currentDir, '\0', 512 * sizeof(char));
        strcpy(currentDir, input->args[1]);
      } else {
        strcat(currentDir, "/"); /* relative path */
        strcat(currentDir, input->args[1]);
      }
      return 0;
    }  
  }
  
  /* If neither absolute or relative work, print error */
  perror("chdir()"); 
  return 1;
}

/********************************************************************************
* Description: Status()
*   This function responds to the command "status". It prints the exit value or
*   terminating signal of the last command, but not both. As a side note, Status()
*   returns 0, so running "status" twice will list an exit value of 0.
********************************************************************************/
int Status(struct statusValues *commandStatus) {
  if (commandStatus->exitStatus >= 0) {
    printf("exit value %d\n", commandStatus->exitStatus);  
  } else if (commandStatus->termSignal >= 0) {
    printf("terminated by signal %d\n", commandStatus->termSignal);
  }
  return 0;
}

/********************************************************************************
* Description: RunBuiltin()
*   This function matches the command to a builtin function, and then runs that
*   builtin function.
********************************************************************************/
void RunBuiltin(struct command *input, struct statusValues *commandStatus, char currentDir[]) {
  if (!input->isComment) {
    if (strcmp(input->args[0], "exit") == 0) {
      commandStatus->exitStatus = ExitSmallSh(currentDir);
    } else if (strcmp(input->args[0], "cd") == 0) {
      commandStatus->exitStatus = ChangeDir(input, currentDir);  
    } else if (strcmp(input->args[0], "status") == 0) {
      commandStatus->exitStatus = Status(commandStatus);
    }
  }
}

/********************************************************************************
* Description: ExecuteCommand()
*   This function executes a command that is passed to it.
********************************************************************************/
void ExecuteCommand(struct command *input, struct statusValues *commandStatus, char currentDir[]) {
  pid_t spawnpid = -5;
  int childExitMethod = -5;
  int fdI = -10;
  int fdO = -10;
  int dupResult = -10;
  char nullDir[64] = "/dev/null";
  int result;

  /* The shell and background processes ignore the SIGINT signal by default */
  /* This restores default SIGINT behavior to foreground processes, so */
  /* that they can be terminated by Ctrl-C */
  struct sigaction restore_action = {{0}};
  restore_action.sa_handler = SIG_DFL;

  /* If forkCount is getting high, then there is probably a fork bomb, so abort */
  if (forkCount > 50) {
    abort();
  }

  /* Check for builtin */
  if (input->isBuiltin) {
    RunBuiltin(input, commandStatus, currentDir);
    commandStatus->termSignal = -5;
  } else {    
    /* Execute command */
    /* Use fork() */
    forkCount++;
    spawnpid = fork();
 
    switch (spawnpid) {
      case -1: {
        perror("We have just lost cabin pressure...\n");
        exit(1);
        break;
      }
      case 0: {
        /* Input redirection */
        if (input->isInputRedirect) {                                                 
          fdI = open(input->inputFile, O_RDONLY);
          fcntl(fdI, F_SETFD, FD_CLOEXEC);
          CheckResult(fdI, "open()");
          dupResult = dup2(fdI, 0);
          CheckResult(dupResult, "dup2()");
        /* Default input redirection to /dev/null for background (when none other specified) */
        } else if (!input->isForeground) {
          fdI = open(nullDir, O_RDONLY);
          fcntl(fdI, F_SETFD, FD_CLOEXEC);
          CheckResult(fdI, "open()");
          dupResult = dup2(fdI, 0);
          CheckResult(dupResult, "dup2()");
        }
       
        /* Output redirection */ 
        if (input->isOutputRedirect) {
          fdO = open(input->outputFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
          fcntl(fdO, F_SETFD, FD_CLOEXEC);
          CheckResult(fdO, "open()");
          dupResult = dup2(fdO, 1);
          CheckResult(dupResult, "dup2()");
        /* Default output redirection to /dev/null for background (when none other specified) */
        } else if (!input->isForeground) {
          fdO = open(nullDir, O_RDONLY);
          fcntl(fdO, F_SETFD, FD_CLOEXEC);
          CheckResult(fdO, "open()");
          dupResult = dup2(fdO, 1);
          CheckResult(dupResult, "dup2()");
        } 

        /* Change child directory */ 
        result = chdir(currentDir);
        CheckResult(result, "chdir()");

        /* Give foreground processes default SIGINT behavior */
        if (input->isForeground) {
          sigaction(SIGINT, &restore_action, NULL);  
        }
 
        /* Attempt to run process, print error if it fails */
        execvp(input->args[0], input->args); 
        perror(input->args[0]);
        exit(1);
        break;
      }
      default: { 
        if (input->isForeground) {
          /* For foreground process, flush stdout, and sit there until the child completes */
          fflush(stdout); 
          waitpid(spawnpid, &childExitMethod, 0);
          forkCount--;
          /* Check exit value or terminate signal */
          if (WIFEXITED(childExitMethod) != 0) {
            commandStatus->exitStatus = WEXITSTATUS(childExitMethod);
            commandStatus->termSignal = -5;
          } else if (WIFSIGNALED(childExitMethod) != 0) {
            commandStatus->termSignal = WTERMSIG(childExitMethod); 
            commandStatus->exitStatus = -5;
            if (strcmp(input->args[0], "kill") != 0) {
              printf("terminated by signal %d\n", commandStatus->termSignal); 
            }
          }
        } else {
          /* For background process, print PID and the shell proceeds */
          printf("background pid is %d\n", spawnpid);
          processBG[countBG] = spawnpid;
          countBG++;
          forkCount--;
        }
        break;
      }
    }
  }
}

int main() {
  struct command shellComm;
  int exitFlag = 0;
  struct statusValues commandStatus;
  commandStatus.exitStatus = -5;
  commandStatus.termSignal = -5;
  char currentDir[512];
  bool stopFlag = false;
  char readBuffer[2049];
  
  memset(currentDir, '\0', sizeof(currentDir));
  
  /* Start currentDir to the current working directory */ 
  getcwd(currentDir, sizeof(currentDir));

  /* Set up signal handlers for SIGUSR1 AND SIGTSTP */
  /* Set up signal ignore handler for SIGINT */
  struct sigaction SIGUSR1_action = {{0}}, SIGTSTP_action = {{0}},
                   ignore_action = {{0}};

  SIGUSR1_action.sa_handler = catchSIGUSR1;
  sigfillset(&SIGUSR1_action.sa_mask);
  SIGUSR1_action.sa_flags = 0;

  SIGTSTP_action.sa_handler = catchSIGTSTP;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = 0;

  ignore_action.sa_handler = SIG_IGN;

  sigaction(SIGUSR1, &SIGUSR1_action, NULL);
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);
  sigaction(SIGINT, &ignore_action, NULL);  

  /* Shell starts */
  do {
    /* Get command */
    memset(readBuffer, '\0', sizeof(readBuffer));
    GetInput(readBuffer);
    
    /* Check if input is a single newline character */
    if (strcmp(readBuffer, "\n") != 0) {
      CreateCommand(readBuffer, &shellComm);
      
      /* Check for SIGTSTP */
      if (firstStop == true && stopFlag == false ) {
        stopFlag = true;
      } else if (secondStop == true && stopFlag == true) {
        firstStop = false;
        secondStop = false;
        stopFlag = false;
      }

      /* Check for foreground only mode */
      if (stopFlag) {
        if (!shellComm.isForeground) {
          shellComm.isForeground = true;
        }
      } 
   
      /* Execute command */
      if (!shellComm.isComment) {
        ExecuteCommand(&shellComm, &commandStatus, currentDir); 
        if (strcmp(shellComm.args[0], "exit") == 0) {
          exitFlag = 1;
        }
      }

      /* Destroy command */
      DestroyCommand(&shellComm); /* Free memory in arguments */     
    }
    /* Call background process handler */
    /* This is a signal handler rather than a function call because */
    /* I want it to block other signals while it runs */
    raise(SIGUSR1);
  } while (!exitFlag);

  return 0;
}


