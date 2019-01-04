/********************************************************************************
* Program Name: CommandLine.c
* Author: Mathew Kagel
* Date: 2018-03-04 
* Description: This is the set of functions that allow smallsh to get user input,
*   recognize commands, and build commands from that input to be executed.
********************************************************************************/
#include "CommandLine.h"

/********************************************************************************
* Description: GetInput()
*   This function gets input from the user. For clarification, if the user hits
*   enter and inputs only a newline character, that is handled by smallsh.c in
*   main. smallsh will not try to build a command out of a newline character.
*   Newlines can't be filtered by this function since smallsh needs to use newline
*   characters to check background processes, but without running the newline as
*   a command.
********************************************************************************/
void GetInput(char inputBuffer[]) {
  size_t bufsize = 2049;
  ssize_t n;
  char *tempBuf = NULL;

  fflush(stdout); /* Flush standard out before and after the colon */
                  /* to make sure it doesn't get in the way */
  while(1) { /* Keep running getline until a successful read is completed */
    printf(": ");
    fflush(stdout);
    n = getline(&tempBuf, &bufsize, stdin);
    if (n == -1) { /* When getline is interrupted by signal handlers, this allows */
      clearerr(stdin); /* getline to return -1 and continue looking for input upon */
    } else {       /* re-entering getline */
      break;
    }
  }
  strcpy(inputBuffer, tempBuf);
  free(tempBuf);
  tempBuf = NULL;
}

/********************************************************************************
* Description: ExpandPid()
*   This function takes a token from a command line input and looks for the $$
*   within that token so that it can expand it into the process id of the shell.
********************************************************************************/
void ExpandPid (char *token, char output[]) {
  char *subToken;
  char *remaining = token;
  char pidString[128];
  char *ref;
  
  if (strcmp(token, "$$") == 0) { /* Check if the token is exactly $$ */
    memset(pidString, '\0', sizeof(pidString));    
    sprintf(pidString, "%d", getpid());
    strcpy(output, pidString);
  } else {
    ref = strstr(token, "$$"); /* Check if the token has $$ somewhere */
    if (ref != NULL) {
      subToken = strtok_r(token, "$$", &remaining); /* If there is $$ */
      memset(pidString, '\0', sizeof(pidString));   /* then make a subtoken */
      sprintf(pidString, "%d", getpid());           /* using it */

      if (token[1] == '$') { /* Make a new string with the subtoken */
        strcpy(output, pidString); /* For example, paul$$apes, would make */
        strcat(output, subToken);  /* a paul subtoken and a new string of */
      } else {                     /* paul7777apes, assuming the PID was 7777 */
        strcpy(output, subToken);
        strcat(output, pidString);
      }
      subToken = strtok_r(NULL, "$$", &remaining);

      if (subToken != NULL) {
        strcat(output, subToken);
      }
    } else {
      strcpy(output, token);
    }
  }
}

/********************************************************************************
* Description: TokenizeInput()
*   This function takes a line from the command line entered by the user and
*   tokenizes it by spaces or the " " character. Basically, this breaks the line
*   into individual pieces for further analysis.
********************************************************************************/
int TokenizeInput(char inputBuffer[], char *tokens[]) {
  int tokenCount = 0;
  char *remaining = inputBuffer;

  /* Use strtok_r for re-entrancy and for multiple strtok calls */
  /* Keep tokeinizing until NULL is reached, and increment tokenCount */
  tokens[tokenCount] = strtok_r(inputBuffer, " ", &remaining);
  while (tokens[tokenCount] != NULL) {
    tokens[++tokenCount] = strtok_r(NULL, " ", &remaining); 
  }
  remaining = tokens[tokenCount - 1];
  tokens[tokenCount - 1] = strtok_r(tokens[tokenCount - 1], "\n", &remaining);

  return tokenCount;
}

