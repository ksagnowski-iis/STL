/*                                                          v2.3 - 03/Dec/2004
  =============================================================================

                          U    U   GGG    SSSS  TTTTT
                          U    U  G       S       T
                          U    U  G  GG   SSSS    T
                          U    U  G   G       S   T
                           UUU     GG     SSS     T

                   ========================================
                    ITU-T - USER'S GROUP ON SOFTWARE TOOLS
                   ========================================

       =============================================================
       COPYRIGHT NOTE: This source code, and all of its derivations,
       is subject to the "ITU-T General Public License". Please have
       it  read  in    the  distribution  disk,   or  in  the  ITU-T
       Recommendation G.191 on "SOFTWARE TOOLS FOR SPEECH AND  AUDIO
       CODING STANDARDS".
       =============================================================

MODULE:         FIRFLT, HIGH QUALITY FIR UP/DOWN-SAMPLING FILTER
                Sub-unit: Basic FIR filtering routines

ORIGINAL BY:
                Rudolf Hofmann
                Advanced Development Digital Signal Processing
                PHILIPS KOMMUNIKATIONS INDUSTRIE AG
                Kommunikationssysteme
                Thurn-und-Taxis-Strasse 14
                D-8500 Nuernberg 10 (Germany)

                Phone : +49 911 526-2603
                FAX   : +49 911 526-3385
                EMail : hf@pkinbg.uucp

DESCRIPTION:
        This file contains procedures for FIR up-/down-sampling 
	filtering, independently of filter characteristics initialized by
	functions present in other sub-units of this module.

FUNCTIONS:
  Global (have prototype in firflt.h)
         = hq_kernel(...)        :  FIR-filter function
         = hq_reset(...)         :  clear state variables
                                    (needed only if another signal should
                                    be processed with the same filter)
         = hq_free(...)          :  deallocate FIR-filter memory

  Local (Used by other sub-units of this module, should not be needed by 
         the user's program. Prototypes here and in the sub-units that use 
	 it, but not in firflt.h)
         = fir_initialization(...) : common initialization function for
                                   all filter types;
  Local (should be used only here -- prototypes only in this file)
         = fir_upsampling_kernel(...) : kernel function for all FIR
                                   up-sampling procedures;
         = fir_downsampling_kernel(...) : kernel function for all FIR
                                   down-sampling procedures;

HISTORY:
    16.Dec.91 v0.1 First beta-version <hf@pkinbg.uucp>
    28.Feb.92 v1.0 Release of 1st version to UGST <hf@pkinbg.uucp>
    20.Apr.94 v2.0 Added new filtering routines: modified IRS at 16kHz and
                   48kHz, Delta-SM, Linear-phase band-pass.
                   <simao@cpqd.ansp.br>
    30.Sep.94 v2.1 Updated to accomodate changes in the name of the name and
                   slitting of module in several files, for ease of expansion.
    22.Feb.96 v2.2 Changed inclusion of stdlib.h to inconditional, as
                   suggested by Kirchherr (FI/DBP Telekom) to run under
				   OpenVMS/AXP <simao@ctd.comsat.com>
    03.Dec.04 v2.3 Added correction in fir_downsampling_kernel() for sample-based 
				   operation.	<Cyril Guillaume & Stephane Ragot - stephane.ragot@francetelecom.com>

  =============================================================================
*/


/*
 * ......... INCLUDES .........
 */
#include <stdio.h>
#include <stdlib.h>		/* General utility definitions */

#include "firflt.h"		/* Global definitions for FIR-FIR filter */


/*
 * ......... Local function prototypes .........
 */


SCD_FIR *fir_initialization ARGS((long lenh0, float h0[], double gain, 
                                                 long idwnup, int hswitch));

static long     fir_upsampling_kernel ARGS((long lenx, float *x_ptr, 
                      float *y_ptr, long lenh0, float *h0_ptr, float *T_ptr, 
                      long iupfac));
static long     fir_downsampling_kernel ARGS((long lenx, float *x_ptr, 
                      float *y_ptr, long lenh0, float *h0_ptr, float *T_ptr, 
                      long downfac, long *k0_ptr));


