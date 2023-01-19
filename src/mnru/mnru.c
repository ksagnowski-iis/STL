/*                                                        31.JULY.1995 v.2.00
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


MODULE:         NEW VERSION OF THE MNRU.C, MODULATED NOISE REFERENCE
                UNIT'S MODULE, MNRU (ACCORDING P.81, 1995).

ORIGINAL BY:    Simao Ferraz de Campos Neto <tdsimao@venus.cpqd.ansp.br>,
                based on ITU-T STL92 MNRU, which was based on
                CSELT's MNRU.FOR algorithm.

FUNCTIONS:

MNRU_process: ......	Processes the `input' buffer of `n' samples (in
                        float format), adding to it modulated noise at a
                        `Q' dB SNR level, if `mode' is modulated noise, and
                        saving to the float buffer `output'. Otherwise, if
                        mode is noise-only, then saves only noise to
                        `output' buffer, or the filtered signal, if
                        signal-only `mode'. Depending on the `operation'
                        chosen, state variables in `s' are reset, as well
                        as memory allocated (start), kept as are (continue)
                        or memory is released (stop). Written for narrow-band
                        model (input data is sampled at 8 kHz). Its prototype
                        is in mnru.h.

random_MNRU: .......... Generates gaussian-like noise samples for use by the
                        MNRU_process function. Depends on a seed when `*mode'
                        is 1 (RANDOM_RESET), causing the initialization of
                        the generator. Then `*mode' is changed to 0
                        (RANDOM_RUN), and on a state variable that should
                        not be changed by the user. Its prototype is found
                        in mnru.h.

HISTORY:

  25.Set.91  v1.0F      Fortran version released to UGST by CSELT/Italy.
  23.Jan.92  v1.0C      C version, bit-exact with Fortran impl. in VAX
                        <tdsimao@venus.cpqd.ansp.br>
  27.Jan.92  v1.1       Modular version with portable RNG
                        <tdsimao@venus.cpqd.ansp.br>
  18.May.92  v1.2       Removal of of addition of +-0.5, at the exit of
                        MNRU_process, needed to make it work with data in the
                        normalized range. <tdsimao@venus.cpqd.ansp.br>
  31.Jul.95  v2.0       Redifinition of the module due to the revision of P.81:
                        no more 1:5 up/down-sampling, inclusion of a DC filter
                        and a low-pass (instead of a band-pass) output filter.
                        To increase speed, a new random number generator
                        has been included. Works for both narrow-band and
                        wideband speech.
=============================================================================
*/

/* Definitions for the algorithm itself */
#include "mnru.h"

/* Coefficients for P.50 IIR and FIR filters */
#include "filtering_coeffs.h"
#include "filtering_routines.h"

/* General includes */
#include <math.h>
#include <stdlib.h>             /* for calloc(), free() */
#include <string.h>             /* for memset() */
#include "ugst-utl.h"           /* for ran16_32c */

#ifndef STL92_RNG               /* Uses the new Random Number Generator */
#define random_MNRU new_random_MNRU

/* Local function prototypes */
float new_random_MNRU ARGS ((char *mode, new_RANDOM_state * r, long seed, float *fseed));
float ran_vax ARGS ((void));
unsigned long ran16_32c ARGS ((void));