/********************************************************************************
* Description: AssignCommandName()
*   This function takes the first token and assigns it to argument one, which
*   is by default the name of the command. This also returns a bool to set the
*   isBuiltin flag within the command struct that is being populated.
********************************************************************************/
bool AssignCommandName(struct command *input, char *firstToken) {
  char firstTokenPid[512];
  memset(firstTokenPid, '\0', sizeof(firstTokenPid));

  input->args[0] = (char *) calloc(128, sizeof(char)); 
  ExpandPid(firstToken, firstTokenPid);
  strcpy(input->args[0], firstTokenPid);  
  input->argCount++; 
  if (strcmp(input->args[0], "exit") == 0) {
    return true;
  } else if (strcmp(input->args[0], "cd") == 0) {
    return true;
  } else if (strcmp(input->args[0], "status") == 0) {
    return true;
  } else {
    return false;
  }
}


/********************************************************************************
* Description: IsComment()
*   This function returns a bool value for whether or not the command is a
*   comment. It checks the first character against the '#' character.
********************************************************************************/
bool IsComment(char inputBuffer[]) {
  if (inputBuffer[0] == '#') { /* Check if first character in */
    return true;               /* the input is a # */
  } else {
    return false;
  }
}

/********************************************************************************
* Description: IsForeground()
*   This function returns a bool value for whether or not the command is to be
*   run in the foreground. It checks for the '&' character, at the very end of
*   the command line input specifically.
********************************************************************************/
bool IsForeground(int tokenCount, char *tokens[]) {
  const char backgroundChar[2] = "&"; 
  if (tokenCount < 2) { /* First token interpretted as command name, not */
    return true;         /* possible to have a background & present */ 
  } else if (strcmp(tokens[tokenCount - 1], backgroundChar) == 0) {
    return false;
  } else {
    return true;
  }
}

/********************************************************************************
* Description: IsInputRedirect()
*   This function returns a bool value for whether or not the command has an input
*   redirect. If the command has an input redirect, the inputFile of the command is
*   populated in addition to returning the bool to set isInputRedirect in the
*   command.
********************************************************************************/
bool IsInputRedirect(struct command *input, char *tokens[]) {
  int backgroundMod = 0;
  const char inputRedirectChar[2] = "<"; 
  char tokenWithPid[512];  

  if (!input->isForeground) {
    backgroundMod = 1; /* Add one to indices being checked if is a background command */
  } 
  
  /* Check if there are at least 3 tokens or 4 for background command */   
  if (input->tokenCount < 3 + backgroundMod) { /* First token interpretted as command name, not */     
    return false;                              /* possible to have a < present */    
  /* Check the second to last token or third to last token if background command */              
  } else if (strcmp(tokens[input->tokenCount - 2 + backgroundMod], inputRedirectChar) == 0) {
    memset(input->inputFile, '\0', sizeof(input->inputFile));
    memset(tokenWithPid, '\0', sizeof(tokenWithPid));
    ExpandPid(tokens[input->tokenCount - 1 + backgroundMod], tokenWithPid);
    strcpy(input->inputFile, tokenWithPid);
    return true;
  /* Check if there are at least 5 tokens or 6 for background command */
  } else if (input->tokenCount < 5 + backgroundMod) {
    return false;                                                    
  /* Check the fourth to last token or fifth to last for background command */        
  } else if (strcmp(tokens[input->tokenCount - 4 + backgroundMod], inputRedirectChar) == 0) {
    memset(input->inputFile, '\0', sizeof(input->inputFile));
    memset(tokenWithPid, '\0', sizeof(tokenWithPid));
    ExpandPid(tokens[input->tokenCount - 3 + backgroundMod], tokenWithPid);
    strcpy(input->inputFile, tokenWithPid);   
    return true;
  } else {
  /* If all else fails, no input redirect */ 
    return false;
  }
}

