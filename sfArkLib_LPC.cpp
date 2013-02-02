// sfArkLib LPC
// copyright 1998-2000, Andy Inman
// Contact via: http://netgenius.co.uk or http://melodymachine.com

#include "wcc.h"
#include "stdio.h"
#include "math.h"

void LPCinit();

#define REFINT      1       // Integers for Reflection Coefficients (faster)
#define LPCWIN      4096

#define PMAX        128              // Max allowed nc
#define ZWINMIN     128              // 128 or 256 best.  Smaller - slightly slower
#define ZWINMAX     ZWINMIN

#if (LPCWIN / ZWINMIN) > 32          // Check use of 32bit *Flags
  #error Invalid LPCWIN / ZWINMIN
#endif

// The following parameters determine the history sized used for LPC analysis (little impact on speed)
// The history is the amount of prev
#define HISTSIZE    (4*128/ZWINMIN)		// Multiple of number of ZWINs to use as history size (seems best value)

typedef float  LPC_FLOAT;

typedef LAWORD  LPC_WORD;                   // LPC_WORD must have (slightly) greater range than AWORD

#define ISCALE_BITS     14                  // Fixed scale seems to work fine in practice
#define ISCALE          (1 << ISCALE_BITS)

#if REFINT == 1
  typedef long LPC_PRAM;
#else
  typedef LPC_FLOAT   LPC_PRAM;
#endif

typedef LPC_FLOAT  LPC_CORR;
typedef LPC_FLOAT  LPC_CORR2;    // Holds LPC_CORR*LPC_CORR


// ======================================================================
LPC_CORR schur(             // returns the minimum mean square error
    LPC_CORR const * ac,    //  in: [0...p] autocorrelation values
    int nc,                 //  in: number of ref. coeff
    LPC_PRAM        * ref)  // out: [0...p-1] reflection coefficients
{
    int i, m;

    LPC_CORR2 error, r;

    LPC_CORR2 Gen0[PMAX], Gen1[PMAX];

    if (ac[0] == 0)
    {
      for (i = 0; i < nc; i++) ref[i] = 0;
      return 0;
    }

    // Initialize the rows of the generator matrix G to ac[1...p].
    error = ac[0];

    for (i = 0; i < nc; i++)
      Gen0[i] = Gen1[i] = ac[i + 1];

    for (i = 0;;)
    {
      // Calculate this iteration's reflection coefficient and error.
      r = -Gen1[0] / error;
      error += Gen1[0] * r;

      #if REFINT == 1
        ref[i] = (LPC_PRAM)(r * ISCALE);    // Scale-up to avoid loss of precision
      #else
        ref[i] = r;
      #endif

      if (++i >= nc)   break;

      // Update the generator matrix.
      for (m = 0; m < nc - i; m++)
      {
        Gen1[m] = Gen1[m + 1]  +  r * Gen0[m];
        Gen0[m] = Gen0[m]      +  r * Gen1[m + 1];
      }
    }
    return error;
}

// ======================================================================
// Compute the autocorrelation
void autocorrelation(int n, LPC_WORD const *ibuf, int nc, LPC_CORR *ac)
{
      int i;

      // Copy integer data to float -- speeds things up slightly...
      LPC_FLOAT buf[ZWINMAX];
      for (i = 0; i < n ; i++)  buf[i] = (LPC_FLOAT) ibuf[i];

      while (nc--)
      {
        LPC_CORR c = 0;
        LPC_FLOAT const *lbuf = buf + nc;          // Points to current sample + nc

        #define CI(I)  (buf[I] * lbuf[I])
        int istop = n - nc - 15;                   // Process 16 steps at a time for speed...
        for (i = 0;  i < istop; i += 16)
          c += CI(i+0) + CI(i+1) + CI(i+2) + CI(i+3) + CI(i+4) + CI(i+5) + CI(i+6) + CI(i+7)
             + CI(i+8) + CI(i+9) + CI(i+10)+ CI(i+11)+ CI(i+12)+ CI(i+13)+ CI(i+14)+ CI(i+15);

        istop = n - nc;                    // Process any remainder, one step at a time...
        for (; i < istop; i++)
          c += CI(i);

        ac[nc] = c;
        #undef CI
      }
}


// ======================================================================
// Add the autocorrelation for the end section of previous Window / start of current window
void AddAC (LPC_WORD const *hbuf, LPC_WORD const *ibuf, int nc, LPC_CORR *ac)
{
      int i;

      // Copy integer data to float -- speeds things up slightly...
      LPC_FLOAT buf[PMAX*2];

      int n = nc-1;                            // Number of samples is always one less than nc value

      for (i = 0; i < n ; i++)
      {
        buf[i] = (LPC_FLOAT) hbuf[i];
        buf[i + n] = (LPC_FLOAT) ibuf[i];
      }

      while (nc-- > 1)
      {
        LPC_CORR c = 0;
        LPC_FLOAT const *lbuf = buf + nc;      // Points to current sample + nc

        int istop;

        #define CI(I)  (buf[I] * lbuf[I])

        istop = n - 15;                     // Process 16 steps at a time for speed...
        i = n - nc;

        for (;  i < istop; i += 16)
          c += CI(i+0) + CI(i+1) + CI(i+2) + CI(i+3) + CI(i+4) + CI(i+5) + CI(i+6) + CI(i+7)
             + CI(i+8) + CI(i+9) + CI(i+10)+ CI(i+11)+ CI(i+12)+ CI(i+13)+ CI(i+14)+ CI(i+15);

        istop = n;                    // Process any remainder, one step at a time...
        for (; i < istop; i++)
          c += CI(i);

        ac[nc] += c;
        #undef CI
      }
}