/*
 * ...................... BEGIN OF FUNCTIONS .........................
 */

/*
  ============================================================================

        long hq_kernel (long lseg, float *x_ptr, SCD_FIR *fir_ptr,
        ~~~~~~~~~~~~~~  float *y_ptr);

        Description:
        ~~~~~~~~~~~~

        Works as switch to FIR-kernel functions; the address of the
        according  function is generated by the initialization
        procedures.

        WARNING! Prior to the first call one of the initialization must
        be called to allocate memory for state variables and the get the
        desired filter coefficients!

        After return from this function, the state variables are  kept
        for allowing segment-wise filtering by successive calls of
        `hq_kernel'.  This is usefull when large files have to be
        processed!


        Parameters:
        ~~~~~~~~~~~
        lseg: .... (In)    number of input samples
        x_ptr: ... (In)    array with input samples
        fir_ptr .. (InOut) pointer to FIR-struct
        y_ptr .... (Out)   output samples

        Return value:
        ~~~~~~~~~~~~~
        Returns the number of filtered samples.

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>

 ============================================================================
*/
long            hq_kernel(lseg, x_ptr, fir_ptr, y_ptr)
  long            lseg;
  float          *x_ptr;
  SCD_FIR        *fir_ptr;
  float          *y_ptr;
{

  if (fir_ptr->hswitch == 'U')	/* call up-sampling procedure */
    return
      fir_upsampling_kernel(	/* returns number of output samples */
			    lseg,	/* In   : length of input signal */
			    x_ptr,	/* In   : array with input samples */
			    y_ptr,	/* Out  : array with output samples */
			    fir_ptr->lenh0,	/* In   : number of
						 * FIR-coefficients */
			    fir_ptr->h0,	/* In   : array with
						 * FIR-coefficients */
			    fir_ptr->T,	/* InOut: state variables */
			    fir_ptr->dwn_up	/* In   : upsampling factor */
      );
  else				/* call down-sampling procedure */
    return
      fir_downsampling_kernel(	/* returns number of output samples */
			      lseg,	/* In   : length of input signal */
			      x_ptr,	/* In   : array with input samples */
			      y_ptr,	/* Out  : array with output samples */
			      fir_ptr->lenh0,	/* In   : number of
						 * FIR-coefficients */
			      fir_ptr->h0,	/* In   : array with
						 * FIR-coefficients */
			      fir_ptr->T,	/* InOut: state variables */
			      fir_ptr->dwn_up,	/* In   : downsampling factor */
			      &(fir_ptr->k0)	/* InOut: starting index in
						 * x-array */
      );
}
/* .......................... End of hq_kernel() .......................... */


/*
  ============================================================================

        void hq_free (SCD_FIR *fir_ptr);
        ~~~~~~~~~~~~

        Description:
        ~~~~~~~~~~~~

        Deallocate memory, which was allocated by an earlier call to one
        of the initilization routines. WARNING! pointer to
        SCD_FIR-struct must not be a (NULL *)

        Parameters:
        ~~~~~~~~~~~
        fir_ptr: (InOut) pointer to struct SCD_FIR;

        Return value:
        ~~~~~~~~~~~~~
        None.


        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>

 ============================================================================
*/
void            hq_free(fir_ptr)
  SCD_FIR        *fir_ptr;
{

  free(fir_ptr->T);		/* free state variables */
  free(fir_ptr->h0);		/* free state impulse response */
  free(fir_ptr);		/* free allocated struct */
}
/* .......................... End of hq_free() .......................... */



/*
  ============================================================================

        void hq_reset (SCD_FIR *fir_ptr);
        ~~~~~~~~~~~~~

        Description:
        ~~~~~~~~~~~~

        Clear state variables in SCD_FIR struct.
        WARNING! A pointer to a valid SCD_FIR-struct must be present.

        Parameters:
        ~~~~~~~~~~~
        fir_ptr: (InOut) pointer to struct SCD_FIR;

        Return value:
        ~~~~~~~~~~~~~
        None.

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>

 ============================================================================
*/
void            hq_reset(fir_ptr)
  SCD_FIR        *fir_ptr;
{
  long            k;
  for (k = 0; k < fir_ptr->lenh0 - 1; k++)	/* clear delay line */
    fir_ptr->T[k] = 0.0;	/* (= state variables) */
  fir_ptr->k0 = 0;		/* default starting index in x-array */
}
/* .......................... End of hq_reset() .......................... */



