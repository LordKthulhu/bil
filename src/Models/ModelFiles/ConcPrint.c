#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "CommonModel.h"
#include "FEM.h"
#include "DataSet.h"
#include "Modules.h"
#include "Elasticity.h"

#include <omp.h>

#define TITLE   "Concrete Printing"
#define AUTHORS " "

#include "PredefinedMethods.h"


/* Nb of equations */
#define NEQ     (dim)
/* Nb of (im/ex)plicit terms and constant terms */
#define NVI     (12)
#define NVE     (0)
#define NV0     (9)

/* Equation index */
#define E_mec   (0)

/* Unknown index */
#define U_u     (0)

/* We define some names for implicit terms */
#define SIG     (vim + 0)
#define F_MASS  (vim + 9)
#define SIG_n   (vim_n + 0)

/* We define some names for constant terms */
#define SIG0    (v0  + 0)


/* Functions */
static Model_ComputePropertyIndex_t  pm ;
static void    GetProperties(Element_t*,double) ;
static double* MicrostructureElasticTensor(DataSet_t*,double*) ;

static double* ComputeVariables(Element_t*,double**,double**,double*,double,double,int) ;
static void    ComputeSecondaryVariables(Element_t*,double,double,double*) ;
//static Model_ComputeSecondaryVariables_t    ComputeSecondaryVariables ;

static void    ComputeMicrostructure(DataSet_t*,double*,double*) ;
static void    CheckMicrostructureDataSet(DataSet_t*) ;


static double* MacroGradient(Element_t*,double) ;
static double* MacroStrain(Element_t*,double) ;


/* Material parameters */
static double  gravity ;
static double  rho_s ;
static double* sig0 ;
static double t_init ;
static double  macrogradient[9] ;
static double  macrostrain[9] ;
static double* cijkl ;
static Elasticity_t* elasty ;


#define ItIsPeriodic  (Geometry_IsPeriodic(Element_GetGeometry(el)))


// #define NbOfVariables  (47)
//
// #define I_U            (0)
// #define I_EPS          (3)
// #define I_SIG          (12)
// #define I_Fmass        (21)
// #define I_EPS_n        (24)
// #define I_SIG_n        (33)
// #define I_Fmass_n      (43)


/* We define some indices for the local variables */
enum {
I_U,
I_U2     = I_U + 2,

I_EPS,
I_EPS8   = I_EPS + 8,

I_SIG,
I_SIG8   = I_SIG + 8,

I_Fmass,
I_Fmass2 = I_Fmass + 2,

I_EPS_n,
I_EPS_n8 = I_EPS_n + 8,

I_SIG_n,
I_SIG_n8 = I_SIG_n +8,

I_Last,
} ;


#define NbOfVariables    (I_Last)

double HydrDeg(double t)
{
  // return( 0.8*(1-exp(-(t+10)/23.5/3600)) ) ; Old test
  // return( 0.3875 + 0.825/3.142 * atan(0.3*(t/3600-11.0)) ) ;
  return( 0.3525 + 0.895/3.142 * atan(0.3*(t/3600-9.0)) ) ;
}


int pm(const char* s)
{
       if(!strcmp(s,"gravity")) return(0) ;
  else if(!strcmp(s,"rho_s"))   return(1) ;
  else if(!strcmp(s,"poisson")) return(2) ;
  else if(!strcmp(s,"young"))   return(3) ;
  else if(!strcmp(s,"sig0"))    return(4) ;
  else if(!strncmp(s,"sig0_",5)) {
    int i = (strlen(s) > 5) ? s[5] - '1' : 0 ;
    int j = (strlen(s) > 6) ? s[6] - '1' : 0 ;

    return(4 + 3*i + j) ;
  } else if(!strcmp(s,"macro-gradient")) return(13) ;
  else if(!strncmp(s,"macro-gradient_",15)) {
    int i = (strlen(s) > 15) ? s[15] - '1' : 0 ;
    int j = (strlen(s) > 16) ? s[16] - '1' : 0 ;

    return(13 + 3*i + j) ;
  } else if(!strcmp(s,"Cijkl")) {
    return(22) ;
  } else if(!strcmp(s,"t_init")) {
    return(23) ;
  } else if(!strcmp(s,"macro-fctindex")) {
    return(103) ;
  } else if(!strncmp(s,"macro-fctindex_",15)) {
    int i = (strlen(s) > 15) ? s[15] - '1' : 0 ;
    int j = (strlen(s) > 16) ? s[16] - '1' : 0 ;

    return(103 + 3*i + j) ;
  } else return(-1) ;
}