/*
  =============================================================================

	new_random_MNRU (char *mode, RANDOM_state *r, long seed, float *fseed)
        ~~~~~~~~~~~~~~~

        Description:
        ~~~~~~~~~~~~

        Random number generator based on a gaussian random
	number table accessed by a uniform random number generator.

	The gaussian random sample table is generated at start-up time
        by a Monte-Carlo algorithm using a linear congruential generator
        (LCG). During run-time the algorithm accesses randomly this table
	using indeces generated by another LCG.

	To (re)initialize the sequence, use mode=RANDOM_RESET (the routine
	will change mode to RANDOM_RUN).

	Functions used:
	~~~~~~~~~~~~~~~
        ran_vax():    generate uniformly distributed samples in the range
                      0..1. Return a float number.
	ran16_32C():  generate uniformly distributed samples in the range
	              0..65535 (2^16-1)

        Prototype: MNRU.H
        ~~~~~~~~~~

        History:
        ~~~~~~~~
        01.Jul.95  1.0	Created based on the random number generator
                        implemented by Aachen University and used by
                        the ITU-T 8kbit/s speech codec host laboratory
                        (in hardware). <simao@ctd.comsat.com>

=============================================================================
*/
#define S1    -8.0              /* s1 = my - 4 * sigma (=-8.0 for gaussian noise) */
#define S2     8.0              /* s2 = my + 4 * sigma (= 8.0 for gaussian noise) */
#define DIF    16.0             /* s2 - s1 */
#define MO     8.0              /* mo = 2 * (sigma)^2 (= 8.0 for gaussian noise) */
#define BIT15  32767.0
#define TABLE_SIZE 8192         /* 2^13 */
#define ITER_NO 8
#define FACTOR 8                /* = 65536(max.no returned by ran16_32c) div.by TABLE_SIZE */
float new_random_MNRU (char *mode, RANDOM_state * r, long seed, float *fseed) {
  long i;
  double z1;                    /* white random number -8...8 */
  /* weighted with a gaussian distribution */
  double z2;                    /* white random number 0...1 */
  double phi;                   /* gauss curve */

  extern float ran_vax ();

  long index;

  /* *** RUN INITIALIZATION SEQUENCE *** */
  if (*mode == RANDOM_RESET) {  /* then reset sequence */
    /* Toogle mode from reset to run */
    *mode = RANDOM_RUN;

    /* Allocate memory for gaussian table */
    r->gauss = (float *) calloc (TABLE_SIZE, sizeof (float));

    /* Generate gaussian random number table */
    for (i = 0L; i < TABLE_SIZE; i++) {
      /* Interact until find gaussian sample */
      do {
        z1 = S1 + DIF * (double) ran_vax ();
        phi = exp (-(z1) * (z1) / MO);
        z2 = (double) ran_vax ();
      } while (z2 > phi);

      /* Save gaussian-distributed sample in table */
      r->gauss[i] = (float) z1;
    }
  }

  /* *** REAL GENERATOR (after initialization) ** */
  for (z1 = 0, i = 0; i < ITER_NO; i++) {
    index = ran16_32c (fseed) / FACTOR;
    z1 += r->gauss[index];
  }
  z1 /= 2;                      /* provisional */

  /* Return gaussian sample */
  return ((float) z1);
}

#undef TABLE_SIZE
#undef BIT15
#undef MO
#undef DIF
#undef S2
#undef S1
/*  .................... End of new_random_MNRU() ....................... */


/*
  ===========================================================================
  float ran_vax(void);
  ~~~~~~~~~~~~~

  Description:
  ~~~~~~~~~~~~

  Function that simulates the VAX Fortran function RAN(x), that returns
  a number uniformly distributed between 0.0 and 1.0. This implementation
  is based on Aachen University's randm() function of the narrow-band
  MNRU table-generation program montrand.c by CA (6.3.90).

  Parameters: none.
  ~~~~~~~~~~~

  Return value:
  ~~~~~~~~~~~~~
  An float number uniformly distributed in the range 0.0 and 1.0.

  Author:
  ~~~~~~~
  Simao Ferraz de Campos Neto
  Comsat Laboratories                  Tel:    +1-301-428-4516
  22300 Comsat Drive                   Fax:    +1-301-428-9287
  Clarksburg MD 20871 - USA            E-mail: simao@ctd.comsat.com

  History:
  ~~~~~~~~
  01.Jul.95  v1.00  Created, adapted from montrand.c

  ===========================================================================
*/
#define CONST         69069
#define INIT          314159265L
#define BIT32         4294967296.0
float ran_vax () {
  static unsigned long seed, buffer;
  static float ran;
  static short firsttime = 0;

  if (firsttime == 0) {
    firsttime = 1;
    seed = INIT;
  }

  seed = seed * CONST + 1;      /* includes the mod 2**32 operation */
  buffer = seed & 0xFFFFFF00;   /* mask the first 24 bit */
  ran = (float) buffer / BIT32; /* and divide by 2**32 to get random */

  return (ran);
}

/*  ......................... End of ran_vax() ............................ */


