#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "Message.h"
#include "Context.h"
#include "CommandLine.h"



/* Global functions */

CommandLine_t*    (CommandLine_Create)(int argc,char** argv)
{
  CommandLine_t* commandline = (CommandLine_t*) malloc(sizeof(CommandLine_t)) ;
  
  assert(commandline) ;
  
  CommandLine_GetNbOfArg(commandline) = argc ;
  CommandLine_GetArg(commandline) = argv ;
    
  return(commandline) ;
}



void (CommandLine_Delete)(void* self)
{
  CommandLine_t** commandline = (CommandLine_t**) self ;
  
  free(*commandline) ;
  *commandline = NULL ;
}