double* MacroGradient(Element_t* el,double t)
{
  double* gradient = macrogradient ;
  double  f[9] = {0,0,0,0,0,0,0,0,0} ;

  {
    Functions_t* fcts = Material_GetFunctions(Element_GetMaterial(el)) ;
    Function_t*  fct = Functions_GetFunction(fcts) ;
    int nf = Functions_GetNbOfFunctions(fcts) ;
    double* fctindex = &Element_GetPropertyValue(el,"macro-fctindex") ;
    int i ;

    for(i = 0 ; i < 9 ; i++) {
      int idx = floor(fctindex[i] + 0.5) ;

      if(0 < idx && idx < nf + 1) {
        Function_t* macrogradfct = fct + idx - 1 ;

        f[i] = Function_ComputeValue(macrogradfct,t) ;
      }
    }
  }

  {
    double* g = &Element_GetPropertyValue(el,"macro-gradient") ;
    int i ;

    for(i = 0 ; i < 9 ; i++) {
      gradient[i] = g[i] * f[i] ;
    }
  }

  return(gradient) ;
}



double* MacroStrain(Element_t* el,double t)
{
  double* strain = macrostrain ;
  double* grd = MacroGradient(el,t) ;
  int i ;

  for(i = 0 ; i < 3 ; i++) {
    int j ;

    for(j = 0 ; j < 3 ; j++) {
      strain[3*i + j] = 0.5*(grd[3*i + j] + grd[3*j + i]) ;
    }
  }

  return(strain) ;
}


void GetProperties(Element_t* el,double t)
{


  t_init = Element_GetPropertyValue(el,"t_init") ;

  double alpha = HydrDeg(t-t_init) ;

  gravity = Element_GetPropertyValue(el,"gravity") ;
  rho_s   = (t >= t_init) ? Element_GetPropertyValue(el,"rho_s") : 0 ;
  sig0    = &Element_GetPropertyValue(el,"sig0") ;
  double young = Element_GetPropertyValue(el,"young") ;
  double poisson = Element_GetPropertyValue(el,"poisson") ;
  double young_t = (t >= t_init) ? alpha*young : 1e-9 ;

  elasty  = Element_FindMaterialData(el,Elasticity_t,"Elasticity") ;
  Elasticity_SetParameters(elasty,young_t,poisson) ;
  cijkl   = Elasticity_GetStiffnessTensor(elasty) ;
  Elasticity_ComputeStiffnessTensor(elasty,cijkl) ;
}



int SetModelProp(Model_t* model)
/** Set the model properties */
{
  int dim = Model_GetDimension(model) ;
  int i ;

  /** Number of equations to be solved */
  Model_GetNbOfEquations(model) = NEQ ;

  /** Names of these equations */
  for(i = 0 ; i < dim ; i++) {
    char name_eqn[7] ;
    sprintf(name_eqn,"meca_%d",i + 1) ;
    Model_CopyNameOfEquation(model,E_mec + i,name_eqn) ;
  }

  /** Names of the main unknowns */
  for(i = 0 ; i < dim ; i++) {
    char name_unk[4] ;
    sprintf(name_unk,"u_%d",i + 1) ;
    Model_CopyNameOfUnknown(model,U_u + i,name_unk) ;
  }

  Model_GetComputePropertyIndex(model) = pm ;

  Model_GetNbOfVariables(model) = NbOfVariables ;
  //Model_GetComputeSecondaryVariables(model) = ComputeSecondaryVariables ;

  return(0) ;
}