/*
  ============================================================================

        SCD_FIR *fir_initialization (long lenh0, float h0[], double gain,
        ~~~~~~~~~~~~~~~~~~~~~~~~~~~  long idwnup, int hswitch);

        Description:
        ~~~~~~~~~~~~

        Allocate & initialize struct for down/up-sampling procedures

        Parameters:
        ~~~~~~~~~~~
        lenh0: ....... (In) number of FIR-coefficients
        h0[]: ........ (In) FIR-coefficients
        gain: ........ (In) gain factor for FIR-coeffic.
        idwnup: ...... (In) Down-/Up-sampling factor
        hswitch: ..... (In) switch to up/downsampling
                            procedure in "hq_kernel"

        Return value:
        ~~~~~~~~~~~~~
        Pointer to a SCD_FIR structure.

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>
        12.Mar.92 v1.1 Corrected casting of malloc.

 ============================================================================
*/
SCD_FIR *fir_initialization(lenh0, h0, gain, idwnup, hswitch)
  long            lenh0;
  float           h0[];
  double          gain;
  long            idwnup;
  int /* char */  hswitch;
{
  SCD_FIR        *ptrFIR;	/* pointer to the new struct */
  float           fak;
  long            k;


/*
 * ......... ALLOCATION OF MEMORY .........
 */

  /* Allocate memory for a new struct */
  if ((ptrFIR = (SCD_FIR *) malloc((long) sizeof(SCD_FIR))) ==(SCD_FIR *) NULL)
  {
    return 0;
  }

  /* Allocate memory for delay line */
  if ((ptrFIR->T = (float *) malloc((lenh0 - 1) * sizeof(fak))) == (float *) 0)
  {
    free(ptrFIR);		/* deallocate struct FIR */
    return 0;
  }

  /* Allocate memory for impulse response */
  if ((ptrFIR->h0 = (float *) malloc(lenh0 * sizeof(fak))) == (float *) 0)
  {
    free(ptrFIR->T);		/* deallocate delay line */
    free(ptrFIR);		/* deallocate struct FIR */
    return 0;
  }

/*
 * ......... STORE VARIABLES INTO STATE VARIABLE .........
 */

  /* Store number of FIR-coefficients */
  ptrFIR->lenh0 = lenh0;

  /* Fill FIR coefficients into struct; for upsampling tasks the
   * FIR-coefficients are multiplied by the upsampling factor 'gain' */
  for (k = 0; k <= ptrFIR->lenh0 - 1; k++)
    ptrFIR->h0[k] = gain * h0[k];

  /* Store down-/up-sampling factor */
  ptrFIR->dwn_up = idwnup;

  /* Store switch to FIR-kernel (up- or downsampling function) */
  ptrFIR->hswitch = hswitch;

  /* Clear Delay Line */
  for (k = 0; k < ptrFIR->lenh0 - 1; k++)
    ptrFIR->T[k] = 0.0;

  /* Store default starting index for the x-array */
  /* NOTE: for down-sampling: if the number of input samples is not a
   * multiple of the down-sampling factor, k0 points to the first sample in
   * the next input segment to be processed */
  ptrFIR->k0 = 0;

  /* Return pointer to struct */
  return (ptrFIR);
}
/* ..................... End of fir_initialization() ..................... */