/*
  ===========================================================================
  unsigned long ran16_32c(void);
  ~~~~~~~~~~~~~~~~~~~~~~~

  Description:
  ~~~~~~~~~~~~

  Function that simulates the DSP32C function RAN24(), modified to return
  a number between 0 and 2^16-1. This is based on Aachen University's
  randm() of the narrow-band MNRU program mnrusim.c by PB (08.04.1991).

  Parameters: none.
  ~~~~~~~~~~~

  Return value:
  ~~~~~~~~~~~~~
  An unsigned long number in the range 0 and 2^16-1.

  Author:
  ~~~~~~~
  Simao Ferraz de Campos Neto
  Comsat Laboratories                  Tel:    +1-301-428-4516
  22300 Comsat Drive                   Fax:    +1-301-428-9287
  Clarksburg MD 20871 - USA            E-mail: simao@ctd.comsat.com

  History:
  ~~~~~~~~
  01.Jul.95  v1.00  Created, adapted from mnrusim.c

  ===========================================================================
*/
#define BIT24	16777216.0
#define BIT8    256.0
unsigned long ran16_32c () {
  static float seed = 12345.0;
  double buffer1, buffer2;
  long seedl;
  unsigned long result;

  buffer1 = ((253.0 * seed) + 1.0);
  buffer2 = (buffer1 / BIT24);
  seedl = ((long) buffer2) & 0x00FFFFFFL;
  seed = buffer1 = buffer1 - (float) seedl *BIT24;
  result = buffer1 / BIT8;

  return result;
}

#undef BIT8
#undef BIT24
/*  .................... End of ran16_32c() ....................... */

#else /* Use the original MNRU noise generator */

#define random_MNRU ori_random_MNRU

/* Local function prototypes */
float ori_random_MNRU ARGS ((char *mode, ori_RANDOM_state * r, long seed));

/*
  ===========================================================================

	ori_random_MNRU (char *mode, RANDOM_state *r, long seed)
        ~~~~~~~~~~~~~~~

        Description:
        ~~~~~~~~~~~~

        Random number generator based on Donald Knuth subtractive
        method [1] and in Press [2] C implementation (ran3()).
        The core of the routine generates a uniform deviate between -0.5
        and 0.5. By calling this core several times (after the initialization
        phase), the cumulation of the uniform noise samples leads to a
        gaussian noise (central limit theorem), thus generating a noise
        suitable for the MNRU module. Tests showed that the SNR of the
        signal processed by the MNRU is very close to the desired value
        of Q when the number of cumulations (defined by ITER_NO) is 47.

	To (re)initialize the sequence, use mode=RANDOM_RESET (the routine
	will change mode to RANDOM_RUN).

        According to [1], any large MBIG and any smaller (but still
        large) MSEED can be used; this keeps the values chose by [2],
        pg.212. The dimension of 56 to ma MUST be kept as is, as well
        as inextp=31 [1].


        [1] Knuth, D.; "Seminumerical Algorithms", 2nd. ed., Vol.2 of
           "The Art of Computer Programming"; Addison-Wesley, Mass;
            1981, Parts 3.2-3.3.

        [2] Press,W.H; Flannery,B.P; Teukolky,S.A.; Vetterling, W.T.;
           "Numerical Recipes in C: The Art of Scientific Computing";
            Cambridge University Press, Cambridge; 1990, 735 pp.
           (ISBN 0-521-35465-X)


        Prototype: MNRU.H
        ~~~~~~~~~~

        History:
        ~~~~~~~~
        27.Jan.92  1.0	Adaptation of [2]'s implementation for UGST
                        MNRU module.<tdsimao@venus.cpqd.ansp.br>

=============================================================================
*/

#define MBIG 1000000000
#define MSEED 161803398
#define MZ 0
#define FAC (1.0/MBIG)
#define ITER_NO 47