int ReadMatProp(Material_t* mat,DataFile_t* datafile)
/** Read the material properties in the stream file ficd */
{
  int NbOfProp = 112 ;
  int i ;

  /* Par defaut tout a 0 */
  for(i = 0 ; i < NbOfProp ; i++) Material_GetProperty(mat)[i] = 0. ;

  Material_ScanProperties(mat,datafile,pm) ;


  /* Elasticity */
  {
    elasty = Elasticity_Create() ;

    Material_AppendData(mat,1,elasty,Elasticity_t,"Elasticity") ;
  }

  /* The 4th rank elastic tensor */
  {
    char* method = Material_GetMethod(mat) ;

    elasty = Material_FindData(mat,Elasticity_t,"Elasticity") ;

    /* obtained from a microstructure */
    if(!strncmp(method,"Microstructure",14)) {
      char* p = strstr(method," ") ;
      char* cellname = p + strspn(p," ") ;
      Options_t* options = Options_Create(NULL) ;
      DataSet_t* jdd = DataSet_Create(cellname,options) ;
      double* c = Elasticity_GetStiffnessTensor(elasty) ;

      CheckMicrostructureDataSet(jdd) ;

      MicrostructureElasticTensor(jdd,c) ;

    /* isotropic Hooke's law */
    } else {
      double young = Material_GetPropertyValue(mat,"young") ;
      double poisson = Material_GetPropertyValue(mat,"poisson") ;

      Elasticity_SetToIsotropy(elasty) ;
      Elasticity_SetParameters(elasty,young,poisson) ;

      {
        double* c = Elasticity_GetStiffnessTensor(elasty) ;

        Elasticity_ComputeStiffnessTensor(elasty,c) ;
      }
    }

#if 0
    {
      Elasticity_PrintStiffnessTensor(elasty) ;
    }
#endif
  }

  return(NbOfProp) ;
}



int PrintModelChar(Model_t* model,FILE* ficd)
/** Print the model characteristics */
{
  printf(TITLE) ;

  if(!ficd) return(0) ;

  printf("\n") ;
  printf("The set of equations is:\n") ;
  printf("\t- Mechanical Equilibrium     (meca_[1,2,3])\n") ;

  printf("\n") ;
  printf("The primary unknowns are:\n") ;
  printf("\t- Displacements              (u_[1,2,3]) \n") ;

  printf("\n") ;
  printf("Example of input data\n") ;

  printf("gravity = 0          # gravity\n") ;
  printf("rho_s = 0            # mass density of the dry material\n") ;
  printf("young = 2713e6       # Young's modulus\n") ;
  printf("poisson = 0.339      # Poisson's ratio\n") ;
  printf("sig0_11 = 0          # initial stress 11 (any ij are allowed)\n") ;
  printf("macro-strain_23 = 1  # For periodic BC (any ij)\n") ;

  return(0) ;
}



int DefineElementProp(Element_t* el,IntFcts_t* intfcts,ShapeFcts_t* shapefcts)
/** Define some properties attached to each element */
{
  {
    IntFct_t* intfct = Element_GetIntFct(el) ;
    int NbOfIntPoints = IntFct_GetNbOfPoints(intfct) ;

    /** Define the length of tables */
    Element_GetNbOfImplicitTerms(el) = NVI*NbOfIntPoints ;
    Element_GetNbOfExplicitTerms(el) = NVE*NbOfIntPoints ;
    Element_GetNbOfConstantTerms(el) = NV0*NbOfIntPoints ;
  }

  return(0) ;
}



int  ComputeLoads(Element_t* el,double t,double dt,Load_t* cg,double* r)
/** Compute the residu (r) due to loads */
{
  int dim = Geometry_GetDimension(Element_GetGeometry(el)) ;
  IntFct_t* intfct = Element_GetIntFct(el) ;
  int nn = Element_GetNbOfNodes(el) ;

  FEM_t* fem ;

  #pragma omp critical
  fem = FEM_GetInstance(el) ;

  {
    double* r1 = FEM_ComputeSurfaceLoadResidu(fem,intfct,cg,t,dt) ;

    {
      int    i ;

      for(i = 0 ; i < NEQ*nn ; i++) r[i] = r1[i] ;
    }
  }

  return(0) ;
}



