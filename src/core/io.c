#include "src/core/include/io.h"
#include "include/mem.h"
#include "src/core/include/debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int prnbppm(const t3u8 *px, unsigned w, unsigned h, byte *img, unsigned imgcap)
{
  const int hdr= snprintf((char *)img, imgcap, "P6\n%u %u\n255\n", w, h);
  if(hdr < 0) return -1;
  if(hdr >= imgcap) return -2;

  const unsigned binsize= 3 * w * h;
  if(hdr + binsize > imgcap) return -2;

  memcpy(img + hdr, px, binsize);
  return binsize + hdr;
}

int fwritebppm(FILE *f, const t3u8 *px, unsigned w, unsigned h)
{
  int hdr= fprintf(f, "P6\n%u %u\n255\n", w, h);
  if(hdr < 0) return -1;
  size_t written= fwrite(px, sizeof *px, w * h, f);

  if(written != w * h) return -2;
  return 0;
}

// TODO these functions kinda suck, use raw Unix
int fprsbppmhdr(FILE *f, unsigned *w, unsigned *h)
{
  if(w == NULL || h == NULL) return -1;  // null data
  unsigned maxval;
  int      status= fscanf(f, "P6 %u %u %u", w, h, &maxval);
  if(status != 3 || status == EOF) return -2;  // parse error
  if(maxval != 255) return -3;                 // bad file
  if(fgetc(f) == EOF) return -3;
  return 0;
}

// only call this function after parsing the header
int freadbppm(FILE *f, struct raster_3u8 data)
{
  if(data.buf == NULL || data.pxw * data.pxh == 0) return -1;  // invalid data
  if(data.fmt != FMT_RGB) return -2;                           // bad fmt

  if(data.pxw == data.stride)
  {
    unsigned npx= fread(data.buf, 3, data.pxh * data.pxw, f);
    if(npx != data.pxh * data.pxw) return -3;  // parse error
  }
  else
    for(unsigned i= 0; i < data.pxh; i++)
    {
      unsigned npx= fread(data.buf + data.stride * i, 3, data.pxw, f);
      if(npx != data.pxw) return -3;
    }

  return 0;
}

int femitspv(const char *rpath, const char *wpath)
{
  int   rc= -1, fd= -1;
  u8   *buf = NULL;
  u32  *out = NULL;
  char *sbuf= NULL;

  NNUL(rpath, goto exit;)
  NNUL(wpath, goto exit;)

  isize bytes= fsize(rpath);
  OK(bytes, goto exit;)

  buf= cdralloc(NULL, bytes);
  NNUL(buf, goto exit;)

  OK(fread_b(rpath, (u8 *)buf, bytes), goto exit;)

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
    // each word is at most "0x01234567, ", 12 chars 256
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
			if((i + 1) % 6 == 0)
				sbuf[cursor++]= '\n'; // newline every 6 words
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

isize fsize(const char *path)
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
int fread_b(const char *path, u8 *buf, usize cap)
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