float ori_random_MNRU (char *mode, RANDOM_state r, long seed) {
  long mj, mk;
  long i, ii, k, iter;
  float tmp;


/*
 *   RESET OF RANDOM SEQUENCE
 */

  if (*mode == RANDOM_RESET) {  /* then reset sequence */
    /* Toogle mode from reset to run */
    *mode = RANDOM_RUN;

    /* Initialize ma[55] using `seed' and `MSEED' */
    mj = MSEED - (seed < 0 ? -seed : seed);
    mj %= MBIG;
    r->ma[55] = mj;
    mk = 1;

    /* Now initialize the rest of the table ma with numbers that are not specially random, in a slightly random order */
    for (i = 1; i <= 54; i++) {
      ii = (21 * i) % 55;
      r->ma[ii] = mk;
      mk = mj - mk;
      if (mk < MZ)
        mk += MBIG;
      mj = r->ma[ii];
    }

    /* Warming-up the generator */
    for (k = 1; k <= 4; k++)
      for (i = 1; i <= 55; i++) {
        r->ma[i] -= r->ma[1 + (i + 30) % 55];
        if (r->ma[i] < MZ)
          r->ma[i] += MBIG;
      }

    /* Prepar indices for 1st.generated number */
    r->inext = 0;
    r->inextp = 31;             /* The constant 31 is special; see [1] */
    r->idum = 1;
  }


/*
 *  REAL START (after initialization)
 */

  /* Accumulate samples to make an approxiamtion of the 'central limit' */
  for (tmp = 0, iter = 0; iter < ITER_NO; iter++) {
    /* Increment inext,inextp (mod 55) */
    if (++r->inext == 56)
      r->inext = 1;
    if (++r->inextp == 56)
      r->inextp = 1;

    /* Generate a new random number, subtractively */
    mj = r->ma[r->inext] - r->ma[r->inextp];

    /* Check range */
    if (mj < MZ)
      mj += MBIG;

    /* Save and return random number */
    r->ma[r->inext] = mj;
    tmp += (mj * FAC - 0.5);
  }
  return (tmp);
}

#undef ITER_NO
#undef MBIG
#undef MSEED
#undef MZ
#undef FAC
/*  .................... End of ori_random_MNRU() ....................... */

#endif /* *********************** STL92_RNG ****************************** */



/*
  ==========================================================================

        double *MNRU_process (char operation, MNRU_state *s,
        ~~~~~~~~~~~~~~~~~~~~  float *input, float *output,
                              long n, long seed, char mode, double Q)

        Description:
        ~~~~~~~~~~~~

        Module for addition of modulated noise to a vector of `n' samples,
        according to ITU-T Recommendation P.81, for the
        narrow-band model. Depending on the `mode', it:

        - add modulated noise to the `input' buffer at a SNR level of
          `Q' dB, saving to `output' buffer (mode==MOD_NOISE);

        - put into `output' only the noise, without adding to the original
          signal (mode==NOISE_ONLY);

        - copy to `output' the `input' samples (mode==SIGNAL_ONLY);

        There is the need of state variables, which are declared in MNRU.H.
        These are reset calling the function with the argument `operation'
        set as MNRU_START. In the last call of the function, call it with
        operation=MNRU_STOP, to release the memory allocated for the
        processing. Normal operation is followed when operation is set as
        MNRU_CONTINUE.

        Valid inputs are:
        operation:    MNRU_START, MNRU_CONTINUE, MNRU_STOP (see description
                      above; defined in MNRU.H);
        s:	      a pointer to a structure defined as MNRU_state, as in
        	      MNRU.H;
        input:        pointer to input float-data vector; must represent
                      8 kHz speech samples.
        output:	      pointer to output float-data vector; will represent
                      8 kHz speech samples.
        n:	      long with the number of samples (float) in input;
        seed:	      initial value for random number generator;
        mode:	      operation mode: MOD_NOISE, SIGNAL_ONLY, NOISE_ONLY
        	      (see description above; defined in MNRU.H);
        Q:	      double defining the desired value for the signal-to-
                      modulated-noise for the output data.

        ==================================================================
        NOTE! New values of `seed', `mode' and `Q' are considered only
              when operation==MNRU_START, because they are considered as
              INITIAL state values.
        ==================================================================

        For more details on the algorithm, see the documentation related.


        Return Value:
        ~~~~~~~~~~~~~
        Returns a (double *)NULL if uninitialized or if initialization
        failed; returns a (double *) to the 20 kHz data vector if reset was
        OK and/or is in "run" (MNRU_CONTINUE) operation.


        History:
        ~~~~~~~~
        05.Feb.1992     1.10 Release of the modular version.
                             <tdsimao@venus.cpqd.ansp.br>
        05.Feb.1992     2.00 Updated according to the new P.81:
                             - no up/downsampling
			     - input signal DC removal filter
			     - output low-pass filter (instead of band-pass)
			     <simao@ctd.comsat.com>

  ==========================================================================
*/
/* original RPELTP: #define ALPHA 0.999 */
#define ALPHA 0.985
#define DNULL (double *)0