int ComputeInitialState(Element_t* el,double t)
/** Compute the initial state i.e.
 *  the constant terms,
 *  the explicit terms,
 *  the implicit terms.
 */
{
  double* vim0 = Element_GetImplicitTerm(el) ;
  double* v00  = Element_GetConstantTerm(el) ;
  double** u   = Element_ComputePointerToNodalUnknowns(el) ;
  IntFct_t*  intfct = Element_GetIntFct(el) ;
  int NbOfIntPoints = IntFct_GetNbOfPoints(intfct) ;
  DataFile_t* datafile = Element_GetDataFile(el) ;
  int    p ;

  /* We skip if the element is a submanifold */
  if(Element_IsSubmanifold(el)) return(0) ;

  /*
    Input Data
  */
  GetProperties(el,t) ;


  /* Pre-initialization */
  //#pragma omp parallel for
  for(p = 0 ; p < NbOfIntPoints ; p++) {
    /* storage in v0 */
    {
      double* v0   = v00 + p*NV0 ;
      double* vim  = vim0 + p*NVI ;
      int    i ;

      /* Initial stresses */
      if(DataFile_ContextIsPartialInitialization(datafile)) {
        for(i = 0 ; i < 9 ; i++) SIG0[i] = SIG[i] ;
      } else {
        for(i = 0 ; i < 9 ; i++) SIG0[i] = sig0[i] ;
      }
    }
  }


  /* If there are initial displacements */
  #pragma omp parallel for
  for(p = 0 ; p < NbOfIntPoints ; p++) {
    /* Variables */
    double* x = ComputeVariables(el,u,u,vim0,t,0,p) ;

    /* storage in vim */
    {
      double* vim  = vim0 + p*NVI ;
      int    i ;

      for(i = 0 ; i < 9 ; i++) SIG[i]    = x[I_SIG + i] ;

      for(i = 0 ; i < 3 ; i++) F_MASS[i] = x[I_Fmass + i] ;
    }
  }

  return(0) ;
}




int  ComputeExplicitTerms(Element_t* el,double t)
/** Compute the explicit terms */
{
  return(0) ;
}




int  ComputeImplicitTerms(Element_t* el,double t,double dt)
/** Compute the implicit terms */
{
  double* vim0 = Element_GetCurrentImplicitTerm(el) ;
  double* vim_n  = Element_GetPreviousImplicitTerm(el) ;
  double* vex0 = Element_GetCurrentExplicitTerm(el) ;
  double** u  = Element_ComputePointerToCurrentNodalUnknowns(el) ;
  double** u_n = Element_ComputePointerToPreviousNodalUnknowns(el) ;
  IntFct_t*  intfct = Element_GetIntFct(el) ;
  int NbOfIntPoints = IntFct_GetNbOfPoints(intfct) ;
  int    p ;

  /* We skip if the element is a submanifold */
  if(Element_IsSubmanifold(el)) return(0) ;

  /*
    Input data
  */
  GetProperties(el,t) ;

  /* Loop on integration points */
  //#pragma omp parallel for
  for(p = 0 ; p < NbOfIntPoints ; p++) {
    /* Variables */
    double* x = ComputeVariables(el,u,u_n,vim_n,t,dt,p) ;

    /* storage in vim */
    {
      double* vim  = vim0 + p*NVI ;
      int    i ;

      for(i = 0 ; i < 9 ; i++) SIG[i]    = x[I_SIG + i] ;

      for(i = 0 ; i < 3 ; i++) F_MASS[i] = x[I_Fmass + i] ;
    }
  }

  return(0) ;
}




