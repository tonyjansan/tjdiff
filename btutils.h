#ifndef _UTILS_H_
#define _UTILS_H_

#define MIN(x,y) (((x)<(y)) ? (x) : (y))
#define getfilesize(fp) ({ long ret; fseek(fp, 0L, SEEK_END); ret = ftell(fp); fseek(fp, 0L, SEEK_SET); ret; })

#ifdef CROSS_COMPILE
#define print_log(x)
#define print_log_ex(x, y)
#else
#define print_log(x) printf(x)
#define print_log_ex(x, y) printf(x, y)
#endif // CROSS_COMPILE

int  btdiff(const char* srcfile, const char* dstfile, const char* difffile);
int btpatch(const char* srcfile, const char* dstfile, const char* difffile);

#endif  // _UTILS_H_