/********************************************************************************
* Description: IsOutputRedirect()
*   This function returns a bool value for whether or not the command has an output
*   redirect. If the command has an output redirect, the outputFile of the command is
*   populated in addition to returning the bool to set isOutputRedirect in the
*   command.
********************************************************************************/
bool IsOutputRedirect(struct command *input, char *tokens[]) {
  int backgroundMod = 0;
  const char outputRedirectChar[2] = ">"; 
  char tokenWithPid[512];

  if (!input->isForeground) {
    backgroundMod = 1; /* Add one to indices being checked if is a background command */
  } 

  /* Check if there are at least 3 tokens or 4 for background command */       
  if (input->tokenCount < 3 + backgroundMod) { /* First token interpretted as command name, not */     
    return false;                              /* possible to have a > present */                  
  /* Check the second to last token or third to last token if background command */              
  } else if (strcmp(tokens[input->tokenCount - 2 + backgroundMod], outputRedirectChar) == 0) {
    memset(input->outputFile, '\0', sizeof(input->outputFile));
    memset(tokenWithPid, '\0', sizeof(tokenWithPid));
    ExpandPid(tokens[input->tokenCount - 1 + backgroundMod], tokenWithPid);
    strcpy(input->outputFile, tokenWithPid);   
    return true;
  /* Check if there are at least 5 tokens or 6 for background command */
  } else if (input->tokenCount < 5 + backgroundMod) {
    return false;                                                            
  /* Check the fourth to last token or fifth to last for background command */        
  } else if (strcmp(tokens[input->tokenCount - 4 + backgroundMod], outputRedirectChar) == 0) {
    memset(input->outputFile, '\0', sizeof(input->outputFile));
    memset(tokenWithPid, '\0', sizeof(tokenWithPid));
    ExpandPid(tokens[input->tokenCount - 3 + backgroundMod], tokenWithPid);
    strcpy(input->outputFile, tokenWithPid);   
    return true;
  /* If all else fails, no output redirect */ 
  } else {
    return false;
  }
}

/********************************************************************************
* Description: AssignArguments()
*   This function assigns all tokens that are not a command name, and are not
*   associated with flagging foreground/redirects to the command's arguments.
*   This requires the function to check the foreground and file redirect flags
*   so that it knows which tokens to grab and which tokens to leave alone.
********************************************************************************/
void AssignArguments(struct command *input, int tokenCount, char *tokens[]) {
  int loopStop = tokenCount;
  char tokenWithPid[512];

  if (!input->isForeground) {
    loopStop -= 1;
  }
  if (input->isInputRedirect){
    loopStop -= 2;   
  } 
  if (input->isOutputRedirect) {
    loopStop -= 2;
  }

  int i;
  for (i = 1; i < loopStop; i++) {
    input->args[i] = (char *) calloc(128, sizeof(char)); 
    memset(tokenWithPid, '\0', sizeof(tokenWithPid));
    ExpandPid(tokens[i], tokenWithPid);
    strcpy(input->args[i], tokenWithPid);   
    input->argCount++;
  }
  input->args[input->argCount] = NULL;
} 

/********************************************************************************
* Description: CreateCommand()
*   This function takes the majority of functions in this file and uses them to
*   build a struct command.
********************************************************************************/
void CreateCommand(char inputBuffer[], struct command *input) {
  char *inputTokens[600]; /* Allow for at least 512 arguments */

  input->argCount = 0; 

  /* Check if command line input is a comment */
  input->isComment = IsComment(inputBuffer);

  /* If command line input is not a comment, parse into tokens
     and assign to appropriate struct data members */ 
  if (!input->isComment) {
    /* Tokenize command line input using space as the delimiter */
    input->tokenCount = TokenizeInput(inputBuffer, inputTokens);
  
    /* Assign first token to command name */
    input->isBuiltin = AssignCommandName(input, inputTokens[0]);

    /* Check for background or foreground */
    input->isForeground = IsForeground(input->tokenCount, inputTokens);

    /* Check for redirects */
    input->isInputRedirect = IsInputRedirect(input, inputTokens);  
    input->isOutputRedirect = IsOutputRedirect(input, inputTokens);    

    /* The remainder are assigned to arguments */
    AssignArguments(input, input->tokenCount, inputTokens); 
  }
}

/********************************************************************************
* Description: DestroyCommand()
*   This function takes a struct command created by CreateCommand() and
*   deallocates all memory that has been dynamically allocated in the args[]
*   array. 
********************************************************************************/
void DestroyCommand(struct command *input) {
  /* Free allocated memory in command struct */
  int i;
  for (i = 0; i < input->argCount; i++) {
    free(input->args[i]);
    input->args[i] = NULL;
  }
  input->argCount = 0;
}