int  ComputeMatrix(Element_t* el,double t,double dt,double* k)
/** Compute the matrix (k) */
{
#define K(i,j)    (k[(i)*ndof + (j)])
  IntFct_t*  intfct = Element_GetIntFct(el) ;
  int nn = Element_GetNbOfNodes(el) ;
  int dim = Element_GetDimensionOfSpace(el) ;
  int ndof = nn*NEQ ;
  int    i ;
  double zero = 0. ;

  /* Initialisation */
  for(i = 0 ; i < ndof*ndof ; i++) k[i] = zero ;

  /* We skip if the element is a submanifold */
  if(Element_IsSubmanifold(el)) return(0) ;

  /*
    Input Data
  */
  GetProperties(el,t) ;

  /*
  ** Elastic Matrix
  */
  {
    FEM_t* fem = FEM_GetInstance(el) ;
    //double c[IntFct_MaxNbOfIntPoints*100] ;
    //int dec = Cijkl(fem,c) ;

    //double* kp = FEM_ComputeElasticMatrix(fem,intfct,c,dec) ;
    double* kp = FEM_ComputeElasticMatrix(fem,intfct,cijkl,0) ;

    #pragma omp parallel for
    for(i = 0 ; i < ndof*ndof ; i++) {
      k[i] = kp[i] ;
    }
  }

  return(0) ;
#undef K
}




int  ComputeResidu(Element_t* el,double t,double dt,double* r)
/** Comput the residu (r) */
{
#define R(n,i)    (r[(n)*NEQ+(i)])
  double* vim = Element_GetCurrentImplicitTerm(el) ;
  int nn = Element_GetNbOfNodes(el) ;
  int dim = Geometry_GetDimension(Element_GetGeometry(el)) ;
  IntFct_t*  intfct = Element_GetIntFct(el) ;
  FEM_t* fem = FEM_GetInstance(el) ;
  int ndof = nn*NEQ ;
  int    i ;
  double zero = 0. ;

  /* Initialisation */
  for(i = 0 ; i < ndof ; i++) r[i] = zero ;

  if(Element_IsSubmanifold(el)) return(0) ;

  /* 1. Mechanics */
  /* 1.1 Stresses */
  {
    double* rw = FEM_ComputeStrainWorkResidu(fem,intfct,SIG,NVI) ;

    //#pragma omp parallel for
    for(i = 0 ; i < nn ; i++) {
      int j ;
      for(j = 0 ; j < dim ; j++) R(i,E_mec + j) -= rw[i*dim + j] ;
    }
  }

  /* 1.2 Body forces */
  {
    double* rbf = FEM_ComputeBodyForceResidu(fem,intfct,F_MASS + dim - 1,NVI) ;

    //#pragma omp parallel for
    for(i = 0 ; i < nn ; i++) R(i,E_mec + dim - 1) -= -rbf[i] ;
  }

  return(0) ;

#undef R
}




int  ComputeOutputs(Element_t* el,double t,double* s,Result_t* r)
/** Compute the outputs (r) */
{
  int NbOfOutputs = 4 ;
  double* vim0 = Element_GetCurrentImplicitTerm(el) ;
  double** u  = Element_ComputePointerToCurrentNodalUnknowns(el) ;
  IntFct_t*  intfct = Element_GetIntFct(el) ;
  int np = IntFct_GetNbOfPoints(intfct) ;
  int dim = Geometry_GetDimension(Element_GetGeometry(el)) ;
  FEM_t* fem = FEM_GetInstance(el) ;

  if(Element_IsSubmanifold(el)) return(0) ;

  /*
    Input data
  */
  GetProperties(el,t) ;

  double alpha = HydrDeg(t-t_init) ;
  double young = Element_GetPropertyValue(el,"young") ;
  double young_t = (t >= t_init) ? alpha*young : 0 ;
  double* modulus = &young_t ;

  {
    /* Interpolation functions at s */
    double* a = Element_ComputeCoordinateInReferenceFrame(el,s) ;
    int p = IntFct_ComputeFunctionIndexAtPointOfReferenceFrame(intfct,a) ;
    /* Variables */
    //double* x = ComputeVariables(el,u,vim0,t,0,p) ;

    double* dis  = FEM_ComputeDisplacementVector(fem,u,intfct,p,U_u) ;
    double dis_tot[3] = {0,0,0} ;
    double sig[9] = {0,0,0,0,0,0,0,0,0} ;
    int    i ;

    /* Displacement */
    for(i = 0 ; i < dim ; i++) {
      //dis[i] = FEM_ComputeUnknown(fem,u,intfct,p,U_u + i) ;
      dis_tot[i] = dis[i] ;
    }

    if(ItIsPeriodic) {
      for(i = 0 ; i < dim ; i++) {
        int j ;

        for(j = 0 ; j < dim ; j++) {
          dis_tot[i] += MacroGradient(el,t)[3*i + j] * s[j] ;
        }
      }
    }

    /* Averaging */
    for(p = 0 ; p < np ; p++) {
      double* vim  = vim0 + p*NVI ;

      for(i = 0 ; i < 9 ; i++) sig[i] += SIG[i]/np ;
    }

    i = 0 ;
    Result_Store(r + i++,dis_tot ,"Displacements",3) ;
    Result_Store(r + i++,sig ,"Stresses",9) ;
    Result_Store(r + i++,dis,"Perturbated-displacements",3) ;
    Result_Store(r + i++,modulus,"Young Modulus",3) ;

    if(i != NbOfOutputs) arret("ComputeOutputs") ;
  }

  return(NbOfOutputs) ;
}