// ======================================================================
static void LPCdecode(
    LPC_PRAM const *ref,    //  in: [0...p-1] reflection coefficients
    int            nc,      //  in: number of coefficients
    int            n,       //      # of samples                        
    LPC_WORD const *in,     //      [0...n-1] residual input            
    LPC_WORD       *out)    // out: [0...n-1] short-term signal         

{
    LPC_WORD s;
    static LPC_WORD u[PMAX+1];      // NB: Intermediate values here can be out of range of AWORD
    int i;

    if (in == LAW_NULL)     // Initialise?
    {
      for (i = 0; i < PMAX; i++)  u[i] = 0;
      return;
    }

    while (n--)
    {
        s = *in++;
        for (i = nc; i--;)
        {
          #if REFINT == 1
            #if 1   // Use SDIV
              long m;
              m = ref[i] * u[i];
              s      = s    - SDIV(m, ISCALE_BITS);
              m = ref[i] * s;
              u[i+1] = u[i] + SDIV(m, ISCALE_BITS);
            #else
              s      = s    - ref[i] * u[i] / ISCALE;
              u[i+1] = u[i] + (ref[i] * s / ISCALE);
            #endif
          #else // Use floating point, convert to int...
            s      = s -    /*(int)*/ (ref[i] * u[i]);
            u[i+1] = u[i] + /*(int)*/ (ref[i] * s);
          #endif
        }
        *out++ = u[0] = s;
    }
}


// ======================================================================

// UnLPC2() is called by UnLPC() -- process one LPCWIN sized chunk

long UnLPC2(LPC_WORD *OutBuf, LPC_WORD *InBuf, short bufsize, short nc, ULONG *Flags)
{
    static LPC_WORD HistBuf[PMAX*2];

    static LPC_CORR AcHist[HISTSIZE][PMAX+1];
    static int HistNum;

    LPC_PRAM ref[PMAX];
    LPC_CORR ac[PMAX+1];

    int i, k;
    ULONG FlagMask = 1;
    int zwin = ZWINMIN;
    if (nc > zwin)  zwin = ZWINMAX;

    if (InBuf == LAW_NULL)  // Initialise?
    {
	  HistNum = 0;
      for (i = 0; i < nc; i++)  ref[i] = 0;
      for (i = 0; i < PMAX*2; i++)  HistBuf[i] = 0;
      for (i = 0; i < PMAX+1; i++)
        for (int j = 0; j < HISTSIZE; j++)
          AcHist[j][i] = 0;
      LPCdecode(LAW_NULL, nc, 0, LAW_NULL, LAW_NULL);
      return 0;
    }

    //if ((bufsize % zwin) != 0)  return -3;

    for (i = 0; i < bufsize; i += zwin)
    {
      #if HISTSIZE == 4
        for (k = 0; k < nc+1; k++)
          ac[k] = AcHist[0][k] + AcHist[1][k] + AcHist[2][k] + AcHist[3][k];
      #else
        for (k = 0; k < nc+1; k++)
        {
          ac[k] = 0;
          for (int h = 0; h < HISTSIZE; h++)
            ac[k] += AcHist[h][k];
        }
      #endif

      // Decode...
      if ((*Flags & FlagMask) == 0)
      {
        schur(ac, nc, ref);
        LPCdecode(ref, nc, zwin, InBuf+i, OutBuf+i);
      }
      else
      {
        LPCinit();                                                  // Re-initialise
        for (int j = 0; j < zwin; j++)  OutBuf[i+j] = InBuf[i+j];   // Copy input to output
      }
      FlagMask <<= 1;

      // Update the AutoCorrelation history data...
      AddAC(HistBuf, OutBuf+i, nc+1, AcHist[HistNum]);           // Process overlap of prev. & current buffer
      if (++HistNum == HISTSIZE)  HistNum = 0;                   // Increment History counter, wrap-around if needed

      autocorrelation(zwin, OutBuf+i, nc+1, AcHist[HistNum]);    // Update AcHist with current buffer
      for (k = 0; k < nc; k++)  HistBuf[k] = OutBuf[i+k];        // Store beginning of current buffer for next AddAC()
    }

    return 0;
}

// ======================================================================

void LPCinit()
{
  UnLPC2(LAW_NULL, LAW_NULL, 0, 0, (ULONG *)0);
}
// ======================================================================

long UnLPC(AWORD *OutBuf, AWORD *InBuf, short bufsize, short nc, ULONG *Flags)
{
  long      OutBits = 0;
  LPC_WORD  lInBuf[MAX_BUFSIZE], lOutBuf[MAX_BUFSIZE];
  LPC_WORD  *inp = lInBuf, *bufend = inp + bufsize, *outp = lOutBuf;

  int i;

  // Copy 16 bit data to 32 bits...
  while (inp < bufend)	*inp++ = *InBuf++;

  inp = lInBuf;

  short WinSize = LPCWIN;
  if (WinSize > bufsize)  WinSize = bufsize;

  int WordsLeft = bufsize;
  do {
    if (WordsLeft < ZWINMIN)
    {
      for (i = 0; i < WordsLeft; i++)
        outp[i] = inp[i];
    }
    else
    {
      long LPCout = UnLPC2(outp, inp, WinSize, nc, Flags);
      if (LPCout < 0)  return LPCout;
      OutBits += LPCout;
    }

    inp += WinSize;
    outp += WinSize;
    WordsLeft -= WinSize;

  } while (inp < bufend);

  // Copy 32 bit data to 16 bits...
  outp = lOutBuf; bufend = outp + bufsize;
  while (outp < bufend)
    *OutBuf++ = (AWORD) *outp++;

  return 0;
}


// ======================================================================
