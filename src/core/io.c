#include "include/mem.h"
#include "src/core/include/debug.h"
#include "src/core/include/io.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// parse a binary P6 PPM header from buf[0..n). writes the image dims and the
// byte offset where pixel data begins. P6 with maxval 255 only; no comments.
static int parse_ppmhdr(
  const u8 *buf, usize n, unsigned *w, unsigned *h, usize *hdrlen)
{
  usize     i     = 2;
  unsigned  maxval= 0;
  unsigned *dst[3]= {w, h, &maxval};

  IF(n < 3, return -1;)
  IF(buf[0] != 'P' || buf[1] != '6', return -1;)

  // three whitespace-separated unsigned fields: width, height, maxval
  for(int f= 0; f < 3; f++)
  {
    while(i < n && isspace(buf[i])) i++;
    IF(i >= n || !isdigit(buf[i]), return -1;)
    unsigned v= 0;
    while(i < n && isdigit(buf[i])) v= v * 10 + (unsigned)(buf[i++] - '0');
    *dst[f]= v;
  }

  IF(maxval != 255, return -1;)
  // exactly one whitespace char separates the header from the pixel data
  IF(i >= n || !isspace(buf[i]), return -1;)
  *hdrlen= i + 1;
  return 0;
}

int fw_ppm(const char *path, struct raster_3u8 img)
{
  int  rc= -1, fd= -1;
  char hdr[64];
  int  hdrlen= 0;

  NNUL(path, goto exit;)
  NNUL(img.buf, goto exit;)
  IF(img.pxw == 0 || img.pxh == 0, goto exit;)

  fd= open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  OK(fd, goto exit;)

  hdrlen= snprintf(hdr, sizeof hdr, "P6\n%u %u\n255\n", img.pxw, img.pxh);
  IF(hdrlen < 0 || (usize)hdrlen >= sizeof hdr, goto exit;)
  IF(write(fd, hdr, hdrlen) != hdrlen, goto exit;)

  const byte *src = (const byte *)img.buf;
  usize       rowb= (usize)img.pxw * 3;
  if(img.stride == img.pxw)
  {
    usize nbytes= rowb * img.pxh;
    IF((usize)write(fd, src, nbytes) != nbytes, goto exit;)
  }
  else
    for(unsigned y= 0; y < img.pxh; y++)
      IF((usize)write(fd, src + (usize)y * img.stride * 3, rowb) != rowb,
         goto exit;)

  rc= 0;
exit:
  if(fd >= 0) close(fd);
  return rc;
}

// reads path into caller-allocated *out, truncating to out's pxw/pxh when the
// file image is larger, partially filling it when smaller. follows fr_b: the
// caller owns out->buf.
int fr_ppm(const char *path, struct raster_3u8 *out)
{
  int      rc= -1, fd= -1;
  u8       hdr[128];
  unsigned wf= 0, hf= 0;
  usize    hdrlen= 0;
  isize    got   = 0;

  NNUL(path, goto exit;)
  NNUL(out, goto exit;)
  NNUL(out->buf, goto exit;)

  fd= open(path, O_RDONLY);
  OK(fd, goto exit;)

  got= read(fd, hdr, sizeof hdr);
  OK(got, goto exit;)
  OK(parse_ppmhdr(hdr, (usize)got, &wf, &hf, &hdrlen), goto exit;)

  // rewind past the header we may have over-read into hdr
  OK(lseek(fd, (off_t)hdrlen, SEEK_SET), goto exit;)

  byte *dst= (byte *)out->buf;

  // fast path: destination matches the file exactly and is contiguous, so the
  // whole binary block lands in one read.
  if(out->stride == out->pxw && out->pxw == wf && out->pxh == hf)
  {
    usize nbytes= (usize)wf * hf * 3;
    IF((usize)read(fd, dst, nbytes) != nbytes, goto exit;)
    rc= 0;
    goto exit;
  }

  // general path: copy min(file, dest) rows/cols, honoring dest stride and
  // skipping the rest of any wider source row.
  unsigned rows= hf < out->pxh ? hf : out->pxh;
  unsigned cols= wf < out->pxw ? wf : out->pxw;
  usize    rowb= (usize)cols * 3;
  usize    skip= (usize)(wf - cols) * 3;
  for(unsigned y= 0; y < rows; y++)
  {
    IF((usize)read(fd, dst + (usize)y * out->stride * 3, rowb) != rowb,
       goto exit;)
    if(skip)
    {
      OK(lseek(fd, (off_t)skip, SEEK_CUR), goto exit;)
    }
  }

  rc= 0;
exit:
  if(fd >= 0) close(fd);
  return rc;
}