double* ComputeVariables(Element_t* el,double** u,double** u_n,double* f_n,double t,double dt,int p)
{
  Model_t*  model  = Element_GetModel(el) ;
  double*   x      = Model_GetVariable(model,p) ;


  {
    IntFct_t* intfct = Element_GetIntFct(el) ;
    FEM_t*    fem    = FEM_GetInstance(el) ;
    int dim = Element_GetDimensionOfSpace(el) ;

    /* Primary Variables */
    {
      int i ;

      // for(i=0; i < 8; i++) printf("%f, ",x[I_SIG + i]);
      // printf("\n");
      // for(i=0; i < 8; i++) printf("%f, ",x[I_SIG_n + i]);
      // printf("\n\n");

      for(i = 0 ; i < dim ; i++) {
        x[U_u + i] = FEM_ComputeUnknown(fem,u,intfct,p,U_u + i) ;
      }

      for(i = dim ; i < 3 ; i++) {
        x[U_u + i] = 0 ;
      }
    }

    /* Strains */
    {
      double* eps =  FEM_ComputeLinearStrainTensor(fem,u,intfct,p,U_u) ;
      int i ;

      if(ItIsPeriodic) {

        for(i = 0 ; i < 9 ; i++) {
          eps[i] += MacroStrain(el,t)[i] ;
        }
      }

      for(i = 0 ; i < 9 ; i++) {
        x[I_EPS + i] = eps[i] ;
      }

      FEM_FreeBufferFrom(fem,eps) ;
    }

    /* Initial stresses */
    {
      double* v00 = Element_GetConstantTerm(el) ;
      double* v0  = v00 + p*NV0 ;
      int i ;

      for(i = 0 ; i < 9 ; i++) {
        x[I_SIG + i] = SIG0[i] ;
      }
    }

      {
        int    i ;

        /* Stresses, strains at previous time step */
        {
          double* eps_n =  FEM_ComputeLinearStrainTensor(fem,u_n,intfct,p,U_u) ;
          double* vim_n = f_n + p*NVI ;

          for(i = 0 ; i < 9 ; i++) {
            x[I_EPS_n   + i] = eps_n[i] ;
            x[I_SIG_n   + i] = SIG_n[i] ;
          }
        }
      }
  }

  /* Needed variables to compute secondary components */

  ComputeSecondaryVariables(el,t,dt,x) ;

  return(x) ;
}