// Noise gain definition for NB and WB MNRU
#ifdef STL92_RNG
#define NOISE_GAIN 0.541
#else
/* NOISE_GAIN = 0.3795 for best match with the average SNR */
/*              0.3787 for best best match with the total SNR */
/*              0.3793 for a "balanced" middle-way between both SNRs */
#define NOISE_GAIN 0.3793
#endif

double *MNRU_process (char operation, MNRU_state * s, float *input, float *output, long n, long seed, char mode, double Q, float *fseed) {
// Noise gain definition for P.50 FB MNRU
#define P50_NOISE_GAIN 3.0287

  long count, i;
  double noise, tmp;
  register double inp_smp, out_tmp, out_flt;


  /*
   *    ..... RESET PORTION .....
   */

  /* Check if is START of operation: reset state and allocate memory buffer */
  if (operation == MNRU_START) {

    /* Reset clip counter */
    s->clip = 0;

    /* Allocate memory for sample's buffer */
    if ((s->vet = (double *) calloc (n, sizeof (double))) == DNULL)
      return ((double *) DNULL);

    /* Seed for random number generation */
    s->seed = seed;

    /* Gain for signal path */
    if (mode == MOD_NOISE)
      s->signal_gain = 1.000;
    else if (mode == SIGNAL_ONLY)
      s->signal_gain = 1.000;
    else                        /* (mode == NOISE_ONLY) */
      s->signal_gain = 0.000;

    /* Gain for noise path */
    if (mode == MOD_NOISE || mode == NOISE_ONLY)
      s->noise_gain = NOISE_GAIN * pow (10.0, (-0.05 * Q));
    else                        /* (mode == SIGNAL_ONLY) */
      s->noise_gain = 0;

    /* Flag for random sequence initialization */
    s->rnd_mode = RANDOM_RESET;

    /* Initialization of the output low-pass filter */
    /* Cleanup memory */
    memset (s->DLY, '\0', sizeof (s->DLY));

#ifdef NBMNRU_MASK_ONLY
    /* Load numerator coefficients */
    s->A[0][0] = 0.758717518025;
    s->A[0][1] = 1.50771485802;
    s->A[0][2] = 0.758717518025;
    s->A[1][0] = 0.758717518025;
    s->A[1][1] = 1.46756552150;
    s->A[1][2] = 0.758717518025;

    /* Load denominator coefficients */
    s->B[0][0] = 1.16833932919;
    s->B[0][1] = 0.400250061172;
    s->B[1][0] = 1.66492368687;
    s->B[1][1] = 0.850653444434;
#else
    /* Load numerator coefficients */
    s->A[0][0] = 0.775841885724;
    s->A[0][1] = 1.54552788762;
    s->A[0][2] = 0.775841885724;
    s->A[1][0] = 0.775841885724;
    s->A[1][1] = 1.51915539326;
    s->A[1][2] = 0.775841885724;

    /* Load denominator coefficients */
    s->B[0][0] = 1.23307153957;
    s->B[0][1] = 0.430807372835;
    s->B[1][0] = 1.71128410940;
    s->B[1][1] = 0.859087959597;
#endif

    /* Initialization of the input DC-removal filter */
    s->last_xk = s->last_yk = 0;
  }

  /*
   *    ..... REAL MNRU WORK .....
   */

  /* Initialize memory */
  memset (s->vet, '\0', n * sizeof (double));

  for (count = 0; count < n; count++) {
    /* Copy sample to local variable */
    inp_smp = *input++;

#ifndef NO_DC_REMOVAL
    /* Remove DC from input sample: H(z)= (1-Z-1)/(1-a.Z-1) */
    tmp = inp_smp - s->last_xk;
    tmp += ALPHA * s->last_yk;

    /* Update for next time */
    s->last_xk = inp_smp;
    s->last_yk = tmp;

    /* Overwrite DC-removed version of the input signal */
    inp_smp = tmp;
#endif

    /* Random number generation */
    if (mode == SIGNAL_ONLY)
      noise = 0;
    else {
      noise = (double) random_MNRU (&s->rnd_mode, &s->rnd_state, s->seed, fseed);
      noise *= s->noise_gain * inp_smp; /* noise modulated by input sample */
      if (noise > 1.00 || noise < -1.00)
        s->clip++;              /* clip counter */
    }

    /* Addition of signal and modulated noise */
    out_tmp = noise + inp_smp * s->signal_gain;

#ifdef NO_OUT_FILTER
    out_flt = out_tmp;
#else
    /* Filter output sample by each stage of the low-pass IIR filter */
    for (i = 0; i < MNRU_STAGE_OUT_FLT; i++) {
      out_flt = out_tmp * s->A[i][0] + s->DLY[i][1];
      s->DLY[i][1] = out_tmp * s->A[i][1] - out_flt * s->B[i][0] + s->DLY[i][0];
      s->DLY[i][0] = out_tmp * s->A[i][2] - out_flt * s->B[i][1];

      out_tmp = out_flt;        /* output becomes input for next stage */
    }
#endif

    /* Copy noise-modulated speech sample to output vector */
    *output++ = out_flt;
  }

  /* Check if is end of operation THEN release memory buffer */
  if (operation == MNRU_STOP) {
    free (s->rnd_state.gauss);
    free (s->vet);
    s->vet = (double *) DNULL;
  }

  /* Return address of vet: if NULL, nothing is allocated */
  return ((double *) s->vet);
}
/*  .................... End of MNRU_process() ....................... */


