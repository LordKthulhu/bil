#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Message.h"
#include "DataFile.h"
#include "Mry.h"
#include "SString.h"
#include "Dates.h"





Dates_t*  Dates_New(const int n_dates)
{
  Dates_t* dates = (Dates_t*) Mry_New(Dates_t) ;
  
  {
    Dates_GetNbOfDates(dates) = n_dates ;

    {
      Date_t* date = (Date_t*) Mry_New(Date_t[n_dates]) ;
    
      Dates_GetDate(dates) = date ;
    }
  }
  
  return(dates) ;
}





Dates_t*  Dates_Create(DataFile_t* datafile)
{
  char* filecontent = DataFile_GetFileContent(datafile) ;
  char* c  = String_FindToken(filecontent,"TEMP,DATE,Dates",",") ;
  int n_dates = (c = String_SkipLine(c)) ? atoi(c) : 0 ;
  Dates_t* dates = Dates_New(n_dates) ;
  
  
  Message_Direct("Enter in %s","Dates") ;
  Message_Direct("\n") ;


  {
    double* t = (double*) Mry_New(double[n_dates]) ;
    
    c = String_SkipLine(c) ;
    
    String_ReadArray(c,n_dates," %lf",t) ;
      
    {
      Date_t* date = Dates_GetDate(dates) ;
      int   i ;
    
      for(i = 0 ; i < n_dates ; i++) {
        Date_GetTime(date + i) = t[i] ;
      }
    }
    
    free(t) ;
  }
  
  
  return(dates) ;
}