void  ComputeSecondaryVariables(Element_t* el,double t,double dt,double* x)
{
  /* Strains */
  double* eps =  x + I_EPS ;
  double* eps_n =  x + I_EPS_n ;


  /* Backup stresses */
  {
    double* sig  = x + I_SIG ;
    double* sig_n = x + I_SIG_n ;
    double  deps[9] ;
    int    i ;

    // for(i=0; i < NbOfVariables; i++) printf("%f",x[i]);
    // printf("\n\n");


    /* Incremental deformations */
    for(i = 0 ; i < 9 ; i++) deps[i] =  eps[i] - eps_n[i] ;

    //printf("\nAt time %f , eps and eps_n: \n %f, %f\n%f,%f\n%f,%f\n%f,%f\n%f,%f\n%f,%f\n%f,%f\n%f,%f\n%f,%f",t,eps[0],eps_n[0],eps[1],eps_n[1],eps[2],eps_n[2],eps[3],eps_n[3],eps[4],eps_n[4],eps[5],eps_n[5],eps[6],eps_n[6],eps[7],eps_n[7],eps[8],eps_n[8]);


    /* Elastic trial stresses */
    for(i = 0 ; i < 9 ; i++) sig[i] = sig_n[i] ;

    #define C(i,j)  (cijkl[(i)*9+(j)])
    for(i = 0 ; i < 9 ; i++) {
      int  j ;

      for(j = 0 ; j < 9 ; j++) {
        sig[i] += C(i,j)*deps[j] ;
      }
      // printf("\nSig%d : %f", i, sig[i]);
    }
    #undef C
  }

  //printf("\nAt time %f , sig and sign: \n %f, %f\n%f,%f\n%f,%f\n%f,%f\n%f,%f\n%f,%f\n%f,%f\n%f,%f\n%f,%f",t,sig[0],sig_n[0],sig[1],sig_n[1],sig[2],sig_n[2],sig[3],sig_n[3],sig[4],sig_n[4],sig[5],sig_n[5],sig[6],sig_n[6],sig[7],sig_n[7],sig[8],sig_n[8]);


  /* Backup body forces */
  {
    int dim = Element_GetDimensionOfSpace(el) ;
    double* fmass = x + I_Fmass ;
    int    i ;

    for(i = 0 ; i < 3 ; i++) fmass[i] = 0. ;

    fmass[dim-1] = (rho_s)*gravity ;
  }
}




double* MicrostructureElasticTensor(DataSet_t* jdd,double* c)
{
#define C(i,j,k,l)  (c[(((i)*3+(j))*3+(k))*3+(l)])

  {
    double grd[9] = {0,0,0,0,0,0,0,0,0} ;
    int k ;

    for(k = 0 ; k < 3 ; k++) {
      int l ;

      for(l = 0 ; l < 3 ; l++) {
        double sig[9] ;
        int i ;

        /* Set the macro gradient (k,l) as 1 */
        grd[k*3 + l] = 1 ;

        /* For eack (k,l) compute the stresses Sij = Cijkl */
        {
          Session_Open() ;
          Message_SetVerbosity(0) ;

          Message_Direct("\n") ;
          Message_Direct("Start a microstructure calculation") ;
          Message_Direct("\n") ;

          ComputeMicrostructure(jdd,grd,sig) ;

          Session_Close() ;
        }

        for(i = 0 ; i < 3 ; i++) {
          int j ;

          for(j = 0 ; j < 3 ; j++) {
            C(i,j,k,l) = sig[i*3 + j] ;
          }
        }

        grd[k*3 + l] = 0 ;
      }
    }
  }

  return(c) ;
#undef C
}



void ComputeMicrostructure(DataSet_t* jdd,double* macrograd,double* sig)
{
  /* Set input data of the microstructure */
  {
    /* Update the macro-gradient */
    {
      Materials_t* mats = DataSet_GetMaterials(jdd) ;
      int nmats = Materials_GetNbOfMaterials(mats) ;
      int j ;

      for(j = 0 ; j < nmats ; j++) {
        Material_t* mat = Materials_GetMaterial(mats) + j ;
        Model_t* model = Material_GetModel(mat) ;
        Model_ComputePropertyIndex_t* pidx = Model_GetComputePropertyIndex(model) ;

        {
          double* grd = Material_GetProperty(mat) + pidx("macro-gradient") ;
          int i ;

          for(i = 0 ; i < 9 ; i++) {
            grd[i] = macrograd[i] ;
          }
        }
      }
    }
  }

  /* Compute the microstructure */
  {
    Module_t* module_i = DataSet_GetModule(jdd) ;

    Module_ComputeProblem(module_i,jdd) ;
  }

  /* Backup stresses as averaged stresses */
  {
    Mesh_t* mesh = DataSet_GetMesh(jdd) ;

    //Mesh_InitializeSolutionPointers(mesh,sols) ;
    FEM_AverageStresses(mesh,sig) ;
  }
}




