#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include "Message.h"
#include "Mry.h"
#include "Graph.h"
#include "AdjacencyList.h"



//void*  Graph_Initialize(void* self,va_list) ;


Graph_t*  Graph_Create(int nvert,int* vert_nedges)
{
  Graph_t* graph = (Graph_t*) Mry_New(Graph_t) ;

  
  {
    Graph_GetNbOfVertices(graph) = nvert ;
    Graph_GetNbOfEdges(graph) = 0 ;
  
  
    {
      AdjacencyList_t* adj = AdjacencyList_Create(nvert,vert_nedges) ;
    
      Graph_GetAdjacencyList(graph) = adj ;
    }
  }

  return(graph) ;
}



#if 0
void*  Graph_Initialize(void* self,va_list ap)
{
  Graph_t* graph = (Graph_t*) self ;
  int nvert = va_arg(ap,int) ;
  int* vert_nedges = va_arg(ap,int*) ;
  

  Graph_GetNbOfVertices(graph) = nvert ;
  Graph_GetNbOfEdges(graph) = 0 ;
  
  
  {
    AdjacencyList_t* adj = AdjacencyList_Create(nvert,vert_nedges) ;
    
    Graph_GetAdjacencyList(graph) = adj ;
  }

  return(graph) ;
}
#endif



void Graph_Delete(void* self)
{
  Graph_t** pgraph = (Graph_t**) self ;
  Graph_t*   graph = *pgraph ;
  
  AdjacencyList_Delete(&Graph_GetAdjacencyList(graph)) ;
  free(graph) ;
  //*pgraph = NULL ;
}

