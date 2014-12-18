#ifndef _UTILS_H_
#define _UTILS_H_

#define MIN(x,y) (((x)<(y)) ? (x) : (y))
#define getfilesize(fp) ({ long ret; fseek(fp, 0L, SEEK_END); ret = ftell(fp); fseek(fp, 0L, SEEK_SET); ret; })

int  diff(const char* srcfile, const char* dstfile, const char* difffile);
int patch(const char* srcfile, const char* dstfile, const char* difffile);

#endif  // _UTILS_H_