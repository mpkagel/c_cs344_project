/********************************************************************************
* Program Name: CommandLine.h
* Author: Mathew Kagel
* Date: 2018-03-04
* Description: Header file for CommandLine.c. Set of functions to deal with command
*   line input and building a command to be executed by smallsh.c.
********************************************************************************/
#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#define _POSIX_C_SOURCE 200809L /* https://stackoverflow.com/questions/23961147/ */
                                /* implicit-declaration-of-function-strtok-r-wimplicit-
                                   function-declaration-in */
                                /* and man7.org/linux/man-pages/man3/getline.3.html */
                                /* for POSIX version supporting getline and strtok_r
                                   support in C */                              

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct command {
  char *args[512]; /* The first argument is the command name */
  char inputFile[1024];
  char outputFile[1024];
  int argCount;
  int tokenCount; /* This is the number of string tokens from command line input */
  bool isComment;
  bool isForeground;
  bool isInputRedirect;
  bool isOutputRedirect;
  bool isBuiltin;
};

void GetInput(char inputBuffer[]);
void ExpandPid(char *input, char output[]); 
int TokenizeInput(char inputBuffer[], char *tokens[]); 
bool AssignCommandName(struct command *input, char *firstToken); 
bool IsComment(char inputBuffer[]); 
bool IsForeground(int tokenCount, char *tokens[]); 
bool IsInputRedirect(struct command *input, char *tokens[]); 
bool IsOutputRedirect(struct command *input, char *tokens[]); 
void AssignArguments(struct command *input, int tokenCount, char *tokens[]); 
void CreateCommand(char inputBuffer[], struct command *input); 
void DestroyCommand(struct command *input); 
#endif