void CheckMicrostructureDataSet(DataSet_t* jdd)
{
  /* Set input data of the microstructure */
  {
    /* Update the macro-fctindex */
    {
      Materials_t* mats = DataSet_GetMaterials(jdd) ;
      int nmats = Materials_GetNbOfMaterials(mats) ;
      int j ;

      for(j = 0 ; j < nmats ; j++) {
        Material_t* mat = Materials_GetMaterial(mats) + j ;
        Model_t* model = Material_GetModel(mat) ;
        Model_ComputePropertyIndex_t* pidx = Model_GetComputePropertyIndex(model) ;

        if(!pidx) {
          arret("ComputeMicrostructure(1): Model_GetComputePropertyIndex(model) undefined") ;
        }

        {
          int k = pidx("macro-fctindex") ;
          int i ;

          for(i = 0 ; i < 9 ; i++) {
            Material_GetProperty(mat)[k + i] = 1 ;
          }
        }
      }
    }

    /* Check and update the function of time */
    {
      Functions_t* fcts = DataSet_GetFunctions(jdd) ;
      int nfcts = Functions_GetNbOfFunctions(fcts) ;

      if(nfcts < 1) {
        arret("ComputeMicrostructure(1): the min nb of functions should be 1") ;
      }

      {
        Function_t* func = Functions_GetFunction(fcts) ;
        int npts = Function_GetNbOfPoints(func) ;

        if(npts < 2) {
          arret("ComputeMicrostructure(2): the min nb of points should be 2") ;
        }

        {
          double* t = Function_GetXValue(func) ;
          double* f = Function_GetFValue(func) ;

          Function_GetNbOfPoints(func) = 2 ;
          t[0] = 0 ;
          t[1] = 1 ;
          f[0] = 0 ;
          f[1] = 1 ;
        }
      }
    }

    /* The dates */
    {
      {
        Dates_t* dates = DataSet_GetDates(jdd) ;
        int     nbofdates  = Dates_GetNbOfDates(dates) ;

        if(nbofdates < 2) {
          arret("ComputeMicrostructure(3): the min nb of dates should be 2") ;
        }

        Dates_GetNbOfDates(dates) = 2 ;
      }
    }
  }
}




/* Not used from here */
#if 0
int Cijkl(FEM_t* fem,double* c)
/*
**  Elastic matrix (c), return the shift (dec)
*/
{
#define C1(i,j,k,l)  (c1[(((i)*3+(j))*3+(k))*3+(l)])
  Element_t* el = FEM_GetElement(fem) ;
  IntFct_t*  intfct = Element_GetIntFct(el) ;
  int np = IntFct_GetNbOfPoints(intfct) ;
  double twomu,lame,mu ;
  int    dec = 81 ;
  int    p ;

  twomu   = young/(1 + poisson) ;
  mu      = twomu/2 ;
  lame    = twomu*poisson/(1 - 2*poisson) ;

  for(p = 0 ; p < np ; p++) {
    int    i,j ;
    double* c1 = c + p*dec ;

    /* initialisation */
    for(i = 0 ; i < dec ; i++) c1[i] = 0. ;

    /* Mechanics */
    /* derivative of sig with respect to eps */
    for(i = 0 ; i < 3 ; i++) for(j = 0 ; j < 3 ; j++) {
      C1(i,i,j,j) += lame ;
      C1(i,j,i,j) += mu ;
      C1(i,j,j,i) += mu ;
    }

  }

  return(dec) ;

#undef C1
}
#endif