/**
*   double *P50_MNRU_process (char operation, MNRU_state *s, double *input, double *output,
*        long n, long seed, char mode, double Q)
*
*   Module for addition of modulated P.50 shaped noise to a vector of `n' samples, according to Recommendation
*   ITU-T P.810 (2023).
*   Depending on the `mode', it:
*
*       - adds modulated noise to the `input' buffer at a SNR level of `Q' dB, saving to `output' buffer
*         (mode==MOD_NOISE);
*
*       - puts into `output' the noise only, without adding to the original signal (mode==NOISE_ONLY);
*
*       - copies to `output' the `input' samples (mode==SIGNAL_ONLY);
*
*       There is the need of state variables, which are declared in mnru.h. These are reset calling the function
*       with the argument `operation' set as MNRU_START. In the last call of the function, call it with
*       operation=MNRU_STOP, to release the memory allocated for the processing. Normal operation is followed when
*       operation is set as MNRU_CONTINUE.
*
*       IMPORTANT NOTES:
*       - The DC Removal filter may alter the timber perception of the input signal. It is therefore recommended not
*         to use the filter provided in this function.
*       - New values of `seed', `mode' and `Q' are considered only when operation==MNRU_START, because they are
*         considered as INITIAL state values.
*
*       For more details on the algorithm, see the related documentation.
*
*
*   @param  operation   MNRU_START, MNRU_CONTINUE, MNRU_STOP (see description above; defined in mnru.h)
*   @param  s           pointer to a structure defined as MNRU_state, as in mnru.h
*   @param  input       pointer to input double-data vector; must represent 48 kHz speech samples.
*   @param  output	    pointer to output double-data vector; will represent 48 kHz speech samples.
*   @param  n           long with the number of samples (double) in input
*   @param  seed        initial value for random number generator
*   @param  mode        operation mode: MOD_NOISE, SIGNAL_ONLY, NOISE_ONLY (see description above; defined in mnru.h)
*   @param  Q           double defining the desired value for the signal-to-modulated-noise for the output data.
*   @param  dcRemoval   0 for disabling DC Removal (recommended - see description of the algorithm),
*                       1 for enabling the DC removal filter (for backward compatibility with P.50 MNNU prior 2023).
*
*   @return (double *)  pointer to the noise vector if reset was OK and/or is in "run" (MNRU_CONTINUE) operation.
*                       NULL if uninitialized or if initialization failed.
**/
double *P50_MNRU_process(char operation, MNRU_state *s, double* input, double* output,
                         long n, long seed, char mode, double Q, char dcRemoval)
{
  long            count;
  double          tmp;

  //Variables needed for filtering functions
  static double					*delayLine_FIR, *delayLine_IIR;
  static double					*filteredNoiseTemp;

  /*
  *    ..... RESET PORTION .....
  */

  /* Check if is START of operation: reset state and allocate memory buffer */
  if (operation == MNRU_START)
  {
    /* Reset clip counter */
    s->clip = 0;

    /* Allocate memory for sample's buffer */
    if ((s->vet = (double *) calloc(n, sizeof(double))) == NULL)
      return (NULL);
    if ((filteredNoiseTemp = (double *) calloc(n, sizeof(double))) == NULL)
      return (NULL);

    /* Seed for random number generation */
    s->seed = seed;

    /* Gain for signal path */
    if (mode == MOD_NOISE)
      s->signal_gain = 1.000;
    else if (mode == SIGNAL_ONLY)
      s->signal_gain = 1.000;
    else    /* (mode == NOISE_ONLY) */
      s->signal_gain = 0.000;

    /* Gain for noise path */
    if (mode == MOD_NOISE || mode == NOISE_ONLY)
      s->noise_gain = P50_NOISE_GAIN * pow(10.0, (-0.05 * Q));
    else			/* (mode == SIGNAL_ONLY) */
      s->noise_gain = 0;

    /* Flag for random sequence initialization */
    s->rnd_mode = RANDOM_RESET;

    /* Initialization of the output low-pass filter */
    /* Cleanup memory */
    memset(s->DLY, '\0', sizeof(s->DLY));

    //	 Init filter delay lines and state variables
	 if ((delayLine_FIR = (double *)calloc(iP50FIRcoeffsLen, sizeof(double))) == NULL)
		 return NULL;
	 if ((delayLine_IIR = (double *)calloc(iP50IIRorder,     sizeof(double))) == NULL)
		 return NULL;

    /* Initialization of the input DC-removal filter */
    s->last_xk = s->last_yk = 0;
  }

  /*
   *    ..... REAL MNRU WORK .....
   */

  if (operation != MNRU_STOP)
  {
	  //skip everything if mode == SIGNAL_ONLY
	  if (mode == SIGNAL_ONLY)
	  {
		  for (count = 0; count < n; count++)
			  output[count] = input[count];
		  return s->vet;
	  }

	  /* Initialize memory and upsample by a factor of 5 */
	  memset(s->vet, '\0', n * sizeof(double));
	  memset(filteredNoiseTemp, 0, n * sizeof(double));

	  //Fill noise array
	  for (count = 0; count < n; count++)
	  {
		 /* Random number generation */
		 if (mode == SIGNAL_ONLY)
			s->vet[count] = 0;
		 else
		 {
			s->vet[count] = (double) random_MNRU(&s->rnd_mode, &s->rnd_state, s->seed);
		 }
	  }

	  for (count = 0; count < n; count++)
		  s->vet[count] *= s->noise_gain;

	  /* Filter the noise according to P.50, two cascaded filters for P.50 filter:
	  * An IIR highpass filter, followed by a FIR lowpass filter.
      * First, filter the data in s->vet using an IIR filter, and store the result in filteredNoiseTemp */
	  filterFunc_IIR(s->vet, filteredNoiseTemp, n, dP50IIRcoeffs, iP50IIRorder, delayLine_IIR);

	  //Second, filter the data in filteredNoiseTemp using an FIR filter and store the result in s->vet
	  filterFunc_FIR(filteredNoiseTemp, s->vet, n, dP50FIRcoeffs, iP50FIRcoeffsLen, delayLine_FIR);

    if (dcRemoval == 1) {
          for (count = 0; count < n; count++)
          {
             /* Remove DC from input sample: H(z)= (1-Z-1)/(1-a.Z-1) */
             tmp = input[count] - s->last_xk;
             tmp += ALPHA * s->last_yk;

             /* Update for next time */
             s->last_xk = input[count];
             s->last_yk = tmp;

             /* Overwrite DC-removed version of the input signal */
             input[count] = tmp;
          }
     }

	 //Add the modulated noise to the signal
	 for (count = 0; count < n; count++)
		 output[count] = input[count] * (s->signal_gain + s->vet[count]);

  }
  else //operation == MNRU_STOP
  {
	 if (s->rnd_state.gauss != NULL)	free(s->rnd_state.gauss);
	 if (s->vet != NULL)				free(s->vet);
	 s->rnd_state.gauss = NULL;
	 s->vet = NULL;

	 if (filteredNoiseTemp)	free(filteredNoiseTemp);
	 filteredNoiseTemp = NULL;

	 //Release filter delay lines and state variables
	 if (delayLine_FIR)	free(delayLine_FIR);
	 if (delayLine_IIR)	free(delayLine_IIR);
	 delayLine_FIR = delayLine_IIR = NULL;
  }

  /* Return address of vet: if NULL, nothing is allocated */
  return ((double *) s->vet);
}

/*  .................... End of P50_MNRU_process() ....................... */

#undef NOISE_GAIN
#undef DNULL
#undef ALPHA
