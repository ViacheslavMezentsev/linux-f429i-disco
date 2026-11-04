#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
void *getheap(unsigned);
int sysopen(const char*,int,int);
int _open(const char *p,int f,int mode){
 return sysopen(p,(f&3)|((f&O_CREAT)?64:0)|((f&O_EXCL)?128:0)|((f&O_TRUNC)?512:0),mode);
}
void *_sbrk(ptrdiff_t n){
 static char *heap; static int used;
 if(!heap){void *p=getheap(65536); if((uintptr_t)p >= (uintptr_t)-4095) return (void*)-1; heap=p;}
 if(n<0 || n>65536-used){errno=ENOMEM; return (void*)-1;}
 void *p=heap+used; used+=n; return p;
}
int _isatty(int fd){return fd<=2;}
int _fstat(int fd,struct stat *s){s->st_mode=S_IFCHR;return 0;}
int _gettimeofday(void *p,void *z){return -1;}
char *getcwd(char *buf,size_t n){return 0;}