/*
  ============================================================================

        long fir_downsampling_kernel (long lenx, float *x_ptr, float *y_ptr,
        ~~~~~~~~~~~~~~~~~~~~~~~~~~~~  long lenh0, float *h0_ptr, float *T_ptr,
                                      long downfac, long *k0_ptr);

        Description:
        ~~~~~~~~~~~~

        FIR-Filter (kernel) (for down-sampling, including downsampling
        factor 1).

        Parameters:
        ~~~~~~~~~~~
        lenx: ..... (In)    length of input signal
        x: ........ (In)    array with input samples
        y: ........ (Out)   array with output samples
        lenh0: .... (In)    number of  FIR-coefficients
        h0: ....... (In)    array with FIR-coefficients
        T: ........ (InOut) state variables
        downfac: .. (In)    downsampling factor
        k0: ....... (InOut) offset in x-array

        Return value:
        ~~~~~~~~~~~~~
        Number of filtered samples.

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.1992 v1.0 Release of 1st version <hf@pkinbg.uucp>
        12.Jul.2000 -    Bug identified; correction solicited  <simao>
		03.Dec.2004 v2.3 Sample-based bug solved. <Cyril Guillaume & Stephane Ragot - stephane.ragot@francetelecom.com>

 ============================================================================
*/
static long     fir_downsampling_kernel(lenx, x, y, lenh0, h0, T, downfac, k0)
  long            lenx;
  float          *x;
  float          *y;
  long            lenh0;
  float          *h0;
  float          *T;
  long            downfac;
  long           *k0;
{
  long            ktrans, kx, kStart, ky, kappa;	/* loop indices */


/*
  * ......... First Step: Transition from k=0 ... k=lenh0-2 .........
  */

  kStart = *k0;
  ky = 0;			/* starting index in output array (y) */
  ktrans = lenh0 - 1;
  if (ktrans > lenx - 1)
    ktrans = lenx - 1;		/* x[*] less than h0[*]? */

  for (kx = *k0; kx <= ktrans; kx += downfac)
  {
    y[ky] = x[kx] * h0[0];	/* first part in dot-product */
    for (kappa = 1; kappa <= kx; kappa++)	/* first part from x-array */
    {
      y[ky] += x[kx - kappa] * h0[kappa];
    }

    for (kappa = kx + 1; kappa < lenh0; kappa++)	/* second part from
							 * T-array */
    {
      y[ky] += T[lenh0 - 2 + kx + 1 - kappa] * h0[kappa];
    }
    ky++;
    kStart = kx;		/* Save index of last processed sample */
  }


/*
  * ......... Second Step: remaining part in x-array .........
  */

  *k0 = kStart;
  for (kx = kStart + downfac; kx <= lenx - 1; kx += downfac)
  {
    y[ky] = x[kx] * h0[0];	/* first part in dot-product */
    for (kappa = 1; kappa <= lenh0 - 1; kappa++)
    {
      y[ky] += x[kx - kappa] * h0[kappa];	/* remaining part of
						 * dot-product */
    }
    ky++;
    *k0 = kx;
  }

  /* if the number of input samples is not a multiple of the down sampling
   * factor, k0 points to the first sample in the next input segment to be
   * processed */
  if (*k0 <= lenx -1)
  {
      *k0 = *k0 + downfac;
      *k0 = *k0 - lenx;
  } else						/* if the offset was greater than the block length, nothing was done.
								 * Just decrease the value of this offset with the size of this "skipped" block */
  {
	  *k0 = *k0 - lenx;
  }


/*
 * ......... Last Step: copy end of x-array into T-array .........
 *                      (update of delay line)
 */

  if (lenx >= lenh0 - 1)	/* ... all samples taken from x-array */
  {
    for (kappa = 0; kappa <= lenh0 - 2; kappa++)
    {
      T[kappa] = x[lenx + 1 - lenh0 + kappa];
    }
  }
  else
  {
    /* Left-Shift of T-array */
    for (kappa = 0; kappa <= lenh0 - 2 - lenx; kappa++)
    {
      T[kappa] = T[kappa + lenx];
    }

    /* Copy complete x-array -> T-array */
    for (kappa = lenh0 - 1 - lenx; kappa <= lenh0 - 2; kappa++)
    {
      T[kappa] = x[lenx - 1 + kappa - (lenh0 - 2)];
    }
  }

  /* Return number of output samples */
  return ky;
}
/* ................... End of fir_downsampling_kernel() ................... */



