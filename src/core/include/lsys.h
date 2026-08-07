#include <string.h>
#include "include/defs.h"
#include "src/core/include/debug.h"

#define LSYS_MAX_VOCAB 256
#define LSYS_MAX_AXM 32
#define LSYS_MAX_PRODS 32
#define LSYS_MAX_SUCC 64

typedef struct
{
  char pred;
  char succ[LSYS_MAX_SUCC];
} lsysprod;

typedef struct lsys
{
  char     axm[LSYS_MAX_AXM];
  lsysprod P[LSYS_MAX_PRODS];
  u32      nP;
  char     V[LSYS_MAX_VOCAB];
} lsys;

// how many bytes of buffer you'd need to store G at step n
static inline isize lsysqry_bufsz(const lsys G, u32 n)
{
  isize res= -1;

  IF(G.nP > 32, goto exit;)

  // write the rules and vocab into a table
  const char *P[256]= {0};
  for(usize i= 0; i < G.nP; i++) P[(unsigned char)G.P[i].pred]= G.P[i].succ;

  // inverse mapping
  usize vidx[256]= {0};
  usize vlen     = strlen(G.V);
  for(usize i= 0; i < vlen; i++) vidx[(unsigned char)G.V[i]]= i;

  isize len[LSYS_MAX_VOCAB]     = {0};
  isize prev_len[LSYS_MAX_VOCAB]= {0};
  for(usize i= 0; i < vlen; i++) len[i]+= 1;
  for(usize i= 0; i < n; i++)
  {
    // preserve a copy of the previous lenghts
    memcpy(prev_len, len, sizeof len);
    for(usize j= 0; j < vlen; j++)
    {
      const char *succ= P[(unsigned char)G.V[j]];
      if(succ == NULL) continue;

      isize acc= 0;
      while(*succ)
        IF(__builtin_add_overflow(
             acc, prev_len[vidx[(unsigned char)(*succ++)]], &acc),
           goto exit;)

      len[vidx[(unsigned char)G.V[j]]]= acc;
    }
  }

  isize       acc= 0;
  const char *axm= G.axm;
  while(*axm)
    IF(__builtin_add_overflow(acc, len[vidx[(unsigned char)*axm++]], &acc),
       goto exit;)

  res= acc + 1;  // +1 for null-terminator
exit:
  return res;
}

// assumes no empty successors
int replaceaxm(
  const lsysprod prods[LSYS_MAX_PRODS],
  u32            nprods,
  const char    *axm,
  char          *buf,
  usize          bufcap)
{
  int rc= -1;

  NNUL(buf, goto exit;)
  NNUL(bufcap, goto exit;)

  memset(buf, 0, bufcap);

  // write the rules into a table
  const char *P[256]= {0};
  for(usize i= 0; i < nprods; i++)
    P[(unsigned char)prods[i].pred]= prods[i].succ;

  u32 axmlen= strlen(axm);
  memcpy(buf, axm, axmlen);  // TODO this needs a length check

  char *cursor= buf;
  char *end   = buf + axmlen;
  for(usize i= 0; i < axmlen; i++)
  {
    // expand each char in the axiom
    char        pred   = axm[i];
    const char *succ   = NULL;
    u32         succlen= 0;

    if(P[(unsigned char)pred] == NULL)  // no rule exists for the char
    {
      succ   = &pred;
      succlen= 1;
    }
    else
    {
      succ   = P[(unsigned char)pred];
      succlen= strlen(succ);
    }

    IF(buf + bufcap <= succlen + cursor, {
      rc= 1;
      goto exit;
    })

    if(succlen > 1)
    {
      u32   shift= succlen - 1;  // magic -1
      char *redge= end;
      while(redge > cursor) *(redge + shift)= *redge, redge--;

      memcpy(cursor, succ, succlen);
    }
    else
      *cursor= *succ;

    cursor+= succlen;
    end+= succlen - 1;
  }
  rc= 0;
exit:
  return rc;
}

static inline int lsysrep(
  const lsys G, u32 n, char *buf, usize bufcap, char *scratch, usize scap)
{
  int rc= -1;

  NNUL(buf, goto exit;)
  NNUL(bufcap, goto exit;)
  NNUL(scratch, goto exit;)
  NNUL(scap, goto exit;)

  memset(scratch, 0, scap);
  // TODO valiadte lsystem

  memcpy(scratch, G.axm, strlen(G.axm));  // TODO check capacity
  for(usize i= 0; i < n; i++)
  {
    rc= replaceaxm(G.P, G.nP, scratch, buf, bufcap);
    OK(rc, goto exit;)
    usize len= strlen(buf);
    IF(scap <= len, goto exit;)

    memcpy(scratch, buf, len);
    scratch[len]= '\0';
  }

  // TODO handle n == 0

  rc= 0;
exit:
  return rc;
}