int fqry_ppmdim(const char *path, unsigned *w, unsigned *h)
{
  int   rc= -1, fd= -1;
  u8    hdr[128];
  usize hdrlen= 0;
  isize got   = 0;

  NNUL(path, goto exit;)
  NNUL(w, goto exit;)
  NNUL(h, goto exit;)

  fd= open(path, O_RDONLY);
  OK(fd, goto exit;)

  got= read(fd, hdr, sizeof hdr);
  OK(got, goto exit;)
  OK(parse_ppmhdr(hdr, (usize)got, w, h, &hdrlen), goto exit;)

  rc= 0;
exit:
  if(fd >= 0) close(fd);
  return rc;
}


isize fqry_sz(const char *path)
{
  isize sz= -1;
  int   fd= -1;
  NNUL(path, goto exit;)

  fd= open(path, O_RDONLY);
  OK(fd, goto exit;)

  struct stat st;
  OK(fstat(fd, &st), goto exit;)

  sz= st.st_size;
exit:
  if(fd >= 0) close(fd);
  return sz;
}
int fr_b(const char *path, u8 *buf, usize cap)
{
  int rc= -1;
  int fd= -1;

  NNUL(path, goto exit;)
  NNUL(buf, goto exit;)
  NNUL(cap, goto exit;)

  fd= open(path, O_RDONLY);
  OK(fd, goto exit;)

  struct stat st;
  OK(fstat(fd, &st), goto exit;)

  IF(st.st_size > cap, goto exit;)

  isize readb= read(fd, buf, st.st_size);
  IF(readb != st.st_size, goto exit;)

  rc= 0;
exit:
  if(fd >= 0) close(fd);
  return rc;
}

int fw_spvh(const char *rpath, const char *wpath)
{
  int   rc= -1, fd= -1;
  u8   *buf = NULL;
  u32  *out = NULL;
  char *sbuf= NULL;

  NNUL(rpath, goto exit;)
  NNUL(wpath, goto exit;)

  isize bytes= fqry_sz(rpath);
  OK(bytes, goto exit;)

  buf= cdralloc(NULL, bytes);
  NNUL(buf, goto exit;)

  OK(fr_b(rpath, (u8 *)buf, bytes), goto exit;)

  out= cdralloc(NULL, bytes);
  NNUL(out, goto exit;)

  IF(bytes % 4 != 0, goto exit;)

  for(usize i= 0; i < bytes; i+= 4)
    out[i / 4]=
      (u32)(buf[i + 0] | buf[i + 1] << 8 | buf[i + 2] << 16 | buf[i + 3] << 24);

  // derive variable name
  char varname[256];
  {
    const char *p    = rpath;
    u32         count= 0;
    while(*p && count < 255)
    {
      char towrite= '\0';
      if(*p == '.' || *p == '/')
        towrite= '_';
      else
        towrite= *p;
      varname[count]= towrite;
      p++, count++;
    }
    varname[count]= '\0';
  }
  // write string buffer
  {
    u32 words= bytes / 4;  // nr of u32 values
    // each word is at most "0x01234567, ", 12 chars. 256
    // extra chars for header/footer boilerplace
    u32 needed= words * 12 + 256;
    sbuf      = cdralloc(NULL, needed);
    NNUL(sbuf, goto exit;)

    usize cursor= 0;
    cursor+= sprintf(
      sbuf, "#include \"include/defs.h\"\nstatic const u32 %s[]=\n{", varname);
    for(usize i= 0; i < words; i++)
    {
      IF(cursor >= needed, goto exit;)
      cursor+= sprintf(sbuf + cursor, "0x%08x, ", out[i]);
      if((i + 1) % 6 == 0) sbuf[cursor++]= '\n';  // newline every 6 words
    }
    cursor+= sprintf(
      sbuf + cursor,
      "};\n\nstatic const usize len_%s= sizeof(%s);",
      varname,
      varname);
    IF(cursor >= needed, goto exit;)
  }

  fd= open(wpath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  OK(fd, goto exit;)
  usize towrite= strlen(sbuf);
  isize written= write(fd, sbuf, towrite);
  OK(written, goto exit;)
  IF(written != towrite, goto exit;)

  rc= 0;
exit:
  if(buf) cdrfree(NULL, buf);
  if(out) cdrfree(NULL, out);
  if(sbuf) cdrfree(NULL, sbuf);
  if(fd >= 0) close(fd);
  return rc;
}