/*
  ============================================================================

        long fir_upsampling_kernel (long lenx, float *x_ptr, float *y_ptr,
        ~~~~~~~~~~~~~~~~~~~~~~~~~~  long lenh0, float *h0_ptr, float *T_ptr,
                                    long iupfac);

        Description:
        ~~~~~~~~~~~~

        FIR-Filter (kernel) for upsampling routine.

        Parameters:
        ~~~~~~~~~~~
        lenx: .... (In)    length of input signal
        x: ....... (In)    array with input samples
        y: ....... (Out)   array with output samples
        lenh0: ... (In)    number of  FIR-coefficients
        h0: ...... (In)    array with FIR-coefficients
        T: ....... (InOut) state variables
        iupfac: .. (In)    upsampling factor

        Return value:
        ~~~~~~~~~~~~~
        Number of filtered samples.

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>

 ============================================================================
*/
static long     fir_upsampling_kernel(lenx, x, y, lenh0, h0, T, iupfac)
  long            lenx;
  float          *x, *y;
  long            lenh0;
  float          *h0, *T;
  long            iupfac;
{
  long            ktrans, iup, kx, kStart, ky, kappa;	/* loop indices */


  ky = 0;			/* starting index in output array (y) */

/*
  * ......... FIRST STEP: Transition from k=(0..lenh0/iupasm-2) .........
  */

  kStart = 0;
  ktrans = (lenh0 / iupfac > lenx ?	/* length of transition */
	    lenx :
	    lenh0 / iupfac);
  for (kx = 0; kx <= ktrans - 1; kx++)
  {
    /* Loop over #iupfac partial FIR coefficients */
    for (iup = 0; iup <= iupfac - 1; iup++)
    {
      /* ... first contribution in dot-product */
      y[ky] = x[kx] * h0[iup];

      /* ... compute partial dot-product with source data from x-array */
      for (kappa = 1; kappa <= kx; kappa++)
      {
	y[ky] += x[kx - kappa] * h0[iup + kappa * iupfac];
      }

      /* ... compute rest of dot-product with source data from T-array */
      for (kappa = kx + 1; kappa < lenh0 / iupfac; kappa++)
      {
	y[ky] += T[lenh0 / iupfac - 2 + kx + 1 - kappa]
	  * h0[iup + kappa * iupfac];
      }
      ky++;
    }
    kStart = kx;		/* Save index of last processed sample */
  }


/*
 * ......... SECOND STEP: compute remaining dot-products ..........
 *                        completely with data from x[*]
 */

  for (kx = kStart + 1; kx <= lenx - 1; kx++)
  {
    for (iup = 0; iup <= iupfac - 1; iup++)
    {
      /* ... first contribution in dot-product */
      y[ky] = x[kx] * h0[iup];

      /* ... compute partial dot-product with source data from x-array */
      for (kappa = 1; kappa <= lenh0 / iupfac - 1; kappa++)
      {
	y[ky] += x[kx - kappa] * h0[iup + kappa * iupfac];
      }
      ky++;
    }
  }


/*
 * ......... Last Step: copy end of x-array into T-array .........
 *                        (update of delay line)
 */

  if (lenx >= lenh0 / iupfac - 1)
  {
    /* ... all samples taken from x-array */
    for (kappa = 0; kappa <= lenh0 / iupfac - 2; kappa++)
    {
      T[kappa] = x[lenx + 1 - lenh0 / iupfac + kappa];
    }
  }
  else
  {
    /* ... left-Shift of T-array */
    for (kappa = 0; kappa <= lenh0 / iupfac - 2 - lenx; kappa++)
    {
      T[kappa] = T[kappa + lenx];
    }

    /* ... copy complete x-array -> T-array */
    for (kappa = lenh0 / iupfac - 1 - lenx;
	 kappa <= lenh0 / iupfac - 2; kappa++)
    {
      T[kappa] = x[lenx - 1 + kappa - (lenh0 / iupfac - 2)];
    }
  }

  return ky;
}
/* ................. End of fir_upsampling_kernel() .................. */


/* **************************** END OF FIR-LIB.C ************************** */
