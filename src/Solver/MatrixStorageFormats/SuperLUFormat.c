#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "BilLib.h"
#include "Options.h"
#include "Mesh.h"
#include "Message.h"

#include "NCFormat.h"

#ifdef SUPERLULIB
#include "SuperLUFormat.h"
#endif




/* Extern functions */


#ifdef SUPERLULIB

SuperLUFormat_t* SuperLUFormat_Create(Mesh_t* mesh)
/* Create a matrix in SuperLUFormat format */
{
  SuperLUFormat_t* aslu = (SuperLUFormat_t*) malloc(sizeof(SuperLUFormat_t)) ;

  assert(aslu) ;
  
  SuperLUFormat_GetStorageType(aslu) = SLU_NC ;
  SuperLUFormat_GetDataType(aslu)    = SLU_D ;
  SuperLUFormat_GetMatrixType(aslu)  = SLU_GE ;
  
  {
    int n_col = Mesh_GetNbOfMatrixColumns(mesh) ;
    
    SuperLUFormat_GetNbOfColumns(aslu) = n_col ;
    SuperLUFormat_GetNbOfRows(aslu)    = n_col ;
  }
  
  {
    NCFormat_t* asluNC = NCFormat_Create(mesh) ;
    
    SuperLUFormat_GetStorage(aslu)     = (void*) asluNC ;
  }
  
  return(aslu) ;
}



void SuperLUFormat_Delete(void* self)
{
  SuperLUFormat_t** aslu = (SuperLUFormat_t**) self ;
  NCFormat_t* asluNC = (NCFormat_t*) SuperLUFormat_GetStorage(*aslu) ;
  
  NCFormat_Delete(&asluNC) ;
  
  free(*aslu) ;
  *aslu = NULL ;
}

#endif