typedef void* FILE;
typedef int __builtin_va_list;
typedef __builtin_va_list __gnuc_va_list;
typedef __gnuc_va_list va_list;
typedef long int ptrdiff_t;
typedef long unsigned int size_t;
typedef int wchar_t;
typedef struct {
  long long __max_align_ll ;
  long double __max_align_ld ;
} max_align_t;
typedef unsigned char __u_char;
typedef unsigned short int __u_short;
typedef unsigned int __u_int;
typedef unsigned long int __u_long;
typedef signed char __int8_t;
typedef unsigned char __uint8_t;
typedef signed short int __int16_t;
typedef unsigned short int __uint16_t;
typedef signed int __int32_t;
typedef unsigned int __uint32_t;
typedef signed long int __int64_t;
typedef unsigned long int __uint64_t;
typedef __int8_t __int_least8_t;
typedef __uint8_t __uint_least8_t;
typedef __int16_t __int_least16_t;
typedef __uint16_t __uint_least16_t;
typedef __int32_t __int_least32_t;
typedef __uint32_t __uint_least32_t;
typedef __int64_t __int_least64_t;
typedef __uint64_t __uint_least64_t;
typedef long int __quad_t;
typedef unsigned long int __u_quad_t;
typedef long int __intmax_t;
typedef unsigned long int __uintmax_t;
typedef unsigned long int __dev_t;
typedef unsigned int __uid_t;
typedef unsigned int __gid_t;
typedef unsigned long int __ino_t;
typedef unsigned long int __ino64_t;
typedef unsigned int __mode_t;
typedef unsigned long int __nlink_t;
typedef long int __off_t;
typedef long int __off64_t;
typedef int __pid_t;
typedef struct { int __val[2]; } __fsid_t;
typedef long int __clock_t;
typedef unsigned long int __rlim_t;
typedef unsigned long int __rlim64_t;
typedef unsigned int __id_t;
typedef long int __time_t;
typedef unsigned int __useconds_t;
typedef long int __suseconds_t;
typedef long int __suseconds64_t;
typedef int __daddr_t;
typedef int __key_t;
typedef int __clockid_t;
typedef void * __timer_t;
typedef long int __blksize_t;
typedef long int __blkcnt_t;
typedef long int __blkcnt64_t;
typedef unsigned long int __fsblkcnt_t;
typedef unsigned long int __fsblkcnt64_t;
typedef unsigned long int __fsfilcnt_t;
typedef unsigned long int __fsfilcnt64_t;
typedef long int __fsword_t;
typedef long int __ssize_t;
typedef long int __syscall_slong_t;
typedef unsigned long int __syscall_ulong_t;
typedef __off64_t __loff_t;
typedef char *__caddr_t;
typedef long int __intptr_t;
typedef unsigned int __socklen_t;
typedef int __sig_atomic_t;
typedef __int8_t int8_t;
typedef __int16_t int16_t;
typedef __int32_t int32_t;
typedef __int64_t int64_t;
typedef __uint8_t uint8_t;
typedef __uint16_t uint16_t;
typedef __uint32_t uint32_t;
typedef __uint64_t uint64_t;
typedef __int_least8_t int_least8_t;
typedef __int_least16_t int_least16_t;
typedef __int_least32_t int_least32_t;
typedef __int_least64_t int_least64_t;
typedef __uint_least8_t uint_least8_t;
typedef __uint_least16_t uint_least16_t;
typedef __uint_least32_t uint_least32_t;
typedef __uint_least64_t uint_least64_t;
typedef signed char int_fast8_t;
typedef long int int_fast16_t;
typedef long int int_fast32_t;
typedef long int int_fast64_t;
typedef unsigned char uint_fast8_t;
typedef unsigned long int uint_fast16_t;
typedef unsigned long int uint_fast32_t;
typedef unsigned long int uint_fast64_t;
typedef long int intptr_t;
typedef unsigned long int uintptr_t;
typedef __intmax_t intmax_t;
typedef __uintmax_t uintmax_t;
typedef struct
  {
    int quot;
    int rem;
  } div_t;
typedef struct
  {
    long int quot;
    long int rem;
  } ldiv_t;
 typedef struct
  {
    long long int quot;
    long long int rem;
  } lldiv_t;
extern size_t __ctype_get_mb_cur_max (void) ;
extern double atof (const char *__nptr)
     ;
extern int atoi (const char *__nptr)
     ;
extern long int atol (const char *__nptr)
     ;
 extern long long int atoll (const char *__nptr)
     ;
extern double strtod (const char * __nptr,
        char ** __endptr)
     ;
extern float strtof (const char * __nptr,
       char ** __endptr) ;
extern long double strtold (const char * __nptr,
       char ** __endptr)
     ;
extern long int strtol (const char * __nptr,
   char ** __endptr, int __base)
     ;
extern unsigned long int strtoul (const char * __nptr,
      char ** __endptr, int __base)
     ;
extern long long int strtoq (const char * __nptr,
        char ** __endptr, int __base)
     ;
extern unsigned long long int strtouq (const char * __nptr,
           char ** __endptr, int __base)
     ;
extern long long int strtoll (const char * __nptr,
         char ** __endptr, int __base)
     ;
extern unsigned long long int strtoull (const char * __nptr,
     char ** __endptr, int __base)
     ;
extern char *l64a (long int __n) ;
extern long int a64l (const char *__s)
     ;
typedef __u_char u_char;
typedef __u_short u_short;
typedef __u_int u_int;
typedef __u_long u_long;
typedef __quad_t quad_t;
typedef __u_quad_t u_quad_t;
typedef __fsid_t fsid_t;
typedef __loff_t loff_t;
typedef __ino_t ino_t;
typedef __dev_t dev_t;
typedef __gid_t gid_t;
typedef __mode_t mode_t;
typedef __nlink_t nlink_t;
typedef __uid_t uid_t;
typedef __off_t off_t;
typedef __pid_t pid_t;
typedef __id_t id_t;
typedef __ssize_t ssize_t;
typedef __daddr_t daddr_t;
typedef __caddr_t caddr_t;
typedef __key_t key_t;
typedef __clock_t clock_t;
typedef __clockid_t clockid_t;
typedef __time_t time_t;
typedef __timer_t timer_t;
typedef unsigned long int ulong;
typedef unsigned short int ushort;
typedef unsigned int uint;
typedef __uint8_t u_int8_t;
typedef __uint16_t u_int16_t;
typedef __uint32_t u_int32_t;
typedef __uint64_t u_int64_t;
typedef int register_t ;
typedef struct
{
  unsigned long int __val[16];
} __sigset_t;
typedef __sigset_t sigset_t;
struct timeval
{
  __time_t tv_sec;
  __suseconds_t tv_usec;
};
struct timespec
{
  __time_t tv_sec;
  __syscall_slong_t tv_nsec;
};
typedef __suseconds_t suseconds_t;
typedef long int __fd_mask;
typedef struct
  {
    __fd_mask __fds_bits[32];
  } fd_set;
typedef __fd_mask fd_mask;
extern int select (int __nfds, fd_set * __readfds,
     fd_set * __writefds,
     fd_set * __exceptfds,
     struct timeval * __timeout);
extern int pselect (int __nfds, fd_set * __readfds,
      fd_set * __writefds,
      fd_set * __exceptfds,
      const struct timespec * __timeout,
      const __sigset_t * __sigmask);
extern long int __fdelt_chk (long int __d);
extern long int __fdelt_warn (long int __d)
  ;
typedef __blksize_t blksize_t;
typedef __blkcnt_t blkcnt_t;
typedef __fsblkcnt_t fsblkcnt_t;
typedef __fsfilcnt_t fsfilcnt_t;
typedef union
{
  unsigned long long int __value64;
  struct
  {
    unsigned int __low;
    unsigned int __high;
  } __value32;
} __atomic_wide_counter;
typedef struct __pthread_internal_list
{
  struct __pthread_internal_list *__prev;
  struct __pthread_internal_list *__next;
} __pthread_list_t;
typedef struct __pthread_internal_slist
{
  struct __pthread_internal_slist *__next;
} __pthread_slist_t;
struct __pthread_mutex_s
{
  int __lock;
  unsigned int __count;
  int __owner;
  unsigned int __nusers;
  int __kind;
  short __spins;
  short __elision;
  __pthread_list_t __list;
};
struct __pthread_rwlock_arch_t
{
  unsigned int __readers;
  unsigned int __writers;
  unsigned int __wrphase_futex;
  unsigned int __writers_futex;
  unsigned int __pad3;
  unsigned int __pad4;
  int __cur_writer;
  int __shared;
  signed char __rwelision;
  unsigned char __pad1[7];
  unsigned long int __pad2;
  unsigned int __flags;
};
struct __pthread_cond_s
{
  __atomic_wide_counter __wseq;
  __atomic_wide_counter __g1_start;
  unsigned int __g_refs[2] ;
  unsigned int __g_size[2];
  unsigned int __g1_orig_size;
  unsigned int __wrefs;
  unsigned int __g_signals[2];
};
typedef unsigned int __tss_t;
typedef unsigned long int __thrd_t;
typedef struct
{
  int __data ;
} __once_flag;
typedef unsigned long int pthread_t;
typedef union
{
  char __size[4];
  int __align;
} pthread_mutexattr_t;
typedef union
{
  char __size[4];
  int __align;
} pthread_condattr_t;
typedef unsigned int pthread_key_t;
typedef int pthread_once_t;
union pthread_attr_t
{
  char __size[56];
  long int __align;
};
typedef union pthread_attr_t pthread_attr_t;
typedef union
{
  struct __pthread_mutex_s __data;
  char __size[40];
  long int __align;
} pthread_mutex_t;
typedef union
{
  struct __pthread_cond_s __data;
  char __size[48];
  long long int __align;
} pthread_cond_t;
typedef union
{
  struct __pthread_rwlock_arch_t __data;
  char __size[56];
  long int __align;
} pthread_rwlock_t;
typedef union
{
  char __size[8];
  long int __align;
} pthread_rwlockattr_t;
typedef volatile int pthread_spinlock_t;
typedef union
{
  char __size[32];
  long int __align;
} pthread_barrier_t;
typedef union
{
  char __size[4];
  int __align;
} pthread_barrierattr_t;
extern long int random (void) ;
extern void srandom (unsigned int __seed) ;
extern char *initstate (unsigned int __seed, char *__statebuf,
   size_t __statelen) ;
extern char *setstate (char *__statebuf) ;
struct random_data
  {
    int32_t *fptr;
    int32_t *rptr;
    int32_t *state;
    int rand_type;
    int rand_deg;
    int rand_sep;
    int32_t *end_ptr;
  };
extern int random_r (struct random_data * __buf,
       int32_t * __result) ;
extern int srandom_r (unsigned int __seed, struct random_data *__buf)
     ;
extern int initstate_r (unsigned int __seed, char * __statebuf,
   size_t __statelen,
   struct random_data * __buf)
     ;
extern int setstate_r (char * __statebuf,
         struct random_data * __buf)
     ;
extern int rand (void) ;
extern void srand (unsigned int __seed) ;
extern int rand_r (unsigned int *__seed) ;
extern double drand48 (void) ;
extern double erand48 (unsigned short int __xsubi[3]) ;
extern long int lrand48 (void) ;
extern long int nrand48 (unsigned short int __xsubi[3])
     ;
extern long int mrand48 (void) ;
extern long int jrand48 (unsigned short int __xsubi[3])
     ;
extern void srand48 (long int __seedval) ;
extern unsigned short int *seed48 (unsigned short int __seed16v[3])
     ;
extern void lcong48 (unsigned short int __param[7]) ;
struct drand48_data
  {
    unsigned short int __x[3];
    unsigned short int __old_x[3];
    unsigned short int __c;
    unsigned short int __init;
    unsigned long long int __a;
  };
extern int drand48_r (struct drand48_data * __buffer,
        double * __result) ;
extern int erand48_r (unsigned short int __xsubi[3],
        struct drand48_data * __buffer,
        double * __result) ;
extern int lrand48_r (struct drand48_data * __buffer,
        long int * __result)
     ;
extern int nrand48_r (unsigned short int __xsubi[3],
        struct drand48_data * __buffer,
        long int * __result)
     ;
extern int mrand48_r (struct drand48_data * __buffer,
        long int * __result)
     ;
extern int jrand48_r (unsigned short int __xsubi[3],
        struct drand48_data * __buffer,
        long int * __result)
     ;
extern int srand48_r (long int __seedval, struct drand48_data *__buffer)
     ;
extern int seed48_r (unsigned short int __seed16v[3],
       struct drand48_data *__buffer) ;
extern int lcong48_r (unsigned short int __param[7],
        struct drand48_data *__buffer)
     ;
extern void *malloc (size_t __size)
     ;
extern void *calloc (size_t __nmemb, size_t __size)
     ;
extern void *realloc (void *__ptr, size_t __size)
     ;
extern void free (void *__ptr) ;
extern void *reallocarray (void *__ptr, size_t __nmemb, size_t __size)
    
    
    ;
extern void *reallocarray (void *__ptr, size_t __nmemb, size_t __size)
     ;
extern void *alloca (size_t __size) ;
extern void *valloc (size_t __size)
     ;
extern int posix_memalign (void **__memptr, size_t __alignment, size_t __size)
     ;
extern void *aligned_alloc (size_t __alignment, size_t __size)
    
     ;
extern void abort (void) ;
extern int atexit (void (*__func) (void)) ;
extern int at_quick_exit (void (*__func) (void)) ;
extern int on_exit (void (*__func) (int __status, void *__arg), void *__arg)
     ;
extern void exit (int __status) ;
extern void quick_exit (int __status) ;
extern void _Exit (int __status) ;
extern char *getenv (const char *__name) ;
extern int putenv (char *__string) ;
extern int setenv (const char *__name, const char *__value, int __replace)
     ;
extern int unsetenv (const char *__name) ;
extern int clearenv (void) ;
extern char *mktemp (char *__template) ;
extern int mkstemp (char *__template) ;
extern int mkstemps (char *__template, int __suffixlen) ;
extern char *mkdtemp (char *__template) ;
extern int system (const char *__command) ;
extern char *realpath (const char * __name,
         char * __resolved) ;
typedef int (*__compar_fn_t) (const void *, const void *);
extern void *bsearch (const void *__key, const void *__base,
        size_t __nmemb, size_t __size, __compar_fn_t __compar)
     ;
extern void qsort (void *__base, size_t __nmemb, size_t __size,
     __compar_fn_t __compar) ;
extern int abs (int __x) ;
extern long int labs (long int __x) ;
 extern long long int llabs (long long int __x)
     ;
extern div_t div (int __numer, int __denom)
     ;
extern ldiv_t ldiv (long int __numer, long int __denom)
     ;
 extern lldiv_t lldiv (long long int __numer,
        long long int __denom)
     ;
extern char *ecvt (double __value, int __ndigit, int * __decpt,
     int * __sign) ;
extern char *fcvt (double __value, int __ndigit, int * __decpt,
     int * __sign) ;
extern char *gcvt (double __value, int __ndigit, char *__buf)
     ;
extern char *qecvt (long double __value, int __ndigit,
      int * __decpt, int * __sign)
     ;
extern char *qfcvt (long double __value, int __ndigit,
      int * __decpt, int * __sign)
     ;
extern char *qgcvt (long double __value, int __ndigit, char *__buf)
     ;
extern int ecvt_r (double __value, int __ndigit, int * __decpt,
     int * __sign, char * __buf,
     size_t __len) ;
extern int fcvt_r (double __value, int __ndigit, int * __decpt,
     int * __sign, char * __buf,
     size_t __len) ;
extern int qecvt_r (long double __value, int __ndigit,
      int * __decpt, int * __sign,
      char * __buf, size_t __len)
     ;
extern int qfcvt_r (long double __value, int __ndigit,
      int * __decpt, int * __sign,
      char * __buf, size_t __len)
     ;
extern int mblen (const char *__s, size_t __n) ;
extern int mbtowc (wchar_t * __pwc,
     const char * __s, size_t __n) ;
extern int wctomb (char *__s, wchar_t __wchar) ;
extern size_t mbstowcs (wchar_t * __pwcs,
   const char * __s, size_t __n)
    ;
extern size_t wcstombs (char * __s,
   const wchar_t * __pwcs, size_t __n)
    
 
  ;
extern int rpmatch (const char *__response) ;
extern int getsubopt (char ** __optionp,
        char *const * __tokens,
        char ** __valuep)
     ;
extern int getloadavg (double __loadavg[], int __nelem)
     ;
extern char *__realpath_chk (const char * __name,
        char * __resolved,
        size_t __resolvedlen) ;
extern char *__realpath_alias (const char * __name, char * __resolved)
                                                      ;
extern char *__realpath_chk_warn (const char * __name, char * __resolved, size_t __resolvedlen)
    
                                      ;
extern int __ptsname_r_chk (int __fd, char *__buf, size_t __buflen,
       size_t __nreal)
    ;
extern int __ptsname_r_alias (int __fd, char *__buf, size_t __buflen)
     ;
extern int __ptsname_r_chk_warn (int __fd, char *__buf, size_t __buflen, size_t __nreal)
    
                   ;
extern int __wctomb_chk (char *__s, wchar_t __wchar, size_t __buflen)
  ;
extern int __wctomb_alias (char *__s, wchar_t __wchar)
                   ;
extern size_t __mbstowcs_chk (wchar_t * __dst,
         const char * __src,
         size_t __len, size_t __dstlen)
    ;
extern size_t __mbstowcs_alias (wchar_t * __dst, const char * __src, size_t __len)
    ;
extern size_t __mbstowcs_chk_warn (wchar_t * __dst, const char * __src, size_t __len, size_t __dstlen)
    
                        ;
extern size_t __wcstombs_chk (char * __dst,
         const wchar_t * __src,
         size_t __len, size_t __dstlen)
  ;
extern size_t __wcstombs_alias (char * __dst, const wchar_t * __src, size_t __len)
  ;
extern size_t __wcstombs_chk_warn (char * __dst, const wchar_t * __src, size_t __len, size_t __dstlen)
     ;
typedef struct
{
  int __count;
  union
  {
    unsigned int __wch;
    char __wchb[4];
  } __value;
} __mbstate_t;
typedef struct _G_fpos_t
{
  __off_t __pos;
  __mbstate_t __state;
} __fpos_t;
typedef struct _G_fpos64_t
{
  __off64_t __pos;
  __mbstate_t __state;
} __fpos64_t;
struct _IO_FILE;
typedef struct _IO_FILE __FILE;
struct _IO_FILE;
typedef struct _IO_FILE FILE;
typedef __fpos_t fpos_t;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
extern int remove (const char *__filename) ;
extern int rename (const char *__old, const char *__new) ;
extern int renameat (int __oldfd, const char *__old, int __newfd,
       const char *__new) ;
extern int fclose (FILE *__stream);
extern FILE *tmpfile (void)
  ;
extern char *tmpnam (char[20]) ;
extern char *tmpnam_r (char __s[20]) ;
extern char *tempnam (const char *__dir, const char *__pfx)
   ;
extern int fflush (FILE *__stream);
extern int fflush_unlocked (FILE *__stream);
extern FILE *fopen (const char * __filename,
      const char * __modes)
  ;
extern FILE *freopen (const char * __filename,
        const char * __modes,
        FILE * __stream) ;
extern FILE *fdopen (int __fd, const char *__modes)
  ;
extern FILE *fmemopen (void *__s, size_t __len, const char *__modes)
  ;
extern FILE *open_memstream (char **__bufloc, size_t *__sizeloc)
  ;
extern void setbuf (FILE * __stream, char * __buf) ;
extern int setvbuf (FILE * __stream, char * __buf,
      int __modes, size_t __n) ;
extern void setbuffer (FILE * __stream, char * __buf,
         size_t __size) ;
extern void setlinebuf (FILE *__stream) ;
extern int fprintf (FILE * __stream,
      const char * __format, ...);
extern int printf (const char * __format, ...);
extern int sprintf (char * __s,
      const char * __format, ...) ;
extern int vfprintf (FILE * __s, const char * __format,
       __gnuc_va_list __arg);
extern int vprintf (const char * __format, __gnuc_va_list __arg);
extern int vsprintf (char * __s, const char * __format,
       __gnuc_va_list __arg) ;
extern int snprintf (char * __s, size_t __maxlen,
       const char * __format, ...)
     ;
extern int vsnprintf (char * __s, size_t __maxlen,
        const char * __format, __gnuc_va_list __arg)
     ;
extern int vdprintf (int __fd, const char * __fmt,
       __gnuc_va_list __arg)
     ;
extern int dprintf (int __fd, const char * __fmt, ...)
     ;
extern int fscanf (FILE * __stream,
     const char * __format, ...) ;
extern int scanf (const char * __format, ...) ;
extern int sscanf (const char * __s,
     const char * __format, ...) ;
extern int fscanf (FILE * __stream, const char * __format, ...)
                               ;
extern int scanf (const char * __format, ...)
                              ;
extern int sscanf (const char * __s, const char * __format, ...)
                      ;
extern int vfscanf (FILE * __s, const char * __format,
      __gnuc_va_list __arg)
     ;
extern int vscanf (const char * __format, __gnuc_va_list __arg)
     ;
extern int vsscanf (const char * __s,
      const char * __format, __gnuc_va_list __arg)
     ;
extern int vfscanf (FILE * __s, const char * __format, __gnuc_va_list __arg)
     ;
extern int vscanf (const char * __format, __gnuc_va_list __arg)
     ;
extern int vsscanf (const char * __s, const char * __format, __gnuc_va_list __arg)
     ;
extern int fgetc (FILE *__stream);
extern int getc (FILE *__stream);
extern int getchar (void);
extern int getc_unlocked (FILE *__stream);
extern int getchar_unlocked (void);
extern int fgetc_unlocked (FILE *__stream);
extern int fputc (int __c, FILE *__stream);
extern int putc (int __c, FILE *__stream);
extern int putchar (int __c);
extern int fputc_unlocked (int __c, FILE *__stream);
extern int putc_unlocked (int __c, FILE *__stream);
extern int putchar_unlocked (int __c);
extern int getw (FILE *__stream);
extern int putw (int __w, FILE *__stream);
extern char *fgets (char * __s, int __n, FILE * __stream)
     ;
extern __ssize_t __getdelim (char ** __lineptr,
                             size_t * __n, int __delimiter,
                             FILE * __stream) ;
extern __ssize_t getdelim (char ** __lineptr,
                           size_t * __n, int __delimiter,
                           FILE * __stream) ;
extern __ssize_t getline (char ** __lineptr,
                          size_t * __n,
                          FILE * __stream) ;
extern int fputs (const char * __s, FILE * __stream);
extern int puts (const char *__s);
extern int ungetc (int __c, FILE *__stream);
extern size_t fread (void * __ptr, size_t __size,
       size_t __n, FILE * __stream) ;
extern size_t fwrite (const void * __ptr, size_t __size,
        size_t __n, FILE * __s);
extern size_t fread_unlocked (void * __ptr, size_t __size,
         size_t __n, FILE * __stream) ;
extern size_t fwrite_unlocked (const void * __ptr, size_t __size,
          size_t __n, FILE * __stream);
extern int fseek (FILE *__stream, long int __off, int __whence);
extern long int ftell (FILE *__stream) ;
extern void rewind (FILE *__stream);
extern int fseeko (FILE *__stream, __off_t __off, int __whence);
extern __off_t ftello (FILE *__stream) ;
extern int fgetpos (FILE * __stream, fpos_t * __pos);
extern int fsetpos (FILE *__stream, const fpos_t *__pos);
extern void clearerr (FILE *__stream) ;
extern int feof (FILE *__stream) ;
extern int ferror (FILE *__stream) ;
extern void clearerr_unlocked (FILE *__stream) ;
extern int feof_unlocked (FILE *__stream) ;
extern int ferror_unlocked (FILE *__stream) ;
extern void perror (const char *__s);
extern int fileno (FILE *__stream) ;
extern int fileno_unlocked (FILE *__stream) ;
extern int pclose (FILE *__stream);
extern FILE *popen (const char *__command, const char *__modes)
  ;
extern char *ctermid (char *__s)
  ;
extern void flockfile (FILE *__stream) ;
extern int ftrylockfile (FILE *__stream) ;
extern void funlockfile (FILE *__stream) ;
extern int __uflow (FILE *);
extern int __overflow (FILE *, int);
extern void *memcpy (void * __dest, const void * __src,
       size_t __n) ;
extern void *memmove (void *__dest, const void *__src, size_t __n)
     ;
extern void *memccpy (void * __dest, const void * __src,
        int __c, size_t __n)
    ;
extern void *memset (void *__s, int __c, size_t __n) ;
extern int memcmp (const void *__s1, const void *__s2, size_t __n)
     ;
extern int __memcmpeq (const void *__s1, const void *__s2, size_t __n)
     ;
extern void *memchr (const void *__s, int __c, size_t __n)
      ;
extern char *strcpy (char * __dest, const char * __src)
     ;
extern char *strncpy (char * __dest,
        const char * __src, size_t __n)
     ;
extern char *strcat (char * __dest, const char * __src)
     ;
extern char *strncat (char * __dest, const char * __src,
        size_t __n) ;
extern int strcmp (const char *__s1, const char *__s2)
     ;
extern int strncmp (const char *__s1, const char *__s2, size_t __n)
     ;
extern int strcoll (const char *__s1, const char *__s2)
     ;
extern size_t strxfrm (char * __dest,
         const char * __src, size_t __n)
    ;
struct __locale_struct
{
  struct __locale_data *__locales[13];
  const unsigned short int *__ctype_b;
  const int *__ctype_tolower;
  const int *__ctype_toupper;
  const char *__names[13];
};
typedef struct __locale_struct *__locale_t;
typedef __locale_t locale_t;
extern int strcoll_l (const char *__s1, const char *__s2, locale_t __l)
     ;
extern size_t strxfrm_l (char *__dest, const char *__src, size_t __n,
    locale_t __l)
     ;
extern char *strdup (const char *__s)
     ;
extern char *strndup (const char *__string, size_t __n)
     ;
extern char *strchr (const char *__s, int __c)
     ;
extern char *strrchr (const char *__s, int __c)
     ;
extern size_t strcspn (const char *__s, const char *__reject)
     ;
extern size_t strspn (const char *__s, const char *__accept)
     ;
extern char *strpbrk (const char *__s, const char *__accept)
     ;
extern char *strstr (const char *__haystack, const char *__needle)
     ;
extern char *strtok (char * __s, const char * __delim)
     ;
extern char *__strtok_r (char * __s,
    const char * __delim,
    char ** __save_ptr)
     ;
extern char *strtok_r (char * __s, const char * __delim,
         char ** __save_ptr)
     ;
extern size_t strlen (const char *__s)
     ;
extern size_t strnlen (const char *__string, size_t __maxlen)
     ;
extern char *strerror (int __errnum) ;
extern int strerror_r (int __errnum, char *__buf, size_t __buflen)
    ;
extern char *strerror_l (int __errnum, locale_t __l) ;
extern int bcmp (const void *__s1, const void *__s2, size_t __n)
     ;
extern void bcopy (const void *__src, void *__dest, size_t __n)
  ;
extern void bzero (void *__s, size_t __n) ;
extern char *index (const char *__s, int __c)
     ;
extern char *rindex (const char *__s, int __c)
     ;
extern int ffs (int __i) ;
extern int ffsl (long int __l) ;
 extern int ffsll (long long int __ll)
     ;
extern int strcasecmp (const char *__s1, const char *__s2)
     ;
extern int strncasecmp (const char *__s1, const char *__s2, size_t __n)
     ;
extern int strcasecmp_l (const char *__s1, const char *__s2, locale_t __loc)
     ;
extern int strncasecmp_l (const char *__s1, const char *__s2,
     size_t __n, locale_t __loc)
     ;
extern void explicit_bzero (void *__s, size_t __n)
    ;
extern char *strsep (char ** __stringp,
       const char * __delim)
     ;
extern char *strsignal (int __sig) ;
extern char *__stpcpy (char * __dest, const char * __src)
     ;
extern char *stpcpy (char * __dest, const char * __src)
     ;
extern char *__stpncpy (char * __dest,
   const char * __src, size_t __n)
     ;
extern char *stpncpy (char * __dest,
        const char * __src, size_t __n)
     ;
typedef int __gwchar_t;
typedef struct
  {
    long int quot;
    long int rem;
  } imaxdiv_t;
extern intmax_t imaxabs (intmax_t __n) ;
extern imaxdiv_t imaxdiv (intmax_t __numer, intmax_t __denom)
      ;
extern intmax_t strtoimax (const char * __nptr,
      char ** __endptr, int __base) ;
extern uintmax_t strtoumax (const char * __nptr,
       char ** __endptr, int __base) ;
extern intmax_t wcstoimax (const __gwchar_t * __nptr,
      __gwchar_t ** __endptr, int __base)
     ;
extern uintmax_t wcstoumax (const __gwchar_t * __nptr,
       __gwchar_t ** __endptr, int __base)
     ;
typedef __useconds_t useconds_t;
typedef __socklen_t socklen_t;
extern int access (const char *__name, int __type) ;
extern int faccessat (int __fd, const char *__file, int __type, int __flag)
     ;
extern __off_t lseek (int __fd, __off_t __offset, int __whence) ;
extern int close (int __fd);
extern void closefrom (int __lowfd) ;
extern ssize_t read (int __fd, void *__buf, size_t __nbytes)
    ;
extern ssize_t write (int __fd, const void *__buf, size_t __n)
    ;
extern ssize_t pread (int __fd, void *__buf, size_t __nbytes,
        __off_t __offset)
    ;
extern ssize_t pwrite (int __fd, const void *__buf, size_t __n,
         __off_t __offset)
    ;
extern int pipe (int __pipedes[2]) ;
extern unsigned int alarm (unsigned int __seconds) ;
extern unsigned int sleep (unsigned int __seconds);
extern __useconds_t ualarm (__useconds_t __value, __useconds_t __interval)
     ;
extern int usleep (__useconds_t __useconds);
extern int pause (void);
extern int chown (const char *__file, __uid_t __owner, __gid_t __group)
     ;
extern int fchown (int __fd, __uid_t __owner, __gid_t __group) ;
extern int lchown (const char *__file, __uid_t __owner, __gid_t __group)
     ;
extern int fchownat (int __fd, const char *__file, __uid_t __owner,
       __gid_t __group, int __flag)
     ;
extern int chdir (const char *__path) ;
extern int fchdir (int __fd) ;
extern char *getcwd (char *__buf, size_t __size) ;
extern char *getwd (char *__buf)
    
    ;
extern int dup (int __fd) ;
extern int dup2 (int __fd, int __fd2) ;
extern char **__environ;
extern int execve (const char *__path, char *const __argv[],
     char *const __envp[]) ;
extern int fexecve (int __fd, char *const __argv[], char *const __envp[])
     ;
extern int execv (const char *__path, char *const __argv[])
     ;
extern int execle (const char *__path, const char *__arg, ...)
     ;
extern int execl (const char *__path, const char *__arg, ...)
     ;
extern int execvp (const char *__file, char *const __argv[])
     ;
extern int execlp (const char *__file, const char *__arg, ...)
     ;
extern int nice (int __inc) ;
extern void _exit (int __status) ;
enum
  {
    _PC_LINK_MAX,
    _PC_MAX_CANON,
    _PC_MAX_INPUT,
    _PC_NAME_MAX,
    _PC_PATH_MAX,
    _PC_PIPE_BUF,
    _PC_CHOWN_RESTRICTED,
    _PC_NO_TRUNC,
    _PC_VDISABLE,
    _PC_SYNC_IO,
    _PC_ASYNC_IO,
    _PC_PRIO_IO,
    _PC_SOCK_MAXBUF,
    _PC_FILESIZEBITS,
    _PC_REC_INCR_XFER_SIZE,
    _PC_REC_MAX_XFER_SIZE,
    _PC_REC_MIN_XFER_SIZE,
    _PC_REC_XFER_ALIGN,
    _PC_ALLOC_SIZE_MIN,
    _PC_SYMLINK_MAX,
    _PC_2_SYMLINKS
  };
enum
  {
    _SC_ARG_MAX,
    _SC_CHILD_MAX,
    _SC_CLK_TCK,
    _SC_NGROUPS_MAX,
    _SC_OPEN_MAX,
    _SC_STREAM_MAX,
    _SC_TZNAME_MAX,
    _SC_JOB_CONTROL,
    _SC_SAVED_IDS,
    _SC_REALTIME_SIGNALS,
    _SC_PRIORITY_SCHEDULING,
    _SC_TIMERS,
    _SC_ASYNCHRONOUS_IO,
    _SC_PRIORITIZED_IO,
    _SC_SYNCHRONIZED_IO,
    _SC_FSYNC,
    _SC_MAPPED_FILES,
    _SC_MEMLOCK,
    _SC_MEMLOCK_RANGE,
    _SC_MEMORY_PROTECTION,
    _SC_MESSAGE_PASSING,
    _SC_SEMAPHORES,
    _SC_SHARED_MEMORY_OBJECTS,
    _SC_AIO_LISTIO_MAX,
    _SC_AIO_MAX,
    _SC_AIO_PRIO_DELTA_MAX,
    _SC_DELAYTIMER_MAX,
    _SC_MQ_OPEN_MAX,
    _SC_MQ_PRIO_MAX,
    _SC_VERSION,
    _SC_PAGESIZE,
    _SC_RTSIG_MAX,
    _SC_SEM_NSEMS_MAX,
    _SC_SEM_VALUE_MAX,
    _SC_SIGQUEUE_MAX,
    _SC_TIMER_MAX,
    _SC_BC_BASE_MAX,
    _SC_BC_DIM_MAX,
    _SC_BC_SCALE_MAX,
    _SC_BC_STRING_MAX,
    _SC_COLL_WEIGHTS_MAX,
    _SC_EQUIV_CLASS_MAX,
    _SC_EXPR_NEST_MAX,
    _SC_LINE_MAX,
    _SC_RE_DUP_MAX,
    _SC_CHARCLASS_NAME_MAX,
    _SC_2_VERSION,
    _SC_2_C_BIND,
    _SC_2_C_DEV,
    _SC_2_FORT_DEV,
    _SC_2_FORT_RUN,
    _SC_2_SW_DEV,
    _SC_2_LOCALEDEF,
    _SC_PII,
    _SC_PII_XTI,
    _SC_PII_SOCKET,
    _SC_PII_INTERNET,
    _SC_PII_OSI,
    _SC_POLL,
    _SC_SELECT,
    _SC_UIO_MAXIOV,
    _SC_IOV_MAX = _SC_UIO_MAXIOV,
    _SC_PII_INTERNET_STREAM,
    _SC_PII_INTERNET_DGRAM,
    _SC_PII_OSI_COTS,
    _SC_PII_OSI_CLTS,
    _SC_PII_OSI_M,
    _SC_T_IOV_MAX,
    _SC_THREADS,
    _SC_THREAD_SAFE_FUNCTIONS,
    _SC_GETGR_R_SIZE_MAX,
    _SC_GETPW_R_SIZE_MAX,
    _SC_LOGIN_NAME_MAX,
    _SC_TTY_NAME_MAX,
    _SC_THREAD_DESTRUCTOR_ITERATIONS,
    _SC_THREAD_KEYS_MAX,
    _SC_THREAD_STACK_MIN,
    _SC_THREAD_THREADS_MAX,
    _SC_THREAD_ATTR_STACKADDR,
    _SC_THREAD_ATTR_STACKSIZE,
    _SC_THREAD_PRIORITY_SCHEDULING,
    _SC_THREAD_PRIO_INHERIT,
    _SC_THREAD_PRIO_PROTECT,
    _SC_THREAD_PROCESS_SHARED,
    _SC_NPROCESSORS_CONF,
    _SC_NPROCESSORS_ONLN,
    _SC_PHYS_PAGES,
    _SC_AVPHYS_PAGES,
    _SC_ATEXIT_MAX,
    _SC_PASS_MAX,
    _SC_XOPEN_VERSION,
    _SC_XOPEN_XCU_VERSION,
    _SC_XOPEN_UNIX,
    _SC_XOPEN_CRYPT,
    _SC_XOPEN_ENH_I18N,
    _SC_XOPEN_SHM,
    _SC_2_CHAR_TERM,
    _SC_2_C_VERSION,
    _SC_2_UPE,
    _SC_XOPEN_XPG2,
    _SC_XOPEN_XPG3,
    _SC_XOPEN_XPG4,
    _SC_CHAR_BIT,
    _SC_CHAR_MAX,
    _SC_CHAR_MIN,
    _SC_INT_MAX,
    _SC_INT_MIN,
    _SC_LONG_BIT,
    _SC_WORD_BIT,
    _SC_MB_LEN_MAX,
    _SC_NZERO,
    _SC_SSIZE_MAX,
    _SC_SCHAR_MAX,
    _SC_SCHAR_MIN,
    _SC_SHRT_MAX,
    _SC_SHRT_MIN,
    _SC_UCHAR_MAX,
    _SC_UINT_MAX,
    _SC_ULONG_MAX,
    _SC_USHRT_MAX,
    _SC_NL_ARGMAX,
    _SC_NL_LANGMAX,
    _SC_NL_MSGMAX,
    _SC_NL_NMAX,
    _SC_NL_SETMAX,
    _SC_NL_TEXTMAX,
    _SC_XBS5_ILP32_OFF32,
    _SC_XBS5_ILP32_OFFBIG,
    _SC_XBS5_LP64_OFF64,
    _SC_XBS5_LPBIG_OFFBIG,
    _SC_XOPEN_LEGACY,
    _SC_XOPEN_REALTIME,
    _SC_XOPEN_REALTIME_THREADS,
    _SC_ADVISORY_INFO,
    _SC_BARRIERS,
    _SC_BASE,
    _SC_C_LANG_SUPPORT,
    _SC_C_LANG_SUPPORT_R,
    _SC_CLOCK_SELECTION,
    _SC_CPUTIME,
    _SC_THREAD_CPUTIME,
    _SC_DEVICE_IO,
    _SC_DEVICE_SPECIFIC,
    _SC_DEVICE_SPECIFIC_R,
    _SC_FD_MGMT,
    _SC_FIFO,
    _SC_PIPE,
    _SC_FILE_ATTRIBUTES,
    _SC_FILE_LOCKING,
    _SC_FILE_SYSTEM,
    _SC_MONOTONIC_CLOCK,
    _SC_MULTI_PROCESS,
    _SC_SINGLE_PROCESS,
    _SC_NETWORKING,
    _SC_READER_WRITER_LOCKS,
    _SC_SPIN_LOCKS,
    _SC_REGEXP,
    _SC_REGEX_VERSION,
    _SC_SHELL,
    _SC_SIGNALS,
    _SC_SPAWN,
    _SC_SPORADIC_SERVER,
    _SC_THREAD_SPORADIC_SERVER,
    _SC_SYSTEM_DATABASE,
    _SC_SYSTEM_DATABASE_R,
    _SC_TIMEOUTS,
    _SC_TYPED_MEMORY_OBJECTS,
    _SC_USER_GROUPS,
    _SC_USER_GROUPS_R,
    _SC_2_PBS,
    _SC_2_PBS_ACCOUNTING,
    _SC_2_PBS_LOCATE,
    _SC_2_PBS_MESSAGE,
    _SC_2_PBS_TRACK,
    _SC_SYMLOOP_MAX,
    _SC_STREAMS,
    _SC_2_PBS_CHECKPOINT,
    _SC_V6_ILP32_OFF32,
    _SC_V6_ILP32_OFFBIG,
    _SC_V6_LP64_OFF64,
    _SC_V6_LPBIG_OFFBIG,
    _SC_HOST_NAME_MAX,
    _SC_TRACE,
    _SC_TRACE_EVENT_FILTER,
    _SC_TRACE_INHERIT,
    _SC_TRACE_LOG,
    _SC_LEVEL1_ICACHE_SIZE,
    _SC_LEVEL1_ICACHE_ASSOC,
    _SC_LEVEL1_ICACHE_LINESIZE,
    _SC_LEVEL1_DCACHE_SIZE,
    _SC_LEVEL1_DCACHE_ASSOC,
    _SC_LEVEL1_DCACHE_LINESIZE,
    _SC_LEVEL2_CACHE_SIZE,
    _SC_LEVEL2_CACHE_ASSOC,
    _SC_LEVEL2_CACHE_LINESIZE,
    _SC_LEVEL3_CACHE_SIZE,
    _SC_LEVEL3_CACHE_ASSOC,
    _SC_LEVEL3_CACHE_LINESIZE,
    _SC_LEVEL4_CACHE_SIZE,
    _SC_LEVEL4_CACHE_ASSOC,
    _SC_LEVEL4_CACHE_LINESIZE,
    _SC_IPV6 = _SC_LEVEL1_ICACHE_SIZE + 50,
    _SC_RAW_SOCKETS,
    _SC_V7_ILP32_OFF32,
    _SC_V7_ILP32_OFFBIG,
    _SC_V7_LP64_OFF64,
    _SC_V7_LPBIG_OFFBIG,
    _SC_SS_REPL_MAX,
    _SC_TRACE_EVENT_NAME_MAX,
    _SC_TRACE_NAME_MAX,
    _SC_TRACE_SYS_MAX,
    _SC_TRACE_USER_EVENT_MAX,
    _SC_XOPEN_STREAMS,
    _SC_THREAD_ROBUST_PRIO_INHERIT,
    _SC_THREAD_ROBUST_PRIO_PROTECT,
    _SC_MINSIGSTKSZ,
    _SC_SIGSTKSZ
  };
enum
  {
    _CS_PATH,
    _CS_V6_WIDTH_RESTRICTED_ENVS,
    _CS_GNU_LIBC_VERSION,
    _CS_GNU_LIBPTHREAD_VERSION,
    _CS_V5_WIDTH_RESTRICTED_ENVS,
    _CS_V7_WIDTH_RESTRICTED_ENVS,
    _CS_LFS_CFLAGS = 1000,
    _CS_LFS_LDFLAGS,
    _CS_LFS_LIBS,
    _CS_LFS_LINTFLAGS,
    _CS_LFS64_CFLAGS,
    _CS_LFS64_LDFLAGS,
    _CS_LFS64_LIBS,
    _CS_LFS64_LINTFLAGS,
    _CS_XBS5_ILP32_OFF32_CFLAGS = 1100,
    _CS_XBS5_ILP32_OFF32_LDFLAGS,
    _CS_XBS5_ILP32_OFF32_LIBS,
    _CS_XBS5_ILP32_OFF32_LINTFLAGS,
    _CS_XBS5_ILP32_OFFBIG_CFLAGS,
    _CS_XBS5_ILP32_OFFBIG_LDFLAGS,
    _CS_XBS5_ILP32_OFFBIG_LIBS,
    _CS_XBS5_ILP32_OFFBIG_LINTFLAGS,
    _CS_XBS5_LP64_OFF64_CFLAGS,
    _CS_XBS5_LP64_OFF64_LDFLAGS,
    _CS_XBS5_LP64_OFF64_LIBS,
    _CS_XBS5_LP64_OFF64_LINTFLAGS,
    _CS_XBS5_LPBIG_OFFBIG_CFLAGS,
    _CS_XBS5_LPBIG_OFFBIG_LDFLAGS,
    _CS_XBS5_LPBIG_OFFBIG_LIBS,
    _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS,
    _CS_POSIX_V6_ILP32_OFF32_CFLAGS,
    _CS_POSIX_V6_ILP32_OFF32_LDFLAGS,
    _CS_POSIX_V6_ILP32_OFF32_LIBS,
    _CS_POSIX_V6_ILP32_OFF32_LINTFLAGS,
    _CS_POSIX_V6_ILP32_OFFBIG_CFLAGS,
    _CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS,
    _CS_POSIX_V6_ILP32_OFFBIG_LIBS,
    _CS_POSIX_V6_ILP32_OFFBIG_LINTFLAGS,
    _CS_POSIX_V6_LP64_OFF64_CFLAGS,
    _CS_POSIX_V6_LP64_OFF64_LDFLAGS,
    _CS_POSIX_V6_LP64_OFF64_LIBS,
    _CS_POSIX_V6_LP64_OFF64_LINTFLAGS,
    _CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS,
    _CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS,
    _CS_POSIX_V6_LPBIG_OFFBIG_LIBS,
    _CS_POSIX_V6_LPBIG_OFFBIG_LINTFLAGS,
    _CS_POSIX_V7_ILP32_OFF32_CFLAGS,
    _CS_POSIX_V7_ILP32_OFF32_LDFLAGS,
    _CS_POSIX_V7_ILP32_OFF32_LIBS,
    _CS_POSIX_V7_ILP32_OFF32_LINTFLAGS,
    _CS_POSIX_V7_ILP32_OFFBIG_CFLAGS,
    _CS_POSIX_V7_ILP32_OFFBIG_LDFLAGS,
    _CS_POSIX_V7_ILP32_OFFBIG_LIBS,
    _CS_POSIX_V7_ILP32_OFFBIG_LINTFLAGS,
    _CS_POSIX_V7_LP64_OFF64_CFLAGS,
    _CS_POSIX_V7_LP64_OFF64_LDFLAGS,
    _CS_POSIX_V7_LP64_OFF64_LIBS,
    _CS_POSIX_V7_LP64_OFF64_LINTFLAGS,
    _CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS,
    _CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS,
    _CS_POSIX_V7_LPBIG_OFFBIG_LIBS,
    _CS_POSIX_V7_LPBIG_OFFBIG_LINTFLAGS,
    _CS_V6_ENV,
    _CS_V7_ENV
  };
extern long int pathconf (const char *__path, int __name)
     ;
extern long int fpathconf (int __fd, int __name) ;
extern long int sysconf (int __name) ;
extern size_t confstr (int __name, char *__buf, size_t __len)
    ;
extern __pid_t getpid (void) ;
extern __pid_t getppid (void) ;
extern __pid_t getpgrp (void) ;
extern __pid_t __getpgid (__pid_t __pid) ;
extern __pid_t getpgid (__pid_t __pid) ;
extern int setpgid (__pid_t __pid, __pid_t __pgid) ;
extern int setpgrp (void) ;
extern __pid_t setsid (void) ;
extern __pid_t getsid (__pid_t __pid) ;
extern __uid_t getuid (void) ;
extern __uid_t geteuid (void) ;
extern __gid_t getgid (void) ;
extern __gid_t getegid (void) ;
extern int getgroups (int __size, __gid_t __list[])
    ;
extern int setuid (__uid_t __uid) ;
extern int setreuid (__uid_t __ruid, __uid_t __euid) ;
extern int seteuid (__uid_t __uid) ;
extern int setgid (__gid_t __gid) ;
extern int setregid (__gid_t __rgid, __gid_t __egid) ;
extern int setegid (__gid_t __gid) ;
extern __pid_t fork (void) ;
extern __pid_t vfork (void) ;
extern char *ttyname (int __fd) ;
extern int ttyname_r (int __fd, char *__buf, size_t __buflen)
    
     ;
extern int isatty (int __fd) ;
extern int ttyslot (void) ;
extern int link (const char *__from, const char *__to)
     ;
extern int linkat (int __fromfd, const char *__from, int __tofd,
     const char *__to, int __flags)
     ;
extern int symlink (const char *__from, const char *__to)
     ;
extern ssize_t readlink (const char * __path,
    char * __buf, size_t __len)
    
     ;
extern int symlinkat (const char *__from, int __tofd,
        const char *__to) ;
extern ssize_t readlinkat (int __fd, const char * __path,
      char * __buf, size_t __len)
    
     ;
extern int unlink (const char *__name) ;
extern int unlinkat (int __fd, const char *__name, int __flag)
     ;
extern int rmdir (const char *__path) ;
extern __pid_t tcgetpgrp (int __fd) ;
extern int tcsetpgrp (int __fd, __pid_t __pgrp_id) ;
extern char *getlogin (void);
extern int getlogin_r (char *__name, size_t __name_len)
    ;
extern int setlogin (const char *__name) ;
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
extern int getopt (int ___argc, char *const *___argv, const char *__shortopts)
       ;
extern int gethostname (char *__name, size_t __len)
    ;
extern int sethostname (const char *__name, size_t __len)
     ;
extern int sethostid (long int __id) ;
extern int getdomainname (char *__name, size_t __len)
    
     ;
extern int setdomainname (const char *__name, size_t __len)
     ;
extern int vhangup (void) ;
extern int revoke (const char *__file) ;
extern int profil (unsigned short int *__sample_buffer, size_t __size,
     size_t __offset, unsigned int __scale)
     ;
extern int acct (const char *__name) ;
extern char *getusershell (void) ;
extern void endusershell (void) ;
extern void setusershell (void) ;
extern int daemon (int __nochdir, int __noclose) ;
extern int chroot (const char *__path) ;
extern char *getpass (const char *__prompt) ;
extern int fsync (int __fd);
extern long int gethostid (void);
extern void sync (void) ;
extern int getpagesize (void) ;
extern int getdtablesize (void) ;
extern int truncate (const char *__file, __off_t __length)
     ;
extern int ftruncate (int __fd, __off_t __length) ;
extern int brk (void *__addr) ;
extern void *sbrk (intptr_t __delta) ;
extern long int syscall (long int __sysno, ...) ;
extern int lockf (int __fd, int __cmd, __off_t __len) ;
extern int fdatasync (int __fildes);
extern char *crypt (const char *__key, const char *__salt)
     ;
int getentropy (void *__buffer, size_t __length)
    ;
extern ssize_t __read_chk (int __fd, void *__buf, size_t __nbytes,
      size_t __buflen)
  ;
extern ssize_t __read_alias (int __fd, void *__buf, size_t __nbytes)
  ;
extern ssize_t __read_chk_warn (int __fd, void *__buf, size_t __nbytes, size_t __buflen)
    
                                  ;
extern ssize_t __pread_chk (int __fd, void *__buf, size_t __nbytes,
       __off_t __offset, size_t __bufsize)
  ;
extern ssize_t __pread64_chk (int __fd, void *__buf, size_t __nbytes,
         __off64_t __offset, size_t __bufsize)
  ;
extern ssize_t __pread_alias (int __fd, void *__buf, size_t __nbytes, __off_t __offset)
  ;
extern ssize_t __pread64_alias (int __fd, void *__buf, size_t __nbytes, __off64_t __offset)
  ;
extern ssize_t __pread_chk_warn (int __fd, void *__buf, size_t __nbytes, __off_t __offset, size_t __bufsize)
    
                                  ;
extern ssize_t __pread64_chk_warn (int __fd, void *__buf, size_t __nbytes, __off64_t __offset, size_t __bufsize)
    
                                  ;
extern ssize_t __readlink_chk (const char * __path,
          char * __buf, size_t __len,
          size_t __buflen)
     ;
extern ssize_t __readlink_alias (const char * __path, char * __buf, size_t __len)
     ;
extern ssize_t __readlink_chk_warn (const char * __path, char * __buf, size_t __len, size_t __buflen)
    
                                         ;
extern ssize_t __readlinkat_chk (int __fd, const char * __path,
     char * __buf, size_t __len,
     size_t __buflen)
     ;
extern ssize_t __readlinkat_alias (int __fd, const char * __path, char * __buf, size_t __len)
     ;
extern ssize_t __readlinkat_chk_warn (int __fd, const char * __path, char * __buf, size_t __len, size_t __buflen)
    
                ;
extern char *__getcwd_chk (char *__buf, size_t __size, size_t __buflen)
     ;
extern char *__getcwd_alias (char *__buf, size_t __size)
                                                   ;
extern char *__getcwd_chk_warn (char *__buf, size_t __size, size_t __buflen)
    
                              ;
extern char *__getwd_chk (char *__buf, size_t buflen)
     ;
extern char *__getwd_warn (char *__buf)
    
                                         ;
extern size_t __confstr_chk (int __name, char *__buf, size_t __len,
        size_t __buflen)
  ;
extern size_t __confstr_alias (int __name, char *__buf, size_t __len)
   ;
extern size_t __confstr_chk_warn (int __name, char *__buf, size_t __len, size_t __buflen)
    
            ;
extern int __getgroups_chk (int __size, __gid_t __list[], size_t __listlen)
  ;
extern int __getgroups_alias (int __size, __gid_t __list[])
                                                             ;
extern int __getgroups_chk_warn (int __size, __gid_t __list[], size_t __listlen)
    
                                           ;
extern int __ttyname_r_chk (int __fd, char *__buf, size_t __buflen,
       size_t __nreal)
   ;
extern int __ttyname_r_alias (int __fd, char *__buf, size_t __buflen)
     ;
extern int __ttyname_r_chk_warn (int __fd, char *__buf, size_t __buflen, size_t __nreal)
    
                                  ;
extern int __getlogin_r_chk (char *__buf, size_t __buflen, size_t __nreal)
     ;
extern int __getlogin_r_alias (char *__buf, size_t __buflen)
                                    ;
extern int __getlogin_r_chk_warn (char *__buf, size_t __buflen, size_t __nreal)
    
                                  ;
extern int __gethostname_chk (char *__buf, size_t __buflen, size_t __nreal)
     ;
extern int __gethostname_alias (char *__buf, size_t __buflen)
  ;
extern int __gethostname_chk_warn (char *__buf, size_t __buflen, size_t __nreal)
    
                                  ;
extern int __getdomainname_chk (char *__buf, size_t __buflen, size_t __nreal)
     ;
extern int __getdomainname_alias (char *__buf, size_t __buflen)
  ;
extern int __getdomainname_chk_warn (char *__buf, size_t __buflen, size_t __nreal)
    
                    ;
struct tm
{
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
  long int tm_gmtoff;
  const char *tm_zone;
};
struct itimerspec
  {
    struct timespec it_interval;
    struct timespec it_value;
  };
struct sigevent;
extern clock_t clock (void) ;
extern time_t time (time_t *__timer) ;
extern double difftime (time_t __time1, time_t __time0)
     ;
extern time_t mktime (struct tm *__tp) ;
extern size_t strftime (char * __s, size_t __maxsize,
   const char * __format,
   const struct tm * __tp) ;
extern size_t strftime_l (char * __s, size_t __maxsize,
     const char * __format,
     const struct tm * __tp,
     locale_t __loc) ;
extern struct tm *gmtime (const time_t *__timer) ;
extern struct tm *localtime (const time_t *__timer) ;
extern struct tm *gmtime_r (const time_t * __timer,
       struct tm * __tp) ;
extern struct tm *localtime_r (const time_t * __timer,
          struct tm * __tp) ;
extern char *asctime (const struct tm *__tp) ;
extern char *ctime (const time_t *__timer) ;
extern char *asctime_r (const struct tm * __tp,
   char * __buf) ;
extern char *ctime_r (const time_t * __timer,
        char * __buf) ;
extern char *__tzname[2];
extern int __daylight;
extern long int __timezone;
extern char *tzname[2];
extern void tzset (void) ;
extern int daylight;
extern long int timezone;
extern time_t timegm (struct tm *__tp) ;
extern time_t timelocal (struct tm *__tp) ;
extern int dysize (int __year) ;
extern int nanosleep (const struct timespec *__requested_time,
        struct timespec *__remaining);
extern int clock_getres (clockid_t __clock_id, struct timespec *__res) ;
extern int clock_gettime (clockid_t __clock_id, struct timespec *__tp) ;
extern int clock_settime (clockid_t __clock_id, const struct timespec *__tp)
     ;
extern int clock_nanosleep (clockid_t __clock_id, int __flags,
       const struct timespec *__req,
       struct timespec *__rem);
extern int clock_getcpuclockid (pid_t __pid, clockid_t *__clock_id) ;
extern int timer_create (clockid_t __clock_id,
    struct sigevent * __evp,
    timer_t * __timerid) ;
extern int timer_delete (timer_t __timerid) ;
extern int timer_settime (timer_t __timerid, int __flags,
     const struct itimerspec * __value,
     struct itimerspec * __ovalue) ;
extern int timer_gettime (timer_t __timerid, struct itimerspec *__value)
     ;
extern int timer_getoverrun (timer_t __timerid) ;
extern int timespec_get (struct timespec *__ts, int __base)
     ;
extern int *__errno_location (void) ;
struct flock
  {
    short int l_type;
    short int l_whence;
    __off_t l_start;
    __off_t l_len;
    __pid_t l_pid;
  };
struct stat
  {
    __dev_t st_dev;
    __ino_t st_ino;
    __nlink_t st_nlink;
    __mode_t st_mode;
    __uid_t st_uid;
    __gid_t st_gid;
    int __pad0;
    __dev_t st_rdev;
    __off_t st_size;
    __blksize_t st_blksize;
    __blkcnt_t st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    __syscall_slong_t __glibc_reserved[3];
  };
extern int fcntl (int __fd, int __cmd, ...);
extern int open (const char *__file, int __oflag, ...) ;
extern int openat (int __fd, const char *__file, int __oflag, ...)
     ;
extern int creat (const char *__file, mode_t __mode) ;
extern int posix_fadvise (int __fd, off_t __offset, off_t __len,
     int __advise) ;
extern int posix_fallocate (int __fd, off_t __offset, off_t __len);
extern int __open_2 (const char *__path, int __oflag) ;
extern int __open_alias (const char *__path, int __oflag, ...)
                              ;
extern void __open_too_many_args (void)
                                                                  ;
extern void __open_missing_mode (void)
                                                                            ;
extern int __openat_2 (int __fd, const char *__path, int __oflag)
     ;
extern int __openat_alias (int __fd, const char *__path, int __oflag, ...)
     ;
extern void __openat_too_many_args (void)
                                                                    ;
extern void __openat_missing_mode (void)
                                                                             ;
struct option
{
  const char *name;
  int has_arg;
  int *flag;
  int val;
};
extern int getopt_long (int ___argc, char *const *___argv,
   const char *__shortopts,
          const struct option *__longopts, int *__longind)
       ;
extern int getopt_long_only (int ___argc, char *const *___argv,
        const char *__shortopts,
               const struct option *__longopts, int *__longind)
       ;
struct iovec
  {
    void *iov_base;
    size_t iov_len;
  };
enum __socket_type
{
  SOCK_STREAM = 1,
  SOCK_DGRAM = 2,
  SOCK_RAW = 3,
  SOCK_RDM = 4,
  SOCK_SEQPACKET = 5,
  SOCK_DCCP = 6,
  SOCK_PACKET = 10,
  SOCK_CLOEXEC = 02000000,
  SOCK_NONBLOCK = 00004000
};
typedef unsigned short int sa_family_t;
struct sockaddr
  {
    sa_family_t sa_family;
    char sa_data[14];
  };
struct sockaddr_storage
  {
    sa_family_t ss_family;
    char __ss_padding[118];
    unsigned long int __ss_align;
  };
enum
  {
    MSG_OOB = 0x01,
    MSG_PEEK = 0x02,
    MSG_DONTROUTE = 0x04,
    MSG_CTRUNC = 0x08,
    MSG_PROXY = 0x10,
    MSG_TRUNC = 0x20,
    MSG_DONTWAIT = 0x40,
    MSG_EOR = 0x80,
    MSG_WAITALL = 0x100,
    MSG_FIN = 0x200,
    MSG_SYN = 0x400,
    MSG_CONFIRM = 0x800,
    MSG_RST = 0x1000,
    MSG_ERRQUEUE = 0x2000,
    MSG_NOSIGNAL = 0x4000,
    MSG_MORE = 0x8000,
    MSG_WAITFORONE = 0x10000,
    MSG_BATCH = 0x40000,
    MSG_ZEROCOPY = 0x4000000,
    MSG_FASTOPEN = 0x20000000,
    MSG_CMSG_CLOEXEC = 0x40000000
  };
struct msghdr
  {
    void *msg_name;
    socklen_t msg_namelen;
    struct iovec *msg_iov;
    size_t msg_iovlen;
    void *msg_control;
    size_t msg_controllen;
    int msg_flags;
  };
struct cmsghdr
  {
    size_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
    unsigned char __cmsg_data [];
  };
extern struct cmsghdr *__cmsg_nxthdr (struct msghdr *__mhdr,
          struct cmsghdr *__cmsg) ;
enum
  {
    SCM_RIGHTS = 0x01
  };
typedef struct {
 unsigned long fds_bits[16];
} __kernel_fd_set;
typedef void (*__kernel_sighandler_t)(int);
typedef int __kernel_key_t;
typedef int __kernel_mqd_t;
typedef unsigned short __kernel_old_uid_t;
typedef unsigned short __kernel_old_gid_t;
typedef unsigned long __kernel_old_dev_t;
typedef long __kernel_long_t;
typedef unsigned long __kernel_ulong_t;
typedef __kernel_ulong_t __kernel_ino_t;
typedef unsigned int __kernel_mode_t;
typedef int __kernel_pid_t;
typedef int __kernel_ipc_pid_t;
typedef unsigned int __kernel_uid_t;
typedef unsigned int __kernel_gid_t;
typedef __kernel_long_t __kernel_suseconds_t;
typedef int __kernel_daddr_t;
typedef unsigned int __kernel_uid32_t;
typedef unsigned int __kernel_gid32_t;
typedef __kernel_ulong_t __kernel_size_t;
typedef __kernel_long_t __kernel_ssize_t;
typedef __kernel_long_t __kernel_ptrdiff_t;
typedef struct {
 int val[2];
} __kernel_fsid_t;
typedef __kernel_long_t __kernel_off_t;
typedef long long __kernel_loff_t;
typedef __kernel_long_t __kernel_old_time_t;
typedef __kernel_long_t __kernel_time_t;
typedef long long __kernel_time64_t;
typedef __kernel_long_t __kernel_clock_t;
typedef int __kernel_timer_t;
typedef int __kernel_clockid_t;
typedef char * __kernel_caddr_t;
typedef unsigned short __kernel_uid16_t;
typedef unsigned short __kernel_gid16_t;
struct linger
  {
    int l_onoff;
    int l_linger;
  };
struct osockaddr
{
  unsigned short int sa_family;
  unsigned char sa_data[14];
};
enum
{
  SHUT_RD = 0,
  SHUT_WR,
  SHUT_RDWR
};
extern int socket (int __domain, int __type, int __protocol) ;
extern int socketpair (int __domain, int __type, int __protocol,
         int __fds[2]) ;
extern int bind (int __fd, const struct sockaddr * __addr, socklen_t __len)
     ;
extern int getsockname (int __fd, struct sockaddr * __addr,
   socklen_t * __len) ;
extern int connect (int __fd, const struct sockaddr * __addr, socklen_t __len);
extern int getpeername (int __fd, struct sockaddr * __addr,
   socklen_t * __len) ;
extern ssize_t send (int __fd, const void *__buf, size_t __n, int __flags);
extern ssize_t recv (int __fd, void *__buf, size_t __n, int __flags);
extern ssize_t sendto (int __fd, const void *__buf, size_t __n,
         int __flags, const struct sockaddr * __addr,
         socklen_t __addr_len);
extern ssize_t recvfrom (int __fd, void * __buf, size_t __n,
    int __flags, struct sockaddr * __addr,
    socklen_t * __addr_len);
extern ssize_t sendmsg (int __fd, const struct msghdr *__message,
   int __flags);
extern ssize_t recvmsg (int __fd, struct msghdr *__message, int __flags);
extern int getsockopt (int __fd, int __level, int __optname,
         void * __optval,
         socklen_t * __optlen) ;
extern int setsockopt (int __fd, int __level, int __optname,
         const void *__optval, socklen_t __optlen) ;
extern int listen (int __fd, int __n) ;
extern int accept (int __fd, struct sockaddr * __addr,
     socklen_t * __addr_len);
extern int shutdown (int __fd, int __how) ;
extern int sockatmark (int __fd) ;
extern int isfdtype (int __fd, int __fdtype) ;
extern ssize_t __recv_chk (int __fd, void *__buf, size_t __n, size_t __buflen,
      int __flags);
extern ssize_t __recv_alias (int __fd, void *__buf, size_t __n, int __flags)
                          ;
extern ssize_t __recv_chk_warn (int __fd, void *__buf, size_t __n, size_t __buflen, int __flags)
    
            ;
extern ssize_t __recvfrom_chk (int __fd, void * __buf, size_t __n,
          size_t __buflen, int __flags,
          struct sockaddr * __addr,
          socklen_t * __addr_len);
extern ssize_t __recvfrom_alias (int __fd, void * __buf, size_t __n, int __flags, struct sockaddr * __addr, socklen_t * __addr_len)
                                                   ;
extern ssize_t __recvfrom_chk_warn (int __fd, void * __buf, size_t __n, size_t __buflen, int __flags, struct sockaddr * __addr, socklen_t * __addr_len)
    
                        ;
typedef uint32_t in_addr_t;
struct in_addr
  {
    in_addr_t s_addr;
  };
struct ip_opts
  {
    struct in_addr ip_dst;
    char ip_opts[40];
  };
struct ip_mreqn
  {
    struct in_addr imr_multiaddr;
    struct in_addr imr_address;
    int imr_ifindex;
  };
struct in_pktinfo
  {
    int ipi_ifindex;
    struct in_addr ipi_spec_dst;
    struct in_addr ipi_addr;
  };
enum
  {
    IPPROTO_IP = 0,
    IPPROTO_ICMP = 1,
    IPPROTO_IGMP = 2,
    IPPROTO_IPIP = 4,
    IPPROTO_TCP = 6,
    IPPROTO_EGP = 8,
    IPPROTO_PUP = 12,
    IPPROTO_UDP = 17,
    IPPROTO_IDP = 22,
    IPPROTO_TP = 29,
    IPPROTO_DCCP = 33,
    IPPROTO_IPV6 = 41,
    IPPROTO_RSVP = 46,
    IPPROTO_GRE = 47,
    IPPROTO_ESP = 50,
    IPPROTO_AH = 51,
    IPPROTO_MTP = 92,
    IPPROTO_BEETPH = 94,
    IPPROTO_ENCAP = 98,
    IPPROTO_PIM = 103,
    IPPROTO_COMP = 108,
    IPPROTO_SCTP = 132,
    IPPROTO_UDPLITE = 136,
    IPPROTO_MPLS = 137,
    IPPROTO_ETHERNET = 143,
    IPPROTO_RAW = 255,
    IPPROTO_MPTCP = 262,
    IPPROTO_MAX
  };
enum
  {
    IPPROTO_HOPOPTS = 0,
    IPPROTO_ROUTING = 43,
    IPPROTO_FRAGMENT = 44,
    IPPROTO_ICMPV6 = 58,
    IPPROTO_NONE = 59,
    IPPROTO_DSTOPTS = 60,
    IPPROTO_MH = 135
  };
typedef uint16_t in_port_t;
enum
  {
    IPPORT_ECHO = 7,
    IPPORT_DISCARD = 9,
    IPPORT_SYSTAT = 11,
    IPPORT_DAYTIME = 13,
    IPPORT_NETSTAT = 15,
    IPPORT_FTP = 21,
    IPPORT_TELNET = 23,
    IPPORT_SMTP = 25,
    IPPORT_TIMESERVER = 37,
    IPPORT_NAMESERVER = 42,
    IPPORT_WHOIS = 43,
    IPPORT_MTP = 57,
    IPPORT_TFTP = 69,
    IPPORT_RJE = 77,
    IPPORT_FINGER = 79,
    IPPORT_TTYLINK = 87,
    IPPORT_SUPDUP = 95,
    IPPORT_EXECSERVER = 512,
    IPPORT_LOGINSERVER = 513,
    IPPORT_CMDSERVER = 514,
    IPPORT_EFSSERVER = 520,
    IPPORT_BIFFUDP = 512,
    IPPORT_WHOSERVER = 513,
    IPPORT_ROUTESERVER = 520,
    IPPORT_RESERVED = 1024,
    IPPORT_USERRESERVED = 5000
  };
struct in6_addr
  {
    union
      {
 uint8_t __u6_addr8[16];
 uint16_t __u6_addr16[8];
 uint32_t __u6_addr32[4];
      } __in6_u;
  };
extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
struct sockaddr_in
  {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
  };
struct sockaddr_in6
  {
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
  };
struct ip_mreq
  {
    struct in_addr imr_multiaddr;
    struct in_addr imr_interface;
  };
struct ip_mreq_source
  {
    struct in_addr imr_multiaddr;
    struct in_addr imr_interface;
    struct in_addr imr_sourceaddr;
  };
struct ipv6_mreq
  {
    struct in6_addr ipv6mr_multiaddr;
    unsigned int ipv6mr_interface;
  };
struct group_req
  {
    uint32_t gr_interface;
    struct sockaddr_storage gr_group;
  };
struct group_source_req
  {
    uint32_t gsr_interface;
    struct sockaddr_storage gsr_group;
    struct sockaddr_storage gsr_source;
  };
struct ip_msfilter
  {
    struct in_addr imsf_multiaddr;
    struct in_addr imsf_interface;
    uint32_t imsf_fmode;
    uint32_t imsf_numsrc;
    struct in_addr imsf_slist[1];
  };
struct group_filter
  {
    uint32_t gf_interface;
    struct sockaddr_storage gf_group;
    uint32_t gf_fmode;
    uint32_t gf_numsrc;
    struct sockaddr_storage gf_slist[1];
};
extern uint32_t ntohl (uint32_t __netlong) ;
extern uint16_t ntohs (uint16_t __netshort)
     ;
extern uint32_t htonl (uint32_t __hostlong)
     ;
extern uint16_t htons (uint16_t __hostshort)
     ;
extern int bindresvport (int __sockfd, struct sockaddr_in *__sock_in) ;
extern int bindresvport6 (int __sockfd, struct sockaddr_in6 *__sock_in)
     ;
typedef uint32_t tcp_seq;
struct tcphdr
  {
    union
    {
      struct
      {
 uint16_t th_sport;
 uint16_t th_dport;
 tcp_seq th_seq;
 tcp_seq th_ack;
 uint8_t th_x2:4;
 uint8_t th_off:4;
 uint8_t th_flags;
 uint16_t th_win;
 uint16_t th_sum;
 uint16_t th_urp;
      };
      struct
      {
 uint16_t source;
 uint16_t dest;
 uint32_t seq;
 uint32_t ack_seq;
 uint16_t res1:4;
 uint16_t doff:4;
 uint16_t fin:1;
 uint16_t syn:1;
 uint16_t rst:1;
 uint16_t psh:1;
 uint16_t ack:1;
 uint16_t urg:1;
 uint16_t res2:2;
 uint16_t window;
 uint16_t check;
 uint16_t urg_ptr;
      };
    };
};
enum
{
  TCP_ESTABLISHED = 1,
  TCP_SYN_SENT,
  TCP_SYN_RECV,
  TCP_FIN_WAIT1,
  TCP_FIN_WAIT2,
  TCP_TIME_WAIT,
  TCP_CLOSE,
  TCP_CLOSE_WAIT,
  TCP_LAST_ACK,
  TCP_LISTEN,
  TCP_CLOSING
};
enum tcp_ca_state
{
  TCP_CA_Open = 0,
  TCP_CA_Disorder = 1,
  TCP_CA_CWR = 2,
  TCP_CA_Recovery = 3,
  TCP_CA_Loss = 4
};
struct tcp_info
{
  uint8_t tcpi_state;
  uint8_t tcpi_ca_state;
  uint8_t tcpi_retransmits;
  uint8_t tcpi_probes;
  uint8_t tcpi_backoff;
  uint8_t tcpi_options;
  uint8_t tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4;
  uint32_t tcpi_rto;
  uint32_t tcpi_ato;
  uint32_t tcpi_snd_mss;
  uint32_t tcpi_rcv_mss;
  uint32_t tcpi_unacked;
  uint32_t tcpi_sacked;
  uint32_t tcpi_lost;
  uint32_t tcpi_retrans;
  uint32_t tcpi_fackets;
  uint32_t tcpi_last_data_sent;
  uint32_t tcpi_last_ack_sent;
  uint32_t tcpi_last_data_recv;
  uint32_t tcpi_last_ack_recv;
  uint32_t tcpi_pmtu;
  uint32_t tcpi_rcv_ssthresh;
  uint32_t tcpi_rtt;
  uint32_t tcpi_rttvar;
  uint32_t tcpi_snd_ssthresh;
  uint32_t tcpi_snd_cwnd;
  uint32_t tcpi_advmss;
  uint32_t tcpi_reordering;
  uint32_t tcpi_rcv_rtt;
  uint32_t tcpi_rcv_space;
  uint32_t tcpi_total_retrans;
};
struct tcp_md5sig
{
  struct sockaddr_storage tcpm_addr;
  uint8_t tcpm_flags;
  uint8_t tcpm_prefixlen;
  uint16_t tcpm_keylen;
  uint32_t __tcpm_pad;
  uint8_t tcpm_key[80];
};
struct tcp_repair_opt
{
  uint32_t opt_code;
  uint32_t opt_val;
};
enum
{
  TCP_NO_QUEUE,
  TCP_RECV_QUEUE,
  TCP_SEND_QUEUE,
  TCP_QUEUES_NR,
};
struct tcp_cookie_transactions
{
  uint16_t tcpct_flags;
  uint8_t __tcpct_pad1;
  uint8_t tcpct_cookie_desired;
  uint16_t tcpct_s_data_desired;
  uint16_t tcpct_used;
  uint8_t tcpct_value[536U];
};
struct tcp_repair_window
{
  uint32_t snd_wl1;
  uint32_t snd_wnd;
  uint32_t max_window;
  uint32_t rcv_wnd;
  uint32_t rcv_wup;
};
struct tcp_zerocopy_receive
{
  uint64_t address;
  uint32_t length;
  uint32_t recv_skip_hint;
};
extern in_addr_t inet_addr (const char *__cp) ;
extern in_addr_t inet_lnaof (struct in_addr __in) ;
extern struct in_addr inet_makeaddr (in_addr_t __net, in_addr_t __host)
     ;
extern in_addr_t inet_netof (struct in_addr __in) ;
extern in_addr_t inet_network (const char *__cp) ;
extern char *inet_ntoa (struct in_addr __in) ;
extern int inet_pton (int __af, const char * __cp,
        void * __buf) ;
extern const char *inet_ntop (int __af, const void * __cp,
         char * __buf, socklen_t __len)
     ;
extern int inet_aton (const char *__cp, struct in_addr *__inp) ;
extern char *inet_neta (in_addr_t __net, char *__buf, size_t __len)
  ;
extern char *inet_net_ntop (int __af, const void *__cp, int __bits,
       char *__buf, size_t __len) ;
extern int inet_net_pton (int __af, const char *__cp,
     void *__buf, size_t __len) ;
extern unsigned int inet_nsap_addr (const char *__cp,
        unsigned char *__buf, int __len) ;
extern char *inet_nsap_ntoa (int __len, const unsigned char *__cp,
        char *__buf) ;
struct sockaddr_un
  {
    sa_family_t sun_family;
    char sun_path[108];
  };
extern unsigned int gnu_dev_major (__dev_t __dev) ;
extern unsigned int gnu_dev_minor (__dev_t __dev) ;
extern __dev_t gnu_dev_makedev (unsigned int __major, unsigned int __minor) ;
typedef union
{
  char __size[32];
  long int __align;
} sem_t;
extern int sem_init (sem_t *__sem, int __pshared, unsigned int __value)
  ;
extern int sem_destroy (sem_t *__sem) ;
extern sem_t *sem_open (const char *__name, int __oflag, ...)
  ;
extern int sem_close (sem_t *__sem) ;
extern int sem_unlink (const char *__name) ;
extern int sem_wait (sem_t *__sem) ;
extern int sem_timedwait (sem_t * __sem,
     const struct timespec * __abstime)
  ;
extern int sem_trywait (sem_t *__sem) ;
extern int sem_post (sem_t *__sem) ;
extern int sem_getvalue (sem_t * __sem, int * __sval)
  ;
extern void *dlopen (const char *__file, int __mode) ;
extern int dlclose (void *__handle) ;
extern void *dlsym (void * __handle,
      const char * __name) ;
extern char *dlerror (void) ;
struct rpcent
{
  char *r_name;
  char **r_aliases;
  int r_number;
};
extern void setrpcent (int __stayopen) ;
extern void endrpcent (void) ;
extern struct rpcent *getrpcbyname (const char *__name) ;
extern struct rpcent *getrpcbynumber (int __number) ;
extern struct rpcent *getrpcent (void) ;
extern int getrpcbyname_r (const char *__name, struct rpcent *__result_buf,
      char *__buffer, size_t __buflen,
      struct rpcent **__result) ;
extern int getrpcbynumber_r (int __number, struct rpcent *__result_buf,
        char *__buffer, size_t __buflen,
        struct rpcent **__result) ;
extern int getrpcent_r (struct rpcent *__result_buf, char *__buffer,
   size_t __buflen, struct rpcent **__result) ;
struct netent
{
  char *n_name;
  char **n_aliases;
  int n_addrtype;
  uint32_t n_net;
};
extern int *__h_errno_location (void) ;
extern void herror (const char *__str) ;
extern const char *hstrerror (int __err_num) ;
struct hostent
{
  char *h_name;
  char **h_aliases;
  int h_addrtype;
  int h_length;
  char **h_addr_list;
};
extern void sethostent (int __stay_open);
extern void endhostent (void);
extern struct hostent *gethostent (void);
extern struct hostent *gethostbyaddr (const void *__addr, __socklen_t __len,
          int __type);
extern struct hostent *gethostbyname (const char *__name);
extern struct hostent *gethostbyname2 (const char *__name, int __af);
extern int gethostent_r (struct hostent * __result_buf,
    char * __buf, size_t __buflen,
    struct hostent ** __result,
    int * __h_errnop);
extern int gethostbyaddr_r (const void * __addr, __socklen_t __len,
       int __type,
       struct hostent * __result_buf,
       char * __buf, size_t __buflen,
       struct hostent ** __result,
       int * __h_errnop);
extern int gethostbyname_r (const char * __name,
       struct hostent * __result_buf,
       char * __buf, size_t __buflen,
       struct hostent ** __result,
       int * __h_errnop);
extern int gethostbyname2_r (const char * __name, int __af,
        struct hostent * __result_buf,
        char * __buf, size_t __buflen,
        struct hostent ** __result,
        int * __h_errnop);
extern void setnetent (int __stay_open);
extern void endnetent (void);
extern struct netent *getnetent (void);
extern struct netent *getnetbyaddr (uint32_t __net, int __type);
extern struct netent *getnetbyname (const char *__name);
extern int getnetent_r (struct netent * __result_buf,
   char * __buf, size_t __buflen,
   struct netent ** __result,
   int * __h_errnop);
extern int getnetbyaddr_r (uint32_t __net, int __type,
      struct netent * __result_buf,
      char * __buf, size_t __buflen,
      struct netent ** __result,
      int * __h_errnop);
extern int getnetbyname_r (const char * __name,
      struct netent * __result_buf,
      char * __buf, size_t __buflen,
      struct netent ** __result,
      int * __h_errnop);
struct servent
{
  char *s_name;
  char **s_aliases;
  int s_port;
  char *s_proto;
};
extern void setservent (int __stay_open);
extern void endservent (void);
extern struct servent *getservent (void);
extern struct servent *getservbyname (const char *__name, const char *__proto);
extern struct servent *getservbyport (int __port, const char *__proto);
extern int getservent_r (struct servent * __result_buf,
    char * __buf, size_t __buflen,
    struct servent ** __result);
extern int getservbyname_r (const char * __name,
       const char * __proto,
       struct servent * __result_buf,
       char * __buf, size_t __buflen,
       struct servent ** __result);
extern int getservbyport_r (int __port, const char * __proto,
       struct servent * __result_buf,
       char * __buf, size_t __buflen,
       struct servent ** __result);
struct protoent
{
  char *p_name;
  char **p_aliases;
  int p_proto;
};
extern void setprotoent (int __stay_open);
extern void endprotoent (void);
extern struct protoent *getprotoent (void);
extern struct protoent *getprotobyname (const char *__name);
extern struct protoent *getprotobynumber (int __proto);
extern int getprotoent_r (struct protoent * __result_buf,
     char * __buf, size_t __buflen,
     struct protoent ** __result);
extern int getprotobyname_r (const char * __name,
        struct protoent * __result_buf,
        char * __buf, size_t __buflen,
        struct protoent ** __result);
extern int getprotobynumber_r (int __proto,
          struct protoent * __result_buf,
          char * __buf, size_t __buflen,
          struct protoent ** __result);
extern int setnetgrent (const char *__netgroup);
extern void endnetgrent (void);
extern int getnetgrent (char ** __hostp,
   char ** __userp,
   char ** __domainp);
extern int innetgr (const char *__netgroup, const char *__host,
      const char *__user, const char *__domain);
extern int getnetgrent_r (char ** __hostp,
     char ** __userp,
     char ** __domainp,
     char * __buffer, size_t __buflen);
extern int rcmd (char ** __ahost, unsigned short int __rport,
   const char * __locuser,
   const char * __remuser,
   const char * __cmd, int * __fd2p);
extern int rcmd_af (char ** __ahost, unsigned short int __rport,
      const char * __locuser,
      const char * __remuser,
      const char * __cmd, int * __fd2p,
      sa_family_t __af);
extern int rexec (char ** __ahost, int __rport,
    const char * __name,
    const char * __pass,
    const char * __cmd, int * __fd2p);
extern int rexec_af (char ** __ahost, int __rport,
       const char * __name,
       const char * __pass,
       const char * __cmd, int * __fd2p,
       sa_family_t __af);
extern int ruserok (const char *__rhost, int __suser,
      const char *__remuser, const char *__locuser);
extern int ruserok_af (const char *__rhost, int __suser,
         const char *__remuser, const char *__locuser,
         sa_family_t __af);
extern int iruserok (uint32_t __raddr, int __suser,
       const char *__remuser, const char *__locuser);
extern int iruserok_af (const void *__raddr, int __suser,
   const char *__remuser, const char *__locuser,
   sa_family_t __af);
extern int rresvport (int *__alport);
extern int rresvport_af (int *__alport, sa_family_t __af);
struct addrinfo
{
  int ai_flags;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  socklen_t ai_addrlen;
  struct sockaddr *ai_addr;
  char *ai_canonname;
  struct addrinfo *ai_next;
};
extern int getaddrinfo (const char * __name,
   const char * __service,
   const struct addrinfo * __req,
   struct addrinfo ** __pai);
extern void freeaddrinfo (struct addrinfo *__ai) ;
extern const char *gai_strerror (int __ecode) ;
extern int getnameinfo (const struct sockaddr * __sa,
   socklen_t __salen, char * __host,
   socklen_t __hostlen, char * __serv,
   socklen_t __servlen, int __flags);
struct passwd
{
  char *pw_name;
  char *pw_passwd;
  __uid_t pw_uid;
  __gid_t pw_gid;
  char *pw_gecos;
  char *pw_dir;
  char *pw_shell;
};
extern void setpwent (void);
extern void endpwent (void);
extern struct passwd *getpwent (void);
extern struct passwd *fgetpwent (FILE *__stream) ;
extern int putpwent (const struct passwd * __p,
       FILE * __f);
extern struct passwd *getpwuid (__uid_t __uid);
extern struct passwd *getpwnam (const char *__name) ;
extern int getpwent_r (struct passwd * __resultbuf,
         char * __buffer, size_t __buflen,
         struct passwd ** __result)
   
    ;
extern int getpwuid_r (__uid_t __uid,
         struct passwd * __resultbuf,
         char * __buffer, size_t __buflen,
         struct passwd ** __result)
   
    ;
extern int getpwnam_r (const char * __name,
         struct passwd * __resultbuf,
         char * __buffer, size_t __buflen,
         struct passwd ** __result)
   
    ;
extern int fgetpwent_r (FILE * __stream,
   struct passwd * __resultbuf,
   char * __buffer, size_t __buflen,
   struct passwd ** __result)
   
    ;
extern void __assert_fail (const char *__assertion, const char *__file,
      unsigned int __line, const char *__function)
     ;
extern void __assert_perror_fail (int __errnum, const char *__file,
      unsigned int __line, const char *__function)
     ;
extern void __assert (const char *__assertion, const char *__file, int __line)
     ;
typedef long int __jmp_buf[8];
struct __jmp_buf_tag
  {
    __jmp_buf __jmpbuf;
    int __mask_was_saved;
    __sigset_t __saved_mask;
  };
typedef struct __jmp_buf_tag jmp_buf[1];
extern int setjmp (jmp_buf __env) ;
extern int __sigsetjmp (struct __jmp_buf_tag __env[1], int __savemask) ;
extern int _setjmp (struct __jmp_buf_tag __env[1]) ;
extern void longjmp (struct __jmp_buf_tag __env[1], int __val)
     ;
extern void _longjmp (struct __jmp_buf_tag __env[1], int __val)
     ;
typedef struct __jmp_buf_tag sigjmp_buf[1];
extern void siglongjmp (sigjmp_buf __env, int __val)
     ;
extern void longjmp (struct __jmp_buf_tag __env[1], int __val)
                                                      ;
extern void _longjmp (struct __jmp_buf_tag __env[1], int __val)
                                                      ;
extern void siglongjmp (struct __jmp_buf_tag __env[1], int __val)
                                                      ;
typedef __sig_atomic_t sig_atomic_t;
union sigval
{
  int sival_int;
  void *sival_ptr;
};
typedef union sigval __sigval_t;
typedef struct
  {
    int si_signo;
    int si_errno;
    int si_code;
    int __pad0;
    union
      {
 int _pad[28];
 struct
   {
     __pid_t si_pid;
     __uid_t si_uid;
   } _kill;
 struct
   {
     int si_tid;
     int si_overrun;
     __sigval_t si_sigval;
   } _timer;
 struct
   {
     __pid_t si_pid;
     __uid_t si_uid;
     __sigval_t si_sigval;
   } _rt;
 struct
   {
     __pid_t si_pid;
     __uid_t si_uid;
     int si_status;
     __clock_t si_utime;
     __clock_t si_stime;
   } _sigchld;
 struct
   {
     void *si_addr;
    
     short int si_addr_lsb;
     union
       {
  struct
    {
      void *_lower;
      void *_upper;
    } _addr_bnd;
  __uint32_t _pkey;
       } _bounds;
   } _sigfault;
 struct
   {
     long int si_band;
     int si_fd;
   } _sigpoll;
 struct
   {
     void *_call_addr;
     int _syscall;
     unsigned int _arch;
   } _sigsys;
      } _sifields;
  } siginfo_t ;
enum
{
  SI_ASYNCNL = -60,
  SI_DETHREAD = -7,
  SI_TKILL,
  SI_SIGIO,
  SI_ASYNCIO,
  SI_MESGQ,
  SI_TIMER,
  SI_QUEUE,
  SI_USER,
  SI_KERNEL = 0x80
};
enum
{
  ILL_ILLOPC = 1,
  ILL_ILLOPN,
  ILL_ILLADR,
  ILL_ILLTRP,
  ILL_PRVOPC,
  ILL_PRVREG,
  ILL_COPROC,
  ILL_BADSTK,
  ILL_BADIADDR
};
enum
{
  FPE_INTDIV = 1,
  FPE_INTOVF,
  FPE_FLTDIV,
  FPE_FLTOVF,
  FPE_FLTUND,
  FPE_FLTRES,
  FPE_FLTINV,
  FPE_FLTSUB,
  FPE_FLTUNK = 14,
  FPE_CONDTRAP
};
enum
{
  SEGV_MAPERR = 1,
  SEGV_ACCERR,
  SEGV_BNDERR,
  SEGV_PKUERR,
  SEGV_ACCADI,
  SEGV_ADIDERR,
  SEGV_ADIPERR,
  SEGV_MTEAERR,
  SEGV_MTESERR
};
enum
{
  BUS_ADRALN = 1,
  BUS_ADRERR,
  BUS_OBJERR,
  BUS_MCEERR_AR,
  BUS_MCEERR_AO
};
enum
{
  CLD_EXITED = 1,
  CLD_KILLED,
  CLD_DUMPED,
  CLD_TRAPPED,
  CLD_STOPPED,
  CLD_CONTINUED
};
enum
{
  POLL_IN = 1,
  POLL_OUT,
  POLL_MSG,
  POLL_ERR,
  POLL_PRI,
  POLL_HUP
};
typedef __sigval_t sigval_t;
typedef struct sigevent
  {
    __sigval_t sigev_value;
    int sigev_signo;
    int sigev_notify;
    union
      {
 int _pad[12];
 __pid_t _tid;
 struct
   {
     void (*_function) (__sigval_t);
     pthread_attr_t *_attribute;
   } _sigev_thread;
      } _sigev_un;
  } sigevent_t;
enum
{
  SIGEV_SIGNAL = 0,
  SIGEV_NONE,
  SIGEV_THREAD,
  SIGEV_THREAD_ID = 4
};
typedef void (*__sighandler_t) (int);
extern __sighandler_t __sysv_signal (int __sig, __sighandler_t __handler)
     ;
extern __sighandler_t signal (int __sig, __sighandler_t __handler)
     ;
extern int kill (__pid_t __pid, int __sig) ;
extern int killpg (__pid_t __pgrp, int __sig) ;
extern int raise (int __sig) ;
extern __sighandler_t ssignal (int __sig, __sighandler_t __handler)
     ;
extern int gsignal (int __sig) ;
extern void psignal (int __sig, const char *__s);
extern void psiginfo (const siginfo_t *__pinfo, const char *__s);
extern int sigblock (int __mask) ;
extern int sigsetmask (int __mask) ;
extern int siggetmask (void) ;
typedef __sighandler_t sig_t;
extern int sigemptyset (sigset_t *__set) ;
extern int sigfillset (sigset_t *__set) ;
extern int sigaddset (sigset_t *__set, int __signo) ;
extern int sigdelset (sigset_t *__set, int __signo) ;
extern int sigismember (const sigset_t *__set, int __signo)
     ;
struct sigaction
  {
    union
      {
 __sighandler_t sa_handler;
 void (*sa_sigaction) (int, siginfo_t *, void *);
      }
    __sigaction_handler;
    __sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer) (void);
  };
extern int sigprocmask (int __how, const sigset_t * __set,
   sigset_t * __oset) ;
extern int sigsuspend (const sigset_t *__set) ;
extern int sigaction (int __sig, const struct sigaction * __act,
        struct sigaction * __oact) ;
extern int sigpending (sigset_t *__set) ;
extern int sigwait (const sigset_t * __set, int * __sig)
     ;
extern int sigwaitinfo (const sigset_t * __set,
   siginfo_t * __info) ;
extern int sigtimedwait (const sigset_t * __set,
    siginfo_t * __info,
    const struct timespec * __timeout)
     ;
extern int sigqueue (__pid_t __pid, int __sig, const union sigval __val)
     ;
struct _fpx_sw_bytes
{
  __uint32_t magic1;
  __uint32_t extended_size;
  __uint64_t xstate_bv;
  __uint32_t xstate_size;
  __uint32_t __glibc_reserved1[7];
};
struct _fpreg
{
  unsigned short significand[4];
  unsigned short exponent;
};
struct _fpxreg
{
  unsigned short significand[4];
  unsigned short exponent;
  unsigned short __glibc_reserved1[3];
};
struct _xmmreg
{
  __uint32_t element[4];
};
struct _fpstate
{
  __uint16_t cwd;
  __uint16_t swd;
  __uint16_t ftw;
  __uint16_t fop;
  __uint64_t rip;
  __uint64_t rdp;
  __uint32_t mxcsr;
  __uint32_t mxcr_mask;
  struct _fpxreg _st[8];
  struct _xmmreg _xmm[16];
  __uint32_t __glibc_reserved1[24];
};
struct sigcontext
{
  __uint64_t r8;
  __uint64_t r9;
  __uint64_t r10;
  __uint64_t r11;
  __uint64_t r12;
  __uint64_t r13;
  __uint64_t r14;
  __uint64_t r15;
  __uint64_t rdi;
  __uint64_t rsi;
  __uint64_t rbp;
  __uint64_t rbx;
  __uint64_t rdx;
  __uint64_t rax;
  __uint64_t rcx;
  __uint64_t rsp;
  __uint64_t rip;
  __uint64_t eflags;
  unsigned short cs;
  unsigned short gs;
  unsigned short fs;
  unsigned short __pad0;
  __uint64_t err;
  __uint64_t trapno;
  __uint64_t oldmask;
  __uint64_t cr2;
  union
    {
      struct _fpstate * fpstate;
      __uint64_t __fpstate_word;
    };
  __uint64_t __reserved1 [8];
};
struct _xsave_hdr
{
  __uint64_t xstate_bv;
  __uint64_t __glibc_reserved1[2];
  __uint64_t __glibc_reserved2[5];
};
struct _ymmh_state
{
  __uint32_t ymmh_space[64];
};
struct _xstate
{
  struct _fpstate fpstate;
  struct _xsave_hdr xstate_hdr;
  struct _ymmh_state ymmh;
};
extern int sigreturn (struct sigcontext *__scp) ;
typedef struct
  {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
  } stack_t;
 typedef long long int greg_t;
typedef greg_t gregset_t[23];
struct _libc_fpxreg
{
  unsigned short int significand[4];
  unsigned short int exponent;
  unsigned short int __glibc_reserved1[3];
};
struct _libc_xmmreg
{
  __uint32_t element[4];
};
struct _libc_fpstate
{
  __uint16_t cwd;
  __uint16_t swd;
  __uint16_t ftw;
  __uint16_t fop;
  __uint64_t rip;
  __uint64_t rdp;
  __uint32_t mxcsr;
  __uint32_t mxcr_mask;
  struct _libc_fpxreg _st[8];
  struct _libc_xmmreg _xmm[16];
  __uint32_t __glibc_reserved1[24];
};
typedef struct _libc_fpstate *fpregset_t;
typedef struct
  {
    gregset_t gregs;
    fpregset_t fpregs;
    unsigned long long __reserved1 [8];
} mcontext_t;
typedef struct ucontext_t
  {
    unsigned long int uc_flags;
    struct ucontext_t *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    sigset_t uc_sigmask;
    struct _libc_fpstate __fpregs_mem;
    unsigned long long int __ssp[4];
  } ucontext_t;
extern int siginterrupt (int __sig, int __interrupt)
  ;
enum
{
  SS_ONSTACK = 1,
  SS_DISABLE
};
extern int sigaltstack (const stack_t * __ss,
   stack_t * __oss) ;
struct sigstack
  {
    void *ss_sp;
    int ss_onstack;
  };
extern int sigstack (struct sigstack *__ss, struct sigstack *__oss)
     ;
extern int pthread_sigmask (int __how,
       const __sigset_t * __newmask,
       __sigset_t * __oldmask);
extern int pthread_kill (pthread_t __threadid, int __signo) ;
extern int __libc_current_sigrtmin (void) ;
extern int __libc_current_sigrtmax (void) ;
extern ssize_t readv (int __fd, const struct iovec *__iovec, int __count)
  ;
extern ssize_t writev (int __fd, const struct iovec *__iovec, int __count)
  ;
extern ssize_t preadv (int __fd, const struct iovec *__iovec, int __count,
         __off_t __offset)
  ;
extern ssize_t pwritev (int __fd, const struct iovec *__iovec, int __count,
   __off_t __offset)
  ;
typedef enum
{
  P_ALL,
  P_PID,
  P_PGID
} idtype_t;
extern __pid_t wait (int *__stat_loc);
extern __pid_t waitpid (__pid_t __pid, int *__stat_loc, int __options);
extern int waitid (idtype_t __idtype, __id_t __id, siginfo_t *__infop,
     int __options);
struct rusage;
extern __pid_t wait3 (int *__stat_loc, int __options,
        struct rusage * __usage) ;
extern __pid_t wait4 (__pid_t __pid, int *__stat_loc, int __options,
        struct rusage *__usage) ;
extern int stat (const char * __file,
   struct stat * __buf) ;
extern int fstat (int __fd, struct stat *__buf) ;
extern int fstatat (int __fd, const char * __file,
      struct stat * __buf, int __flag)
     ;
extern int lstat (const char * __file,
    struct stat * __buf) ;
extern int chmod (const char *__file, __mode_t __mode)
     ;
extern int lchmod (const char *__file, __mode_t __mode)
     ;
extern int fchmod (int __fd, __mode_t __mode) ;
extern int fchmodat (int __fd, const char *__file, __mode_t __mode,
       int __flag)
     ;
extern __mode_t umask (__mode_t __mask) ;
extern int mkdir (const char *__path, __mode_t __mode)
     ;
extern int mkdirat (int __fd, const char *__path, __mode_t __mode)
     ;
extern int mknod (const char *__path, __mode_t __mode, __dev_t __dev)
     ;
extern int mknodat (int __fd, const char *__path, __mode_t __mode,
      __dev_t __dev) ;
extern int mkfifo (const char *__path, __mode_t __mode)
     ;
extern int mkfifoat (int __fd, const char *__path, __mode_t __mode)
     ;
extern int utimensat (int __fd, const char *__path,
        const struct timespec __times[2],
        int __flags)
     ;
extern int futimens (int __fd, const struct timespec __times[2]) ;
struct timezone
  {
    int tz_minuteswest;
    int tz_dsttime;
  };
extern int gettimeofday (struct timeval * __tv,
    void * __tz) ;
extern int settimeofday (const struct timeval *__tv,
    const struct timezone *__tz)
     ;
extern int adjtime (const struct timeval *__delta,
      struct timeval *__olddelta) ;
enum __itimer_which
  {
    ITIMER_REAL = 0,
    ITIMER_VIRTUAL = 1,
    ITIMER_PROF = 2
  };
struct itimerval
  {
    struct timeval it_interval;
    struct timeval it_value;
  };
typedef int __itimer_which_t;
extern int getitimer (__itimer_which_t __which,
        struct itimerval *__value) ;
extern int setitimer (__itimer_which_t __which,
        const struct itimerval * __new,
        struct itimerval * __old) ;
extern int utimes (const char *__file, const struct timeval __tvp[2])
     ;
extern int lutimes (const char *__file, const struct timeval __tvp[2])
     ;
extern int futimes (int __fd, const struct timeval __tvp[2]) ;
extern void *mmap (void *__addr, size_t __len, int __prot,
     int __flags, int __fd, __off_t __offset) ;
extern int munmap (void *__addr, size_t __len) ;
extern int mprotect (void *__addr, size_t __len, int __prot) ;
extern int msync (void *__addr, size_t __len, int __flags);
extern int madvise (void *__addr, size_t __len, int __advice) ;
extern int posix_madvise (void *__addr, size_t __len, int __advice) ;
extern int mlock (const void *__addr, size_t __len) ;
extern int munlock (const void *__addr, size_t __len) ;
extern int mlockall (int __flags) ;
extern int munlockall (void) ;
extern int mincore (void *__start, size_t __len, unsigned char *__vec)
     ;
extern int shm_open (const char *__name, int __oflag, mode_t __mode);
extern int shm_unlink (const char *__name);
typedef signed char gint8;
typedef unsigned char guint8;
typedef signed short gint16;
typedef unsigned short guint16;
typedef signed int gint32;
typedef unsigned int guint32;
typedef signed long gint64;
typedef unsigned long guint64;
typedef signed long gssize;
typedef unsigned long gsize;
typedef gint64 goffset;
typedef signed long gintptr;
typedef unsigned long guintptr;
typedef int GPid;
typedef char gchar;
typedef short gshort;
typedef long glong;
typedef int gint;
typedef gint gboolean;
typedef unsigned char guchar;
typedef unsigned short gushort;
typedef unsigned long gulong;
typedef unsigned int guint;
typedef float gfloat;
typedef double gdouble;
typedef void* gpointer;
typedef const void *gconstpointer;
typedef gint (*GCompareFunc) (gconstpointer a,
                                                 gconstpointer b);
typedef gint (*GCompareDataFunc) (gconstpointer a,
                                                 gconstpointer b,
       gpointer user_data);
typedef gboolean (*GEqualFunc) (gconstpointer a,
                                                 gconstpointer b);
typedef void (*GDestroyNotify) (gpointer data);
typedef void (*GFunc) (gpointer data,
                                                 gpointer user_data);
typedef guint (*GHashFunc) (gconstpointer key);
typedef void (*GHFunc) (gpointer key,
                                                 gpointer value,
                                                 gpointer user_data);
typedef gpointer (*GCopyFunc) (gconstpointer src,
                                                 gpointer data);
typedef void (*GFreeFunc) (gpointer data);
typedef const gchar * (*GTranslateFunc) (const gchar *str,
       gpointer data);
typedef union _GDoubleIEEE754 GDoubleIEEE754;
typedef union _GFloatIEEE754 GFloatIEEE754;
union _GFloatIEEE754
{
  gfloat v_float;
  struct {
    guint mantissa : 23;
    guint biased_exponent : 8;
    guint sign : 1;
  } mpn;
};
union _GDoubleIEEE754
{
  gdouble v_double;
  struct {
    guint mantissa_low : 32;
    guint mantissa_high : 20;
    guint biased_exponent : 11;
    guint sign : 1;
  } mpn;
};
typedef struct _GTimeVal GTimeVal ;
struct _GTimeVal
{
  glong tv_sec;
  glong tv_usec;
} ;
typedef gint grefcount;
typedef gint gatomicrefcount;
typedef struct _GBytes GBytes;
typedef struct _GArray GArray;
typedef struct _GByteArray GByteArray;
typedef struct _GPtrArray GPtrArray;
struct _GArray
{
  gchar *data;
  guint len;
};
struct _GByteArray
{
  guint8 *data;
  guint len;
};
struct _GPtrArray
{
  gpointer *pdata;
  guint len;
};
extern
GArray* g_array_new (gboolean zero_terminated,
       gboolean clear_,
       guint element_size);
extern
gpointer g_array_steal (GArray *array,
                                   gsize *len);
extern
GArray* g_array_sized_new (gboolean zero_terminated,
       gboolean clear_,
       guint element_size,
       guint reserved_size);
extern
GArray* g_array_copy (GArray *array);
extern
gchar* g_array_free (GArray *array,
       gboolean free_segment);
extern
GArray *g_array_ref (GArray *array);
extern
void g_array_unref (GArray *array);
extern
guint g_array_get_element_size (GArray *array);
extern
GArray* g_array_append_vals (GArray *array,
       gconstpointer data,
       guint len);
extern
GArray* g_array_prepend_vals (GArray *array,
       gconstpointer data,
       guint len);
extern
GArray* g_array_insert_vals (GArray *array,
       guint index_,
       gconstpointer data,
       guint len);
extern
GArray* g_array_set_size (GArray *array,
       guint length);
extern
GArray* g_array_remove_index (GArray *array,
       guint index_);
extern
GArray* g_array_remove_index_fast (GArray *array,
       guint index_);
extern
GArray* g_array_remove_range (GArray *array,
       guint index_,
       guint length);
extern
void g_array_sort (GArray *array,
       GCompareFunc compare_func);
extern
void g_array_sort_with_data (GArray *array,
       GCompareDataFunc compare_func,
       gpointer user_data);
extern
gboolean g_array_binary_search (GArray *array,
                                   gconstpointer target,
                                   GCompareFunc compare_func,
                                   guint *out_match_index);
extern
void g_array_set_clear_func (GArray *array,
                                   GDestroyNotify clear_func);
extern
GPtrArray* g_ptr_array_new (void);
extern
GPtrArray* g_ptr_array_new_with_free_func (GDestroyNotify element_free_func);
extern
gpointer* g_ptr_array_steal (GPtrArray *array,
                                            gsize *len);
extern
GPtrArray *g_ptr_array_copy (GPtrArray *array,
                                           GCopyFunc func,
                                           gpointer user_data);
extern
GPtrArray* g_ptr_array_sized_new (guint reserved_size);
extern
GPtrArray* g_ptr_array_new_full (guint reserved_size,
        GDestroyNotify element_free_func);
extern
gpointer* g_ptr_array_free (GPtrArray *array,
        gboolean free_seg);
extern
GPtrArray* g_ptr_array_ref (GPtrArray *array);
extern
void g_ptr_array_unref (GPtrArray *array);
extern
void g_ptr_array_set_free_func (GPtrArray *array,
                                           GDestroyNotify element_free_func);
extern
void g_ptr_array_set_size (GPtrArray *array,
        gint length);
extern
gpointer g_ptr_array_remove_index (GPtrArray *array,
        guint index_);
extern
gpointer g_ptr_array_remove_index_fast (GPtrArray *array,
        guint index_);
extern
gpointer g_ptr_array_steal_index (GPtrArray *array,
                                           guint index_);
extern
gpointer g_ptr_array_steal_index_fast (GPtrArray *array,
                                           guint index_);
extern
gboolean g_ptr_array_remove (GPtrArray *array,
        gpointer data);
extern
gboolean g_ptr_array_remove_fast (GPtrArray *array,
        gpointer data);
extern
GPtrArray *g_ptr_array_remove_range (GPtrArray *array,
        guint index_,
        guint length);
extern
void g_ptr_array_add (GPtrArray *array,
        gpointer data);
extern
void g_ptr_array_extend (GPtrArray *array_to_extend,
                                           GPtrArray *array,
                                           GCopyFunc func,
                                           gpointer user_data);
extern
void g_ptr_array_extend_and_steal (GPtrArray *array_to_extend,
                                           GPtrArray *array);
extern
void g_ptr_array_insert (GPtrArray *array,
                                           gint index_,
                                           gpointer data);
extern
void g_ptr_array_sort (GPtrArray *array,
        GCompareFunc compare_func);
extern
void g_ptr_array_sort_with_data (GPtrArray *array,
        GCompareDataFunc compare_func,
        gpointer user_data);
extern
void g_ptr_array_foreach (GPtrArray *array,
        GFunc func,
        gpointer user_data);
extern
gboolean g_ptr_array_find (GPtrArray *haystack,
                                           gconstpointer needle,
                                           guint *index_);
extern
gboolean g_ptr_array_find_with_equal_func (GPtrArray *haystack,
                                             gconstpointer needle,
                                             GEqualFunc equal_func,
                                             guint *index_);
extern
GByteArray* g_byte_array_new (void);
extern
GByteArray* g_byte_array_new_take (guint8 *data,
                                            gsize len);
extern
guint8* g_byte_array_steal (GByteArray *array,
                                            gsize *len);
extern
GByteArray* g_byte_array_sized_new (guint reserved_size);
extern
guint8* g_byte_array_free (GByteArray *array,
         gboolean free_segment);
extern
GBytes* g_byte_array_free_to_bytes (GByteArray *array);
extern
GByteArray *g_byte_array_ref (GByteArray *array);
extern
void g_byte_array_unref (GByteArray *array);
extern
GByteArray* g_byte_array_append (GByteArray *array,
         const guint8 *data,
         guint len);
extern
GByteArray* g_byte_array_prepend (GByteArray *array,
         const guint8 *data,
         guint len);
extern
GByteArray* g_byte_array_set_size (GByteArray *array,
         guint length);
extern
GByteArray* g_byte_array_remove_index (GByteArray *array,
         guint index_);
extern
GByteArray* g_byte_array_remove_index_fast (GByteArray *array,
         guint index_);
extern
GByteArray* g_byte_array_remove_range (GByteArray *array,
         guint index_,
         guint length);
extern
void g_byte_array_sort (GByteArray *array,
         GCompareFunc compare_func);
extern
void g_byte_array_sort_with_data (GByteArray *array,
         GCompareDataFunc compare_func,
         gpointer user_data);
extern
gint g_atomic_int_get (const volatile gint *atomic);
extern
void g_atomic_int_set (volatile gint *atomic,
                                                               gint newval);
extern
void g_atomic_int_inc (volatile gint *atomic);
extern
gboolean g_atomic_int_dec_and_test (volatile gint *atomic);
extern
gboolean g_atomic_int_compare_and_exchange (volatile gint *atomic,
                                                               gint oldval,
                                                               gint newval);
extern
gint g_atomic_int_add (volatile gint *atomic,
                                                               gint val);
extern
guint g_atomic_int_and (volatile guint *atomic,
                                                               guint val);
extern
guint g_atomic_int_or (volatile guint *atomic,
                                                               guint val);
extern
guint g_atomic_int_xor (volatile guint *atomic,
                                                               guint val);
extern
gpointer g_atomic_pointer_get (const volatile void *atomic);
extern
void g_atomic_pointer_set (volatile void *atomic,
                                                               gpointer newval);
extern
gboolean g_atomic_pointer_compare_and_exchange (volatile void *atomic,
                                                               gpointer oldval,
                                                               gpointer newval);
extern
gssize g_atomic_pointer_add (volatile void *atomic,
                                                               gssize val);
extern
gsize g_atomic_pointer_and (volatile void *atomic,
                                                               gsize val);
extern
gsize g_atomic_pointer_or (volatile void *atomic,
                                                               gsize val);
extern
gsize g_atomic_pointer_xor (volatile void *atomic,
                                                               gsize val);
 extern
gint g_atomic_int_exchange_and_add (volatile gint *atomic,
                                                               gint val);
typedef guint32 GQuark;
extern
GQuark g_quark_try_string (const gchar *string);
extern
GQuark g_quark_from_static_string (const gchar *string);
extern
GQuark g_quark_from_string (const gchar *string);
extern
const gchar * g_quark_to_string (GQuark quark) ;
extern
const gchar * g_intern_string (const gchar *string);
extern
const gchar * g_intern_static_string (const gchar *string);
typedef struct _GError GError;
struct _GError
{
  GQuark domain;
  gint code;
  gchar *message;
};
typedef void (*GErrorInitFunc) (GError *error);
typedef void (*GErrorCopyFunc) (const GError *src_error, GError *dest_error);
typedef void (*GErrorClearFunc) (GError *error);
extern
GQuark g_error_domain_register_static (const char *error_type_name,
                                         gsize error_type_private_size,
                                         GErrorInitFunc error_type_init,
                                         GErrorCopyFunc error_type_copy,
                                         GErrorClearFunc error_type_clear);
extern
GQuark g_error_domain_register (const char *error_type_name,
                                  gsize error_type_private_size,
                                  GErrorInitFunc error_type_init,
                                  GErrorCopyFunc error_type_copy,
                                  GErrorClearFunc error_type_clear);
extern
GError* g_error_new (GQuark domain,
                                gint code,
                                const gchar *format,
                                ...) ;
extern
GError* g_error_new_literal (GQuark domain,
                                gint code,
                                const gchar *message);
extern
GError* g_error_new_valist (GQuark domain,
                                gint code,
                                const gchar *format,
                                va_list args) ;
extern
void g_error_free (GError *error);
extern
GError* g_error_copy (const GError *error);
extern
gboolean g_error_matches (const GError *error,
                                GQuark domain,
                                gint code);
extern
void g_set_error (GError **err,
                                GQuark domain,
                                gint code,
                                const gchar *format,
                                ...) ;
extern
void g_set_error_literal (GError **err,
                                GQuark domain,
                                gint code,
                                const gchar *message);
extern
void g_propagate_error (GError **dest,
    GError *src);
extern
void g_clear_error (GError **err);
extern
void g_prefix_error (GError **err,
                                       const gchar *format,
                                       ...) ;
extern
void g_prefix_error_literal (GError **err,
                                       const gchar *prefix);
extern
void g_propagate_prefixed_error (GError **dest,
                                       GError *src,
                                       const gchar *format,
                                       ...) ;
extern
GQuark g_thread_error_quark (void);
typedef enum
{
  G_THREAD_ERROR_AGAIN
} GThreadError;
typedef gpointer (*GThreadFunc) (gpointer data);
typedef struct _GThread GThread;
typedef union _GMutex GMutex;
typedef struct _GRecMutex GRecMutex;
typedef struct _GRWLock GRWLock;
typedef struct _GCond GCond;
typedef struct _GPrivate GPrivate;
typedef struct _GOnce GOnce;
union _GMutex
{
  gpointer p;
  guint i[2];
};
struct _GRWLock
{
  gpointer p;
  guint i[2];
};
struct _GCond
{
  gpointer p;
  guint i[2];
};
struct _GRecMutex
{
  gpointer p;
  guint i[2];
};
struct _GPrivate
{
  gpointer p;
  GDestroyNotify notify;
  gpointer future[2];
};
typedef enum
{
  G_ONCE_STATUS_NOTCALLED,
  G_ONCE_STATUS_PROGRESS,
  G_ONCE_STATUS_READY
} GOnceStatus;
struct _GOnce
{
  volatile GOnceStatus status;
  volatile gpointer retval;
};
extern
GThread * g_thread_ref (GThread *thread);
extern
void g_thread_unref (GThread *thread);
extern
GThread * g_thread_new (const gchar *name,
                                                 GThreadFunc func,
                                                 gpointer data);
extern
GThread * g_thread_try_new (const gchar *name,
                                                 GThreadFunc func,
                                                 gpointer data,
                                                 GError **error);
extern
GThread * g_thread_self (void);
extern
void g_thread_exit (gpointer retval);
extern
gpointer g_thread_join (GThread *thread);
extern
void g_thread_yield (void);
extern
void g_mutex_init (GMutex *mutex);
extern
void g_mutex_clear (GMutex *mutex);
extern
void g_mutex_lock (GMutex *mutex);
extern
gboolean g_mutex_trylock (GMutex *mutex);
extern
void g_mutex_unlock (GMutex *mutex);
extern
void g_rw_lock_init (GRWLock *rw_lock);
extern
void g_rw_lock_clear (GRWLock *rw_lock);
extern
void g_rw_lock_writer_lock (GRWLock *rw_lock);
extern
gboolean g_rw_lock_writer_trylock (GRWLock *rw_lock);
extern
void g_rw_lock_writer_unlock (GRWLock *rw_lock);
extern
void g_rw_lock_reader_lock (GRWLock *rw_lock);
extern
gboolean g_rw_lock_reader_trylock (GRWLock *rw_lock);
extern
void g_rw_lock_reader_unlock (GRWLock *rw_lock);
extern
void g_rec_mutex_init (GRecMutex *rec_mutex);
extern
void g_rec_mutex_clear (GRecMutex *rec_mutex);
extern
void g_rec_mutex_lock (GRecMutex *rec_mutex);
extern
gboolean g_rec_mutex_trylock (GRecMutex *rec_mutex);
extern
void g_rec_mutex_unlock (GRecMutex *rec_mutex);
extern
void g_cond_init (GCond *cond);
extern
void g_cond_clear (GCond *cond);
extern
void g_cond_wait (GCond *cond,
                                                 GMutex *mutex);
extern
void g_cond_signal (GCond *cond);
extern
void g_cond_broadcast (GCond *cond);
extern
gboolean g_cond_wait_until (GCond *cond,
                                                 GMutex *mutex,
                                                 gint64 end_time);
extern
gpointer g_private_get (GPrivate *key);
extern
void g_private_set (GPrivate *key,
                                                 gpointer value);
extern
void g_private_replace (GPrivate *key,
                                                 gpointer value);
extern
gpointer g_once_impl (GOnce *once,
                                                 GThreadFunc func,
                                                 gpointer arg);
extern
gboolean g_once_init_enter (volatile void *location);
extern
void g_once_init_leave (volatile void *location,
                                                 gsize result);
extern
guint g_get_num_processors (void);
typedef void GMutexLocker;
typedef void GRecMutexLocker;
typedef void GRWLockWriterLocker;
typedef void GRWLockReaderLocker;
typedef struct _GAsyncQueue GAsyncQueue;
extern
GAsyncQueue *g_async_queue_new (void);
extern
GAsyncQueue *g_async_queue_new_full (GDestroyNotify item_free_func);
extern
void g_async_queue_lock (GAsyncQueue *queue);
extern
void g_async_queue_unlock (GAsyncQueue *queue);
extern
GAsyncQueue *g_async_queue_ref (GAsyncQueue *queue);
extern
void g_async_queue_unref (GAsyncQueue *queue);
 extern
void g_async_queue_ref_unlocked (GAsyncQueue *queue);
 extern
void g_async_queue_unref_and_unlock (GAsyncQueue *queue);
extern
void g_async_queue_push (GAsyncQueue *queue,
                                                 gpointer data);
extern
void g_async_queue_push_unlocked (GAsyncQueue *queue,
                                                 gpointer data);
extern
void g_async_queue_push_sorted (GAsyncQueue *queue,
                                                 gpointer data,
                                                 GCompareDataFunc func,
                                                 gpointer user_data);
extern
void g_async_queue_push_sorted_unlocked (GAsyncQueue *queue,
                                                 gpointer data,
                                                 GCompareDataFunc func,
                                                 gpointer user_data);
extern
gpointer g_async_queue_pop (GAsyncQueue *queue);
extern
gpointer g_async_queue_pop_unlocked (GAsyncQueue *queue);
extern
gpointer g_async_queue_try_pop (GAsyncQueue *queue);
extern
gpointer g_async_queue_try_pop_unlocked (GAsyncQueue *queue);
extern
gpointer g_async_queue_timeout_pop (GAsyncQueue *queue,
                                                 guint64 timeout);
extern
gpointer g_async_queue_timeout_pop_unlocked (GAsyncQueue *queue,
                                                 guint64 timeout);
extern
gint g_async_queue_length (GAsyncQueue *queue);
extern
gint g_async_queue_length_unlocked (GAsyncQueue *queue);
extern
void g_async_queue_sort (GAsyncQueue *queue,
                                                 GCompareDataFunc func,
                                                 gpointer user_data);
extern
void g_async_queue_sort_unlocked (GAsyncQueue *queue,
                                                 GCompareDataFunc func,
                                                 gpointer user_data);
extern
gboolean g_async_queue_remove (GAsyncQueue *queue,
                                                 gpointer item);
extern
gboolean g_async_queue_remove_unlocked (GAsyncQueue *queue,
                                                 gpointer item);
extern
void g_async_queue_push_front (GAsyncQueue *queue,
                                                 gpointer item);
extern
void g_async_queue_push_front_unlocked (GAsyncQueue *queue,
                                                 gpointer item);
 extern
gpointer g_async_queue_timed_pop (GAsyncQueue *queue,
                                                 GTimeVal *end_time);
 extern
gpointer g_async_queue_timed_pop_unlocked (GAsyncQueue *queue,
                                                 GTimeVal *end_time);
extern
void g_on_error_query (const gchar *prg_name);
extern
void g_on_error_stack_trace (const gchar *prg_name);
extern
gsize g_base64_encode_step (const guchar *in,
                                 gsize len,
                                 gboolean break_lines,
                                 gchar *out,
                                 gint *state,
                                 gint *save);
extern
gsize g_base64_encode_close (gboolean break_lines,
                                 gchar *out,
                                 gint *state,
                                 gint *save);
extern
gchar* g_base64_encode (const guchar *data,
                                 gsize len) ;
extern
gsize g_base64_decode_step (const gchar *in,
                                 gsize len,
                                 guchar *out,
                                 gint *state,
                                 guint *save);
extern
guchar *g_base64_decode (const gchar *text,
                                 gsize *out_len) ;
extern
guchar *g_base64_decode_inplace (gchar *text,
                                 gsize *out_len);
extern
void g_bit_lock (volatile gint *address,
                                           gint lock_bit);
extern
gboolean g_bit_trylock (volatile gint *address,
                                           gint lock_bit);
extern
void g_bit_unlock (volatile gint *address,
                                           gint lock_bit);
extern
void g_pointer_bit_lock (volatile void *address,
                                           gint lock_bit);
extern
gboolean g_pointer_bit_trylock (volatile void *address,
                                           gint lock_bit);
extern
void g_pointer_bit_unlock (volatile void *address,
                                           gint lock_bit);
typedef struct _GTimeZone GTimeZone;
typedef enum
{
  G_TIME_TYPE_STANDARD,
  G_TIME_TYPE_DAYLIGHT,
  G_TIME_TYPE_UNIVERSAL
} GTimeType;
 extern
GTimeZone * g_time_zone_new (const gchar *identifier);
extern
GTimeZone * g_time_zone_new_identifier (const gchar *identifier);
extern
GTimeZone * g_time_zone_new_utc (void);
extern
GTimeZone * g_time_zone_new_local (void);
extern
GTimeZone * g_time_zone_new_offset (gint32 seconds);
extern
GTimeZone * g_time_zone_ref (GTimeZone *tz);
extern
void g_time_zone_unref (GTimeZone *tz);
extern
gint g_time_zone_find_interval (GTimeZone *tz,
                                                                         GTimeType type,
                                                                         gint64 time_);
extern
gint g_time_zone_adjust_time (GTimeZone *tz,
                                                                         GTimeType type,
                                                                         gint64 *time_);
extern
const gchar * g_time_zone_get_abbreviation (GTimeZone *tz,
                                                                         gint interval);
extern
gint32 g_time_zone_get_offset (GTimeZone *tz,
                                                                         gint interval);
extern
gboolean g_time_zone_is_dst (GTimeZone *tz,
                                                                         gint interval);
extern
const gchar * g_time_zone_get_identifier (GTimeZone *tz);
typedef gint64 GTimeSpan;
typedef struct _GDateTime GDateTime;
extern
void g_date_time_unref (GDateTime *datetime);
extern
GDateTime * g_date_time_ref (GDateTime *datetime);
extern
GDateTime * g_date_time_new_now (GTimeZone *tz);
extern
GDateTime * g_date_time_new_now_local (void);
extern
GDateTime * g_date_time_new_now_utc (void);
extern
GDateTime * g_date_time_new_from_unix_local (gint64 t);
extern
GDateTime * g_date_time_new_from_unix_utc (gint64 t);
 extern
GDateTime * g_date_time_new_from_timeval_local (const GTimeVal *tv);
 extern
GDateTime * g_date_time_new_from_timeval_utc (const GTimeVal *tv);
extern
GDateTime * g_date_time_new_from_iso8601 (const gchar *text,
                                                                         GTimeZone *default_tz);
extern
GDateTime * g_date_time_new (GTimeZone *tz,
                                                                         gint year,
                                                                         gint month,
                                                                         gint day,
                                                                         gint hour,
                                                                         gint minute,
                                                                         gdouble seconds);
extern
GDateTime * g_date_time_new_local (gint year,
                                                                         gint month,
                                                                         gint day,
                                                                         gint hour,
                                                                         gint minute,
                                                                         gdouble seconds);
extern
GDateTime * g_date_time_new_utc (gint year,
                                                                         gint month,
                                                                         gint day,
                                                                         gint hour,
                                                                         gint minute,
                                                                         gdouble seconds);
extern
GDateTime * g_date_time_add (GDateTime *datetime,
                                                                         GTimeSpan timespan);
extern
GDateTime * g_date_time_add_years (GDateTime *datetime,
                                                                         gint years);
extern
GDateTime * g_date_time_add_months (GDateTime *datetime,
                                                                         gint months);
extern
GDateTime * g_date_time_add_weeks (GDateTime *datetime,
                                                                         gint weeks);
extern
GDateTime * g_date_time_add_days (GDateTime *datetime,
                                                                         gint days);
extern
GDateTime * g_date_time_add_hours (GDateTime *datetime,
                                                                         gint hours);
extern
GDateTime * g_date_time_add_minutes (GDateTime *datetime,
                                                                         gint minutes);
extern
GDateTime * g_date_time_add_seconds (GDateTime *datetime,
                                                                         gdouble seconds);
extern
GDateTime * g_date_time_add_full (GDateTime *datetime,
                                                                         gint years,
                                                                         gint months,
                                                                         gint days,
                                                                         gint hours,
                                                                         gint minutes,
                                                                         gdouble seconds);
extern
gint g_date_time_compare (gconstpointer dt1,
                                                                         gconstpointer dt2);
extern
GTimeSpan g_date_time_difference (GDateTime *end,
                                                                         GDateTime *begin);
extern
guint g_date_time_hash (gconstpointer datetime);
extern
gboolean g_date_time_equal (gconstpointer dt1,
                                                                         gconstpointer dt2);
extern
void g_date_time_get_ymd (GDateTime *datetime,
                                                                         gint *year,
                                                                         gint *month,
                                                                         gint *day);
extern
gint g_date_time_get_year (GDateTime *datetime);
extern
gint g_date_time_get_month (GDateTime *datetime);
extern
gint g_date_time_get_day_of_month (GDateTime *datetime);
extern
gint g_date_time_get_week_numbering_year (GDateTime *datetime);
extern
gint g_date_time_get_week_of_year (GDateTime *datetime);
extern
gint g_date_time_get_day_of_week (GDateTime *datetime);
extern
gint g_date_time_get_day_of_year (GDateTime *datetime);
extern
gint g_date_time_get_hour (GDateTime *datetime);
extern
gint g_date_time_get_minute (GDateTime *datetime);
extern
gint g_date_time_get_second (GDateTime *datetime);
extern
gint g_date_time_get_microsecond (GDateTime *datetime);
extern
gdouble g_date_time_get_seconds (GDateTime *datetime);
extern
gint64 g_date_time_to_unix (GDateTime *datetime);
 extern
gboolean g_date_time_to_timeval (GDateTime *datetime,
                                                                         GTimeVal *tv);
extern
GTimeSpan g_date_time_get_utc_offset (GDateTime *datetime);
extern
GTimeZone * g_date_time_get_timezone (GDateTime *datetime);
extern
const gchar * g_date_time_get_timezone_abbreviation (GDateTime *datetime);
extern
gboolean g_date_time_is_daylight_savings (GDateTime *datetime);
extern
GDateTime * g_date_time_to_timezone (GDateTime *datetime,
                                                                         GTimeZone *tz);
extern
GDateTime * g_date_time_to_local (GDateTime *datetime);
extern
GDateTime * g_date_time_to_utc (GDateTime *datetime);
extern
gchar * g_date_time_format (GDateTime *datetime,
                                                                         const gchar *format) ;
extern
gchar * g_date_time_format_iso8601 (GDateTime *datetime) ;
typedef enum
{
  G_BOOKMARK_FILE_ERROR_INVALID_URI,
  G_BOOKMARK_FILE_ERROR_INVALID_VALUE,
  G_BOOKMARK_FILE_ERROR_APP_NOT_REGISTERED,
  G_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
  G_BOOKMARK_FILE_ERROR_READ,
  G_BOOKMARK_FILE_ERROR_UNKNOWN_ENCODING,
  G_BOOKMARK_FILE_ERROR_WRITE,
  G_BOOKMARK_FILE_ERROR_FILE_NOT_FOUND
} GBookmarkFileError;
extern
GQuark g_bookmark_file_error_quark (void);
typedef struct _GBookmarkFile GBookmarkFile;
extern
GBookmarkFile *g_bookmark_file_new (void);
extern
void g_bookmark_file_free (GBookmarkFile *bookmark);
extern
gboolean g_bookmark_file_load_from_file (GBookmarkFile *bookmark,
          const gchar *filename,
          GError **error);
extern
gboolean g_bookmark_file_load_from_data (GBookmarkFile *bookmark,
          const gchar *data,
          gsize length,
          GError **error);
extern
gboolean g_bookmark_file_load_from_data_dirs (GBookmarkFile *bookmark,
          const gchar *file,
          gchar **full_path,
          GError **error);
extern
gchar * g_bookmark_file_to_data (GBookmarkFile *bookmark,
          gsize *length,
          GError **error) ;
extern
gboolean g_bookmark_file_to_file (GBookmarkFile *bookmark,
          const gchar *filename,
          GError **error);
extern
void g_bookmark_file_set_title (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *title);
extern
gchar * g_bookmark_file_get_title (GBookmarkFile *bookmark,
          const gchar *uri,
          GError **error) ;
extern
void g_bookmark_file_set_description (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *description);
extern
gchar * g_bookmark_file_get_description (GBookmarkFile *bookmark,
          const gchar *uri,
          GError **error) ;
extern
void g_bookmark_file_set_mime_type (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *mime_type);
extern
gchar * g_bookmark_file_get_mime_type (GBookmarkFile *bookmark,
          const gchar *uri,
          GError **error) ;
extern
void g_bookmark_file_set_groups (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar **groups,
          gsize length);
extern
void g_bookmark_file_add_group (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *group);
extern
gboolean g_bookmark_file_has_group (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *group,
          GError **error);
extern
gchar ** g_bookmark_file_get_groups (GBookmarkFile *bookmark,
          const gchar *uri,
          gsize *length,
          GError **error);
extern
void g_bookmark_file_add_application (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *name,
          const gchar *exec);
extern
gboolean g_bookmark_file_has_application (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *name,
          GError **error);
extern
gchar ** g_bookmark_file_get_applications (GBookmarkFile *bookmark,
          const gchar *uri,
          gsize *length,
          GError **error);
 extern
gboolean g_bookmark_file_set_app_info (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *name,
          const gchar *exec,
          gint count,
          time_t stamp,
          GError **error);
extern
gboolean g_bookmark_file_set_application_info (GBookmarkFile *bookmark,
                                                     const char *uri,
                                                     const char *name,
                                                     const char *exec,
                                                     int count,
                                                     GDateTime *stamp,
                                                     GError **error);
 extern
gboolean g_bookmark_file_get_app_info (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *name,
          gchar **exec,
          guint *count,
          time_t *stamp,
          GError **error);
extern
gboolean g_bookmark_file_get_application_info (GBookmarkFile *bookmark,
                                                     const char *uri,
                                                     const char *name,
                                                     char **exec,
                                                     unsigned int *count,
                                                     GDateTime **stamp,
                                                     GError **error);
extern
void g_bookmark_file_set_is_private (GBookmarkFile *bookmark,
          const gchar *uri,
          gboolean is_private);
extern
gboolean g_bookmark_file_get_is_private (GBookmarkFile *bookmark,
          const gchar *uri,
          GError **error);
extern
void g_bookmark_file_set_icon (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *href,
          const gchar *mime_type);
extern
gboolean g_bookmark_file_get_icon (GBookmarkFile *bookmark,
          const gchar *uri,
          gchar **href,
          gchar **mime_type,
          GError **error);
 extern
void g_bookmark_file_set_added (GBookmarkFile *bookmark,
          const gchar *uri,
          time_t added);
extern
void g_bookmark_file_set_added_date_time (GBookmarkFile *bookmark,
                                                    const char *uri,
                                                    GDateTime *added);
 extern
time_t g_bookmark_file_get_added (GBookmarkFile *bookmark,
          const gchar *uri,
          GError **error);
extern
GDateTime *g_bookmark_file_get_added_date_time (GBookmarkFile *bookmark,
                                                    const char *uri,
                                                    GError **error);
 extern
void g_bookmark_file_set_modified (GBookmarkFile *bookmark,
          const gchar *uri,
          time_t modified);
extern
void g_bookmark_file_set_modified_date_time (GBookmarkFile *bookmark,
                                                       const char *uri,
                                                       GDateTime *modified);
 extern
time_t g_bookmark_file_get_modified (GBookmarkFile *bookmark,
          const gchar *uri,
          GError **error);
extern
GDateTime *g_bookmark_file_get_modified_date_time (GBookmarkFile *bookmark,
                                                       const char *uri,
                                                       GError **error);
 extern
void g_bookmark_file_set_visited (GBookmarkFile *bookmark,
          const gchar *uri,
          time_t visited);
extern
void g_bookmark_file_set_visited_date_time (GBookmarkFile *bookmark,
                                                      const char *uri,
                                                      GDateTime *visited);
 extern
time_t g_bookmark_file_get_visited (GBookmarkFile *bookmark,
          const gchar *uri,
          GError **error);
extern
GDateTime *g_bookmark_file_get_visited_date_time (GBookmarkFile *bookmark,
                                                      const char *uri,
                                                      GError **error);
extern
gboolean g_bookmark_file_has_item (GBookmarkFile *bookmark,
          const gchar *uri);
extern
gint g_bookmark_file_get_size (GBookmarkFile *bookmark);
extern
gchar ** g_bookmark_file_get_uris (GBookmarkFile *bookmark,
          gsize *length);
extern
gboolean g_bookmark_file_remove_group (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *group,
          GError **error);
extern
gboolean g_bookmark_file_remove_application (GBookmarkFile *bookmark,
          const gchar *uri,
          const gchar *name,
          GError **error);
extern
gboolean g_bookmark_file_remove_item (GBookmarkFile *bookmark,
          const gchar *uri,
          GError **error);
extern
gboolean g_bookmark_file_move_item (GBookmarkFile *bookmark,
          const gchar *old_uri,
          const gchar *new_uri,
          GError **error);
extern
GBytes * g_bytes_new (gconstpointer data,
                                                 gsize size);
extern
GBytes * g_bytes_new_take (gpointer data,
                                                 gsize size);
extern
GBytes * g_bytes_new_static (gconstpointer data,
                                                 gsize size);
extern
GBytes * g_bytes_new_with_free_func (gconstpointer data,
                                                 gsize size,
                                                 GDestroyNotify free_func,
                                                 gpointer user_data);
extern
GBytes * g_bytes_new_from_bytes (GBytes *bytes,
                                                 gsize offset,
                                                 gsize length);
extern
gconstpointer g_bytes_get_data (GBytes *bytes,
                                                 gsize *size);
extern
gsize g_bytes_get_size (GBytes *bytes);
extern
GBytes * g_bytes_ref (GBytes *bytes);
extern
void g_bytes_unref (GBytes *bytes);
extern
gpointer g_bytes_unref_to_data (GBytes *bytes,
                                                 gsize *size);
extern
GByteArray * g_bytes_unref_to_array (GBytes *bytes);
extern
guint g_bytes_hash (gconstpointer bytes);
extern
gboolean g_bytes_equal (gconstpointer bytes1,
                                                 gconstpointer bytes2);
extern
gint g_bytes_compare (gconstpointer bytes1,
                                                 gconstpointer bytes2);
extern
gconstpointer g_bytes_get_region (GBytes *bytes,
                                                 gsize element_size,
                                                 gsize offset,
                                                 gsize n_elements);
extern
gboolean g_get_charset (const char **charset);
extern
gchar * g_get_codeset (void);
extern
gboolean g_get_console_charset (const char **charset);
extern
const gchar * const * g_get_language_names (void);
extern
const gchar * const * g_get_language_names_with_category
                                            (const gchar *category_name);
extern
gchar ** g_get_locale_variants (const gchar *locale);
typedef enum {
  G_CHECKSUM_MD5,
  G_CHECKSUM_SHA1,
  G_CHECKSUM_SHA256,
  G_CHECKSUM_SHA512,
  G_CHECKSUM_SHA384
} GChecksumType;
typedef struct _GChecksum GChecksum;
extern
gssize g_checksum_type_get_length (GChecksumType checksum_type);
extern
GChecksum * g_checksum_new (GChecksumType checksum_type);
extern
void g_checksum_reset (GChecksum *checksum);
extern
GChecksum * g_checksum_copy (const GChecksum *checksum);
extern
void g_checksum_free (GChecksum *checksum);
extern
void g_checksum_update (GChecksum *checksum,
                                                     const guchar *data,
                                                     gssize length);
extern
const gchar * g_checksum_get_string (GChecksum *checksum);
extern
void g_checksum_get_digest (GChecksum *checksum,
                                                     guint8 *buffer,
                                                     gsize *digest_len);
extern
gchar *g_compute_checksum_for_data (GChecksumType checksum_type,
                                                     const guchar *data,
                                                     gsize length);
extern
gchar *g_compute_checksum_for_string (GChecksumType checksum_type,
                                                     const gchar *str,
                                                     gssize length);
extern
gchar *g_compute_checksum_for_bytes (GChecksumType checksum_type,
                                                     GBytes *data);
typedef enum
{
  G_CONVERT_ERROR_NO_CONVERSION,
  G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
  G_CONVERT_ERROR_FAILED,
  G_CONVERT_ERROR_PARTIAL_INPUT,
  G_CONVERT_ERROR_BAD_URI,
  G_CONVERT_ERROR_NOT_ABSOLUTE_PATH,
  G_CONVERT_ERROR_NO_MEMORY,
  G_CONVERT_ERROR_EMBEDDED_NUL
} GConvertError;
extern
GQuark g_convert_error_quark (void);
typedef struct _GIConv *GIConv;
extern
GIConv g_iconv_open (const gchar *to_codeset,
         const gchar *from_codeset);
extern
gsize g_iconv (GIConv converter,
         gchar **inbuf,
         gsize *inbytes_left,
         gchar **outbuf,
         gsize *outbytes_left);
extern
gint g_iconv_close (GIConv converter);
extern
gchar* g_convert (const gchar *str,
    gssize len,
    const gchar *to_codeset,
    const gchar *from_codeset,
    gsize *bytes_read,
    gsize *bytes_written,
    GError **error) ;
extern
gchar* g_convert_with_iconv (const gchar *str,
    gssize len,
    GIConv converter,
    gsize *bytes_read,
    gsize *bytes_written,
    GError **error) ;
extern
gchar* g_convert_with_fallback (const gchar *str,
    gssize len,
    const gchar *to_codeset,
    const gchar *from_codeset,
    const gchar *fallback,
    gsize *bytes_read,
    gsize *bytes_written,
    GError **error) ;
extern
gchar* g_locale_to_utf8 (const gchar *opsysstring,
      gssize len,
      gsize *bytes_read,
      gsize *bytes_written,
      GError **error) ;
extern
gchar* g_locale_from_utf8 (const gchar *utf8string,
      gssize len,
      gsize *bytes_read,
      gsize *bytes_written,
      GError **error) ;
extern
gchar* g_filename_to_utf8 (const gchar *opsysstring,
        gssize len,
        gsize *bytes_read,
        gsize *bytes_written,
        GError **error) ;
extern
gchar* g_filename_from_utf8 (const gchar *utf8string,
        gssize len,
        gsize *bytes_read,
        gsize *bytes_written,
        GError **error) ;
extern
gchar *g_filename_from_uri (const gchar *uri,
       gchar **hostname,
       GError **error) ;
extern
gchar *g_filename_to_uri (const gchar *filename,
       const gchar *hostname,
       GError **error) ;
extern
gchar *g_filename_display_name (const gchar *filename) ;
extern
gboolean g_get_filename_charsets (const gchar ***filename_charsets);
extern
gchar *g_filename_display_basename (const gchar *filename) ;
extern
gchar **g_uri_list_extract_uris (const gchar *uri_list);
typedef struct _GData GData;
typedef void (*GDataForeachFunc) (GQuark key_id,
                                                 gpointer data,
                                                 gpointer user_data);
extern
void g_datalist_init (GData **datalist);
extern
void g_datalist_clear (GData **datalist);
extern
gpointer g_datalist_id_get_data (GData **datalist,
      GQuark key_id);
extern
void g_datalist_id_set_data_full (GData **datalist,
      GQuark key_id,
      gpointer data,
      GDestroyNotify destroy_func);
typedef gpointer (*GDuplicateFunc) (gpointer data, gpointer user_data);
extern
gpointer g_datalist_id_dup_data (GData **datalist,
                                         GQuark key_id,
                                         GDuplicateFunc dup_func,
      gpointer user_data);
extern
gboolean g_datalist_id_replace_data (GData **datalist,
                                         GQuark key_id,
                                         gpointer oldval,
                                         gpointer newval,
                                         GDestroyNotify destroy,
      GDestroyNotify *old_destroy);
extern
gpointer g_datalist_id_remove_no_notify (GData **datalist,
      GQuark key_id);
extern
void g_datalist_foreach (GData **datalist,
      GDataForeachFunc func,
      gpointer user_data);
extern
void g_datalist_set_flags (GData **datalist,
      guint flags);
extern
void g_datalist_unset_flags (GData **datalist,
      guint flags);
extern
guint g_datalist_get_flags (GData **datalist);
extern
void g_dataset_destroy (gconstpointer dataset_location);
extern
gpointer g_dataset_id_get_data (gconstpointer dataset_location,
                                         GQuark key_id);
extern
gpointer g_datalist_get_data (GData **datalist,
       const gchar *key);
extern
void g_dataset_id_set_data_full (gconstpointer dataset_location,
                                         GQuark key_id,
                                         gpointer data,
                                         GDestroyNotify destroy_func);
extern
gpointer g_dataset_id_remove_no_notify (gconstpointer dataset_location,
                                         GQuark key_id);
extern
void g_dataset_foreach (gconstpointer dataset_location,
                                         GDataForeachFunc func,
                                         gpointer user_data);
typedef gint32 GTime ;
typedef guint16 GDateYear;
typedef guint8 GDateDay;
typedef struct _GDate GDate;
typedef enum
{
  G_DATE_DAY = 0,
  G_DATE_MONTH = 1,
  G_DATE_YEAR = 2
} GDateDMY;
typedef enum
{
  G_DATE_BAD_WEEKDAY = 0,
  G_DATE_MONDAY = 1,
  G_DATE_TUESDAY = 2,
  G_DATE_WEDNESDAY = 3,
  G_DATE_THURSDAY = 4,
  G_DATE_FRIDAY = 5,
  G_DATE_SATURDAY = 6,
  G_DATE_SUNDAY = 7
} GDateWeekday;
typedef enum
{
  G_DATE_BAD_MONTH = 0,
  G_DATE_JANUARY = 1,
  G_DATE_FEBRUARY = 2,
  G_DATE_MARCH = 3,
  G_DATE_APRIL = 4,
  G_DATE_MAY = 5,
  G_DATE_JUNE = 6,
  G_DATE_JULY = 7,
  G_DATE_AUGUST = 8,
  G_DATE_SEPTEMBER = 9,
  G_DATE_OCTOBER = 10,
  G_DATE_NOVEMBER = 11,
  G_DATE_DECEMBER = 12
} GDateMonth;
struct _GDate
{
  guint julian_days : 32;
  guint julian : 1;
  guint dmy : 1;
  guint day : 6;
  guint month : 4;
  guint year : 16;
};
extern
GDate* g_date_new (void);
extern
GDate* g_date_new_dmy (GDateDay day,
                                           GDateMonth month,
                                           GDateYear year);
extern
GDate* g_date_new_julian (guint32 julian_day);
extern
void g_date_free (GDate *date);
extern
GDate* g_date_copy (const GDate *date);
extern
gboolean g_date_valid (const GDate *date);
extern
gboolean g_date_valid_day (GDateDay day) ;
extern
gboolean g_date_valid_month (GDateMonth month) ;
extern
gboolean g_date_valid_year (GDateYear year) ;
extern
gboolean g_date_valid_weekday (GDateWeekday weekday) ;
extern
gboolean g_date_valid_julian (guint32 julian_date) ;
extern
gboolean g_date_valid_dmy (GDateDay day,
                                           GDateMonth month,
                                           GDateYear year) ;
extern
GDateWeekday g_date_get_weekday (const GDate *date);
extern
GDateMonth g_date_get_month (const GDate *date);
extern
GDateYear g_date_get_year (const GDate *date);
extern
GDateDay g_date_get_day (const GDate *date);
extern
guint32 g_date_get_julian (const GDate *date);
extern
guint g_date_get_day_of_year (const GDate *date);
extern
guint g_date_get_monday_week_of_year (const GDate *date);
extern
guint g_date_get_sunday_week_of_year (const GDate *date);
extern
guint g_date_get_iso8601_week_of_year (const GDate *date);
extern
void g_date_clear (GDate *date,
                                           guint n_dates);
extern
void g_date_set_parse (GDate *date,
                                           const gchar *str);
extern
void g_date_set_time_t (GDate *date,
        time_t timet);
 extern
void g_date_set_time_val (GDate *date,
        GTimeVal *timeval);
 extern
void g_date_set_time (GDate *date,
                                           GTime time_);
extern
void g_date_set_month (GDate *date,
                                           GDateMonth month);
extern
void g_date_set_day (GDate *date,
                                           GDateDay day);
extern
void g_date_set_year (GDate *date,
                                           GDateYear year);
extern
void g_date_set_dmy (GDate *date,
                                           GDateDay day,
                                           GDateMonth month,
                                           GDateYear y);
extern
void g_date_set_julian (GDate *date,
                                           guint32 julian_date);
extern
gboolean g_date_is_first_of_month (const GDate *date);
extern
gboolean g_date_is_last_of_month (const GDate *date);
extern
void g_date_add_days (GDate *date,
                                           guint n_days);
extern
void g_date_subtract_days (GDate *date,
                                           guint n_days);
extern
void g_date_add_months (GDate *date,
                                           guint n_months);
extern
void g_date_subtract_months (GDate *date,
                                           guint n_months);
extern
void g_date_add_years (GDate *date,
                                           guint n_years);
extern
void g_date_subtract_years (GDate *date,
                                           guint n_years);
extern
gboolean g_date_is_leap_year (GDateYear year) ;
extern
guint8 g_date_get_days_in_month (GDateMonth month,
                                           GDateYear year) ;
extern
guint8 g_date_get_monday_weeks_in_year (GDateYear year) ;
extern
guint8 g_date_get_sunday_weeks_in_year (GDateYear year) ;
extern
gint g_date_days_between (const GDate *date1,
        const GDate *date2);
extern
gint g_date_compare (const GDate *lhs,
                                           const GDate *rhs);
extern
void g_date_to_struct_tm (const GDate *date,
                                           struct tm *tm);
extern
void g_date_clamp (GDate *date,
        const GDate *min_date,
        const GDate *max_date);
extern
void g_date_order (GDate *date1, GDate *date2);
extern
gsize g_date_strftime (gchar *s,
                                           gsize slen,
                                           const gchar *format,
                                           const GDate *date);
struct dirent
  {
    __ino_t d_ino;
    __off_t d_off;
    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[256];
  };
enum
  {
    DT_UNKNOWN = 0,
    DT_FIFO = 1,
    DT_CHR = 2,
    DT_DIR = 4,
    DT_BLK = 6,
    DT_REG = 8,
    DT_LNK = 10,
    DT_SOCK = 12,
    DT_WHT = 14
  };
typedef struct __dirstream DIR;
extern DIR *opendir (const char *__name) ;
extern DIR *fdopendir (int __fd);
extern int closedir (DIR *__dirp) ;
extern struct dirent *readdir (DIR *__dirp) ;
extern int readdir_r (DIR * __dirp,
        struct dirent * __entry,
        struct dirent ** __result)
     ;
extern void rewinddir (DIR *__dirp) ;
extern void seekdir (DIR *__dirp, long int __pos) ;
extern long int telldir (DIR *__dirp) ;
extern int dirfd (DIR *__dirp) ;
extern int scandir (const char * __dir,
      struct dirent *** __namelist,
      int (*__selector) (const struct dirent *),
      int (*__cmp) (const struct dirent **,
      const struct dirent **))
     ;
extern int alphasort (const struct dirent **__e1,
        const struct dirent **__e2)
     ;
extern __ssize_t getdirentries (int __fd, char * __buf,
    size_t __nbytes,
    __off_t * __basep)
     ;
typedef struct _GDir GDir;
extern
GDir * g_dir_open (const gchar *path,
            guint flags,
            GError **error);
extern
const gchar * g_dir_read_name (GDir *dir);
extern
void g_dir_rewind (GDir *dir);
extern
void g_dir_close (GDir *dir);
extern
const gchar * g_getenv (const gchar *variable);
extern
gboolean g_setenv (const gchar *variable,
                                  const gchar *value,
                                  gboolean overwrite);
extern
void g_unsetenv (const gchar *variable);
extern
gchar ** g_listenv (void);
extern
gchar ** g_get_environ (void);
extern
const gchar * g_environ_getenv (gchar **envp,
                                  const gchar *variable);
extern
gchar ** g_environ_setenv (gchar **envp,
                                  const gchar *variable,
                                  const gchar *value,
                                  gboolean overwrite) ;
extern
gchar ** g_environ_unsetenv (gchar **envp,
                                  const gchar *variable) ;
typedef enum
{
  G_FILE_ERROR_EXIST,
  G_FILE_ERROR_ISDIR,
  G_FILE_ERROR_ACCES,
  G_FILE_ERROR_NAMETOOLONG,
  G_FILE_ERROR_NOENT,
  G_FILE_ERROR_NOTDIR,
  G_FILE_ERROR_NXIO,
  G_FILE_ERROR_NODEV,
  G_FILE_ERROR_ROFS,
  G_FILE_ERROR_TXTBSY,
  G_FILE_ERROR_FAULT,
  G_FILE_ERROR_LOOP,
  G_FILE_ERROR_NOSPC,
  G_FILE_ERROR_NOMEM,
  G_FILE_ERROR_MFILE,
  G_FILE_ERROR_NFILE,
  G_FILE_ERROR_BADF,
  G_FILE_ERROR_INVAL,
  G_FILE_ERROR_PIPE,
  G_FILE_ERROR_AGAIN,
  G_FILE_ERROR_INTR,
  G_FILE_ERROR_IO,
  G_FILE_ERROR_PERM,
  G_FILE_ERROR_NOSYS,
  G_FILE_ERROR_FAILED
} GFileError;
typedef enum
{
  G_FILE_TEST_IS_REGULAR = 1 << 0,
  G_FILE_TEST_IS_SYMLINK = 1 << 1,
  G_FILE_TEST_IS_DIR = 1 << 2,
  G_FILE_TEST_IS_EXECUTABLE = 1 << 3,
  G_FILE_TEST_EXISTS = 1 << 4
} GFileTest;
typedef enum
{
  G_FILE_SET_CONTENTS_NONE = 0,
  G_FILE_SET_CONTENTS_CONSISTENT = 1 << 0,
  G_FILE_SET_CONTENTS_DURABLE = 1 << 1,
  G_FILE_SET_CONTENTS_ONLY_EXISTING = 1 << 2
} GFileSetContentsFlags
;
extern
GQuark g_file_error_quark (void);
extern
GFileError g_file_error_from_errno (gint err_no);
extern
gboolean g_file_test (const gchar *filename,
                              GFileTest test);
extern
gboolean g_file_get_contents (const gchar *filename,
                              gchar **contents,
                              gsize *length,
                              GError **error);
extern
gboolean g_file_set_contents (const gchar *filename,
                              const gchar *contents,
                              gssize length,
                              GError **error);
extern
gboolean g_file_set_contents_full (const gchar *filename,
                                   const gchar *contents,
                                   gssize length,
                                   GFileSetContentsFlags flags,
                                   int mode,
                                   GError **error);
extern
gchar *g_file_read_link (const gchar *filename,
                              GError **error);
extern
gchar *g_mkdtemp (gchar *tmpl);
extern
gchar *g_mkdtemp_full (gchar *tmpl,
                               gint mode);
extern
gint g_mkstemp (gchar *tmpl);
extern
gint g_mkstemp_full (gchar *tmpl,
                               gint flags,
                               gint mode);
extern
gint g_file_open_tmp (const gchar *tmpl,
                               gchar **name_used,
                               GError **error);
extern
gchar *g_dir_make_tmp (const gchar *tmpl,
                               GError **error);
extern
gchar *g_build_path (const gchar *separator,
                               const gchar *first_element,
                               ...) ;
extern
gchar *g_build_pathv (const gchar *separator,
                               gchar **args) ;
extern
gchar *g_build_filename (const gchar *first_element,
                               ...) ;
extern
gchar *g_build_filenamev (gchar **args) ;
extern
gchar *g_build_filename_valist (const gchar *first_element,
                                  va_list *args) ;
extern
gint g_mkdir_with_parents (const gchar *pathname,
                               gint mode);
extern
gboolean g_path_is_absolute (const gchar *file_name);
extern
const gchar *g_path_skip_root (const gchar *file_name);
 extern
const gchar *g_basename (const gchar *file_name);
extern
gchar *g_get_current_dir (void);
extern
gchar *g_path_get_basename (const gchar *file_name) ;
extern
gchar *g_path_get_dirname (const gchar *file_name) ;
extern
gchar *g_canonicalize_filename (const gchar *filename,
                                const gchar *relative_to) ;
extern
const gchar *g_strip_context (const gchar *msgid,
                              const gchar *msgval) ;
extern
const gchar *g_dgettext (const gchar *domain,
                              const gchar *msgid) ;
extern
const gchar *g_dcgettext (const gchar *domain,
                              const gchar *msgid,
                              gint category) ;
extern
const gchar *g_dngettext (const gchar *domain,
                              const gchar *msgid,
                              const gchar *msgid_plural,
                              gulong n) ;
extern
const gchar *g_dpgettext (const gchar *domain,
                              const gchar *msgctxtid,
                              gsize msgidoffset) ;
extern
const gchar *g_dpgettext2 (const gchar *domain,
                              const gchar *context,
                              const gchar *msgid) ;
typedef struct _GMemVTable GMemVTable;
extern
void g_free (gpointer mem);
extern
void g_clear_pointer (gpointer *pp,
                           GDestroyNotify destroy);
extern
gpointer g_malloc (gsize n_bytes) ;
extern
gpointer g_malloc0 (gsize n_bytes) ;
extern
gpointer g_realloc (gpointer mem,
      gsize n_bytes) ;
extern
gpointer g_try_malloc (gsize n_bytes) ;
extern
gpointer g_try_malloc0 (gsize n_bytes) ;
extern
gpointer g_try_realloc (gpointer mem,
      gsize n_bytes) ;
extern
gpointer g_malloc_n (gsize n_blocks,
      gsize n_block_bytes) ;
extern
gpointer g_malloc0_n (gsize n_blocks,
      gsize n_block_bytes) ;
extern
gpointer g_realloc_n (gpointer mem,
      gsize n_blocks,
      gsize n_block_bytes) ;
extern
gpointer g_try_malloc_n (gsize n_blocks,
      gsize n_block_bytes) ;
extern
gpointer g_try_malloc0_n (gsize n_blocks,
      gsize n_block_bytes) ;
extern
gpointer g_try_realloc_n (gpointer mem,
      gsize n_blocks,
      gsize n_block_bytes) ;
extern
gpointer g_aligned_alloc (gsize n_blocks,
                           gsize n_block_bytes,
                           gsize alignment) ;
extern
gpointer g_aligned_alloc0 (gsize n_blocks,
                           gsize n_block_bytes,
                           gsize alignment) ;
extern
void g_aligned_free (gpointer mem);
struct _GMemVTable {
  gpointer (*malloc) (gsize n_bytes);
  gpointer (*realloc) (gpointer mem,
      gsize n_bytes);
  void (*free) (gpointer mem);
  gpointer (*calloc) (gsize n_blocks,
      gsize n_block_bytes);
  gpointer (*try_malloc) (gsize n_bytes);
  gpointer (*try_realloc) (gpointer mem,
      gsize n_bytes);
};
 extern
void g_mem_set_vtable (GMemVTable *vtable);
 extern
gboolean g_mem_is_system_malloc (void);
extern gboolean g_mem_gc_friendly;
extern GMemVTable *glib_mem_profiler_table;
 extern
void g_mem_profile (void);
typedef struct _GNode GNode;
typedef enum
{
  G_TRAVERSE_LEAVES = 1 << 0,
  G_TRAVERSE_NON_LEAVES = 1 << 1,
  G_TRAVERSE_ALL = G_TRAVERSE_LEAVES | G_TRAVERSE_NON_LEAVES,
  G_TRAVERSE_MASK = 0x03,
  G_TRAVERSE_LEAFS = G_TRAVERSE_LEAVES,
  G_TRAVERSE_NON_LEAFS = G_TRAVERSE_NON_LEAVES
} GTraverseFlags;
typedef enum
{
  G_IN_ORDER,
  G_PRE_ORDER,
  G_POST_ORDER,
  G_LEVEL_ORDER
} GTraverseType;
typedef gboolean (*GNodeTraverseFunc) (GNode *node,
       gpointer data);
typedef void (*GNodeForeachFunc) (GNode *node,
       gpointer data);
struct _GNode
{
  gpointer data;
  GNode *next;
  GNode *prev;
  GNode *parent;
  GNode *children;
};
extern
GNode* g_node_new (gpointer data);
extern
void g_node_destroy (GNode *root);
extern
void g_node_unlink (GNode *node);
extern
GNode* g_node_copy_deep (GNode *node,
     GCopyFunc copy_func,
     gpointer data);
extern
GNode* g_node_copy (GNode *node);
extern
GNode* g_node_insert (GNode *parent,
     gint position,
     GNode *node);
extern
GNode* g_node_insert_before (GNode *parent,
     GNode *sibling,
     GNode *node);
extern
GNode* g_node_insert_after (GNode *parent,
     GNode *sibling,
     GNode *node);
extern
GNode* g_node_prepend (GNode *parent,
     GNode *node);
extern
guint g_node_n_nodes (GNode *root,
     GTraverseFlags flags);
extern
GNode* g_node_get_root (GNode *node);
extern
gboolean g_node_is_ancestor (GNode *node,
     GNode *descendant);
extern
guint g_node_depth (GNode *node);
extern
GNode* g_node_find (GNode *root,
     GTraverseType order,
     GTraverseFlags flags,
     gpointer data);
extern
void g_node_traverse (GNode *root,
     GTraverseType order,
     GTraverseFlags flags,
     gint max_depth,
     GNodeTraverseFunc func,
     gpointer data);
extern
guint g_node_max_height (GNode *root);
extern
void g_node_children_foreach (GNode *node,
      GTraverseFlags flags,
      GNodeForeachFunc func,
      gpointer data);
extern
void g_node_reverse_children (GNode *node);
extern
guint g_node_n_children (GNode *node);
extern
GNode* g_node_nth_child (GNode *node,
      guint n);
extern
GNode* g_node_last_child (GNode *node);
extern
GNode* g_node_find_child (GNode *node,
      GTraverseFlags flags,
      gpointer data);
extern
gint g_node_child_position (GNode *node,
      GNode *child);
extern
gint g_node_child_index (GNode *node,
      gpointer data);
extern
GNode* g_node_first_sibling (GNode *node);
extern
GNode* g_node_last_sibling (GNode *node);
typedef struct _GList GList;
struct _GList
{
  gpointer data;
  GList *next;
  GList *prev;
};
extern
GList* g_list_alloc (void) ;
extern
void g_list_free (GList *list);
extern
void g_list_free_1 (GList *list);
extern
void g_list_free_full (GList *list,
      GDestroyNotify free_func);
extern
GList* g_list_append (GList *list,
      gpointer data) ;
extern
GList* g_list_prepend (GList *list,
      gpointer data) ;
extern
GList* g_list_insert (GList *list,
      gpointer data,
      gint position) ;
extern
GList* g_list_insert_sorted (GList *list,
      gpointer data,
      GCompareFunc func) ;
extern
GList* g_list_insert_sorted_with_data (GList *list,
      gpointer data,
      GCompareDataFunc func,
      gpointer user_data) ;
extern
GList* g_list_insert_before (GList *list,
      GList *sibling,
      gpointer data) ;
extern
GList* g_list_insert_before_link (GList *list,
      GList *sibling,
      GList *link_) ;
extern
GList* g_list_concat (GList *list1,
      GList *list2) ;
extern
GList* g_list_remove (GList *list,
      gconstpointer data) ;
extern
GList* g_list_remove_all (GList *list,
      gconstpointer data) ;
extern
GList* g_list_remove_link (GList *list,
      GList *llink) ;
extern
GList* g_list_delete_link (GList *list,
      GList *link_) ;
extern
GList* g_list_reverse (GList *list) ;
extern
GList* g_list_copy (GList *list) ;
extern
GList* g_list_copy_deep (GList *list,
      GCopyFunc func,
      gpointer user_data) ;
extern
GList* g_list_nth (GList *list,
      guint n);
extern
GList* g_list_nth_prev (GList *list,
      guint n);
extern
GList* g_list_find (GList *list,
      gconstpointer data);
extern
GList* g_list_find_custom (GList *list,
      gconstpointer data,
      GCompareFunc func);
extern
gint g_list_position (GList *list,
      GList *llink);
extern
gint g_list_index (GList *list,
      gconstpointer data);
extern
GList* g_list_last (GList *list);
extern
GList* g_list_first (GList *list);
extern
guint g_list_length (GList *list);
extern
void g_list_foreach (GList *list,
      GFunc func,
      gpointer user_data);
extern
GList* g_list_sort (GList *list,
      GCompareFunc compare_func) ;
extern
GList* g_list_sort_with_data (GList *list,
      GCompareDataFunc compare_func,
      gpointer user_data) ;
extern
gpointer g_list_nth_data (GList *list,
      guint n);
extern
void g_clear_list (GList **list_ptr,
                                         GDestroyNotify destroy);
typedef struct _GHashTable GHashTable;
typedef gboolean (*GHRFunc) (gpointer key,
                               gpointer value,
                               gpointer user_data);
typedef struct _GHashTableIter GHashTableIter;
struct _GHashTableIter
{
  gpointer dummy1;
  gpointer dummy2;
  gpointer dummy3;
  int dummy4;
  gboolean dummy5;
  gpointer dummy6;
};
extern
GHashTable* g_hash_table_new (GHashFunc hash_func,
                                            GEqualFunc key_equal_func);
extern
GHashTable* g_hash_table_new_full (GHashFunc hash_func,
                                            GEqualFunc key_equal_func,
                                            GDestroyNotify key_destroy_func,
                                            GDestroyNotify value_destroy_func);
extern
GHashTable *g_hash_table_new_similar (GHashTable *other_hash_table);
extern
void g_hash_table_destroy (GHashTable *hash_table);
extern
gboolean g_hash_table_insert (GHashTable *hash_table,
                                            gpointer key,
                                            gpointer value);
extern
gboolean g_hash_table_replace (GHashTable *hash_table,
                                            gpointer key,
                                            gpointer value);
extern
gboolean g_hash_table_add (GHashTable *hash_table,
                                            gpointer key);
extern
gboolean g_hash_table_remove (GHashTable *hash_table,
                                            gconstpointer key);
extern
void g_hash_table_remove_all (GHashTable *hash_table);
extern
gboolean g_hash_table_steal (GHashTable *hash_table,
                                            gconstpointer key);
extern
gboolean g_hash_table_steal_extended (GHashTable *hash_table,
                                            gconstpointer lookup_key,
                                            gpointer *stolen_key,
                                            gpointer *stolen_value);
extern
void g_hash_table_steal_all (GHashTable *hash_table);
extern
gpointer g_hash_table_lookup (GHashTable *hash_table,
                                            gconstpointer key);
extern
gboolean g_hash_table_contains (GHashTable *hash_table,
                                            gconstpointer key);
extern
gboolean g_hash_table_lookup_extended (GHashTable *hash_table,
                                            gconstpointer lookup_key,
                                            gpointer *orig_key,
                                            gpointer *value);
extern
void g_hash_table_foreach (GHashTable *hash_table,
                                            GHFunc func,
                                            gpointer user_data);
extern
gpointer g_hash_table_find (GHashTable *hash_table,
                                            GHRFunc predicate,
                                            gpointer user_data);
extern
guint g_hash_table_foreach_remove (GHashTable *hash_table,
                                            GHRFunc func,
                                            gpointer user_data);
extern
guint g_hash_table_foreach_steal (GHashTable *hash_table,
                                            GHRFunc func,
                                            gpointer user_data);
extern
guint g_hash_table_size (GHashTable *hash_table);
extern
GList * g_hash_table_get_keys (GHashTable *hash_table);
extern
GList * g_hash_table_get_values (GHashTable *hash_table);
extern
gpointer * g_hash_table_get_keys_as_array (GHashTable *hash_table,
                                            guint *length);
extern
void g_hash_table_iter_init (GHashTableIter *iter,
                                            GHashTable *hash_table);
extern
gboolean g_hash_table_iter_next (GHashTableIter *iter,
                                            gpointer *key,
                                            gpointer *value);
extern
GHashTable* g_hash_table_iter_get_hash_table (GHashTableIter *iter);
extern
void g_hash_table_iter_remove (GHashTableIter *iter);
extern
void g_hash_table_iter_replace (GHashTableIter *iter,
                                            gpointer value);
extern
void g_hash_table_iter_steal (GHashTableIter *iter);
extern
GHashTable* g_hash_table_ref (GHashTable *hash_table);
extern
void g_hash_table_unref (GHashTable *hash_table);
extern
gboolean g_str_equal (gconstpointer v1,
                         gconstpointer v2);
extern
guint g_str_hash (gconstpointer v);
extern
gboolean g_int_equal (gconstpointer v1,
                         gconstpointer v2);
extern
guint g_int_hash (gconstpointer v);
extern
gboolean g_int64_equal (gconstpointer v1,
                         gconstpointer v2);
extern
guint g_int64_hash (gconstpointer v);
extern
gboolean g_double_equal (gconstpointer v1,
                         gconstpointer v2);
extern
guint g_double_hash (gconstpointer v);
extern
guint g_direct_hash (gconstpointer v) ;
extern
gboolean g_direct_equal (gconstpointer v1,
                         gconstpointer v2) ;
typedef struct _GHmac GHmac;
extern
GHmac * g_hmac_new (GChecksumType digest_type,
                                                     const guchar *key,
                                                     gsize key_len);
extern
GHmac * g_hmac_copy (const GHmac *hmac);
extern
GHmac * g_hmac_ref (GHmac *hmac);
extern
void g_hmac_unref (GHmac *hmac);
extern
void g_hmac_update (GHmac *hmac,
                                                     const guchar *data,
                                                     gssize length);
extern
const gchar * g_hmac_get_string (GHmac *hmac);
extern
void g_hmac_get_digest (GHmac *hmac,
                                                     guint8 *buffer,
                                                     gsize *digest_len);
extern
gchar *g_compute_hmac_for_data (GChecksumType digest_type,
                                                     const guchar *key,
                                                     gsize key_len,
                                                     const guchar *data,
                                                     gsize length);
extern
gchar *g_compute_hmac_for_string (GChecksumType digest_type,
                                                     const guchar *key,
                                                     gsize key_len,
                                                     const gchar *str,
                                                     gssize length);
extern
gchar *g_compute_hmac_for_bytes (GChecksumType digest_type,
                                                     GBytes *key,
                                                     GBytes *data);
typedef struct _GHook GHook;
typedef struct _GHookList GHookList;
typedef gint (*GHookCompareFunc) (GHook *new_hook,
       GHook *sibling);
typedef gboolean (*GHookFindFunc) (GHook *hook,
       gpointer data);
typedef void (*GHookMarshaller) (GHook *hook,
       gpointer marshal_data);
typedef gboolean (*GHookCheckMarshaller) (GHook *hook,
       gpointer marshal_data);
typedef void (*GHookFunc) (gpointer data);
typedef gboolean (*GHookCheckFunc) (gpointer data);
typedef void (*GHookFinalizeFunc) (GHookList *hook_list,
       GHook *hook);
typedef enum
{
  G_HOOK_FLAG_ACTIVE = 1 << 0,
  G_HOOK_FLAG_IN_CALL = 1 << 1,
  G_HOOK_FLAG_MASK = 0x0f
} GHookFlagMask;
struct _GHookList
{
  gulong seq_id;
  guint hook_size : 16;
  guint is_setup : 1;
  GHook *hooks;
  gpointer dummy3;
  GHookFinalizeFunc finalize_hook;
  gpointer dummy[2];
};
struct _GHook
{
  gpointer data;
  GHook *next;
  GHook *prev;
  guint ref_count;
  gulong hook_id;
  guint flags;
  gpointer func;
  GDestroyNotify destroy;
};
extern
void g_hook_list_init (GHookList *hook_list,
      guint hook_size);
extern
void g_hook_list_clear (GHookList *hook_list);
extern
GHook* g_hook_alloc (GHookList *hook_list);
extern
void g_hook_free (GHookList *hook_list,
      GHook *hook);
extern
GHook * g_hook_ref (GHookList *hook_list,
      GHook *hook);
extern
void g_hook_unref (GHookList *hook_list,
      GHook *hook);
extern
gboolean g_hook_destroy (GHookList *hook_list,
      gulong hook_id);
extern
void g_hook_destroy_link (GHookList *hook_list,
      GHook *hook);
extern
void g_hook_prepend (GHookList *hook_list,
      GHook *hook);
extern
void g_hook_insert_before (GHookList *hook_list,
      GHook *sibling,
      GHook *hook);
extern
void g_hook_insert_sorted (GHookList *hook_list,
      GHook *hook,
      GHookCompareFunc func);
extern
GHook* g_hook_get (GHookList *hook_list,
      gulong hook_id);
extern
GHook* g_hook_find (GHookList *hook_list,
      gboolean need_valids,
      GHookFindFunc func,
      gpointer data);
extern
GHook* g_hook_find_data (GHookList *hook_list,
      gboolean need_valids,
      gpointer data);
extern
GHook* g_hook_find_func (GHookList *hook_list,
      gboolean need_valids,
      gpointer func);
extern
GHook* g_hook_find_func_data (GHookList *hook_list,
      gboolean need_valids,
      gpointer func,
      gpointer data);
extern
GHook* g_hook_first_valid (GHookList *hook_list,
      gboolean may_be_in_call);
extern
GHook* g_hook_next_valid (GHookList *hook_list,
      GHook *hook,
      gboolean may_be_in_call);
extern
gint g_hook_compare_ids (GHook *new_hook,
      GHook *sibling);
extern
void g_hook_list_invoke (GHookList *hook_list,
      gboolean may_recurse);
extern
void g_hook_list_invoke_check (GHookList *hook_list,
      gboolean may_recurse);
extern
void g_hook_list_marshal (GHookList *hook_list,
      gboolean may_recurse,
      GHookMarshaller marshaller,
      gpointer marshal_data);
extern
void g_hook_list_marshal_check (GHookList *hook_list,
      gboolean may_recurse,
      GHookCheckMarshaller marshaller,
      gpointer marshal_data);
extern
gboolean g_hostname_is_non_ascii (const gchar *hostname);
extern
gboolean g_hostname_is_ascii_encoded (const gchar *hostname);
extern
gboolean g_hostname_is_ip_address (const gchar *hostname);
extern
gchar *g_hostname_to_ascii (const gchar *hostname);
extern
gchar *g_hostname_to_unicode (const gchar *hostname);
typedef struct _GPollFD GPollFD;
typedef gint (*GPollFunc) (GPollFD *ufds,
                                 guint nfsd,
                                 gint timeout_);
struct _GPollFD
{
  gint fd;
  gushort events;
  gushort revents;
};
extern
gint
g_poll (GPollFD *fds,
 guint nfds,
 gint timeout);
typedef struct _GSList GSList;
struct _GSList
{
  gpointer data;
  GSList *next;
};
extern
GSList* g_slist_alloc (void) ;
extern
void g_slist_free (GSList *list);
extern
void g_slist_free_1 (GSList *list);
extern
void g_slist_free_full (GSList *list,
       GDestroyNotify free_func);
extern
GSList* g_slist_append (GSList *list,
       gpointer data) ;
extern
GSList* g_slist_prepend (GSList *list,
       gpointer data) ;
extern
GSList* g_slist_insert (GSList *list,
       gpointer data,
       gint position) ;
extern
GSList* g_slist_insert_sorted (GSList *list,
       gpointer data,
       GCompareFunc func) ;
extern
GSList* g_slist_insert_sorted_with_data (GSList *list,
       gpointer data,
       GCompareDataFunc func,
       gpointer user_data) ;
extern
GSList* g_slist_insert_before (GSList *slist,
       GSList *sibling,
       gpointer data) ;
extern
GSList* g_slist_concat (GSList *list1,
       GSList *list2) ;
extern
GSList* g_slist_remove (GSList *list,
       gconstpointer data) ;
extern
GSList* g_slist_remove_all (GSList *list,
       gconstpointer data) ;
extern
GSList* g_slist_remove_link (GSList *list,
       GSList *link_) ;
extern
GSList* g_slist_delete_link (GSList *list,
       GSList *link_) ;
extern
GSList* g_slist_reverse (GSList *list) ;
extern
GSList* g_slist_copy (GSList *list) ;
extern
GSList* g_slist_copy_deep (GSList *list,
       GCopyFunc func,
       gpointer user_data) ;
extern
GSList* g_slist_nth (GSList *list,
       guint n);
extern
GSList* g_slist_find (GSList *list,
       gconstpointer data);
extern
GSList* g_slist_find_custom (GSList *list,
       gconstpointer data,
       GCompareFunc func);
extern
gint g_slist_position (GSList *list,
       GSList *llink);
extern
gint g_slist_index (GSList *list,
       gconstpointer data);
extern
GSList* g_slist_last (GSList *list);
extern
guint g_slist_length (GSList *list);
extern
void g_slist_foreach (GSList *list,
       GFunc func,
       gpointer user_data);
extern
GSList* g_slist_sort (GSList *list,
       GCompareFunc compare_func) ;
extern
GSList* g_slist_sort_with_data (GSList *list,
       GCompareDataFunc compare_func,
       gpointer user_data) ;
extern
gpointer g_slist_nth_data (GSList *list,
       guint n);
extern
void g_clear_slist (GSList **slist_ptr,
                                          GDestroyNotify destroy);
typedef enum
{
  G_IO_IN =1,
  G_IO_OUT =4,
  G_IO_PRI =2,
  G_IO_ERR =8,
  G_IO_HUP =16,
  G_IO_NVAL =32
} GIOCondition;
typedef enum
{
  G_MAIN_CONTEXT_FLAGS_NONE = 0,
  G_MAIN_CONTEXT_FLAGS_OWNERLESS_POLLING = 1
} GMainContextFlags;
typedef struct _GMainContext GMainContext;
typedef struct _GMainLoop GMainLoop;
typedef struct _GSource GSource;
typedef struct _GSourcePrivate GSourcePrivate;
typedef struct _GSourceCallbackFuncs GSourceCallbackFuncs;
typedef struct _GSourceFuncs GSourceFuncs;
typedef gboolean (*GSourceFunc) (gpointer user_data);
typedef void (*GChildWatchFunc) (GPid pid,
                                       gint wait_status,
                                       gpointer user_data);
typedef void (*GSourceDisposeFunc) (GSource *source);
struct _GSource
{
  gpointer callback_data;
  GSourceCallbackFuncs *callback_funcs;
  const GSourceFuncs *source_funcs;
  guint ref_count;
  GMainContext *context;
  gint priority;
  guint flags;
  guint source_id;
  GSList *poll_fds;
  GSource *prev;
  GSource *next;
  char *name;
  GSourcePrivate *priv;
};
struct _GSourceCallbackFuncs
{
  void (*ref) (gpointer cb_data);
  void (*unref) (gpointer cb_data);
  void (*get) (gpointer cb_data,
                 GSource *source,
                 GSourceFunc *func,
                 gpointer *data);
};
typedef void (*GSourceDummyMarshal) (void);
struct _GSourceFuncs
{
  gboolean (*prepare) (GSource *source,
                        gint *timeout_);
  gboolean (*check) (GSource *source);
  gboolean (*dispatch) (GSource *source,
                        GSourceFunc callback,
                        gpointer user_data);
  void (*finalize) (GSource *source);
  GSourceFunc closure_callback;
  GSourceDummyMarshal closure_marshal;
};
extern
GMainContext *g_main_context_new (void);
extern
GMainContext *g_main_context_new_with_flags (GMainContextFlags flags);
extern
GMainContext *g_main_context_ref (GMainContext *context);
extern
void g_main_context_unref (GMainContext *context);
extern
GMainContext *g_main_context_default (void);
extern
gboolean g_main_context_iteration (GMainContext *context,
                                        gboolean may_block);
extern
gboolean g_main_context_pending (GMainContext *context);
extern
GSource *g_main_context_find_source_by_id (GMainContext *context,
                                                             guint source_id);
extern
GSource *g_main_context_find_source_by_user_data (GMainContext *context,
                                                             gpointer user_data);
extern
GSource *g_main_context_find_source_by_funcs_user_data (GMainContext *context,
                                                             GSourceFuncs *funcs,
                                                             gpointer user_data);
extern
void g_main_context_wakeup (GMainContext *context);
extern
gboolean g_main_context_acquire (GMainContext *context);
extern
void g_main_context_release (GMainContext *context);
extern
gboolean g_main_context_is_owner (GMainContext *context);
 extern
gboolean g_main_context_wait (GMainContext *context,
                                 GCond *cond,
                                 GMutex *mutex);
extern
gboolean g_main_context_prepare (GMainContext *context,
                                  gint *priority);
extern
gint g_main_context_query (GMainContext *context,
                                  gint max_priority,
                                  gint *timeout_,
                                  GPollFD *fds,
                                  gint n_fds);
extern
gboolean g_main_context_check (GMainContext *context,
                                      gint max_priority,
                                      GPollFD *fds,
                                      gint n_fds);
extern
void g_main_context_dispatch (GMainContext *context);
extern
void g_main_context_set_poll_func (GMainContext *context,
                                       GPollFunc func);
extern
GPollFunc g_main_context_get_poll_func (GMainContext *context);
extern
void g_main_context_add_poll (GMainContext *context,
                                     GPollFD *fd,
                                     gint priority);
extern
void g_main_context_remove_poll (GMainContext *context,
                                     GPollFD *fd);
extern
gint g_main_depth (void);
extern
GSource *g_main_current_source (void);
extern
void g_main_context_push_thread_default (GMainContext *context);
extern
void g_main_context_pop_thread_default (GMainContext *context);
extern
GMainContext *g_main_context_get_thread_default (void);
extern
GMainContext *g_main_context_ref_thread_default (void);
typedef void GMainContextPusher ;
extern
GMainLoop *g_main_loop_new (GMainContext *context,
                                   gboolean is_running);
extern
void g_main_loop_run (GMainLoop *loop);
extern
void g_main_loop_quit (GMainLoop *loop);
extern
GMainLoop *g_main_loop_ref (GMainLoop *loop);
extern
void g_main_loop_unref (GMainLoop *loop);
extern
gboolean g_main_loop_is_running (GMainLoop *loop);
extern
GMainContext *g_main_loop_get_context (GMainLoop *loop);
extern
GSource *g_source_new (GSourceFuncs *source_funcs,
                                   guint struct_size);
extern
void g_source_set_dispose_function (GSource *source,
                                        GSourceDisposeFunc dispose);
extern
GSource *g_source_ref (GSource *source);
extern
void g_source_unref (GSource *source);
extern
guint g_source_attach (GSource *source,
                                   GMainContext *context);
extern
void g_source_destroy (GSource *source);
extern
void g_source_set_priority (GSource *source,
                                   gint priority);
extern
gint g_source_get_priority (GSource *source);
extern
void g_source_set_can_recurse (GSource *source,
                                   gboolean can_recurse);
extern
gboolean g_source_get_can_recurse (GSource *source);
extern
guint g_source_get_id (GSource *source);
extern
GMainContext *g_source_get_context (GSource *source);
extern
void g_source_set_callback (GSource *source,
                                   GSourceFunc func,
                                   gpointer data,
                                   GDestroyNotify notify);
extern
void g_source_set_funcs (GSource *source,
                                   GSourceFuncs *funcs);
extern
gboolean g_source_is_destroyed (GSource *source);
extern
void g_source_set_name (GSource *source,
                                              const char *name);
extern
void g_source_set_static_name (GSource *source,
                                               const char *name);
extern
const char * g_source_get_name (GSource *source);
extern
void g_source_set_name_by_id (guint tag,
                                              const char *name);
extern
void g_source_set_ready_time (GSource *source,
                                              gint64 ready_time);
extern
gint64 g_source_get_ready_time (GSource *source);
extern
gpointer g_source_add_unix_fd (GSource *source,
                                              gint fd,
                                              GIOCondition events);
extern
void g_source_modify_unix_fd (GSource *source,
                                              gpointer tag,
                                              GIOCondition new_events);
extern
void g_source_remove_unix_fd (GSource *source,
                                              gpointer tag);
extern
GIOCondition g_source_query_unix_fd (GSource *source,
                                              gpointer tag);
extern
void g_source_set_callback_indirect (GSource *source,
                                     gpointer callback_data,
                                     GSourceCallbackFuncs *callback_funcs);
extern
void g_source_add_poll (GSource *source,
           GPollFD *fd);
extern
void g_source_remove_poll (GSource *source,
           GPollFD *fd);
extern
void g_source_add_child_source (GSource *source,
           GSource *child_source);
extern
void g_source_remove_child_source (GSource *source,
           GSource *child_source);
 extern
void g_source_get_current_time (GSource *source,
                                    GTimeVal *timeval);
extern
gint64 g_source_get_time (GSource *source);
extern
GSource *g_idle_source_new (void);
extern
GSource *g_child_watch_source_new (GPid pid);
extern
GSource *g_timeout_source_new (guint interval);
extern
GSource *g_timeout_source_new_seconds (guint interval);
 extern
void g_get_current_time (GTimeVal *result);
extern
gint64 g_get_monotonic_time (void);
extern
gint64 g_get_real_time (void);
extern
gboolean g_source_remove (guint tag);
extern
gboolean g_source_remove_by_user_data (gpointer user_data);
extern
gboolean g_source_remove_by_funcs_user_data (GSourceFuncs *funcs,
                                              gpointer user_data);
typedef void (* GClearHandleFunc) (guint handle_id);
extern
void g_clear_handle_id (guint *tag_ptr,
                           GClearHandleFunc clear_func);
extern
guint g_timeout_add_full (gint priority,
                                     guint interval,
                                     GSourceFunc function,
                                     gpointer data,
                                     GDestroyNotify notify);
extern
guint g_timeout_add (guint interval,
                                     GSourceFunc function,
                                     gpointer data);
extern
guint g_timeout_add_seconds_full (gint priority,
                                     guint interval,
                                     GSourceFunc function,
                                     gpointer data,
                                     GDestroyNotify notify);
extern
guint g_timeout_add_seconds (guint interval,
                                     GSourceFunc function,
                                     gpointer data);
extern
guint g_child_watch_add_full (gint priority,
                                     GPid pid,
                                     GChildWatchFunc function,
                                     gpointer data,
                                     GDestroyNotify notify);
extern
guint g_child_watch_add (GPid pid,
                                     GChildWatchFunc function,
                                     gpointer data);
extern
guint g_idle_add (GSourceFunc function,
                                     gpointer data);
extern
guint g_idle_add_full (gint priority,
                                     GSourceFunc function,
                                     gpointer data,
                                     GDestroyNotify notify);
extern
gboolean g_idle_remove_by_data (gpointer data);
extern
void g_main_context_invoke_full (GMainContext *context,
                                     gint priority,
                                     GSourceFunc function,
                                     gpointer data,
                                     GDestroyNotify notify);
extern
void g_main_context_invoke (GMainContext *context,
                                     GSourceFunc function,
                                     gpointer data);
extern GSourceFuncs g_timeout_funcs;
extern GSourceFuncs g_child_watch_funcs;
extern GSourceFuncs g_idle_funcs;
extern GSourceFuncs g_unix_signal_funcs;
extern GSourceFuncs g_unix_fd_source_funcs;
typedef guint32 gunichar;
typedef guint16 gunichar2;
typedef enum
{
  G_UNICODE_CONTROL,
  G_UNICODE_FORMAT,
  G_UNICODE_UNASSIGNED,
  G_UNICODE_PRIVATE_USE,
  G_UNICODE_SURROGATE,
  G_UNICODE_LOWERCASE_LETTER,
  G_UNICODE_MODIFIER_LETTER,
  G_UNICODE_OTHER_LETTER,
  G_UNICODE_TITLECASE_LETTER,
  G_UNICODE_UPPERCASE_LETTER,
  G_UNICODE_SPACING_MARK,
  G_UNICODE_ENCLOSING_MARK,
  G_UNICODE_NON_SPACING_MARK,
  G_UNICODE_DECIMAL_NUMBER,
  G_UNICODE_LETTER_NUMBER,
  G_UNICODE_OTHER_NUMBER,
  G_UNICODE_CONNECT_PUNCTUATION,
  G_UNICODE_DASH_PUNCTUATION,
  G_UNICODE_CLOSE_PUNCTUATION,
  G_UNICODE_FINAL_PUNCTUATION,
  G_UNICODE_INITIAL_PUNCTUATION,
  G_UNICODE_OTHER_PUNCTUATION,
  G_UNICODE_OPEN_PUNCTUATION,
  G_UNICODE_CURRENCY_SYMBOL,
  G_UNICODE_MODIFIER_SYMBOL,
  G_UNICODE_MATH_SYMBOL,
  G_UNICODE_OTHER_SYMBOL,
  G_UNICODE_LINE_SEPARATOR,
  G_UNICODE_PARAGRAPH_SEPARATOR,
  G_UNICODE_SPACE_SEPARATOR
} GUnicodeType;
typedef enum
{
  G_UNICODE_BREAK_MANDATORY,
  G_UNICODE_BREAK_CARRIAGE_RETURN,
  G_UNICODE_BREAK_LINE_FEED,
  G_UNICODE_BREAK_COMBINING_MARK,
  G_UNICODE_BREAK_SURROGATE,
  G_UNICODE_BREAK_ZERO_WIDTH_SPACE,
  G_UNICODE_BREAK_INSEPARABLE,
  G_UNICODE_BREAK_NON_BREAKING_GLUE,
  G_UNICODE_BREAK_CONTINGENT,
  G_UNICODE_BREAK_SPACE,
  G_UNICODE_BREAK_AFTER,
  G_UNICODE_BREAK_BEFORE,
  G_UNICODE_BREAK_BEFORE_AND_AFTER,
  G_UNICODE_BREAK_HYPHEN,
  G_UNICODE_BREAK_NON_STARTER,
  G_UNICODE_BREAK_OPEN_PUNCTUATION,
  G_UNICODE_BREAK_CLOSE_PUNCTUATION,
  G_UNICODE_BREAK_QUOTATION,
  G_UNICODE_BREAK_EXCLAMATION,
  G_UNICODE_BREAK_IDEOGRAPHIC,
  G_UNICODE_BREAK_NUMERIC,
  G_UNICODE_BREAK_INFIX_SEPARATOR,
  G_UNICODE_BREAK_SYMBOL,
  G_UNICODE_BREAK_ALPHABETIC,
  G_UNICODE_BREAK_PREFIX,
  G_UNICODE_BREAK_POSTFIX,
  G_UNICODE_BREAK_COMPLEX_CONTEXT,
  G_UNICODE_BREAK_AMBIGUOUS,
  G_UNICODE_BREAK_UNKNOWN,
  G_UNICODE_BREAK_NEXT_LINE,
  G_UNICODE_BREAK_WORD_JOINER,
  G_UNICODE_BREAK_HANGUL_L_JAMO,
  G_UNICODE_BREAK_HANGUL_V_JAMO,
  G_UNICODE_BREAK_HANGUL_T_JAMO,
  G_UNICODE_BREAK_HANGUL_LV_SYLLABLE,
  G_UNICODE_BREAK_HANGUL_LVT_SYLLABLE,
  G_UNICODE_BREAK_CLOSE_PARANTHESIS,
  G_UNICODE_BREAK_CLOSE_PARENTHESIS = G_UNICODE_BREAK_CLOSE_PARANTHESIS,
  G_UNICODE_BREAK_CONDITIONAL_JAPANESE_STARTER,
  G_UNICODE_BREAK_HEBREW_LETTER,
  G_UNICODE_BREAK_REGIONAL_INDICATOR,
  G_UNICODE_BREAK_EMOJI_BASE,
  G_UNICODE_BREAK_EMOJI_MODIFIER,
  G_UNICODE_BREAK_ZERO_WIDTH_JOINER
} GUnicodeBreakType;
typedef enum
{
  G_UNICODE_SCRIPT_INVALID_CODE = -1,
  G_UNICODE_SCRIPT_COMMON = 0,
  G_UNICODE_SCRIPT_INHERITED,
  G_UNICODE_SCRIPT_ARABIC,
  G_UNICODE_SCRIPT_ARMENIAN,
  G_UNICODE_SCRIPT_BENGALI,
  G_UNICODE_SCRIPT_BOPOMOFO,
  G_UNICODE_SCRIPT_CHEROKEE,
  G_UNICODE_SCRIPT_COPTIC,
  G_UNICODE_SCRIPT_CYRILLIC,
  G_UNICODE_SCRIPT_DESERET,
  G_UNICODE_SCRIPT_DEVANAGARI,
  G_UNICODE_SCRIPT_ETHIOPIC,
  G_UNICODE_SCRIPT_GEORGIAN,
  G_UNICODE_SCRIPT_GOTHIC,
  G_UNICODE_SCRIPT_GREEK,
  G_UNICODE_SCRIPT_GUJARATI,
  G_UNICODE_SCRIPT_GURMUKHI,
  G_UNICODE_SCRIPT_HAN,
  G_UNICODE_SCRIPT_HANGUL,
  G_UNICODE_SCRIPT_HEBREW,
  G_UNICODE_SCRIPT_HIRAGANA,
  G_UNICODE_SCRIPT_KANNADA,
  G_UNICODE_SCRIPT_KATAKANA,
  G_UNICODE_SCRIPT_KHMER,
  G_UNICODE_SCRIPT_LAO,
  G_UNICODE_SCRIPT_LATIN,
  G_UNICODE_SCRIPT_MALAYALAM,
  G_UNICODE_SCRIPT_MONGOLIAN,
  G_UNICODE_SCRIPT_MYANMAR,
  G_UNICODE_SCRIPT_OGHAM,
  G_UNICODE_SCRIPT_OLD_ITALIC,
  G_UNICODE_SCRIPT_ORIYA,
  G_UNICODE_SCRIPT_RUNIC,
  G_UNICODE_SCRIPT_SINHALA,
  G_UNICODE_SCRIPT_SYRIAC,
  G_UNICODE_SCRIPT_TAMIL,
  G_UNICODE_SCRIPT_TELUGU,
  G_UNICODE_SCRIPT_THAANA,
  G_UNICODE_SCRIPT_THAI,
  G_UNICODE_SCRIPT_TIBETAN,
  G_UNICODE_SCRIPT_CANADIAN_ABORIGINAL,
  G_UNICODE_SCRIPT_YI,
  G_UNICODE_SCRIPT_TAGALOG,
  G_UNICODE_SCRIPT_HANUNOO,
  G_UNICODE_SCRIPT_BUHID,
  G_UNICODE_SCRIPT_TAGBANWA,
  G_UNICODE_SCRIPT_BRAILLE,
  G_UNICODE_SCRIPT_CYPRIOT,
  G_UNICODE_SCRIPT_LIMBU,
  G_UNICODE_SCRIPT_OSMANYA,
  G_UNICODE_SCRIPT_SHAVIAN,
  G_UNICODE_SCRIPT_LINEAR_B,
  G_UNICODE_SCRIPT_TAI_LE,
  G_UNICODE_SCRIPT_UGARITIC,
  G_UNICODE_SCRIPT_NEW_TAI_LUE,
  G_UNICODE_SCRIPT_BUGINESE,
  G_UNICODE_SCRIPT_GLAGOLITIC,
  G_UNICODE_SCRIPT_TIFINAGH,
  G_UNICODE_SCRIPT_SYLOTI_NAGRI,
  G_UNICODE_SCRIPT_OLD_PERSIAN,
  G_UNICODE_SCRIPT_KHAROSHTHI,
  G_UNICODE_SCRIPT_UNKNOWN,
  G_UNICODE_SCRIPT_BALINESE,
  G_UNICODE_SCRIPT_CUNEIFORM,
  G_UNICODE_SCRIPT_PHOENICIAN,
  G_UNICODE_SCRIPT_PHAGS_PA,
  G_UNICODE_SCRIPT_NKO,
  G_UNICODE_SCRIPT_KAYAH_LI,
  G_UNICODE_SCRIPT_LEPCHA,
  G_UNICODE_SCRIPT_REJANG,
  G_UNICODE_SCRIPT_SUNDANESE,
  G_UNICODE_SCRIPT_SAURASHTRA,
  G_UNICODE_SCRIPT_CHAM,
  G_UNICODE_SCRIPT_OL_CHIKI,
  G_UNICODE_SCRIPT_VAI,
  G_UNICODE_SCRIPT_CARIAN,
  G_UNICODE_SCRIPT_LYCIAN,
  G_UNICODE_SCRIPT_LYDIAN,
  G_UNICODE_SCRIPT_AVESTAN,
  G_UNICODE_SCRIPT_BAMUM,
  G_UNICODE_SCRIPT_EGYPTIAN_HIEROGLYPHS,
  G_UNICODE_SCRIPT_IMPERIAL_ARAMAIC,
  G_UNICODE_SCRIPT_INSCRIPTIONAL_PAHLAVI,
  G_UNICODE_SCRIPT_INSCRIPTIONAL_PARTHIAN,
  G_UNICODE_SCRIPT_JAVANESE,
  G_UNICODE_SCRIPT_KAITHI,
  G_UNICODE_SCRIPT_LISU,
  G_UNICODE_SCRIPT_MEETEI_MAYEK,
  G_UNICODE_SCRIPT_OLD_SOUTH_ARABIAN,
  G_UNICODE_SCRIPT_OLD_TURKIC,
  G_UNICODE_SCRIPT_SAMARITAN,
  G_UNICODE_SCRIPT_TAI_THAM,
  G_UNICODE_SCRIPT_TAI_VIET,
  G_UNICODE_SCRIPT_BATAK,
  G_UNICODE_SCRIPT_BRAHMI,
  G_UNICODE_SCRIPT_MANDAIC,
  G_UNICODE_SCRIPT_CHAKMA,
  G_UNICODE_SCRIPT_MEROITIC_CURSIVE,
  G_UNICODE_SCRIPT_MEROITIC_HIEROGLYPHS,
  G_UNICODE_SCRIPT_MIAO,
  G_UNICODE_SCRIPT_SHARADA,
  G_UNICODE_SCRIPT_SORA_SOMPENG,
  G_UNICODE_SCRIPT_TAKRI,
  G_UNICODE_SCRIPT_BASSA_VAH,
  G_UNICODE_SCRIPT_CAUCASIAN_ALBANIAN,
  G_UNICODE_SCRIPT_DUPLOYAN,
  G_UNICODE_SCRIPT_ELBASAN,
  G_UNICODE_SCRIPT_GRANTHA,
  G_UNICODE_SCRIPT_KHOJKI,
  G_UNICODE_SCRIPT_KHUDAWADI,
  G_UNICODE_SCRIPT_LINEAR_A,
  G_UNICODE_SCRIPT_MAHAJANI,
  G_UNICODE_SCRIPT_MANICHAEAN,
  G_UNICODE_SCRIPT_MENDE_KIKAKUI,
  G_UNICODE_SCRIPT_MODI,
  G_UNICODE_SCRIPT_MRO,
  G_UNICODE_SCRIPT_NABATAEAN,
  G_UNICODE_SCRIPT_OLD_NORTH_ARABIAN,
  G_UNICODE_SCRIPT_OLD_PERMIC,
  G_UNICODE_SCRIPT_PAHAWH_HMONG,
  G_UNICODE_SCRIPT_PALMYRENE,
  G_UNICODE_SCRIPT_PAU_CIN_HAU,
  G_UNICODE_SCRIPT_PSALTER_PAHLAVI,
  G_UNICODE_SCRIPT_SIDDHAM,
  G_UNICODE_SCRIPT_TIRHUTA,
  G_UNICODE_SCRIPT_WARANG_CITI,
  G_UNICODE_SCRIPT_AHOM,
  G_UNICODE_SCRIPT_ANATOLIAN_HIEROGLYPHS,
  G_UNICODE_SCRIPT_HATRAN,
  G_UNICODE_SCRIPT_MULTANI,
  G_UNICODE_SCRIPT_OLD_HUNGARIAN,
  G_UNICODE_SCRIPT_SIGNWRITING,
  G_UNICODE_SCRIPT_ADLAM,
  G_UNICODE_SCRIPT_BHAIKSUKI,
  G_UNICODE_SCRIPT_MARCHEN,
  G_UNICODE_SCRIPT_NEWA,
  G_UNICODE_SCRIPT_OSAGE,
  G_UNICODE_SCRIPT_TANGUT,
  G_UNICODE_SCRIPT_MASARAM_GONDI,
  G_UNICODE_SCRIPT_NUSHU,
  G_UNICODE_SCRIPT_SOYOMBO,
  G_UNICODE_SCRIPT_ZANABAZAR_SQUARE,
  G_UNICODE_SCRIPT_DOGRA,
  G_UNICODE_SCRIPT_GUNJALA_GONDI,
  G_UNICODE_SCRIPT_HANIFI_ROHINGYA,
  G_UNICODE_SCRIPT_MAKASAR,
  G_UNICODE_SCRIPT_MEDEFAIDRIN,
  G_UNICODE_SCRIPT_OLD_SOGDIAN,
  G_UNICODE_SCRIPT_SOGDIAN,
  G_UNICODE_SCRIPT_ELYMAIC,
  G_UNICODE_SCRIPT_NANDINAGARI,
  G_UNICODE_SCRIPT_NYIAKENG_PUACHUE_HMONG,
  G_UNICODE_SCRIPT_WANCHO,
  G_UNICODE_SCRIPT_CHORASMIAN,
  G_UNICODE_SCRIPT_DIVES_AKURU,
  G_UNICODE_SCRIPT_KHITAN_SMALL_SCRIPT,
  G_UNICODE_SCRIPT_YEZIDI,
  G_UNICODE_SCRIPT_CYPRO_MINOAN,
  G_UNICODE_SCRIPT_OLD_UYGHUR,
  G_UNICODE_SCRIPT_TANGSA,
  G_UNICODE_SCRIPT_TOTO,
  G_UNICODE_SCRIPT_VITHKUQI,
  G_UNICODE_SCRIPT_MATH,
} GUnicodeScript;
extern
guint32 g_unicode_script_to_iso15924 (GUnicodeScript script);
extern
GUnicodeScript g_unicode_script_from_iso15924 (guint32 iso15924);
extern
gboolean g_unichar_isalnum (gunichar c) ;
extern
gboolean g_unichar_isalpha (gunichar c) ;
extern
gboolean g_unichar_iscntrl (gunichar c) ;
extern
gboolean g_unichar_isdigit (gunichar c) ;
extern
gboolean g_unichar_isgraph (gunichar c) ;
extern
gboolean g_unichar_islower (gunichar c) ;
extern
gboolean g_unichar_isprint (gunichar c) ;
extern
gboolean g_unichar_ispunct (gunichar c) ;
extern
gboolean g_unichar_isspace (gunichar c) ;
extern
gboolean g_unichar_isupper (gunichar c) ;
extern
gboolean g_unichar_isxdigit (gunichar c) ;
extern
gboolean g_unichar_istitle (gunichar c) ;
extern
gboolean g_unichar_isdefined (gunichar c) ;
extern
gboolean g_unichar_iswide (gunichar c) ;
extern
gboolean g_unichar_iswide_cjk(gunichar c) ;
extern
gboolean g_unichar_iszerowidth(gunichar c) ;
extern
gboolean g_unichar_ismark (gunichar c) ;
extern
gunichar g_unichar_toupper (gunichar c) ;
extern
gunichar g_unichar_tolower (gunichar c) ;
extern
gunichar g_unichar_totitle (gunichar c) ;
extern
gint g_unichar_digit_value (gunichar c) ;
extern
gint g_unichar_xdigit_value (gunichar c) ;
extern
GUnicodeType g_unichar_type (gunichar c) ;
extern
GUnicodeBreakType g_unichar_break_type (gunichar c) ;
extern
gint g_unichar_combining_class (gunichar uc) ;
extern
gboolean g_unichar_get_mirror_char (gunichar ch,
                                    gunichar *mirrored_ch);
extern
GUnicodeScript g_unichar_get_script (gunichar ch) ;
extern
gboolean g_unichar_validate (gunichar ch) ;
extern
gboolean g_unichar_compose (gunichar a,
                            gunichar b,
                            gunichar *ch);
extern
gboolean g_unichar_decompose (gunichar ch,
                              gunichar *a,
                              gunichar *b);
extern
gsize g_unichar_fully_decompose (gunichar ch,
                                 gboolean compat,
                                 gunichar *result,
                                 gsize result_len);
extern
void g_unicode_canonical_ordering (gunichar *string,
                                   gsize len);
 extern
gunichar *g_unicode_canonical_decomposition (gunichar ch,
                                             gsize *result_len) ;
extern const gchar * const g_utf8_skip;
extern
gunichar g_utf8_get_char (const gchar *p) ;
extern
gunichar g_utf8_get_char_validated (const gchar *p,
                                    gssize max_len) ;
extern
gchar* g_utf8_offset_to_pointer (const gchar *str,
                                   glong offset) ;
extern
glong g_utf8_pointer_to_offset (const gchar *str,
                                   const gchar *pos) ;
extern
gchar* g_utf8_prev_char (const gchar *p) ;
extern
gchar* g_utf8_find_next_char (const gchar *p,
                                   const gchar *end) ;
extern
gchar* g_utf8_find_prev_char (const gchar *str,
                                   const gchar *p) ;
extern
glong g_utf8_strlen (const gchar *p,
                                   gssize max) ;
extern
gchar *g_utf8_substring (const gchar *str,
                                   glong start_pos,
                                   glong end_pos) ;
extern
gchar *g_utf8_strncpy (gchar *dest,
                                   const gchar *src,
                                   gsize n);
extern
gchar* g_utf8_strchr (const gchar *p,
                       gssize len,
                       gunichar c);
extern
gchar* g_utf8_strrchr (const gchar *p,
                       gssize len,
                       gunichar c);
extern
gchar* g_utf8_strreverse (const gchar *str,
                          gssize len);
extern
gunichar2 *g_utf8_to_utf16 (const gchar *str,
                                glong len,
                                glong *items_read,
                                glong *items_written,
                                GError **error) ;
extern
gunichar * g_utf8_to_ucs4 (const gchar *str,
                                glong len,
                                glong *items_read,
                                glong *items_written,
                                GError **error) ;
extern
gunichar * g_utf8_to_ucs4_fast (const gchar *str,
                                glong len,
                                glong *items_written) ;
extern
gunichar * g_utf16_to_ucs4 (const gunichar2 *str,
                                glong len,
                                glong *items_read,
                                glong *items_written,
                                GError **error) ;
extern
gchar* g_utf16_to_utf8 (const gunichar2 *str,
                                glong len,
                                glong *items_read,
                                glong *items_written,
                                GError **error) ;
extern
gunichar2 *g_ucs4_to_utf16 (const gunichar *str,
                                glong len,
                                glong *items_read,
                                glong *items_written,
                                GError **error) ;
extern
gchar* g_ucs4_to_utf8 (const gunichar *str,
                                glong len,
                                glong *items_read,
                                glong *items_written,
                                GError **error) ;
extern
gint g_unichar_to_utf8 (gunichar c,
                             gchar *outbuf);
extern
gboolean g_utf8_validate (const gchar *str,
                          gssize max_len,
                          const gchar **end);
extern
gboolean g_utf8_validate_len (const gchar *str,
                              gsize max_len,
                              const gchar **end);
extern
gchar *g_utf8_strup (const gchar *str,
                       gssize len) ;
extern
gchar *g_utf8_strdown (const gchar *str,
                       gssize len) ;
extern
gchar *g_utf8_casefold (const gchar *str,
                        gssize len) ;
typedef enum {
  G_NORMALIZE_DEFAULT,
  G_NORMALIZE_NFD = G_NORMALIZE_DEFAULT,
  G_NORMALIZE_DEFAULT_COMPOSE,
  G_NORMALIZE_NFC = G_NORMALIZE_DEFAULT_COMPOSE,
  G_NORMALIZE_ALL,
  G_NORMALIZE_NFKD = G_NORMALIZE_ALL,
  G_NORMALIZE_ALL_COMPOSE,
  G_NORMALIZE_NFKC = G_NORMALIZE_ALL_COMPOSE
} GNormalizeMode;
extern
gchar *g_utf8_normalize (const gchar *str,
                         gssize len,
                         GNormalizeMode mode) ;
extern
gint g_utf8_collate (const gchar *str1,
                           const gchar *str2) ;
extern
gchar *g_utf8_collate_key (const gchar *str,
                           gssize len) ;
extern
gchar *g_utf8_collate_key_for_filename (const gchar *str,
                                        gssize len) ;
extern
gchar *g_utf8_make_valid (const gchar *str,
                          gssize len) ;
typedef struct _GString GString;
struct _GString
{
  gchar *str;
  gsize len;
  gsize allocated_len;
};
extern
GString* g_string_new (const gchar *init);
extern
GString* g_string_new_len (const gchar *init,
                                         gssize len);
extern
GString* g_string_sized_new (gsize dfl_size);
extern
gchar* g_string_free (GString *string,
                                         gboolean free_segment);
extern
GBytes* g_string_free_to_bytes (GString *string);
extern
gboolean g_string_equal (const GString *v,
                                         const GString *v2);
extern
guint g_string_hash (const GString *str);
extern
GString* g_string_assign (GString *string,
                                         const gchar *rval);
extern
GString* g_string_truncate (GString *string,
                                         gsize len);
extern
GString* g_string_set_size (GString *string,
                                         gsize len);
extern
GString* g_string_insert_len (GString *string,
                                         gssize pos,
                                         const gchar *val,
                                         gssize len);
extern
GString* g_string_append (GString *string,
                                         const gchar *val);
extern
GString* g_string_append_len (GString *string,
                                         const gchar *val,
                                         gssize len);
extern
GString* g_string_append_c (GString *string,
                                         gchar c);
extern
GString* g_string_append_unichar (GString *string,
                                         gunichar wc);
extern
GString* g_string_prepend (GString *string,
                                         const gchar *val);
extern
GString* g_string_prepend_c (GString *string,
                                         gchar c);
extern
GString* g_string_prepend_unichar (GString *string,
                                         gunichar wc);
extern
GString* g_string_prepend_len (GString *string,
                                         const gchar *val,
                                         gssize len);
extern
GString* g_string_insert (GString *string,
                                         gssize pos,
                                         const gchar *val);
extern
GString* g_string_insert_c (GString *string,
                                         gssize pos,
                                         gchar c);
extern
GString* g_string_insert_unichar (GString *string,
                                         gssize pos,
                                         gunichar wc);
extern
GString* g_string_overwrite (GString *string,
                                         gsize pos,
                                         const gchar *val);
extern
GString* g_string_overwrite_len (GString *string,
                                         gsize pos,
                                         const gchar *val,
                                         gssize len);
extern
GString* g_string_erase (GString *string,
                                         gssize pos,
                                         gssize len);
extern
guint g_string_replace (GString *string,
                                         const gchar *find,
                                         const gchar *replace,
                                         guint limit);
extern
GString* g_string_ascii_down (GString *string);
extern
GString* g_string_ascii_up (GString *string);
extern
void g_string_vprintf (GString *string,
                                         const gchar *format,
                                         va_list args)
                                         ;
extern
void g_string_printf (GString *string,
                                         const gchar *format,
                                         ...) ;
extern
void g_string_append_vprintf (GString *string,
                                         const gchar *format,
                                         va_list args)
                                         ;
extern
void g_string_append_printf (GString *string,
                                         const gchar *format,
                                         ...) ;
extern
GString* g_string_append_uri_escaped (GString *string,
                                          const gchar *unescaped,
                                          const gchar *reserved_chars_allowed,
                                          gboolean allow_utf8);
 extern
GString *g_string_down (GString *string);
 extern
GString *g_string_up (GString *string);
typedef struct _GIOChannel GIOChannel;
typedef struct _GIOFuncs GIOFuncs;
typedef enum
{
  G_IO_ERROR_NONE,
  G_IO_ERROR_AGAIN,
  G_IO_ERROR_INVAL,
  G_IO_ERROR_UNKNOWN
} GIOError;
typedef enum
{
  G_IO_CHANNEL_ERROR_FBIG,
  G_IO_CHANNEL_ERROR_INVAL,
  G_IO_CHANNEL_ERROR_IO,
  G_IO_CHANNEL_ERROR_ISDIR,
  G_IO_CHANNEL_ERROR_NOSPC,
  G_IO_CHANNEL_ERROR_NXIO,
  G_IO_CHANNEL_ERROR_OVERFLOW,
  G_IO_CHANNEL_ERROR_PIPE,
  G_IO_CHANNEL_ERROR_FAILED
} GIOChannelError;
typedef enum
{
  G_IO_STATUS_ERROR,
  G_IO_STATUS_NORMAL,
  G_IO_STATUS_EOF,
  G_IO_STATUS_AGAIN
} GIOStatus;
typedef enum
{
  G_SEEK_CUR,
  G_SEEK_SET,
  G_SEEK_END
} GSeekType;
typedef enum
{
  G_IO_FLAG_APPEND = 1 << 0,
  G_IO_FLAG_NONBLOCK = 1 << 1,
  G_IO_FLAG_IS_READABLE = 1 << 2,
  G_IO_FLAG_IS_WRITABLE = 1 << 3,
  G_IO_FLAG_IS_WRITEABLE = 1 << 3,
  G_IO_FLAG_IS_SEEKABLE = 1 << 4,
  G_IO_FLAG_MASK = (1 << 5) - 1,
  G_IO_FLAG_GET_MASK = G_IO_FLAG_MASK,
  G_IO_FLAG_SET_MASK = G_IO_FLAG_APPEND | G_IO_FLAG_NONBLOCK
} GIOFlags;
struct _GIOChannel
{
  gint ref_count;
  GIOFuncs *funcs;
  gchar *encoding;
  GIConv read_cd;
  GIConv write_cd;
  gchar *line_term;
  guint line_term_len;
  gsize buf_size;
  GString *read_buf;
  GString *encoded_read_buf;
  GString *write_buf;
  gchar partial_write_buf[6];
  guint use_buffer : 1;
  guint do_encode : 1;
  guint close_on_unref : 1;
  guint is_readable : 1;
  guint is_writeable : 1;
  guint is_seekable : 1;
  gpointer reserved1;
  gpointer reserved2;
};
typedef gboolean (*GIOFunc) (GIOChannel *source,
        GIOCondition condition,
        gpointer data);
struct _GIOFuncs
{
  GIOStatus (*io_read) (GIOChannel *channel,
             gchar *buf,
      gsize count,
      gsize *bytes_read,
      GError **err);
  GIOStatus (*io_write) (GIOChannel *channel,
      const gchar *buf,
      gsize count,
      gsize *bytes_written,
      GError **err);
  GIOStatus (*io_seek) (GIOChannel *channel,
      gint64 offset,
      GSeekType type,
      GError **err);
  GIOStatus (*io_close) (GIOChannel *channel,
      GError **err);
  GSource* (*io_create_watch) (GIOChannel *channel,
      GIOCondition condition);
  void (*io_free) (GIOChannel *channel);
  GIOStatus (*io_set_flags) (GIOChannel *channel,
                                  GIOFlags flags,
      GError **err);
  GIOFlags (*io_get_flags) (GIOChannel *channel);
};
extern
void g_io_channel_init (GIOChannel *channel);
extern
GIOChannel *g_io_channel_ref (GIOChannel *channel);
extern
void g_io_channel_unref (GIOChannel *channel);
 extern
GIOError g_io_channel_read (GIOChannel *channel,
                                 gchar *buf,
                                 gsize count,
                                 gsize *bytes_read);
 extern
GIOError g_io_channel_write (GIOChannel *channel,
                                 const gchar *buf,
                                 gsize count,
                                 gsize *bytes_written);
 extern
GIOError g_io_channel_seek (GIOChannel *channel,
                                 gint64 offset,
                                 GSeekType type);
 extern
void g_io_channel_close (GIOChannel *channel);
extern
GIOStatus g_io_channel_shutdown (GIOChannel *channel,
     gboolean flush,
     GError **err);
extern
guint g_io_add_watch_full (GIOChannel *channel,
     gint priority,
     GIOCondition condition,
     GIOFunc func,
     gpointer user_data,
     GDestroyNotify notify);
extern
GSource * g_io_create_watch (GIOChannel *channel,
     GIOCondition condition);
extern
guint g_io_add_watch (GIOChannel *channel,
     GIOCondition condition,
     GIOFunc func,
     gpointer user_data);
extern
void g_io_channel_set_buffer_size (GIOChannel *channel,
        gsize size);
extern
gsize g_io_channel_get_buffer_size (GIOChannel *channel);
extern
GIOCondition g_io_channel_get_buffer_condition (GIOChannel *channel);
extern
GIOStatus g_io_channel_set_flags (GIOChannel *channel,
        GIOFlags flags,
        GError **error);
extern
GIOFlags g_io_channel_get_flags (GIOChannel *channel);
extern
void g_io_channel_set_line_term (GIOChannel *channel,
        const gchar *line_term,
        gint length);
extern
const gchar * g_io_channel_get_line_term (GIOChannel *channel,
        gint *length);
extern
void g_io_channel_set_buffered (GIOChannel *channel,
        gboolean buffered);
extern
gboolean g_io_channel_get_buffered (GIOChannel *channel);
extern
GIOStatus g_io_channel_set_encoding (GIOChannel *channel,
        const gchar *encoding,
        GError **error);
extern
const gchar * g_io_channel_get_encoding (GIOChannel *channel);
extern
void g_io_channel_set_close_on_unref (GIOChannel *channel,
        gboolean do_close);
extern
gboolean g_io_channel_get_close_on_unref (GIOChannel *channel);
extern
GIOStatus g_io_channel_flush (GIOChannel *channel,
        GError **error);
extern
GIOStatus g_io_channel_read_line (GIOChannel *channel,
        gchar **str_return,
        gsize *length,
        gsize *terminator_pos,
        GError **error);
extern
GIOStatus g_io_channel_read_line_string (GIOChannel *channel,
        GString *buffer,
        gsize *terminator_pos,
        GError **error);
extern
GIOStatus g_io_channel_read_to_end (GIOChannel *channel,
        gchar **str_return,
        gsize *length,
        GError **error);
extern
GIOStatus g_io_channel_read_chars (GIOChannel *channel,
        gchar *buf,
        gsize count,
        gsize *bytes_read,
        GError **error);
extern
GIOStatus g_io_channel_read_unichar (GIOChannel *channel,
        gunichar *thechar,
        GError **error);
extern
GIOStatus g_io_channel_write_chars (GIOChannel *channel,
        const gchar *buf,
        gssize count,
        gsize *bytes_written,
        GError **error);
extern
GIOStatus g_io_channel_write_unichar (GIOChannel *channel,
        gunichar thechar,
        GError **error);
extern
GIOStatus g_io_channel_seek_position (GIOChannel *channel,
        gint64 offset,
        GSeekType type,
        GError **error);
extern
GIOChannel* g_io_channel_new_file (const gchar *filename,
        const gchar *mode,
        GError **error);
extern
GQuark g_io_channel_error_quark (void);
extern
GIOChannelError g_io_channel_error_from_errno (gint en);
extern
GIOChannel* g_io_channel_unix_new (int fd);
extern
gint g_io_channel_unix_get_fd (GIOChannel *channel);
extern GSourceFuncs g_io_watch_funcs;
typedef enum
{
  G_KEY_FILE_ERROR_UNKNOWN_ENCODING,
  G_KEY_FILE_ERROR_PARSE,
  G_KEY_FILE_ERROR_NOT_FOUND,
  G_KEY_FILE_ERROR_KEY_NOT_FOUND,
  G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
  G_KEY_FILE_ERROR_INVALID_VALUE
} GKeyFileError;
extern
GQuark g_key_file_error_quark (void);
typedef struct _GKeyFile GKeyFile;
typedef enum
{
  G_KEY_FILE_NONE = 0,
  G_KEY_FILE_KEEP_COMMENTS = 1 << 0,
  G_KEY_FILE_KEEP_TRANSLATIONS = 1 << 1
} GKeyFileFlags;
extern
GKeyFile *g_key_file_new (void);
extern
GKeyFile *g_key_file_ref (GKeyFile *key_file);
extern
void g_key_file_unref (GKeyFile *key_file);
extern
void g_key_file_free (GKeyFile *key_file);
extern
void g_key_file_set_list_separator (GKeyFile *key_file,
          gchar separator);
extern
gboolean g_key_file_load_from_file (GKeyFile *key_file,
          const gchar *file,
          GKeyFileFlags flags,
          GError **error);
extern
gboolean g_key_file_load_from_data (GKeyFile *key_file,
          const gchar *data,
          gsize length,
          GKeyFileFlags flags,
          GError **error);
extern
gboolean g_key_file_load_from_bytes (GKeyFile *key_file,
                                             GBytes *bytes,
                                             GKeyFileFlags flags,
                                             GError **error);
extern
gboolean g_key_file_load_from_dirs (GKeyFile *key_file,
          const gchar *file,
          const gchar **search_dirs,
          gchar **full_path,
          GKeyFileFlags flags,
          GError **error);
extern
gboolean g_key_file_load_from_data_dirs (GKeyFile *key_file,
          const gchar *file,
          gchar **full_path,
          GKeyFileFlags flags,
          GError **error);
extern
gchar *g_key_file_to_data (GKeyFile *key_file,
          gsize *length,
          GError **error) ;
extern
gboolean g_key_file_save_to_file (GKeyFile *key_file,
                                             const gchar *filename,
                                             GError **error);
extern
gchar *g_key_file_get_start_group (GKeyFile *key_file) ;
extern
gchar **g_key_file_get_groups (GKeyFile *key_file,
          gsize *length);
extern
gchar **g_key_file_get_keys (GKeyFile *key_file,
          const gchar *group_name,
          gsize *length,
          GError **error);
extern
gboolean g_key_file_has_group (GKeyFile *key_file,
          const gchar *group_name);
extern
gboolean g_key_file_has_key (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          GError **error);
extern
gchar *g_key_file_get_value (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          GError **error) ;
extern
void g_key_file_set_value (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          const gchar *value);
extern
gchar *g_key_file_get_string (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          GError **error) ;
extern
void g_key_file_set_string (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          const gchar *string);
extern
gchar *g_key_file_get_locale_string (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          const gchar *locale,
          GError **error) ;
extern
gchar *g_key_file_get_locale_for_key (GKeyFile *key_file,
                                             const gchar *group_name,
                                             const gchar *key,
                                             const gchar *locale) ;
extern
void g_key_file_set_locale_string (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          const gchar *locale,
          const gchar *string);
extern
gboolean g_key_file_get_boolean (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          GError **error);
extern
void g_key_file_set_boolean (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          gboolean value);
extern
gint g_key_file_get_integer (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          GError **error);
extern
void g_key_file_set_integer (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          gint value);
extern
gint64 g_key_file_get_int64 (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          GError **error);
extern
void g_key_file_set_int64 (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          gint64 value);
extern
guint64 g_key_file_get_uint64 (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          GError **error);
extern
void g_key_file_set_uint64 (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          guint64 value);
extern
gdouble g_key_file_get_double (GKeyFile *key_file,
                                             const gchar *group_name,
                                             const gchar *key,
                                             GError **error);
extern
void g_key_file_set_double (GKeyFile *key_file,
                                             const gchar *group_name,
                                             const gchar *key,
                                             gdouble value);
extern
gchar **g_key_file_get_string_list (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          gsize *length,
          GError **error);
extern
void g_key_file_set_string_list (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          const gchar * const list[],
          gsize length);
extern
gchar **g_key_file_get_locale_string_list (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          const gchar *locale,
          gsize *length,
          GError **error);
extern
void g_key_file_set_locale_string_list (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          const gchar *locale,
          const gchar * const list[],
          gsize length);
extern
gboolean *g_key_file_get_boolean_list (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          gsize *length,
          GError **error) ;
extern
void g_key_file_set_boolean_list (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          gboolean list[],
          gsize length);
extern
gint *g_key_file_get_integer_list (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          gsize *length,
          GError **error) ;
extern
void g_key_file_set_double_list (GKeyFile *key_file,
                                             const gchar *group_name,
                                             const gchar *key,
                                             gdouble list[],
                                             gsize length);
extern
gdouble *g_key_file_get_double_list (GKeyFile *key_file,
                                             const gchar *group_name,
                                             const gchar *key,
                                             gsize *length,
                                             GError **error) ;
extern
void g_key_file_set_integer_list (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          gint list[],
          gsize length);
extern
gboolean g_key_file_set_comment (GKeyFile *key_file,
                                             const gchar *group_name,
                                             const gchar *key,
                                             const gchar *comment,
                                             GError **error);
extern
gchar *g_key_file_get_comment (GKeyFile *key_file,
                                             const gchar *group_name,
                                             const gchar *key,
                                             GError **error) ;
extern
gboolean g_key_file_remove_comment (GKeyFile *key_file,
                                             const gchar *group_name,
                                             const gchar *key,
          GError **error);
extern
gboolean g_key_file_remove_key (GKeyFile *key_file,
          const gchar *group_name,
          const gchar *key,
          GError **error);
extern
gboolean g_key_file_remove_group (GKeyFile *key_file,
          const gchar *group_name,
          GError **error);
typedef struct _GMappedFile GMappedFile;
extern
GMappedFile *g_mapped_file_new (const gchar *filename,
             gboolean writable,
             GError **error);
extern
GMappedFile *g_mapped_file_new_from_fd (gint fd,
      gboolean writable,
      GError **error);
extern
gsize g_mapped_file_get_length (GMappedFile *file);
extern
gchar *g_mapped_file_get_contents (GMappedFile *file);
extern
GBytes * g_mapped_file_get_bytes (GMappedFile *file);
extern
GMappedFile *g_mapped_file_ref (GMappedFile *file);
extern
void g_mapped_file_unref (GMappedFile *file);
 extern
void g_mapped_file_free (GMappedFile *file);
typedef enum
{
  G_MARKUP_ERROR_BAD_UTF8,
  G_MARKUP_ERROR_EMPTY,
  G_MARKUP_ERROR_PARSE,
  G_MARKUP_ERROR_UNKNOWN_ELEMENT,
  G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
  G_MARKUP_ERROR_INVALID_CONTENT,
  G_MARKUP_ERROR_MISSING_ATTRIBUTE
} GMarkupError;
extern
GQuark g_markup_error_quark (void);
typedef enum
{
  G_MARKUP_DO_NOT_USE_THIS_UNSUPPORTED_FLAG = 1 << 0,
  G_MARKUP_TREAT_CDATA_AS_TEXT = 1 << 1,
  G_MARKUP_PREFIX_ERROR_POSITION = 1 << 2,
  G_MARKUP_IGNORE_QUALIFIED = 1 << 3
} GMarkupParseFlags;
typedef struct _GMarkupParseContext GMarkupParseContext;
typedef struct _GMarkupParser GMarkupParser;
struct _GMarkupParser
{
  void (*start_element) (GMarkupParseContext *context,
                          const gchar *element_name,
                          const gchar **attribute_names,
                          const gchar **attribute_values,
                          gpointer user_data,
                          GError **error);
  void (*end_element) (GMarkupParseContext *context,
                          const gchar *element_name,
                          gpointer user_data,
                          GError **error);
  void (*text) (GMarkupParseContext *context,
                          const gchar *text,
                          gsize text_len,
                          gpointer user_data,
                          GError **error);
  void (*passthrough) (GMarkupParseContext *context,
                          const gchar *passthrough_text,
                          gsize text_len,
                          gpointer user_data,
                          GError **error);
  void (*error) (GMarkupParseContext *context,
                          GError *error,
                          gpointer user_data);
};
extern
GMarkupParseContext *g_markup_parse_context_new (const GMarkupParser *parser,
                                                   GMarkupParseFlags flags,
                                                   gpointer user_data,
                                                   GDestroyNotify user_data_dnotify);
extern
GMarkupParseContext *g_markup_parse_context_ref (GMarkupParseContext *context);
extern
void g_markup_parse_context_unref (GMarkupParseContext *context);
extern
void g_markup_parse_context_free (GMarkupParseContext *context);
extern
gboolean g_markup_parse_context_parse (GMarkupParseContext *context,
                                                   const gchar *text,
                                                   gssize text_len,
                                                   GError **error);
extern
void g_markup_parse_context_push (GMarkupParseContext *context,
                                                   const GMarkupParser *parser,
                                                   gpointer user_data);
extern
gpointer g_markup_parse_context_pop (GMarkupParseContext *context);
extern
gboolean g_markup_parse_context_end_parse (GMarkupParseContext *context,
                                                       GError **error);
extern
const gchar * g_markup_parse_context_get_element (GMarkupParseContext *context);
extern
const GSList * g_markup_parse_context_get_element_stack (GMarkupParseContext *context);
extern
void g_markup_parse_context_get_position (GMarkupParseContext *context,
                                                          gint *line_number,
                                                          gint *char_number);
extern
gpointer g_markup_parse_context_get_user_data (GMarkupParseContext *context);
extern
gchar* g_markup_escape_text (const gchar *text,
                             gssize length);
extern
gchar *g_markup_printf_escaped (const char *format,
    ...) ;
extern
gchar *g_markup_vprintf_escaped (const char *format,
     va_list args) ;
typedef enum
{
  G_MARKUP_COLLECT_INVALID,
  G_MARKUP_COLLECT_STRING,
  G_MARKUP_COLLECT_STRDUP,
  G_MARKUP_COLLECT_BOOLEAN,
  G_MARKUP_COLLECT_TRISTATE,
  G_MARKUP_COLLECT_OPTIONAL = 65536
} GMarkupCollectType;
extern
gboolean g_markup_collect_attributes (const gchar *element_name,
                                        const gchar **attribute_names,
                                        const gchar **attribute_values,
                                        GError **error,
                                        GMarkupCollectType first_type,
                                        const gchar *first_attr,
                                        ...);
typedef struct _GVariantType GVariantType;
extern
gboolean g_variant_type_string_is_valid (const gchar *type_string);
extern
gboolean g_variant_type_string_scan (const gchar *string,
                                                                         const gchar *limit,
                                                                         const gchar **endptr);
extern
void g_variant_type_free (GVariantType *type);
extern
GVariantType * g_variant_type_copy (const GVariantType *type);
extern
GVariantType * g_variant_type_new (const gchar *type_string);
extern
gsize g_variant_type_get_string_length (const GVariantType *type);
extern
const gchar * g_variant_type_peek_string (const GVariantType *type);
extern
gchar * g_variant_type_dup_string (const GVariantType *type);
extern
gboolean g_variant_type_is_definite (const GVariantType *type);
extern
gboolean g_variant_type_is_container (const GVariantType *type);
extern
gboolean g_variant_type_is_basic (const GVariantType *type);
extern
gboolean g_variant_type_is_maybe (const GVariantType *type);
extern
gboolean g_variant_type_is_array (const GVariantType *type);
extern
gboolean g_variant_type_is_tuple (const GVariantType *type);
extern
gboolean g_variant_type_is_dict_entry (const GVariantType *type);
extern
gboolean g_variant_type_is_variant (const GVariantType *type);
extern
guint g_variant_type_hash (gconstpointer type);
extern
gboolean g_variant_type_equal (gconstpointer type1,
                                                                         gconstpointer type2);
extern
gboolean g_variant_type_is_subtype_of (const GVariantType *type,
                                                                         const GVariantType *supertype);
extern
const GVariantType * g_variant_type_element (const GVariantType *type);
extern
const GVariantType * g_variant_type_first (const GVariantType *type);
extern
const GVariantType * g_variant_type_next (const GVariantType *type);
extern
gsize g_variant_type_n_items (const GVariantType *type);
extern
const GVariantType * g_variant_type_key (const GVariantType *type);
extern
const GVariantType * g_variant_type_value (const GVariantType *type);
extern
GVariantType * g_variant_type_new_array (const GVariantType *element);
extern
GVariantType * g_variant_type_new_maybe (const GVariantType *element);
extern
GVariantType * g_variant_type_new_tuple (const GVariantType * const *items,
                                                                         gint length);
extern
GVariantType * g_variant_type_new_dict_entry (const GVariantType *key,
                                                                         const GVariantType *value);
extern
const GVariantType * g_variant_type_checked_ (const gchar *);
extern
gsize g_variant_type_string_get_depth_ (const gchar *type_string);
typedef struct _GVariant GVariant;
typedef enum
{
  G_VARIANT_CLASS_BOOLEAN = 'b',
  G_VARIANT_CLASS_BYTE = 'y',
  G_VARIANT_CLASS_INT16 = 'n',
  G_VARIANT_CLASS_UINT16 = 'q',
  G_VARIANT_CLASS_INT32 = 'i',
  G_VARIANT_CLASS_UINT32 = 'u',
  G_VARIANT_CLASS_INT64 = 'x',
  G_VARIANT_CLASS_UINT64 = 't',
  G_VARIANT_CLASS_HANDLE = 'h',
  G_VARIANT_CLASS_DOUBLE = 'd',
  G_VARIANT_CLASS_STRING = 's',
  G_VARIANT_CLASS_OBJECT_PATH = 'o',
  G_VARIANT_CLASS_SIGNATURE = 'g',
  G_VARIANT_CLASS_VARIANT = 'v',
  G_VARIANT_CLASS_MAYBE = 'm',
  G_VARIANT_CLASS_ARRAY = 'a',
  G_VARIANT_CLASS_TUPLE = '(',
  G_VARIANT_CLASS_DICT_ENTRY = '{'
} GVariantClass;
extern
void g_variant_unref (GVariant *value);
extern
GVariant * g_variant_ref (GVariant *value);
extern
GVariant * g_variant_ref_sink (GVariant *value);
extern
gboolean g_variant_is_floating (GVariant *value);
extern
GVariant * g_variant_take_ref (GVariant *value);
extern
const GVariantType * g_variant_get_type (GVariant *value);
extern
const gchar * g_variant_get_type_string (GVariant *value);
extern
gboolean g_variant_is_of_type (GVariant *value,
                                                                         const GVariantType *type);
extern
gboolean g_variant_is_container (GVariant *value);
extern
GVariantClass g_variant_classify (GVariant *value);
extern
GVariant * g_variant_new_boolean (gboolean value);
extern
GVariant * g_variant_new_byte (guint8 value);
extern
GVariant * g_variant_new_int16 (gint16 value);
extern
GVariant * g_variant_new_uint16 (guint16 value);
extern
GVariant * g_variant_new_int32 (gint32 value);
extern
GVariant * g_variant_new_uint32 (guint32 value);
extern
GVariant * g_variant_new_int64 (gint64 value);
extern
GVariant * g_variant_new_uint64 (guint64 value);
extern
GVariant * g_variant_new_handle (gint32 value);
extern
GVariant * g_variant_new_double (gdouble value);
extern
GVariant * g_variant_new_string (const gchar *string);
extern
GVariant * g_variant_new_take_string (gchar *string);
extern
GVariant * g_variant_new_printf (const gchar *format_string,
                                                                         ...) ;
extern
GVariant * g_variant_new_object_path (const gchar *object_path);
extern
gboolean g_variant_is_object_path (const gchar *string);
extern
GVariant * g_variant_new_signature (const gchar *signature);
extern
gboolean g_variant_is_signature (const gchar *string);
extern
GVariant * g_variant_new_variant (GVariant *value);
extern
GVariant * g_variant_new_strv (const gchar * const *strv,
                                                                         gssize length);
extern
GVariant * g_variant_new_objv (const gchar * const *strv,
                                                                         gssize length);
extern
GVariant * g_variant_new_bytestring (const gchar *string);
extern
GVariant * g_variant_new_bytestring_array (const gchar * const *strv,
                                                                         gssize length);
extern
GVariant * g_variant_new_fixed_array (const GVariantType *element_type,
                                                                         gconstpointer elements,
                                                                         gsize n_elements,
                                                                         gsize element_size);
extern
gboolean g_variant_get_boolean (GVariant *value);
extern
guint8 g_variant_get_byte (GVariant *value);
extern
gint16 g_variant_get_int16 (GVariant *value);
extern
guint16 g_variant_get_uint16 (GVariant *value);
extern
gint32 g_variant_get_int32 (GVariant *value);
extern
guint32 g_variant_get_uint32 (GVariant *value);
extern
gint64 g_variant_get_int64 (GVariant *value);
extern
guint64 g_variant_get_uint64 (GVariant *value);
extern
gint32 g_variant_get_handle (GVariant *value);
extern
gdouble g_variant_get_double (GVariant *value);
extern
GVariant * g_variant_get_variant (GVariant *value);
extern
const gchar * g_variant_get_string (GVariant *value,
                                                                         gsize *length);
extern
gchar * g_variant_dup_string (GVariant *value,
                                                                         gsize *length);
extern
const gchar ** g_variant_get_strv (GVariant *value,
                                                                         gsize *length);
extern
gchar ** g_variant_dup_strv (GVariant *value,
                                                                         gsize *length);
extern
const gchar ** g_variant_get_objv (GVariant *value,
                                                                         gsize *length);
extern
gchar ** g_variant_dup_objv (GVariant *value,
                                                                         gsize *length);
extern
const gchar * g_variant_get_bytestring (GVariant *value);
extern
gchar * g_variant_dup_bytestring (GVariant *value,
                                                                         gsize *length);
extern
const gchar ** g_variant_get_bytestring_array (GVariant *value,
                                                                         gsize *length);
extern
gchar ** g_variant_dup_bytestring_array (GVariant *value,
                                                                         gsize *length);
extern
GVariant * g_variant_new_maybe (const GVariantType *child_type,
                                                                         GVariant *child);
extern
GVariant * g_variant_new_array (const GVariantType *child_type,
                                                                         GVariant * const *children,
                                                                         gsize n_children);
extern
GVariant * g_variant_new_tuple (GVariant * const *children,
                                                                         gsize n_children);
extern
GVariant * g_variant_new_dict_entry (GVariant *key,
                                                                         GVariant *value);
extern
GVariant * g_variant_get_maybe (GVariant *value);
extern
gsize g_variant_n_children (GVariant *value);
extern
void g_variant_get_child (GVariant *value,
                                                                         gsize index_,
                                                                         const gchar *format_string,
                                                                         ...);
extern
GVariant * g_variant_get_child_value (GVariant *value,
                                                                         gsize index_);
extern
gboolean g_variant_lookup (GVariant *dictionary,
                                                                         const gchar *key,
                                                                         const gchar *format_string,
                                                                         ...);
extern
GVariant * g_variant_lookup_value (GVariant *dictionary,
                                                                         const gchar *key,
                                                                         const GVariantType *expected_type);
extern
gconstpointer g_variant_get_fixed_array (GVariant *value,
                                                                         gsize *n_elements,
                                                                         gsize element_size);
extern
gsize g_variant_get_size (GVariant *value);
extern
gconstpointer g_variant_get_data (GVariant *value);
extern
GBytes * g_variant_get_data_as_bytes (GVariant *value);
extern
void g_variant_store (GVariant *value,
                                                                         gpointer data);
extern
gchar * g_variant_print (GVariant *value,
                                                                         gboolean type_annotate);
extern
GString * g_variant_print_string (GVariant *value,
                                                                         GString *string,
                                                                         gboolean type_annotate);
extern
guint g_variant_hash (gconstpointer value);
extern
gboolean g_variant_equal (gconstpointer one,
                                                                         gconstpointer two);
extern
GVariant * g_variant_get_normal_form (GVariant *value);
extern
gboolean g_variant_is_normal_form (GVariant *value);
extern
GVariant * g_variant_byteswap (GVariant *value);
extern
GVariant * g_variant_new_from_bytes (const GVariantType *type,
                                                                         GBytes *bytes,
                                                                         gboolean trusted);
extern
GVariant * g_variant_new_from_data (const GVariantType *type,
                                                                         gconstpointer data,
                                                                         gsize size,
                                                                         gboolean trusted,
                                                                         GDestroyNotify notify,
                                                                         gpointer user_data);
typedef struct _GVariantIter GVariantIter;
struct _GVariantIter {
  gsize x[16];
};
extern
GVariantIter * g_variant_iter_new (GVariant *value);
extern
gsize g_variant_iter_init (GVariantIter *iter,
                                                                         GVariant *value);
extern
GVariantIter * g_variant_iter_copy (GVariantIter *iter);
extern
gsize g_variant_iter_n_children (GVariantIter *iter);
extern
void g_variant_iter_free (GVariantIter *iter);
extern
GVariant * g_variant_iter_next_value (GVariantIter *iter);
extern
gboolean g_variant_iter_next (GVariantIter *iter,
                                                                         const gchar *format_string,
                                                                         ...);
extern
gboolean g_variant_iter_loop (GVariantIter *iter,
                                                                         const gchar *format_string,
                                                                         ...);
typedef struct _GVariantBuilder GVariantBuilder;
struct _GVariantBuilder {
  union
  {
    struct {
      gsize partial_magic;
      const GVariantType *type;
      gsize y[14];
    } s;
    gsize x[16];
  } u;
};
typedef enum
{
  G_VARIANT_PARSE_ERROR_FAILED,
  G_VARIANT_PARSE_ERROR_BASIC_TYPE_EXPECTED,
  G_VARIANT_PARSE_ERROR_CANNOT_INFER_TYPE,
  G_VARIANT_PARSE_ERROR_DEFINITE_TYPE_EXPECTED,
  G_VARIANT_PARSE_ERROR_INPUT_NOT_AT_END,
  G_VARIANT_PARSE_ERROR_INVALID_CHARACTER,
  G_VARIANT_PARSE_ERROR_INVALID_FORMAT_STRING,
  G_VARIANT_PARSE_ERROR_INVALID_OBJECT_PATH,
  G_VARIANT_PARSE_ERROR_INVALID_SIGNATURE,
  G_VARIANT_PARSE_ERROR_INVALID_TYPE_STRING,
  G_VARIANT_PARSE_ERROR_NO_COMMON_TYPE,
  G_VARIANT_PARSE_ERROR_NUMBER_OUT_OF_RANGE,
  G_VARIANT_PARSE_ERROR_NUMBER_TOO_BIG,
  G_VARIANT_PARSE_ERROR_TYPE_ERROR,
  G_VARIANT_PARSE_ERROR_UNEXPECTED_TOKEN,
  G_VARIANT_PARSE_ERROR_UNKNOWN_KEYWORD,
  G_VARIANT_PARSE_ERROR_UNTERMINATED_STRING_CONSTANT,
  G_VARIANT_PARSE_ERROR_VALUE_EXPECTED,
  G_VARIANT_PARSE_ERROR_RECURSION
} GVariantParseError;
 extern
GQuark g_variant_parser_get_error_quark (void);
extern
GQuark g_variant_parse_error_quark (void);
extern
GVariantBuilder * g_variant_builder_new (const GVariantType *type);
extern
void g_variant_builder_unref (GVariantBuilder *builder);
extern
GVariantBuilder * g_variant_builder_ref (GVariantBuilder *builder);
extern
void g_variant_builder_init (GVariantBuilder *builder,
                                                                         const GVariantType *type);
extern
GVariant * g_variant_builder_end (GVariantBuilder *builder);
extern
void g_variant_builder_clear (GVariantBuilder *builder);
extern
void g_variant_builder_open (GVariantBuilder *builder,
                                                                         const GVariantType *type);
extern
void g_variant_builder_close (GVariantBuilder *builder);
extern
void g_variant_builder_add_value (GVariantBuilder *builder,
                                                                         GVariant *value);
extern
void g_variant_builder_add (GVariantBuilder *builder,
                                                                         const gchar *format_string,
                                                                         ...);
extern
void g_variant_builder_add_parsed (GVariantBuilder *builder,
                                                                         const gchar *format,
                                                                         ...);
extern
GVariant * g_variant_new (const gchar *format_string,
                                                                         ...);
extern
void g_variant_get (GVariant *value,
                                                                         const gchar *format_string,
                                                                         ...);
extern
GVariant * g_variant_new_va (const gchar *format_string,
                                                                         const gchar **endptr,
                                                                         va_list *app);
extern
void g_variant_get_va (GVariant *value,
                                                                         const gchar *format_string,
                                                                         const gchar **endptr,
                                                                         va_list *app);
extern
gboolean g_variant_check_format_string (GVariant *value,
                                                                         const gchar *format_string,
                                                                         gboolean copy_only);
extern
GVariant * g_variant_parse (const GVariantType *type,
                                                                         const gchar *text,
                                                                         const gchar *limit,
                                                                         const gchar **endptr,
                                                                         GError **error);
extern
GVariant * g_variant_new_parsed (const gchar *format,
                                                                         ...);
extern
GVariant * g_variant_new_parsed_va (const gchar *format,
                                                                         va_list *app);
extern
gchar * g_variant_parse_error_print_context (GError *error,
                                                                         const gchar *source_str);
extern
gint g_variant_compare (gconstpointer one,
                                                                         gconstpointer two);
typedef struct _GVariantDict GVariantDict;
struct _GVariantDict {
  union
  {
    struct {
      GVariant *asv;
      gsize partial_magic;
      gsize y[14];
    } s;
    gsize x[16];
  } u;
};
extern
GVariantDict * g_variant_dict_new (GVariant *from_asv);
extern
void g_variant_dict_init (GVariantDict *dict,
                                                                         GVariant *from_asv);
extern
gboolean g_variant_dict_lookup (GVariantDict *dict,
                                                                         const gchar *key,
                                                                         const gchar *format_string,
                                                                         ...);
extern
GVariant * g_variant_dict_lookup_value (GVariantDict *dict,
                                                                         const gchar *key,
                                                                         const GVariantType *expected_type);
extern
gboolean g_variant_dict_contains (GVariantDict *dict,
                                                                         const gchar *key);
extern
void g_variant_dict_insert (GVariantDict *dict,
                                                                         const gchar *key,
                                                                         const gchar *format_string,
                                                                         ...);
extern
void g_variant_dict_insert_value (GVariantDict *dict,
                                                                         const gchar *key,
                                                                         GVariant *value);
extern
gboolean g_variant_dict_remove (GVariantDict *dict,
                                                                         const gchar *key);
extern
void g_variant_dict_clear (GVariantDict *dict);
extern
GVariant * g_variant_dict_end (GVariantDict *dict);
extern
GVariantDict * g_variant_dict_ref (GVariantDict *dict);
extern
void g_variant_dict_unref (GVariantDict *dict);
extern
gsize g_printf_string_upper_bound (const gchar* format,
         va_list args) ;
typedef enum
{
  G_LOG_FLAG_RECURSION = 1 << 0,
  G_LOG_FLAG_FATAL = 1 << 1,
  G_LOG_LEVEL_ERROR = 1 << 2,
  G_LOG_LEVEL_CRITICAL = 1 << 3,
  G_LOG_LEVEL_WARNING = 1 << 4,
  G_LOG_LEVEL_MESSAGE = 1 << 5,
  G_LOG_LEVEL_INFO = 1 << 6,
  G_LOG_LEVEL_DEBUG = 1 << 7,
  G_LOG_LEVEL_MASK = -4
} GLogLevelFlags;
typedef void (*GLogFunc) (const gchar *log_domain,
                                                 GLogLevelFlags log_level,
                                                 const gchar *message,
                                                 gpointer user_data);
extern
guint g_log_set_handler (const gchar *log_domain,
                                         GLogLevelFlags log_levels,
                                         GLogFunc log_func,
                                         gpointer user_data);
extern
guint g_log_set_handler_full (const gchar *log_domain,
                                         GLogLevelFlags log_levels,
                                         GLogFunc log_func,
                                         gpointer user_data,
                                         GDestroyNotify destroy);
extern
void g_log_remove_handler (const gchar *log_domain,
                                         guint handler_id);
extern
void g_log_default_handler (const gchar *log_domain,
                                         GLogLevelFlags log_level,
                                         const gchar *message,
                                         gpointer unused_data);
extern
GLogFunc g_log_set_default_handler (GLogFunc log_func,
        gpointer user_data);
extern
void g_log (const gchar *log_domain,
                                         GLogLevelFlags log_level,
                                         const gchar *format,
                                         ...) ;
extern
void g_logv (const gchar *log_domain,
                                         GLogLevelFlags log_level,
                                         const gchar *format,
                                         va_list args) ;
extern
GLogLevelFlags g_log_set_fatal_mask (const gchar *log_domain,
                                         GLogLevelFlags fatal_mask);
extern
GLogLevelFlags g_log_set_always_fatal (GLogLevelFlags fatal_mask);
typedef enum
{
  G_LOG_WRITER_HANDLED = 1,
  G_LOG_WRITER_UNHANDLED = 0,
} GLogWriterOutput;
typedef struct _GLogField GLogField;
struct _GLogField
{
  const gchar *key;
  gconstpointer value;
  gssize length;
};
typedef GLogWriterOutput (*GLogWriterFunc) (GLogLevelFlags log_level,
                                                const GLogField *fields,
                                                gsize n_fields,
                                                gpointer user_data);
extern
void g_log_structured (const gchar *log_domain,
                                                GLogLevelFlags log_level,
                                                ...);
extern
void g_log_structured_array (GLogLevelFlags log_level,
                                                const GLogField *fields,
                                                gsize n_fields);
extern
void g_log_variant (const gchar *log_domain,
                                                GLogLevelFlags log_level,
                                                GVariant *fields);
extern
void g_log_set_writer_func (GLogWriterFunc func,
                                                gpointer user_data,
                                                GDestroyNotify user_data_free);
extern
gboolean g_log_writer_supports_color (gint output_fd);
extern
gboolean g_log_writer_is_journald (gint output_fd);
extern
gchar *g_log_writer_format_fields (GLogLevelFlags log_level,
                                                const GLogField *fields,
                                                gsize n_fields,
                                                gboolean use_color);
extern
GLogWriterOutput g_log_writer_journald (GLogLevelFlags log_level,
                                                const GLogField *fields,
                                                gsize n_fields,
                                                gpointer user_data);
extern
GLogWriterOutput g_log_writer_standard_streams (GLogLevelFlags log_level,
                                                const GLogField *fields,
                                                gsize n_fields,
                                                gpointer user_data);
extern
GLogWriterOutput g_log_writer_default (GLogLevelFlags log_level,
                                                const GLogField *fields,
                                                gsize n_fields,
                                                gpointer user_data);
extern
void g_log_writer_default_set_use_stderr (gboolean use_stderr);
extern
gboolean g_log_writer_default_would_drop (GLogLevelFlags log_level,
                                                 const char *log_domain);
extern
gboolean g_log_get_debug_enabled (void);
extern
void g_log_set_debug_enabled (gboolean enabled);
void _g_log_fallback_handler (const gchar *log_domain,
       GLogLevelFlags log_level,
       const gchar *message,
       gpointer unused_data);
extern
void g_return_if_fail_warning (const char *log_domain,
          const char *pretty_function,
          const char *expression) ;
extern
void g_warn_message (const char *domain,
                               const char *file,
                               int line,
                               const char *func,
                               const char *warnexpr) ;
 extern
void g_assert_warning (const char *log_domain,
          const char *file,
          const int line,
                 const char *pretty_function,
                 const char *expression);
extern
void g_log_structured_standard (const gchar *log_domain,
                                GLogLevelFlags log_level,
                                const gchar *file,
                                const gchar *line,
                                const gchar *func,
                                const gchar *message_format,
                                ...) ;
typedef void (*GPrintFunc) (const gchar *string);
extern
void g_print (const gchar *format,
                                         ...) ;
extern
GPrintFunc g_set_print_handler (GPrintFunc func);
extern
void g_printerr (const gchar *format,
                                         ...) ;
extern
GPrintFunc g_set_printerr_handler (GPrintFunc func);
typedef struct _GOptionContext GOptionContext;
typedef struct _GOptionGroup GOptionGroup;
typedef struct _GOptionEntry GOptionEntry;
typedef enum
{
  G_OPTION_FLAG_NONE = 0,
  G_OPTION_FLAG_HIDDEN = 1 << 0,
  G_OPTION_FLAG_IN_MAIN = 1 << 1,
  G_OPTION_FLAG_REVERSE = 1 << 2,
  G_OPTION_FLAG_NO_ARG = 1 << 3,
  G_OPTION_FLAG_FILENAME = 1 << 4,
  G_OPTION_FLAG_OPTIONAL_ARG = 1 << 5,
  G_OPTION_FLAG_NOALIAS = 1 << 6
} GOptionFlags;
typedef enum
{
  G_OPTION_ARG_NONE,
  G_OPTION_ARG_STRING,
  G_OPTION_ARG_INT,
  G_OPTION_ARG_CALLBACK,
  G_OPTION_ARG_FILENAME,
  G_OPTION_ARG_STRING_ARRAY,
  G_OPTION_ARG_FILENAME_ARRAY,
  G_OPTION_ARG_DOUBLE,
  G_OPTION_ARG_INT64
} GOptionArg;
typedef gboolean (*GOptionArgFunc) (const gchar *option_name,
        const gchar *value,
        gpointer data,
        GError **error);
typedef gboolean (*GOptionParseFunc) (GOptionContext *context,
          GOptionGroup *group,
          gpointer data,
          GError **error);
typedef void (*GOptionErrorFunc) (GOptionContext *context,
      GOptionGroup *group,
      gpointer data,
      GError **error);
typedef enum
{
  G_OPTION_ERROR_UNKNOWN_OPTION,
  G_OPTION_ERROR_BAD_VALUE,
  G_OPTION_ERROR_FAILED
} GOptionError;
extern
GQuark g_option_error_quark (void);
struct _GOptionEntry
{
  const gchar *long_name;
  gchar short_name;
  gint flags;
  GOptionArg arg;
  gpointer arg_data;
  const gchar *description;
  const gchar *arg_description;
};
extern
GOptionContext *g_option_context_new (const gchar *parameter_string);
extern
void g_option_context_set_summary (GOptionContext *context,
                                                   const gchar *summary);
extern
const gchar * g_option_context_get_summary (GOptionContext *context);
extern
void g_option_context_set_description (GOptionContext *context,
                                                   const gchar *description);
extern
const gchar * g_option_context_get_description (GOptionContext *context);
extern
void g_option_context_free (GOptionContext *context);
extern
void g_option_context_set_help_enabled (GOptionContext *context,
         gboolean help_enabled);
extern
gboolean g_option_context_get_help_enabled (GOptionContext *context);
extern
void g_option_context_set_ignore_unknown_options (GOptionContext *context,
            gboolean ignore_unknown);
extern
gboolean g_option_context_get_ignore_unknown_options (GOptionContext *context);
extern
void g_option_context_set_strict_posix (GOptionContext *context,
                                                             gboolean strict_posix);
extern
gboolean g_option_context_get_strict_posix (GOptionContext *context);
extern
void g_option_context_add_main_entries (GOptionContext *context,
         const GOptionEntry *entries,
         const gchar *translation_domain);
extern
gboolean g_option_context_parse (GOptionContext *context,
         gint *argc,
         gchar ***argv,
         GError **error);
extern
gboolean g_option_context_parse_strv (GOptionContext *context,
                                                   gchar ***arguments,
                                                   GError **error);
extern
void g_option_context_set_translate_func (GOptionContext *context,
           GTranslateFunc func,
           gpointer data,
           GDestroyNotify destroy_notify);
extern
void g_option_context_set_translation_domain (GOptionContext *context,
        const gchar *domain);
extern
void g_option_context_add_group (GOptionContext *context,
       GOptionGroup *group);
extern
void g_option_context_set_main_group (GOptionContext *context,
            GOptionGroup *group);
extern
GOptionGroup *g_option_context_get_main_group (GOptionContext *context);
extern
gchar *g_option_context_get_help (GOptionContext *context,
                                               gboolean main_help,
                                               GOptionGroup *group);
extern
GOptionGroup *g_option_group_new (const gchar *name,
           const gchar *description,
           const gchar *help_description,
           gpointer user_data,
           GDestroyNotify destroy);
extern
void g_option_group_set_parse_hooks (GOptionGroup *group,
           GOptionParseFunc pre_parse_func,
           GOptionParseFunc post_parse_func);
extern
void g_option_group_set_error_hook (GOptionGroup *group,
           GOptionErrorFunc error_func);
 extern
void g_option_group_free (GOptionGroup *group);
extern
GOptionGroup *g_option_group_ref (GOptionGroup *group);
extern
void g_option_group_unref (GOptionGroup *group);
extern
void g_option_group_add_entries (GOptionGroup *group,
           const GOptionEntry *entries);
extern
void g_option_group_set_translate_func (GOptionGroup *group,
           GTranslateFunc func,
           gpointer data,
           GDestroyNotify destroy_notify);
extern
void g_option_group_set_translation_domain (GOptionGroup *group,
           const gchar *domain);
typedef struct _GPatternSpec GPatternSpec;
extern
GPatternSpec* g_pattern_spec_new (const gchar *pattern);
extern
void g_pattern_spec_free (GPatternSpec *pspec);
extern
GPatternSpec *g_pattern_spec_copy (GPatternSpec *pspec);
extern
gboolean g_pattern_spec_equal (GPatternSpec *pspec1,
     GPatternSpec *pspec2);
extern
gboolean g_pattern_spec_match (GPatternSpec *pspec,
                               gsize string_length,
                               const gchar *string,
                               const gchar *string_reversed);
extern
gboolean g_pattern_spec_match_string (GPatternSpec *pspec,
                                      const gchar *string);
 extern
gboolean g_pattern_match (GPatternSpec *pspec,
     guint string_length,
     const gchar *string,
     const gchar *string_reversed);
 extern
gboolean g_pattern_match_string (GPatternSpec *pspec,
     const gchar *string);
extern
gboolean g_pattern_match_simple (const gchar *pattern,
     const gchar *string);
extern
guint g_spaced_primes_closest (guint num) ;
extern
void g_qsort_with_data (gconstpointer pbase,
   gint total_elems,
   gsize size,
   GCompareDataFunc compare_func,
   gpointer user_data);
typedef struct _GQueue GQueue;
struct _GQueue
{
  GList *head;
  GList *tail;
  guint length;
};
extern
GQueue* g_queue_new (void);
extern
void g_queue_free (GQueue *queue);
extern
void g_queue_free_full (GQueue *queue,
    GDestroyNotify free_func);
extern
void g_queue_init (GQueue *queue);
extern
void g_queue_clear (GQueue *queue);
extern
gboolean g_queue_is_empty (GQueue *queue);
extern
void g_queue_clear_full (GQueue *queue,
                                 GDestroyNotify free_func);
extern
guint g_queue_get_length (GQueue *queue);
extern
void g_queue_reverse (GQueue *queue);
extern
GQueue * g_queue_copy (GQueue *queue);
extern
void g_queue_foreach (GQueue *queue,
                                 GFunc func,
                                 gpointer user_data);
extern
GList * g_queue_find (GQueue *queue,
                                 gconstpointer data);
extern
GList * g_queue_find_custom (GQueue *queue,
                                 gconstpointer data,
                                 GCompareFunc func);
extern
void g_queue_sort (GQueue *queue,
                                 GCompareDataFunc compare_func,
                                 gpointer user_data);
extern
void g_queue_push_head (GQueue *queue,
                                 gpointer data);
extern
void g_queue_push_tail (GQueue *queue,
                                 gpointer data);
extern
void g_queue_push_nth (GQueue *queue,
                                 gpointer data,
                                 gint n);
extern
gpointer g_queue_pop_head (GQueue *queue);
extern
gpointer g_queue_pop_tail (GQueue *queue);
extern
gpointer g_queue_pop_nth (GQueue *queue,
                                 guint n);
extern
gpointer g_queue_peek_head (GQueue *queue);
extern
gpointer g_queue_peek_tail (GQueue *queue);
extern
gpointer g_queue_peek_nth (GQueue *queue,
                                 guint n);
extern
gint g_queue_index (GQueue *queue,
                                 gconstpointer data);
extern
gboolean g_queue_remove (GQueue *queue,
                                 gconstpointer data);
extern
guint g_queue_remove_all (GQueue *queue,
                                 gconstpointer data);
extern
void g_queue_insert_before (GQueue *queue,
                                 GList *sibling,
                                 gpointer data);
extern
void g_queue_insert_before_link
                                (GQueue *queue,
                                 GList *sibling,
                                 GList *link_);
extern
void g_queue_insert_after (GQueue *queue,
                                 GList *sibling,
                                 gpointer data);
extern
void g_queue_insert_after_link
                                (GQueue *queue,
                                 GList *sibling,
                                 GList *link_);
extern
void g_queue_insert_sorted (GQueue *queue,
                                 gpointer data,
                                 GCompareDataFunc func,
                                 gpointer user_data);
extern
void g_queue_push_head_link (GQueue *queue,
                                 GList *link_);
extern
void g_queue_push_tail_link (GQueue *queue,
                                 GList *link_);
extern
void g_queue_push_nth_link (GQueue *queue,
                                 gint n,
                                 GList *link_);
extern
GList* g_queue_pop_head_link (GQueue *queue);
extern
GList* g_queue_pop_tail_link (GQueue *queue);
extern
GList* g_queue_pop_nth_link (GQueue *queue,
                                 guint n);
extern
GList* g_queue_peek_head_link (GQueue *queue);
extern
GList* g_queue_peek_tail_link (GQueue *queue);
extern
GList* g_queue_peek_nth_link (GQueue *queue,
                                 guint n);
extern
gint g_queue_link_index (GQueue *queue,
                                 GList *link_);
extern
void g_queue_unlink (GQueue *queue,
                                 GList *link_);
extern
void g_queue_delete_link (GQueue *queue,
                                 GList *link_);
typedef struct _GRand GRand;
extern
GRand* g_rand_new_with_seed (guint32 seed);
extern
GRand* g_rand_new_with_seed_array (const guint32 *seed,
        guint seed_length);
extern
GRand* g_rand_new (void);
extern
void g_rand_free (GRand *rand_);
extern
GRand* g_rand_copy (GRand *rand_);
extern
void g_rand_set_seed (GRand *rand_,
          guint32 seed);
extern
void g_rand_set_seed_array (GRand *rand_,
          const guint32 *seed,
          guint seed_length);
extern
guint32 g_rand_int (GRand *rand_);
extern
gint32 g_rand_int_range (GRand *rand_,
          gint32 begin,
          gint32 end);
extern
gdouble g_rand_double (GRand *rand_);
extern
gdouble g_rand_double_range (GRand *rand_,
          gdouble begin,
          gdouble end);
extern
void g_random_set_seed (guint32 seed);
extern
guint32 g_random_int (void);
extern
gint32 g_random_int_range (gint32 begin,
          gint32 end);
extern
gdouble g_random_double (void);
extern
gdouble g_random_double_range (gdouble begin,
          gdouble end);
       
extern
gpointer g_rc_box_alloc (gsize block_size) ;
extern
gpointer g_rc_box_alloc0 (gsize block_size) ;
extern
gpointer g_rc_box_dup (gsize block_size,
                                                 gconstpointer mem_block) ;
extern
gpointer g_rc_box_acquire (gpointer mem_block);
extern
void g_rc_box_release (gpointer mem_block);
extern
void g_rc_box_release_full (gpointer mem_block,
                                                 GDestroyNotify clear_func);
extern
gsize g_rc_box_get_size (gpointer mem_block);
extern
gpointer g_atomic_rc_box_alloc (gsize block_size) ;
extern
gpointer g_atomic_rc_box_alloc0 (gsize block_size) ;
extern
gpointer g_atomic_rc_box_dup (gsize block_size,
                                                 gconstpointer mem_block) ;
extern
gpointer g_atomic_rc_box_acquire (gpointer mem_block);
extern
void g_atomic_rc_box_release (gpointer mem_block);
extern
void g_atomic_rc_box_release_full (gpointer mem_block,
                                                 GDestroyNotify clear_func);
extern
gsize g_atomic_rc_box_get_size (gpointer mem_block);
extern
void g_ref_count_init (grefcount *rc);
extern
void g_ref_count_inc (grefcount *rc);
extern
gboolean g_ref_count_dec (grefcount *rc);
extern
gboolean g_ref_count_compare (grefcount *rc,
                                                 gint val);
extern
void g_atomic_ref_count_init (gatomicrefcount *arc);
extern
void g_atomic_ref_count_inc (gatomicrefcount *arc);
extern
gboolean g_atomic_ref_count_dec (gatomicrefcount *arc);
extern
gboolean g_atomic_ref_count_compare (gatomicrefcount *arc,
                                                 gint val);
       
extern
char * g_ref_string_new (const char *str);
extern
char * g_ref_string_new_len (const char *str,
                                 gssize len);
extern
char * g_ref_string_new_intern (const char *str);
extern
char * g_ref_string_acquire (char *str);
extern
void g_ref_string_release (char *str);
extern
gsize g_ref_string_length (char *str);
typedef char GRefString;
typedef enum
{
  G_REGEX_ERROR_COMPILE,
  G_REGEX_ERROR_OPTIMIZE,
  G_REGEX_ERROR_REPLACE,
  G_REGEX_ERROR_MATCH,
  G_REGEX_ERROR_INTERNAL,
  G_REGEX_ERROR_STRAY_BACKSLASH = 101,
  G_REGEX_ERROR_MISSING_CONTROL_CHAR = 102,
  G_REGEX_ERROR_UNRECOGNIZED_ESCAPE = 103,
  G_REGEX_ERROR_QUANTIFIERS_OUT_OF_ORDER = 104,
  G_REGEX_ERROR_QUANTIFIER_TOO_BIG = 105,
  G_REGEX_ERROR_UNTERMINATED_CHARACTER_CLASS = 106,
  G_REGEX_ERROR_INVALID_ESCAPE_IN_CHARACTER_CLASS = 107,
  G_REGEX_ERROR_RANGE_OUT_OF_ORDER = 108,
  G_REGEX_ERROR_NOTHING_TO_REPEAT = 109,
  G_REGEX_ERROR_UNRECOGNIZED_CHARACTER = 112,
  G_REGEX_ERROR_POSIX_NAMED_CLASS_OUTSIDE_CLASS = 113,
  G_REGEX_ERROR_UNMATCHED_PARENTHESIS = 114,
  G_REGEX_ERROR_INEXISTENT_SUBPATTERN_REFERENCE = 115,
  G_REGEX_ERROR_UNTERMINATED_COMMENT = 118,
  G_REGEX_ERROR_EXPRESSION_TOO_LARGE = 120,
  G_REGEX_ERROR_MEMORY_ERROR = 121,
  G_REGEX_ERROR_VARIABLE_LENGTH_LOOKBEHIND = 125,
  G_REGEX_ERROR_MALFORMED_CONDITION = 126,
  G_REGEX_ERROR_TOO_MANY_CONDITIONAL_BRANCHES = 127,
  G_REGEX_ERROR_ASSERTION_EXPECTED = 128,
  G_REGEX_ERROR_UNKNOWN_POSIX_CLASS_NAME = 130,
  G_REGEX_ERROR_POSIX_COLLATING_ELEMENTS_NOT_SUPPORTED = 131,
  G_REGEX_ERROR_HEX_CODE_TOO_LARGE = 134,
  G_REGEX_ERROR_INVALID_CONDITION = 135,
  G_REGEX_ERROR_SINGLE_BYTE_MATCH_IN_LOOKBEHIND = 136,
  G_REGEX_ERROR_INFINITE_LOOP = 140,
  G_REGEX_ERROR_MISSING_SUBPATTERN_NAME_TERMINATOR = 142,
  G_REGEX_ERROR_DUPLICATE_SUBPATTERN_NAME = 143,
  G_REGEX_ERROR_MALFORMED_PROPERTY = 146,
  G_REGEX_ERROR_UNKNOWN_PROPERTY = 147,
  G_REGEX_ERROR_SUBPATTERN_NAME_TOO_LONG = 148,
  G_REGEX_ERROR_TOO_MANY_SUBPATTERNS = 149,
  G_REGEX_ERROR_INVALID_OCTAL_VALUE = 151,
  G_REGEX_ERROR_TOO_MANY_BRANCHES_IN_DEFINE = 154,
  G_REGEX_ERROR_DEFINE_REPETION = 155,
  G_REGEX_ERROR_INCONSISTENT_NEWLINE_OPTIONS = 156,
  G_REGEX_ERROR_MISSING_BACK_REFERENCE = 157,
  G_REGEX_ERROR_INVALID_RELATIVE_REFERENCE = 158,
  G_REGEX_ERROR_BACKTRACKING_CONTROL_VERB_ARGUMENT_FORBIDDEN = 159,
  G_REGEX_ERROR_UNKNOWN_BACKTRACKING_CONTROL_VERB = 160,
  G_REGEX_ERROR_NUMBER_TOO_BIG = 161,
  G_REGEX_ERROR_MISSING_SUBPATTERN_NAME = 162,
  G_REGEX_ERROR_MISSING_DIGIT = 163,
  G_REGEX_ERROR_INVALID_DATA_CHARACTER = 164,
  G_REGEX_ERROR_EXTRA_SUBPATTERN_NAME = 165,
  G_REGEX_ERROR_BACKTRACKING_CONTROL_VERB_ARGUMENT_REQUIRED = 166,
  G_REGEX_ERROR_INVALID_CONTROL_CHAR = 168,
  G_REGEX_ERROR_MISSING_NAME = 169,
  G_REGEX_ERROR_NOT_SUPPORTED_IN_CLASS = 171,
  G_REGEX_ERROR_TOO_MANY_FORWARD_REFERENCES = 172,
  G_REGEX_ERROR_NAME_TOO_LONG = 175,
  G_REGEX_ERROR_CHARACTER_VALUE_TOO_LARGE = 176
} GRegexError;
extern
GQuark g_regex_error_quark (void);
typedef enum
{
  G_REGEX_CASELESS = 1 << 0,
  G_REGEX_MULTILINE = 1 << 1,
  G_REGEX_DOTALL = 1 << 2,
  G_REGEX_EXTENDED = 1 << 3,
  G_REGEX_ANCHORED = 1 << 4,
  G_REGEX_DOLLAR_ENDONLY = 1 << 5,
  G_REGEX_UNGREEDY = 1 << 9,
  G_REGEX_RAW = 1 << 11,
  G_REGEX_NO_AUTO_CAPTURE = 1 << 12,
  G_REGEX_OPTIMIZE = 1 << 13,
  G_REGEX_FIRSTLINE = 1 << 18,
  G_REGEX_DUPNAMES = 1 << 19,
  G_REGEX_NEWLINE_CR = 1 << 20,
  G_REGEX_NEWLINE_LF = 1 << 21,
  G_REGEX_NEWLINE_CRLF = G_REGEX_NEWLINE_CR | G_REGEX_NEWLINE_LF,
  G_REGEX_NEWLINE_ANYCRLF = G_REGEX_NEWLINE_CR | 1 << 22,
  G_REGEX_BSR_ANYCRLF = 1 << 23,
  G_REGEX_JAVASCRIPT_COMPAT = 1 << 25
} GRegexCompileFlags;
typedef enum
{
  G_REGEX_MATCH_ANCHORED = 1 << 4,
  G_REGEX_MATCH_NOTBOL = 1 << 7,
  G_REGEX_MATCH_NOTEOL = 1 << 8,
  G_REGEX_MATCH_NOTEMPTY = 1 << 10,
  G_REGEX_MATCH_PARTIAL = 1 << 15,
  G_REGEX_MATCH_NEWLINE_CR = 1 << 20,
  G_REGEX_MATCH_NEWLINE_LF = 1 << 21,
  G_REGEX_MATCH_NEWLINE_CRLF = G_REGEX_MATCH_NEWLINE_CR | G_REGEX_MATCH_NEWLINE_LF,
  G_REGEX_MATCH_NEWLINE_ANY = 1 << 22,
  G_REGEX_MATCH_NEWLINE_ANYCRLF = G_REGEX_MATCH_NEWLINE_CR | G_REGEX_MATCH_NEWLINE_ANY,
  G_REGEX_MATCH_BSR_ANYCRLF = 1 << 23,
  G_REGEX_MATCH_BSR_ANY = 1 << 24,
  G_REGEX_MATCH_PARTIAL_SOFT = G_REGEX_MATCH_PARTIAL,
  G_REGEX_MATCH_PARTIAL_HARD = 1 << 27,
  G_REGEX_MATCH_NOTEMPTY_ATSTART = 1 << 28
} GRegexMatchFlags;
typedef struct _GRegex GRegex;
typedef struct _GMatchInfo GMatchInfo;
typedef gboolean (*GRegexEvalCallback) (const GMatchInfo *match_info,
       GString *result,
       gpointer user_data);
extern
GRegex *g_regex_new (const gchar *pattern,
       GRegexCompileFlags compile_options,
       GRegexMatchFlags match_options,
       GError **error);
extern
GRegex *g_regex_ref (GRegex *regex);
extern
void g_regex_unref (GRegex *regex);
extern
const gchar *g_regex_get_pattern (const GRegex *regex);
extern
gint g_regex_get_max_backref (const GRegex *regex);
extern
gint g_regex_get_capture_count (const GRegex *regex);
extern
gboolean g_regex_get_has_cr_or_lf (const GRegex *regex);
extern
gint g_regex_get_max_lookbehind (const GRegex *regex);
extern
gint g_regex_get_string_number (const GRegex *regex,
       const gchar *name);
extern
gchar *g_regex_escape_string (const gchar *string,
       gint length);
extern
gchar *g_regex_escape_nul (const gchar *string,
       gint length);
extern
GRegexCompileFlags g_regex_get_compile_flags (const GRegex *regex);
extern
GRegexMatchFlags g_regex_get_match_flags (const GRegex *regex);
extern
gboolean g_regex_match_simple (const gchar *pattern,
       const gchar *string,
       GRegexCompileFlags compile_options,
       GRegexMatchFlags match_options);
extern
gboolean g_regex_match (const GRegex *regex,
       const gchar *string,
       GRegexMatchFlags match_options,
       GMatchInfo **match_info);
extern
gboolean g_regex_match_full (const GRegex *regex,
       const gchar *string,
       gssize string_len,
       gint start_position,
       GRegexMatchFlags match_options,
       GMatchInfo **match_info,
       GError **error);
extern
gboolean g_regex_match_all (const GRegex *regex,
       const gchar *string,
       GRegexMatchFlags match_options,
       GMatchInfo **match_info);
extern
gboolean g_regex_match_all_full (const GRegex *regex,
       const gchar *string,
       gssize string_len,
       gint start_position,
       GRegexMatchFlags match_options,
       GMatchInfo **match_info,
       GError **error);
extern
gchar **g_regex_split_simple (const gchar *pattern,
       const gchar *string,
       GRegexCompileFlags compile_options,
       GRegexMatchFlags match_options);
extern
gchar **g_regex_split (const GRegex *regex,
       const gchar *string,
       GRegexMatchFlags match_options);
extern
gchar **g_regex_split_full (const GRegex *regex,
       const gchar *string,
       gssize string_len,
       gint start_position,
       GRegexMatchFlags match_options,
       gint max_tokens,
       GError **error);
extern
gchar *g_regex_replace (const GRegex *regex,
       const gchar *string,
       gssize string_len,
       gint start_position,
       const gchar *replacement,
       GRegexMatchFlags match_options,
       GError **error);
extern
gchar *g_regex_replace_literal (const GRegex *regex,
       const gchar *string,
       gssize string_len,
       gint start_position,
       const gchar *replacement,
       GRegexMatchFlags match_options,
       GError **error);
extern
gchar *g_regex_replace_eval (const GRegex *regex,
       const gchar *string,
       gssize string_len,
       gint start_position,
       GRegexMatchFlags match_options,
       GRegexEvalCallback eval,
       gpointer user_data,
       GError **error);
extern
gboolean g_regex_check_replacement (const gchar *replacement,
       gboolean *has_references,
       GError **error);
extern
GRegex *g_match_info_get_regex (const GMatchInfo *match_info);
extern
const gchar *g_match_info_get_string (const GMatchInfo *match_info);
extern
GMatchInfo *g_match_info_ref (GMatchInfo *match_info);
extern
void g_match_info_unref (GMatchInfo *match_info);
extern
void g_match_info_free (GMatchInfo *match_info);
extern
gboolean g_match_info_next (GMatchInfo *match_info,
       GError **error);
extern
gboolean g_match_info_matches (const GMatchInfo *match_info);
extern
gint g_match_info_get_match_count (const GMatchInfo *match_info);
extern
gboolean g_match_info_is_partial_match (const GMatchInfo *match_info);
extern
gchar *g_match_info_expand_references(const GMatchInfo *match_info,
       const gchar *string_to_expand,
       GError **error);
extern
gchar *g_match_info_fetch (const GMatchInfo *match_info,
       gint match_num);
extern
gboolean g_match_info_fetch_pos (const GMatchInfo *match_info,
       gint match_num,
       gint *start_pos,
       gint *end_pos);
extern
gchar *g_match_info_fetch_named (const GMatchInfo *match_info,
       const gchar *name);
extern
gboolean g_match_info_fetch_named_pos (const GMatchInfo *match_info,
       const gchar *name,
       gint *start_pos,
       gint *end_pos);
extern
gchar **g_match_info_fetch_all (const GMatchInfo *match_info);
typedef struct _GScanner GScanner;
typedef struct _GScannerConfig GScannerConfig;
typedef union _GTokenValue GTokenValue;
typedef void (*GScannerMsgFunc) (GScanner *scanner,
       gchar *message,
       gboolean error);
typedef enum
{
  G_ERR_UNKNOWN,
  G_ERR_UNEXP_EOF,
  G_ERR_UNEXP_EOF_IN_STRING,
  G_ERR_UNEXP_EOF_IN_COMMENT,
  G_ERR_NON_DIGIT_IN_CONST,
  G_ERR_DIGIT_RADIX,
  G_ERR_FLOAT_RADIX,
  G_ERR_FLOAT_MALFORMED
} GErrorType;
typedef enum
{
  G_TOKEN_EOF = 0,
  G_TOKEN_LEFT_PAREN = '(',
  G_TOKEN_RIGHT_PAREN = ')',
  G_TOKEN_LEFT_CURLY = '{',
  G_TOKEN_RIGHT_CURLY = '}',
  G_TOKEN_LEFT_BRACE = '[',
  G_TOKEN_RIGHT_BRACE = ']',
  G_TOKEN_EQUAL_SIGN = '=',
  G_TOKEN_COMMA = ',',
  G_TOKEN_NONE = 256,
  G_TOKEN_ERROR,
  G_TOKEN_CHAR,
  G_TOKEN_BINARY,
  G_TOKEN_OCTAL,
  G_TOKEN_INT,
  G_TOKEN_HEX,
  G_TOKEN_FLOAT,
  G_TOKEN_STRING,
  G_TOKEN_SYMBOL,
  G_TOKEN_IDENTIFIER,
  G_TOKEN_IDENTIFIER_NULL,
  G_TOKEN_COMMENT_SINGLE,
  G_TOKEN_COMMENT_MULTI,
  G_TOKEN_LAST
} GTokenType;
union _GTokenValue
{
  gpointer v_symbol;
  gchar *v_identifier;
  gulong v_binary;
  gulong v_octal;
  gulong v_int;
  guint64 v_int64;
  gdouble v_float;
  gulong v_hex;
  gchar *v_string;
  gchar *v_comment;
  guchar v_char;
  guint v_error;
};
struct _GScannerConfig
{
  gchar *cset_skip_characters;
  gchar *cset_identifier_first;
  gchar *cset_identifier_nth;
  gchar *cpair_comment_single;
  guint case_sensitive : 1;
  guint skip_comment_multi : 1;
  guint skip_comment_single : 1;
  guint scan_comment_multi : 1;
  guint scan_identifier : 1;
  guint scan_identifier_1char : 1;
  guint scan_identifier_NULL : 1;
  guint scan_symbols : 1;
  guint scan_binary : 1;
  guint scan_octal : 1;
  guint scan_float : 1;
  guint scan_hex : 1;
  guint scan_hex_dollar : 1;
  guint scan_string_sq : 1;
  guint scan_string_dq : 1;
  guint numbers_2_int : 1;
  guint int_2_float : 1;
  guint identifier_2_string : 1;
  guint char_2_token : 1;
  guint symbol_2_token : 1;
  guint scope_0_fallback : 1;
  guint store_int64 : 1;
  guint padding_dummy;
};
struct _GScanner
{
  gpointer user_data;
  guint max_parse_errors;
  guint parse_errors;
  const gchar *input_name;
  GData *qdata;
  GScannerConfig *config;
  GTokenType token;
  GTokenValue value;
  guint line;
  guint position;
  GTokenType next_token;
  GTokenValue next_value;
  guint next_line;
  guint next_position;
  GHashTable *symbol_table;
  gint input_fd;
  const gchar *text;
  const gchar *text_end;
  gchar *buffer;
  guint scope_id;
  GScannerMsgFunc msg_handler;
};
extern
GScanner* g_scanner_new (const GScannerConfig *config_templ);
extern
void g_scanner_destroy (GScanner *scanner);
extern
void g_scanner_input_file (GScanner *scanner,
       gint input_fd);
extern
void g_scanner_sync_file_offset (GScanner *scanner);
extern
void g_scanner_input_text (GScanner *scanner,
       const gchar *text,
       guint text_len);
extern
GTokenType g_scanner_get_next_token (GScanner *scanner);
extern
GTokenType g_scanner_peek_next_token (GScanner *scanner);
extern
GTokenType g_scanner_cur_token (GScanner *scanner);
extern
GTokenValue g_scanner_cur_value (GScanner *scanner);
extern
guint g_scanner_cur_line (GScanner *scanner);
extern
guint g_scanner_cur_position (GScanner *scanner);
extern
gboolean g_scanner_eof (GScanner *scanner);
extern
guint g_scanner_set_scope (GScanner *scanner,
       guint scope_id);
extern
void g_scanner_scope_add_symbol (GScanner *scanner,
       guint scope_id,
       const gchar *symbol,
       gpointer value);
extern
void g_scanner_scope_remove_symbol (GScanner *scanner,
       guint scope_id,
       const gchar *symbol);
extern
gpointer g_scanner_scope_lookup_symbol (GScanner *scanner,
       guint scope_id,
       const gchar *symbol);
extern
void g_scanner_scope_foreach_symbol (GScanner *scanner,
       guint scope_id,
       GHFunc func,
       gpointer user_data);
extern
gpointer g_scanner_lookup_symbol (GScanner *scanner,
       const gchar *symbol);
extern
void g_scanner_unexp_token (GScanner *scanner,
       GTokenType expected_token,
       const gchar *identifier_spec,
       const gchar *symbol_spec,
       const gchar *symbol_name,
       const gchar *message,
       gint is_error);
extern
void g_scanner_error (GScanner *scanner,
       const gchar *format,
       ...) ;
extern
void g_scanner_warn (GScanner *scanner,
       const gchar *format,
       ...) ;
typedef struct _GSequence GSequence;
typedef struct _GSequenceNode GSequenceIter;
typedef gint (* GSequenceIterCompareFunc) (GSequenceIter *a,
                                           GSequenceIter *b,
                                           gpointer data);
extern
GSequence * g_sequence_new (GDestroyNotify data_destroy);
extern
void g_sequence_free (GSequence *seq);
extern
gint g_sequence_get_length (GSequence *seq);
extern
void g_sequence_foreach (GSequence *seq,
                                              GFunc func,
                                              gpointer user_data);
extern
void g_sequence_foreach_range (GSequenceIter *begin,
                                              GSequenceIter *end,
                                              GFunc func,
                                              gpointer user_data);
extern
void g_sequence_sort (GSequence *seq,
                                              GCompareDataFunc cmp_func,
                                              gpointer cmp_data);
extern
void g_sequence_sort_iter (GSequence *seq,
                                              GSequenceIterCompareFunc cmp_func,
                                              gpointer cmp_data);
extern
gboolean g_sequence_is_empty (GSequence *seq);
extern
GSequenceIter *g_sequence_get_begin_iter (GSequence *seq);
extern
GSequenceIter *g_sequence_get_end_iter (GSequence *seq);
extern
GSequenceIter *g_sequence_get_iter_at_pos (GSequence *seq,
                                              gint pos);
extern
GSequenceIter *g_sequence_append (GSequence *seq,
                                              gpointer data);
extern
GSequenceIter *g_sequence_prepend (GSequence *seq,
                                              gpointer data);
extern
GSequenceIter *g_sequence_insert_before (GSequenceIter *iter,
                                              gpointer data);
extern
void g_sequence_move (GSequenceIter *src,
                                              GSequenceIter *dest);
extern
void g_sequence_swap (GSequenceIter *a,
                                              GSequenceIter *b);
extern
GSequenceIter *g_sequence_insert_sorted (GSequence *seq,
                                              gpointer data,
                                              GCompareDataFunc cmp_func,
                                              gpointer cmp_data);
extern
GSequenceIter *g_sequence_insert_sorted_iter (GSequence *seq,
                                              gpointer data,
                                              GSequenceIterCompareFunc iter_cmp,
                                              gpointer cmp_data);
extern
void g_sequence_sort_changed (GSequenceIter *iter,
                                              GCompareDataFunc cmp_func,
                                              gpointer cmp_data);
extern
void g_sequence_sort_changed_iter (GSequenceIter *iter,
                                              GSequenceIterCompareFunc iter_cmp,
                                              gpointer cmp_data);
extern
void g_sequence_remove (GSequenceIter *iter);
extern
void g_sequence_remove_range (GSequenceIter *begin,
                                              GSequenceIter *end);
extern
void g_sequence_move_range (GSequenceIter *dest,
                                              GSequenceIter *begin,
                                              GSequenceIter *end);
extern
GSequenceIter *g_sequence_search (GSequence *seq,
                                              gpointer data,
                                              GCompareDataFunc cmp_func,
                                              gpointer cmp_data);
extern
GSequenceIter *g_sequence_search_iter (GSequence *seq,
                                              gpointer data,
                                              GSequenceIterCompareFunc iter_cmp,
                                              gpointer cmp_data);
extern
GSequenceIter *g_sequence_lookup (GSequence *seq,
                                              gpointer data,
                                              GCompareDataFunc cmp_func,
                                              gpointer cmp_data);
extern
GSequenceIter *g_sequence_lookup_iter (GSequence *seq,
                                              gpointer data,
                                              GSequenceIterCompareFunc iter_cmp,
                                              gpointer cmp_data);
extern
gpointer g_sequence_get (GSequenceIter *iter);
extern
void g_sequence_set (GSequenceIter *iter,
                                              gpointer data);
extern
gboolean g_sequence_iter_is_begin (GSequenceIter *iter);
extern
gboolean g_sequence_iter_is_end (GSequenceIter *iter);
extern
GSequenceIter *g_sequence_iter_next (GSequenceIter *iter);
extern
GSequenceIter *g_sequence_iter_prev (GSequenceIter *iter);
extern
gint g_sequence_iter_get_position (GSequenceIter *iter);
extern
GSequenceIter *g_sequence_iter_move (GSequenceIter *iter,
                                              gint delta);
extern
GSequence * g_sequence_iter_get_sequence (GSequenceIter *iter);
extern
gint g_sequence_iter_compare (GSequenceIter *a,
                                              GSequenceIter *b);
extern
GSequenceIter *g_sequence_range_get_midpoint (GSequenceIter *begin,
                                              GSequenceIter *end);
typedef enum
{
  G_SHELL_ERROR_BAD_QUOTING,
  G_SHELL_ERROR_EMPTY_STRING,
  G_SHELL_ERROR_FAILED
} GShellError;
extern
GQuark g_shell_error_quark (void);
extern
gchar* g_shell_quote (const gchar *unquoted_string);
extern
gchar* g_shell_unquote (const gchar *quoted_string,
                             GError **error);
extern
gboolean g_shell_parse_argv (const gchar *command_line,
                             gint *argcp,
                             gchar ***argvp,
                             GError **error);
extern
gpointer g_slice_alloc (gsize block_size) ;
extern
gpointer g_slice_alloc0 (gsize block_size) ;
extern
gpointer g_slice_copy (gsize block_size,
                                         gconstpointer mem_block) ;
extern
void g_slice_free1 (gsize block_size,
      gpointer mem_block);
extern
void g_slice_free_chain_with_offset (gsize block_size,
      gpointer mem_chain,
      gsize next_offset);
typedef enum {
  G_SLICE_CONFIG_ALWAYS_MALLOC = 1,
  G_SLICE_CONFIG_BYPASS_MAGAZINES,
  G_SLICE_CONFIG_WORKING_SET_MSECS,
  G_SLICE_CONFIG_COLOR_INCREMENT,
  G_SLICE_CONFIG_CHUNK_SIZES,
  G_SLICE_CONFIG_CONTENTION_COUNTER
} GSliceConfig;
 extern
void g_slice_set_config (GSliceConfig ckey, gint64 value);
 extern
gint64 g_slice_get_config (GSliceConfig ckey);
 extern
gint64* g_slice_get_config_state (GSliceConfig ckey, gint64 address, guint *n_values);
typedef enum
{
  G_SPAWN_ERROR_FORK,
  G_SPAWN_ERROR_READ,
  G_SPAWN_ERROR_CHDIR,
  G_SPAWN_ERROR_ACCES,
  G_SPAWN_ERROR_PERM,
  G_SPAWN_ERROR_TOO_BIG,
  G_SPAWN_ERROR_2BIG = G_SPAWN_ERROR_TOO_BIG,
  G_SPAWN_ERROR_NOEXEC,
  G_SPAWN_ERROR_NAMETOOLONG,
  G_SPAWN_ERROR_NOENT,
  G_SPAWN_ERROR_NOMEM,
  G_SPAWN_ERROR_NOTDIR,
  G_SPAWN_ERROR_LOOP,
  G_SPAWN_ERROR_TXTBUSY,
  G_SPAWN_ERROR_IO,
  G_SPAWN_ERROR_NFILE,
  G_SPAWN_ERROR_MFILE,
  G_SPAWN_ERROR_INVAL,
  G_SPAWN_ERROR_ISDIR,
  G_SPAWN_ERROR_LIBBAD,
  G_SPAWN_ERROR_FAILED
} GSpawnError;
typedef void (* GSpawnChildSetupFunc) (gpointer user_data);
typedef enum
{
  G_SPAWN_DEFAULT = 0,
  G_SPAWN_LEAVE_DESCRIPTORS_OPEN = 1 << 0,
  G_SPAWN_DO_NOT_REAP_CHILD = 1 << 1,
  G_SPAWN_SEARCH_PATH = 1 << 2,
  G_SPAWN_STDOUT_TO_DEV_NULL = 1 << 3,
  G_SPAWN_STDERR_TO_DEV_NULL = 1 << 4,
  G_SPAWN_CHILD_INHERITS_STDIN = 1 << 5,
  G_SPAWN_FILE_AND_ARGV_ZERO = 1 << 6,
  G_SPAWN_SEARCH_PATH_FROM_ENVP = 1 << 7,
  G_SPAWN_CLOEXEC_PIPES = 1 << 8
} GSpawnFlags;
extern
GQuark g_spawn_error_quark (void);
extern
GQuark g_spawn_exit_error_quark (void);
extern
gboolean g_spawn_async (const gchar *working_directory,
                        gchar **argv,
                        gchar **envp,
                        GSpawnFlags flags,
                        GSpawnChildSetupFunc child_setup,
                        gpointer user_data,
                        GPid *child_pid,
                        GError **error);
extern
gboolean g_spawn_async_with_pipes (const gchar *working_directory,
                                   gchar **argv,
                                   gchar **envp,
                                   GSpawnFlags flags,
                                   GSpawnChildSetupFunc child_setup,
                                   gpointer user_data,
                                   GPid *child_pid,
                                   gint *standard_input,
                                   gint *standard_output,
                                   gint *standard_error,
                                   GError **error);
extern
gboolean g_spawn_async_with_pipes_and_fds (const gchar *working_directory,
                                           const gchar * const *argv,
                                           const gchar * const *envp,
                                           GSpawnFlags flags,
                                           GSpawnChildSetupFunc child_setup,
                                           gpointer user_data,
                                           gint stdin_fd,
                                           gint stdout_fd,
                                           gint stderr_fd,
                                           const gint *source_fds,
                                           const gint *target_fds,
                                           gsize n_fds,
                                           GPid *child_pid_out,
                                           gint *stdin_pipe_out,
                                           gint *stdout_pipe_out,
                                           gint *stderr_pipe_out,
                                           GError **error);
extern
gboolean g_spawn_async_with_fds (const gchar *working_directory,
                                 gchar **argv,
                                 gchar **envp,
                                 GSpawnFlags flags,
                                 GSpawnChildSetupFunc child_setup,
                                 gpointer user_data,
                                 GPid *child_pid,
                                 gint stdin_fd,
                                 gint stdout_fd,
                                 gint stderr_fd,
                                 GError **error);
extern
gboolean g_spawn_sync (const gchar *working_directory,
                               gchar **argv,
                               gchar **envp,
                               GSpawnFlags flags,
                               GSpawnChildSetupFunc child_setup,
                               gpointer user_data,
                               gchar **standard_output,
                               gchar **standard_error,
                               gint *wait_status,
                               GError **error);
extern
gboolean g_spawn_command_line_sync (const gchar *command_line,
                                     gchar **standard_output,
                                     gchar **standard_error,
                                     gint *wait_status,
                                     GError **error);
extern
gboolean g_spawn_command_line_async (const gchar *command_line,
                                     GError **error);
extern
gboolean g_spawn_check_wait_status (gint wait_status,
                                    GError **error);
 extern
gboolean g_spawn_check_exit_status (gint wait_status,
        GError **error);
extern
void g_spawn_close_pid (GPid pid);
typedef enum {
  G_ASCII_ALNUM = 1 << 0,
  G_ASCII_ALPHA = 1 << 1,
  G_ASCII_CNTRL = 1 << 2,
  G_ASCII_DIGIT = 1 << 3,
  G_ASCII_GRAPH = 1 << 4,
  G_ASCII_LOWER = 1 << 5,
  G_ASCII_PRINT = 1 << 6,
  G_ASCII_PUNCT = 1 << 7,
  G_ASCII_SPACE = 1 << 8,
  G_ASCII_UPPER = 1 << 9,
  G_ASCII_XDIGIT = 1 << 10
} GAsciiType;
extern const guint16 * const g_ascii_table;
extern
gchar g_ascii_tolower (gchar c) ;
extern
gchar g_ascii_toupper (gchar c) ;
extern
gint g_ascii_digit_value (gchar c) ;
extern
gint g_ascii_xdigit_value (gchar c) ;
extern
gchar* g_strdelimit (gchar *string,
     const gchar *delimiters,
     gchar new_delimiter);
extern
gchar* g_strcanon (gchar *string,
     const gchar *valid_chars,
     gchar substitutor);
extern
const gchar * g_strerror (gint errnum) ;
extern
const gchar * g_strsignal (gint signum) ;
extern
gchar * g_strreverse (gchar *string);
extern
gsize g_strlcpy (gchar *dest,
     const gchar *src,
     gsize dest_size);
extern
gsize g_strlcat (gchar *dest,
     const gchar *src,
     gsize dest_size);
extern
gchar * g_strstr_len (const gchar *haystack,
     gssize haystack_len,
     const gchar *needle);
extern
gchar * g_strrstr (const gchar *haystack,
     const gchar *needle);
extern
gchar * g_strrstr_len (const gchar *haystack,
     gssize haystack_len,
     const gchar *needle);
extern
gboolean g_str_has_suffix (const gchar *str,
     const gchar *suffix);
extern
gboolean g_str_has_prefix (const gchar *str,
     const gchar *prefix);
extern
gdouble g_strtod (const gchar *nptr,
     gchar **endptr);
extern
gdouble g_ascii_strtod (const gchar *nptr,
     gchar **endptr);
extern
guint64 g_ascii_strtoull (const gchar *nptr,
     gchar **endptr,
     guint base);
extern
gint64 g_ascii_strtoll (const gchar *nptr,
     gchar **endptr,
     guint base);
extern
gchar * g_ascii_dtostr (gchar *buffer,
     gint buf_len,
     gdouble d);
extern
gchar * g_ascii_formatd (gchar *buffer,
     gint buf_len,
     const gchar *format,
     gdouble d);
extern
gchar* g_strchug (gchar *string);
extern
gchar* g_strchomp (gchar *string);
extern
gint g_ascii_strcasecmp (const gchar *s1,
        const gchar *s2);
extern
gint g_ascii_strncasecmp (const gchar *s1,
        const gchar *s2,
        gsize n);
extern
gchar* g_ascii_strdown (const gchar *str,
        gssize len) ;
extern
gchar* g_ascii_strup (const gchar *str,
        gssize len) ;
extern
gboolean g_str_is_ascii (const gchar *str);
 extern
gint g_strcasecmp (const gchar *s1,
                                        const gchar *s2);
 extern
gint g_strncasecmp (const gchar *s1,
                                        const gchar *s2,
                                        guint n);
 extern
gchar* g_strdown (gchar *string);
 extern
gchar* g_strup (gchar *string);
extern
gchar* g_strdup (const gchar *str) ;
extern
gchar* g_strdup_printf (const gchar *format,
     ...) ;
extern
gchar* g_strdup_vprintf (const gchar *format,
     va_list args) ;
extern
gchar* g_strndup (const gchar *str,
     gsize n) ;
extern
gchar* g_strnfill (gsize length,
     gchar fill_char) ;
extern
gchar* g_strconcat (const gchar *string1,
     ...) ;
extern
gchar* g_strjoin (const gchar *separator,
     ...) ;
extern
gchar* g_strcompress (const gchar *source) ;
extern
gchar* g_strescape (const gchar *source,
     const gchar *exceptions) ;
 extern
gpointer g_memdup (gconstpointer mem,
                                        guint byte_size) ;
extern
gpointer g_memdup2 (gconstpointer mem,
                                        gsize byte_size) ;
typedef gchar** GStrv;
extern
gchar** g_strsplit (const gchar *string,
     const gchar *delimiter,
     gint max_tokens);
extern
gchar ** g_strsplit_set (const gchar *string,
     const gchar *delimiters,
     gint max_tokens);
extern
gchar* g_strjoinv (const gchar *separator,
     gchar **str_array) ;
extern
void g_strfreev (gchar **str_array);
extern
gchar** g_strdupv (gchar **str_array);
extern
guint g_strv_length (gchar **str_array);
extern
gchar* g_stpcpy (gchar *dest,
                                        const char *src);
extern
gchar * g_str_to_ascii (const gchar *str,
                                                                         const gchar *from_locale);
extern
gchar ** g_str_tokenize_and_fold (const gchar *string,
                                                                         const gchar *translit_locale,
                                                                         gchar ***ascii_alternates);
extern
gboolean g_str_match_string (const gchar *search_term,
                                                                         const gchar *potential_hit,
                                                                         gboolean accept_alternates);
extern
gboolean g_strv_contains (const gchar * const *strv,
                                        const gchar *str);
extern
gboolean g_strv_equal (const gchar * const *strv1,
                                        const gchar * const *strv2);
typedef enum
  {
    G_NUMBER_PARSER_ERROR_INVALID,
    G_NUMBER_PARSER_ERROR_OUT_OF_BOUNDS,
  } GNumberParserError;
extern
GQuark g_number_parser_error_quark (void);
extern
gboolean g_ascii_string_to_signed (const gchar *str,
                                                    guint base,
                                                    gint64 min,
                                                    gint64 max,
                                                    gint64 *out_num,
                                                    GError **error);
extern
gboolean g_ascii_string_to_unsigned (const gchar *str,
                                                    guint base,
                                                    guint64 min,
                                                    guint64 max,
                                                    guint64 *out_num,
                                                    GError **error);
typedef struct _GStringChunk GStringChunk;
extern
GStringChunk* g_string_chunk_new (gsize size);
extern
void g_string_chunk_free (GStringChunk *chunk);
extern
void g_string_chunk_clear (GStringChunk *chunk);
extern
gchar* g_string_chunk_insert (GStringChunk *chunk,
                                           const gchar *string);
extern
gchar* g_string_chunk_insert_len (GStringChunk *chunk,
                                           const gchar *string,
                                           gssize len);
extern
gchar* g_string_chunk_insert_const (GStringChunk *chunk,
                                           const gchar *string);
typedef struct _GStrvBuilder GStrvBuilder;
extern
GStrvBuilder *g_strv_builder_new (void);
extern
void g_strv_builder_unref (GStrvBuilder *builder);
extern
GStrvBuilder *g_strv_builder_ref (GStrvBuilder *builder);
extern
void g_strv_builder_add (GStrvBuilder *builder,
                         const char *value);
extern
void g_strv_builder_addv (GStrvBuilder *builder,
                          const char **value);
extern
void g_strv_builder_add_many (GStrvBuilder *builder,
                              ...) ;
extern
GStrv g_strv_builder_end (GStrvBuilder *builder);
typedef struct GTestCase GTestCase;
typedef struct GTestSuite GTestSuite;
typedef void (*GTestFunc) (void);
typedef void (*GTestDataFunc) (gconstpointer user_data);
typedef void (*GTestFixtureFunc) (gpointer fixture,
                                  gconstpointer user_data);
extern
int g_strcmp0 (const char *str1,
                                         const char *str2);
extern
void g_test_minimized_result (double minimized_quantity,
                                         const char *format,
                                         ...) ;
extern
void g_test_maximized_result (double maximized_quantity,
                                         const char *format,
                                         ...) ;
extern
void g_test_init (int *argc,
                                         char ***argv,
                                         ...) ;
extern
gboolean g_test_subprocess (void);
extern
int g_test_run (void);
extern
void g_test_add_func (const char *testpath,
                                         GTestFunc test_func);
extern
void g_test_add_data_func (const char *testpath,
                                         gconstpointer test_data,
                                         GTestDataFunc test_func);
extern
void g_test_add_data_func_full (const char *testpath,
                                         gpointer test_data,
                                         GTestDataFunc test_func,
                                         GDestroyNotify data_free_func);
extern
const char * g_test_get_path (void);
extern
void g_test_fail (void);
extern
void g_test_fail_printf (const char *format,
                                         ...) ;
extern
void g_test_incomplete (const gchar *msg);
extern
void g_test_incomplete_printf (const char *format,
                                         ...) ;
extern
void g_test_skip (const gchar *msg);
extern
void g_test_skip_printf (const char *format,
                                         ...) ;
extern
gboolean g_test_failed (void);
extern
void g_test_set_nonfatal_assertions (void);
extern
void g_test_message (const char *format,
                                         ...) ;
extern
void g_test_bug_base (const char *uri_pattern);
extern
void g_test_bug (const char *bug_uri_snippet);
extern
void g_test_summary (const char *summary);
extern
void g_test_timer_start (void);
extern
double g_test_timer_elapsed (void);
extern
double g_test_timer_last (void);
extern
void g_test_queue_free (gpointer gfree_pointer);
extern
void g_test_queue_destroy (GDestroyNotify destroy_func,
                                         gpointer destroy_data);
typedef enum {
  G_TEST_TRAP_SILENCE_STDOUT = 1 << 7,
  G_TEST_TRAP_SILENCE_STDERR = 1 << 8,
  G_TEST_TRAP_INHERIT_STDIN = 1 << 9
} GTestTrapFlags ;
 extern
gboolean g_test_trap_fork (guint64 usec_timeout,
                                         GTestTrapFlags test_trap_flags);
typedef enum {
  G_TEST_SUBPROCESS_INHERIT_STDIN = 1 << 0,
  G_TEST_SUBPROCESS_INHERIT_STDOUT = 1 << 1,
  G_TEST_SUBPROCESS_INHERIT_STDERR = 1 << 2
} GTestSubprocessFlags;
extern
void g_test_trap_subprocess (const char *test_path,
                                         guint64 usec_timeout,
                                         GTestSubprocessFlags test_flags);
extern
gboolean g_test_trap_has_passed (void);
extern
gboolean g_test_trap_reached_timeout (void);
extern
gint32 g_test_rand_int (void);
extern
gint32 g_test_rand_int_range (gint32 begin,
                                         gint32 end);
extern
double g_test_rand_double (void);
extern
double g_test_rand_double_range (double range_start,
                                         double range_end);
extern
GTestCase* g_test_create_case (const char *test_name,
                                         gsize data_size,
                                         gconstpointer test_data,
                                         GTestFixtureFunc data_setup,
                                         GTestFixtureFunc data_test,
                                         GTestFixtureFunc data_teardown);
extern
GTestSuite* g_test_create_suite (const char *suite_name);
extern
GTestSuite* g_test_get_root (void);
extern
void g_test_suite_add (GTestSuite *suite,
                                         GTestCase *test_case);
extern
void g_test_suite_add_suite (GTestSuite *suite,
                                         GTestSuite *nestedsuite);
extern
int g_test_run_suite (GTestSuite *suite);
extern
void g_test_case_free (GTestCase *test_case);
extern
void g_test_suite_free (GTestSuite *suite);
extern
void g_test_trap_assertions (const char *domain,
                                         const char *file,
                                         int line,
                                         const char *func,
                                         guint64 assertion_flags,
                                         const char *pattern);
extern
void g_assertion_message (const char *domain,
                                         const char *file,
                                         int line,
                                         const char *func,
                                         const char *message) ;
extern
void g_assertion_message_expr (const char *domain,
                                         const char *file,
                                         int line,
                                         const char *func,
                                         const char *expr);
extern
void g_assertion_message_cmpstr (const char *domain,
                                         const char *file,
                                         int line,
                                         const char *func,
                                         const char *expr,
                                         const char *arg1,
                                         const char *cmp,
                                         const char *arg2) ;
extern
void g_assertion_message_cmpstrv (const char *domain,
                                         const char *file,
                                         int line,
                                         const char *func,
                                         const char *expr,
                                         const char * const *arg1,
                                         const char * const *arg2,
                                         gsize first_wrong_idx) ;
extern
void g_assertion_message_cmpnum (const char *domain,
                                         const char *file,
                                         int line,
                                         const char *func,
                                         const char *expr,
                                         long double arg1,
                                         const char *cmp,
                                         long double arg2,
                                         char numtype) ;
extern
void g_assertion_message_error (const char *domain,
                                         const char *file,
                                         int line,
                                         const char *func,
                                         const char *expr,
                                         const GError *error,
                                         GQuark error_domain,
                                         int error_code) ;
extern
void g_test_add_vtable (const char *testpath,
                                         gsize data_size,
                                         gconstpointer test_data,
                                         GTestFixtureFunc data_setup,
                                         GTestFixtureFunc data_test,
                                         GTestFixtureFunc data_teardown);
typedef struct {
  gboolean test_initialized;
  gboolean test_quick;
  gboolean test_perf;
  gboolean test_verbose;
  gboolean test_quiet;
  gboolean test_undefined;
} GTestConfig;
extern const GTestConfig * const g_test_config_vars;
typedef enum {
  G_TEST_RUN_SUCCESS,
  G_TEST_RUN_SKIPPED,
  G_TEST_RUN_FAILURE,
  G_TEST_RUN_INCOMPLETE
} GTestResult;
typedef enum {
  G_TEST_LOG_NONE,
  G_TEST_LOG_ERROR,
  G_TEST_LOG_START_BINARY,
  G_TEST_LOG_LIST_CASE,
  G_TEST_LOG_SKIP_CASE,
  G_TEST_LOG_START_CASE,
  G_TEST_LOG_STOP_CASE,
  G_TEST_LOG_MIN_RESULT,
  G_TEST_LOG_MAX_RESULT,
  G_TEST_LOG_MESSAGE,
  G_TEST_LOG_START_SUITE,
  G_TEST_LOG_STOP_SUITE
} GTestLogType;
typedef struct {
  GTestLogType log_type;
  guint n_strings;
  gchar **strings;
  guint n_nums;
  long double *nums;
} GTestLogMsg;
typedef struct {
  GString *data;
  GSList *msgs;
} GTestLogBuffer;
extern
const char* g_test_log_type_name (GTestLogType log_type);
extern
GTestLogBuffer* g_test_log_buffer_new (void);
extern
void g_test_log_buffer_free (GTestLogBuffer *tbuffer);
extern
void g_test_log_buffer_push (GTestLogBuffer *tbuffer,
                                         guint n_bytes,
                                         const guint8 *bytes);
extern
GTestLogMsg* g_test_log_buffer_pop (GTestLogBuffer *tbuffer);
extern
void g_test_log_msg_free (GTestLogMsg *tmsg);
typedef gboolean (*GTestLogFatalFunc) (const gchar *log_domain,
                                                 GLogLevelFlags log_level,
                                                 const gchar *message,
                                                 gpointer user_data);
extern
void
g_test_log_set_fatal_handler (GTestLogFatalFunc log_func,
                                         gpointer user_data);
extern
void g_test_expect_message (const gchar *log_domain,
                                                  GLogLevelFlags log_level,
                                                  const gchar *pattern);
extern
void g_test_assert_expected_messages_internal (const char *domain,
                                                  const char *file,
                                                  int line,
                                                  const char *func);
typedef enum
{
  G_TEST_DIST,
  G_TEST_BUILT
} GTestFileType;
extern
gchar * g_test_build_filename (GTestFileType file_type,
                                                  const gchar *first_path,
                                                  ...) ;
extern
const gchar *g_test_get_dir (GTestFileType file_type);
extern
const gchar *g_test_get_filename (GTestFileType file_type,
                                                  const gchar *first_path,
                                                  ...) ;
typedef struct _GThreadPool GThreadPool;
struct _GThreadPool
{
  GFunc func;
  gpointer user_data;
  gboolean exclusive;
};
extern
GThreadPool * g_thread_pool_new (GFunc func,
                                                 gpointer user_data,
                                                 gint max_threads,
                                                 gboolean exclusive,
                                                 GError **error);
extern
GThreadPool * g_thread_pool_new_full (GFunc func,
                                                 gpointer user_data,
                                                 GDestroyNotify item_free_func,
                                                 gint max_threads,
                                                 gboolean exclusive,
                                                 GError **error);
extern
void g_thread_pool_free (GThreadPool *pool,
                                                 gboolean immediate,
                                                 gboolean wait_);
extern
gboolean g_thread_pool_push (GThreadPool *pool,
                                                 gpointer data,
                                                 GError **error);
extern
guint g_thread_pool_unprocessed (GThreadPool *pool);
extern
void g_thread_pool_set_sort_function (GThreadPool *pool,
                                                 GCompareDataFunc func,
                                                 gpointer user_data);
extern
gboolean g_thread_pool_move_to_front (GThreadPool *pool,
                                                 gpointer data);
extern
gboolean g_thread_pool_set_max_threads (GThreadPool *pool,
                                                 gint max_threads,
                                                 GError **error);
extern
gint g_thread_pool_get_max_threads (GThreadPool *pool);
extern
guint g_thread_pool_get_num_threads (GThreadPool *pool);
extern
void g_thread_pool_set_max_unused_threads (gint max_threads);
extern
gint g_thread_pool_get_max_unused_threads (void);
extern
guint g_thread_pool_get_num_unused_threads (void);
extern
void g_thread_pool_stop_unused_threads (void);
extern
void g_thread_pool_set_max_idle_time (guint interval);
extern
guint g_thread_pool_get_max_idle_time (void);
typedef struct _GTimer GTimer;
extern
GTimer* g_timer_new (void);
extern
void g_timer_destroy (GTimer *timer);
extern
void g_timer_start (GTimer *timer);
extern
void g_timer_stop (GTimer *timer);
extern
void g_timer_reset (GTimer *timer);
extern
void g_timer_continue (GTimer *timer);
extern
gdouble g_timer_elapsed (GTimer *timer,
      gulong *microseconds);
extern
gboolean g_timer_is_active (GTimer *timer);
extern
void g_usleep (gulong microseconds);
 extern
void g_time_val_add (GTimeVal *time_,
                                  glong microseconds);
 extern
gboolean g_time_val_from_iso8601 (const gchar *iso_date,
      GTimeVal *time_);
 extern
gchar* g_time_val_to_iso8601 (GTimeVal *time_) ;
typedef struct _GTrashStack GTrashStack ;
struct _GTrashStack
{
  GTrashStack *next;
} ;
 extern
void g_trash_stack_push (GTrashStack **stack_p,
                                gpointer data_p);
 extern
gpointer g_trash_stack_pop (GTrashStack **stack_p);
 extern
gpointer g_trash_stack_peek (GTrashStack **stack_p);
 extern
guint g_trash_stack_height (GTrashStack **stack_p);
typedef struct _GTree GTree;
typedef struct _GTreeNode GTreeNode;
typedef gboolean (*GTraverseFunc) (gpointer key,
                                   gpointer value,
                                   gpointer data);
typedef gboolean (*GTraverseNodeFunc) (GTreeNode *node,
                                       gpointer data);
extern
GTree* g_tree_new (GCompareFunc key_compare_func);
extern
GTree* g_tree_new_with_data (GCompareDataFunc key_compare_func,
                                 gpointer key_compare_data);
extern
GTree* g_tree_new_full (GCompareDataFunc key_compare_func,
                                 gpointer key_compare_data,
                                 GDestroyNotify key_destroy_func,
                                 GDestroyNotify value_destroy_func);
extern
GTreeNode *g_tree_node_first (GTree *tree);
extern
GTreeNode *g_tree_node_last (GTree *tree);
extern
GTreeNode *g_tree_node_previous (GTreeNode *node);
extern
GTreeNode *g_tree_node_next (GTreeNode *node);
extern
GTree* g_tree_ref (GTree *tree);
extern
void g_tree_unref (GTree *tree);
extern
void g_tree_destroy (GTree *tree);
extern
GTreeNode *g_tree_insert_node (GTree *tree,
                               gpointer key,
                               gpointer value);
extern
void g_tree_insert (GTree *tree,
                                 gpointer key,
                                 gpointer value);
extern
GTreeNode *g_tree_replace_node (GTree *tree,
                                gpointer key,
                                gpointer value);
extern
void g_tree_replace (GTree *tree,
                                 gpointer key,
                                 gpointer value);
extern
gboolean g_tree_remove (GTree *tree,
                                 gconstpointer key);
extern
void g_tree_remove_all (GTree *tree);
extern
gboolean g_tree_steal (GTree *tree,
                                 gconstpointer key);
extern
gpointer g_tree_node_key (GTreeNode *node);
extern
gpointer g_tree_node_value (GTreeNode *node);
extern
GTreeNode *g_tree_lookup_node (GTree *tree,
                               gconstpointer key);
extern
gpointer g_tree_lookup (GTree *tree,
                                 gconstpointer key);
extern
gboolean g_tree_lookup_extended (GTree *tree,
                                 gconstpointer lookup_key,
                                 gpointer *orig_key,
                                 gpointer *value);
extern
void g_tree_foreach (GTree *tree,
                                 GTraverseFunc func,
                                 gpointer user_data);
extern
void g_tree_foreach_node (GTree *tree,
                          GTraverseNodeFunc func,
                          gpointer user_data);
 extern
void g_tree_traverse (GTree *tree,
                                 GTraverseFunc traverse_func,
                                 GTraverseType traverse_type,
                                 gpointer user_data);
extern
GTreeNode *g_tree_search_node (GTree *tree,
                               GCompareFunc search_func,
                               gconstpointer user_data);
extern
gpointer g_tree_search (GTree *tree,
                                 GCompareFunc search_func,
                                 gconstpointer user_data);
extern
GTreeNode *g_tree_lower_bound (GTree *tree,
                               gconstpointer key);
extern
GTreeNode *g_tree_upper_bound (GTree *tree,
                               gconstpointer key);
extern
gint g_tree_height (GTree *tree);
extern
gint g_tree_nnodes (GTree *tree);
       
typedef struct _GUri GUri;
extern
GUri * g_uri_ref (GUri *uri);
extern
void g_uri_unref (GUri *uri);
typedef enum {
  G_URI_FLAGS_NONE = 0,
  G_URI_FLAGS_PARSE_RELAXED = 1 << 0,
  G_URI_FLAGS_HAS_PASSWORD = 1 << 1,
  G_URI_FLAGS_HAS_AUTH_PARAMS = 1 << 2,
  G_URI_FLAGS_ENCODED = 1 << 3,
  G_URI_FLAGS_NON_DNS = 1 << 4,
  G_URI_FLAGS_ENCODED_QUERY = 1 << 5,
  G_URI_FLAGS_ENCODED_PATH = 1 << 6,
  G_URI_FLAGS_ENCODED_FRAGMENT = 1 << 7,
  G_URI_FLAGS_SCHEME_NORMALIZE = 1 << 8,
} GUriFlags;
extern
gboolean g_uri_split (const gchar *uri_ref,
                                     GUriFlags flags,
                                     gchar **scheme,
                                     gchar **userinfo,
                                     gchar **host,
                                     gint *port,
                                     gchar **path,
                                     gchar **query,
                                     gchar **fragment,
                                     GError **error);
extern
gboolean g_uri_split_with_user (const gchar *uri_ref,
                                     GUriFlags flags,
                                     gchar **scheme,
                                     gchar **user,
                                     gchar **password,
                                     gchar **auth_params,
                                     gchar **host,
                                     gint *port,
                                     gchar **path,
                                     gchar **query,
                                     gchar **fragment,
                                     GError **error);
extern
gboolean g_uri_split_network (const gchar *uri_string,
                                     GUriFlags flags,
                                     gchar **scheme,
                                     gchar **host,
                                     gint *port,
                                     GError **error);
extern
gboolean g_uri_is_valid (const gchar *uri_string,
                                     GUriFlags flags,
                                     GError **error);
extern
gchar * g_uri_join (GUriFlags flags,
                                     const gchar *scheme,
                                     const gchar *userinfo,
                                     const gchar *host,
                                     gint port,
                                     const gchar *path,
                                     const gchar *query,
                                     const gchar *fragment);
extern
gchar * g_uri_join_with_user (GUriFlags flags,
                                     const gchar *scheme,
                                     const gchar *user,
                                     const gchar *password,
                                     const gchar *auth_params,
                                     const gchar *host,
                                     gint port,
                                     const gchar *path,
                                     const gchar *query,
                                     const gchar *fragment);
extern
GUri * g_uri_parse (const gchar *uri_string,
                                     GUriFlags flags,
                                     GError **error);
extern
GUri * g_uri_parse_relative (GUri *base_uri,
                                     const gchar *uri_ref,
                                     GUriFlags flags,
                                     GError **error);
extern
gchar * g_uri_resolve_relative (const gchar *base_uri_string,
                                     const gchar *uri_ref,
                                     GUriFlags flags,
                                     GError **error);
extern
GUri * g_uri_build (GUriFlags flags,
                                     const gchar *scheme,
                                     const gchar *userinfo,
                                     const gchar *host,
                                     gint port,
                                     const gchar *path,
                                     const gchar *query,
                                     const gchar *fragment);
extern
GUri * g_uri_build_with_user (GUriFlags flags,
                                     const gchar *scheme,
                                     const gchar *user,
                                     const gchar *password,
                                     const gchar *auth_params,
                                     const gchar *host,
                                     gint port,
                                     const gchar *path,
                                     const gchar *query,
                                     const gchar *fragment);
typedef enum {
  G_URI_HIDE_NONE = 0,
  G_URI_HIDE_USERINFO = 1 << 0,
  G_URI_HIDE_PASSWORD = 1 << 1,
  G_URI_HIDE_AUTH_PARAMS = 1 << 2,
  G_URI_HIDE_QUERY = 1 << 3,
  G_URI_HIDE_FRAGMENT = 1 << 4,
} GUriHideFlags;
extern
char * g_uri_to_string (GUri *uri);
extern
char * g_uri_to_string_partial (GUri *uri,
                                      GUriHideFlags flags);
extern
const gchar *g_uri_get_scheme (GUri *uri);
extern
const gchar *g_uri_get_userinfo (GUri *uri);
extern
const gchar *g_uri_get_user (GUri *uri);
extern
const gchar *g_uri_get_password (GUri *uri);
extern
const gchar *g_uri_get_auth_params (GUri *uri);
extern
const gchar *g_uri_get_host (GUri *uri);
extern
gint g_uri_get_port (GUri *uri);
extern
const gchar *g_uri_get_path (GUri *uri);
extern
const gchar *g_uri_get_query (GUri *uri);
extern
const gchar *g_uri_get_fragment (GUri *uri);
extern
GUriFlags g_uri_get_flags (GUri *uri);
typedef enum {
  G_URI_PARAMS_NONE = 0,
  G_URI_PARAMS_CASE_INSENSITIVE = 1 << 0,
  G_URI_PARAMS_WWW_FORM = 1 << 1,
  G_URI_PARAMS_PARSE_RELAXED = 1 << 2,
} GUriParamsFlags;
extern
GHashTable *g_uri_parse_params (const gchar *params,
                                      gssize length,
                                      const gchar *separators,
                                      GUriParamsFlags flags,
                                      GError **error);
typedef struct _GUriParamsIter GUriParamsIter;
struct _GUriParamsIter
{
  gint dummy0;
  gpointer dummy1;
  gpointer dummy2;
  guint8 dummy3[256];
};
extern
void g_uri_params_iter_init (GUriParamsIter *iter,
                                      const gchar *params,
                                      gssize length,
                                      const gchar *separators,
                                      GUriParamsFlags flags);
extern
gboolean g_uri_params_iter_next (GUriParamsIter *iter,
                                      gchar **attribute,
                                      gchar **value,
                                      GError **error);
extern
GQuark g_uri_error_quark (void);
typedef enum {
  G_URI_ERROR_FAILED,
  G_URI_ERROR_BAD_SCHEME,
  G_URI_ERROR_BAD_USER,
  G_URI_ERROR_BAD_PASSWORD,
  G_URI_ERROR_BAD_AUTH_PARAMS,
  G_URI_ERROR_BAD_HOST,
  G_URI_ERROR_BAD_PORT,
  G_URI_ERROR_BAD_PATH,
  G_URI_ERROR_BAD_QUERY,
  G_URI_ERROR_BAD_FRAGMENT,
} GUriError;
extern
char * g_uri_unescape_string (const char *escaped_string,
                                    const char *illegal_characters);
extern
char * g_uri_unescape_segment (const char *escaped_string,
                                    const char *escaped_string_end,
                                    const char *illegal_characters);
extern
char * g_uri_parse_scheme (const char *uri);
extern
const char *g_uri_peek_scheme (const char *uri);
extern
char * g_uri_escape_string (const char *unescaped,
                                    const char *reserved_chars_allowed,
                                    gboolean allow_utf8);
extern
GBytes * g_uri_unescape_bytes (const char *escaped_string,
                                    gssize length,
                                    const char *illegal_characters,
                                    GError **error);
extern
char * g_uri_escape_bytes (const guint8 *unescaped,
                                    gsize length,
                                    const char *reserved_chars_allowed);
extern
gboolean g_uuid_string_is_valid (const gchar *str);
extern
gchar * g_uuid_string_random (void);
extern const guint glib_major_version;
extern const guint glib_minor_version;
extern const guint glib_micro_version;
extern const guint glib_interface_age;
extern const guint glib_binary_age;
extern
const gchar * glib_check_version (guint required_major,
                                  guint required_minor,
                                  guint required_micro);
typedef struct _GAllocator GAllocator;
typedef struct _GMemChunk GMemChunk;
 extern
GMemChunk * g_mem_chunk_new (const gchar *name,
                                         gint atom_size,
                                         gsize area_size,
                                         gint type);
 extern
void g_mem_chunk_destroy (GMemChunk *mem_chunk);
 extern
gpointer g_mem_chunk_alloc (GMemChunk *mem_chunk);
 extern
gpointer g_mem_chunk_alloc0 (GMemChunk *mem_chunk);
 extern
void g_mem_chunk_free (GMemChunk *mem_chunk,
                                         gpointer mem);
 extern
void g_mem_chunk_clean (GMemChunk *mem_chunk);
 extern
void g_mem_chunk_reset (GMemChunk *mem_chunk);
 extern
void g_mem_chunk_print (GMemChunk *mem_chunk);
 extern
void g_mem_chunk_info (void);
 extern
void g_blow_chunks (void);
 extern
GAllocator * g_allocator_new (const gchar *name,
                                         guint n_preallocs);
 extern
void g_allocator_free (GAllocator *allocator);
 extern
void g_list_push_allocator (GAllocator *allocator);
 extern
void g_list_pop_allocator (void);
 extern
void g_slist_push_allocator (GAllocator *allocator);
 extern
void g_slist_pop_allocator (void);
 extern
void g_node_push_allocator (GAllocator *allocator);
 extern
void g_node_pop_allocator (void);
typedef struct _GCache GCache ;
typedef gpointer (*GCacheNewFunc) (gpointer key) ;
typedef gpointer (*GCacheDupFunc) (gpointer value) ;
typedef void (*GCacheDestroyFunc) (gpointer value) ;
 extern
GCache* g_cache_new (GCacheNewFunc value_new_func,
                                GCacheDestroyFunc value_destroy_func,
                                GCacheDupFunc key_dup_func,
                                GCacheDestroyFunc key_destroy_func,
                                GHashFunc hash_key_func,
                                GHashFunc hash_value_func,
                                GEqualFunc key_equal_func);
 extern
void g_cache_destroy (GCache *cache);
 extern
gpointer g_cache_insert (GCache *cache,
                                gpointer key);
 extern
void g_cache_remove (GCache *cache,
                                gconstpointer value);
 extern
void g_cache_key_foreach (GCache *cache,
                                GHFunc func,
                                gpointer user_data);
 extern
void g_cache_value_foreach (GCache *cache,
                                GHFunc func,
                                gpointer user_data);
typedef struct _GCompletion GCompletion;
typedef gchar* (*GCompletionFunc) (gpointer);
typedef gint (*GCompletionStrncmpFunc) (const gchar *s1,
                                        const gchar *s2,
                                        gsize n);
struct _GCompletion
{
  GList* items;
  GCompletionFunc func;
  gchar* prefix;
  GList* cache;
  GCompletionStrncmpFunc strncmp_func;
};
 extern
GCompletion* g_completion_new (GCompletionFunc func);
 extern
void g_completion_add_items (GCompletion* cmp,
                                         GList* items);
 extern
void g_completion_remove_items (GCompletion* cmp,
                                         GList* items);
 extern
void g_completion_clear_items (GCompletion* cmp);
 extern
GList* g_completion_complete (GCompletion* cmp,
                                         const gchar* prefix,
                                         gchar** new_prefix);
 extern
GList* g_completion_complete_utf8 (GCompletion *cmp,
                                         const gchar* prefix,
                                         gchar** new_prefix);
 extern
void g_completion_set_compare (GCompletion *cmp,
                                         GCompletionStrncmpFunc strncmp_func);
 extern
void g_completion_free (GCompletion* cmp);
typedef struct _GRelation GRelation;
typedef struct _GTuples GTuples;
struct _GTuples
{
  guint len;
};
 extern
GRelation* g_relation_new (gint fields);
 extern
void g_relation_destroy (GRelation *relation);
 extern
void g_relation_index (GRelation *relation,
                               gint field,
                               GHashFunc hash_func,
                               GEqualFunc key_equal_func);
 extern
void g_relation_insert (GRelation *relation,
                               ...);
 extern
gint g_relation_delete (GRelation *relation,
                               gconstpointer key,
                               gint field);
 extern
GTuples* g_relation_select (GRelation *relation,
                               gconstpointer key,
                               gint field);
 extern
gint g_relation_count (GRelation *relation,
                               gconstpointer key,
                               gint field);
 extern
gboolean g_relation_exists (GRelation *relation,
                               ...);
 extern
void g_relation_print (GRelation *relation);
 extern
void g_tuples_destroy (GTuples *tuples);
 extern
gpointer g_tuples_index (GTuples *tuples,
                               gint index_,
                               gint field);
typedef enum
{
  G_THREAD_PRIORITY_LOW,
  G_THREAD_PRIORITY_NORMAL,
  G_THREAD_PRIORITY_HIGH,
  G_THREAD_PRIORITY_URGENT
} GThreadPriority ;
struct _GThread
{
  GThreadFunc func;
  gpointer data;
  gboolean joinable;
  GThreadPriority priority;
};
typedef struct _GThreadFunctions GThreadFunctions ;
struct _GThreadFunctions
{
  GMutex* (*mutex_new) (void);
  void (*mutex_lock) (GMutex *mutex);
  gboolean (*mutex_trylock) (GMutex *mutex);
  void (*mutex_unlock) (GMutex *mutex);
  void (*mutex_free) (GMutex *mutex);
  GCond* (*cond_new) (void);
  void (*cond_signal) (GCond *cond);
  void (*cond_broadcast) (GCond *cond);
  void (*cond_wait) (GCond *cond,
                                   GMutex *mutex);
  gboolean (*cond_timed_wait) (GCond *cond,
                                   GMutex *mutex,
                                   GTimeVal *end_time);
  void (*cond_free) (GCond *cond);
  GPrivate* (*private_new) (GDestroyNotify destructor);
  gpointer (*private_get) (GPrivate *private_key);
  void (*private_set) (GPrivate *private_key,
                                   gpointer data);
  void (*thread_create) (GThreadFunc func,
                                   gpointer data,
                                   gulong stack_size,
                                   gboolean joinable,
                                   gboolean bound,
                                   GThreadPriority priority,
                                   gpointer thread,
                                   GError **error);
  void (*thread_yield) (void);
  void (*thread_join) (gpointer thread);
  void (*thread_exit) (void);
  void (*thread_set_priority)(gpointer thread,
                                   GThreadPriority priority);
  void (*thread_self) (gpointer thread);
  gboolean (*thread_equal) (gpointer thread1,
                                   gpointer thread2);
} ;
extern GThreadFunctions g_thread_functions_for_glib_use;
extern gboolean g_thread_use_default_impl;
extern guint64 (*g_thread_gettime) (void);
 extern
GThread *g_thread_create (GThreadFunc func,
                                gpointer data,
                                gboolean joinable,
                                GError **error);
 extern
GThread *g_thread_create_full (GThreadFunc func,
                                gpointer data,
                                gulong stack_size,
                                gboolean joinable,
                                gboolean bound,
                                GThreadPriority priority,
                                GError **error);
 extern
void g_thread_set_priority (GThread *thread,
                                GThreadPriority priority);
 extern
void g_thread_foreach (GFunc thread_func,
                                gpointer user_data);
struct sched_param
{
  int sched_priority;
};
typedef unsigned long int __cpu_mask;
typedef struct
{
  __cpu_mask __bits[16];
} cpu_set_t;
extern int __sched_cpucount (size_t __setsize, const cpu_set_t *__setp)
     ;
extern cpu_set_t *__sched_cpualloc (size_t __count) ;
extern void __sched_cpufree (cpu_set_t *__set) ;
extern int sched_setparam (__pid_t __pid, const struct sched_param *__param)
     ;
extern int sched_getparam (__pid_t __pid, struct sched_param *__param) ;
extern int sched_setscheduler (__pid_t __pid, int __policy,
          const struct sched_param *__param) ;
extern int sched_getscheduler (__pid_t __pid) ;
extern int sched_yield (void) ;
extern int sched_get_priority_max (int __algorithm) ;
extern int sched_get_priority_min (int __algorithm) ;
extern int sched_rr_get_interval (__pid_t __pid, struct timespec *__t) ;
enum
{
  PTHREAD_CREATE_JOINABLE,
  PTHREAD_CREATE_DETACHED
};
enum
{
  PTHREAD_MUTEX_TIMED_NP,
  PTHREAD_MUTEX_RECURSIVE_NP,
  PTHREAD_MUTEX_ERRORCHECK_NP,
  PTHREAD_MUTEX_ADAPTIVE_NP
  ,
  PTHREAD_MUTEX_NORMAL = PTHREAD_MUTEX_TIMED_NP,
  PTHREAD_MUTEX_RECURSIVE = PTHREAD_MUTEX_RECURSIVE_NP,
  PTHREAD_MUTEX_ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK_NP,
  PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
};
enum
{
  PTHREAD_MUTEX_STALLED,
  PTHREAD_MUTEX_STALLED_NP = PTHREAD_MUTEX_STALLED,
  PTHREAD_MUTEX_ROBUST,
  PTHREAD_MUTEX_ROBUST_NP = PTHREAD_MUTEX_ROBUST
};
enum
{
  PTHREAD_PRIO_NONE,
  PTHREAD_PRIO_INHERIT,
  PTHREAD_PRIO_PROTECT
};
enum
{
  PTHREAD_RWLOCK_PREFER_READER_NP,
  PTHREAD_RWLOCK_PREFER_WRITER_NP,
  PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP,
  PTHREAD_RWLOCK_DEFAULT_NP = PTHREAD_RWLOCK_PREFER_READER_NP
};
enum
{
  PTHREAD_INHERIT_SCHED,
  PTHREAD_EXPLICIT_SCHED
};
enum
{
  PTHREAD_SCOPE_SYSTEM,
  PTHREAD_SCOPE_PROCESS
};
enum
{
  PTHREAD_PROCESS_PRIVATE,
  PTHREAD_PROCESS_SHARED
};
struct _pthread_cleanup_buffer
{
  void (*__routine) (void *);
  void *__arg;
  int __canceltype;
  struct _pthread_cleanup_buffer *__prev;
};
enum
{
  PTHREAD_CANCEL_ENABLE,
  PTHREAD_CANCEL_DISABLE
};
enum
{
  PTHREAD_CANCEL_DEFERRED,
  PTHREAD_CANCEL_ASYNCHRONOUS
};
extern int pthread_create (pthread_t * __newthread,
      const pthread_attr_t * __attr,
      void *(*__start_routine) (void *),
      void * __arg) ;
extern void pthread_exit (void *__retval) ;
extern int pthread_join (pthread_t __th, void **__thread_return);
extern int pthread_detach (pthread_t __th) ;
extern pthread_t pthread_self (void) ;
extern int pthread_equal (pthread_t __thread1, pthread_t __thread2)
  ;
extern int pthread_attr_init (pthread_attr_t *__attr) ;
extern int pthread_attr_destroy (pthread_attr_t *__attr)
     ;
extern int pthread_attr_getdetachstate (const pthread_attr_t *__attr,
     int *__detachstate)
     ;
extern int pthread_attr_setdetachstate (pthread_attr_t *__attr,
     int __detachstate)
     ;
extern int pthread_attr_getguardsize (const pthread_attr_t *__attr,
          size_t *__guardsize)
     ;
extern int pthread_attr_setguardsize (pthread_attr_t *__attr,
          size_t __guardsize)
     ;
extern int pthread_attr_getschedparam (const pthread_attr_t * __attr,
           struct sched_param * __param)
     ;
extern int pthread_attr_setschedparam (pthread_attr_t * __attr,
           const struct sched_param *
           __param) ;
extern int pthread_attr_getschedpolicy (const pthread_attr_t *
     __attr, int * __policy)
     ;
extern int pthread_attr_setschedpolicy (pthread_attr_t *__attr, int __policy)
     ;
extern int pthread_attr_getinheritsched (const pthread_attr_t *
      __attr, int * __inherit)
     ;
extern int pthread_attr_setinheritsched (pthread_attr_t *__attr,
      int __inherit)
     ;
extern int pthread_attr_getscope (const pthread_attr_t * __attr,
      int * __scope)
     ;
extern int pthread_attr_setscope (pthread_attr_t *__attr, int __scope)
     ;
extern int pthread_attr_getstackaddr (const pthread_attr_t *
          __attr, void ** __stackaddr)
     ;
extern int pthread_attr_setstackaddr (pthread_attr_t *__attr,
          void *__stackaddr)
     ;
extern int pthread_attr_getstacksize (const pthread_attr_t *
          __attr, size_t * __stacksize)
     ;
extern int pthread_attr_setstacksize (pthread_attr_t *__attr,
          size_t __stacksize)
     ;
extern int pthread_attr_getstack (const pthread_attr_t * __attr,
      void ** __stackaddr,
      size_t * __stacksize)
     ;
extern int pthread_attr_setstack (pthread_attr_t *__attr, void *__stackaddr,
      size_t __stacksize) ;
extern int pthread_setschedparam (pthread_t __target_thread, int __policy,
      const struct sched_param *__param)
     ;
extern int pthread_getschedparam (pthread_t __target_thread,
      int * __policy,
      struct sched_param * __param)
     ;
extern int pthread_setschedprio (pthread_t __target_thread, int __prio)
     ;
extern int pthread_once (pthread_once_t *__once_control,
    void (*__init_routine) (void)) ;
extern int pthread_setcancelstate (int __state, int *__oldstate);
extern int pthread_setcanceltype (int __type, int *__oldtype);
extern int pthread_cancel (pthread_t __th);
extern void pthread_testcancel (void);
struct __cancel_jmp_buf_tag
{
  __jmp_buf __cancel_jmp_buf;
  int __mask_was_saved;
};
typedef struct
{
  struct __cancel_jmp_buf_tag __cancel_jmp_buf[1];
  void *__pad[4];
} __pthread_unwind_buf_t ;
struct __pthread_cleanup_frame
{
  void (*__cancel_routine) (void *);
  void *__cancel_arg;
  int __do_it;
  int __cancel_type;
};
extern void __pthread_register_cancel (__pthread_unwind_buf_t *__buf)
     ;
extern void __pthread_unregister_cancel (__pthread_unwind_buf_t *__buf)
  ;
extern void __pthread_unwind_next (__pthread_unwind_buf_t *__buf)
    
    
     ;
extern int __sigsetjmp_cancel (struct __cancel_jmp_buf_tag __env[1], int __savemask)
                                                ;
extern int pthread_mutex_init (pthread_mutex_t *__mutex,
          const pthread_mutexattr_t *__mutexattr)
     ;
extern int pthread_mutex_destroy (pthread_mutex_t *__mutex)
     ;
extern int pthread_mutex_trylock (pthread_mutex_t *__mutex)
     ;
extern int pthread_mutex_lock (pthread_mutex_t *__mutex)
     ;
extern int pthread_mutex_timedlock (pthread_mutex_t * __mutex,
        const struct timespec *
        __abstime) ;
extern int pthread_mutex_unlock (pthread_mutex_t *__mutex)
     ;
extern int pthread_mutex_getprioceiling (const pthread_mutex_t *
      __mutex,
      int * __prioceiling)
     ;
extern int pthread_mutex_setprioceiling (pthread_mutex_t * __mutex,
      int __prioceiling,
      int * __old_ceiling)
     ;
extern int pthread_mutex_consistent (pthread_mutex_t *__mutex)
     ;
extern int pthread_mutexattr_init (pthread_mutexattr_t *__attr)
     ;
extern int pthread_mutexattr_destroy (pthread_mutexattr_t *__attr)
     ;
extern int pthread_mutexattr_getpshared (const pthread_mutexattr_t *
      __attr,
      int * __pshared)
     ;
extern int pthread_mutexattr_setpshared (pthread_mutexattr_t *__attr,
      int __pshared)
     ;
extern int pthread_mutexattr_gettype (const pthread_mutexattr_t *
          __attr, int * __kind)
     ;
extern int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind)
     ;
extern int pthread_mutexattr_getprotocol (const pthread_mutexattr_t *
       __attr,
       int * __protocol)
     ;
extern int pthread_mutexattr_setprotocol (pthread_mutexattr_t *__attr,
       int __protocol)
     ;
extern int pthread_mutexattr_getprioceiling (const pthread_mutexattr_t *
          __attr,
          int * __prioceiling)
     ;
extern int pthread_mutexattr_setprioceiling (pthread_mutexattr_t *__attr,
          int __prioceiling)
     ;
extern int pthread_mutexattr_getrobust (const pthread_mutexattr_t *__attr,
     int *__robustness)
     ;
extern int pthread_mutexattr_setrobust (pthread_mutexattr_t *__attr,
     int __robustness)
     ;
extern int pthread_rwlock_init (pthread_rwlock_t * __rwlock,
    const pthread_rwlockattr_t *
    __attr) ;
extern int pthread_rwlock_destroy (pthread_rwlock_t *__rwlock)
     ;
extern int pthread_rwlock_rdlock (pthread_rwlock_t *__rwlock)
     ;
extern int pthread_rwlock_tryrdlock (pthread_rwlock_t *__rwlock)
  ;
extern int pthread_rwlock_timedrdlock (pthread_rwlock_t * __rwlock,
           const struct timespec *
           __abstime) ;
extern int pthread_rwlock_wrlock (pthread_rwlock_t *__rwlock)
     ;
extern int pthread_rwlock_trywrlock (pthread_rwlock_t *__rwlock)
     ;
extern int pthread_rwlock_timedwrlock (pthread_rwlock_t * __rwlock,
           const struct timespec *
           __abstime) ;
extern int pthread_rwlock_unlock (pthread_rwlock_t *__rwlock)
     ;
extern int pthread_rwlockattr_init (pthread_rwlockattr_t *__attr)
     ;
extern int pthread_rwlockattr_destroy (pthread_rwlockattr_t *__attr)
     ;
extern int pthread_rwlockattr_getpshared (const pthread_rwlockattr_t *
       __attr,
       int * __pshared)
     ;
extern int pthread_rwlockattr_setpshared (pthread_rwlockattr_t *__attr,
       int __pshared)
     ;
extern int pthread_rwlockattr_getkind_np (const pthread_rwlockattr_t *
       __attr,
       int * __pref)
     ;
extern int pthread_rwlockattr_setkind_np (pthread_rwlockattr_t *__attr,
       int __pref) ;
extern int pthread_cond_init (pthread_cond_t * __cond,
         const pthread_condattr_t * __cond_attr)
     ;
extern int pthread_cond_destroy (pthread_cond_t *__cond)
     ;
extern int pthread_cond_signal (pthread_cond_t *__cond)
     ;
extern int pthread_cond_broadcast (pthread_cond_t *__cond)
     ;
extern int pthread_cond_wait (pthread_cond_t * __cond,
         pthread_mutex_t * __mutex)
     ;
extern int pthread_cond_timedwait (pthread_cond_t * __cond,
       pthread_mutex_t * __mutex,
       const struct timespec * __abstime)
     ;
extern int pthread_condattr_init (pthread_condattr_t *__attr)
     ;
extern int pthread_condattr_destroy (pthread_condattr_t *__attr)
     ;
extern int pthread_condattr_getpshared (const pthread_condattr_t *
     __attr,
     int * __pshared)
     ;
extern int pthread_condattr_setpshared (pthread_condattr_t *__attr,
     int __pshared) ;
extern int pthread_condattr_getclock (const pthread_condattr_t *
          __attr,
          __clockid_t * __clock_id)
     ;
extern int pthread_condattr_setclock (pthread_condattr_t *__attr,
          __clockid_t __clock_id)
     ;
extern int pthread_spin_init (pthread_spinlock_t *__lock, int __pshared)
     ;
extern int pthread_spin_destroy (pthread_spinlock_t *__lock)
     ;
extern int pthread_spin_lock (pthread_spinlock_t *__lock)
     ;
extern int pthread_spin_trylock (pthread_spinlock_t *__lock)
     ;
extern int pthread_spin_unlock (pthread_spinlock_t *__lock)
     ;
extern int pthread_barrier_init (pthread_barrier_t * __barrier,
     const pthread_barrierattr_t *
     __attr, unsigned int __count)
     ;
extern int pthread_barrier_destroy (pthread_barrier_t *__barrier)
     ;
extern int pthread_barrier_wait (pthread_barrier_t *__barrier)
     ;
extern int pthread_barrierattr_init (pthread_barrierattr_t *__attr)
     ;
extern int pthread_barrierattr_destroy (pthread_barrierattr_t *__attr)
     ;
extern int pthread_barrierattr_getpshared (const pthread_barrierattr_t *
        __attr,
        int * __pshared)
     ;
extern int pthread_barrierattr_setpshared (pthread_barrierattr_t *__attr,
        int __pshared)
     ;
extern int pthread_key_create (pthread_key_t *__key,
          void (*__destr_function) (void *))
     ;
extern int pthread_key_delete (pthread_key_t __key) ;
extern void *pthread_getspecific (pthread_key_t __key) ;
extern int pthread_setspecific (pthread_key_t __key,
    const void *__pointer)
  ;
extern int pthread_getcpuclockid (pthread_t __thread_id,
      __clockid_t *__clock_id)
     ;
extern int pthread_atfork (void (*__prepare) (void),
      void (*__parent) (void),
      void (*__child) (void)) ;
typedef struct
{
  GMutex *mutex;
  pthread_mutex_t unused;
} GStaticMutex ;
 extern
void g_static_mutex_init (GStaticMutex *mutex);
 extern
void g_static_mutex_free (GStaticMutex *mutex);
 extern
GMutex *g_static_mutex_get_mutex_impl (GStaticMutex *mutex);
typedef struct _GStaticRecMutex GStaticRecMutex ;
struct _GStaticRecMutex
{
  GStaticMutex mutex;
  guint depth;
  union {
    pthread_t owner;
    gdouble dummy;
  } unused;
} ;
 extern
void g_static_rec_mutex_init (GStaticRecMutex *mutex);
 extern
void g_static_rec_mutex_lock (GStaticRecMutex *mutex);
 extern
gboolean g_static_rec_mutex_trylock (GStaticRecMutex *mutex);
 extern
void g_static_rec_mutex_unlock (GStaticRecMutex *mutex);
 extern
void g_static_rec_mutex_lock_full (GStaticRecMutex *mutex,
                                         guint depth);
 extern
guint g_static_rec_mutex_unlock_full (GStaticRecMutex *mutex);
 extern
void g_static_rec_mutex_free (GStaticRecMutex *mutex);
typedef struct _GStaticRWLock GStaticRWLock ;
struct _GStaticRWLock
{
  GStaticMutex mutex;
  GCond *read_cond;
  GCond *write_cond;
  guint read_counter;
  gboolean have_writer;
  guint want_to_read;
  guint want_to_write;
} ;
 extern
void g_static_rw_lock_init (GStaticRWLock *lock);
 extern
void g_static_rw_lock_reader_lock (GStaticRWLock *lock);
 extern
gboolean g_static_rw_lock_reader_trylock (GStaticRWLock *lock);
 extern
void g_static_rw_lock_reader_unlock (GStaticRWLock *lock);
 extern
void g_static_rw_lock_writer_lock (GStaticRWLock *lock);
 extern
gboolean g_static_rw_lock_writer_trylock (GStaticRWLock *lock);
 extern
void g_static_rw_lock_writer_unlock (GStaticRWLock *lock);
 extern
void g_static_rw_lock_free (GStaticRWLock *lock);
 extern
GPrivate * g_private_new (GDestroyNotify notify);
typedef struct _GStaticPrivate GStaticPrivate ;
struct _GStaticPrivate
{
  guint index;
} ;
 extern
void g_static_private_init (GStaticPrivate *private_key);
 extern
gpointer g_static_private_get (GStaticPrivate *private_key);
 extern
void g_static_private_set (GStaticPrivate *private_key,
                                          gpointer data,
                                          GDestroyNotify notify);
 extern
void g_static_private_free (GStaticPrivate *private_key);
 extern
gboolean g_once_init_enter_impl (volatile gsize *location);
 extern
void g_thread_init (gpointer vtable);
 extern
void g_thread_init_with_errorcheck_mutexes (gpointer vtable);
 extern
gboolean g_thread_get_initialized (void);
extern gboolean g_threads_got_initialized;
 extern
GMutex * g_mutex_new (void);
 extern
void g_mutex_free (GMutex *mutex);
 extern
GCond * g_cond_new (void);
 extern
void g_cond_free (GCond *cond);
 extern
gboolean g_cond_timed_wait (GCond *cond,
                                         GMutex *mutex,
                                         GTimeVal *timeval);
typedef GAsyncQueue *GAsyncQueue_autoptr; typedef GList *GAsyncQueue_listautoptr; typedef GSList *GAsyncQueue_slistautoptr; typedef GQueue *GAsyncQueue_queueautoptr;
typedef GBookmarkFile *GBookmarkFile_autoptr; typedef GList *GBookmarkFile_listautoptr; typedef GSList *GBookmarkFile_slistautoptr; typedef GQueue *GBookmarkFile_queueautoptr;
typedef GBytes *GBytes_autoptr; typedef GList *GBytes_listautoptr; typedef GSList *GBytes_slistautoptr; typedef GQueue *GBytes_queueautoptr;
typedef GChecksum *GChecksum_autoptr; typedef GList *GChecksum_listautoptr; typedef GSList *GChecksum_slistautoptr; typedef GQueue *GChecksum_queueautoptr;
typedef GDateTime *GDateTime_autoptr; typedef GList *GDateTime_listautoptr; typedef GSList *GDateTime_slistautoptr; typedef GQueue *GDateTime_queueautoptr;
typedef GDate *GDate_autoptr; typedef GList *GDate_listautoptr; typedef GSList *GDate_slistautoptr; typedef GQueue *GDate_queueautoptr;
typedef GDir *GDir_autoptr; typedef GList *GDir_listautoptr; typedef GSList *GDir_slistautoptr; typedef GQueue *GDir_queueautoptr;
typedef GError *GError_autoptr; typedef GList *GError_listautoptr; typedef GSList *GError_slistautoptr; typedef GQueue *GError_queueautoptr;
typedef GHashTable *GHashTable_autoptr; typedef GList *GHashTable_listautoptr; typedef GSList *GHashTable_slistautoptr; typedef GQueue *GHashTable_queueautoptr;
typedef GHmac *GHmac_autoptr; typedef GList *GHmac_listautoptr; typedef GSList *GHmac_slistautoptr; typedef GQueue *GHmac_queueautoptr;
typedef GIOChannel *GIOChannel_autoptr; typedef GList *GIOChannel_listautoptr; typedef GSList *GIOChannel_slistautoptr; typedef GQueue *GIOChannel_queueautoptr;
typedef GKeyFile *GKeyFile_autoptr; typedef GList *GKeyFile_listautoptr; typedef GSList *GKeyFile_slistautoptr; typedef GQueue *GKeyFile_queueautoptr;
typedef GList *GList_autoptr; typedef GList *GList_listautoptr; typedef GSList *GList_slistautoptr; typedef GQueue *GList_queueautoptr;
typedef GArray *GArray_autoptr; typedef GList *GArray_listautoptr; typedef GSList *GArray_slistautoptr; typedef GQueue *GArray_queueautoptr;
typedef GPtrArray *GPtrArray_autoptr; typedef GList *GPtrArray_listautoptr; typedef GSList *GPtrArray_slistautoptr; typedef GQueue *GPtrArray_queueautoptr;
typedef GByteArray *GByteArray_autoptr; typedef GList *GByteArray_listautoptr; typedef GSList *GByteArray_slistautoptr; typedef GQueue *GByteArray_queueautoptr;
typedef GMainContext *GMainContext_autoptr; typedef GList *GMainContext_listautoptr; typedef GSList *GMainContext_slistautoptr; typedef GQueue *GMainContext_queueautoptr;
typedef GMainContextPusher *GMainContextPusher_autoptr; typedef GList *GMainContextPusher_listautoptr; typedef GSList *GMainContextPusher_slistautoptr; typedef GQueue *GMainContextPusher_queueautoptr;
typedef GMainLoop *GMainLoop_autoptr; typedef GList *GMainLoop_listautoptr; typedef GSList *GMainLoop_slistautoptr; typedef GQueue *GMainLoop_queueautoptr;
typedef GSource *GSource_autoptr; typedef GList *GSource_listautoptr; typedef GSList *GSource_slistautoptr; typedef GQueue *GSource_queueautoptr;
typedef GMappedFile *GMappedFile_autoptr; typedef GList *GMappedFile_listautoptr; typedef GSList *GMappedFile_slistautoptr; typedef GQueue *GMappedFile_queueautoptr;
typedef GMarkupParseContext *GMarkupParseContext_autoptr; typedef GList *GMarkupParseContext_listautoptr; typedef GSList *GMarkupParseContext_slistautoptr; typedef GQueue *GMarkupParseContext_queueautoptr;
typedef GNode *GNode_autoptr; typedef GList *GNode_listautoptr; typedef GSList *GNode_slistautoptr; typedef GQueue *GNode_queueautoptr;
typedef GOptionContext *GOptionContext_autoptr; typedef GList *GOptionContext_listautoptr; typedef GSList *GOptionContext_slistautoptr; typedef GQueue *GOptionContext_queueautoptr;
typedef GOptionGroup *GOptionGroup_autoptr; typedef GList *GOptionGroup_listautoptr; typedef GSList *GOptionGroup_slistautoptr; typedef GQueue *GOptionGroup_queueautoptr;
typedef GPatternSpec *GPatternSpec_autoptr; typedef GList *GPatternSpec_listautoptr; typedef GSList *GPatternSpec_slistautoptr; typedef GQueue *GPatternSpec_queueautoptr;
typedef GQueue *GQueue_autoptr; typedef GList *GQueue_listautoptr; typedef GSList *GQueue_slistautoptr; typedef GQueue *GQueue_queueautoptr;
typedef GRand *GRand_autoptr; typedef GList *GRand_listautoptr; typedef GSList *GRand_slistautoptr; typedef GQueue *GRand_queueautoptr;
typedef GRegex *GRegex_autoptr; typedef GList *GRegex_listautoptr; typedef GSList *GRegex_slistautoptr; typedef GQueue *GRegex_queueautoptr;
typedef GMatchInfo *GMatchInfo_autoptr; typedef GList *GMatchInfo_listautoptr; typedef GSList *GMatchInfo_slistautoptr; typedef GQueue *GMatchInfo_queueautoptr;
typedef GScanner *GScanner_autoptr; typedef GList *GScanner_listautoptr; typedef GSList *GScanner_slistautoptr; typedef GQueue *GScanner_queueautoptr;
typedef GSequence *GSequence_autoptr; typedef GList *GSequence_listautoptr; typedef GSList *GSequence_slistautoptr; typedef GQueue *GSequence_queueautoptr;
typedef GSList *GSList_autoptr; typedef GList *GSList_listautoptr; typedef GSList *GSList_slistautoptr; typedef GQueue *GSList_queueautoptr;
typedef GString *GString_autoptr; typedef GList *GString_listautoptr; typedef GSList *GString_slistautoptr; typedef GQueue *GString_queueautoptr;
typedef GStringChunk *GStringChunk_autoptr; typedef GList *GStringChunk_listautoptr; typedef GSList *GStringChunk_slistautoptr; typedef GQueue *GStringChunk_queueautoptr;
typedef GStrvBuilder *GStrvBuilder_autoptr; typedef GList *GStrvBuilder_listautoptr; typedef GSList *GStrvBuilder_slistautoptr; typedef GQueue *GStrvBuilder_queueautoptr;
typedef GThread *GThread_autoptr; typedef GList *GThread_listautoptr; typedef GSList *GThread_slistautoptr; typedef GQueue *GThread_queueautoptr;
typedef GMutexLocker *GMutexLocker_autoptr; typedef GList *GMutexLocker_listautoptr; typedef GSList *GMutexLocker_slistautoptr; typedef GQueue *GMutexLocker_queueautoptr;
typedef GRecMutexLocker *GRecMutexLocker_autoptr; typedef GList *GRecMutexLocker_listautoptr; typedef GSList *GRecMutexLocker_slistautoptr; typedef GQueue *GRecMutexLocker_queueautoptr;
typedef GRWLockWriterLocker *GRWLockWriterLocker_autoptr; typedef GList *GRWLockWriterLocker_listautoptr; typedef GSList *GRWLockWriterLocker_slistautoptr; typedef GQueue *GRWLockWriterLocker_queueautoptr;
typedef GRWLockReaderLocker *GRWLockReaderLocker_autoptr; typedef GList *GRWLockReaderLocker_listautoptr; typedef GSList *GRWLockReaderLocker_slistautoptr; typedef GQueue *GRWLockReaderLocker_queueautoptr;
typedef GTimer *GTimer_autoptr; typedef GList *GTimer_listautoptr; typedef GSList *GTimer_slistautoptr; typedef GQueue *GTimer_queueautoptr;
typedef GTimeZone *GTimeZone_autoptr; typedef GList *GTimeZone_listautoptr; typedef GSList *GTimeZone_slistautoptr; typedef GQueue *GTimeZone_queueautoptr;
typedef GTree *GTree_autoptr; typedef GList *GTree_listautoptr; typedef GSList *GTree_slistautoptr; typedef GQueue *GTree_queueautoptr;
typedef GVariant *GVariant_autoptr; typedef GList *GVariant_listautoptr; typedef GSList *GVariant_slistautoptr; typedef GQueue *GVariant_queueautoptr;
typedef GVariantBuilder *GVariantBuilder_autoptr; typedef GList *GVariantBuilder_listautoptr; typedef GSList *GVariantBuilder_slistautoptr; typedef GQueue *GVariantBuilder_queueautoptr;
typedef GVariantIter *GVariantIter_autoptr; typedef GList *GVariantIter_listautoptr; typedef GSList *GVariantIter_slistautoptr; typedef GQueue *GVariantIter_queueautoptr;
typedef GVariantDict *GVariantDict_autoptr; typedef GList *GVariantDict_listautoptr; typedef GSList *GVariantDict_slistautoptr; typedef GQueue *GVariantDict_queueautoptr;
typedef GVariantType *GVariantType_autoptr; typedef GList *GVariantType_listautoptr; typedef GSList *GVariantType_slistautoptr; typedef GQueue *GVariantType_queueautoptr;
typedef GRefString *GRefString_autoptr; typedef GList *GRefString_listautoptr; typedef GSList *GRefString_slistautoptr; typedef GQueue *GRefString_queueautoptr;
typedef GUri *GUri_autoptr; typedef GList *GUri_listautoptr; typedef GSList *GUri_slistautoptr; typedef GQueue *GUri_queueautoptr;
extern
GQuark g_unix_error_quark (void);
extern
gboolean g_unix_open_pipe (gint *fds,
                           gint flags,
                           GError **error);
extern
gboolean g_unix_set_fd_nonblocking (gint fd,
                                    gboolean nonblock,
                                    GError **error);
extern
GSource *g_unix_signal_source_new (gint signum);
extern
guint g_unix_signal_add_full (gint priority,
                                    gint signum,
                                    GSourceFunc handler,
                                    gpointer user_data,
                                    GDestroyNotify notify);
extern
guint g_unix_signal_add (gint signum,
                                    GSourceFunc handler,
                                    gpointer user_data);
typedef gboolean (*GUnixFDSourceFunc) (gint fd,
                                       GIOCondition condition,
                                       gpointer user_data);
extern
GSource *g_unix_fd_source_new (gint fd,
                                    GIOCondition condition);
extern
guint g_unix_fd_add_full (gint priority,
                                    gint fd,
                                    GIOCondition condition,
                                    GUnixFDSourceFunc function,
                                    gpointer user_data,
                                    GDestroyNotify notify);
extern
guint g_unix_fd_add (gint fd,
                                    GIOCondition condition,
                                    GUnixFDSourceFunc function,
                                    gpointer user_data);
extern
struct passwd *g_unix_get_passwd_entry (const gchar *user_name,
                                        GError **error);
typedef enum
{
  G_MODULE_BIND_LAZY = 1 << 0,
  G_MODULE_BIND_LOCAL = 1 << 1,
  G_MODULE_BIND_MASK = 0x03
} GModuleFlags;
typedef struct _GModule GModule;
typedef const gchar* (*GModuleCheckInit) (GModule *module);
typedef void (*GModuleUnload) (GModule *module);
extern
GQuark g_module_error_quark (void);
typedef enum
{
  G_MODULE_ERROR_FAILED,
  G_MODULE_ERROR_CHECK_FAILED,
} GModuleError
;
extern
gboolean g_module_supported (void) ;
extern
GModule* g_module_open (const gchar *file_name,
           GModuleFlags flags);
extern
GModule *g_module_open_full (const gchar *file_name,
                                              GModuleFlags flags,
                                              GError **error);
extern
gboolean g_module_close (GModule *module);
extern
void g_module_make_resident (GModule *module);
extern
const gchar * g_module_error (void);
extern
gboolean g_module_symbol (GModule *module,
           const gchar *symbol_name,
           gpointer *symbol);
extern
const gchar * g_module_name (GModule *module);
extern
gchar* g_module_build_path (const gchar *directory,
           const gchar *module_name);
 int breakheader();
       
       
       
typedef int Int128;
typedef int Int128Aligned;
typedef union {
    int u;
    int i;
    Int128 s;
} Int128Alias ;
void urshift(uint64_t *plow, uint64_t *phigh, int32_t shift);
void ulshift(uint64_t *plow, uint64_t *phigh, int32_t shift, 
                                                            _Bool 
                                                                 *overflow);
Int128 divu256(Int128 *plow, Int128 *phigh, Int128 divisor);
Int128 divs256(Int128 *plow, Int128 *phigh, Int128 divisor);
       
       
void os_set_line_buffering(void);
void os_setup_early_signal_handling(void);
void os_set_proc_name(const char *s);
void os_setup_signal_handling(void);
int os_set_daemonize(
                    _Bool 
                         d);
_Bool 
    is_daemonized(void);
void os_daemonize(void);
_Bool 
    os_set_runas(const char *user_id);
void os_set_chroot(const char *path);
void os_setup_limits(void);
void os_setup_post(void);
int os_mlock(void);
void *qemu_alloc_stack(size_t *sz);
void qemu_free_stack(void *stack, size_t sz);
typedef struct AccelCPUState AccelCPUState;
typedef struct AccelState AccelState;
typedef struct AddressSpace AddressSpace;
typedef struct AioContext AioContext;
typedef struct Aml Aml;
typedef struct ArchCPU ArchCPU;
typedef struct BdrvDirtyBitmap BdrvDirtyBitmap;
typedef struct BdrvDirtyBitmapIter BdrvDirtyBitmapIter;
typedef struct BlockBackend BlockBackend;
typedef struct BlockBackendRootState BlockBackendRootState;
typedef struct BlockDriverState BlockDriverState;
typedef struct BusClass BusClass;
typedef struct BusState BusState;
typedef struct Chardev Chardev;
typedef struct Clock Clock;
typedef struct ConfidentialGuestSupport ConfidentialGuestSupport;
typedef struct CPUArchState CPUArchState;
typedef struct CPUPluginState CPUPluginState;
typedef struct CPUState CPUState;
typedef struct CPUTLBEntryFull CPUTLBEntryFull;
typedef struct DeviceState DeviceState;
typedef struct DirtyBitmapSnapshot DirtyBitmapSnapshot;
typedef struct DisasContextBase DisasContextBase;
typedef struct DisplayChangeListener DisplayChangeListener;
typedef struct DriveInfo DriveInfo;
typedef struct DumpState DumpState;
typedef struct Error Error;
typedef struct EventNotifier EventNotifier;
typedef struct FlatView FlatView;
typedef struct FWCfgState FWCfgState;
typedef struct HostMemoryBackend HostMemoryBackend;
typedef struct I2CBus I2CBus;
typedef struct I2SCodec I2SCodec;
typedef struct IOMMUMemoryRegion IOMMUMemoryRegion;
typedef struct ISABus ISABus;
typedef struct ISADevice ISADevice;
typedef struct IsaDma IsaDma;
typedef struct JSONWriter JSONWriter;
typedef struct MACAddr MACAddr;
typedef struct MachineClass MachineClass;
typedef struct MachineState MachineState;
typedef struct MemoryListener MemoryListener;
typedef struct MemoryMappingList MemoryMappingList;
typedef struct MemoryRegion MemoryRegion;
typedef struct MemoryRegionCache MemoryRegionCache;
typedef struct MemoryRegionSection MemoryRegionSection;
typedef struct MigrationIncomingState MigrationIncomingState;
typedef struct MigrationState MigrationState;
typedef struct Monitor Monitor;
typedef struct MSIMessage MSIMessage;
typedef struct NetClientState NetClientState;
typedef struct NetFilterState NetFilterState;
typedef struct NICInfo NICInfo;
typedef struct Object Object;
typedef struct ObjectClass ObjectClass;
typedef struct PCIBridge PCIBridge;
typedef struct PCIBus PCIBus;
typedef struct PCIDevice PCIDevice;
typedef struct PCIEPort PCIEPort;
typedef struct PCIESlot PCIESlot;
typedef struct PCIExpressDevice PCIExpressDevice;
typedef struct PCIExpressHost PCIExpressHost;
typedef struct PCIHostDeviceAddress PCIHostDeviceAddress;
typedef struct PCIHostState PCIHostState;
typedef struct Property Property;
typedef struct PropertyInfo PropertyInfo;
typedef struct QBool QBool;
typedef struct QDict QDict;
typedef struct QEMUBH QEMUBH;
typedef struct QemuConsole QemuConsole;
typedef struct QEMUCursor QEMUCursor;
typedef struct QEMUFile QEMUFile;
typedef struct QemuMutex QemuMutex;
typedef struct QemuOpts QemuOpts;
typedef struct QemuOptsList QemuOptsList;
typedef struct QEMUSGList QEMUSGList;
typedef struct QemuSpin QemuSpin;
typedef struct QEMUTimer QEMUTimer;
typedef struct QEMUTimerListGroup QEMUTimerListGroup;
typedef struct QList QList;
typedef struct QNull QNull;
typedef struct QNum QNum;
typedef struct QObject QObject;
typedef struct QString QString;
typedef struct RAMBlock RAMBlock;
typedef struct Range Range;
typedef struct ReservedRegion ReservedRegion;
typedef struct SHPCDevice SHPCDevice;
typedef struct SSIBus SSIBus;
typedef struct TCGCPUOps TCGCPUOps;
typedef struct TCGHelperInfo TCGHelperInfo;
typedef struct TaskState TaskState;
typedef struct TranslationBlock TranslationBlock;
typedef struct VirtIODevice VirtIODevice;
typedef struct Visitor Visitor;
typedef struct VMChangeStateEntry VMChangeStateEntry;
typedef struct VMStateDescription VMStateDescription;
typedef struct IRQState *qemu_irq;
typedef void (*qemu_irq_handler)(void *opaque, int n, int level);
void
    qemu_build_not_reached_always(void);
int qemu_daemon(int nochdir, int noclose);
void *qemu_anon_ram_alloc(size_t size, uint64_t *align, 
                                                       _Bool 
                                                            shared,
                          
                         _Bool 
                              noreserve);
void qemu_anon_ram_free(void *ptr, size_t size);
struct qemu_signalfd_siginfo {
    uint32_t ssi_signo;
    int32_t ssi_errno;
    int32_t ssi_code;
    uint32_t ssi_pid;
    uint32_t ssi_uid;
    int32_t ssi_fd;
    uint32_t ssi_tid;
    uint32_t ssi_band;
    uint32_t ssi_overrun;
    uint32_t ssi_trapno;
    int32_t ssi_status;
    int32_t ssi_int;
    uint64_t ssi_ptr;
    uint64_t ssi_utime;
    uint64_t ssi_stime;
    uint64_t ssi_addr;
    uint8_t pad[48];
};
int qemu_signalfd(const sigset_t *mask);
void sigaction_invoke(struct sigaction *action,
                      struct qemu_signalfd_siginfo *info);
int qemu_open_old(const char *name, int flags, ...);
int qemu_open(const char *name, int flags, Error **errp);
int qemu_create(const char *name, int flags, mode_t mode, Error **errp);
int qemu_close(int fd);
int qemu_unlink(const char *name);
int qemu_dup_flags(int fd, int flags);
int qemu_dup(int fd);
int qemu_lock_fd(int fd, int64_t start, int64_t len, 
                                                    _Bool 
                                                         exclusive);
int qemu_unlock_fd(int fd, int64_t start, int64_t len);
int qemu_lock_fd_test(int fd, int64_t start, int64_t len, 
                                                         _Bool 
                                                              exclusive);
_Bool 
    qemu_has_ofd_lock(void);
_Bool 
    qemu_has_direct_io(void);
_Bool 
    qemu_write_pidfile(const char *pidfile, Error **errp);
int qemu_get_thread_id(void);
ssize_t qemu_write_full(int fd, const void *buf, size_t count)
    ;
void qemu_set_cloexec(int fd);
char *qemu_get_local_state_dir(void);
unsigned long qemu_getauxval(unsigned long type);
void qemu_set_tty_echo(int fd, 
                              _Bool 
                                   echo);
typedef struct ThreadContext ThreadContext;
_Bool 
    qemu_prealloc_mem(int fd, char *area, size_t sz, int max_threads,
                       ThreadContext *tc, 
                                         _Bool 
                                              async, Error **errp);
_Bool 
    qemu_finish_async_prealloc_mem(Error **errp);
char *qemu_get_pid_name(pid_t pid);
int qemu_fdatasync(int fd);
void qemu_close_all_open_fd(const int *skip, unsigned int nskip);
int qemu_msync(void *addr, size_t length, int fd);
size_t qemu_get_host_physmem(void);
typedef uint64_t hwaddr;
typedef struct MemMapEntry {
    hwaddr base;
    hwaddr size;
} MemMapEntry;
typedef uint64_t vaddr;
typedef int64_t aligned_int64_t ;
typedef uint64_t aligned_uint64_t ;
typedef struct QTailQLink {
    void *tql_next;
    struct QTailQLink *tql_prev;
} QTailQLink;
unsigned long find_last_bit(const unsigned long *addr,
                            unsigned long size);
unsigned long find_next_bit(const unsigned long *addr,
                            unsigned long size,
                            unsigned long offset);
unsigned long find_next_zero_bit(const unsigned long *addr,
                                 unsigned long size,
                                 unsigned long offset);
int slow_bitmap_empty(const unsigned long *bitmap, long bits);
int slow_bitmap_full(const unsigned long *bitmap, long bits);
int slow_bitmap_equal(const unsigned long *bitmap1,
                      const unsigned long *bitmap2, long bits);
void slow_bitmap_complement(unsigned long *dst, const unsigned long *src,
                            long bits);
int slow_bitmap_and(unsigned long *dst, const unsigned long *bitmap1,
                    const unsigned long *bitmap2, long bits);
void slow_bitmap_or(unsigned long *dst, const unsigned long *bitmap1,
                    const unsigned long *bitmap2, long bits);
void slow_bitmap_xor(unsigned long *dst, const unsigned long *bitmap1,
                     const unsigned long *bitmap2, long bits);
int slow_bitmap_andnot(unsigned long *dst, const unsigned long *bitmap1,
                       const unsigned long *bitmap2, long bits);
int slow_bitmap_intersects(const unsigned long *bitmap1,
                           const unsigned long *bitmap2, long bits);
long slow_bitmap_count_one(const unsigned long *bitmap, long nbits);
void bitmap_set(unsigned long *map, long i, long len);
void bitmap_set_atomic(unsigned long *map, long i, long len);
void bitmap_clear(unsigned long *map, long start, long nr);
_Bool 
    bitmap_test_and_clear_atomic(unsigned long *map, long start, long nr);
_Bool 
    bitmap_test_and_clear(unsigned long *map, long start, long nr);
void bitmap_copy_and_clear_atomic(unsigned long *dst, unsigned long *src,
                                  long nr);
unsigned long bitmap_find_next_zero_area(unsigned long *map,
                                         unsigned long size,
                                         unsigned long start,
                                         unsigned long nr,
                                         unsigned long align_mask);
void bitmap_to_le(unsigned long *dst, const unsigned long *src,
                  long nbits);
void bitmap_from_le(unsigned long *dst, const unsigned long *src,
                    long nbits);
void bitmap_copy_with_src_offset(unsigned long *dst, const unsigned long *src,
                                 unsigned long offset, unsigned long nbits);
void bitmap_copy_with_dst_offset(unsigned long *dst, const unsigned long *src,
                                 unsigned long shift, unsigned long nbits);
typedef struct QemuCond QemuCond;
typedef struct QemuSemaphore QemuSemaphore;
typedef struct QemuEvent QemuEvent;
typedef struct QemuLockCnt QemuLockCnt;
typedef struct QemuThread QemuThread;
struct QemuMutex {
    pthread_mutex_t lock;
    
   _Bool 
        initialized;
};
typedef struct QemuRecMutex {
    QemuMutex m;
} QemuRecMutex;
struct QemuCond {
    pthread_cond_t cond;
    
   _Bool 
        initialized;
};
struct QemuSemaphore {
    QemuMutex mutex;
    QemuCond cond;
    unsigned int count;
};
struct QemuEvent {
    unsigned value;
    
   _Bool 
        initialized;
};
struct QemuThread {
    pthread_t thread;
};
enum QSPSortBy {
    QSP_SORT_BY_TOTAL_WAIT_TIME,
    QSP_SORT_BY_AVG_WAIT_TIME,
};
void qsp_report(size_t max, enum QSPSortBy sort_by,
                
               _Bool 
                    callsite_coalesce);
_Bool 
    qsp_is_enabled(void);
void qsp_enable(void);
void qsp_disable(void);
void qsp_reset(void);
void qemu_mutex_init(QemuMutex *mutex);
void qemu_mutex_destroy(QemuMutex *mutex);
int qemu_mutex_trylock_impl(QemuMutex *mutex, const char *file,
                                       const int line);
void qemu_mutex_lock_impl(QemuMutex *mutex, const char *file,
                                     const int line);
void qemu_mutex_unlock_impl(QemuMutex *mutex, const char *file,
                                       const int line);
void qemu_rec_mutex_init(QemuRecMutex *mutex);
void qemu_rec_mutex_destroy(QemuRecMutex *mutex);
void qemu_rec_mutex_lock_impl(QemuRecMutex *mutex, const char *file, int line);
int qemu_rec_mutex_trylock_impl(QemuRecMutex *mutex, const char *file, int line);
void qemu_rec_mutex_unlock_impl(QemuRecMutex *mutex, const char *file, int line);
typedef void (*QemuMutexLockFunc)(QemuMutex *m, const char *f, int l);
typedef int (*QemuMutexTrylockFunc)(QemuMutex *m, const char *f, int l);
typedef void (*QemuRecMutexLockFunc)(QemuRecMutex *m, const char *f, int l);
typedef int (*QemuRecMutexTrylockFunc)(QemuRecMutex *m, const char *f, int l);
typedef void (*QemuCondWaitFunc)(QemuCond *c, QemuMutex *m, const char *f,
                                 int l);
typedef 
       _Bool 
            (*QemuCondTimedWaitFunc)(QemuCond *c, QemuMutex *m, int ms,
                                      const char *f, int l);
extern QemuMutexLockFunc bql_mutex_lock_func;
extern QemuMutexLockFunc qemu_mutex_lock_func;
extern QemuMutexTrylockFunc qemu_mutex_trylock_func;
extern QemuRecMutexLockFunc qemu_rec_mutex_lock_func;
extern QemuRecMutexTrylockFunc qemu_rec_mutex_trylock_func;
extern QemuCondWaitFunc qemu_cond_wait_func;
extern QemuCondTimedWaitFunc qemu_cond_timedwait_func;
void qemu_cond_init(QemuCond *cond);
void qemu_cond_destroy(QemuCond *cond);
void qemu_cond_signal(QemuCond *cond);
void qemu_cond_broadcast(QemuCond *cond);
void qemu_cond_wait_impl(QemuCond *cond, QemuMutex *mutex,
                                    const char *file, const int line);
_Bool 
    qemu_cond_timedwait_impl(QemuCond *cond, QemuMutex *mutex, int ms,
                              const char *file, const int line);
void qemu_sem_init(QemuSemaphore *sem, int init);
void qemu_sem_post(QemuSemaphore *sem);
void qemu_sem_wait(QemuSemaphore *sem);
int qemu_sem_timedwait(QemuSemaphore *sem, int ms);
void qemu_sem_destroy(QemuSemaphore *sem);
void qemu_event_init(QemuEvent *ev, 
                                   _Bool 
                                        init);
void qemu_event_set(QemuEvent *ev);
void qemu_event_reset(QemuEvent *ev);
void qemu_event_wait(QemuEvent *ev);
void qemu_event_destroy(QemuEvent *ev);
void qemu_thread_create(QemuThread *thread, const char *name,
                        void *(*start_routine)(void *),
                        void *arg, int mode);
int qemu_thread_set_affinity(QemuThread *thread, unsigned long *host_cpus,
                             unsigned long nbits);
int qemu_thread_get_affinity(QemuThread *thread, unsigned long **host_cpus,
                             unsigned long *nbits);
void *qemu_thread_join(QemuThread *thread);
void qemu_thread_get_self(QemuThread *thread);
_Bool 
    qemu_thread_is_self(QemuThread *thread);
 void qemu_thread_exit(void *retval);
void qemu_thread_naming(
                       _Bool 
                            enable);
struct Notifier;
void qemu_thread_atexit_add(struct Notifier *notifier);
void qemu_thread_atexit_remove(struct Notifier *notifier);
struct QemuSpin {
    int value;
};
typedef struct Notifier Notifier;
struct Notifier
{
    void (*notify)(Notifier *notifier, void *data);
    struct { struct Notifier *le_next; struct Notifier **le_prev; } node;
};
typedef struct NotifierList
{
    struct { struct Notifier *lh_first; } notifiers;
} NotifierList;
void notifier_list_init(NotifierList *list);
void notifier_list_add(NotifierList *list, Notifier *notifier);
void notifier_remove(Notifier *notifier);
void notifier_list_notify(NotifierList *list, void *data);
_Bool 
    notifier_list_empty(NotifierList *list);
typedef struct NotifierWithReturn NotifierWithReturn;
typedef int (*NotifierWithReturnFunc)(NotifierWithReturn *notifier, void *data,
                                      Error **errp);
struct NotifierWithReturn {
    NotifierWithReturnFunc notify;
    struct { struct NotifierWithReturn *le_next; struct NotifierWithReturn **le_prev; } node;
};
typedef struct NotifierWithReturnList {
    struct { struct NotifierWithReturn *lh_first; } notifiers;
} NotifierWithReturnList;
void notifier_with_return_list_init(NotifierWithReturnList *list);
void notifier_with_return_list_add(NotifierWithReturnList *list,
                                   NotifierWithReturn *notifier);
void notifier_with_return_remove(NotifierWithReturn *notifier);
int notifier_with_return_list_notify(NotifierWithReturnList *list,
                                     void *data, Error **errp);
extern unsigned long rcu_gp_ctr;
extern QemuEvent rcu_gp_event;
struct rcu_reader_data {
    unsigned long ctr;
    
   _Bool 
        waiting;
    unsigned depth;
    struct { struct rcu_reader_data *le_next; struct rcu_reader_data **le_prev; } node;
    NotifierList force_rcu;
};
 struct rcu_reader_data get_rcu_reader(void); void set_rcu_reader(struct rcu_reader_data v); struct rcu_reader_data *get_ptr_rcu_reader(void);
void synchronize_rcu(void);
void rcu_register_thread(void);
void rcu_unregister_thread(void);
void rcu_enable_atfork(void);
void rcu_disable_atfork(void);
struct rcu_head;
typedef void RCUCBFunc(struct rcu_head *head);
struct rcu_head {
    struct rcu_head *next;
    RCUCBFunc *func;
};
void call_rcu1(struct rcu_head *head, RCUCBFunc *func);
void drain_call_rcu(void);
typedef void RCUReadAuto;
typedef RCUReadAuto *RCUReadAuto_autoptr; typedef GList *RCUReadAuto_listautoptr; typedef GSList *RCUReadAuto_slistautoptr; typedef GQueue *RCUReadAuto_queueautoptr;
void rcu_add_force_rcu_notifier(Notifier *n);
void rcu_remove_force_rcu_notifier(Notifier *n);
typedef enum {
    QAPI_DEPRECATED,
    QAPI_UNSTABLE,
} QapiSpecialFeature;
typedef struct QEnumLookup {
    const char *const *array;
    const unsigned char *const special_features;
    const int size;
} QEnumLookup;
const char *qapi_enum_lookup(const QEnumLookup *lookup, int val);
int qapi_enum_parse(const QEnumLookup *lookup, const char *buf,
                    int def, Error **errp);
_Bool 
    qapi_bool_parse(const char *name, const char *value, 
                                                         _Bool 
                                                              *obj,
                     Error **errp);
int parse_qapi_name(const char *name, 
                                     _Bool 
                                          complete);
typedef struct strList strList;
typedef struct numberList numberList;
typedef struct intList intList;
typedef struct int8List int8List;
typedef struct int16List int16List;
typedef struct int32List int32List;
typedef struct int64List int64List;
typedef struct uint8List uint8List;
typedef struct uint16List uint16List;
typedef struct uint32List uint32List;
typedef struct uint64List uint64List;
typedef struct sizeList sizeList;
typedef struct boolList boolList;
typedef struct anyList anyList;
typedef struct nullList nullList;
typedef enum QType {
    QTYPE_NONE,
    QTYPE_QNULL,
    QTYPE_QNUM,
    QTYPE_QSTRING,
    QTYPE_QDICT,
    QTYPE_QLIST,
    QTYPE_QBOOL,
    QTYPE__MAX,
} QType;
extern const QEnumLookup QType_lookup;
struct strList {
    strList *next;
    char *value;
};
void qapi_free_strList(strList *obj);
typedef strList *strList_autoptr; typedef GList *strList_listautoptr; typedef GSList *strList_slistautoptr; typedef GQueue *strList_queueautoptr;
struct numberList {
    numberList *next;
    double value;
};
void qapi_free_numberList(numberList *obj);
typedef numberList *numberList_autoptr; typedef GList *numberList_listautoptr; typedef GSList *numberList_slistautoptr; typedef GQueue *numberList_queueautoptr;
struct intList {
    intList *next;
    int64_t value;
};
void qapi_free_intList(intList *obj);
typedef intList *intList_autoptr; typedef GList *intList_listautoptr; typedef GSList *intList_slistautoptr; typedef GQueue *intList_queueautoptr;
struct int8List {
    int8List *next;
    int8_t value;
};
void qapi_free_int8List(int8List *obj);
typedef int8List *int8List_autoptr; typedef GList *int8List_listautoptr; typedef GSList *int8List_slistautoptr; typedef GQueue *int8List_queueautoptr;
struct int16List {
    int16List *next;
    int16_t value;
};
void qapi_free_int16List(int16List *obj);
typedef int16List *int16List_autoptr; typedef GList *int16List_listautoptr; typedef GSList *int16List_slistautoptr; typedef GQueue *int16List_queueautoptr;
struct int32List {
    int32List *next;
    int32_t value;
};
void qapi_free_int32List(int32List *obj);
typedef int32List *int32List_autoptr; typedef GList *int32List_listautoptr; typedef GSList *int32List_slistautoptr; typedef GQueue *int32List_queueautoptr;
struct int64List {
    int64List *next;
    int64_t value;
};
void qapi_free_int64List(int64List *obj);
typedef int64List *int64List_autoptr; typedef GList *int64List_listautoptr; typedef GSList *int64List_slistautoptr; typedef GQueue *int64List_queueautoptr;
struct uint8List {
    uint8List *next;
    uint8_t value;
};
void qapi_free_uint8List(uint8List *obj);
typedef uint8List *uint8List_autoptr; typedef GList *uint8List_listautoptr; typedef GSList *uint8List_slistautoptr; typedef GQueue *uint8List_queueautoptr;
struct uint16List {
    uint16List *next;
    uint16_t value;
};
void qapi_free_uint16List(uint16List *obj);
typedef uint16List *uint16List_autoptr; typedef GList *uint16List_listautoptr; typedef GSList *uint16List_slistautoptr; typedef GQueue *uint16List_queueautoptr;
struct uint32List {
    uint32List *next;
    uint32_t value;
};
void qapi_free_uint32List(uint32List *obj);
typedef uint32List *uint32List_autoptr; typedef GList *uint32List_listautoptr; typedef GSList *uint32List_slistautoptr; typedef GQueue *uint32List_queueautoptr;
struct uint64List {
    uint64List *next;
    uint64_t value;
};
void qapi_free_uint64List(uint64List *obj);
typedef uint64List *uint64List_autoptr; typedef GList *uint64List_listautoptr; typedef GSList *uint64List_slistautoptr; typedef GQueue *uint64List_queueautoptr;
struct sizeList {
    sizeList *next;
    uint64_t value;
};
void qapi_free_sizeList(sizeList *obj);
typedef sizeList *sizeList_autoptr; typedef GList *sizeList_listautoptr; typedef GSList *sizeList_slistautoptr; typedef GQueue *sizeList_queueautoptr;
struct boolList {
    boolList *next;
    
   _Bool 
        value;
};
void qapi_free_boolList(boolList *obj);
typedef boolList *boolList_autoptr; typedef GList *boolList_listautoptr; typedef GSList *boolList_slistautoptr; typedef GQueue *boolList_queueautoptr;
struct anyList {
    anyList *next;
    QObject *value;
};
void qapi_free_anyList(anyList *obj);
typedef anyList *anyList_autoptr; typedef GList *anyList_listautoptr; typedef GSList *anyList_slistautoptr; typedef GQueue *anyList_queueautoptr;
struct nullList {
    nullList *next;
    QNull *value;
};
void qapi_free_nullList(nullList *obj);
typedef nullList *nullList_autoptr; typedef GList *nullList_listautoptr; typedef GSList *nullList_slistautoptr; typedef GQueue *nullList_queueautoptr;
typedef enum {
    MODULE_INIT_MIGRATION,
    MODULE_INIT_BLOCK,
    MODULE_INIT_OPTS,
    MODULE_INIT_QOM,
    MODULE_INIT_TRACE,
    MODULE_INIT_XEN_BACKEND,
    MODULE_INIT_LIBQOS,
    MODULE_INIT_FUZZ_TARGET,
    MODULE_INIT_MAX
} module_init_type;
void register_module_init(void (*fn)(void), module_init_type type);
void register_dso_module_init(void (*fn)(void), module_init_type type);
void module_call_init(module_init_type type);
int module_load(const char *prefix, const char *name, Error **errp);
int module_load_qom(const char *type, Error **errp);
void module_load_qom_all(void);
void module_allow_arch(const char *arch);
typedef struct QemuModinfo QemuModinfo;
struct QemuModinfo {
    const char *name;
    const char *arch;
    const char **objs;
    const char **deps;
    const char **opts;
};
extern const QemuModinfo qemu_modinfo[];
void module_init_info(const QemuModinfo *info);
struct TypeImpl;
typedef struct TypeImpl *Type;
typedef struct TypeInfo TypeInfo;
typedef struct InterfaceClass InterfaceClass;
typedef struct InterfaceInfo InterfaceInfo;
typedef struct ObjectProperty ObjectProperty;
typedef void (ObjectPropertyAccessor)(Object *obj,
                                      Visitor *v,
                                      const char *name,
                                      void *opaque,
                                      Error **errp);
typedef Object *(ObjectPropertyResolve)(Object *obj,
                                        void *opaque,
                                        const char *part);
typedef void (ObjectPropertyRelease)(Object *obj,
                                     const char *name,
                                     void *opaque);
typedef void (ObjectPropertyInit)(Object *obj, ObjectProperty *prop);
struct ObjectProperty
{
    char *name;
    char *type;
    char *description;
    ObjectPropertyAccessor *get;
    ObjectPropertyAccessor *set;
    ObjectPropertyResolve *resolve;
    ObjectPropertyRelease *release;
    ObjectPropertyInit *init;
    void *opaque;
    QObject *defval;
};
typedef void (ObjectUnparent)(Object *obj);
typedef void (ObjectFree)(void *obj);
struct ObjectClass
{
    Type type;
    GSList *interfaces;
    const char *object_cast_cache[4];
    const char *class_cast_cache[4];
    ObjectUnparent *unparent;
    GHashTable *properties;
};
struct Object
{
    ObjectClass *class;
    ObjectFree *free;
    GHashTable *properties;
    uint32_t ref;
    Object *parent;
};
struct TypeInfo
{
    const char *name;
    const char *parent;
    size_t instance_size;
    size_t instance_align;
    void (*instance_init)(Object *obj);
    void (*instance_post_init)(Object *obj);
    void (*instance_finalize)(Object *obj);
    
   _Bool 
        abstract;
    size_t class_size;
    void (*class_init)(ObjectClass *klass, void *data);
    void (*class_base_init)(ObjectClass *klass, void *data);
    void *class_data;
    InterfaceInfo *interfaces;
};
struct InterfaceInfo {
    const char *type;
};
struct InterfaceClass
{
    ObjectClass parent_class;
    Type interface_type;
};
Object *object_new_with_class(ObjectClass *klass);
Object *object_new(const char *typename);
Object *object_new_with_props(const char *typename,
                              Object *parent,
                              const char *id,
                              Error **errp,
                              ...) ;
Object *object_new_with_propv(const char *typename,
                              Object *parent,
                              const char *id,
                              Error **errp,
                              va_list vargs);
_Bool 
    object_apply_global_props(Object *obj, const GPtrArray *props,
                               Error **errp);
void object_set_machine_compat_props(GPtrArray *compat_props);
void object_set_accelerator_compat_props(GPtrArray *compat_props);
void object_register_sugar_prop(const char *driver, const char *prop,
                                const char *value, 
                                                  _Bool 
                                                       optional);
void object_apply_compat_props(Object *obj);
_Bool 
    object_set_props(Object *obj, Error **errp, ...) ;
_Bool 
    object_set_propv(Object *obj, Error **errp, va_list vargs);
void object_initialize(void *obj, size_t size, const char *typename);
_Bool 
    object_initialize_child_with_props(Object *parentobj,
                             const char *propname,
                             void *childobj, size_t size, const char *type,
                             Error **errp, ...) ;
_Bool 
    object_initialize_child_with_propsv(Object *parentobj,
                              const char *propname,
                              void *childobj, size_t size, const char *type,
                              Error **errp, va_list vargs);
void object_initialize_child_internal(Object *parent, const char *propname,
                                      void *child, size_t size,
                                      const char *type);
Object *object_dynamic_cast(Object *obj, const char *typename);
Object *object_dynamic_cast_assert(Object *obj, const char *typename,
                                   const char *file, int line, const char *func);
ObjectClass *object_get_class(Object *obj);
const char *object_get_typename(const Object *obj);
Type type_register_static(const TypeInfo *info);
void type_register_static_array(const TypeInfo *infos, int nr_infos);
_Bool 
    type_print_class_properties(const char *type);
void object_set_properties_from_keyval(Object *obj, const QDict *qdict,
                                       
                                      _Bool 
                                           from_json, Error **errp);
ObjectClass *object_class_dynamic_cast_assert(ObjectClass *klass,
                                              const char *typename,
                                              const char *file, int line,
                                              const char *func);
ObjectClass *object_class_dynamic_cast(ObjectClass *klass,
                                       const char *typename);
ObjectClass *object_class_get_parent(ObjectClass *klass);
const char *object_class_get_name(ObjectClass *klass);
_Bool 
    object_class_is_abstract(ObjectClass *klass);
ObjectClass *object_class_by_name(const char *typename);
ObjectClass *module_object_class_by_name(const char *typename);
void object_class_foreach(void (*fn)(ObjectClass *klass, void *opaque),
                          const char *implements_type, 
                                                      _Bool 
                                                           include_abstract,
                          void *opaque);
GSList *object_class_get_list(const char *implements_type,
                              
                             _Bool 
                                  include_abstract);
GSList *object_class_get_list_sorted(const char *implements_type,
                              
                             _Bool 
                                  include_abstract);
Object *object_ref(void *obj);
void object_unref(void *obj);
ObjectProperty *object_property_try_add(Object *obj, const char *name,
                                        const char *type,
                                        ObjectPropertyAccessor *get,
                                        ObjectPropertyAccessor *set,
                                        ObjectPropertyRelease *release,
                                        void *opaque, Error **errp);
ObjectProperty *object_property_add(Object *obj, const char *name,
                                    const char *type,
                                    ObjectPropertyAccessor *get,
                                    ObjectPropertyAccessor *set,
                                    ObjectPropertyRelease *release,
                                    void *opaque);
void object_property_del(Object *obj, const char *name);
ObjectProperty *object_class_property_add(ObjectClass *klass, const char *name,
                                          const char *type,
                                          ObjectPropertyAccessor *get,
                                          ObjectPropertyAccessor *set,
                                          ObjectPropertyRelease *release,
                                          void *opaque);
void object_property_set_default_bool(ObjectProperty *prop, 
                                                           _Bool 
                                                                value);
void object_property_set_default_str(ObjectProperty *prop, const char *value);
void object_property_set_default_list(ObjectProperty *prop);
void object_property_set_default_int(ObjectProperty *prop, int64_t value);
void object_property_set_default_uint(ObjectProperty *prop, uint64_t value);
ObjectProperty *object_property_find(Object *obj, const char *name);
ObjectProperty *object_property_find_err(Object *obj,
                                         const char *name,
                                         Error **errp);
ObjectProperty *object_class_property_find(ObjectClass *klass,
                                           const char *name);
ObjectProperty *object_class_property_find_err(ObjectClass *klass,
                                               const char *name,
                                               Error **errp);
typedef struct ObjectPropertyIterator {
    ObjectClass *nextclass;
    GHashTableIter iter;
} ObjectPropertyIterator;
void object_property_iter_init(ObjectPropertyIterator *iter,
                               Object *obj);
void object_class_property_iter_init(ObjectPropertyIterator *iter,
                                     ObjectClass *klass);
ObjectProperty *object_property_iter_next(ObjectPropertyIterator *iter);
void object_unparent(Object *obj);
_Bool 
    object_property_get(Object *obj, const char *name, Visitor *v,
                         Error **errp);
_Bool 
    object_property_set_str(Object *obj, const char *name,
                             const char *value, Error **errp);
char *object_property_get_str(Object *obj, const char *name,
                              Error **errp);
_Bool 
    object_property_set_link(Object *obj, const char *name,
                              Object *value, Error **errp);
Object *object_property_get_link(Object *obj, const char *name,
                                 Error **errp);
_Bool 
    object_property_set_bool(Object *obj, const char *name,
                              
                             _Bool 
                                  value, Error **errp);
_Bool 
    object_property_get_bool(Object *obj, const char *name,
                              Error **errp);
_Bool 
    object_property_set_int(Object *obj, const char *name,
                             int64_t value, Error **errp);
int64_t object_property_get_int(Object *obj, const char *name,
                                Error **errp);
_Bool 
    object_property_set_uint(Object *obj, const char *name,
                              uint64_t value, Error **errp);
uint64_t object_property_get_uint(Object *obj, const char *name,
                                  Error **errp);
int object_property_get_enum(Object *obj, const char *name,
                             const char *typename, Error **errp);
_Bool 
    object_property_set(Object *obj, const char *name, Visitor *v,
                         Error **errp);
_Bool 
    object_property_parse(Object *obj, const char *name,
                           const char *string, Error **errp);
char *object_property_print(Object *obj, const char *name, 
                                                          _Bool 
                                                               human,
                            Error **errp);
const char *object_property_get_type(Object *obj, const char *name,
                                     Error **errp);
Object *object_get_root(void);
Object *object_get_container(const char *name);
Object *object_get_objects_root(void);
Object *object_get_internal_root(void);
const char *object_get_canonical_path_component(const Object *obj);
char *object_get_canonical_path(const Object *obj);
Object *object_resolve_path(const char *path, 
                                             _Bool 
                                                  *ambiguous);
Object *object_resolve_path_type(const char *path, const char *typename,
                                 
                                _Bool 
                                     *ambiguous);
Object *object_resolve_type_unambiguous(const char *typename, Error **errp);
Object *object_resolve_path_at(Object *parent, const char *path);
Object *object_resolve_path_component(Object *parent, const char *part);
ObjectProperty *object_property_try_add_child(Object *obj, const char *name,
                                              Object *child, Error **errp);
ObjectProperty *object_property_add_child(Object *obj, const char *name,
                                          Object *child);
typedef enum {
    OBJ_PROP_LINK_STRONG = 0x1,
    OBJ_PROP_LINK_DIRECT = 0x2,
    OBJ_PROP_LINK_CLASS = 0x4,
} ObjectPropertyLinkFlags;
void object_property_allow_set_link(const Object *obj, const char *name,
                                    Object *child, Error **errp);
ObjectProperty *object_property_add_link(Object *obj, const char *name,
                              const char *type, Object **targetp,
                              void (*check)(const Object *obj, const char *name,
                                            Object *val, Error **errp),
                              ObjectPropertyLinkFlags flags);
ObjectProperty *object_class_property_add_link(ObjectClass *oc,
                              const char *name,
                              const char *type, ptrdiff_t offset,
                              void (*check)(const Object *obj, const char *name,
                                            Object *val, Error **errp),
                              ObjectPropertyLinkFlags flags);
ObjectProperty *object_property_add_str(Object *obj, const char *name,
                             char *(*get)(Object *, Error **),
                             void (*set)(Object *, const char *, Error **));
ObjectProperty *object_class_property_add_str(ObjectClass *klass,
                                   const char *name,
                                   char *(*get)(Object *, Error **),
                                   void (*set)(Object *, const char *,
                                               Error **));
ObjectProperty *object_property_add_bool(Object *obj, const char *name,
                              
                             _Bool 
                                  (*get)(Object *, Error **),
                              void (*set)(Object *, 
                                                   _Bool
                                                       , Error **));
ObjectProperty *object_class_property_add_bool(ObjectClass *klass,
                                    const char *name,
                                    
                                   _Bool 
                                        (*get)(Object *, Error **),
                                    void (*set)(Object *, 
                                                         _Bool
                                                             , Error **));
ObjectProperty *object_property_add_enum(Object *obj, const char *name,
                              const char *typename,
                              const QEnumLookup *lookup,
                              int (*get)(Object *, Error **),
                              void (*set)(Object *, int, Error **));
ObjectProperty *object_class_property_add_enum(ObjectClass *klass,
                                    const char *name,
                                    const char *typename,
                                    const QEnumLookup *lookup,
                                    int (*get)(Object *, Error **),
                                    void (*set)(Object *, int, Error **));
ObjectProperty *object_property_add_tm(Object *obj, const char *name,
                            void (*get)(Object *, struct tm *, Error **));
ObjectProperty *object_class_property_add_tm(ObjectClass *klass,
                            const char *name,
                            void (*get)(Object *, struct tm *, Error **));
typedef enum {
    OBJ_PROP_FLAG_READ = 1 << 0,
    OBJ_PROP_FLAG_WRITE = 1 << 1,
    OBJ_PROP_FLAG_READWRITE = 3,
} ObjectPropertyFlags;
ObjectProperty *object_property_add_uint8_ptr(Object *obj, const char *name,
                                              const uint8_t *v,
                                              ObjectPropertyFlags flags);
ObjectProperty *object_class_property_add_uint8_ptr(ObjectClass *klass,
                                         const char *name,
                                         const uint8_t *v,
                                         ObjectPropertyFlags flags);
ObjectProperty *object_property_add_uint16_ptr(Object *obj, const char *name,
                                    const uint16_t *v,
                                    ObjectPropertyFlags flags);
ObjectProperty *object_class_property_add_uint16_ptr(ObjectClass *klass,
                                          const char *name,
                                          const uint16_t *v,
                                          ObjectPropertyFlags flags);
ObjectProperty *object_property_add_uint32_ptr(Object *obj, const char *name,
                                    const uint32_t *v,
                                    ObjectPropertyFlags flags);
ObjectProperty *object_class_property_add_uint32_ptr(ObjectClass *klass,
                                          const char *name,
                                          const uint32_t *v,
                                          ObjectPropertyFlags flags);
ObjectProperty *object_property_add_uint64_ptr(Object *obj, const char *name,
                                    const uint64_t *v,
                                    ObjectPropertyFlags flags);
ObjectProperty *object_class_property_add_uint64_ptr(ObjectClass *klass,
                                          const char *name,
                                          const uint64_t *v,
                                          ObjectPropertyFlags flags);
ObjectProperty *object_property_add_alias(Object *obj, const char *name,
                               Object *target_obj, const char *target_name);
ObjectProperty *object_property_add_const_link(Object *obj, const char *name,
                                               Object *target);
void object_property_set_description(Object *obj, const char *name,
                                     const char *description);
void object_class_property_set_description(ObjectClass *klass, const char *name,
                                           const char *description);
int object_child_foreach(Object *obj, int (*fn)(Object *child, void *opaque),
                         void *opaque);
int object_child_foreach_recursive(Object *obj,
                                   int (*fn)(Object *child, void *opaque),
                                   void *opaque);
Object *object_property_add_new_container(Object *obj, const char *name);
char *object_property_help(const char *name, const char *type,
                           QObject *defval, const char *description);
typedef Object *Object_autoptr; typedef GList *Object_listautoptr; typedef GSList *Object_slistautoptr; typedef GQueue *Object_queueautoptr;
typedef struct HotplugHandlerClass HotplugHandlerClass;
typedef struct HotplugHandler HotplugHandler;
typedef void (*hotplug_fn)(HotplugHandler *plug_handler,
                           DeviceState *plugged_dev, Error **errp);
struct HotplugHandlerClass {
    InterfaceClass parent;
    hotplug_fn pre_plug;
    hotplug_fn plug;
    hotplug_fn unplug_request;
    hotplug_fn unplug;
    
   _Bool 
        (*is_hotpluggable_bus)(HotplugHandler *plug_handler, BusState *bus);
};
void hotplug_handler_plug(HotplugHandler *plug_handler,
                          DeviceState *plugged_dev,
                          Error **errp);
void hotplug_handler_pre_plug(HotplugHandler *plug_handler,
                              DeviceState *plugged_dev,
                              Error **errp);
void hotplug_handler_unplug_request(HotplugHandler *plug_handler,
                                    DeviceState *plugged_dev,
                                    Error **errp);
void hotplug_handler_unplug(HotplugHandler *plug_handler,
                            DeviceState *plugged_dev,
                            Error **errp);
typedef struct ResettableClass ResettableClass;
typedef struct ResettableState ResettableState;
typedef enum ResetType {
    RESET_TYPE_COLD,
    RESET_TYPE_SNAPSHOT_LOAD,
    RESET_TYPE_WAKEUP,
    RESET_TYPE_S390_CPU_INITIAL,
    RESET_TYPE_S390_CPU_NORMAL,
} ResetType;
typedef void (*ResettableEnterPhase)(Object *obj, ResetType type);
typedef void (*ResettableHoldPhase)(Object *obj, ResetType type);
typedef void (*ResettableExitPhase)(Object *obj, ResetType type);
typedef ResettableState * (*ResettableGetState)(Object *obj);
typedef void (*ResettableChildCallback)(Object *, void *opaque,
                                        ResetType type);
typedef void (*ResettableChildForeach)(Object *obj,
                                       ResettableChildCallback cb,
                                       void *opaque, ResetType type);
typedef struct ResettablePhases {
    ResettableEnterPhase enter;
    ResettableHoldPhase hold;
    ResettableExitPhase exit;
} ResettablePhases;
struct ResettableClass {
    InterfaceClass parent_class;
    ResettablePhases phases;
    ResettableGetState get_state;
    ResettableChildForeach child_foreach;
};
struct ResettableState {
    unsigned count;
    
   _Bool 
        hold_phase_pending;
    
   _Bool 
        exit_phase_in_progress;
};
void resettable_reset(Object *obj, ResetType type);
void resettable_assert_reset(Object *obj, ResetType type);
void resettable_release_reset(Object *obj, ResetType type);
_Bool 
    resettable_is_in_reset(Object *obj);
void resettable_change_parent(Object *obj, Object *newp, Object *oldp);
void resettable_cold_reset_fn(void *opaque);
void resettable_class_set_parent_phases(ResettableClass *rc,
                                        ResettableEnterPhase enter,
                                        ResettableHoldPhase hold,
                                        ResettableExitPhase exit,
                                        ResettablePhases *parent_phases);
enum {
    DEV_NVECTORS_UNSPECIFIED = -1,
};
typedef struct DeviceState DeviceState; typedef struct DeviceClass DeviceClass; typedef DeviceState *DeviceState_autoptr; typedef GList *DeviceState_listautoptr; typedef GSList *DeviceState_slistautoptr; typedef GQueue *DeviceState_queueautoptr;
typedef enum DeviceCategory {
    DEVICE_CATEGORY_BRIDGE,
    DEVICE_CATEGORY_USB,
    DEVICE_CATEGORY_STORAGE,
    DEVICE_CATEGORY_NETWORK,
    DEVICE_CATEGORY_INPUT,
    DEVICE_CATEGORY_DISPLAY,
    DEVICE_CATEGORY_SOUND,
    DEVICE_CATEGORY_MISC,
    DEVICE_CATEGORY_CPU,
    DEVICE_CATEGORY_WATCHDOG,
    DEVICE_CATEGORY_MAX
} DeviceCategory;
typedef void (*DeviceRealize)(DeviceState *dev, Error **errp);
typedef void (*DeviceUnrealize)(DeviceState *dev);
typedef void (*DeviceReset)(DeviceState *dev);
typedef void (*BusRealize)(BusState *bus, Error **errp);
typedef void (*BusUnrealize)(BusState *bus);
typedef int (*DeviceSyncConfig)(DeviceState *dev, Error **errp);
struct DeviceClass {
    ObjectClass parent_class;
    unsigned long categories[1];
    const char *fw_name;
    const char *desc;
    const Property *props_;
    uint16_t props_count_;
    
   _Bool 
        user_creatable;
    
   _Bool 
        hotpluggable;
    DeviceReset legacy_reset;
    DeviceRealize realize;
    DeviceUnrealize unrealize;
    DeviceSyncConfig sync_config;
    const VMStateDescription *vmsd;
    const char *bus_type;
};
typedef struct NamedGPIOList NamedGPIOList;
struct NamedGPIOList {
    char *name;
    qemu_irq *in;
    int num_in;
    int num_out;
    struct { struct NamedGPIOList *le_next; struct NamedGPIOList **le_prev; } node;
};
typedef struct Clock Clock;
typedef struct NamedClockList NamedClockList;
struct NamedClockList {
    char *name;
    Clock *clock;
    
   _Bool 
        output;
    
   _Bool 
        alias;
    struct { struct NamedClockList *le_next; struct NamedClockList **le_prev; } node;
};
typedef struct {
    
   _Bool 
        engaged_in_io;
} MemReentrancyGuard;
typedef struct { struct NamedGPIOList *lh_first; } NamedGPIOListHead;
typedef struct { struct NamedClockList *lh_first; } NamedClockListHead;
typedef struct { struct BusState *lh_first; } BusStateHead;
struct DeviceState {
    Object parent_obj;
    char *id;
    char *canonical_path;
    
   _Bool 
        realized;
    
   _Bool 
        pending_deleted_event;
    int64_t pending_deleted_expires_ms;
    int hotplugged;
    
   _Bool 
        allow_unplug_during_migration;
    BusState *parent_bus;
    NamedGPIOListHead gpios;
    NamedClockListHead clocks;
    BusStateHead child_bus;
    int num_child_bus;
    int instance_id_alias;
    int alias_required_for_version;
    ResettableState reset;
    GSList *unplug_blockers;
    MemReentrancyGuard mem_reentrancy_guard;
};
typedef struct DeviceListener DeviceListener;
struct DeviceListener {
    void (*realize)(DeviceListener *listener, DeviceState *dev);
    void (*unrealize)(DeviceListener *listener, DeviceState *dev);
    
   _Bool 
        (*hide_device)(DeviceListener *listener, const QDict *device_opts,
                        
                       _Bool 
                            from_json, Error **errp);
    union { struct DeviceListener *tqe_next; QTailQLink tqe_circ; } link;
};
struct BusClass {
    ObjectClass parent_class;
    void (*print_dev)(Monitor *mon, DeviceState *dev, int indent);
    char *(*get_dev_path)(DeviceState *dev);
    char *(*get_fw_dev_path)(DeviceState *dev);
    
   _Bool 
        (*check_address)(BusState *bus, DeviceState *dev, Error **errp);
    BusRealize realize;
    BusUnrealize unrealize;
    int max_dev;
    int automatic_ids;
};
typedef struct BusChild {
    struct rcu_head rcu;
    DeviceState *child;
    int index;
    union { struct BusChild *tqe_next; QTailQLink tqe_circ; } sibling;
} BusChild;
typedef union { struct BusChild *tqh_first; QTailQLink tqh_circ; } BusChildHead;
typedef struct { struct BusState *le_next; struct BusState **le_prev; } BusStateEntry;
struct BusState {
    Object obj;
    DeviceState *parent;
    char *name;
    HotplugHandler *hotplug_handler;
    int max_index;
    
   _Bool 
        realized;
    
   _Bool 
        full;
    int num_children;
    BusChildHead children;
    BusStateEntry sibling;
    ResettableState reset;
};
typedef struct GlobalProperty {
    const char *driver;
    const char *property;
    const char *value;
    
   _Bool 
        used;
    
   _Bool 
        optional;
} GlobalProperty;
DeviceState *qdev_new(const char *name);
DeviceState *qdev_try_new(const char *name);
_Bool 
    qdev_realize(DeviceState *dev, BusState *bus, Error **errp);
_Bool 
    qdev_realize_and_unref(DeviceState *dev, BusState *bus, Error **errp);
void qdev_unrealize(DeviceState *dev);
void qdev_set_legacy_instance_id(DeviceState *dev, int alias_id,
                                 int required_for_version);
HotplugHandler *qdev_get_bus_hotplug_handler(DeviceState *dev);
HotplugHandler *qdev_get_machine_hotplug_handler(DeviceState *dev);
_Bool 
    qdev_hotplug_allowed(DeviceState *dev, BusState *bus, Error **errp);
_Bool 
    qdev_hotunplug_allowed(DeviceState *dev, Error **errp);
HotplugHandler *qdev_get_hotplug_handler(DeviceState *dev);
void qdev_unplug(DeviceState *dev, Error **errp);
int qdev_sync_config(DeviceState *dev, Error **errp);
void qdev_simple_device_unplug_cb(HotplugHandler *hotplug_dev,
                                  DeviceState *dev, Error **errp);
void qdev_machine_creation_done(void);
_Bool 
    qdev_machine_modified(void);
void qdev_add_unplug_blocker(DeviceState *dev, Error *reason);
void qdev_del_unplug_blocker(DeviceState *dev, Error *reason);
_Bool 
    qdev_unplug_blocked(DeviceState *dev, Error **errp);
typedef enum {
    GPIO_POLARITY_ACTIVE_LOW,
    GPIO_POLARITY_ACTIVE_HIGH
} GpioPolarity;
qemu_irq qdev_get_gpio_in(DeviceState *dev, int n);
qemu_irq qdev_get_gpio_in_named(DeviceState *dev, const char *name, int n);
void qdev_connect_gpio_out(DeviceState *dev, int n, qemu_irq pin);
void qdev_connect_gpio_out_named(DeviceState *dev, const char *name, int n,
                                 qemu_irq input_pin);
qemu_irq qdev_get_gpio_out_connector(DeviceState *dev, const char *name, int n);
qemu_irq qdev_intercept_gpio_out(DeviceState *dev, qemu_irq icpt,
                                 const char *name, int n);
BusState *qdev_get_child_bus(DeviceState *dev, const char *name);
void qdev_init_gpio_in(DeviceState *dev, qemu_irq_handler handler, int n);
void qdev_init_gpio_out(DeviceState *dev, qemu_irq *pins, int n);
void qdev_init_gpio_out_named(DeviceState *dev, qemu_irq *pins,
                              const char *name, int n);
void qdev_init_gpio_in_named_with_opaque(DeviceState *dev,
                                         qemu_irq_handler handler,
                                         void *opaque,
                                         const char *name, int n);
void qdev_pass_gpios(DeviceState *dev, DeviceState *container,
                     const char *name);
BusState *qdev_get_parent_bus(const DeviceState *dev);
DeviceState *qdev_find_recursive(BusState *bus, const char *id);
typedef int (qbus_walkerfn)(BusState *bus, void *opaque);
typedef int (qdev_walkerfn)(DeviceState *dev, void *opaque);
void qbus_init(void *bus, size_t size, const char *typename,
               DeviceState *parent, const char *name);
BusState *qbus_new(const char *typename, DeviceState *parent, const char *name);
_Bool 
    qbus_realize(BusState *bus, Error **errp);
void qbus_unrealize(BusState *bus);
int qbus_walk_children(BusState *bus,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque);
int qdev_walk_children(DeviceState *dev,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque);
void device_cold_reset(DeviceState *dev);
void bus_cold_reset(BusState *bus);
_Bool 
    device_is_in_reset(DeviceState *dev);
_Bool 
    bus_is_in_reset(BusState *bus);
BusState *sysbus_get_default(void);
char *qdev_get_fw_dev_path(DeviceState *dev);
char *qdev_get_own_fw_dev_path_from_handler(BusState *bus, DeviceState *dev);
void device_class_set_props_n(DeviceClass *dc, const Property *props, size_t n);
void device_class_set_parent_realize(DeviceClass *dc,
                                     DeviceRealize dev_realize,
                                     DeviceRealize *parent_realize);
void device_class_set_legacy_reset(DeviceClass *dc,
                                   DeviceReset dev_reset);
void device_class_set_parent_unrealize(DeviceClass *dc,
                                       DeviceUnrealize dev_unrealize,
                                       DeviceUnrealize *parent_unrealize);
const VMStateDescription *qdev_get_vmsd(DeviceState *dev);
const char *qdev_fw_name(DeviceState *dev);
void qdev_assert_realized_properly(void);
Object *qdev_get_machine(void);
void qdev_create_fake_machine(void);
Object *machine_get_container(const char *name);
char *qdev_get_human_name(DeviceState *dev);
_Bool 
    qdev_set_parent_bus(DeviceState *dev, BusState *bus, Error **errp);
extern 
      _Bool 
           qdev_hot_removed;
char *qdev_get_dev_path(DeviceState *dev);
void qbus_set_hotplug_handler(BusState *bus, Object *handler);
void qbus_set_bus_hotplug_handler(BusState *bus);
void device_listener_register(DeviceListener *listener);
void device_listener_unregister(DeviceListener *listener);
_Bool 
    qdev_should_hide_device(const QDict *opts, 
                                               _Bool 
                                                    from_json, Error **errp);
typedef enum MachineInitPhase {
    PHASE_NO_MACHINE,
    PHASE_MACHINE_CREATED,
    PHASE_ACCEL_CREATED,
    PHASE_LATE_BACKENDS_CREATED,
    PHASE_MACHINE_INITIALIZED,
    PHASE_MACHINE_READY,
} MachineInitPhase;
_Bool 
    phase_check(MachineInitPhase phase);
void phase_advance(MachineInitPhase phase);
typedef void *PTR;
typedef uint64_t bfd_vma;
typedef int64_t bfd_signed_vma;
typedef uint8_t bfd_byte;
enum bfd_flavour {
  bfd_target_unknown_flavour,
  bfd_target_aout_flavour,
  bfd_target_coff_flavour,
  bfd_target_ecoff_flavour,
  bfd_target_elf_flavour,
  bfd_target_ieee_flavour,
  bfd_target_nlm_flavour,
  bfd_target_oasys_flavour,
  bfd_target_tekhex_flavour,
  bfd_target_srec_flavour,
  bfd_target_ihex_flavour,
  bfd_target_som_flavour,
  bfd_target_os9k_flavour,
  bfd_target_versados_flavour,
  bfd_target_msdos_flavour,
  bfd_target_evax_flavour
};
enum bfd_endian { BFD_ENDIAN_BIG, BFD_ENDIAN_LITTLE, BFD_ENDIAN_UNKNOWN };
enum bfd_architecture
{
  bfd_arch_unknown,
  bfd_arch_obscure,
  bfd_arch_m68k,
  bfd_arch_vax,
  bfd_arch_i960,
  bfd_arch_a29k,
  bfd_arch_sparc,
  bfd_arch_mips,
  bfd_arch_i386,
  bfd_arch_we32k,
  bfd_arch_tahoe,
  bfd_arch_i860,
  bfd_arch_romp,
  bfd_arch_alliant,
  bfd_arch_convex,
  bfd_arch_m88k,
  bfd_arch_pyramid,
  bfd_arch_h8300,
  bfd_arch_powerpc,
  bfd_arch_rs6000,
  bfd_arch_hppa,
  bfd_arch_d10v,
  bfd_arch_z8k,
  bfd_arch_h8500,
  bfd_arch_sh,
  bfd_arch_alpha,
  bfd_arch_arm,
  bfd_arch_ns32k,
  bfd_arch_w65,
  bfd_arch_tic30,
  bfd_arch_v850,
  bfd_arch_arc,
  bfd_arch_m32r,
  bfd_arch_mn10200,
  bfd_arch_mn10300,
  bfd_arch_avr,
  bfd_arch_microblaze,
  bfd_arch_moxie,
  bfd_arch_ia64,
  bfd_arch_rx,
  bfd_arch_loongarch,
  bfd_arch_last
  };
typedef struct symbol_cache_entry
{
    const char *name;
    union
    {
        PTR p;
        bfd_vma i;
    } udata;
} asymbol;
typedef int (*fprintf_function)(FILE *f, const char *fmt, ...)
    ;
enum dis_insn_type {
  dis_noninsn,
  dis_nonbranch,
  dis_branch,
  dis_condbranch,
  dis_jsr,
  dis_condjsr,
  dis_dref,
  dis_dref2
};
typedef struct disassemble_info {
  fprintf_function fprintf_func;
  FILE *stream;
  PTR application_data;
  enum bfd_flavour flavour;
  enum bfd_architecture arch;
  unsigned long mach;
  enum bfd_endian endian;
  asymbol **symbols;
  int num_symbols;
  unsigned long flags;
  PTR private_data;
  int (*read_memory_func)
    (bfd_vma memaddr, bfd_byte *myaddr, int length,
        struct disassemble_info *info);
  void (*memory_error_func)
    (int status, bfd_vma memaddr, struct disassemble_info *info);
  void (*print_address_func)
    (bfd_vma addr, struct disassemble_info *info);
    int (*print_insn)(bfd_vma addr, struct disassemble_info *info);
  int (* symbol_at_address_func)
    (bfd_vma addr, struct disassemble_info * info);
  const bfd_byte *buffer;
  bfd_vma buffer_vma;
  int buffer_length;
  int bytes_per_line;
  int bytes_per_chunk;
  enum bfd_endian display_endian;
  char insn_info_valid;
  char branch_delay_insns;
  char data_size;
  enum dis_insn_type insn_type;
  bfd_vma target;
  bfd_vma target2;
  char * disassembler_options;
  
 _Bool 
      show_opcodes;
  void *target_info;
  int cap_arch;
  int cap_mode;
  int cap_insn_unit;
  int cap_insn_split;
} disassemble_info;
typedef int (*disassembler_ftype) (bfd_vma, disassemble_info *);
int print_insn_tci(bfd_vma, disassemble_info*);
int print_insn_big_mips (bfd_vma, disassemble_info*);
int print_insn_little_mips (bfd_vma, disassemble_info*);
int print_insn_nanomips (bfd_vma, disassemble_info*);
int print_insn_m68k (bfd_vma, disassemble_info*);
int print_insn_z8001 (bfd_vma, disassemble_info*);
int print_insn_z8002 (bfd_vma, disassemble_info*);
int print_insn_h8300 (bfd_vma, disassemble_info*);
int print_insn_h8300h (bfd_vma, disassemble_info*);
int print_insn_h8300s (bfd_vma, disassemble_info*);
int print_insn_h8500 (bfd_vma, disassemble_info*);
int print_insn_arm_a64 (bfd_vma, disassemble_info*);
int print_insn_alpha (bfd_vma, disassemble_info*);
disassembler_ftype arc_get_disassembler (int, int);
int print_insn_sparc (bfd_vma, disassemble_info*);
int print_insn_big_a29k (bfd_vma, disassemble_info*);
int print_insn_little_a29k (bfd_vma, disassemble_info*);
int print_insn_i960 (bfd_vma, disassemble_info*);
int print_insn_sh (bfd_vma, disassemble_info*);
int print_insn_shl (bfd_vma, disassemble_info*);
int print_insn_hppa (bfd_vma, disassemble_info*);
int print_insn_m32r (bfd_vma, disassemble_info*);
int print_insn_m88k (bfd_vma, disassemble_info*);
int print_insn_mn10200 (bfd_vma, disassemble_info*);
int print_insn_mn10300 (bfd_vma, disassemble_info*);
int print_insn_ns32k (bfd_vma, disassemble_info*);
int print_insn_big_powerpc (bfd_vma, disassemble_info*);
int print_insn_little_powerpc (bfd_vma, disassemble_info*);
int print_insn_rs6000 (bfd_vma, disassemble_info*);
int print_insn_w65 (bfd_vma, disassemble_info*);
int print_insn_d10v (bfd_vma, disassemble_info*);
int print_insn_v850 (bfd_vma, disassemble_info*);
int print_insn_tic30 (bfd_vma, disassemble_info*);
int print_insn_microblaze (bfd_vma, disassemble_info*);
int print_insn_ia64 (bfd_vma, disassemble_info*);
int print_insn_xtensa (bfd_vma, disassemble_info*);
int print_insn_riscv32 (bfd_vma, disassemble_info*);
int print_insn_riscv64 (bfd_vma, disassemble_info*);
int print_insn_riscv128 (bfd_vma, disassemble_info*);
int print_insn_rx(bfd_vma, disassemble_info *);
int print_insn_hexagon(bfd_vma, disassemble_info *);
int print_insn_loongarch(bfd_vma, disassemble_info *);
_Bool 
    cap_disas_target(disassemble_info *info, uint64_t pc, size_t size);
_Bool 
    cap_disas_host(disassemble_info *info, const void *code, size_t size);
_Bool 
    cap_disas_monitor(disassemble_info *info, uint64_t pc, int count);
_Bool 
    cap_disas_plugin(disassemble_info *info, uint64_t pc, size_t size);
typedef 
       _Bool 
            bfd_boolean;
typedef struct MemTxAttrs {
    unsigned int unspecified:1;
    unsigned int secure:1;
    unsigned int space:2;
    unsigned int user:1;
    unsigned int memory:1;
    unsigned int requester_id:16;
    unsigned int pid:8;
} MemTxAttrs;
typedef uint32_t MemTxResult;
typedef struct CPUBreakpoint {
    vaddr pc;
    int flags;
    union { struct CPUBreakpoint *tqe_next; QTailQLink tqe_circ; } entry;
} CPUBreakpoint;
typedef struct CPUWatchpoint {
    vaddr vaddr;
    vaddr len;
    vaddr hitaddr;
    MemTxAttrs hitattrs;
    int flags;
    union { struct CPUWatchpoint *tqe_next; QTailQLink tqe_circ; } entry;
} CPUWatchpoint;
typedef enum MMUAccessType {
    MMU_DATA_LOAD = 0,
    MMU_DATA_STORE = 1,
    MMU_INST_FETCH = 2
} MMUAccessType;
typedef union CPUTLBEntry {
    struct {
        uint64_t addr_read;
        uint64_t addr_write;
        uint64_t addr_code;
        uintptr_t addend;
    };
    uint64_t addr_idx[4];
} CPUTLBEntry;
;
typedef struct CPUTLBDescFast {
    uintptr_t mask;
    CPUTLBEntry *table;
} CPUTLBDescFast ;
typedef enum IoOperationType {
    IO_OPERATION_TYPE_READ,
    IO_OPERATION_TYPE_WRITE,
    IO_OPERATION_TYPE__MAX,
} IoOperationType;
extern const QEnumLookup IoOperationType_lookup;
typedef enum OnOffAuto {
    ON_OFF_AUTO_AUTO,
    ON_OFF_AUTO_ON,
    ON_OFF_AUTO_OFF,
    ON_OFF_AUTO__MAX,
} OnOffAuto;
extern const QEnumLookup OnOffAuto_lookup;
typedef enum OnOffSplit {
    ON_OFF_SPLIT_ON,
    ON_OFF_SPLIT_OFF,
    ON_OFF_SPLIT_SPLIT,
    ON_OFF_SPLIT__MAX,
} OnOffSplit;
extern const QEnumLookup OnOffSplit_lookup;
typedef struct StrOrNull StrOrNull;
typedef enum OffAutoPCIBAR {
    OFF_AUTO_PCIBAR_OFF,
    OFF_AUTO_PCIBAR_AUTO,
    OFF_AUTO_PCIBAR_BAR0,
    OFF_AUTO_PCIBAR_BAR1,
    OFF_AUTO_PCIBAR_BAR2,
    OFF_AUTO_PCIBAR_BAR3,
    OFF_AUTO_PCIBAR_BAR4,
    OFF_AUTO_PCIBAR_BAR5,
    OFF_AUTO_PCIBAR__MAX,
} OffAutoPCIBAR;
extern const QEnumLookup OffAutoPCIBAR_lookup;
typedef enum PCIELinkSpeed {
    PCIE_LINK_SPEED_2_5,
    PCIE_LINK_SPEED_5,
    PCIE_LINK_SPEED_8,
    PCIE_LINK_SPEED_16,
    PCIE_LINK_SPEED_32,
    PCIE_LINK_SPEED_64,
    PCIE_LINK_SPEED__MAX,
} PCIELinkSpeed;
extern const QEnumLookup PCIELinkSpeed_lookup;
typedef enum PCIELinkWidth {
    PCIE_LINK_WIDTH_1,
    PCIE_LINK_WIDTH_2,
    PCIE_LINK_WIDTH_4,
    PCIE_LINK_WIDTH_8,
    PCIE_LINK_WIDTH_12,
    PCIE_LINK_WIDTH_16,
    PCIE_LINK_WIDTH_32,
    PCIE_LINK_WIDTH__MAX,
} PCIELinkWidth;
extern const QEnumLookup PCIELinkWidth_lookup;
typedef enum HostMemPolicy {
    HOST_MEM_POLICY_DEFAULT,
    HOST_MEM_POLICY_PREFERRED,
    HOST_MEM_POLICY_BIND,
    HOST_MEM_POLICY_INTERLEAVE,
    HOST_MEM_POLICY__MAX,
} HostMemPolicy;
extern const QEnumLookup HostMemPolicy_lookup;
typedef enum NetFilterDirection {
    NET_FILTER_DIRECTION_ALL,
    NET_FILTER_DIRECTION_RX,
    NET_FILTER_DIRECTION_TX,
    NET_FILTER_DIRECTION__MAX,
} NetFilterDirection;
extern const QEnumLookup NetFilterDirection_lookup;
typedef enum GrabToggleKeys {
    GRAB_TOGGLE_KEYS_CTRL_CTRL,
    GRAB_TOGGLE_KEYS_ALT_ALT,
    GRAB_TOGGLE_KEYS_SHIFT_SHIFT,
    GRAB_TOGGLE_KEYS_META_META,
    GRAB_TOGGLE_KEYS_SCROLLLOCK,
    GRAB_TOGGLE_KEYS_CTRL_SCROLLLOCK,
    GRAB_TOGGLE_KEYS__MAX,
} GrabToggleKeys;
extern const QEnumLookup GrabToggleKeys_lookup;
typedef struct HumanReadableText HumanReadableText;
struct StrOrNull {
    QType type;
    union {
        char *s;
        QNull *n;
    } u;
};
void qapi_free_StrOrNull(StrOrNull *obj);
typedef StrOrNull *StrOrNull_autoptr; typedef GList *StrOrNull_listautoptr; typedef GSList *StrOrNull_slistautoptr; typedef GQueue *StrOrNull_queueautoptr;
struct HumanReadableText {
    char *human_readable_text;
};
void qapi_free_HumanReadableText(HumanReadableText *obj);
typedef HumanReadableText *HumanReadableText_autoptr; typedef GList *HumanReadableText_listautoptr; typedef GSList *HumanReadableText_slistautoptr; typedef GQueue *HumanReadableText_queueautoptr;
typedef enum S390CpuEntitlement {
    S390_CPU_ENTITLEMENT_AUTO,
    S390_CPU_ENTITLEMENT_LOW,
    S390_CPU_ENTITLEMENT_MEDIUM,
    S390_CPU_ENTITLEMENT_HIGH,
    S390_CPU_ENTITLEMENT__MAX,
} S390CpuEntitlement;
extern const QEnumLookup S390CpuEntitlement_lookup;
typedef enum CpuTopologyLevel {
    CPU_TOPOLOGY_LEVEL_THREAD,
    CPU_TOPOLOGY_LEVEL_CORE,
    CPU_TOPOLOGY_LEVEL_MODULE,
    CPU_TOPOLOGY_LEVEL_CLUSTER,
    CPU_TOPOLOGY_LEVEL_DIE,
    CPU_TOPOLOGY_LEVEL_SOCKET,
    CPU_TOPOLOGY_LEVEL_BOOK,
    CPU_TOPOLOGY_LEVEL_DRAWER,
    CPU_TOPOLOGY_LEVEL_DEFAULT,
    CPU_TOPOLOGY_LEVEL__MAX,
} CpuTopologyLevel;
extern const QEnumLookup CpuTopologyLevel_lookup;
typedef enum CacheLevelAndType {
    CACHE_LEVEL_AND_TYPE_L1D,
    CACHE_LEVEL_AND_TYPE_L1I,
    CACHE_LEVEL_AND_TYPE_L2,
    CACHE_LEVEL_AND_TYPE_L3,
    CACHE_LEVEL_AND_TYPE__MAX,
} CacheLevelAndType;
extern const QEnumLookup CacheLevelAndType_lookup;
typedef struct SmpCacheProperties SmpCacheProperties;
typedef struct SmpCachePropertiesList SmpCachePropertiesList;
typedef struct SmpCachePropertiesWrapper SmpCachePropertiesWrapper;
struct SmpCacheProperties {
    CacheLevelAndType cache;
    CpuTopologyLevel topology;
};
void qapi_free_SmpCacheProperties(SmpCacheProperties *obj);
typedef SmpCacheProperties *SmpCacheProperties_autoptr; typedef GList *SmpCacheProperties_listautoptr; typedef GSList *SmpCacheProperties_slistautoptr; typedef GQueue *SmpCacheProperties_queueautoptr;
struct SmpCachePropertiesList {
    SmpCachePropertiesList *next;
    SmpCacheProperties *value;
};
void qapi_free_SmpCachePropertiesList(SmpCachePropertiesList *obj);
typedef SmpCachePropertiesList *SmpCachePropertiesList_autoptr; typedef GList *SmpCachePropertiesList_listautoptr; typedef GSList *SmpCachePropertiesList_slistautoptr; typedef GQueue *SmpCachePropertiesList_queueautoptr;
struct SmpCachePropertiesWrapper {
    SmpCachePropertiesList *caches;
};
void qapi_free_SmpCachePropertiesWrapper(SmpCachePropertiesWrapper *obj);
typedef SmpCachePropertiesWrapper *SmpCachePropertiesWrapper_autoptr; typedef GList *SmpCachePropertiesWrapper_listautoptr; typedef GSList *SmpCachePropertiesWrapper_slistautoptr; typedef GQueue *SmpCachePropertiesWrapper_queueautoptr;
typedef enum SysEmuTarget {
    SYS_EMU_TARGET_AARCH64,
    SYS_EMU_TARGET_ALPHA,
    SYS_EMU_TARGET_ARM,
    SYS_EMU_TARGET_AVR,
    SYS_EMU_TARGET_HPPA,
    SYS_EMU_TARGET_I386,
    SYS_EMU_TARGET_LOONGARCH64,
    SYS_EMU_TARGET_M68K,
    SYS_EMU_TARGET_MICROBLAZE,
    SYS_EMU_TARGET_MICROBLAZEEL,
    SYS_EMU_TARGET_MIPS,
    SYS_EMU_TARGET_MIPS64,
    SYS_EMU_TARGET_MIPS64EL,
    SYS_EMU_TARGET_MIPSEL,
    SYS_EMU_TARGET_OR1K,
    SYS_EMU_TARGET_PPC,
    SYS_EMU_TARGET_PPC64,
    SYS_EMU_TARGET_RISCV32,
    SYS_EMU_TARGET_RISCV64,
    SYS_EMU_TARGET_RX,
    SYS_EMU_TARGET_S390X,
    SYS_EMU_TARGET_SH4,
    SYS_EMU_TARGET_SH4EB,
    SYS_EMU_TARGET_SPARC,
    SYS_EMU_TARGET_SPARC64,
    SYS_EMU_TARGET_TRICORE,
    SYS_EMU_TARGET_X86_64,
    SYS_EMU_TARGET_XTENSA,
    SYS_EMU_TARGET_XTENSAEB,
    SYS_EMU_TARGET__MAX,
} SysEmuTarget;
extern const QEnumLookup SysEmuTarget_lookup;
typedef enum S390CpuState {
    S390_CPU_STATE_UNINITIALIZED,
    S390_CPU_STATE_STOPPED,
    S390_CPU_STATE_CHECK_STOP,
    S390_CPU_STATE_OPERATING,
    S390_CPU_STATE_LOAD,
    S390_CPU_STATE__MAX,
} S390CpuState;
extern const QEnumLookup S390CpuState_lookup;
typedef struct CpuInfoS390 CpuInfoS390;
typedef struct q_obj_CpuInfoFast_base q_obj_CpuInfoFast_base;
typedef struct CpuInfoFast CpuInfoFast;
typedef struct CpuInfoFastList CpuInfoFastList;
typedef struct CompatProperty CompatProperty;
typedef struct CompatPropertyList CompatPropertyList;
typedef struct MachineInfo MachineInfo;
typedef struct q_obj_query_machines_arg q_obj_query_machines_arg;
typedef struct MachineInfoList MachineInfoList;
typedef struct CurrentMachineParams CurrentMachineParams;
typedef struct TargetInfo TargetInfo;
typedef struct UuidInfo UuidInfo;
typedef struct GuidInfo GuidInfo;
typedef enum LostTickPolicy {
    LOST_TICK_POLICY_DISCARD,
    LOST_TICK_POLICY_DELAY,
    LOST_TICK_POLICY_SLEW,
    LOST_TICK_POLICY__MAX,
} LostTickPolicy;
extern const QEnumLookup LostTickPolicy_lookup;
typedef struct KvmInfo KvmInfo;
typedef enum NumaOptionsType {
    NUMA_OPTIONS_TYPE_NODE,
    NUMA_OPTIONS_TYPE_DIST,
    NUMA_OPTIONS_TYPE_CPU,
    NUMA_OPTIONS_TYPE_HMAT_LB,
    NUMA_OPTIONS_TYPE_HMAT_CACHE,
    NUMA_OPTIONS_TYPE__MAX,
} NumaOptionsType;
extern const QEnumLookup NumaOptionsType_lookup;
typedef struct q_obj_NumaOptions_base q_obj_NumaOptions_base;
typedef struct NumaOptions NumaOptions;
typedef struct NumaNodeOptions NumaNodeOptions;
typedef struct NumaDistOptions NumaDistOptions;
typedef struct CXLFixedMemoryWindowOptions CXLFixedMemoryWindowOptions;
typedef struct CXLFixedMemoryWindowOptionsList CXLFixedMemoryWindowOptionsList;
typedef struct CXLFMWProperties CXLFMWProperties;
typedef enum X86CPURegister32 {
    X86_CPU_REGISTER32_EAX,
    X86_CPU_REGISTER32_EBX,
    X86_CPU_REGISTER32_ECX,
    X86_CPU_REGISTER32_EDX,
    X86_CPU_REGISTER32_ESP,
    X86_CPU_REGISTER32_EBP,
    X86_CPU_REGISTER32_ESI,
    X86_CPU_REGISTER32_EDI,
    X86_CPU_REGISTER32__MAX,
} X86CPURegister32;
extern const QEnumLookup X86CPURegister32_lookup;
typedef struct X86CPUFeatureWordInfo X86CPUFeatureWordInfo;
typedef struct X86CPUFeatureWordInfoList X86CPUFeatureWordInfoList;
typedef struct DummyForceArrays DummyForceArrays;
typedef struct NumaCpuOptions NumaCpuOptions;
typedef enum HmatLBMemoryHierarchy {
    HMAT_LB_MEMORY_HIERARCHY_MEMORY,
    HMAT_LB_MEMORY_HIERARCHY_FIRST_LEVEL,
    HMAT_LB_MEMORY_HIERARCHY_SECOND_LEVEL,
    HMAT_LB_MEMORY_HIERARCHY_THIRD_LEVEL,
    HMAT_LB_MEMORY_HIERARCHY__MAX,
} HmatLBMemoryHierarchy;
extern const QEnumLookup HmatLBMemoryHierarchy_lookup;
typedef enum HmatLBDataType {
    HMAT_LB_DATA_TYPE_ACCESS_LATENCY,
    HMAT_LB_DATA_TYPE_READ_LATENCY,
    HMAT_LB_DATA_TYPE_WRITE_LATENCY,
    HMAT_LB_DATA_TYPE_ACCESS_BANDWIDTH,
    HMAT_LB_DATA_TYPE_READ_BANDWIDTH,
    HMAT_LB_DATA_TYPE_WRITE_BANDWIDTH,
    HMAT_LB_DATA_TYPE__MAX,
} HmatLBDataType;
extern const QEnumLookup HmatLBDataType_lookup;
typedef struct NumaHmatLBOptions NumaHmatLBOptions;
typedef enum HmatCacheAssociativity {
    HMAT_CACHE_ASSOCIATIVITY_NONE,
    HMAT_CACHE_ASSOCIATIVITY_DIRECT,
    HMAT_CACHE_ASSOCIATIVITY_COMPLEX,
    HMAT_CACHE_ASSOCIATIVITY__MAX,
} HmatCacheAssociativity;
extern const QEnumLookup HmatCacheAssociativity_lookup;
typedef enum HmatCacheWritePolicy {
    HMAT_CACHE_WRITE_POLICY_NONE,
    HMAT_CACHE_WRITE_POLICY_WRITE_BACK,
    HMAT_CACHE_WRITE_POLICY_WRITE_THROUGH,
    HMAT_CACHE_WRITE_POLICY__MAX,
} HmatCacheWritePolicy;
extern const QEnumLookup HmatCacheWritePolicy_lookup;
typedef struct NumaHmatCacheOptions NumaHmatCacheOptions;
typedef struct q_obj_memsave_arg q_obj_memsave_arg;
typedef struct q_obj_pmemsave_arg q_obj_pmemsave_arg;
typedef struct Memdev Memdev;
typedef struct MemdevList MemdevList;
typedef struct CpuInstanceProperties CpuInstanceProperties;
typedef struct HotpluggableCPU HotpluggableCPU;
typedef struct HotpluggableCPUList HotpluggableCPUList;
typedef struct q_obj_balloon_arg q_obj_balloon_arg;
typedef struct BalloonInfo BalloonInfo;
typedef struct q_obj_BALLOON_CHANGE_arg q_obj_BALLOON_CHANGE_arg;
typedef struct HvBalloonInfo HvBalloonInfo;
typedef struct MemoryInfo MemoryInfo;
typedef struct PCDIMMDeviceInfo PCDIMMDeviceInfo;
typedef struct VirtioPMEMDeviceInfo VirtioPMEMDeviceInfo;
typedef struct VirtioMEMDeviceInfo VirtioMEMDeviceInfo;
typedef struct SgxEPCDeviceInfo SgxEPCDeviceInfo;
typedef struct HvBalloonDeviceInfo HvBalloonDeviceInfo;
typedef enum MemoryDeviceInfoKind {
    MEMORY_DEVICE_INFO_KIND_DIMM,
    MEMORY_DEVICE_INFO_KIND_NVDIMM,
    MEMORY_DEVICE_INFO_KIND_VIRTIO_PMEM,
    MEMORY_DEVICE_INFO_KIND_VIRTIO_MEM,
    MEMORY_DEVICE_INFO_KIND_SGX_EPC,
    MEMORY_DEVICE_INFO_KIND_HV_BALLOON,
    MEMORY_DEVICE_INFO_KIND__MAX,
} MemoryDeviceInfoKind;
extern const QEnumLookup MemoryDeviceInfoKind_lookup;
typedef struct PCDIMMDeviceInfoWrapper PCDIMMDeviceInfoWrapper;
typedef struct VirtioPMEMDeviceInfoWrapper VirtioPMEMDeviceInfoWrapper;
typedef struct VirtioMEMDeviceInfoWrapper VirtioMEMDeviceInfoWrapper;
typedef struct SgxEPCDeviceInfoWrapper SgxEPCDeviceInfoWrapper;
typedef struct HvBalloonDeviceInfoWrapper HvBalloonDeviceInfoWrapper;
typedef struct q_obj_MemoryDeviceInfo_base q_obj_MemoryDeviceInfo_base;
typedef struct MemoryDeviceInfo MemoryDeviceInfo;
typedef struct SgxEPC SgxEPC;
typedef struct SgxEPCList SgxEPCList;
typedef struct SgxEPCProperties SgxEPCProperties;
typedef struct MemoryDeviceInfoList MemoryDeviceInfoList;
typedef struct q_obj_MEMORY_DEVICE_SIZE_CHANGE_arg q_obj_MEMORY_DEVICE_SIZE_CHANGE_arg;
typedef struct BootConfiguration BootConfiguration;
typedef struct SMPConfiguration SMPConfiguration;
typedef enum SmbiosEntryPointType {
    SMBIOS_ENTRY_POINT_TYPE_32,
    SMBIOS_ENTRY_POINT_TYPE_64,
    SMBIOS_ENTRY_POINT_TYPE_AUTO,
    SMBIOS_ENTRY_POINT_TYPE__MAX,
} SmbiosEntryPointType;
extern const QEnumLookup SmbiosEntryPointType_lookup;
typedef struct MemorySizeConfiguration MemorySizeConfiguration;
typedef struct q_obj_dumpdtb_arg q_obj_dumpdtb_arg;
struct CpuInfoS390 {
    S390CpuState cpu_state;
    
   _Bool 
        has_dedicated;
    
   _Bool 
        dedicated;
    
   _Bool 
        has_entitlement;
    S390CpuEntitlement entitlement;
};
void qapi_free_CpuInfoS390(CpuInfoS390 *obj);
typedef CpuInfoS390 *CpuInfoS390_autoptr; typedef GList *CpuInfoS390_listautoptr; typedef GSList *CpuInfoS390_slistautoptr; typedef GQueue *CpuInfoS390_queueautoptr;
struct q_obj_CpuInfoFast_base {
    int64_t cpu_index;
    char *qom_path;
    int64_t thread_id;
    CpuInstanceProperties *props;
    SysEmuTarget target;
};
struct CpuInfoFast {
    int64_t cpu_index;
    char *qom_path;
    int64_t thread_id;
    CpuInstanceProperties *props;
    SysEmuTarget target;
    union {
        CpuInfoS390 s390x;
    } u;
};
void qapi_free_CpuInfoFast(CpuInfoFast *obj);
typedef CpuInfoFast *CpuInfoFast_autoptr; typedef GList *CpuInfoFast_listautoptr; typedef GSList *CpuInfoFast_slistautoptr; typedef GQueue *CpuInfoFast_queueautoptr;
struct CpuInfoFastList {
    CpuInfoFastList *next;
    CpuInfoFast *value;
};
void qapi_free_CpuInfoFastList(CpuInfoFastList *obj);
typedef CpuInfoFastList *CpuInfoFastList_autoptr; typedef GList *CpuInfoFastList_listautoptr; typedef GSList *CpuInfoFastList_slistautoptr; typedef GQueue *CpuInfoFastList_queueautoptr;
struct CompatProperty {
    char *qom_type;
    char *property;
    char *value;
};
void qapi_free_CompatProperty(CompatProperty *obj);
typedef CompatProperty *CompatProperty_autoptr; typedef GList *CompatProperty_listautoptr; typedef GSList *CompatProperty_slistautoptr; typedef GQueue *CompatProperty_queueautoptr;
struct CompatPropertyList {
    CompatPropertyList *next;
    CompatProperty *value;
};
void qapi_free_CompatPropertyList(CompatPropertyList *obj);
typedef CompatPropertyList *CompatPropertyList_autoptr; typedef GList *CompatPropertyList_listautoptr; typedef GSList *CompatPropertyList_slistautoptr; typedef GQueue *CompatPropertyList_queueautoptr;
struct MachineInfo {
    char *name;
    char *alias;
    
   _Bool 
        has_is_default;
    
   _Bool 
        is_default;
    int64_t cpu_max;
    
   _Bool 
        hotpluggable_cpus;
    
   _Bool 
        numa_mem_supported;
    
   _Bool 
        deprecated;
    char *default_cpu_type;
    char *default_ram_id;
    
   _Bool 
        acpi;
    
   _Bool 
        has_compat_props;
    CompatPropertyList *compat_props;
};
void qapi_free_MachineInfo(MachineInfo *obj);
typedef MachineInfo *MachineInfo_autoptr; typedef GList *MachineInfo_listautoptr; typedef GSList *MachineInfo_slistautoptr; typedef GQueue *MachineInfo_queueautoptr;
struct q_obj_query_machines_arg {
    
   _Bool 
        has_compat_props;
    
   _Bool 
        compat_props;
};
struct MachineInfoList {
    MachineInfoList *next;
    MachineInfo *value;
};
void qapi_free_MachineInfoList(MachineInfoList *obj);
typedef MachineInfoList *MachineInfoList_autoptr; typedef GList *MachineInfoList_listautoptr; typedef GSList *MachineInfoList_slistautoptr; typedef GQueue *MachineInfoList_queueautoptr;
struct CurrentMachineParams {
    
   _Bool 
        wakeup_suspend_support;
};
void qapi_free_CurrentMachineParams(CurrentMachineParams *obj);
typedef CurrentMachineParams *CurrentMachineParams_autoptr; typedef GList *CurrentMachineParams_listautoptr; typedef GSList *CurrentMachineParams_slistautoptr; typedef GQueue *CurrentMachineParams_queueautoptr;
struct TargetInfo {
    SysEmuTarget arch;
};
void qapi_free_TargetInfo(TargetInfo *obj);
typedef TargetInfo *TargetInfo_autoptr; typedef GList *TargetInfo_listautoptr; typedef GSList *TargetInfo_slistautoptr; typedef GQueue *TargetInfo_queueautoptr;
struct UuidInfo {
    char *UUID;
};
void qapi_free_UuidInfo(UuidInfo *obj);
typedef UuidInfo *UuidInfo_autoptr; typedef GList *UuidInfo_listautoptr; typedef GSList *UuidInfo_slistautoptr; typedef GQueue *UuidInfo_queueautoptr;
struct GuidInfo {
    char *guid;
};
void qapi_free_GuidInfo(GuidInfo *obj);
typedef GuidInfo *GuidInfo_autoptr; typedef GList *GuidInfo_listautoptr; typedef GSList *GuidInfo_slistautoptr; typedef GQueue *GuidInfo_queueautoptr;
struct KvmInfo {
    
   _Bool 
        enabled;
    
   _Bool 
        present;
};
void qapi_free_KvmInfo(KvmInfo *obj);
typedef KvmInfo *KvmInfo_autoptr; typedef GList *KvmInfo_listautoptr; typedef GSList *KvmInfo_slistautoptr; typedef GQueue *KvmInfo_queueautoptr;
struct q_obj_NumaOptions_base {
    NumaOptionsType type;
};
struct NumaNodeOptions {
    
   _Bool 
        has_nodeid;
    uint16_t nodeid;
    
   _Bool 
        has_cpus;
    uint16List *cpus;
    
   _Bool 
        has_mem;
    uint64_t mem;
    char *memdev;
    
   _Bool 
        has_initiator;
    uint16_t initiator;
};
struct NumaDistOptions {
    uint16_t src;
    uint16_t dst;
    uint8_t val;
};
struct NumaCpuOptions {
    
   _Bool 
        has_node_id;
    int64_t node_id;
    
   _Bool 
        has_drawer_id;
    int64_t drawer_id;
    
   _Bool 
        has_book_id;
    int64_t book_id;
    
   _Bool 
        has_socket_id;
    int64_t socket_id;
    
   _Bool 
        has_die_id;
    int64_t die_id;
    
   _Bool 
        has_cluster_id;
    int64_t cluster_id;
    
   _Bool 
        has_module_id;
    int64_t module_id;
    
   _Bool 
        has_core_id;
    int64_t core_id;
    
   _Bool 
        has_thread_id;
    int64_t thread_id;
};
struct NumaHmatLBOptions {
    uint16_t initiator;
    uint16_t target;
    HmatLBMemoryHierarchy hierarchy;
    HmatLBDataType data_type;
    
   _Bool 
        has_latency;
    uint64_t latency;
    
   _Bool 
        has_bandwidth;
    uint64_t bandwidth;
};
struct NumaHmatCacheOptions {
    uint32_t node_id;
    uint64_t size;
    uint8_t level;
    HmatCacheAssociativity associativity;
    HmatCacheWritePolicy policy;
    uint16_t line;
};
struct NumaOptions {
    NumaOptionsType type;
    union {
        NumaNodeOptions node;
        NumaDistOptions dist;
        NumaCpuOptions cpu;
        NumaHmatLBOptions hmat_lb;
        NumaHmatCacheOptions hmat_cache;
    } u;
};
void qapi_free_NumaOptions(NumaOptions *obj);
typedef NumaOptions *NumaOptions_autoptr; typedef GList *NumaOptions_listautoptr; typedef GSList *NumaOptions_slistautoptr; typedef GQueue *NumaOptions_queueautoptr;
void qapi_free_NumaNodeOptions(NumaNodeOptions *obj);
typedef NumaNodeOptions *NumaNodeOptions_autoptr; typedef GList *NumaNodeOptions_listautoptr; typedef GSList *NumaNodeOptions_slistautoptr; typedef GQueue *NumaNodeOptions_queueautoptr;
void qapi_free_NumaDistOptions(NumaDistOptions *obj);
typedef NumaDistOptions *NumaDistOptions_autoptr; typedef GList *NumaDistOptions_listautoptr; typedef GSList *NumaDistOptions_slistautoptr; typedef GQueue *NumaDistOptions_queueautoptr;
struct CXLFixedMemoryWindowOptions {
    uint64_t size;
    
   _Bool 
        has_interleave_granularity;
    uint64_t interleave_granularity;
    strList *targets;
};
void qapi_free_CXLFixedMemoryWindowOptions(CXLFixedMemoryWindowOptions *obj);
typedef CXLFixedMemoryWindowOptions *CXLFixedMemoryWindowOptions_autoptr; typedef GList *CXLFixedMemoryWindowOptions_listautoptr; typedef GSList *CXLFixedMemoryWindowOptions_slistautoptr; typedef GQueue *CXLFixedMemoryWindowOptions_queueautoptr;
struct CXLFixedMemoryWindowOptionsList {
    CXLFixedMemoryWindowOptionsList *next;
    CXLFixedMemoryWindowOptions *value;
};
void qapi_free_CXLFixedMemoryWindowOptionsList(CXLFixedMemoryWindowOptionsList *obj);
typedef CXLFixedMemoryWindowOptionsList *CXLFixedMemoryWindowOptionsList_autoptr; typedef GList *CXLFixedMemoryWindowOptionsList_listautoptr; typedef GSList *CXLFixedMemoryWindowOptionsList_slistautoptr; typedef GQueue *CXLFixedMemoryWindowOptionsList_queueautoptr;
struct CXLFMWProperties {
    CXLFixedMemoryWindowOptionsList *cxl_fmw;
};
void qapi_free_CXLFMWProperties(CXLFMWProperties *obj);
typedef CXLFMWProperties *CXLFMWProperties_autoptr; typedef GList *CXLFMWProperties_listautoptr; typedef GSList *CXLFMWProperties_slistautoptr; typedef GQueue *CXLFMWProperties_queueautoptr;
struct X86CPUFeatureWordInfo {
    int64_t cpuid_input_eax;
    
   _Bool 
        has_cpuid_input_ecx;
    int64_t cpuid_input_ecx;
    X86CPURegister32 cpuid_register;
    int64_t features;
};
void qapi_free_X86CPUFeatureWordInfo(X86CPUFeatureWordInfo *obj);
typedef X86CPUFeatureWordInfo *X86CPUFeatureWordInfo_autoptr; typedef GList *X86CPUFeatureWordInfo_listautoptr; typedef GSList *X86CPUFeatureWordInfo_slistautoptr; typedef GQueue *X86CPUFeatureWordInfo_queueautoptr;
struct X86CPUFeatureWordInfoList {
    X86CPUFeatureWordInfoList *next;
    X86CPUFeatureWordInfo *value;
};
void qapi_free_X86CPUFeatureWordInfoList(X86CPUFeatureWordInfoList *obj);
typedef X86CPUFeatureWordInfoList *X86CPUFeatureWordInfoList_autoptr; typedef GList *X86CPUFeatureWordInfoList_listautoptr; typedef GSList *X86CPUFeatureWordInfoList_slistautoptr; typedef GQueue *X86CPUFeatureWordInfoList_queueautoptr;
struct DummyForceArrays {
    X86CPUFeatureWordInfoList *unused;
};
void qapi_free_DummyForceArrays(DummyForceArrays *obj);
typedef DummyForceArrays *DummyForceArrays_autoptr; typedef GList *DummyForceArrays_listautoptr; typedef GSList *DummyForceArrays_slistautoptr; typedef GQueue *DummyForceArrays_queueautoptr;
void qapi_free_NumaCpuOptions(NumaCpuOptions *obj);
typedef NumaCpuOptions *NumaCpuOptions_autoptr; typedef GList *NumaCpuOptions_listautoptr; typedef GSList *NumaCpuOptions_slistautoptr; typedef GQueue *NumaCpuOptions_queueautoptr;
void qapi_free_NumaHmatLBOptions(NumaHmatLBOptions *obj);
typedef NumaHmatLBOptions *NumaHmatLBOptions_autoptr; typedef GList *NumaHmatLBOptions_listautoptr; typedef GSList *NumaHmatLBOptions_slistautoptr; typedef GQueue *NumaHmatLBOptions_queueautoptr;
void qapi_free_NumaHmatCacheOptions(NumaHmatCacheOptions *obj);
typedef NumaHmatCacheOptions *NumaHmatCacheOptions_autoptr; typedef GList *NumaHmatCacheOptions_listautoptr; typedef GSList *NumaHmatCacheOptions_slistautoptr; typedef GQueue *NumaHmatCacheOptions_queueautoptr;
struct q_obj_memsave_arg {
    uint64_t val;
    uint64_t size;
    char *filename;
    
   _Bool 
        has_cpu_index;
    int64_t cpu_index;
};
struct q_obj_pmemsave_arg {
    uint64_t val;
    uint64_t size;
    char *filename;
};
struct Memdev {
    char *id;
    uint64_t size;
    
   _Bool 
        merge;
    
   _Bool 
        dump;
    
   _Bool 
        prealloc;
    
   _Bool 
        share;
    
   _Bool 
        has_reserve;
    
   _Bool 
        reserve;
    uint16List *host_nodes;
    HostMemPolicy policy;
};
void qapi_free_Memdev(Memdev *obj);
typedef Memdev *Memdev_autoptr; typedef GList *Memdev_listautoptr; typedef GSList *Memdev_slistautoptr; typedef GQueue *Memdev_queueautoptr;
struct MemdevList {
    MemdevList *next;
    Memdev *value;
};
void qapi_free_MemdevList(MemdevList *obj);
typedef MemdevList *MemdevList_autoptr; typedef GList *MemdevList_listautoptr; typedef GSList *MemdevList_slistautoptr; typedef GQueue *MemdevList_queueautoptr;
struct CpuInstanceProperties {
    
   _Bool 
        has_node_id;
    int64_t node_id;
    
   _Bool 
        has_drawer_id;
    int64_t drawer_id;
    
   _Bool 
        has_book_id;
    int64_t book_id;
    
   _Bool 
        has_socket_id;
    int64_t socket_id;
    
   _Bool 
        has_die_id;
    int64_t die_id;
    
   _Bool 
        has_cluster_id;
    int64_t cluster_id;
    
   _Bool 
        has_module_id;
    int64_t module_id;
    
   _Bool 
        has_core_id;
    int64_t core_id;
    
   _Bool 
        has_thread_id;
    int64_t thread_id;
};
void qapi_free_CpuInstanceProperties(CpuInstanceProperties *obj);
typedef CpuInstanceProperties *CpuInstanceProperties_autoptr; typedef GList *CpuInstanceProperties_listautoptr; typedef GSList *CpuInstanceProperties_slistautoptr; typedef GQueue *CpuInstanceProperties_queueautoptr;
struct HotpluggableCPU {
    char *type;
    int64_t vcpus_count;
    CpuInstanceProperties *props;
    char *qom_path;
};
void qapi_free_HotpluggableCPU(HotpluggableCPU *obj);
typedef HotpluggableCPU *HotpluggableCPU_autoptr; typedef GList *HotpluggableCPU_listautoptr; typedef GSList *HotpluggableCPU_slistautoptr; typedef GQueue *HotpluggableCPU_queueautoptr;
struct HotpluggableCPUList {
    HotpluggableCPUList *next;
    HotpluggableCPU *value;
};
void qapi_free_HotpluggableCPUList(HotpluggableCPUList *obj);
typedef HotpluggableCPUList *HotpluggableCPUList_autoptr; typedef GList *HotpluggableCPUList_listautoptr; typedef GSList *HotpluggableCPUList_slistautoptr; typedef GQueue *HotpluggableCPUList_queueautoptr;
struct q_obj_balloon_arg {
    int64_t value;
};
struct BalloonInfo {
    int64_t actual;
};
void qapi_free_BalloonInfo(BalloonInfo *obj);
typedef BalloonInfo *BalloonInfo_autoptr; typedef GList *BalloonInfo_listautoptr; typedef GSList *BalloonInfo_slistautoptr; typedef GQueue *BalloonInfo_queueautoptr;
struct q_obj_BALLOON_CHANGE_arg {
    int64_t actual;
};
struct HvBalloonInfo {
    uint64_t committed;
    uint64_t available;
};
void qapi_free_HvBalloonInfo(HvBalloonInfo *obj);
typedef HvBalloonInfo *HvBalloonInfo_autoptr; typedef GList *HvBalloonInfo_listautoptr; typedef GSList *HvBalloonInfo_slistautoptr; typedef GQueue *HvBalloonInfo_queueautoptr;
struct MemoryInfo {
    uint64_t base_memory;
    
   _Bool 
        has_plugged_memory;
    uint64_t plugged_memory;
};
void qapi_free_MemoryInfo(MemoryInfo *obj);
typedef MemoryInfo *MemoryInfo_autoptr; typedef GList *MemoryInfo_listautoptr; typedef GSList *MemoryInfo_slistautoptr; typedef GQueue *MemoryInfo_queueautoptr;
struct PCDIMMDeviceInfo {
    char *id;
    int64_t addr;
    int64_t size;
    int64_t slot;
    int64_t node;
    char *memdev;
    
   _Bool 
        hotplugged;
    
   _Bool 
        hotpluggable;
};
void qapi_free_PCDIMMDeviceInfo(PCDIMMDeviceInfo *obj);
typedef PCDIMMDeviceInfo *PCDIMMDeviceInfo_autoptr; typedef GList *PCDIMMDeviceInfo_listautoptr; typedef GSList *PCDIMMDeviceInfo_slistautoptr; typedef GQueue *PCDIMMDeviceInfo_queueautoptr;
struct VirtioPMEMDeviceInfo {
    char *id;
    uint64_t memaddr;
    uint64_t size;
    char *memdev;
};
void qapi_free_VirtioPMEMDeviceInfo(VirtioPMEMDeviceInfo *obj);
typedef VirtioPMEMDeviceInfo *VirtioPMEMDeviceInfo_autoptr; typedef GList *VirtioPMEMDeviceInfo_listautoptr; typedef GSList *VirtioPMEMDeviceInfo_slistautoptr; typedef GQueue *VirtioPMEMDeviceInfo_queueautoptr;
struct VirtioMEMDeviceInfo {
    char *id;
    uint64_t memaddr;
    uint64_t requested_size;
    uint64_t size;
    uint64_t max_size;
    uint64_t block_size;
    int64_t node;
    char *memdev;
};
void qapi_free_VirtioMEMDeviceInfo(VirtioMEMDeviceInfo *obj);
typedef VirtioMEMDeviceInfo *VirtioMEMDeviceInfo_autoptr; typedef GList *VirtioMEMDeviceInfo_listautoptr; typedef GSList *VirtioMEMDeviceInfo_slistautoptr; typedef GQueue *VirtioMEMDeviceInfo_queueautoptr;
struct SgxEPCDeviceInfo {
    char *id;
    uint64_t memaddr;
    uint64_t size;
    int64_t node;
    char *memdev;
};
void qapi_free_SgxEPCDeviceInfo(SgxEPCDeviceInfo *obj);
typedef SgxEPCDeviceInfo *SgxEPCDeviceInfo_autoptr; typedef GList *SgxEPCDeviceInfo_listautoptr; typedef GSList *SgxEPCDeviceInfo_slistautoptr; typedef GQueue *SgxEPCDeviceInfo_queueautoptr;
struct HvBalloonDeviceInfo {
    char *id;
    
   _Bool 
        has_memaddr;
    uint64_t memaddr;
    uint64_t max_size;
    char *memdev;
};
void qapi_free_HvBalloonDeviceInfo(HvBalloonDeviceInfo *obj);
typedef HvBalloonDeviceInfo *HvBalloonDeviceInfo_autoptr; typedef GList *HvBalloonDeviceInfo_listautoptr; typedef GSList *HvBalloonDeviceInfo_slistautoptr; typedef GQueue *HvBalloonDeviceInfo_queueautoptr;
struct PCDIMMDeviceInfoWrapper {
    PCDIMMDeviceInfo *data;
};
void qapi_free_PCDIMMDeviceInfoWrapper(PCDIMMDeviceInfoWrapper *obj);
typedef PCDIMMDeviceInfoWrapper *PCDIMMDeviceInfoWrapper_autoptr; typedef GList *PCDIMMDeviceInfoWrapper_listautoptr; typedef GSList *PCDIMMDeviceInfoWrapper_slistautoptr; typedef GQueue *PCDIMMDeviceInfoWrapper_queueautoptr;
struct VirtioPMEMDeviceInfoWrapper {
    VirtioPMEMDeviceInfo *data;
};
void qapi_free_VirtioPMEMDeviceInfoWrapper(VirtioPMEMDeviceInfoWrapper *obj);
typedef VirtioPMEMDeviceInfoWrapper *VirtioPMEMDeviceInfoWrapper_autoptr; typedef GList *VirtioPMEMDeviceInfoWrapper_listautoptr; typedef GSList *VirtioPMEMDeviceInfoWrapper_slistautoptr; typedef GQueue *VirtioPMEMDeviceInfoWrapper_queueautoptr;
struct VirtioMEMDeviceInfoWrapper {
    VirtioMEMDeviceInfo *data;
};
void qapi_free_VirtioMEMDeviceInfoWrapper(VirtioMEMDeviceInfoWrapper *obj);
typedef VirtioMEMDeviceInfoWrapper *VirtioMEMDeviceInfoWrapper_autoptr; typedef GList *VirtioMEMDeviceInfoWrapper_listautoptr; typedef GSList *VirtioMEMDeviceInfoWrapper_slistautoptr; typedef GQueue *VirtioMEMDeviceInfoWrapper_queueautoptr;
struct SgxEPCDeviceInfoWrapper {
    SgxEPCDeviceInfo *data;
};
void qapi_free_SgxEPCDeviceInfoWrapper(SgxEPCDeviceInfoWrapper *obj);
typedef SgxEPCDeviceInfoWrapper *SgxEPCDeviceInfoWrapper_autoptr; typedef GList *SgxEPCDeviceInfoWrapper_listautoptr; typedef GSList *SgxEPCDeviceInfoWrapper_slistautoptr; typedef GQueue *SgxEPCDeviceInfoWrapper_queueautoptr;
struct HvBalloonDeviceInfoWrapper {
    HvBalloonDeviceInfo *data;
};
void qapi_free_HvBalloonDeviceInfoWrapper(HvBalloonDeviceInfoWrapper *obj);
typedef HvBalloonDeviceInfoWrapper *HvBalloonDeviceInfoWrapper_autoptr; typedef GList *HvBalloonDeviceInfoWrapper_listautoptr; typedef GSList *HvBalloonDeviceInfoWrapper_slistautoptr; typedef GQueue *HvBalloonDeviceInfoWrapper_queueautoptr;
struct q_obj_MemoryDeviceInfo_base {
    MemoryDeviceInfoKind type;
};
struct MemoryDeviceInfo {
    MemoryDeviceInfoKind type;
    union {
        PCDIMMDeviceInfoWrapper dimm;
        PCDIMMDeviceInfoWrapper nvdimm;
        VirtioPMEMDeviceInfoWrapper virtio_pmem;
        VirtioMEMDeviceInfoWrapper virtio_mem;
        SgxEPCDeviceInfoWrapper sgx_epc;
        HvBalloonDeviceInfoWrapper hv_balloon;
    } u;
};
void qapi_free_MemoryDeviceInfo(MemoryDeviceInfo *obj);
typedef MemoryDeviceInfo *MemoryDeviceInfo_autoptr; typedef GList *MemoryDeviceInfo_listautoptr; typedef GSList *MemoryDeviceInfo_slistautoptr; typedef GQueue *MemoryDeviceInfo_queueautoptr;
struct SgxEPC {
    char *memdev;
    int64_t node;
};
void qapi_free_SgxEPC(SgxEPC *obj);
typedef SgxEPC *SgxEPC_autoptr; typedef GList *SgxEPC_listautoptr; typedef GSList *SgxEPC_slistautoptr; typedef GQueue *SgxEPC_queueautoptr;
struct SgxEPCList {
    SgxEPCList *next;
    SgxEPC *value;
};
void qapi_free_SgxEPCList(SgxEPCList *obj);
typedef SgxEPCList *SgxEPCList_autoptr; typedef GList *SgxEPCList_listautoptr; typedef GSList *SgxEPCList_slistautoptr; typedef GQueue *SgxEPCList_queueautoptr;
struct SgxEPCProperties {
    SgxEPCList *sgx_epc;
};
void qapi_free_SgxEPCProperties(SgxEPCProperties *obj);
typedef SgxEPCProperties *SgxEPCProperties_autoptr; typedef GList *SgxEPCProperties_listautoptr; typedef GSList *SgxEPCProperties_slistautoptr; typedef GQueue *SgxEPCProperties_queueautoptr;
struct MemoryDeviceInfoList {
    MemoryDeviceInfoList *next;
    MemoryDeviceInfo *value;
};
void qapi_free_MemoryDeviceInfoList(MemoryDeviceInfoList *obj);
typedef MemoryDeviceInfoList *MemoryDeviceInfoList_autoptr; typedef GList *MemoryDeviceInfoList_listautoptr; typedef GSList *MemoryDeviceInfoList_slistautoptr; typedef GQueue *MemoryDeviceInfoList_queueautoptr;
struct q_obj_MEMORY_DEVICE_SIZE_CHANGE_arg {
    char *id;
    uint64_t size;
    char *qom_path;
};
struct BootConfiguration {
    char *order;
    char *once;
    
   _Bool 
        has_menu;
    
   _Bool 
        menu;
    char *splash;
    
   _Bool 
        has_splash_time;
    int64_t splash_time;
    
   _Bool 
        has_reboot_timeout;
    int64_t reboot_timeout;
    
   _Bool 
        has_strict;
    
   _Bool 
        strict;
};
void qapi_free_BootConfiguration(BootConfiguration *obj);
typedef BootConfiguration *BootConfiguration_autoptr; typedef GList *BootConfiguration_listautoptr; typedef GSList *BootConfiguration_slistautoptr; typedef GQueue *BootConfiguration_queueautoptr;
struct SMPConfiguration {
    
   _Bool 
        has_cpus;
    int64_t cpus;
    
   _Bool 
        has_drawers;
    int64_t drawers;
    
   _Bool 
        has_books;
    int64_t books;
    
   _Bool 
        has_sockets;
    int64_t sockets;
    
   _Bool 
        has_dies;
    int64_t dies;
    
   _Bool 
        has_clusters;
    int64_t clusters;
    
   _Bool 
        has_modules;
    int64_t modules;
    
   _Bool 
        has_cores;
    int64_t cores;
    
   _Bool 
        has_threads;
    int64_t threads;
    
   _Bool 
        has_maxcpus;
    int64_t maxcpus;
};
void qapi_free_SMPConfiguration(SMPConfiguration *obj);
typedef SMPConfiguration *SMPConfiguration_autoptr; typedef GList *SMPConfiguration_listautoptr; typedef GSList *SMPConfiguration_slistautoptr; typedef GQueue *SMPConfiguration_queueautoptr;
struct MemorySizeConfiguration {
    
   _Bool 
        has_size;
    uint64_t size;
    
   _Bool 
        has_max_size;
    uint64_t max_size;
    
   _Bool 
        has_slots;
    uint64_t slots;
};
void qapi_free_MemorySizeConfiguration(MemorySizeConfiguration *obj);
typedef MemorySizeConfiguration *MemorySizeConfiguration_autoptr; typedef GList *MemorySizeConfiguration_listautoptr; typedef GSList *MemorySizeConfiguration_slistautoptr; typedef GQueue *MemorySizeConfiguration_queueautoptr;
struct q_obj_dumpdtb_arg {
    char *filename;
};
typedef enum RunState {
    RUN_STATE_DEBUG,
    RUN_STATE_INMIGRATE,
    RUN_STATE_INTERNAL_ERROR,
    RUN_STATE_IO_ERROR,
    RUN_STATE_PAUSED,
    RUN_STATE_POSTMIGRATE,
    RUN_STATE_PRELAUNCH,
    RUN_STATE_FINISH_MIGRATE,
    RUN_STATE_RESTORE_VM,
    RUN_STATE_RUNNING,
    RUN_STATE_SAVE_VM,
    RUN_STATE_SHUTDOWN,
    RUN_STATE_SUSPENDED,
    RUN_STATE_WATCHDOG,
    RUN_STATE_GUEST_PANICKED,
    RUN_STATE_COLO,
    RUN_STATE__MAX,
} RunState;
extern const QEnumLookup RunState_lookup;
typedef enum ShutdownCause {
    SHUTDOWN_CAUSE_NONE,
    SHUTDOWN_CAUSE_HOST_ERROR,
    SHUTDOWN_CAUSE_HOST_QMP_QUIT,
    SHUTDOWN_CAUSE_HOST_QMP_SYSTEM_RESET,
    SHUTDOWN_CAUSE_HOST_SIGNAL,
    SHUTDOWN_CAUSE_HOST_UI,
    SHUTDOWN_CAUSE_GUEST_SHUTDOWN,
    SHUTDOWN_CAUSE_GUEST_RESET,
    SHUTDOWN_CAUSE_GUEST_PANIC,
    SHUTDOWN_CAUSE_SUBSYSTEM_RESET,
    SHUTDOWN_CAUSE_SNAPSHOT_LOAD,
    SHUTDOWN_CAUSE__MAX,
} ShutdownCause;
extern const QEnumLookup ShutdownCause_lookup;
typedef struct StatusInfo StatusInfo;
typedef struct q_obj_SHUTDOWN_arg q_obj_SHUTDOWN_arg;
typedef struct q_obj_RESET_arg q_obj_RESET_arg;
typedef struct q_obj_WATCHDOG_arg q_obj_WATCHDOG_arg;
typedef enum WatchdogAction {
    WATCHDOG_ACTION_RESET,
    WATCHDOG_ACTION_SHUTDOWN,
    WATCHDOG_ACTION_POWEROFF,
    WATCHDOG_ACTION_PAUSE,
    WATCHDOG_ACTION_DEBUG,
    WATCHDOG_ACTION_NONE,
    WATCHDOG_ACTION_INJECT_NMI,
    WATCHDOG_ACTION__MAX,
} WatchdogAction;
extern const QEnumLookup WatchdogAction_lookup;
typedef enum RebootAction {
    REBOOT_ACTION_RESET,
    REBOOT_ACTION_SHUTDOWN,
    REBOOT_ACTION__MAX,
} RebootAction;
extern const QEnumLookup RebootAction_lookup;
typedef enum ShutdownAction {
    SHUTDOWN_ACTION_POWEROFF,
    SHUTDOWN_ACTION_PAUSE,
    SHUTDOWN_ACTION__MAX,
} ShutdownAction;
extern const QEnumLookup ShutdownAction_lookup;
typedef enum PanicAction {
    PANIC_ACTION_PAUSE,
    PANIC_ACTION_SHUTDOWN,
    PANIC_ACTION_EXIT_FAILURE,
    PANIC_ACTION_NONE,
    PANIC_ACTION__MAX,
} PanicAction;
extern const QEnumLookup PanicAction_lookup;
typedef struct q_obj_watchdog_set_action_arg q_obj_watchdog_set_action_arg;
typedef struct q_obj_set_action_arg q_obj_set_action_arg;
typedef struct q_obj_GUEST_PANICKED_arg q_obj_GUEST_PANICKED_arg;
typedef struct q_obj_GUEST_CRASHLOADED_arg q_obj_GUEST_CRASHLOADED_arg;
typedef enum GuestPanicAction {
    GUEST_PANIC_ACTION_PAUSE,
    GUEST_PANIC_ACTION_POWEROFF,
    GUEST_PANIC_ACTION_RUN,
    GUEST_PANIC_ACTION__MAX,
} GuestPanicAction;
extern const QEnumLookup GuestPanicAction_lookup;
typedef enum GuestPanicInformationType {
    GUEST_PANIC_INFORMATION_TYPE_HYPER_V,
    GUEST_PANIC_INFORMATION_TYPE_S390,
    GUEST_PANIC_INFORMATION_TYPE__MAX,
} GuestPanicInformationType;
extern const QEnumLookup GuestPanicInformationType_lookup;
typedef struct q_obj_GuestPanicInformation_base q_obj_GuestPanicInformation_base;
typedef struct GuestPanicInformation GuestPanicInformation;
typedef struct GuestPanicInformationHyperV GuestPanicInformationHyperV;
typedef enum S390CrashReason {
    S390_CRASH_REASON_UNKNOWN,
    S390_CRASH_REASON_DISABLED_WAIT,
    S390_CRASH_REASON_EXTINT_LOOP,
    S390_CRASH_REASON_PGMINT_LOOP,
    S390_CRASH_REASON_OPINT_LOOP,
    S390_CRASH_REASON__MAX,
} S390CrashReason;
extern const QEnumLookup S390CrashReason_lookup;
typedef struct GuestPanicInformationS390 GuestPanicInformationS390;
typedef struct q_obj_MEMORY_FAILURE_arg q_obj_MEMORY_FAILURE_arg;
typedef enum MemoryFailureRecipient {
    MEMORY_FAILURE_RECIPIENT_HYPERVISOR,
    MEMORY_FAILURE_RECIPIENT_GUEST,
    MEMORY_FAILURE_RECIPIENT__MAX,
} MemoryFailureRecipient;
extern const QEnumLookup MemoryFailureRecipient_lookup;
typedef enum MemoryFailureAction {
    MEMORY_FAILURE_ACTION_IGNORE,
    MEMORY_FAILURE_ACTION_INJECT,
    MEMORY_FAILURE_ACTION_FATAL,
    MEMORY_FAILURE_ACTION_RESET,
    MEMORY_FAILURE_ACTION__MAX,
} MemoryFailureAction;
extern const QEnumLookup MemoryFailureAction_lookup;
typedef struct MemoryFailureFlags MemoryFailureFlags;
typedef enum NotifyVmexitOption {
    NOTIFY_VMEXIT_OPTION_RUN,
    NOTIFY_VMEXIT_OPTION_INTERNAL_ERROR,
    NOTIFY_VMEXIT_OPTION_DISABLE,
    NOTIFY_VMEXIT_OPTION__MAX,
} NotifyVmexitOption;
extern const QEnumLookup NotifyVmexitOption_lookup;
struct StatusInfo {
    
   _Bool 
        running;
    RunState status;
};
void qapi_free_StatusInfo(StatusInfo *obj);
typedef StatusInfo *StatusInfo_autoptr; typedef GList *StatusInfo_listautoptr; typedef GSList *StatusInfo_slistautoptr; typedef GQueue *StatusInfo_queueautoptr;
struct q_obj_SHUTDOWN_arg {
    
   _Bool 
        guest;
    ShutdownCause reason;
};
struct q_obj_RESET_arg {
    
   _Bool 
        guest;
    ShutdownCause reason;
};
struct q_obj_WATCHDOG_arg {
    WatchdogAction action;
};
struct q_obj_watchdog_set_action_arg {
    WatchdogAction action;
};
struct q_obj_set_action_arg {
    
   _Bool 
        has_reboot;
    RebootAction reboot;
    
   _Bool 
        has_shutdown;
    ShutdownAction shutdown;
    
   _Bool 
        has_panic;
    PanicAction panic;
    
   _Bool 
        has_watchdog;
    WatchdogAction watchdog;
};
struct q_obj_GUEST_PANICKED_arg {
    GuestPanicAction action;
    GuestPanicInformation *info;
};
struct q_obj_GUEST_CRASHLOADED_arg {
    GuestPanicAction action;
    GuestPanicInformation *info;
};
struct q_obj_GuestPanicInformation_base {
    GuestPanicInformationType type;
};
struct GuestPanicInformationHyperV {
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;
};
struct GuestPanicInformationS390 {
    uint32_t core;
    uint64_t psw_mask;
    uint64_t psw_addr;
    S390CrashReason reason;
};
struct GuestPanicInformation {
    GuestPanicInformationType type;
    union {
        GuestPanicInformationHyperV hyper_v;
        GuestPanicInformationS390 s390;
    } u;
};
void qapi_free_GuestPanicInformation(GuestPanicInformation *obj);
typedef GuestPanicInformation *GuestPanicInformation_autoptr; typedef GList *GuestPanicInformation_listautoptr; typedef GSList *GuestPanicInformation_slistautoptr; typedef GQueue *GuestPanicInformation_queueautoptr;
void qapi_free_GuestPanicInformationHyperV(GuestPanicInformationHyperV *obj);
typedef GuestPanicInformationHyperV *GuestPanicInformationHyperV_autoptr; typedef GList *GuestPanicInformationHyperV_listautoptr; typedef GSList *GuestPanicInformationHyperV_slistautoptr; typedef GQueue *GuestPanicInformationHyperV_queueautoptr;
void qapi_free_GuestPanicInformationS390(GuestPanicInformationS390 *obj);
typedef GuestPanicInformationS390 *GuestPanicInformationS390_autoptr; typedef GList *GuestPanicInformationS390_listautoptr; typedef GSList *GuestPanicInformationS390_slistautoptr; typedef GQueue *GuestPanicInformationS390_queueautoptr;
struct q_obj_MEMORY_FAILURE_arg {
    MemoryFailureRecipient recipient;
    MemoryFailureAction action;
    MemoryFailureFlags *flags;
};
struct MemoryFailureFlags {
    
   _Bool 
        action_required;
    
   _Bool 
        recursive;
};
void qapi_free_MemoryFailureFlags(MemoryFailureFlags *obj);
typedef MemoryFailureFlags *MemoryFailureFlags_autoptr; typedef GList *MemoryFailureFlags_listautoptr; typedef GSList *MemoryFailureFlags_slistautoptr; typedef GQueue *MemoryFailureFlags_queueautoptr;
typedef struct QemuLockCnt QemuLockCnt;
struct QemuLockCnt {
    unsigned count;
};
void qemu_lockcnt_init(QemuLockCnt *lockcnt);
void qemu_lockcnt_destroy(QemuLockCnt *lockcnt);
void qemu_lockcnt_inc(QemuLockCnt *lockcnt);
void qemu_lockcnt_dec(QemuLockCnt *lockcnt);
_Bool 
    qemu_lockcnt_dec_and_lock(QemuLockCnt *lockcnt);
_Bool 
    qemu_lockcnt_dec_if_lock(QemuLockCnt *lockcnt);
void qemu_lockcnt_lock(QemuLockCnt *lockcnt);
void qemu_lockcnt_unlock(QemuLockCnt *lockcnt);
void qemu_lockcnt_inc_and_unlock(QemuLockCnt *lockcnt);
unsigned qemu_lockcnt_count(QemuLockCnt *lockcnt);
typedef int (*WriteCoreDumpFunction)(const void *buf, size_t size,
                                     void *opaque);
typedef struct CPUClass CPUClass;
typedef struct CPUWatchpoint CPUWatchpoint;
struct CPUAddressSpace;
struct CPUJumpCache;
struct AccelCPUClass;
struct SysemuCPUOps;
struct CPUClass {
    DeviceClass parent_class;
    ObjectClass *(*class_by_name)(const char *cpu_model);
    void (*parse_features)(const char *typename, char *str, Error **errp);
    
   _Bool 
        (*has_work)(CPUState *cpu);
    int (*mmu_index)(CPUState *cpu, 
                                   _Bool 
                                        ifetch);
    int (*memory_rw_debug)(CPUState *cpu, vaddr addr,
                           uint8_t *buf, int len, 
                                                 _Bool 
                                                      is_write);
    void (*dump_state)(CPUState *cpu, FILE *, int flags);
    void (*query_cpu_fast)(CPUState *cpu, CpuInfoFast *value);
    int64_t (*get_arch_id)(CPUState *cpu);
    void (*set_pc)(CPUState *cpu, vaddr value);
    vaddr (*get_pc)(CPUState *cpu);
    int (*gdb_read_register)(CPUState *cpu, GByteArray *buf, int reg);
    int (*gdb_write_register)(CPUState *cpu, uint8_t *buf, int reg);
    vaddr (*gdb_adjust_breakpoint)(CPUState *cpu, vaddr addr);
    const char *gdb_core_xml_file;
    const gchar * (*gdb_arch_name)(CPUState *cpu);
    void (*disas_set_info)(CPUState *cpu, disassemble_info *info);
    const char *deprecation_note;
    struct AccelCPUClass *accel_cpu;
    const struct SysemuCPUOps *sysemu_ops;
    const TCGCPUOps *tcg_ops;
    void (*init_accel_cpu)(struct AccelCPUClass *accel_cpu, CPUClass *cc);
    int reset_dump_flags;
    int gdb_num_core_regs;
    
   _Bool 
        gdb_stop_before_watchpoint;
};
struct CPUTLBEntryFull {
    hwaddr xlat_section;
    hwaddr phys_addr;
    MemTxAttrs attrs;
    uint8_t prot;
    uint8_t lg_page_size;
    uint8_t tlb_fill_flags;
    uint8_t slow_flags[3];
    union {
        struct {
            uint8_t pte_attrs;
            uint8_t shareability;
            
           _Bool 
                guarded;
        } arm;
    } extra;
};
typedef struct CPUTLBDesc {
    vaddr large_page_addr;
    vaddr large_page_mask;
    int64_t window_begin_ns;
    size_t window_max_entries;
    size_t n_used_entries;
    size_t vindex;
    CPUTLBEntry vtable[8];
    CPUTLBEntryFull vfulltlb[8];
    CPUTLBEntryFull *fulltlb;
} CPUTLBDesc;
typedef struct CPUTLBCommon {
    QemuSpin lock;
    uint16_t dirty;
    size_t full_flush_count;
    size_t part_flush_count;
    size_t elide_flush_count;
} CPUTLBCommon;
typedef struct CPUTLB {
    CPUTLBCommon c;
    CPUTLBDesc d[16];
    CPUTLBDescFast f[16];
} CPUTLB;
typedef union IcountDecr {
    uint32_t u32;
    struct {
        uint16_t low;
        uint16_t high;
    } u16;
} IcountDecr;
typedef struct CPUNegativeOffsetState {
    CPUTLB tlb;
    GArray *plugin_mem_cbs;
    uint64_t plugin_mem_value_low;
    uint64_t plugin_mem_value_high;
    IcountDecr icount_decr;
    
   _Bool 
        can_do_io;
} CPUNegativeOffsetState;
struct KVMState;
struct kvm_run;
typedef union {
    int host_int;
    unsigned long host_ulong;
    void *host_ptr;
    vaddr target_ptr;
} run_on_cpu_data;
typedef void (*run_on_cpu_func)(CPUState *cpu, run_on_cpu_data data);
struct qemu_work_item;
struct CPUState {
    DeviceState parent_obj;
    CPUClass *cc;
    int nr_threads;
    struct QemuThread *thread;
    int thread_id;
    
   _Bool 
        running, has_waiter;
    struct QemuCond *halt_cond;
    
   _Bool 
        thread_kicked;
    
   _Bool 
        created;
    
   _Bool 
        stop;
    
   _Bool 
        stopped;
    
   _Bool 
        start_powered_off;
    
   _Bool 
        unplug;
    
   _Bool 
        crash_occurred;
    
   _Bool 
        exit_request;
    int exclusive_context_count;
    uint32_t cflags_next_tb;
    uint32_t interrupt_request;
    int singlestep_enabled;
    int64_t icount_budget;
    int64_t icount_extra;
    uint64_t random_seed;
    sigjmp_buf jmp_env;
    QemuMutex work_mutex;
    struct { struct qemu_work_item *sqh_first; struct qemu_work_item **sqh_last; } work_list;
    struct CPUAddressSpace *cpu_ases;
    int cpu_ases_count;
    int num_ases;
    AddressSpace *as;
    MemoryRegion *memory;
    struct CPUJumpCache *tb_jmp_cache;
    GArray *gdb_regs;
    int gdb_num_regs;
    int gdb_num_g_regs;
    union { struct CPUState *tqe_next; QTailQLink tqe_circ; } node;
    union { struct CPUBreakpoint *tqh_first; QTailQLink tqh_circ; } breakpoints;
    union { struct CPUWatchpoint *tqh_first; QTailQLink tqh_circ; } watchpoints;
    CPUWatchpoint *watchpoint_hit;
    void *opaque;
    uintptr_t mem_io_pc;
    int kvm_fd;
    struct KVMState *kvm_state;
    struct kvm_run *kvm_run;
    struct kvm_dirty_gfn *kvm_dirty_gfns;
    uint32_t kvm_fetch_index;
    uint64_t dirty_pages;
    int kvm_vcpu_stats_fd;
    
   _Bool 
        vcpu_dirty;
    QemuLockCnt in_ioctl_lock;
    CPUPluginState *plugin_state;
    int cpu_index;
    int cluster_index;
    uint32_t tcg_cflags;
    uint32_t halted;
    int32_t exception_index;
    AccelCPUState *accel;
    
   _Bool 
        throttle_thread_scheduled;
    int64_t throttle_us_per_full;
    
   _Bool 
        ignore_memory_transaction_failures;
    
   _Bool 
        prctl_unalign_sigbus;
    GArray *iommu_notifiers;
    char neg_align[0] ;
    CPUNegativeOffsetState neg;
};
                                                                    ;
typedef union CPUTailQ { struct CPUState *tqh_first; QTailQLink tqh_circ; } CPUTailQ;
extern CPUTailQ cpus_queue;
extern CPUState *current_cpu;
extern 
      _Bool 
           mttcg_enabled;
_Bool 
    cpu_paging_enabled(const CPUState *cpu);
_Bool 
    cpu_get_memory_mapping(CPUState *cpu, MemoryMappingList *list,
                            Error **errp);
int cpu_write_elf64_note(WriteCoreDumpFunction f, CPUState *cpu,
                         int cpuid, void *opaque);
int cpu_write_elf64_qemunote(WriteCoreDumpFunction f, CPUState *cpu,
                             void *opaque);
int cpu_write_elf32_note(WriteCoreDumpFunction f, CPUState *cpu,
                         int cpuid, void *opaque);
int cpu_write_elf32_qemunote(WriteCoreDumpFunction f, CPUState *cpu,
                             void *opaque);
GuestPanicInformation *cpu_get_crash_info(CPUState *cpu);
enum CPUDumpFlags {
    CPU_DUMP_CODE = 0x00010000,
    CPU_DUMP_FPU = 0x00020000,
    CPU_DUMP_CCOP = 0x00040000,
    CPU_DUMP_VPU = 0x00080000,
};
void cpu_dump_state(CPUState *cpu, FILE *f, int flags);
hwaddr cpu_get_phys_page_attrs_debug(CPUState *cpu, vaddr addr,
                                     MemTxAttrs *attrs);
hwaddr cpu_get_phys_page_debug(CPUState *cpu, vaddr addr);
int cpu_asidx_from_attrs(CPUState *cpu, MemTxAttrs attrs);
_Bool 
    cpu_virtio_is_big_endian(CPUState *cpu);
void cpu_list_add(CPUState *cpu);
void cpu_list_remove(CPUState *cpu);
void cpu_reset(CPUState *cpu);
ObjectClass *cpu_class_by_name(const char *typename, const char *cpu_model);
char *cpu_model_from_type(const char *typename);
CPUState *cpu_create(const char *typename);
const char *parse_cpu_option(const char *cpu_option);
_Bool 
    qemu_cpu_is_self(CPUState *cpu);
void qemu_cpu_kick(CPUState *cpu);
_Bool 
    cpu_is_stopped(CPUState *cpu);
void do_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data,
                   QemuMutex *mutex);
void run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data);
void async_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data);
void async_safe_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data);
CPUState *qemu_get_cpu(int index);
_Bool 
    cpu_exists(int64_t id);
CPUState *cpu_by_arch_id(int64_t id);
void cpu_interrupt(CPUState *cpu, int mask);
void cpu_reset_interrupt(CPUState *cpu, int mask);
void cpu_exit(CPUState *cpu);
void cpu_pause(CPUState *cpu);
void cpu_resume(CPUState *cpu);
void cpu_remove_sync(CPUState *cpu);
void free_queued_cpu_work(CPUState *cpu);
void process_queued_cpu_work(CPUState *cpu);
void cpu_exec_start(CPUState *cpu);
void cpu_exec_end(CPUState *cpu);
void start_exclusive(void);
void end_exclusive(void);
void qemu_init_vcpu(CPUState *cpu);
void cpu_single_step(CPUState *cpu, int enabled);
int cpu_breakpoint_insert(CPUState *cpu, vaddr pc, int flags,
                          CPUBreakpoint **breakpoint);
int cpu_breakpoint_remove(CPUState *cpu, vaddr pc, int flags);
void cpu_breakpoint_remove_by_ref(CPUState *cpu, CPUBreakpoint *breakpoint);
void cpu_breakpoint_remove_all(CPUState *cpu, int mask);
int cpu_watchpoint_insert(CPUState *cpu, vaddr addr, vaddr len,
                          int flags, CPUWatchpoint **watchpoint);
int cpu_watchpoint_remove(CPUState *cpu, vaddr addr,
                          vaddr len, int flags);
void cpu_watchpoint_remove_by_ref(CPUState *cpu, CPUWatchpoint *watchpoint);
void cpu_watchpoint_remove_all(CPUState *cpu, int mask);
AddressSpace *cpu_get_address_space(CPUState *cpu, int asidx);
 void cpu_abort(CPUState *cpu, const char *fmt, ...)
    ;
void cpu_class_init_props(DeviceClass *dc);
void cpu_exec_initfn(CPUState *cpu);
_Bool 
    cpu_exec_realizefn(CPUState *cpu, Error **errp);
void cpu_exec_unrealizefn(CPUState *cpu);
void cpu_exec_reset_hold(CPUState *cpu);
const char *target_name(void);
extern const VMStateDescription vmstate_cpu_common;
void cpu_exec_init_all(void);
void cpu_exec_step_atomic(CPUState *cpu);
extern QemuMutex qemu_cpu_list_lock;
void qemu_init_cpu_list(void);
void cpu_list_lock(void);
void cpu_list_unlock(void);
unsigned int cpu_list_generation_id_get(void);
int cpu_get_free_index(void);
void tcg_iommu_init_notifier_list(CPUState *cpu);
void tcg_iommu_free_notifier_list(CPUState *cpu);
enum device_endian {
    DEVICE_NATIVE_ENDIAN,
    DEVICE_BIG_ENDIAN,
    DEVICE_LITTLE_ENDIAN,
};
typedef uint64_t ram_addr_t;
void qemu_ram_remap(ram_addr_t addr, ram_addr_t length);
ram_addr_t qemu_ram_addr_from_host(void *ptr);
ram_addr_t qemu_ram_addr_from_host_nofail(void *ptr);
RAMBlock *qemu_ram_block_by_name(const char *name);
RAMBlock *qemu_ram_block_from_host(void *ptr, 
                                             _Bool 
                                                  round_offset,
                                   ram_addr_t *offset);
ram_addr_t qemu_ram_block_host_offset(RAMBlock *rb, void *host);
void qemu_ram_set_idstr(RAMBlock *block, const char *name, DeviceState *dev);
void qemu_ram_unset_idstr(RAMBlock *block);
const char *qemu_ram_get_idstr(RAMBlock *rb);
void *qemu_ram_get_host_addr(RAMBlock *rb);
ram_addr_t qemu_ram_get_offset(RAMBlock *rb);
ram_addr_t qemu_ram_get_used_length(RAMBlock *rb);
ram_addr_t qemu_ram_get_max_length(RAMBlock *rb);
_Bool 
    qemu_ram_is_shared(RAMBlock *rb);
_Bool 
    qemu_ram_is_noreserve(RAMBlock *rb);
_Bool 
    qemu_ram_is_uf_zeroable(RAMBlock *rb);
void qemu_ram_set_uf_zeroable(RAMBlock *rb);
_Bool 
    qemu_ram_is_migratable(RAMBlock *rb);
void qemu_ram_set_migratable(RAMBlock *rb);
void qemu_ram_unset_migratable(RAMBlock *rb);
_Bool 
    qemu_ram_is_named_file(RAMBlock *rb);
int qemu_ram_get_fd(RAMBlock *rb);
size_t qemu_ram_pagesize(RAMBlock *block);
size_t qemu_ram_pagesize_largest(void);
void cpu_address_space_init(CPUState *cpu, int asidx,
                            const char *prefix, MemoryRegion *mr);
void cpu_address_space_destroy(CPUState *cpu, int asidx);
void cpu_physical_memory_rw(hwaddr addr, void *buf,
                            hwaddr len, 
                                       _Bool 
                                            is_write);
void *cpu_physical_memory_map(hwaddr addr,
                              hwaddr *plen,
                              
                             _Bool 
                                  is_write);
void cpu_physical_memory_unmap(void *buffer, hwaddr len,
                               
                              _Bool 
                                   is_write, hwaddr access_len);
_Bool 
    cpu_physical_memory_is_io(hwaddr phys_addr);
void qemu_flush_coalesced_mmio_buffer(void);
void cpu_flush_icache_range(hwaddr start, hwaddr len);
typedef int (RAMBlockIterFunc)(RAMBlock *rb, void *opaque);
int qemu_ram_foreach_block(RAMBlockIterFunc func, void *opaque);
int ram_block_discard_range(RAMBlock *rb, uint64_t start, size_t length);
int ram_block_discard_guest_memfd_range(RAMBlock *rb, uint64_t start,
                                        size_t length);
int cpu_memory_rw_debug(CPUState *cpu, vaddr addr,
                        void *ptr, size_t len, 
                                              _Bool 
                                                   is_write);
void list_cpus(void);
_Bool 
    cpu_unwind_state_data(CPUState *cpu, uintptr_t host_pc, uint64_t *data);
_Bool 
    cpu_restore_state(CPUState *cpu, uintptr_t host_pc);
 void cpu_loop_exit_noexc(CPUState *cpu);
 void cpu_loop_exit_atomic(CPUState *cpu, uintptr_t pc);
 void cpu_loop_exit(CPUState *cpu);
 void cpu_loop_exit_restore(CPUState *cpu, uintptr_t pc);
int cpu_exec(CPUState *cpu);
typedef int32_t target_long;
typedef uint32_t target_ulong;
_Bool 
    runstate_check(RunState state);
void runstate_set(RunState new_state);
RunState runstate_get(void);
_Bool 
    runstate_is_running(void);
_Bool 
    runstate_needs_reset(void);
void runstate_replay_enable(void);
typedef void VMChangeStateHandler(void *opaque, 
                                               _Bool 
                                                    running, RunState state);
VMChangeStateEntry *qemu_add_vm_change_state_handler(VMChangeStateHandler *cb,
                                                     void *opaque);
VMChangeStateEntry *qemu_add_vm_change_state_handler_prio(
        VMChangeStateHandler *cb, void *opaque, int priority);
VMChangeStateEntry *
qemu_add_vm_change_state_handler_prio_full(VMChangeStateHandler *cb,
                                           VMChangeStateHandler *prepare_cb,
                                           void *opaque, int priority);
VMChangeStateEntry *qdev_add_vm_change_state_handler(DeviceState *dev,
                                                     VMChangeStateHandler *cb,
                                                     void *opaque);
VMChangeStateEntry *qdev_add_vm_change_state_handler_full(
    DeviceState *dev, VMChangeStateHandler *cb,
    VMChangeStateHandler *prepare_cb, void *opaque);
void qemu_del_vm_change_state_handler(VMChangeStateEntry *e);
void vm_state_notify(
                    _Bool 
                         running, RunState state);
void vm_start(void);
int vm_prepare_start(
                    _Bool 
                         step_pending);
void vm_resume(RunState state);
int vm_stop(RunState state);
int vm_stop_force_state(RunState state);
int vm_shutdown(void);
void vm_set_suspended(
                     _Bool 
                          suspended);
_Bool 
    vm_get_suspended(void);
typedef enum WakeupReason {
    QEMU_WAKEUP_REASON_NONE = 0,
    QEMU_WAKEUP_REASON_RTC,
    QEMU_WAKEUP_REASON_PMTIMER,
    QEMU_WAKEUP_REASON_OTHER,
} WakeupReason;
void qemu_system_reset_request(ShutdownCause reason);
void qemu_system_suspend_request(void);
void qemu_register_suspend_notifier(Notifier *notifier);
_Bool 
    qemu_wakeup_suspend_enabled(void);
void qemu_system_wakeup_request(WakeupReason reason, Error **errp);
void qemu_system_wakeup_enable(WakeupReason reason, 
                                                   _Bool 
                                                        enabled);
void qemu_register_wakeup_notifier(Notifier *notifier);
void qemu_register_wakeup_support(void);
void qemu_system_shutdown_request_with_code(ShutdownCause reason,
                                            int exit_code);
void qemu_system_shutdown_request(ShutdownCause reason);
void qemu_system_powerdown_request(void);
void qemu_register_powerdown_notifier(Notifier *notifier);
void qemu_register_shutdown_notifier(Notifier *notifier);
void qemu_system_debug_request(void);
void qemu_system_vmstop_request(RunState reason);
void qemu_system_vmstop_request_prepare(void);
_Bool 
    qemu_vmstop_requested(RunState *r);
ShutdownCause qemu_shutdown_requested_get(void);
ShutdownCause qemu_reset_requested_get(void);
void qemu_system_killed(int signal, pid_t pid);
void qemu_system_reset(ShutdownCause reason);
void qemu_system_guest_panicked(GuestPanicInformation *info);
void qemu_system_guest_crashloaded(GuestPanicInformation *info);
void qemu_system_guest_pvshutdown(void);
_Bool 
    qemu_system_dump_in_progress(void);
enum {
    QEMU_ARCH_ALL = -1,
    QEMU_ARCH_ALPHA = 1,
    QEMU_ARCH_ARM = 2,
    QEMU_ARCH_I386 = 8,
    QEMU_ARCH_M68K = 16,
    QEMU_ARCH_MICROBLAZE = 64,
    QEMU_ARCH_MIPS = 128,
    QEMU_ARCH_PPC = 256,
    QEMU_ARCH_S390X = 512,
    QEMU_ARCH_SH4 = 1024,
    QEMU_ARCH_SPARC = 2048,
    QEMU_ARCH_XTENSA = 4096,
    QEMU_ARCH_OPENRISC = 8192,
    QEMU_ARCH_TRICORE = 65536,
    QEMU_ARCH_HPPA = 262144,
    QEMU_ARCH_RISCV = 524288,
    QEMU_ARCH_RX = 1048576,
    QEMU_ARCH_AVR = 2097152,
    QEMU_ARCH_HEXAGON = 4194304,
    QEMU_ARCH_LOONGARCH = 8388608,
};
extern const uint32_t arch_type;
void qemu_init_arch_modules(void);
typedef void QEMUConfigCB(const char *group, QDict *qdict, void *opaque, Error **errp);
void qemu_load_module_for_opts(const char *group);
QemuOptsList *qemu_find_opts(const char *group);
QemuOptsList *qemu_find_opts_err(const char *group, Error **errp);
QemuOpts *qemu_find_opts_singleton(const char *group);
extern QemuOptsList *vm_config_groups[];
extern QemuOptsList *drive_config_groups[];
void qemu_add_opts(QemuOptsList *list);
void qemu_add_drive_opts(QemuOptsList *list);
int qemu_global_option(const char *str);
int qemu_config_parse(FILE *fp, QemuOptsList **lists, const char *fname,
                      Error **errp);
void qemu_config_do_parse(const char *group, QDict *qdict, void *opaque, Error **errp);
int qemu_read_config_file(const char *filename, QEMUConfigCB *f, Error **errp);
_Bool 
    qemu_config_parse_qdict(QDict *options, QemuOptsList **lists,
                             Error **errp);
typedef uint64_t qemu_plugin_id_t;
extern int qemu_plugin_version;
typedef struct qemu_info_t {
    const char *target_name;
    struct {
        int min;
        int cur;
    } version;
    
   _Bool 
        system_emulation;
    union {
        struct {
            int smp_vcpus;
            int max_vcpus;
        } system;
    };
} qemu_info_t;
 int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv);
typedef void (*qemu_plugin_simple_cb_t)(qemu_plugin_id_t id);
typedef void (*qemu_plugin_udata_cb_t)(qemu_plugin_id_t id, void *userdata);
typedef void (*qemu_plugin_vcpu_simple_cb_t)(qemu_plugin_id_t id,
                                             unsigned int vcpu_index);
typedef void (*qemu_plugin_vcpu_udata_cb_t)(unsigned int vcpu_index,
                                            void *userdata);
void qemu_plugin_uninstall(qemu_plugin_id_t id, qemu_plugin_simple_cb_t cb);
void qemu_plugin_reset(qemu_plugin_id_t id, qemu_plugin_simple_cb_t cb);
void qemu_plugin_register_vcpu_init_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);
void qemu_plugin_register_vcpu_exit_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);
void qemu_plugin_register_vcpu_idle_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);
void qemu_plugin_register_vcpu_resume_cb(qemu_plugin_id_t id,
                                         qemu_plugin_vcpu_simple_cb_t cb);
struct qemu_plugin_tb;
struct qemu_plugin_insn;
struct qemu_plugin_scoreboard;
typedef struct {
    struct qemu_plugin_scoreboard *score;
    size_t offset;
} qemu_plugin_u64;
enum qemu_plugin_cb_flags {
    QEMU_PLUGIN_CB_NO_REGS,
    QEMU_PLUGIN_CB_R_REGS,
    QEMU_PLUGIN_CB_RW_REGS,
};
enum qemu_plugin_mem_rw {
    QEMU_PLUGIN_MEM_R = 1,
    QEMU_PLUGIN_MEM_W,
    QEMU_PLUGIN_MEM_RW,
};
enum qemu_plugin_mem_value_type {
    QEMU_PLUGIN_MEM_VALUE_U8,
    QEMU_PLUGIN_MEM_VALUE_U16,
    QEMU_PLUGIN_MEM_VALUE_U32,
    QEMU_PLUGIN_MEM_VALUE_U64,
    QEMU_PLUGIN_MEM_VALUE_U128,
};
typedef struct {
    enum qemu_plugin_mem_value_type type;
    union {
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        struct {
            uint64_t low;
            uint64_t high;
        } u128;
    } data;
} qemu_plugin_mem_value;
enum qemu_plugin_cond {
    QEMU_PLUGIN_COND_NEVER,
    QEMU_PLUGIN_COND_ALWAYS,
    QEMU_PLUGIN_COND_EQ,
    QEMU_PLUGIN_COND_NE,
    QEMU_PLUGIN_COND_LT,
    QEMU_PLUGIN_COND_LE,
    QEMU_PLUGIN_COND_GT,
    QEMU_PLUGIN_COND_GE,
};
typedef void (*qemu_plugin_vcpu_tb_trans_cb_t)(qemu_plugin_id_t id,
                                               struct qemu_plugin_tb *tb);
void qemu_plugin_register_vcpu_tb_trans_cb(qemu_plugin_id_t id,
                                           qemu_plugin_vcpu_tb_trans_cb_t cb);
void qemu_plugin_register_vcpu_tb_exec_cb(struct qemu_plugin_tb *tb,
                                          qemu_plugin_vcpu_udata_cb_t cb,
                                          enum qemu_plugin_cb_flags flags,
                                          void *userdata);
void qemu_plugin_register_vcpu_tb_exec_cond_cb(struct qemu_plugin_tb *tb,
                                               qemu_plugin_vcpu_udata_cb_t cb,
                                               enum qemu_plugin_cb_flags flags,
                                               enum qemu_plugin_cond cond,
                                               qemu_plugin_u64 entry,
                                               uint64_t imm,
                                               void *userdata);
enum qemu_plugin_op {
    QEMU_PLUGIN_INLINE_ADD_U64,
    QEMU_PLUGIN_INLINE_STORE_U64,
};
void qemu_plugin_register_vcpu_tb_exec_inline_per_vcpu(
    struct qemu_plugin_tb *tb,
    enum qemu_plugin_op op,
    qemu_plugin_u64 entry,
    uint64_t imm);
void qemu_plugin_register_vcpu_insn_exec_cb(struct qemu_plugin_insn *insn,
                                            qemu_plugin_vcpu_udata_cb_t cb,
                                            enum qemu_plugin_cb_flags flags,
                                            void *userdata);
void qemu_plugin_register_vcpu_insn_exec_cond_cb(
    struct qemu_plugin_insn *insn,
    qemu_plugin_vcpu_udata_cb_t cb,
    enum qemu_plugin_cb_flags flags,
    enum qemu_plugin_cond cond,
    qemu_plugin_u64 entry,
    uint64_t imm,
    void *userdata);
void qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(
    struct qemu_plugin_insn *insn,
    enum qemu_plugin_op op,
    qemu_plugin_u64 entry,
    uint64_t imm);
size_t qemu_plugin_tb_n_insns(const struct qemu_plugin_tb *tb);
size_t qemu_plugin_tb_size(const struct qemu_plugin_tb *tb);
uint64_t qemu_plugin_tb_vaddr(const struct qemu_plugin_tb *tb);
struct qemu_plugin_insn *
qemu_plugin_tb_get_insn(const struct qemu_plugin_tb *tb, size_t idx);
struct qemu_plugin_insn *
qemu_plugin_tb_get_insn_by_vaddr(const struct qemu_plugin_tb *tb, uint64_t vaddr);
size_t qemu_plugin_insn_data(const struct qemu_plugin_insn *insn,
                             void *dest, size_t len);
size_t qemu_plugin_insn_size(const struct qemu_plugin_insn *insn);
uint64_t qemu_plugin_insn_vaddr(const struct qemu_plugin_insn *insn);
void *qemu_plugin_insn_haddr(const struct qemu_plugin_insn *insn);
typedef uint32_t qemu_plugin_meminfo_t;
struct qemu_plugin_hwaddr;
unsigned int qemu_plugin_mem_size_shift(qemu_plugin_meminfo_t info);
_Bool 
    qemu_plugin_mem_is_sign_extended(qemu_plugin_meminfo_t info);
_Bool 
    qemu_plugin_mem_is_big_endian(qemu_plugin_meminfo_t info);
_Bool 
    qemu_plugin_mem_is_store(qemu_plugin_meminfo_t info);
qemu_plugin_mem_value qemu_plugin_mem_get_value(qemu_plugin_meminfo_t info);
struct qemu_plugin_hwaddr *qemu_plugin_get_hwaddr(qemu_plugin_meminfo_t info,
                                                  uint64_t vaddr);
_Bool 
    qemu_plugin_hwaddr_is_io(const struct qemu_plugin_hwaddr *haddr);
uint64_t qemu_plugin_hwaddr_phys_addr(const struct qemu_plugin_hwaddr *haddr);
const char *qemu_plugin_hwaddr_device_name(const struct qemu_plugin_hwaddr *h);
typedef void (*qemu_plugin_vcpu_mem_cb_t) (unsigned int vcpu_index,
                                           qemu_plugin_meminfo_t info,
                                           uint64_t vaddr,
                                           void *userdata);
void qemu_plugin_register_vcpu_mem_cb(struct qemu_plugin_insn *insn,
                                      qemu_plugin_vcpu_mem_cb_t cb,
                                      enum qemu_plugin_cb_flags flags,
                                      enum qemu_plugin_mem_rw rw,
                                      void *userdata);
void qemu_plugin_register_vcpu_mem_inline_per_vcpu(
    struct qemu_plugin_insn *insn,
    enum qemu_plugin_mem_rw rw,
    enum qemu_plugin_op op,
    qemu_plugin_u64 entry,
    uint64_t imm);
const void *qemu_plugin_request_time_control(void);
void qemu_plugin_update_ns(const void *handle, int64_t time);
typedef void
(*qemu_plugin_vcpu_syscall_cb_t)(qemu_plugin_id_t id, unsigned int vcpu_index,
                                 int64_t num, uint64_t a1, uint64_t a2,
                                 uint64_t a3, uint64_t a4, uint64_t a5,
                                 uint64_t a6, uint64_t a7, uint64_t a8);
void qemu_plugin_register_vcpu_syscall_cb(qemu_plugin_id_t id,
                                          qemu_plugin_vcpu_syscall_cb_t cb);
typedef void
(*qemu_plugin_vcpu_syscall_ret_cb_t)(qemu_plugin_id_t id, unsigned int vcpu_idx,
                                     int64_t num, int64_t ret);
void
qemu_plugin_register_vcpu_syscall_ret_cb(qemu_plugin_id_t id,
                                         qemu_plugin_vcpu_syscall_ret_cb_t cb);
char *qemu_plugin_insn_disas(const struct qemu_plugin_insn *insn);
const char *qemu_plugin_insn_symbol(const struct qemu_plugin_insn *insn);
void qemu_plugin_vcpu_for_each(qemu_plugin_id_t id,
                               qemu_plugin_vcpu_simple_cb_t cb);
void qemu_plugin_register_flush_cb(qemu_plugin_id_t id,
                                   qemu_plugin_simple_cb_t cb);
void qemu_plugin_register_atexit_cb(qemu_plugin_id_t id,
                                    qemu_plugin_udata_cb_t cb, void *userdata);
int qemu_plugin_num_vcpus(void);
void qemu_plugin_outs(const char *string);
_Bool 
    qemu_plugin_bool_parse(const char *name, const char *val, 
                                                              _Bool 
                                                                   *ret);
const char *qemu_plugin_path_to_binary(void);
uint64_t qemu_plugin_start_code(void);
uint64_t qemu_plugin_end_code(void);
uint64_t qemu_plugin_entry_code(void);
struct qemu_plugin_register;
typedef struct {
    struct qemu_plugin_register *handle;
    const char *name;
    const char *feature;
} qemu_plugin_reg_descriptor;
GArray *qemu_plugin_get_registers(void);
_Bool 
    qemu_plugin_read_memory_vaddr(uint64_t addr,
                                   GByteArray *data, size_t len);
int qemu_plugin_read_register(struct qemu_plugin_register *handle,
                              GByteArray *buf);
struct qemu_plugin_scoreboard *qemu_plugin_scoreboard_new(size_t element_size);
void qemu_plugin_scoreboard_free(struct qemu_plugin_scoreboard *score);
void *qemu_plugin_scoreboard_find(struct qemu_plugin_scoreboard *score,
                                  unsigned int vcpu_index);
void qemu_plugin_u64_add(qemu_plugin_u64 entry, unsigned int vcpu_index,
                         uint64_t added);
uint64_t qemu_plugin_u64_get(qemu_plugin_u64 entry, unsigned int vcpu_index);
void qemu_plugin_u64_set(qemu_plugin_u64 entry, unsigned int vcpu_index,
                         uint64_t val);
uint64_t qemu_plugin_u64_sum(qemu_plugin_u64 entry);
typedef struct Location {
    enum { LOC_NONE, LOC_CMDLINE, LOC_FILE } kind;
    int num;
    const void *ptr;
    struct Location *prev;
} Location;
Location *loc_push_restore(Location *loc);
Location *loc_push_none(Location *loc);
Location *loc_pop(Location *loc);
Location *loc_save(Location *loc);
void loc_restore(Location *loc);
void loc_set_none(void);
void loc_set_cmdline(char **argv, int idx, int cnt);
void loc_set_file(const char *fname, int lno);
int error_vprintf(const char *fmt, va_list ap) ;
int error_printf(const char *fmt, ...) ;
void error_vreport(const char *fmt, va_list ap) ;
void warn_vreport(const char *fmt, va_list ap) ;
void info_vreport(const char *fmt, va_list ap) ;
void error_report(const char *fmt, ...) ;
void warn_report(const char *fmt, ...) ;
void info_report(const char *fmt, ...) ;
_Bool 
    error_report_once_cond(
                           _Bool 
                                *printed, const char *fmt, ...)
    ;
_Bool 
    warn_report_once_cond(
                          _Bool 
                               *printed, const char *fmt, ...)
    ;
void error_init(const char *argv0);
extern 
      _Bool 
           message_with_timestamp;
extern 
      _Bool 
           error_with_guestname;
extern const char *error_guest_name;
const char *get_opt_value(const char *p, char **value);
_Bool 
    parse_option_size(const char *name, const char *value,
                       uint64_t *ret, Error **errp);
_Bool 
    has_help_option(const char *param);
enum QemuOptType {
    QEMU_OPT_STRING = 0,
    QEMU_OPT_BOOL,
    QEMU_OPT_NUMBER,
    QEMU_OPT_SIZE,
};
typedef struct QemuOpt QemuOpt;
typedef struct QemuOptDesc {
    const char *name;
    enum QemuOptType type;
    const char *help;
    const char *def_value_str;
} QemuOptDesc;
struct QemuOptsList {
    const char *name;
    const char *implied_opt_name;
    
   _Bool 
        merge_lists;
    union { struct QemuOpts *tqh_first; QTailQLink tqh_circ; } head;
    QemuOptDesc desc[];
};
const char *qemu_opt_get(QemuOpts *opts, const char *name);
char *qemu_opt_get_del(QemuOpts *opts, const char *name);
_Bool 
    qemu_opt_has_help_opt(QemuOpts *opts);
QemuOpt *qemu_opt_find(QemuOpts *opts, const char *name);
_Bool 
    qemu_opt_get_bool(QemuOpts *opts, const char *name, 
                                                        _Bool 
                                                             defval);
uint64_t qemu_opt_get_number(QemuOpts *opts, const char *name, uint64_t defval);
uint64_t qemu_opt_get_size(QemuOpts *opts, const char *name, uint64_t defval);
_Bool 
    qemu_opt_get_bool_del(QemuOpts *opts, const char *name, 
                                                            _Bool 
                                                                 defval);
uint64_t qemu_opt_get_number_del(QemuOpts *opts, const char *name,
                                 uint64_t defval);
uint64_t qemu_opt_get_size_del(QemuOpts *opts, const char *name,
                               uint64_t defval);
int qemu_opt_unset(QemuOpts *opts, const char *name);
_Bool 
    qemu_opt_set(QemuOpts *opts, const char *name, const char *value,
                  Error **errp);
_Bool 
    qemu_opt_set_bool(QemuOpts *opts, const char *name, 
                                                        _Bool 
                                                             val,
                       Error **errp);
_Bool 
    qemu_opt_set_number(QemuOpts *opts, const char *name, int64_t val,
                         Error **errp);
typedef int (*qemu_opt_loopfunc)(void *opaque,
                                 const char *name, const char *value,
                                 Error **errp);
int qemu_opt_foreach(QemuOpts *opts, qemu_opt_loopfunc func, void *opaque,
                     Error **errp);
typedef struct {
    QemuOpts *opts;
    QemuOpt *opt;
    const char *name;
} QemuOptsIter;
void qemu_opt_iter_init(QemuOptsIter *iter, QemuOpts *opts, const char *name);
const char *qemu_opt_iter_next(QemuOptsIter *iter);
QemuOpts *qemu_opts_find(QemuOptsList *list, const char *id);
QemuOpts *qemu_opts_create(QemuOptsList *list, const char *id,
                           int fail_if_exists, Error **errp);
void qemu_opts_reset(QemuOptsList *list);
void qemu_opts_loc_restore(QemuOpts *opts);
const char *qemu_opts_id(QemuOpts *opts);
void qemu_opts_set_id(QemuOpts *opts, char *id);
void qemu_opts_del(QemuOpts *opts);
_Bool 
    qemu_opts_validate(QemuOpts *opts, const QemuOptDesc *desc, Error **errp);
_Bool 
    qemu_opts_do_parse(QemuOpts *opts, const char *params,
                        const char *firstname, Error **errp);
QemuOpts *qemu_opts_parse_noisily(QemuOptsList *list, const char *params,
                                  
                                 _Bool 
                                      permit_abbrev);
QemuOpts *qemu_opts_parse(QemuOptsList *list, const char *params,
                          
                         _Bool 
                              permit_abbrev, Error **errp);
QemuOpts *qemu_opts_from_qdict(QemuOptsList *list, const QDict *qdict,
                               Error **errp);
QDict *qemu_opts_to_qdict_filtered(QemuOpts *opts, QDict *qdict,
                                   QemuOptsList *list, 
                                                      _Bool 
                                                           del);
QDict *qemu_opts_to_qdict(QemuOpts *opts, QDict *qdict);
_Bool 
    qemu_opts_absorb_qdict(QemuOpts *opts, QDict *qdict, Error **errp);
typedef int (*qemu_opts_loopfunc)(void *opaque, QemuOpts *opts, Error **errp);
int qemu_opts_foreach(QemuOptsList *list, qemu_opts_loopfunc func,
                      void *opaque, Error **errp);
void qemu_opts_print(QemuOpts *opts, const char *sep);
void qemu_opts_print_help(QemuOptsList *list, 
                                             _Bool 
                                                  print_caption);
void qemu_opts_free(QemuOptsList *list);
QemuOptsList *qemu_opts_append(QemuOptsList *dst, QemuOptsList *list);
typedef QemuOpts *QemuOpts_autoptr; typedef GList *QemuOpts_listautoptr; typedef GSList *QemuOpts_slistautoptr; typedef GQueue *QemuOpts_queueautoptr;
enum qemu_plugin_event {
    QEMU_PLUGIN_EV_VCPU_INIT,
    QEMU_PLUGIN_EV_VCPU_EXIT,
    QEMU_PLUGIN_EV_VCPU_TB_TRANS,
    QEMU_PLUGIN_EV_VCPU_IDLE,
    QEMU_PLUGIN_EV_VCPU_RESUME,
    QEMU_PLUGIN_EV_VCPU_SYSCALL,
    QEMU_PLUGIN_EV_VCPU_SYSCALL_RET,
    QEMU_PLUGIN_EV_FLUSH,
    QEMU_PLUGIN_EV_ATEXIT,
    QEMU_PLUGIN_EV_MAX,
};
typedef enum MemOp {
    MO_8 = 0,
    MO_16 = 1,
    MO_32 = 2,
    MO_64 = 3,
    MO_128 = 4,
    MO_256 = 5,
    MO_512 = 6,
    MO_1024 = 7,
    MO_SIZE = 0x07,
    MO_SIGN = 0x08,
    MO_BSWAP = 0x10,
    MO_LE = 0,
    MO_BE = MO_BSWAP,
    MO_TE = MO_BE,
    MO_ASHIFT = 5,
    MO_AMASK = 0x7 << MO_ASHIFT,
    MO_UNALN = 0,
    MO_ALIGN_2 = 1 << MO_ASHIFT,
    MO_ALIGN_4 = 2 << MO_ASHIFT,
    MO_ALIGN_8 = 3 << MO_ASHIFT,
    MO_ALIGN_16 = 4 << MO_ASHIFT,
    MO_ALIGN_32 = 5 << MO_ASHIFT,
    MO_ALIGN_64 = 6 << MO_ASHIFT,
    MO_ALIGN = MO_AMASK,
    MO_ATOM_SHIFT = 8,
    MO_ATOM_IFALIGN = 0 << MO_ATOM_SHIFT,
    MO_ATOM_IFALIGN_PAIR = 1 << MO_ATOM_SHIFT,
    MO_ATOM_WITHIN16 = 2 << MO_ATOM_SHIFT,
    MO_ATOM_WITHIN16_PAIR = 3 << MO_ATOM_SHIFT,
    MO_ATOM_SUBALIGN = 4 << MO_ATOM_SHIFT,
    MO_ATOM_NONE = 5 << MO_ATOM_SHIFT,
    MO_ATOM_MASK = 7 << MO_ATOM_SHIFT,
    MO_UB = MO_8,
    MO_UW = MO_16,
    MO_UL = MO_32,
    MO_UQ = MO_64,
    MO_UO = MO_128,
    MO_SB = MO_SIGN | MO_8,
    MO_SW = MO_SIGN | MO_16,
    MO_SL = MO_SIGN | MO_32,
    MO_SQ = MO_SIGN | MO_64,
    MO_SO = MO_SIGN | MO_128,
    MO_LEUW = MO_LE | MO_UW,
    MO_LEUL = MO_LE | MO_UL,
    MO_LEUQ = MO_LE | MO_UQ,
    MO_LESW = MO_LE | MO_SW,
    MO_LESL = MO_LE | MO_SL,
    MO_LESQ = MO_LE | MO_SQ,
    MO_BEUW = MO_BE | MO_UW,
    MO_BEUL = MO_BE | MO_UL,
    MO_BEUQ = MO_BE | MO_UQ,
    MO_BESW = MO_BE | MO_SW,
    MO_BESL = MO_BE | MO_SL,
    MO_BESQ = MO_BE | MO_SQ,
    MO_TEUW = MO_TE | MO_UW,
    MO_TEUL = MO_TE | MO_UL,
    MO_TEUQ = MO_TE | MO_UQ,
    MO_TEUO = MO_TE | MO_UO,
    MO_TESW = MO_TE | MO_SW,
    MO_TESL = MO_TE | MO_SL,
    MO_TESQ = MO_TE | MO_SQ,
    MO_SSIZE = MO_SIZE | MO_SIGN,
} MemOp;
typedef uint32_t MemOpIdx;
struct qemu_plugin_desc;
typedef union { struct qemu_plugin_desc *tqh_first; QTailQLink tqh_circ; } QemuPluginList;
extern QemuOptsList qemu_plugin_opts;
void qemu_plugin_opt_parse(const char *optstr, QemuPluginList *head);
int qemu_plugin_load_list(QemuPluginList *head, Error **errp);
union qemu_plugin_cb_sig {
    qemu_plugin_simple_cb_t simple;
    qemu_plugin_udata_cb_t udata;
    qemu_plugin_vcpu_simple_cb_t vcpu_simple;
    qemu_plugin_vcpu_udata_cb_t vcpu_udata;
    qemu_plugin_vcpu_tb_trans_cb_t vcpu_tb_trans;
    qemu_plugin_vcpu_mem_cb_t vcpu_mem;
    qemu_plugin_vcpu_syscall_cb_t vcpu_syscall;
    qemu_plugin_vcpu_syscall_ret_cb_t vcpu_syscall_ret;
    void *generic;
};
enum plugin_dyn_cb_type {
    PLUGIN_CB_REGULAR,
    PLUGIN_CB_COND,
    PLUGIN_CB_MEM_REGULAR,
    PLUGIN_CB_INLINE_ADD_U64,
    PLUGIN_CB_INLINE_STORE_U64,
};
struct qemu_plugin_regular_cb {
    union qemu_plugin_cb_sig f;
    TCGHelperInfo *info;
    void *userp;
    enum qemu_plugin_mem_rw rw;
};
struct qemu_plugin_inline_cb {
    qemu_plugin_u64 entry;
    uint64_t imm;
    enum qemu_plugin_mem_rw rw;
};
struct qemu_plugin_conditional_cb {
    union qemu_plugin_cb_sig f;
    TCGHelperInfo *info;
    void *userp;
    qemu_plugin_u64 entry;
    enum qemu_plugin_cond cond;
    uint64_t imm;
};
struct qemu_plugin_dyn_cb {
    enum plugin_dyn_cb_type type;
    union {
        struct qemu_plugin_regular_cb regular;
        struct qemu_plugin_conditional_cb cond;
        struct qemu_plugin_inline_cb inline_insn;
    };
};
struct qemu_plugin_insn {
    uint64_t vaddr;
    GArray *insn_cbs;
    GArray *mem_cbs;
    uint8_t len;
    
   _Bool 
        calls_helpers;
    
   _Bool 
        mem_helper;
};
struct qemu_plugin_scoreboard {
    GArray *data;
    struct { struct qemu_plugin_scoreboard *le_next; struct qemu_plugin_scoreboard **le_prev; } entry;
};
struct qemu_plugin_tb {
    GPtrArray *insns;
    size_t n;
    
   _Bool 
        mem_helper;
    GArray *cbs;
};
struct CPUPluginState {
    unsigned long event_mask[1];
};
CPUPluginState *qemu_plugin_create_vcpu_state(void);
void qemu_plugin_vcpu_init_hook(CPUState *cpu);
void qemu_plugin_vcpu_exit_hook(CPUState *cpu);
void qemu_plugin_tb_trans_cb(CPUState *cpu, struct qemu_plugin_tb *tb);
void qemu_plugin_vcpu_idle_cb(CPUState *cpu);
void qemu_plugin_vcpu_resume_cb(CPUState *cpu);
void
qemu_plugin_vcpu_syscall(CPUState *cpu, int64_t num, uint64_t a1,
                         uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7, uint64_t a8);
void qemu_plugin_vcpu_syscall_ret(CPUState *cpu, int64_t num, int64_t ret);
void qemu_plugin_vcpu_mem_cb(CPUState *cpu, uint64_t vaddr,
                             uint64_t value_low,
                             uint64_t value_high,
                             MemOpIdx oi, enum qemu_plugin_mem_rw rw);
void qemu_plugin_flush_cb(void);
void qemu_plugin_atexit_cb(void);
void qemu_plugin_add_dyn_cb_arr(GArray *arr);
void qemu_plugin_user_exit(void);
void qemu_plugin_user_prefork_lock(void);
void qemu_plugin_user_postfork(
                              _Bool 
                                   is_child);
typedef struct Coroutine Coroutine;
typedef struct CoMutex CoMutex;
typedef void CoroutineEntry(void *opaque);
Coroutine *qemu_coroutine_create(CoroutineEntry *entry, void *opaque);
void qemu_coroutine_enter(Coroutine *coroutine);
void qemu_coroutine_enter_if_inactive(Coroutine *co);
void qemu_aio_coroutine_enter(AioContext *ctx, Coroutine *co);
void qemu_coroutine_yield(void);
AioContext *qemu_coroutine_get_aio_context(Coroutine *co);
Coroutine *qemu_coroutine_self(void);
_Bool 
    qemu_in_coroutine(void);
_Bool 
    qemu_coroutine_entered(Coroutine *co);
void qemu_co_mutex_init(CoMutex *mutex);
void qemu_co_mutex_lock(CoMutex *mutex);
void qemu_co_mutex_unlock(CoMutex *mutex);
typedef void QemuLockUnlockFunc(void *);
typedef struct QemuLockable {
    void *object;
    QemuLockUnlockFunc *lock;
    QemuLockUnlockFunc *unlock;
} QemuLockable;
typedef QemuLockable *QemuLockable_autoptr; typedef GList *QemuLockable_listautoptr; typedef GSList *QemuLockable_slistautoptr; typedef GQueue *QemuLockable_queueautoptr;
typedef struct QemuSeqLock QemuSeqLock;
struct QemuSeqLock {
    unsigned sequence;
};
struct qdist_entry {
    double x;
    unsigned long count;
};
struct qdist {
    struct qdist_entry *entries;
    size_t n;
    size_t size;
};
void qdist_init(struct qdist *dist);
void qdist_destroy(struct qdist *dist);
void qdist_add(struct qdist *dist, double x, long count);
void qdist_inc(struct qdist *dist, double x);
double qdist_xmin(const struct qdist *dist);
double qdist_xmax(const struct qdist *dist);
double qdist_avg(const struct qdist *dist);
unsigned long qdist_sample_count(const struct qdist *dist);
size_t qdist_unique_entries(const struct qdist *dist);
char *qdist_pr_plain(const struct qdist *dist, size_t n_groups);
char *qdist_pr(const struct qdist *dist, size_t n_groups, uint32_t opt);
void qdist_bin__internal(struct qdist *to, const struct qdist *from, size_t n);
typedef 
       _Bool 
            (*qht_cmp_func_t)(const void *a, const void *b);
struct qht {
    struct qht_map *map;
    qht_cmp_func_t cmp;
    QemuMutex lock;
    unsigned int mode;
};
struct qht_stats {
    size_t head_buckets;
    size_t used_head_buckets;
    size_t entries;
    struct qdist chain;
    struct qdist occupancy;
};
typedef 
       _Bool 
            (*qht_lookup_func_t)(const void *obj, const void *userp);
typedef void (*qht_iter_func_t)(void *p, uint32_t h, void *up);
typedef 
       _Bool 
            (*qht_iter_bool_func_t)(void *p, uint32_t h, void *up);
void qht_init(struct qht *ht, qht_cmp_func_t cmp, size_t n_elems,
              unsigned int mode);
void qht_destroy(struct qht *ht);
_Bool 
    qht_insert(struct qht *ht, void *p, uint32_t hash, void **existing);
void *qht_lookup_custom(const struct qht *ht, const void *userp, uint32_t hash,
                        qht_lookup_func_t func);
void *qht_lookup(const struct qht *ht, const void *userp, uint32_t hash);
_Bool 
    qht_remove(struct qht *ht, const void *p, uint32_t hash);
void qht_reset(struct qht *ht);
_Bool 
    qht_reset_size(struct qht *ht, size_t n_elems);
_Bool 
    qht_resize(struct qht *ht, size_t n_elems);
void qht_iter(struct qht *ht, qht_iter_func_t func, void *userp);
void qht_iter_remove(struct qht *ht, qht_iter_bool_func_t func, void *userp);
void qht_statistics_init(const struct qht *ht, struct qht_stats *stats);
void qht_statistics_destroy(struct qht_stats *stats);
struct qemu_plugin_state {
    union { struct qemu_plugin_ctx *tqh_first; QTailQLink tqh_circ; } ctxs;
    struct { struct qemu_plugin_cb *lh_first; } cb_lists[QEMU_PLUGIN_EV_MAX];
    GHashTable *id_ht;
    GHashTable *cpu_ht;
    struct { struct qemu_plugin_scoreboard *lh_first; } scoreboards;
    size_t scoreboard_alloc_size;
    unsigned long mask[1];
    QemuRecMutex lock;
    struct qht dyn_cb_arr_ht;
    int num_vcpus;
};
struct qemu_plugin_ctx {
    GModule *handle;
    qemu_plugin_id_t id;
    struct qemu_plugin_cb *callbacks[QEMU_PLUGIN_EV_MAX];
    union { struct qemu_plugin_ctx *tqe_next; QTailQLink tqe_circ; } entry;
    struct qemu_plugin_desc *desc;
    
   _Bool 
        installing;
    
   _Bool 
        uninstalling;
    
   _Bool 
        resetting;
};
struct qemu_plugin_ctx *plugin_id_to_ctx_locked(qemu_plugin_id_t id);
void plugin_register_inline_op_on_entry(GArray **arr,
                                        enum qemu_plugin_mem_rw rw,
                                        enum qemu_plugin_op op,
                                        qemu_plugin_u64 entry,
                                        uint64_t imm);
void plugin_reset_uninstall(qemu_plugin_id_t id,
                            qemu_plugin_simple_cb_t cb,
                            
                           _Bool 
                                reset);
void plugin_register_cb(qemu_plugin_id_t id, enum qemu_plugin_event ev,
                        void *func);
void plugin_unregister_cb__locked(struct qemu_plugin_ctx *ctx,
                                  enum qemu_plugin_event ev);
void
plugin_register_cb_udata(qemu_plugin_id_t id, enum qemu_plugin_event ev,
                         void *func, void *udata);
void
plugin_register_dyn_cb__udata(GArray **arr,
                              qemu_plugin_vcpu_udata_cb_t cb,
                              enum qemu_plugin_cb_flags flags, void *udata);
void
plugin_register_dyn_cond_cb__udata(GArray **arr,
                                   qemu_plugin_vcpu_udata_cb_t cb,
                                   enum qemu_plugin_cb_flags flags,
                                   enum qemu_plugin_cond cond,
                                   qemu_plugin_u64 entry,
                                   uint64_t imm,
                                   void *udata);
void plugin_register_vcpu_mem_cb(GArray **arr,
                                 void *cb,
                                 enum qemu_plugin_cb_flags flags,
                                 enum qemu_plugin_mem_rw rw,
                                 void *udata);
void exec_inline_op(enum plugin_dyn_cb_type type,
                    struct qemu_plugin_inline_cb *cb,
                    int cpu_index);
int plugin_num_vcpus(void);
struct qemu_plugin_scoreboard *plugin_scoreboard_new(size_t element_size);
void plugin_scoreboard_free(struct qemu_plugin_scoreboard *score);
       
typedef uint16_t float16;
typedef uint32_t float32;
typedef uint64_t float64;
typedef struct {
    uint64_t low;
    uint16_t high;
} floatx80;
typedef struct {
    uint64_t low, high;
} float128;
typedef uint16_t bfloat16;
typedef enum {
    float_round_nearest_even = 0,
    float_round_down = 1,
    float_round_up = 2,
    float_round_to_zero = 3,
    float_round_ties_away = 4,
    float_round_to_odd = 5,
    float_round_to_odd_inf = 6,
    float_round_nearest_even_max = 7,
} FloatRoundMode;
enum {
    float_flag_invalid = 0x0001,
    float_flag_divbyzero = 0x0002,
    float_flag_overflow = 0x0004,
    float_flag_underflow = 0x0008,
    float_flag_inexact = 0x0010,
    float_flag_input_denormal = 0x0020,
    float_flag_output_denormal = 0x0040,
    float_flag_invalid_isi = 0x0080,
    float_flag_invalid_imz = 0x0100,
    float_flag_invalid_idi = 0x0200,
    float_flag_invalid_zdz = 0x0400,
    float_flag_invalid_sqrt = 0x0800,
    float_flag_invalid_cvti = 0x1000,
    float_flag_invalid_snan = 0x2000,
};
typedef enum {
    floatx80_precision_x,
    floatx80_precision_d,
    floatx80_precision_s,
} FloatX80RoundPrec;
typedef enum {
    float_2nan_prop_none = 0,
    float_2nan_prop_s_ab,
    float_2nan_prop_s_ba,
    float_2nan_prop_ab,
    float_2nan_prop_ba,
    float_2nan_prop_x87,
} Float2NaNPropRule;
enum { R_3NAN_1ST_SHIFT = 0}; enum { R_3NAN_1ST_LENGTH = 2}; enum { R_3NAN_1ST_MASK = 3};
enum { R_3NAN_2ND_SHIFT = 2}; enum { R_3NAN_2ND_LENGTH = 2}; enum { R_3NAN_2ND_MASK = 12};
enum { R_3NAN_3RD_SHIFT = 4}; enum { R_3NAN_3RD_LENGTH = 2}; enum { R_3NAN_3RD_MASK = 48};
enum { R_3NAN_SNAN_SHIFT = 6}; enum { R_3NAN_SNAN_LENGTH = 1}; enum { R_3NAN_SNAN_MASK = 64};
typedef enum {
    float_3nan_prop_none = 0,
    float_3nan_prop_abc = 36,
    float_3nan_prop_acb = 24,
    float_3nan_prop_bac = 33,
    float_3nan_prop_bca = 9,
    float_3nan_prop_cab = 18,
    float_3nan_prop_cba = 6,
    float_3nan_prop_s_abc = float_3nan_prop_abc | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_acb = float_3nan_prop_acb | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_bac = float_3nan_prop_bac | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_bca = float_3nan_prop_bca | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_cab = float_3nan_prop_cab | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_cba = float_3nan_prop_cba | R_3NAN_SNAN_MASK,
} Float3NaNPropRule;
typedef enum {
    float_infzeronan_none = 0,
    float_infzeronan_dnan_never,
    float_infzeronan_dnan_always,
    float_infzeronan_dnan_if_qnan,
} FloatInfZeroNaNRule;
typedef struct float_status {
    uint16_t float_exception_flags;
    FloatRoundMode float_rounding_mode;
    FloatX80RoundPrec floatx80_rounding_precision;
    Float2NaNPropRule float_2nan_prop_rule;
    Float3NaNPropRule float_3nan_prop_rule;
    FloatInfZeroNaNRule float_infzeronan_rule;
    
   _Bool 
        tininess_before_rounding;
    
   _Bool 
        flush_to_zero;
    
   _Bool 
        flush_inputs_to_zero;
    
   _Bool 
        default_nan_mode;
    uint8_t default_nan_pattern;
    
   _Bool 
        snan_bit_is_one;
    
   _Bool 
        no_signaling_nans;
    
   _Bool 
        rebias_overflow;
    
   _Bool 
        rebias_underflow;
} float_status;
typedef union {
    float32 f;
    uint32_t l;
} CPU_FloatU;
typedef union {
    float64 d;
    struct {
        uint32_t lower;
        uint32_t upper;
    } l;
    uint64_t ll;
} CPU_DoubleU;
typedef union {
     floatx80 d;
     struct {
         uint64_t lower;
         uint16_t upper;
     } l;
} CPU_LDoubleU;
typedef union {
    float128 q;
    struct {
        uint32_t lowest;
        uint32_t lower;
        uint32_t upper;
        uint32_t upmost;
    } l;
    struct {
        uint64_t lower;
        uint64_t upper;
    } ll;
} CPU_QuadU;
typedef struct GDBFeature {
    const char *xmlname;
    const char *xml;
    const char *name;
    const char * const *regs;
    int num_regs;
} GDBFeature;
typedef struct GDBFeatureBuilder {
    GDBFeature *feature;
    GPtrArray *xml;
    GPtrArray *regs;
    int base_reg;
} GDBFeatureBuilder;
typedef int (*gdb_get_reg_cb)(CPUState *cpu, GByteArray *buf, int reg);
typedef int (*gdb_set_reg_cb)(CPUState *cpu, uint8_t *buf, int reg);
void gdb_init_cpu(CPUState *cpu);
void gdb_register_coprocessor(CPUState *cpu,
                              gdb_get_reg_cb get_reg, gdb_set_reg_cb set_reg,
                              const GDBFeature *feature, int g_pos);
void gdb_unregister_coprocessor_all(CPUState *cpu);
_Bool 
    gdbserver_start(const char *port_or_device, Error **errp);
void gdb_feature_builder_init(GDBFeatureBuilder *builder, GDBFeature *feature,
                              const char *name, const char *xmlname,
                              int base_reg);
void
gdb_feature_builder_append_tag(const GDBFeatureBuilder *builder,
                               const char *format, ...);
void gdb_feature_builder_append_reg(const GDBFeatureBuilder *builder,
                                    const char *name,
                                    int bitsize,
                                    int regnum,
                                    const char *type,
                                    const char *group);
void gdb_feature_builder_end(const GDBFeatureBuilder *builder);
const GDBFeature *gdb_find_static_feature(const char *xmlname);
int gdb_read_register(CPUState *cpu, GByteArray *buf, int reg);
typedef struct {
    int gdb_reg;
    const char *name;
    const char *feature_name;
} GDBRegDesc;
GArray *gdb_get_register_list(CPUState *cpu);
void gdb_set_stop_cpu(CPUState *cpu);
extern const GDBFeature gdb_static_features[];
typedef struct ArchCPU PowerPCCPU; typedef struct ArchCPU ArchCPU; typedef struct PowerPCCPUClass PowerPCCPUClass; typedef ArchCPU *ArchCPU_autoptr; typedef GList *ArchCPU_listautoptr; typedef GSList *ArchCPU_slistautoptr; typedef GQueue *ArchCPU_queueautoptr;
typedef struct PPCTimebase {
    uint64_t guest_timebase;
    int64_t time_of_the_day_ns;
    
   _Bool 
        runstate_paused;
} PPCTimebase;
extern const VMStateDescription vmstate_ppc_timebase;
void cpu_ppc_clock_vm_state_change(void *opaque, 
                                                _Bool 
                                                     running,
                                   RunState state);
enum {
    POWERPC_EXCP_NONE = -1,
    POWERPC_EXCP_CRITICAL = 0,
    POWERPC_EXCP_MCHECK = 1,
    POWERPC_EXCP_DSI = 2,
    POWERPC_EXCP_ISI = 3,
    POWERPC_EXCP_EXTERNAL = 4,
    POWERPC_EXCP_ALIGN = 5,
    POWERPC_EXCP_PROGRAM = 6,
    POWERPC_EXCP_FPU = 7,
    POWERPC_EXCP_SYSCALL = 8,
    POWERPC_EXCP_APU = 9,
    POWERPC_EXCP_DECR = 10,
    POWERPC_EXCP_FIT = 11,
    POWERPC_EXCP_WDT = 12,
    POWERPC_EXCP_DTLB = 13,
    POWERPC_EXCP_ITLB = 14,
    POWERPC_EXCP_DEBUG = 15,
    POWERPC_EXCP_SPEU = 32,
    POWERPC_EXCP_EFPDI = 33,
    POWERPC_EXCP_EFPRI = 34,
    POWERPC_EXCP_EPERFM = 35,
    POWERPC_EXCP_DOORI = 36,
    POWERPC_EXCP_DOORCI = 37,
    POWERPC_EXCP_GDOORI = 38,
    POWERPC_EXCP_GDOORCI = 39,
    POWERPC_EXCP_HYPPRIV = 41,
    POWERPC_EXCP_RESET = 64,
    POWERPC_EXCP_DSEG = 65,
    POWERPC_EXCP_ISEG = 66,
    POWERPC_EXCP_HDECR = 67,
    POWERPC_EXCP_TRACE = 68,
    POWERPC_EXCP_HDSI = 69,
    POWERPC_EXCP_HISI = 70,
    POWERPC_EXCP_HDSEG = 71,
    POWERPC_EXCP_HISEG = 72,
    POWERPC_EXCP_VPU = 73,
    POWERPC_EXCP_PIT = 74,
    POWERPC_EXCP_EMUL = 77,
    POWERPC_EXCP_IFTLB = 78,
    POWERPC_EXCP_DLTLB = 79,
    POWERPC_EXCP_DSTLB = 80,
    POWERPC_EXCP_FPA = 81,
    POWERPC_EXCP_DABR = 82,
    POWERPC_EXCP_IABR = 83,
    POWERPC_EXCP_SMI = 84,
    POWERPC_EXCP_PERFM = 85,
    POWERPC_EXCP_THERM = 86,
    POWERPC_EXCP_VPUA = 87,
    POWERPC_EXCP_SOFTP = 88,
    POWERPC_EXCP_MAINT = 89,
    POWERPC_EXCP_MEXTBR = 90,
    POWERPC_EXCP_NMEXTBR = 91,
    POWERPC_EXCP_ITLBE = 92,
    POWERPC_EXCP_DTLBE = 93,
    POWERPC_EXCP_VSXU = 94,
    POWERPC_EXCP_FU = 95,
    POWERPC_EXCP_HV_EMU = 96,
    POWERPC_EXCP_HV_MAINT = 97,
    POWERPC_EXCP_HV_FU = 98,
    POWERPC_EXCP_SDOOR = 99,
    POWERPC_EXCP_SDOOR_HV = 100,
    POWERPC_EXCP_HVIRT = 101,
    POWERPC_EXCP_SYSCALL_VECTORED = 102,
    POWERPC_EXCP_PERFM_EBB = 103,
    POWERPC_EXCP_EXTERNAL_EBB = 104,
    POWERPC_EXCP_NB = 105,
    POWERPC_EXCP_SYSCALL_USER = 0x203,
};
enum {
    POWERPC_EXCP_ALIGN_FP = 0x01,
    POWERPC_EXCP_ALIGN_LST = 0x02,
    POWERPC_EXCP_ALIGN_LE = 0x03,
    POWERPC_EXCP_ALIGN_PROT = 0x04,
    POWERPC_EXCP_ALIGN_BAT = 0x05,
    POWERPC_EXCP_ALIGN_CACHE = 0x06,
    POWERPC_EXCP_ALIGN_INSN = 0x07,
    POWERPC_EXCP_FP = 0x10,
    POWERPC_EXCP_FP_OX = 0x01,
    POWERPC_EXCP_FP_UX = 0x02,
    POWERPC_EXCP_FP_ZX = 0x03,
    POWERPC_EXCP_FP_XX = 0x04,
    POWERPC_EXCP_FP_VXSNAN = 0x05,
    POWERPC_EXCP_FP_VXISI = 0x06,
    POWERPC_EXCP_FP_VXIDI = 0x07,
    POWERPC_EXCP_FP_VXZDZ = 0x08,
    POWERPC_EXCP_FP_VXIMZ = 0x09,
    POWERPC_EXCP_FP_VXVC = 0x0A,
    POWERPC_EXCP_FP_VXSOFT = 0x0B,
    POWERPC_EXCP_FP_VXSQRT = 0x0C,
    POWERPC_EXCP_FP_VXCVI = 0x0D,
    POWERPC_EXCP_INVAL = 0x20,
    POWERPC_EXCP_INVAL_INVAL = 0x01,
    POWERPC_EXCP_INVAL_LSWX = 0x02,
    POWERPC_EXCP_INVAL_SPR = 0x03,
    POWERPC_EXCP_INVAL_FP = 0x04,
    POWERPC_EXCP_PRIV = 0x30,
    POWERPC_EXCP_PRIV_OPC = 0x01,
    POWERPC_EXCP_PRIV_REG = 0x02,
    POWERPC_EXCP_TRAP = 0x40,
};
typedef enum powerpc_excp_t {
    POWERPC_EXCP_UNKNOWN = 0,
    POWERPC_EXCP_STD,
    POWERPC_EXCP_40x,
    POWERPC_EXCP_6xx,
    POWERPC_EXCP_7xx,
    POWERPC_EXCP_74xx,
    POWERPC_EXCP_BOOKE,
    POWERPC_EXCP_970,
    POWERPC_EXCP_POWER7,
    POWERPC_EXCP_POWER8,
    POWERPC_EXCP_POWER9,
    POWERPC_EXCP_POWER10,
    POWERPC_EXCP_POWER11,
} powerpc_excp_t;
typedef enum powerpc_mmu_t {
    POWERPC_MMU_UNKNOWN = 0x00000000,
    POWERPC_MMU_32B = 0x00000001,
    POWERPC_MMU_SOFT_6xx = 0x00000002,
    POWERPC_MMU_SOFT_74xx = 0x00000003,
    POWERPC_MMU_SOFT_4xx = 0x00000004,
    POWERPC_MMU_REAL = 0x00000006,
    POWERPC_MMU_MPC8xx = 0x00000007,
    POWERPC_MMU_BOOKE = 0x00000008,
    POWERPC_MMU_BOOKE206 = 0x00000009,
    POWERPC_MMU_64B = 0x00010000 | 0x00000001,
    POWERPC_MMU_2_03 = 0x00010000 | 0x00000002,
    POWERPC_MMU_2_06 = 0x00010000 | 0x00000003,
    POWERPC_MMU_2_07 = 0x00010000 | 0x00000004,
    POWERPC_MMU_3_00 = 0x00010000 | 0x00000005,
} powerpc_mmu_t;
typedef enum powerpc_input_t {
    PPC_FLAGS_INPUT_UNKNOWN = 0,
    PPC_FLAGS_INPUT_6xx,
    PPC_FLAGS_INPUT_BookE,
    PPC_FLAGS_INPUT_405,
    PPC_FLAGS_INPUT_970,
    PPC_FLAGS_INPUT_POWER7,
    PPC_FLAGS_INPUT_POWER9,
    PPC_FLAGS_INPUT_RCPU,
} powerpc_input_t;
typedef struct opc_handler_t opc_handler_t;
typedef struct DisasContext DisasContext;
typedef struct ppc_dcr_t ppc_dcr_t;
typedef struct ppc_spr_t ppc_spr_t;
typedef struct ppc_tb_t ppc_tb_t;
typedef union ppc_tlb_t ppc_tlb_t;
typedef struct ppc_hash_pte64 ppc_hash_pte64_t;
typedef struct PPCHash64Options PPCHash64Options;
typedef struct CPUArchState CPUPPCState;
struct ppc_spr_t {
    const char *name;
    target_ulong default_value;
    unsigned int gdb_id;
    void (*uea_read)(DisasContext *ctx, int gpr_num, int spr_num);
    void (*uea_write)(DisasContext *ctx, int spr_num, int gpr_num);
    void (*oea_read)(DisasContext *ctx, int gpr_num, int spr_num);
    void (*oea_write)(DisasContext *ctx, int spr_num, int gpr_num);
    void (*hea_read)(DisasContext *ctx, int gpr_num, int spr_num);
    void (*hea_write)(DisasContext *ctx, int spr_num, int gpr_num);
};
typedef union _ppc_vsr_t {
    uint8_t u8[16];
    uint16_t u16[8];
    uint32_t u32[4];
    uint64_t u64[2];
    int8_t s8[16];
    int16_t s16[8];
    int32_t s32[4];
    int64_t s64[2];
    float16 f16[8];
    float32 f32[4];
    float64 f64[2];
    float128 f128;
    int u128;
    Int128 s128;
} ppc_vsr_t;
typedef ppc_vsr_t ppc_avr_t;
typedef ppc_vsr_t ppc_fprp_t;
typedef ppc_vsr_t ppc_acc_t;
typedef struct ppc6xx_tlb_t ppc6xx_tlb_t;
struct ppc6xx_tlb_t {
    target_ulong pte0;
    target_ulong pte1;
    target_ulong EPN;
};
typedef struct ppcemb_tlb_t ppcemb_tlb_t;
struct ppcemb_tlb_t {
    uint64_t RPN;
    target_ulong EPN;
    target_ulong PID;
    target_ulong size;
    uint32_t prot;
    uint32_t attr;
};
typedef struct ppcmas_tlb_t {
     uint32_t mas8;
     uint32_t mas1;
     uint64_t mas2;
     uint64_t mas7_3;
} ppcmas_tlb_t;
union ppc_tlb_t {
    ppc6xx_tlb_t *tlb6;
    ppcemb_tlb_t *tlbe;
    ppcmas_tlb_t *tlbm;
};
typedef struct PPCHash64SegmentPageSizes PPCHash64SegmentPageSizes;
typedef struct ppc_slb_t ppc_slb_t;
struct ppc_slb_t {
    uint64_t esid;
    uint64_t vsid;
    const PPCHash64SegmentPageSizes *sps;
};
typedef struct ppc_v3_pate_t {
    uint64_t dw0;
    uint64_t dw1;
} ppc_v3_pate_t;
typedef enum {
    PMU_EVENT_INVALID = 0,
    PMU_EVENT_INACTIVE,
    PMU_EVENT_CYCLES,
    PMU_EVENT_INSTRUCTIONS,
    PMU_EVENT_INSN_RUN_LATCH,
} PMUEventType;
enum { R_MSR_SF_SHIFT = 63}; enum { R_MSR_SF_LENGTH = 1}; enum { R_MSR_SF_MASK = 9223372036854775808};
enum { R_MSR_TAG_SHIFT = 62}; enum { R_MSR_TAG_LENGTH = 1}; enum { R_MSR_TAG_MASK = 4611686018427387904};
enum { R_MSR_ISF_SHIFT = 61}; enum { R_MSR_ISF_LENGTH = 1}; enum { R_MSR_ISF_MASK = 2305843009213693952};
enum { R_MSR_TS0_SHIFT = 34}; enum { R_MSR_TS0_LENGTH = 1}; enum { R_MSR_TS0_MASK = 17179869184};
enum { R_MSR_TS1_SHIFT = 33}; enum { R_MSR_TS1_LENGTH = 1}; enum { R_MSR_TS1_MASK = 8589934592};
enum { R_MSR_TS_SHIFT = 34}; enum { R_MSR_TS_LENGTH = 2}; enum { R_MSR_TS_MASK = 51539607552};
enum { R_MSR_TM_SHIFT = 32}; enum { R_MSR_TM_LENGTH = 1}; enum { R_MSR_TM_MASK = 4294967296};
enum { R_MSR_CM_SHIFT = 31}; enum { R_MSR_CM_LENGTH = 1}; enum { R_MSR_CM_MASK = 2147483648};
enum { R_MSR_ICM_SHIFT = 30}; enum { R_MSR_ICM_LENGTH = 1}; enum { R_MSR_ICM_MASK = 1073741824};
enum { R_MSR_GS_SHIFT = 28}; enum { R_MSR_GS_LENGTH = 1}; enum { R_MSR_GS_MASK = 268435456};
enum { R_MSR_UCLE_SHIFT = 26}; enum { R_MSR_UCLE_LENGTH = 1}; enum { R_MSR_UCLE_MASK = 67108864};
enum { R_MSR_VR_SHIFT = 25}; enum { R_MSR_VR_LENGTH = 1}; enum { R_MSR_VR_MASK = 33554432};
enum { R_MSR_SPE_SHIFT = 25}; enum { R_MSR_SPE_LENGTH = 1}; enum { R_MSR_SPE_MASK = 33554432};
enum { R_MSR_VSX_SHIFT = 23}; enum { R_MSR_VSX_LENGTH = 1}; enum { R_MSR_VSX_MASK = 8388608};
enum { R_MSR_S_SHIFT = 22}; enum { R_MSR_S_LENGTH = 1}; enum { R_MSR_S_MASK = 4194304};
enum { R_MSR_KEY_SHIFT = 19}; enum { R_MSR_KEY_LENGTH = 1}; enum { R_MSR_KEY_MASK = 524288};
enum { R_MSR_POW_SHIFT = 18}; enum { R_MSR_POW_LENGTH = 1}; enum { R_MSR_POW_MASK = 262144};
enum { R_MSR_WE_SHIFT = 18}; enum { R_MSR_WE_LENGTH = 1}; enum { R_MSR_WE_MASK = 262144};
enum { R_MSR_TGPR_SHIFT = 17}; enum { R_MSR_TGPR_LENGTH = 1}; enum { R_MSR_TGPR_MASK = 131072};
enum { R_MSR_CE_SHIFT = 17}; enum { R_MSR_CE_LENGTH = 1}; enum { R_MSR_CE_MASK = 131072};
enum { R_MSR_ILE_SHIFT = 16}; enum { R_MSR_ILE_LENGTH = 1}; enum { R_MSR_ILE_MASK = 65536};
enum { R_MSR_EE_SHIFT = 15}; enum { R_MSR_EE_LENGTH = 1}; enum { R_MSR_EE_MASK = 32768};
enum { R_MSR_PR_SHIFT = 14}; enum { R_MSR_PR_LENGTH = 1}; enum { R_MSR_PR_MASK = 16384};
enum { R_MSR_FP_SHIFT = 13}; enum { R_MSR_FP_LENGTH = 1}; enum { R_MSR_FP_MASK = 8192};
enum { R_MSR_ME_SHIFT = 12}; enum { R_MSR_ME_LENGTH = 1}; enum { R_MSR_ME_MASK = 4096};
enum { R_MSR_FE0_SHIFT = 11}; enum { R_MSR_FE0_LENGTH = 1}; enum { R_MSR_FE0_MASK = 2048};
enum { R_MSR_SE_SHIFT = 10}; enum { R_MSR_SE_LENGTH = 1}; enum { R_MSR_SE_MASK = 1024};
enum { R_MSR_DWE_SHIFT = 10}; enum { R_MSR_DWE_LENGTH = 1}; enum { R_MSR_DWE_MASK = 1024};
enum { R_MSR_UBLE_SHIFT = 10}; enum { R_MSR_UBLE_LENGTH = 1}; enum { R_MSR_UBLE_MASK = 1024};
enum { R_MSR_BE_SHIFT = 9}; enum { R_MSR_BE_LENGTH = 1}; enum { R_MSR_BE_MASK = 512};
enum { R_MSR_DE_SHIFT = 9}; enum { R_MSR_DE_LENGTH = 1}; enum { R_MSR_DE_MASK = 512};
enum { R_MSR_FE1_SHIFT = 8}; enum { R_MSR_FE1_LENGTH = 1}; enum { R_MSR_FE1_MASK = 256};
enum { R_MSR_AL_SHIFT = 7}; enum { R_MSR_AL_LENGTH = 1}; enum { R_MSR_AL_MASK = 128};
enum { R_MSR_EP_SHIFT = 6}; enum { R_MSR_EP_LENGTH = 1}; enum { R_MSR_EP_MASK = 64};
enum { R_MSR_IR_SHIFT = 5}; enum { R_MSR_IR_LENGTH = 1}; enum { R_MSR_IR_MASK = 32};
enum { R_MSR_DR_SHIFT = 4}; enum { R_MSR_DR_LENGTH = 1}; enum { R_MSR_DR_MASK = 16};
enum { R_MSR_IS_SHIFT = 5}; enum { R_MSR_IS_LENGTH = 1}; enum { R_MSR_IS_MASK = 32};
enum { R_MSR_DS_SHIFT = 4}; enum { R_MSR_DS_LENGTH = 1}; enum { R_MSR_DS_MASK = 16};
enum { R_MSR_PE_SHIFT = 3}; enum { R_MSR_PE_LENGTH = 1}; enum { R_MSR_PE_MASK = 8};
enum { R_MSR_PX_SHIFT = 2}; enum { R_MSR_PX_LENGTH = 1}; enum { R_MSR_PX_MASK = 4};
enum { R_MSR_PMM_SHIFT = 2}; enum { R_MSR_PMM_LENGTH = 1}; enum { R_MSR_PMM_MASK = 4};
enum { R_MSR_RI_SHIFT = 1}; enum { R_MSR_RI_LENGTH = 1}; enum { R_MSR_RI_MASK = 2};
enum { R_MSR_LE_SHIFT = 0}; enum { R_MSR_LE_LENGTH = 1}; enum { R_MSR_LE_MASK = 1};
enum {
    POWERPC_FLAG_NONE = 0x00000000,
    POWERPC_FLAG_SPE = 0x00000001,
    POWERPC_FLAG_VRE = 0x00000002,
    POWERPC_FLAG_TGPR = 0x00000004,
    POWERPC_FLAG_CE = 0x00000008,
    POWERPC_FLAG_SE = 0x00000010,
    POWERPC_FLAG_DWE = 0x00000020,
    POWERPC_FLAG_UBLE = 0x00000040,
    POWERPC_FLAG_BE = 0x00000080,
    POWERPC_FLAG_DE = 0x00000100,
    POWERPC_FLAG_PX = 0x00000200,
    POWERPC_FLAG_PMM = 0x00000400,
    POWERPC_FLAG_BUS_CLK = 0x00020000,
    POWERPC_FLAG_CFAR = 0x00040000,
    POWERPC_FLAG_VSX = 0x00080000,
    POWERPC_FLAG_TM = 0x00100000,
    POWERPC_FLAG_SCV = 0x00200000,
    POWERPC_FLAG_SMT = 0x00400000,
    POWERPC_FLAG_SMT_1LPAR = 0x00800000,
    POWERPC_FLAG_BHRB = 0x01000000,
};
enum {
    HFLAGS_LE = 0,
    HFLAGS_HV = 1,
    HFLAGS_64 = 2,
    HFLAGS_GTSE = 3,
    HFLAGS_DR = 4,
    HFLAGS_HR = 5,
    HFLAGS_SPE = 6,
    HFLAGS_TM = 8,
    HFLAGS_BE = 9,
    HFLAGS_SE = 10,
    HFLAGS_FP = 13,
    HFLAGS_PR = 14,
    HFLAGS_PMCC0 = 15,
    HFLAGS_PMCC1 = 16,
    HFLAGS_PMCJCE = 17,
    HFLAGS_PMC_OTHER = 18,
    HFLAGS_INSN_CNT = 19,
    HFLAGS_BHRB_ENABLE = 20,
    HFLAGS_VSX = 23,
    HFLAGS_VR = 25,
    HFLAGS_IMMU_IDX = 26,
    HFLAGS_DMMU_IDX = 29,
};
enum { R_FPSCR_FI_SHIFT = 17}; enum { R_FPSCR_FI_LENGTH = 1}; enum { R_FPSCR_FI_MASK = 131072};
struct ppc_radix_page_info {
    uint32_t count;
    uint32_t entries[8];
};
enum { R_DEXCR_PNH_SBHE_SHIFT = 63}; enum { R_DEXCR_PNH_SBHE_LENGTH = 1}; enum { R_DEXCR_PNH_SBHE_MASK = 9223372036854775808}; enum { R_DEXCR_PRO_SBHE_SHIFT = 31}; enum { R_DEXCR_PRO_SBHE_LENGTH = 1}; enum { R_DEXCR_PRO_SBHE_MASK = 2147483648}; enum { R_HDEXCR_HNU_SBHE_SHIFT = 63}; enum { R_HDEXCR_HNU_SBHE_LENGTH = 1}; enum { R_HDEXCR_HNU_SBHE_MASK = 9223372036854775808}; enum { R_HDEXCR_ENF_SBHE_SHIFT = 31}; enum { R_HDEXCR_ENF_SBHE_LENGTH = 1}; enum { R_HDEXCR_ENF_SBHE_MASK = 2147483648};
enum { R_DEXCR_PNH_IBRTPD_SHIFT = 62}; enum { R_DEXCR_PNH_IBRTPD_LENGTH = 1}; enum { R_DEXCR_PNH_IBRTPD_MASK = 4611686018427387904}; enum { R_DEXCR_PRO_IBRTPD_SHIFT = 30}; enum { R_DEXCR_PRO_IBRTPD_LENGTH = 1}; enum { R_DEXCR_PRO_IBRTPD_MASK = 1073741824}; enum { R_HDEXCR_HNU_IBRTPD_SHIFT = 62}; enum { R_HDEXCR_HNU_IBRTPD_LENGTH = 1}; enum { R_HDEXCR_HNU_IBRTPD_MASK = 4611686018427387904}; enum { R_HDEXCR_ENF_IBRTPD_SHIFT = 30}; enum { R_HDEXCR_ENF_IBRTPD_LENGTH = 1}; enum { R_HDEXCR_ENF_IBRTPD_MASK = 1073741824};
enum { R_DEXCR_PNH_SRAPD_SHIFT = 59}; enum { R_DEXCR_PNH_SRAPD_LENGTH = 1}; enum { R_DEXCR_PNH_SRAPD_MASK = 576460752303423488}; enum { R_DEXCR_PRO_SRAPD_SHIFT = 27}; enum { R_DEXCR_PRO_SRAPD_LENGTH = 1}; enum { R_DEXCR_PRO_SRAPD_MASK = 134217728}; enum { R_HDEXCR_HNU_SRAPD_SHIFT = 59}; enum { R_HDEXCR_HNU_SRAPD_LENGTH = 1}; enum { R_HDEXCR_HNU_SRAPD_MASK = 576460752303423488}; enum { R_HDEXCR_ENF_SRAPD_SHIFT = 27}; enum { R_HDEXCR_ENF_SRAPD_LENGTH = 1}; enum { R_HDEXCR_ENF_SRAPD_MASK = 134217728};
enum { R_DEXCR_PNH_NPHIE_SHIFT = 58}; enum { R_DEXCR_PNH_NPHIE_LENGTH = 1}; enum { R_DEXCR_PNH_NPHIE_MASK = 288230376151711744}; enum { R_DEXCR_PRO_NPHIE_SHIFT = 26}; enum { R_DEXCR_PRO_NPHIE_LENGTH = 1}; enum { R_DEXCR_PRO_NPHIE_MASK = 67108864}; enum { R_HDEXCR_HNU_NPHIE_SHIFT = 58}; enum { R_HDEXCR_HNU_NPHIE_LENGTH = 1}; enum { R_HDEXCR_HNU_NPHIE_MASK = 288230376151711744}; enum { R_HDEXCR_ENF_NPHIE_SHIFT = 26}; enum { R_HDEXCR_ENF_NPHIE_LENGTH = 1}; enum { R_HDEXCR_ENF_NPHIE_MASK = 67108864};
enum { R_DEXCR_PNH_PHIE_SHIFT = 57}; enum { R_DEXCR_PNH_PHIE_LENGTH = 1}; enum { R_DEXCR_PNH_PHIE_MASK = 144115188075855872}; enum { R_DEXCR_PRO_PHIE_SHIFT = 25}; enum { R_DEXCR_PRO_PHIE_LENGTH = 1}; enum { R_DEXCR_PRO_PHIE_MASK = 33554432}; enum { R_HDEXCR_HNU_PHIE_SHIFT = 57}; enum { R_HDEXCR_HNU_PHIE_LENGTH = 1}; enum { R_HDEXCR_HNU_PHIE_MASK = 144115188075855872}; enum { R_HDEXCR_ENF_PHIE_SHIFT = 25}; enum { R_HDEXCR_ENF_PHIE_LENGTH = 1}; enum { R_HDEXCR_ENF_PHIE_MASK = 33554432};
struct CPUArchState {
    target_ulong gpr[32];
    target_ulong gprh[32];
    target_ulong lr;
    target_ulong ctr;
    uint32_t crf[8];
    target_ulong xer;
    target_ulong so;
    target_ulong ov;
    target_ulong ca;
    target_ulong ov32;
    target_ulong ca32;
    target_ulong reserve_addr;
    target_ulong reserve_length;
    target_ulong reserve_val;
    target_ulong msr;
    target_ulong tgpr[4];
    target_ulong nip;
    int access_type;
    
   _Bool 
        has_smt_siblings;
    int core_index;
    int chip_index;
    target_ulong sr[32];
    uint32_t nb_BATs;
    target_ulong DBAT[2][8];
    target_ulong IBAT[2][8];
    int32_t nb_tlb;
    int tlb_per_way;
    int nb_ways;
    int last_way;
    int nb_pids;
    int tlb_type;
    ppc_tlb_t tlb;
    uint32_t tlb_need_flush;
    target_ulong spr[1024];
    ppc_spr_t spr_cb[1024];
    uint8_t pmc_ins_cnt;
    uint8_t pmc_cyc_cnt;
    uint32_t vscr;
    ppc_vsr_t vsr[64] ;
    ppc_vsr_t vscr_sat ;
    uint64_t spe_acc;
    uint32_t spe_fscr;
    float_status vec_status;
    float_status fp_status;
    target_ulong fpscr;
    ppc_tb_t *tb_env;
    ppc_dcr_t *dcr_env;
    int dcache_line_size;
    int icache_line_size;
    target_ulong msr_mask;
    powerpc_mmu_t mmu_model;
    powerpc_excp_t excp_model;
    powerpc_input_t bus_model;
    int bfd_mach;
    uint32_t flags;
    uint64_t insns_flags;
    uint64_t insns_flags2;
    int error_code;
    uint32_t pending_interrupts;
    uint64_t excp_stats[POWERPC_EXCP_NB];
    uint32_t irq_input_state;
    target_ulong excp_vectors[POWERPC_EXCP_NB];
    target_ulong excp_prefix;
    target_ulong ivor_mask;
    target_ulong ivpr_mask;
    target_ulong hreset_vector;
    hwaddr mpic_iack;
    
   _Bool 
        mpic_proxy;
    
   _Bool 
        has_hv_mode;
    
   _Bool 
        resume_as_sreset;
    
   _Bool 
        quiesced;
    uint32_t hflags;
    target_ulong hflags_compat_nmsr;
    int (*check_pow)(CPUPPCState *env);
    int (*check_attn)(CPUPPCState *env);
    void *load_info;
    uint8_t fit_period[4];
    uint8_t wdt_period[4];
    target_ulong tm_gpr[32];
    ppc_avr_t tm_vsr[64];
    uint64_t tm_cr;
    uint64_t tm_lr;
    uint64_t tm_ctr;
    uint64_t tm_fpscr;
    uint64_t tm_amr;
    uint64_t tm_ppr;
    uint64_t tm_vrsave;
    uint32_t tm_vscr;
    uint64_t tm_dscr;
    uint64_t tm_tar;
    QEMUTimer *pmu_cyc_overflow_timers[6];
    uint64_t pmu_base_time;
};
typedef struct PPCVirtualHypervisor PPCVirtualHypervisor;
typedef struct PPCVirtualHypervisorClass PPCVirtualHypervisorClass;
struct ArchCPU {
    CPUState parent_obj;
    CPUPPCState env;
    int vcpu_id;
    uint32_t compat_pvr;
    PPCVirtualHypervisor *vhyp;
    PPCVirtualHypervisorClass *vhyp_class;
    void *machine_data;
    int32_t node_id;
    PPCHash64Options *hash64_opts;
    opc_handler_t *opcodes[0x40];
};
struct PowerPCCPUClass {
    CPUClass parent_class;
    DeviceRealize parent_realize;
    DeviceUnrealize parent_unrealize;
    ResettablePhases parent_phases;
    void (*parent_parse_features)(const char *type, char *str, Error **errp);
    uint32_t pvr;
    uint32_t spapr_logical_pvr;
    
   _Bool 
        (*pvr_match)(struct PowerPCCPUClass *pcc, uint32_t pvr, 
                                                                _Bool 
                                                                     best);
    uint64_t pcr_mask;
    uint64_t pcr_supported;
    uint32_t svr;
    uint64_t insns_flags;
    uint64_t insns_flags2;
    uint64_t msr_mask;
    uint64_t lpcr_mask;
    uint64_t lpcr_pm;
    powerpc_mmu_t mmu_model;
    powerpc_excp_t excp_model;
    powerpc_input_t bus_model;
    uint32_t flags;
    int bfd_mach;
    uint32_t l1_dcache_size, l1_icache_size;
    GDBFeature gdb_spr;
    const PPCHash64Options *hash64_opts;
    struct ppc_radix_page_info *radix_page_info;
    uint32_t lrg_decr_bits;
    int n_host_threads;
    void (*init_proc)(CPUPPCState *env);
    int (*check_pow)(CPUPPCState *env);
    int (*check_attn)(CPUPPCState *env);
};
ObjectClass *ppc_cpu_class_by_name(const char *name);
PowerPCCPUClass *ppc_cpu_class_by_pvr(uint32_t pvr);
PowerPCCPUClass *ppc_cpu_class_by_pvr_mask(uint32_t pvr);
PowerPCCPUClass *ppc_cpu_get_family_class(PowerPCCPUClass *pcc);
struct PPCVirtualHypervisorClass {
    InterfaceClass parent;
    
   _Bool 
        (*cpu_in_nested)(PowerPCCPU *cpu);
    void (*deliver_hv_excp)(PowerPCCPU *cpu, int excp);
    void (*hypercall)(PPCVirtualHypervisor *vhyp, PowerPCCPU *cpu);
    hwaddr (*hpt_mask)(PPCVirtualHypervisor *vhyp);
    const ppc_hash_pte64_t *(*map_hptes)(PPCVirtualHypervisor *vhyp,
                                         hwaddr ptex, int n);
    void (*unmap_hptes)(PPCVirtualHypervisor *vhyp,
                        const ppc_hash_pte64_t *hptes,
                        hwaddr ptex, int n);
    void (*hpte_set_c)(PPCVirtualHypervisor *vhyp, hwaddr ptex, uint64_t pte1);
    void (*hpte_set_r)(PPCVirtualHypervisor *vhyp, hwaddr ptex, uint64_t pte1);
    
   _Bool 
        (*get_pate)(PPCVirtualHypervisor *vhyp, PowerPCCPU *cpu,
                     target_ulong lpid, ppc_v3_pate_t *entry);
    target_ulong (*encode_hpt_for_kvm_pr)(PPCVirtualHypervisor *vhyp);
    void (*cpu_exec_enter)(PPCVirtualHypervisor *vhyp, PowerPCCPU *cpu);
    void (*cpu_exec_exit)(PPCVirtualHypervisor *vhyp, PowerPCCPU *cpu);
};
void ppc_cpu_dump_state(CPUState *cpu, FILE *f, int flags);
int ppc_cpu_gdb_read_register(CPUState *cpu, GByteArray *buf, int reg);
int ppc_cpu_gdb_read_register_apple(CPUState *cpu, GByteArray *buf, int reg);
int ppc_cpu_gdb_write_register(CPUState *cpu, uint8_t *buf, int reg);
int ppc_cpu_gdb_write_register_apple(CPUState *cpu, uint8_t *buf, int reg);
hwaddr ppc_cpu_get_phys_page_debug(CPUState *cpu, vaddr addr);
int ppc64_cpu_write_elf64_note(WriteCoreDumpFunction f, CPUState *cs,
                               int cpuid, DumpState *s);
int ppc32_cpu_write_elf32_note(WriteCoreDumpFunction f, CPUState *cs,
                               int cpuid, DumpState *s);
void ppc_maybe_interrupt(CPUPPCState *env);
void ppc_cpu_do_interrupt(CPUState *cpu);
_Bool 
    ppc_cpu_exec_interrupt(CPUState *cpu, int int_req);
void ppc_cpu_do_system_reset(CPUState *cs);
void ppc_cpu_do_fwnmi_machine_check(CPUState *cs, target_ulong vector);
extern const VMStateDescription vmstate_ppc_cpu;
void ppc_translate_init(void);
void ppc_translate_code(CPUState *cs, TranslationBlock *tb,
                        int *max_insns, vaddr pc, void *host_pc);
void ppc_store_sdr1(CPUPPCState *env, target_ulong value);
void ppc_store_lpcr(PowerPCCPU *cpu, target_ulong val);
void ppc_update_ciabr(CPUPPCState *env);
void ppc_store_ciabr(CPUPPCState *env, target_ulong value);
void ppc_update_daw0(CPUPPCState *env);
void ppc_store_dawr0(CPUPPCState *env, target_ulong value);
void ppc_store_dawrx0(CPUPPCState *env, uint32_t value);
void ppc_store_msr(CPUPPCState *env, target_ulong value);
void ppc_cpu_list(void);
uint64_t cpu_ppc_load_tbl(CPUPPCState *env);
uint32_t cpu_ppc_load_tbu(CPUPPCState *env);
void cpu_ppc_store_tbu(CPUPPCState *env, uint32_t value);
void cpu_ppc_store_tbl(CPUPPCState *env, uint32_t value);
uint64_t cpu_ppc_load_atbl(CPUPPCState *env);
uint32_t cpu_ppc_load_atbu(CPUPPCState *env);
void cpu_ppc_store_atbl(CPUPPCState *env, uint32_t value);
void cpu_ppc_store_atbu(CPUPPCState *env, uint32_t value);
void cpu_ppc_increase_tb_by_offset(CPUPPCState *env, int64_t offset);
void cpu_ppc_decrease_tb_by_offset(CPUPPCState *env, int64_t offset);
uint64_t cpu_ppc_load_vtb(CPUPPCState *env);
void cpu_ppc_store_vtb(CPUPPCState *env, uint64_t value);
_Bool 
    ppc_decr_clear_on_delivery(CPUPPCState *env);
target_ulong cpu_ppc_load_decr(CPUPPCState *env);
void cpu_ppc_store_decr(CPUPPCState *env, target_ulong value);
target_ulong cpu_ppc_load_hdecr(CPUPPCState *env);
void cpu_ppc_store_hdecr(CPUPPCState *env, target_ulong value);
void cpu_ppc_store_tbu40(CPUPPCState *env, uint64_t value);
uint64_t cpu_ppc_load_purr(CPUPPCState *env);
void cpu_ppc_store_purr(CPUPPCState *env, uint64_t value);
target_ulong load_40x_pit(CPUPPCState *env);
void store_40x_pit(CPUPPCState *env, target_ulong val);
void store_40x_dbcr0(CPUPPCState *env, uint32_t val);
void store_40x_sler(CPUPPCState *env, uint32_t val);
void store_40x_tcr(CPUPPCState *env, target_ulong val);
void store_40x_tsr(CPUPPCState *env, target_ulong val);
void store_booke_tcr(CPUPPCState *env, target_ulong val);
void store_booke_tsr(CPUPPCState *env, target_ulong val);
void ppc_tlb_invalidate_all(CPUPPCState *env);
void ppc_tlb_invalidate_one(CPUPPCState *env, target_ulong addr);
void cpu_ppc_set_vhyp(PowerPCCPU *cpu, PPCVirtualHypervisor *vhyp);
void cpu_ppc_set_1lpar(PowerPCCPU *cpu);
void ppc_store_fpscr(CPUPPCState *env, target_ulong val);
void helper_hfscr_facility_check(CPUPPCState *env, uint32_t bit,
                                 const char *caller, uint32_t cause);
int ppc_dcr_read(ppc_dcr_t *dcr_env, int dcrn, uint32_t *valp);
int ppc_dcr_write(ppc_dcr_t *dcr_env, int dcrn, uint32_t val);
typedef struct RAMBlockNotifier RAMBlockNotifier;
typedef struct {
    struct rcu_head rcu;
    unsigned long *blocks[];
} DirtyMemoryBlocks;
typedef struct RAMList {
    QemuMutex mutex;
    RAMBlock *mru_block;
    struct { struct RAMBlock *lh_first; } blocks;
    DirtyMemoryBlocks *dirty_memory[3];
    unsigned int num_dirty_blocks;
    uint32_t version;
    struct { struct RAMBlockNotifier *lh_first; } ramblock_notifiers;
} RAMList;
extern RAMList ram_list;
void qemu_mutex_lock_ramlist(void);
void qemu_mutex_unlock_ramlist(void);
struct RAMBlockNotifier {
    void (*ram_block_added)(RAMBlockNotifier *n, void *host, size_t size,
                            size_t max_size);
    void (*ram_block_removed)(RAMBlockNotifier *n, void *host, size_t size,
                              size_t max_size);
    void (*ram_block_resized)(RAMBlockNotifier *n, void *host, size_t old_size,
                              size_t new_size);
    struct { struct RAMBlockNotifier *le_next; struct RAMBlockNotifier **le_prev; } next;
};
void ram_block_notifier_add(RAMBlockNotifier *n);
void ram_block_notifier_remove(RAMBlockNotifier *n);
void ram_block_notify_add(void *host, size_t size, size_t max_size);
void ram_block_notify_remove(void *host, size_t size, size_t max_size);
void ram_block_notify_resize(void *host, size_t old_size, size_t new_size);
GString *ram_block_format(void);
struct Range {
    uint64_t lob;
    uint64_t upb;
};
int range_compare(Range *a, Range *b);
GList *range_list_insert(GList *list, Range *data);
void range_inverse_array(GList *in_ranges,
                         GList **out_ranges,
                         uint64_t low, uint64_t high);
typedef struct IOMMUMemoryRegionClass IOMMUMemoryRegionClass;
typedef struct RamDiscardManagerClass RamDiscardManagerClass;
typedef struct RamDiscardManager RamDiscardManager;
                                                                   ;
extern unsigned int global_dirty_tracking;
typedef struct MemoryRegionOps MemoryRegionOps;
struct ReservedRegion {
    Range range;
    unsigned type;
};
struct MemoryRegionSection {
    Int128 size;
    MemoryRegion *mr;
    FlatView *fv;
    hwaddr offset_within_region;
    hwaddr offset_within_address_space;
    
   _Bool 
        readonly;
    
   _Bool 
        nonvolatile;
    
   _Bool 
        unmergeable;
};
typedef struct IOMMUTLBEntry IOMMUTLBEntry;
typedef enum {
    IOMMU_NONE = 0,
    IOMMU_RO = 1,
    IOMMU_WO = 2,
    IOMMU_RW = 3,
} IOMMUAccessFlags;
struct IOMMUTLBEntry {
    AddressSpace *target_as;
    hwaddr iova;
    hwaddr translated_addr;
    hwaddr addr_mask;
    IOMMUAccessFlags perm;
};
typedef enum {
    IOMMU_NOTIFIER_NONE = 0,
    IOMMU_NOTIFIER_UNMAP = 0x1,
    IOMMU_NOTIFIER_MAP = 0x2,
    IOMMU_NOTIFIER_DEVIOTLB_UNMAP = 0x04,
} IOMMUNotifierFlag;
struct IOMMUNotifier;
typedef void (*IOMMUNotify)(struct IOMMUNotifier *notifier,
                            IOMMUTLBEntry *data);
struct IOMMUNotifier {
    IOMMUNotify notify;
    IOMMUNotifierFlag notifier_flags;
    hwaddr start;
    hwaddr end;
    int iommu_idx;
    struct { struct IOMMUNotifier *le_next; struct IOMMUNotifier **le_prev; } node;
};
typedef struct IOMMUNotifier IOMMUNotifier;
typedef struct IOMMUTLBEvent {
    IOMMUNotifierFlag type;
    IOMMUTLBEntry entry;
} IOMMUTLBEvent;
struct MemoryRegionOps {
    uint64_t (*read)(void *opaque,
                     hwaddr addr,
                     unsigned size);
    void (*write)(void *opaque,
                  hwaddr addr,
                  uint64_t data,
                  unsigned size);
    MemTxResult (*read_with_attrs)(void *opaque,
                                   hwaddr addr,
                                   uint64_t *data,
                                   unsigned size,
                                   MemTxAttrs attrs);
    MemTxResult (*write_with_attrs)(void *opaque,
                                    hwaddr addr,
                                    uint64_t data,
                                    unsigned size,
                                    MemTxAttrs attrs);
    enum device_endian endianness;
    struct {
        unsigned min_access_size;
        unsigned max_access_size;
         
        _Bool 
             unaligned;
        
       _Bool 
            (*accepts)(void *opaque, hwaddr addr,
                        unsigned size, 
                                      _Bool 
                                           is_write,
                        MemTxAttrs attrs);
    } valid;
    struct {
        unsigned min_access_size;
        unsigned max_access_size;
        
       _Bool 
            unaligned;
    } impl;
};
typedef struct MemoryRegionClass {
    ObjectClass parent_class;
} MemoryRegionClass;
enum IOMMUMemoryRegionAttr {
    IOMMU_ATTR_SPAPR_TCE_FD
};
struct IOMMUMemoryRegionClass {
    MemoryRegionClass parent_class;
    IOMMUTLBEntry (*translate)(IOMMUMemoryRegion *iommu, hwaddr addr,
                               IOMMUAccessFlags flag, int iommu_idx);
    uint64_t (*get_min_page_size)(IOMMUMemoryRegion *iommu);
    int (*notify_flag_changed)(IOMMUMemoryRegion *iommu,
                               IOMMUNotifierFlag old_flags,
                               IOMMUNotifierFlag new_flags,
                               Error **errp);
    void (*replay)(IOMMUMemoryRegion *iommu, IOMMUNotifier *notifier);
    int (*get_attr)(IOMMUMemoryRegion *iommu, enum IOMMUMemoryRegionAttr attr,
                    void *data);
    int (*attrs_to_index)(IOMMUMemoryRegion *iommu, MemTxAttrs attrs);
    int (*num_indexes)(IOMMUMemoryRegion *iommu);
};
typedef struct RamDiscardListener RamDiscardListener;
typedef int (*NotifyRamPopulate)(RamDiscardListener *rdl,
                                 MemoryRegionSection *section);
typedef void (*NotifyRamDiscard)(RamDiscardListener *rdl,
                                 MemoryRegionSection *section);
struct RamDiscardListener {
    NotifyRamPopulate notify_populate;
    NotifyRamDiscard notify_discard;
    
   _Bool 
        double_discard_supported;
    MemoryRegionSection *section;
    struct { struct RamDiscardListener *le_next; struct RamDiscardListener **le_prev; } next;
};
typedef int (*ReplayRamPopulate)(MemoryRegionSection *section, void *opaque);
typedef void (*ReplayRamDiscard)(MemoryRegionSection *section, void *opaque);
struct RamDiscardManagerClass {
    InterfaceClass parent_class;
    uint64_t (*get_min_granularity)(const RamDiscardManager *rdm,
                                    const MemoryRegion *mr);
    
   _Bool 
        (*is_populated)(const RamDiscardManager *rdm,
                         const MemoryRegionSection *section);
    int (*replay_populated)(const RamDiscardManager *rdm,
                            MemoryRegionSection *section,
                            ReplayRamPopulate replay_fn, void *opaque);
    void (*replay_discarded)(const RamDiscardManager *rdm,
                             MemoryRegionSection *section,
                             ReplayRamDiscard replay_fn, void *opaque);
    void (*register_listener)(RamDiscardManager *rdm,
                              RamDiscardListener *rdl,
                              MemoryRegionSection *section);
    void (*unregister_listener)(RamDiscardManager *rdm,
                                RamDiscardListener *rdl);
};
uint64_t ram_discard_manager_get_min_granularity(const RamDiscardManager *rdm,
                                                 const MemoryRegion *mr);
_Bool 
    ram_discard_manager_is_populated(const RamDiscardManager *rdm,
                                      const MemoryRegionSection *section);
int ram_discard_manager_replay_populated(const RamDiscardManager *rdm,
                                         MemoryRegionSection *section,
                                         ReplayRamPopulate replay_fn,
                                         void *opaque);
void ram_discard_manager_replay_discarded(const RamDiscardManager *rdm,
                                          MemoryRegionSection *section,
                                          ReplayRamDiscard replay_fn,
                                          void *opaque);
void ram_discard_manager_register_listener(RamDiscardManager *rdm,
                                           RamDiscardListener *rdl,
                                           MemoryRegionSection *section);
void ram_discard_manager_unregister_listener(RamDiscardManager *rdm,
                                             RamDiscardListener *rdl);
_Bool 
    memory_get_xlat_addr(IOMMUTLBEntry *iotlb, void **vaddr,
                          ram_addr_t *ram_addr, 
                                               _Bool 
                                                    *read_only,
                          
                         _Bool 
                              *mr_has_discard_manager, Error **errp);
typedef struct CoalescedMemoryRange CoalescedMemoryRange;
typedef struct MemoryRegionIoeventfd MemoryRegionIoeventfd;
struct MemoryRegion {
    Object parent_obj;
    
   _Bool 
        romd_mode;
    
   _Bool 
        ram;
    
   _Bool 
        subpage;
    
   _Bool 
        readonly;
    
   _Bool 
        nonvolatile;
    
   _Bool 
        rom_device;
    
   _Bool 
        flush_coalesced_mmio;
    
   _Bool 
        unmergeable;
    uint8_t dirty_log_mask;
    
   _Bool 
        is_iommu;
    RAMBlock *ram_block;
    Object *owner;
    DeviceState *dev;
    const MemoryRegionOps *ops;
    void *opaque;
    MemoryRegion *container;
    int mapped_via_alias;
    Int128 size;
    hwaddr addr;
    void (*destructor)(MemoryRegion *mr);
    uint64_t align;
    
   _Bool 
        terminates;
    
   _Bool 
        ram_device;
    
   _Bool 
        enabled;
    uint8_t vga_logging_count;
    MemoryRegion *alias;
    hwaddr alias_offset;
    int32_t priority;
    union { struct MemoryRegion *tqh_first; QTailQLink tqh_circ; } subregions;
    union { struct MemoryRegion *tqe_next; QTailQLink tqe_circ; } subregions_link;
    union { struct CoalescedMemoryRange *tqh_first; QTailQLink tqh_circ; } coalesced;
    const char *name;
    unsigned ioeventfd_nb;
    MemoryRegionIoeventfd *ioeventfds;
    RamDiscardManager *rdm;
    
   _Bool 
        disable_reentrancy_guard;
};
struct IOMMUMemoryRegion {
    MemoryRegion parent_obj;
    struct { struct IOMMUNotifier *lh_first; } iommu_notify;
    IOMMUNotifierFlag iommu_notify_flags;
};
struct MemoryListener {
    void (*begin)(MemoryListener *listener);
    void (*commit)(MemoryListener *listener);
    void (*region_add)(MemoryListener *listener, MemoryRegionSection *section);
    void (*region_del)(MemoryListener *listener, MemoryRegionSection *section);
    void (*region_nop)(MemoryListener *listener, MemoryRegionSection *section);
    void (*log_start)(MemoryListener *listener, MemoryRegionSection *section,
                      int old_val, int new_val);
    void (*log_stop)(MemoryListener *listener, MemoryRegionSection *section,
                     int old_val, int new_val);
    void (*log_sync)(MemoryListener *listener, MemoryRegionSection *section);
    void (*log_sync_global)(MemoryListener *listener, 
                                                     _Bool 
                                                          last_stage);
    void (*log_clear)(MemoryListener *listener, MemoryRegionSection *section);
    
   _Bool 
        (*log_global_start)(MemoryListener *listener, Error **errp);
    void (*log_global_stop)(MemoryListener *listener);
    void (*log_global_after_sync)(MemoryListener *listener);
    void (*eventfd_add)(MemoryListener *listener, MemoryRegionSection *section,
                        
                       _Bool 
                            match_data, uint64_t data, EventNotifier *e);
    void (*eventfd_del)(MemoryListener *listener, MemoryRegionSection *section,
                        
                       _Bool 
                            match_data, uint64_t data, EventNotifier *e);
    void (*coalesced_io_add)(MemoryListener *listener, MemoryRegionSection *section,
                               hwaddr addr, hwaddr len);
    void (*coalesced_io_del)(MemoryListener *listener, MemoryRegionSection *section,
                               hwaddr addr, hwaddr len);
    unsigned priority;
    const char *name;
    AddressSpace *address_space;
    union { struct MemoryListener *tqe_next; QTailQLink tqe_circ; } link;
    union { struct MemoryListener *tqe_next; QTailQLink tqe_circ; } link_as;
};
typedef struct AddressSpaceMapClient {
    QEMUBH *bh;
    struct { struct AddressSpaceMapClient *le_next; struct AddressSpaceMapClient **le_prev; } link;
} AddressSpaceMapClient;
struct AddressSpace {
    struct rcu_head rcu;
    char *name;
    MemoryRegion *root;
    struct FlatView *current_map;
    int ioeventfd_nb;
    int ioeventfd_notifiers;
    struct MemoryRegionIoeventfd *ioeventfds;
    union { struct MemoryListener *tqh_first; QTailQLink tqh_circ; } listeners;
    union { struct AddressSpace *tqe_next; QTailQLink tqe_circ; } address_spaces_link;
    size_t max_bounce_buffer_size;
    size_t bounce_buffer_size;
    QemuMutex map_client_list_lock;
    struct { struct AddressSpaceMapClient *lh_first; } map_client_list;
};
typedef struct AddressSpaceDispatch AddressSpaceDispatch;
typedef struct FlatRange FlatRange;
struct FlatView {
    struct rcu_head rcu;
    unsigned ref;
    FlatRange *ranges;
    unsigned nr;
    unsigned nr_allocated;
    struct AddressSpaceDispatch *dispatch;
    MemoryRegion *root;
};
typedef 
       _Bool 
            (*flatview_cb)(Int128 start,
                            Int128 len,
                            const MemoryRegion *mr,
                            hwaddr offset_in_region,
                            void *opaque);
void flatview_for_each_range(FlatView *fv, flatview_cb cb, void *opaque);
MemoryRegionSection *memory_region_section_new_copy(MemoryRegionSection *s);
void memory_region_section_free_copy(MemoryRegionSection *s);
void memory_region_init(MemoryRegion *mr,
                        Object *owner,
                        const char *name,
                        uint64_t size);
void memory_region_ref(MemoryRegion *mr);
void memory_region_unref(MemoryRegion *mr);
void memory_region_init_io(MemoryRegion *mr,
                           Object *owner,
                           const MemoryRegionOps *ops,
                           void *opaque,
                           const char *name,
                           uint64_t size);
_Bool 
    memory_region_init_ram_nomigrate(MemoryRegion *mr,
                                      Object *owner,
                                      const char *name,
                                      uint64_t size,
                                      Error **errp);
_Bool 
    memory_region_init_ram_flags_nomigrate(MemoryRegion *mr,
                                            Object *owner,
                                            const char *name,
                                            uint64_t size,
                                            uint32_t ram_flags,
                                            Error **errp);
_Bool 
    memory_region_init_resizeable_ram(MemoryRegion *mr,
                                       Object *owner,
                                       const char *name,
                                       uint64_t size,
                                       uint64_t max_size,
                                       void (*resized)(const char*,
                                                       uint64_t length,
                                                       void *host),
                                       Error **errp);
_Bool 
    memory_region_init_ram_from_file(MemoryRegion *mr,
                                      Object *owner,
                                      const char *name,
                                      uint64_t size,
                                      uint64_t align,
                                      uint32_t ram_flags,
                                      const char *path,
                                      ram_addr_t offset,
                                      Error **errp);
_Bool 
    memory_region_init_ram_from_fd(MemoryRegion *mr,
                                    Object *owner,
                                    const char *name,
                                    uint64_t size,
                                    uint32_t ram_flags,
                                    int fd,
                                    ram_addr_t offset,
                                    Error **errp);
void memory_region_init_ram_ptr(MemoryRegion *mr,
                                Object *owner,
                                const char *name,
                                uint64_t size,
                                void *ptr);
void memory_region_init_ram_device_ptr(MemoryRegion *mr,
                                       Object *owner,
                                       const char *name,
                                       uint64_t size,
                                       void *ptr);
void memory_region_init_alias(MemoryRegion *mr,
                              Object *owner,
                              const char *name,
                              MemoryRegion *orig,
                              hwaddr offset,
                              uint64_t size);
_Bool 
    memory_region_init_rom_nomigrate(MemoryRegion *mr,
                                      Object *owner,
                                      const char *name,
                                      uint64_t size,
                                      Error **errp);
_Bool 
    memory_region_init_rom_device_nomigrate(MemoryRegion *mr,
                                             Object *owner,
                                             const MemoryRegionOps *ops,
                                             void *opaque,
                                             const char *name,
                                             uint64_t size,
                                             Error **errp);
void memory_region_init_iommu(void *_iommu_mr,
                              size_t instance_size,
                              const char *mrtypename,
                              Object *owner,
                              const char *name,
                              uint64_t size);
_Bool 
    memory_region_init_ram(MemoryRegion *mr,
                            Object *owner,
                            const char *name,
                            uint64_t size,
                            Error **errp);
_Bool 
    memory_region_init_ram_guest_memfd(MemoryRegion *mr,
                                        Object *owner,
                                        const char *name,
                                        uint64_t size,
                                        Error **errp);
_Bool 
    memory_region_init_rom(MemoryRegion *mr,
                            Object *owner,
                            const char *name,
                            uint64_t size,
                            Error **errp);
_Bool 
    memory_region_init_rom_device(MemoryRegion *mr,
                                   Object *owner,
                                   const MemoryRegionOps *ops,
                                   void *opaque,
                                   const char *name,
                                   uint64_t size,
                                   Error **errp);
Object *memory_region_owner(MemoryRegion *mr);
uint64_t memory_region_size(MemoryRegion *mr);
_Bool 
    memory_region_is_ram_device(MemoryRegion *mr);
_Bool 
    memory_region_is_protected(MemoryRegion *mr);
_Bool 
    memory_region_has_guest_memfd(MemoryRegion *mr);
uint64_t memory_region_iommu_get_min_page_size(IOMMUMemoryRegion *iommu_mr);
void memory_region_notify_iommu(IOMMUMemoryRegion *iommu_mr,
                                int iommu_idx,
                                const IOMMUTLBEvent event);
void memory_region_notify_iommu_one(IOMMUNotifier *notifier,
                                    const IOMMUTLBEvent *event);
void memory_region_unmap_iommu_notifier_range(IOMMUNotifier *notifier);
int memory_region_register_iommu_notifier(MemoryRegion *mr,
                                          IOMMUNotifier *n, Error **errp);
void memory_region_iommu_replay(IOMMUMemoryRegion *iommu_mr, IOMMUNotifier *n);
void memory_region_unregister_iommu_notifier(MemoryRegion *mr,
                                             IOMMUNotifier *n);
int memory_region_iommu_get_attr(IOMMUMemoryRegion *iommu_mr,
                                 enum IOMMUMemoryRegionAttr attr,
                                 void *data);
int memory_region_iommu_attrs_to_index(IOMMUMemoryRegion *iommu_mr,
                                       MemTxAttrs attrs);
int memory_region_iommu_num_indexes(IOMMUMemoryRegion *iommu_mr);
const char *memory_region_name(const MemoryRegion *mr);
_Bool 
    memory_region_is_logging(MemoryRegion *mr, uint8_t client);
uint8_t memory_region_get_dirty_log_mask(MemoryRegion *mr);
int memory_region_get_fd(MemoryRegion *mr);
MemoryRegion *memory_region_from_host(void *ptr, ram_addr_t *offset);
void *memory_region_get_ram_ptr(MemoryRegion *mr);
void memory_region_ram_resize(MemoryRegion *mr, ram_addr_t newsize,
                              Error **errp);
void memory_region_msync(MemoryRegion *mr, hwaddr addr, hwaddr size);
void memory_region_writeback(MemoryRegion *mr, hwaddr addr, hwaddr size);
void memory_region_set_log(MemoryRegion *mr, 
                                            _Bool 
                                                 log, unsigned client);
void memory_region_set_dirty(MemoryRegion *mr, hwaddr addr,
                             hwaddr size);
void memory_region_clear_dirty_bitmap(MemoryRegion *mr, hwaddr start,
                                      hwaddr len);
DirtyBitmapSnapshot *memory_region_snapshot_and_clear_dirty(MemoryRegion *mr,
                                                            hwaddr addr,
                                                            hwaddr size,
                                                            unsigned client);
_Bool 
    memory_region_snapshot_get_dirty(MemoryRegion *mr,
                                      DirtyBitmapSnapshot *snap,
                                      hwaddr addr, hwaddr size);
void memory_region_reset_dirty(MemoryRegion *mr, hwaddr addr,
                               hwaddr size, unsigned client);
void memory_region_flush_rom_device(MemoryRegion *mr, hwaddr addr, hwaddr size);
void memory_region_set_readonly(MemoryRegion *mr, 
                                                 _Bool 
                                                      readonly);
void memory_region_set_nonvolatile(MemoryRegion *mr, 
                                                    _Bool 
                                                         nonvolatile);
void memory_region_rom_device_set_romd(MemoryRegion *mr, 
                                                        _Bool 
                                                             romd_mode);
void memory_region_set_coalescing(MemoryRegion *mr);
void memory_region_add_coalescing(MemoryRegion *mr,
                                  hwaddr offset,
                                  uint64_t size);
void memory_region_clear_coalescing(MemoryRegion *mr);
void memory_region_set_flush_coalesced(MemoryRegion *mr);
void memory_region_clear_flush_coalesced(MemoryRegion *mr);
void memory_region_add_eventfd(MemoryRegion *mr,
                               hwaddr addr,
                               unsigned size,
                               
                              _Bool 
                                   match_data,
                               uint64_t data,
                               EventNotifier *e);
void memory_region_del_eventfd(MemoryRegion *mr,
                               hwaddr addr,
                               unsigned size,
                               
                              _Bool 
                                   match_data,
                               uint64_t data,
                               EventNotifier *e);
void memory_region_add_subregion(MemoryRegion *mr,
                                 hwaddr offset,
                                 MemoryRegion *subregion);
void memory_region_add_subregion_overlap(MemoryRegion *mr,
                                         hwaddr offset,
                                         MemoryRegion *subregion,
                                         int priority);
ram_addr_t memory_region_get_ram_addr(MemoryRegion *mr);
uint64_t memory_region_get_alignment(const MemoryRegion *mr);
void memory_region_del_subregion(MemoryRegion *mr,
                                 MemoryRegion *subregion);
void memory_region_set_enabled(MemoryRegion *mr, 
                                                _Bool 
                                                     enabled);
void memory_region_set_address(MemoryRegion *mr, hwaddr addr);
void memory_region_set_size(MemoryRegion *mr, uint64_t size);
void memory_region_set_alias_offset(MemoryRegion *mr,
                                    hwaddr offset);
void memory_region_set_unmergeable(MemoryRegion *mr, 
                                                    _Bool 
                                                         unmergeable);
_Bool 
    memory_region_present(MemoryRegion *container, hwaddr addr);
_Bool 
    memory_region_is_mapped(MemoryRegion *mr);
RamDiscardManager *memory_region_get_ram_discard_manager(MemoryRegion *mr);
void memory_region_set_ram_discard_manager(MemoryRegion *mr,
                                           RamDiscardManager *rdm);
MemoryRegionSection memory_region_find(MemoryRegion *mr,
                                       hwaddr addr, uint64_t size);
void memory_global_dirty_log_sync(
                                 _Bool 
                                      last_stage);
void memory_global_after_dirty_log_sync(void);
void memory_region_transaction_begin(void);
void memory_region_transaction_commit(void);
void memory_listener_register(MemoryListener *listener, AddressSpace *filter);
void memory_listener_unregister(MemoryListener *listener);
_Bool 
    memory_global_dirty_log_start(unsigned int flags, Error **errp);
void memory_global_dirty_log_stop(unsigned int flags);
void mtree_info(
               _Bool 
                    flatview, 
                              _Bool 
                                   dispatch_tree, 
                                                  _Bool 
                                                       owner, 
                                                              _Bool 
                                                                   disabled);
_Bool 
    memory_region_access_valid(MemoryRegion *mr, hwaddr addr,
                                unsigned size, 
                                              _Bool 
                                                   is_write,
                                MemTxAttrs attrs);
MemTxResult memory_region_dispatch_read(MemoryRegion *mr,
                                        hwaddr addr,
                                        uint64_t *pval,
                                        MemOp op,
                                        MemTxAttrs attrs);
MemTxResult memory_region_dispatch_write(MemoryRegion *mr,
                                         hwaddr addr,
                                         uint64_t data,
                                         MemOp op,
                                         MemTxAttrs attrs);
void address_space_init(AddressSpace *as, MemoryRegion *root, const char *name);
void address_space_destroy(AddressSpace *as);
void address_space_remove_listeners(AddressSpace *as);
MemTxResult address_space_rw(AddressSpace *as, hwaddr addr,
                             MemTxAttrs attrs, void *buf,
                             hwaddr len, 
                                        _Bool 
                                             is_write);
MemTxResult address_space_write(AddressSpace *as, hwaddr addr,
                                MemTxAttrs attrs,
                                const void *buf, hwaddr len);
MemTxResult address_space_write_rom(AddressSpace *as, hwaddr addr,
                                    MemTxAttrs attrs,
                                    const void *buf, hwaddr len);
uint8_t address_space_ldub(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint16_t address_space_lduw_le(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint16_t address_space_lduw_be(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl_le(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl_be(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq_le(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq_be(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
void address_space_stb(AddressSpace *as,
    hwaddr addr, uint8_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw_le(AddressSpace *as,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw_be(AddressSpace *as,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_le(AddressSpace *as,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_be(AddressSpace *as,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq_le(AddressSpace *as,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq_be(AddressSpace *as,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
struct MemoryRegionCache {
    uint8_t *ptr;
    hwaddr xlat;
    hwaddr len;
    FlatView *fv;
    MemoryRegionSection mrs;
    
   _Bool 
        is_write;
};
uint8_t address_space_ldub_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint16_t address_space_lduw_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint16_t address_space_lduw_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
void address_space_stb_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint8_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
int64_t address_space_cache_init(MemoryRegionCache *cache,
                                 AddressSpace *as,
                                 hwaddr addr,
                                 hwaddr len,
                                 
                                _Bool 
                                     is_write);
void address_space_cache_invalidate(MemoryRegionCache *cache,
                                    hwaddr addr,
                                    hwaddr access_len);
void address_space_cache_destroy(MemoryRegionCache *cache);
IOMMUTLBEntry address_space_get_iotlb_entry(AddressSpace *as, hwaddr addr,
                                            
                                           _Bool 
                                                is_write, MemTxAttrs attrs);
MemoryRegion *flatview_translate(FlatView *fv,
                                 hwaddr addr, hwaddr *xlat,
                                 hwaddr *len, 
                                             _Bool 
                                                  is_write,
                                 MemTxAttrs attrs);
_Bool 
    address_space_access_valid(AddressSpace *as, hwaddr addr, hwaddr len,
                                
                               _Bool 
                                    is_write, MemTxAttrs attrs);
void *address_space_map(AddressSpace *as, hwaddr addr,
                        hwaddr *plen, 
                                     _Bool 
                                          is_write, MemTxAttrs attrs);
void address_space_unmap(AddressSpace *as, void *buffer, hwaddr len,
                         
                        _Bool 
                             is_write, hwaddr access_len);
void address_space_register_map_client(AddressSpace *as, QEMUBH *bh);
void address_space_unregister_map_client(AddressSpace *as, QEMUBH *bh);
MemTxResult address_space_read_full(AddressSpace *as, hwaddr addr,
                                    MemTxAttrs attrs, void *buf, hwaddr len);
MemTxResult flatview_read_continue(FlatView *fv, hwaddr addr,
                                   MemTxAttrs attrs, void *buf,
                                   hwaddr len, hwaddr addr1, hwaddr l,
                                   MemoryRegion *mr);
void *qemu_map_ram_ptr(RAMBlock *ram_block, ram_addr_t addr);
MemTxResult address_space_read_cached_slow(MemoryRegionCache *cache,
                                           hwaddr addr, void *buf, hwaddr len);
MemTxResult address_space_write_cached_slow(MemoryRegionCache *cache,
                                            hwaddr addr, const void *buf,
                                            hwaddr len);
int memory_access_size(MemoryRegion *mr, unsigned l, hwaddr addr);
_Bool 
    prepare_mmio_access(MemoryRegion *mr);
MemTxResult address_space_set(AddressSpace *as, hwaddr addr,
                              uint8_t c, hwaddr len, MemTxAttrs attrs);
int ram_block_discard_disable(
                             _Bool 
                                  state);
int ram_block_uncoordinated_discard_disable(
                                           _Bool 
                                                state);
int ram_block_discard_require(
                             _Bool 
                                  state);
int ram_block_coordinated_discard_require(
                                         _Bool 
                                              state);
_Bool 
    ram_block_discard_is_disabled(void);
_Bool 
    ram_block_discard_is_required(void);
_Bool 
    target_words_bigendian(void);
uint16_t address_space_lduw(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_notdirty(AddressSpace *as,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw(AddressSpace *as,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl(AddressSpace *as,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq(AddressSpace *as,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
uint16_t address_space_lduw_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_notdirty_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
CPUArchState *cpu_copy(CPUArchState *env);
;
;
;
enum {
    PPC_NONE = 0x0000000000000000ULL,
    PPC_INSNS_BASE = 0x0000000000000001ULL,
    PPC_64B = 0x0000000000000020ULL,
    PPC_64BX = 0x0000000000000040ULL,
    PPC_64H = 0x0000000000000080ULL,
    PPC_WAIT = 0x0000000000000100ULL,
    PPC_MFTB = 0x0000000000000200ULL,
    PPC_ISEL = 0x0000000000000800ULL,
    PPC_POPCNTB = 0x0000000000001000ULL,
    PPC_STRING = 0x0000000000002000ULL,
    PPC_CILDST = 0x0000000000004000ULL,
    PPC_FLOAT = 0x0000000000010000ULL,
    PPC_FLOAT_EXT = 0x0000000000020000ULL,
    PPC_FLOAT_FSQRT = 0x0000000000040000ULL,
    PPC_FLOAT_FRES = 0x0000000000080000ULL,
    PPC_FLOAT_FRSQRTE = 0x0000000000100000ULL,
    PPC_FLOAT_FRSQRTES = 0x0000000000200000ULL,
    PPC_FLOAT_FSEL = 0x0000000000400000ULL,
    PPC_FLOAT_STFIWX = 0x0000000000800000ULL,
    PPC_ALTIVEC = 0x0000000001000000ULL,
    PPC_SPE = 0x0000000002000000ULL,
    PPC_SPE_SINGLE = 0x0000000004000000ULL,
    PPC_SPE_DOUBLE = 0x0000000008000000ULL,
    PPC_MEM_TLBIA = 0x0000000010000000ULL,
    PPC_MEM_TLBIE = 0x0000000020000000ULL,
    PPC_MEM_TLBSYNC = 0x0000000040000000ULL,
    PPC_MEM_SYNC = 0x0000000080000000ULL,
    PPC_MEM_EIEIO = 0x0000000100000000ULL,
    PPC_CACHE = 0x0000000200000000ULL,
    PPC_CACHE_ICBI = 0x0000000400000000ULL,
    PPC_CACHE_DCBZ = 0x0000000800000000ULL,
    PPC_CACHE_DCBA = 0x0000002000000000ULL,
    PPC_CACHE_LOCK = 0x0000004000000000ULL,
    PPC_EXTERN = 0x0000010000000000ULL,
    PPC_SEGMENT = 0x0000020000000000ULL,
    PPC_6xx_TLB = 0x0000040000000000ULL,
    PPC_40x_TLB = 0x0000100000000000ULL,
    PPC_SEGMENT_64B = 0x0000200000000000ULL,
    PPC_SLBI = 0x0000400000000000ULL,
    PPC_WRTEE = 0x0001000000000000ULL,
    PPC_40x_EXCP = 0x0002000000000000ULL,
    PPC_405_MAC = 0x0004000000000000ULL,
    PPC_440_SPEC = 0x0008000000000000ULL,
    PPC_BOOKE = 0x0010000000000000ULL,
    PPC_MFAPIDI = 0x0020000000000000ULL,
    PPC_TLBIVA = 0x0040000000000000ULL,
    PPC_TLBIVAX = 0x0080000000000000ULL,
    PPC_4xx_COMMON = 0x0100000000000000ULL,
    PPC_40x_ICBT = 0x0200000000000000ULL,
    PPC_RFMCI = 0x0400000000000000ULL,
    PPC_RFDI = 0x0800000000000000ULL,
    PPC_DCR = 0x1000000000000000ULL,
    PPC_DCRX = 0x2000000000000000ULL,
    PPC_POPCNTWD = 0x8000000000000000ULL,
    PPC2_BOOKE206 = 0x0000000000000001ULL,
    PPC2_VSX = 0x0000000000000002ULL,
    PPC2_DFP = 0x0000000000000004ULL,
    PPC2_PRCNTL = 0x0000000000000008ULL,
    PPC2_DBRX = 0x0000000000000010ULL,
    PPC2_ISA205 = 0x0000000000000020ULL,
    PPC2_VSX207 = 0x0000000000000040ULL,
    PPC2_PERM_ISA206 = 0x0000000000000080ULL,
    PPC2_DIVE_ISA206 = 0x0000000000000100ULL,
    PPC2_ATOMIC_ISA206 = 0x0000000000000200ULL,
    PPC2_FP_CVT_ISA206 = 0x0000000000000400ULL,
    PPC2_FP_TST_ISA206 = 0x0000000000000800ULL,
    PPC2_BCTAR_ISA207 = 0x0000000000001000ULL,
    PPC2_LSQ_ISA207 = 0x0000000000002000ULL,
    PPC2_ALTIVEC_207 = 0x0000000000004000ULL,
    PPC2_ISA207S = 0x0000000000008000ULL,
    PPC2_FP_CVT_S64 = 0x0000000000010000ULL,
    PPC2_TM = 0x0000000000020000ULL,
    PPC2_PM_ISA206 = 0x0000000000040000ULL,
    PPC2_ISA300 = 0x0000000000080000ULL,
    PPC2_ISA310 = 0x0000000000100000ULL,
    PPC2_MEM_LWSYNC = 0x0000000000200000ULL,
    PPC2_BCDA_ISA206 = 0x0000000000400000ULL,
};
enum {
    ACCESS_CODE = 0x10,
    ACCESS_INT = 0x20,
    ACCESS_FLOAT = 0x30,
    ACCESS_RES = 0x40,
    ACCESS_EXT = 0x50,
    ACCESS_CACHE = 0x60,
};
enum {
    PPC6xx_INPUT_HRESET = 0,
    PPC6xx_INPUT_SRESET = 1,
    PPC6xx_INPUT_CKSTP_IN = 2,
    PPC6xx_INPUT_MCP = 3,
    PPC6xx_INPUT_SMI = 4,
    PPC6xx_INPUT_INT = 5,
    PPC6xx_INPUT_TBEN = 6,
    PPC6xx_INPUT_WAKEUP = 7,
    PPC6xx_INPUT_NB,
};
enum {
    PPCBookE_INPUT_HRESET = 0,
    PPCBookE_INPUT_SRESET = 1,
    PPCBookE_INPUT_CKSTP_IN = 2,
    PPCBookE_INPUT_MCP = 3,
    PPCBookE_INPUT_SMI = 4,
    PPCBookE_INPUT_INT = 5,
    PPCBookE_INPUT_CINT = 6,
    PPCBookE_INPUT_NB,
};
enum {
    PPCE500_INPUT_RESET_CORE = 0,
    PPCE500_INPUT_MCK = 1,
    PPCE500_INPUT_CINT = 3,
    PPCE500_INPUT_INT = 4,
    PPCE500_INPUT_DEBUG = 6,
    PPCE500_INPUT_NB,
};
enum {
    PPC40x_INPUT_RESET_CORE = 0,
    PPC40x_INPUT_RESET_CHIP = 1,
    PPC40x_INPUT_RESET_SYS = 2,
    PPC40x_INPUT_CINT = 3,
    PPC40x_INPUT_INT = 4,
    PPC40x_INPUT_HALT = 5,
    PPC40x_INPUT_DEBUG = 6,
    PPC40x_INPUT_NB,
};
enum {
    PPCRCPU_INPUT_PORESET = 0,
    PPCRCPU_INPUT_HRESET = 1,
    PPCRCPU_INPUT_SRESET = 2,
    PPCRCPU_INPUT_IRQ0 = 3,
    PPCRCPU_INPUT_IRQ1 = 4,
    PPCRCPU_INPUT_IRQ2 = 5,
    PPCRCPU_INPUT_IRQ3 = 6,
    PPCRCPU_INPUT_IRQ4 = 7,
    PPCRCPU_INPUT_IRQ5 = 8,
    PPCRCPU_INPUT_IRQ6 = 9,
    PPCRCPU_INPUT_IRQ7 = 10,
    PPCRCPU_INPUT_NB,
};
enum {
    PPC_INTERRUPT_RESET = 0x00001,
    PPC_INTERRUPT_WAKEUP = 0x00002,
    PPC_INTERRUPT_MCK = 0x00004,
    PPC_INTERRUPT_EXT = 0x00008,
    PPC_INTERRUPT_SMI = 0x00010,
    PPC_INTERRUPT_CEXT = 0x00020,
    PPC_INTERRUPT_DEBUG = 0x00040,
    PPC_INTERRUPT_THERM = 0x00080,
    PPC_INTERRUPT_DECR = 0x00100,
    PPC_INTERRUPT_HDECR = 0x00200,
    PPC_INTERRUPT_PIT = 0x00400,
    PPC_INTERRUPT_FIT = 0x00800,
    PPC_INTERRUPT_WDT = 0x01000,
    PPC_INTERRUPT_CDOORBELL = 0x02000,
    PPC_INTERRUPT_DOORBELL = 0x04000,
    PPC_INTERRUPT_PERFM = 0x08000,
    PPC_INTERRUPT_HMI = 0x10000,
    PPC_INTERRUPT_HDOORBELL = 0x20000,
    PPC_INTERRUPT_HVIRT = 0x40000,
    PPC_INTERRUPT_EBB = 0x80000,
};
enum {
    PCR_COMPAT_2_05 = 2,
    PCR_COMPAT_2_06 = 4,
    PCR_COMPAT_2_07 = 8,
    PCR_COMPAT_3_00 = 16,
    PCR_COMPAT_3_10 = 32,
    PCR_VEC_DIS = 9223372036854775808,
    PCR_VSX_DIS = 4611686018427387904,
    PCR_TM_DIS = 2305843009213693952,
};
enum {
    HMER_MALFUNCTION_ALERT = 9223372036854775808,
    HMER_PROC_RECV_DONE = 2305843009213693952,
    HMER_PROC_RECV_ERROR_MASKED = 1152921504606846976,
    HMER_TFAC_ERROR = 576460752303423488,
    HMER_TFMR_PARITY_ERROR = 288230376151711744,
    HMER_XSCOM_FAIL = 36028797018963968,
    HMER_XSCOM_DONE = 18014398509481984,
    HMER_PROC_RECV_AGAIN = 4503599627370496,
    HMER_WARN_RISE = 562949953421312,
    HMER_WARN_FALL = 281474976710656,
    HMER_SCOM_FIR_HMI = 140737488355328,
    HMER_TRIG_FIR_HMI = 70368744177664,
    HMER_HYP_RESOURCE_ERR = 8796093022208,
    HMER_XSCOM_STATUS_MASK = 7696581394432,
};
enum {
    TFMR_CONTROL_MASK = 18446743523953737728,
    TFMR_MASK_HMI = 9007199254740992,
    TFMR_TB_ECLIPZ = 562949953421312,
    TFMR_LOAD_TOD_MOD = 140737488355328,
    TFMR_MOVE_CHIP_TOD_TO_TB = 35184372088832,
    TFMR_CLEAR_TB_ERRORS = 549755813888,
    TFMR_STATUS_MASK = 549755813887,
    TFMR_TBST_ENCODED = 64424509440,
    TFMR_TBST_LAST = 4026531840,
    TFMR_TB_ENABLED = 8388608,
    TFMR_TB_VALID = 4194304,
    TFMR_TB_SYNC_OCCURED = 2097152,
    TFMR_FIRMWARE_CONTROL_ERROR = 131072,
};
enum {
    TBST_RESET = 0x0,
    TBST_SEND_TOD_MOD = 0x1,
    TBST_NOT_SET = 0x2,
    TBST_SYNC_WAIT = 0x6,
    TBST_GET_TOD = 0x7,
    TBST_TB_RUNNING = 0x8,
    TBST_TB_ERROR = 0x9,
};
target_ulong cpu_read_xer(const CPUPPCState *env);
void cpu_write_xer(CPUPPCState *env, target_ulong xer);
 void raise_exception(CPUPPCState *env, uint32_t exception);
 void raise_exception_ra(CPUPPCState *env, uint32_t exception,
                                   uintptr_t raddr);
 void raise_exception_err(CPUPPCState *env, uint32_t exception,
                                    uint32_t error_code);
 void raise_exception_err_ra(CPUPPCState *env, uint32_t exception,
                                       uint32_t error_code, uintptr_t raddr);
void dump_mmu(CPUPPCState *env);
void ppc_maybe_bswap_register(CPUPPCState *env, uint8_t *mem_buf, int len);
void ppc_store_vscr(CPUPPCState *env, uint32_t vscr);
uint32_t ppc_get_vscr(CPUPPCState *env);
void ppc_set_cr(CPUPPCState *env, uint64_t cr);
uint64_t ppc_get_cr(const CPUPPCState *env);
typedef target_ulong target_ptr_t;
typedef int32_t target_pid_t;
       
typedef enum panda_cb_type {
    PANDA_CB_BEFORE_BLOCK_TRANSLATE,
    PANDA_CB_AFTER_BLOCK_TRANSLATE,
    PANDA_CB_BLOCK_TRANSLATE,
    PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT,
    PANDA_CB_BEFORE_TCG_CODEGEN,
    PANDA_CB_BEFORE_BLOCK_EXEC,
    PANDA_CB_AFTER_BLOCK_EXEC,
    PANDA_CB_INSN_TRANSLATE,
    PANDA_CB_INSN_EXEC,
    PANDA_CB_AFTER_INSN_TRANSLATE,
    PANDA_CB_AFTER_INSN_EXEC,
    PANDA_CB_VIRT_MEM_BEFORE_READ,
    PANDA_CB_VIRT_MEM_BEFORE_WRITE,
    PANDA_CB_PHYS_MEM_BEFORE_READ,
    PANDA_CB_PHYS_MEM_BEFORE_WRITE,
    PANDA_CB_VIRT_MEM_AFTER_READ,
    PANDA_CB_VIRT_MEM_AFTER_WRITE,
    PANDA_CB_PHYS_MEM_AFTER_READ,
    PANDA_CB_PHYS_MEM_AFTER_WRITE,
    PANDA_CB_MMIO_AFTER_READ,
    PANDA_CB_MMIO_BEFORE_WRITE,
    PANDA_CB_HD_READ,
    PANDA_CB_HD_WRITE,
    PANDA_CB_GUEST_HYPERCALL,
    PANDA_CB_MONITOR,
    PANDA_CB_QMP,
    PANDA_CB_CPU_RESTORE_STATE,
    PANDA_CB_BEFORE_LOADVM,
    PANDA_CB_ASID_CHANGED,
    PANDA_CB_REPLAY_HD_TRANSFER,
    PANDA_CB_REPLAY_NET_TRANSFER,
    PANDA_CB_REPLAY_SERIAL_RECEIVE,
    PANDA_CB_REPLAY_SERIAL_READ,
    PANDA_CB_REPLAY_SERIAL_SEND,
    PANDA_CB_REPLAY_SERIAL_WRITE,
    PANDA_CB_REPLAY_BEFORE_DMA,
    PANDA_CB_REPLAY_AFTER_DMA,
    PANDA_CB_REPLAY_HANDLE_PACKET,
    PANDA_CB_AFTER_CPU_EXEC_ENTER,
    PANDA_CB_BEFORE_CPU_EXEC_EXIT,
    PANDA_CB_AFTER_MACHINE_INIT,
    PANDA_CB_AFTER_LOADVM,
    PANDA_CB_TOP_LOOP,
    PANDA_CB_DURING_MACHINE_INIT,
    PANDA_CB_MAIN_LOOP_WAIT,
    PANDA_CB_PRE_SHUTDOWN,
    PANDA_CB_UNASSIGNED_IO_READ,
    PANDA_CB_UNASSIGNED_IO_WRITE,
    PANDA_CB_BEFORE_HANDLE_EXCEPTION,
    PANDA_CB_BEFORE_HANDLE_INTERRUPT,
    PANDA_CB_START_BLOCK_EXEC,
    PANDA_CB_END_BLOCK_EXEC,
    PANDA_CB_LAST
} panda_cb_type;
typedef union panda_cb {
    
   _Bool 
        (*before_block_exec_invalidate_opt)(CPUState *env, TranslationBlock *tb);
    void (*before_tcg_codegen)(CPUState *env, TranslationBlock *tb);
    void (*before_block_exec)(CPUState *env, TranslationBlock *tb);
    void (*after_block_exec)(CPUState *env, TranslationBlock *tb, uint8_t exitCode);
    void (*before_block_translate)(CPUState *env, uint64_t pc);
    void (*after_block_translate)(CPUState *env, TranslationBlock *tb);
    void (*block_translate)(CPUState *env, struct qemu_plugin_tb *tb);
    void (*after_cpu_exec_enter)(CPUState *env);
    void (*before_cpu_exec_exit)(CPUState *env, 
                                               _Bool 
                                                    ranBlock);
    
   _Bool 
        (*insn_translate)(CPUState *env, uint64_t pc);
    int (*insn_exec)(CPUState *env, uint64_t pc);
    
   _Bool 
        (*after_insn_translate)(CPUState *env, uint64_t pc);
    int (*after_insn_exec)(CPUState *env, uint64_t pc);
    void (*virt_mem_before_read)(CPUState *env, uint64_t pc, uint64_t addr, size_t size);
    void (*virt_mem_before_write)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_before_read)(CPUState *env, uint64_t pc, uint64_t addr, size_t size);
    void (*phys_mem_before_write)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*virt_mem_after_read)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*virt_mem_after_write)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_after_read)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_after_write)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*mmio_after_read)(CPUState *env, uint64_t physaddr, uint64_t vaddr, size_t size, uint64_t *val);
    void (*mmio_before_write)(CPUState *env, uint64_t physaddr, uint64_t vaddr, size_t size, uint64_t *val);
    void (*hd_read)(CPUState *env);
    void (*hd_write)(CPUState *env);
    
   _Bool 
        (*guest_hypercall)(CPUState *env);
    int (*monitor)(Monitor *mon, const char *cmd);
    
   _Bool 
        (*qmp)(char *command, char* args, char **result);
    void (*cpu_restore_state)(CPUState *env, TranslationBlock *tb);
    int (*before_loadvm)(void);
    
   _Bool 
        (*asid_changed)(CPUState *env, uint64_t oldval, uint64_t newval);
    void (*replay_hd_transfer)(CPUState *env, uint32_t type, uint64_t src_addr, uint64_t dest_addr, size_t num_bytes);
    void (*replay_before_dma)(CPUState *env, const uint8_t *buf, hwaddr addr, size_t size, 
                                                                                          _Bool 
                                                                                               is_write);
    void (*replay_after_dma)(CPUState *env, const uint8_t *buf, hwaddr addr, size_t size, 
                                                                                         _Bool 
                                                                                              is_write);
    void (*replay_handle_packet)(CPUState *env, uint8_t *buf, size_t size, uint8_t direction, uint64_t buf_addr_rec);
    void (*replay_net_transfer)(CPUState *env, uint32_t type, uint64_t src_addr, uint64_t dest_addr, size_t num_bytes);
    void (*replay_serial_receive)(CPUState *env, uint64_t fifo_addr, uint8_t value);
    void (*replay_serial_read)(CPUState *env, uint64_t fifo_addr, uint32_t port_addr, uint8_t value);
    void (*replay_serial_send)(CPUState *env, uint64_t fifo_addr, uint8_t value);
    void (*replay_serial_write)(CPUState *env, uint64_t fifo_addr, uint32_t port_addr, uint8_t value);
    void (*after_machine_init)(CPUState *env);
    void (*after_loadvm)(CPUState *env);
    void (*top_loop)(CPUState *env);
    void (*during_machine_init)(MachineState *machine);
    void (*main_loop_wait)(void);
    void (*pre_shutdown)(void);
    
   _Bool 
        (*unassigned_io_read)(CPUState *env, uint64_t pc, hwaddr addr, size_t size, uint64_t *val);
    
   _Bool 
        (*unassigned_io_write)(CPUState *env, uint64_t pc, hwaddr addr, size_t size, uint64_t val);
    int32_t (*before_handle_exception)(CPUState *cpu, int32_t exception_index);
    int32_t (*before_handle_interrupt)(CPUState *cpu, int32_t interrupt_request);
    void (*start_block_exec)(CPUState *cpu, TranslationBlock* tb);
    void (*end_block_exec)(CPUState *cpu, TranslationBlock* tb);
    void (*cbaddr)(void);
} panda_cb;
typedef union panda_cb_with_context {
    
   _Bool 
        (*before_block_exec_invalidate_opt)(void* context, CPUState *env, TranslationBlock *tb);
    void (*before_tcg_codegen)(void* context, CPUState *env, TranslationBlock *tb);
    void (*before_block_exec)(void* context, CPUState *env, TranslationBlock *tb);
    void (*after_block_exec)(void* context, CPUState *env, TranslationBlock *tb, uint8_t exitCode);
    void (*before_block_translate)(void* context, CPUState *env, uint64_t pc);
    void (*after_block_translate)(void* context, CPUState *env, TranslationBlock *tb);
    void (*block_translate)(void* context, CPUState *env, struct qemu_plugin_tb *tb);
    void (*after_cpu_exec_enter)(void* context, CPUState *env);
    void (*before_cpu_exec_exit)(void* context, CPUState *env, 
                                                              _Bool 
                                                                   ranBlock);
    
   _Bool 
        (*insn_translate)(void* context, CPUState *env, uint64_t pc);
    int (*insn_exec)(void* context, CPUState *env, uint64_t pc);
    
   _Bool 
        (*after_insn_translate)(void* context, CPUState *env, uint64_t pc);
    int (*after_insn_exec)(void* context, CPUState *env, uint64_t pc);
    void (*virt_mem_before_read)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size);
    void (*virt_mem_before_write)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_before_read)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size);
    void (*phys_mem_before_write)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*virt_mem_after_read)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*virt_mem_after_write)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_after_read)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_after_write)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*mmio_after_read)(void* context, CPUState *env, uint64_t physaddr, uint64_t vaddr, size_t size, uint64_t *val);
    void (*mmio_before_write)(void* context, CPUState *env, uint64_t physaddr, uint64_t vaddr, size_t size, uint64_t *val);
    void (*hd_read)(void* context, CPUState *env);
    void (*hd_write)(void* context, CPUState *env);
    
   _Bool 
        (*guest_hypercall)(void* context, CPUState *env);
    int (*monitor)(void* context, Monitor *mon, const char *cmd);
    
   _Bool 
        (*qmp)(void* context, char *command, char* args, char **result);
    void (*cpu_restore_state)(void* context, CPUState *env, TranslationBlock *tb);
    int (*before_loadvm)(void* context);
    
   _Bool 
        (*asid_changed)(void* context, CPUState *env, uint64_t oldval, uint64_t newval);
    void (*replay_hd_transfer)(void* context, CPUState *env, uint32_t type, uint64_t src_addr, uint64_t dest_addr, size_t num_bytes);
    void (*replay_before_dma)(void* context, CPUState *env, const uint8_t *buf, hwaddr addr, size_t size, 
                                                                                                         _Bool 
                                                                                                              is_write);
    void (*replay_after_dma)(void* context, CPUState *env, const uint8_t *buf, hwaddr addr, size_t size, 
                                                                                                        _Bool 
                                                                                                             is_write);
    void (*replay_handle_packet)(void* context, CPUState *env, uint8_t *buf, size_t size, uint8_t direction, uint64_t buf_addr_rec);
    void (*replay_net_transfer)(void* context, CPUState *env, uint32_t type, uint64_t src_addr, uint64_t dest_addr, size_t num_bytes);
    void (*replay_serial_receive)(void* context, CPUState *env, uint64_t fifo_addr, uint8_t value);
    void (*replay_serial_read)(void* context, CPUState *env, uint64_t fifo_addr, uint32_t port_addr, uint8_t value);
    void (*replay_serial_send)(void* context, CPUState *env, uint64_t fifo_addr, uint8_t value);
    void (*replay_serial_write)(void* context, CPUState *env, uint64_t fifo_addr, uint32_t port_addr, uint8_t value);
    void (*after_machine_init)(void* context, CPUState *env);
    void (*after_loadvm)(void* context, CPUState *env);
    void (*top_loop)(void* context, CPUState *env);
    void (*during_machine_init)(void* context, MachineState *machine);
    void (*main_loop_wait)(void* context);
    void (*pre_shutdown)(void* context);
    
   _Bool 
        (*unassigned_io_read)(void* context, CPUState *env, uint64_t pc, hwaddr addr, size_t size, uint64_t *val);
    
   _Bool 
        (*unassigned_io_write)(void* context, CPUState *env, uint64_t pc, hwaddr addr, size_t size, uint64_t val);
    int32_t (*before_handle_exception)(void* context, CPUState *cpu, int32_t exception_index);
    int32_t (*before_handle_interrupt)(void* context, CPUState *cpu, int32_t interrupt_request);
    void (*start_block_exec)(void* context, CPUState *cpu, TranslationBlock* tb);
    void (*end_block_exec)(void* context, CPUState *cpu, TranslationBlock* tb);
    void (*cbaddr)(void);
} panda_cb_with_context;
       
       
       
struct AccelState {
    Object parent_obj;
};
typedef struct AccelClass {
    ObjectClass parent_class;
    const char *name;
    int (*init_machine)(MachineState *ms);
    void (*setup_post)(MachineState *ms, AccelState *accel);
    
   _Bool 
        (*has_memory)(MachineState *ms, AddressSpace *as,
                       hwaddr start_addr, hwaddr size);
    
   _Bool 
        (*cpu_common_realize)(CPUState *cpu, Error **errp);
    void (*cpu_common_unrealize)(CPUState *cpu);
    int (*gdbstub_supported_sstep_flags)(void);
    
   _Bool 
        *allowed;
    GPtrArray *compat_props;
} AccelClass;
AccelClass *accel_find(const char *opt_name);
AccelState *current_accel(void);
const char *current_accel_name(void);
void accel_init_interfaces(AccelClass *ac);
int accel_init_machine(AccelState *accel, MachineState *ms);
void accel_setup_post(MachineState *ms);
void accel_cpu_instance_init(CPUState *cpu);
_Bool 
    accel_cpu_common_realize(CPUState *cpu, Error **errp);
void accel_cpu_common_unrealize(CPUState *cpu);
int accel_supported_gdbstub_sstep_flags(void);
MemoryRegion *get_system_memory(void);
MemoryRegion *get_system_io(void);
extern AddressSpace address_space_memory;
extern AddressSpace address_space_io;
typedef ram_addr_t tb_page_addr_t;
struct tb_tc {
    const void *ptr;
    size_t size;
};
struct TranslationBlock {
    vaddr pc;
    uint64_t cs_base;
    uint32_t flags;
    uint32_t cflags;
    uint16_t size;
    uint16_t icount;
    struct tb_tc tc;
    uintptr_t page_next[2];
    tb_page_addr_t page_addr[2];
    QemuSpin jmp_lock;
    uint16_t jmp_reset_offset[2];
    uint16_t jmp_insn_offset[2];
    uintptr_t jmp_target_addr[2];
    uintptr_t jmp_list_head;
    uintptr_t jmp_list_next[2];
    uintptr_t jmp_dest[2];
};
_Bool 
    tcg_cflags_has(CPUState *cpu, uint32_t flags);
void tcg_cflags_set(CPUState *cpu, uint32_t flags);
void tlb_init(CPUState *cpu);
void tlb_destroy(CPUState *cpu);
void tlb_flush_page(CPUState *cpu, vaddr addr);
void tlb_flush_page_all_cpus_synced(CPUState *src, vaddr addr);
void tlb_flush(CPUState *cpu);
void tlb_flush_all_cpus_synced(CPUState *src_cpu);
void tlb_flush_page_by_mmuidx(CPUState *cpu, vaddr addr,
                              uint16_t idxmap);
void tlb_flush_page_by_mmuidx_all_cpus_synced(CPUState *cpu, vaddr addr,
                                              uint16_t idxmap);
void tlb_flush_by_mmuidx(CPUState *cpu, uint16_t idxmap);
void tlb_flush_by_mmuidx_all_cpus_synced(CPUState *cpu, uint16_t idxmap);
void tlb_flush_page_bits_by_mmuidx(CPUState *cpu, vaddr addr,
                                   uint16_t idxmap, unsigned bits);
void tlb_flush_page_bits_by_mmuidx_all_cpus_synced
    (CPUState *cpu, vaddr addr, uint16_t idxmap, unsigned bits);
void tlb_flush_range_by_mmuidx(CPUState *cpu, vaddr addr,
                               vaddr len, uint16_t idxmap,
                               unsigned bits);
void tlb_flush_range_by_mmuidx_all_cpus_synced(CPUState *cpu,
                                               vaddr addr,
                                               vaddr len,
                                               uint16_t idxmap,
                                               unsigned bits);
void tlb_set_page_full(CPUState *cpu, int mmu_idx, vaddr addr,
                       CPUTLBEntryFull *full);
void tlb_set_page_with_attrs(CPUState *cpu, vaddr addr,
                             hwaddr paddr, MemTxAttrs attrs,
                             int prot, int mmu_idx, vaddr size);
void tlb_set_page(CPUState *cpu, vaddr addr,
                  hwaddr paddr, int prot,
                  int mmu_idx, vaddr size);
void *probe_access(CPUArchState *env, vaddr addr, int size,
                   MMUAccessType access_type, int mmu_idx, uintptr_t retaddr);
int probe_access_flags(CPUArchState *env, vaddr addr, int size,
                       MMUAccessType access_type, int mmu_idx,
                       
                      _Bool 
                           nonfault, void **phost, uintptr_t retaddr);
int probe_access_full(CPUArchState *env, vaddr addr, int size,
                      MMUAccessType access_type, int mmu_idx,
                      
                     _Bool 
                          nonfault, void **phost,
                      CPUTLBEntryFull **pfull, uintptr_t retaddr);
int probe_access_full_mmu(CPUArchState *env, vaddr addr, int size,
                          MMUAccessType access_type, int mmu_idx,
                          void **phost, CPUTLBEntryFull **pfull);
void tb_phys_invalidate(TranslationBlock *tb, tb_page_addr_t page_addr);
void tb_invalidate_phys_range(tb_page_addr_t start, tb_page_addr_t last);
void tb_set_jmp_target(TranslationBlock *tb, int n, uintptr_t addr);
struct MemoryRegionSection *iotlb_to_section(CPUState *cpu,
                                             hwaddr index, MemTxAttrs attrs);
tb_page_addr_t get_page_addr_code_hostp(CPUArchState *env, vaddr addr,
                                        void **hostp);
void tlb_reset_dirty(CPUState *cpu, ram_addr_t start1, ram_addr_t length);
void tlb_reset_dirty_range_all(ram_addr_t start, ram_addr_t length);
MemoryRegionSection *
address_space_translate_for_iotlb(CPUState *cpu, int asidx, hwaddr addr,
                                  hwaddr *xlat, hwaddr *plen,
                                  MemTxAttrs attrs, int *prot);
hwaddr memory_region_section_get_iotlb(CPUState *cpu,
                                       MemoryRegionSection *section);
enum {
    GDB_SIGNAL_0 = 0,
    GDB_SIGNAL_INT = 2,
    GDB_SIGNAL_QUIT = 3,
    GDB_SIGNAL_TRAP = 5,
    GDB_SIGNAL_ABRT = 6,
    GDB_SIGNAL_ALRM = 14,
    GDB_SIGNAL_STOP = 17,
    GDB_SIGNAL_IO = 23,
    GDB_SIGNAL_XCPU = 24,
    GDB_SIGNAL_UNKNOWN = 143
};
typedef struct GDBProcess {
    uint32_t pid;
    
   _Bool 
        attached;
    char *target_xml;
} GDBProcess;
enum RSState {
    RS_INACTIVE,
    RS_IDLE,
    RS_GETLINE,
    RS_GETLINE_ESC,
    RS_GETLINE_RLE,
    RS_CHKSUM1,
    RS_CHKSUM2,
};
typedef struct GDBState {
    
   _Bool 
        init;
    CPUState *c_cpu;
    CPUState *g_cpu;
    CPUState *query_cpu;
    enum RSState state;
    char line_buf[4096];
    int line_buf_index;
    int line_sum;
    int line_csum;
    GByteArray *last_packet;
    int signal;
    
   _Bool 
        multiprocess;
    GDBProcess *processes;
    int process_num;
    GString *str_buf;
    GByteArray *mem_buf;
    int sstep_flags;
    int supported_sstep_flags;
    
   _Bool 
        allow_stop_reply;
} GDBState;
extern GDBState gdbserver_state;
void gdb_put_strbuf(void);
int gdb_put_packet_binary(const char *buf, int len, 
                                                   _Bool 
                                                        dump);
void gdb_memtohex(GString *buf, const uint8_t *mem, int len);
void gdb_memtox(GString *buf, const char *mem, int len);
void gdb_read_byte(uint8_t ch);
_Bool 
    gdb_got_immediate_ack(void);
GDBProcess *gdb_get_process(uint32_t pid);
CPUState *gdb_get_first_cpu_in_process(GDBProcess *process);
CPUState *gdb_first_attached_cpu(void);
void gdb_append_thread_id(CPUState *cpu, GString *buf);
int gdb_get_cpu_index(CPUState *cpu);
unsigned int gdb_get_max_cpus(void);
_Bool 
    gdb_can_reverse(void);
int gdb_target_sigtrap(void);
void gdb_create_default_process(GDBState *s);
int gdb_signal_to_target(int sig);
int gdb_target_signal_to_gdb(int sig);
int gdb_get_char(void);
void gdb_continue(void);
int gdb_continue_partial(char *newstates);
void gdb_put_buffer(const uint8_t *buf, int len);
void gdb_init_gdbserver_state(void);
void gdb_handle_query_rcmd(GArray *params, void *ctx);
void gdb_handle_query_offsets(GArray *params, void *user_ctx);
void gdb_handle_query_xfer_auxv(GArray *params, void *user_ctx);
void gdb_handle_query_xfer_siginfo(GArray *params, void *user_ctx);
void gdb_handle_v_file_open(GArray *params, void *user_ctx);
void gdb_handle_v_file_close(GArray *params, void *user_ctx);
void gdb_handle_v_file_pread(GArray *params, void *user_ctx);
void gdb_handle_v_file_readlink(GArray *params, void *user_ctx);
void gdb_handle_query_xfer_exec_file(GArray *params, void *user_ctx);
void gdb_handle_set_catch_syscalls(GArray *params, void *user_ctx);
void gdb_handle_query_supported_user(const char *gdb_supported);
_Bool 
    gdb_handle_set_thread_user(uint32_t pid, uint32_t tid);
_Bool 
    gdb_handle_detach_user(uint32_t pid);
void gdb_handle_query_attached(GArray *params, void *ctx);
void gdb_handle_query_qemu_phy_mem_mode(GArray *params, void *ctx);
void gdb_handle_set_qemu_phy_mem_mode(GArray *params, void *ctx);
void gdb_handle_file_io(GArray *params, void *user_ctx);
_Bool 
    gdb_handled_syscall(void);
void gdb_disable_syscalls(void);
void gdb_syscall_reset(void);
void gdb_syscall_handling(const char *syscall_packet);
_Bool 
    gdb_supports_guest_debug(void);
int gdb_breakpoint_insert(CPUState *cs, int type, vaddr addr, vaddr len);
int gdb_breakpoint_remove(CPUState *cs, int type, vaddr addr, vaddr len);
void gdb_breakpoint_remove_all(CPUState *cs);
int gdb_target_memory_rw_debug(CPUState *cs, hwaddr addr,
                               uint8_t *buf, int len, 
                                                     _Bool 
                                                          is_write);
void panda_cleanup(void);
void panda_set_os_name(char *os_name);
void panda_before_find_fast(void);
void panda_disas(FILE *out, void *code, unsigned long size);
void panda_break_main_loop(void);
MemoryRegion* panda_find_ram(void);
extern 
      _Bool 
           panda_exit_loop;
extern 
      _Bool 
           panda_break_vl_loop_req;
target_ulong panda_current_asid(CPUState *env);
target_ulong panda_current_pc(CPUState *cpu);
Int128 panda_find_max_ram_address(void);
int panda_physical_memory_rw(hwaddr addr, uint8_t *buf, int len, 
                                                                _Bool 
                                                                     is_write);
int panda_physical_memory_read(hwaddr addr, uint8_t *buf, int len);
int panda_physical_memory_write(hwaddr addr, uint8_t *buf, int len);
hwaddr panda_virt_to_phys(CPUState * env, target_ulong addr);
_Bool 
    enter_priv(CPUState * cpu);
void exit_priv(CPUState * cpu);
int panda_virtual_memory_rw(CPUState * cpu, target_ulong addr,
                            uint8_t * buf, int len, 
                                                   _Bool 
                                                        is_write);
int panda_virtual_memory_read(CPUState * env, target_ulong addr,
                              uint8_t * buf, int len);
int panda_virtual_memory_write(CPUState * env, target_ulong addr,
                               uint8_t * buf, int len);
void *panda_map_virt_to_host(CPUState * env, target_ulong addr, int len);
_Bool 
    panda_in_kernel_mode(const CPUState *cpu);
_Bool 
    panda_in_kernel(const CPUState *cpu);
_Bool 
    address_in_kernel_code_linux(target_ulong addr);
_Bool 
    panda_in_kernel_code_linux(CPUState * cpu);
target_ulong panda_current_ksp(CPUState * cpu);
target_ulong panda_current_sp(const CPUState *cpu);
target_ulong panda_get_retval(const CPUState *cpu);
typedef enum {
    TCG_MO_LD_LD = 0x01,
    TCG_MO_ST_LD = 0x02,
    TCG_MO_LD_ST = 0x04,
    TCG_MO_ST_ST = 0x08,
    TCG_MO_ALL = 0x0F,
    TCG_BAR_LDAQ = 0x10,
    TCG_BAR_STRL = 0x20,
    TCG_BAR_SC = 0x30,
} TCGBar;
typedef enum {
    TCG_REG_EAX = 0,
    TCG_REG_ECX,
    TCG_REG_EDX,
    TCG_REG_EBX,
    TCG_REG_ESP,
    TCG_REG_EBP,
    TCG_REG_ESI,
    TCG_REG_EDI,
    TCG_REG_R8,
    TCG_REG_R9,
    TCG_REG_R10,
    TCG_REG_R11,
    TCG_REG_R12,
    TCG_REG_R13,
    TCG_REG_R14,
    TCG_REG_R15,
    TCG_REG_XMM0,
    TCG_REG_XMM1,
    TCG_REG_XMM2,
    TCG_REG_XMM3,
    TCG_REG_XMM4,
    TCG_REG_XMM5,
    TCG_REG_XMM6,
    TCG_REG_XMM7,
    TCG_REG_XMM8,
    TCG_REG_XMM9,
    TCG_REG_XMM10,
    TCG_REG_XMM11,
    TCG_REG_XMM12,
    TCG_REG_XMM13,
    TCG_REG_XMM14,
    TCG_REG_XMM15,
    TCG_REG_RAX = TCG_REG_EAX,
    TCG_REG_RCX = TCG_REG_ECX,
    TCG_REG_RDX = TCG_REG_EDX,
    TCG_REG_RBX = TCG_REG_EBX,
    TCG_REG_RSP = TCG_REG_ESP,
    TCG_REG_RBP = TCG_REG_EBP,
    TCG_REG_RSI = TCG_REG_ESI,
    TCG_REG_RDI = TCG_REG_EDI,
    TCG_AREG0 = TCG_REG_EBP,
    TCG_REG_CALL_STACK = TCG_REG_ESP
} TCGReg;
typedef enum {
    TCG_COND_NEVER = 0 | 0 | 0 | 0,
    TCG_COND_ALWAYS = 0 | 0 | 0 | 1,
    TCG_COND_EQ = 8 | 0 | 0 | 0,
    TCG_COND_NE = 8 | 0 | 0 | 1,
    TCG_COND_TSTEQ = 8 | 4 | 0 | 0,
    TCG_COND_TSTNE = 8 | 4 | 0 | 1,
    TCG_COND_LT = 0 | 0 | 2 | 0,
    TCG_COND_GE = 0 | 0 | 2 | 1,
    TCG_COND_GT = 0 | 4 | 2 | 0,
    TCG_COND_LE = 0 | 4 | 2 | 1,
    TCG_COND_LTU = 8 | 0 | 2 | 0,
    TCG_COND_GEU = 8 | 0 | 2 | 1,
    TCG_COND_GTU = 8 | 4 | 2 | 0,
    TCG_COND_LEU = 8 | 4 | 2 | 1,
} TCGCond;
typedef int64_t tcg_target_long;
typedef uint64_t tcg_target_ulong;
typedef uint32_t TCGRegSet;
typedef enum TCGOpcode {
INDEX_op_discard,
INDEX_op_set_label,
INDEX_op_call,
INDEX_op_br,
INDEX_op_mb,
INDEX_op_mov_i32,
INDEX_op_setcond_i32,
INDEX_op_negsetcond_i32,
INDEX_op_movcond_i32,
INDEX_op_ld8u_i32,
INDEX_op_ld8s_i32,
INDEX_op_ld16u_i32,
INDEX_op_ld16s_i32,
INDEX_op_ld_i32,
INDEX_op_st8_i32,
INDEX_op_st16_i32,
INDEX_op_st_i32,
INDEX_op_add_i32,
INDEX_op_sub_i32,
INDEX_op_mul_i32,
INDEX_op_div_i32,
INDEX_op_divu_i32,
INDEX_op_rem_i32,
INDEX_op_remu_i32,
INDEX_op_div2_i32,
INDEX_op_divu2_i32,
INDEX_op_and_i32,
INDEX_op_or_i32,
INDEX_op_xor_i32,
INDEX_op_shl_i32,
INDEX_op_shr_i32,
INDEX_op_sar_i32,
INDEX_op_rotl_i32,
INDEX_op_rotr_i32,
INDEX_op_deposit_i32,
INDEX_op_extract_i32,
INDEX_op_sextract_i32,
INDEX_op_extract2_i32,
INDEX_op_brcond_i32,
INDEX_op_add2_i32,
INDEX_op_sub2_i32,
INDEX_op_mulu2_i32,
INDEX_op_muls2_i32,
INDEX_op_muluh_i32,
INDEX_op_mulsh_i32,
INDEX_op_brcond2_i32,
INDEX_op_setcond2_i32,
INDEX_op_ext8s_i32,
INDEX_op_ext16s_i32,
INDEX_op_ext8u_i32,
INDEX_op_ext16u_i32,
INDEX_op_bswap16_i32,
INDEX_op_bswap32_i32,
INDEX_op_not_i32,
INDEX_op_neg_i32,
INDEX_op_andc_i32,
INDEX_op_orc_i32,
INDEX_op_eqv_i32,
INDEX_op_nand_i32,
INDEX_op_nor_i32,
INDEX_op_clz_i32,
INDEX_op_ctz_i32,
INDEX_op_ctpop_i32,
INDEX_op_mov_i64,
INDEX_op_setcond_i64,
INDEX_op_negsetcond_i64,
INDEX_op_movcond_i64,
INDEX_op_ld8u_i64,
INDEX_op_ld8s_i64,
INDEX_op_ld16u_i64,
INDEX_op_ld16s_i64,
INDEX_op_ld32u_i64,
INDEX_op_ld32s_i64,
INDEX_op_ld_i64,
INDEX_op_st8_i64,
INDEX_op_st16_i64,
INDEX_op_st32_i64,
INDEX_op_st_i64,
INDEX_op_add_i64,
INDEX_op_sub_i64,
INDEX_op_mul_i64,
INDEX_op_div_i64,
INDEX_op_divu_i64,
INDEX_op_rem_i64,
INDEX_op_remu_i64,
INDEX_op_div2_i64,
INDEX_op_divu2_i64,
INDEX_op_and_i64,
INDEX_op_or_i64,
INDEX_op_xor_i64,
INDEX_op_shl_i64,
INDEX_op_shr_i64,
INDEX_op_sar_i64,
INDEX_op_rotl_i64,
INDEX_op_rotr_i64,
INDEX_op_deposit_i64,
INDEX_op_extract_i64,
INDEX_op_sextract_i64,
INDEX_op_extract2_i64,
INDEX_op_ext_i32_i64,
INDEX_op_extu_i32_i64,
INDEX_op_extrl_i64_i32,
INDEX_op_extrh_i64_i32,
INDEX_op_brcond_i64,
INDEX_op_ext8s_i64,
INDEX_op_ext16s_i64,
INDEX_op_ext32s_i64,
INDEX_op_ext8u_i64,
INDEX_op_ext16u_i64,
INDEX_op_ext32u_i64,
INDEX_op_bswap16_i64,
INDEX_op_bswap32_i64,
INDEX_op_bswap64_i64,
INDEX_op_not_i64,
INDEX_op_neg_i64,
INDEX_op_andc_i64,
INDEX_op_orc_i64,
INDEX_op_eqv_i64,
INDEX_op_nand_i64,
INDEX_op_nor_i64,
INDEX_op_clz_i64,
INDEX_op_ctz_i64,
INDEX_op_ctpop_i64,
INDEX_op_add2_i64,
INDEX_op_sub2_i64,
INDEX_op_mulu2_i64,
INDEX_op_muls2_i64,
INDEX_op_muluh_i64,
INDEX_op_mulsh_i64,
INDEX_op_insn_start,
INDEX_op_exit_tb,
INDEX_op_goto_tb,
INDEX_op_goto_ptr,
INDEX_op_plugin_cb,
INDEX_op_plugin_mem_cb,
INDEX_op_qemu_ld_a32_i32,
INDEX_op_qemu_st_a32_i32,
INDEX_op_qemu_ld_a32_i64,
INDEX_op_qemu_st_a32_i64,
INDEX_op_qemu_ld_a64_i32,
INDEX_op_qemu_st_a64_i32,
INDEX_op_qemu_ld_a64_i64,
INDEX_op_qemu_st_a64_i64,
INDEX_op_qemu_st8_a32_i32,
INDEX_op_qemu_st8_a64_i32,
INDEX_op_qemu_ld_a32_i128,
INDEX_op_qemu_ld_a64_i128,
INDEX_op_qemu_st_a32_i128,
INDEX_op_qemu_st_a64_i128,
INDEX_op_mov_vec,
INDEX_op_dup_vec,
INDEX_op_dup2_vec,
INDEX_op_ld_vec,
INDEX_op_st_vec,
INDEX_op_dupm_vec,
INDEX_op_add_vec,
INDEX_op_sub_vec,
INDEX_op_mul_vec,
INDEX_op_neg_vec,
INDEX_op_abs_vec,
INDEX_op_ssadd_vec,
INDEX_op_usadd_vec,
INDEX_op_sssub_vec,
INDEX_op_ussub_vec,
INDEX_op_smin_vec,
INDEX_op_umin_vec,
INDEX_op_smax_vec,
INDEX_op_umax_vec,
INDEX_op_and_vec,
INDEX_op_or_vec,
INDEX_op_xor_vec,
INDEX_op_andc_vec,
INDEX_op_orc_vec,
INDEX_op_nand_vec,
INDEX_op_nor_vec,
INDEX_op_eqv_vec,
INDEX_op_not_vec,
INDEX_op_shli_vec,
INDEX_op_shri_vec,
INDEX_op_sari_vec,
INDEX_op_rotli_vec,
INDEX_op_shls_vec,
INDEX_op_shrs_vec,
INDEX_op_sars_vec,
INDEX_op_rotls_vec,
INDEX_op_shlv_vec,
INDEX_op_shrv_vec,
INDEX_op_sarv_vec,
INDEX_op_rotlv_vec,
INDEX_op_rotrv_vec,
INDEX_op_cmp_vec,
INDEX_op_bitsel_vec,
INDEX_op_cmpsel_vec,
INDEX_op_last_generic,
INDEX_op_x86_shufps_vec,
INDEX_op_x86_blend_vec,
INDEX_op_x86_packss_vec,
INDEX_op_x86_packus_vec,
INDEX_op_x86_psrldq_vec,
INDEX_op_x86_vperm2i128_vec,
INDEX_op_x86_punpckl_vec,
INDEX_op_x86_punpckh_vec,
INDEX_op_x86_vpshldi_vec,
INDEX_op_x86_vpshldv_vec,
INDEX_op_x86_vpshrdv_vec,
    NB_OPS,
} TCGOpcode;
typedef uint8_t tcg_insn_unit;
typedef struct TCGRelocation TCGRelocation;
struct TCGRelocation {
    struct { struct TCGRelocation *sqe_next; } next;
    tcg_insn_unit *ptr;
    intptr_t addend;
    int type;
};
typedef struct TCGOp TCGOp;
typedef struct TCGLabelUse TCGLabelUse;
struct TCGLabelUse {
    struct { struct TCGLabelUse *sqe_next; } next;
    TCGOp *op;
};
typedef struct TCGLabel TCGLabel;
struct TCGLabel {
    
   _Bool 
        present;
    
   _Bool 
        has_value;
    uint16_t id;
    union {
        uintptr_t value;
        const tcg_insn_unit *value_ptr;
    } u;
    struct { struct TCGLabelUse *sqh_first; struct TCGLabelUse **sqh_last; } branches;
    struct { struct TCGRelocation *sqh_first; struct TCGRelocation **sqh_last; } relocs;
    struct { struct TCGLabel *sqe_next; } next;
};
typedef struct TCGPool {
    struct TCGPool *next;
    int size;
    uint8_t data[] ;
} TCGPool;
typedef enum TCGType {
    TCG_TYPE_I32,
    TCG_TYPE_I64,
    TCG_TYPE_I128,
    TCG_TYPE_V64,
    TCG_TYPE_V128,
    TCG_TYPE_V256,
    TCG_TYPE_REG = TCG_TYPE_I64,
    TCG_TYPE_PTR = TCG_TYPE_I64,
} TCGType;
typedef tcg_target_ulong TCGArg;
typedef struct TCGv_i32_d *TCGv_i32;
typedef struct TCGv_i64_d *TCGv_i64;
typedef struct TCGv_i128_d *TCGv_i128;
typedef struct TCGv_ptr_d *TCGv_ptr;
typedef struct TCGv_vec_d *TCGv_vec;
typedef TCGv_ptr TCGv_env;
enum {
    TCG_BSWAP_IZ = 1,
    TCG_BSWAP_OZ = 2,
    TCG_BSWAP_OS = 4,
};
typedef enum TCGTempVal {
    TEMP_VAL_DEAD,
    TEMP_VAL_REG,
    TEMP_VAL_MEM,
    TEMP_VAL_CONST,
} TCGTempVal;
typedef enum TCGTempKind {
    TEMP_EBB,
    TEMP_TB,
    TEMP_GLOBAL,
    TEMP_FIXED,
    TEMP_CONST,
} TCGTempKind;
typedef struct TCGTemp {
    TCGReg reg:8;
    TCGTempVal val_type:8;
    TCGType base_type:8;
    TCGType type:8;
    TCGTempKind kind:3;
    unsigned int indirect_reg:1;
    unsigned int indirect_base:1;
    unsigned int mem_coherent:1;
    unsigned int mem_allocated:1;
    unsigned int temp_allocated:1;
    unsigned int temp_subindex:2;
    int64_t val;
    struct TCGTemp *mem_base;
    intptr_t mem_offset;
    const char *name;
    uintptr_t state;
    void *state_ptr;
} TCGTemp;
typedef struct TCGContext TCGContext;
typedef struct TCGTempSet {
    unsigned long l[8];
} TCGTempSet;
typedef uint32_t TCGLifeData;
struct TCGOp {
    TCGOpcode opc : 8;
    unsigned nargs : 8;
    unsigned param1 : 8;
    unsigned param2 : 8;
    TCGLifeData life;
    union { struct TCGOp *tqe_next; QTailQLink tqe_circ; } link;
    TCGRegSet output_pref[2];
    TCGArg args[];
};
;
struct TCGContext {
    uint8_t *pool_cur, *pool_end;
    TCGPool *pool_first, *pool_current, *pool_first_large;
    int nb_labels;
    int nb_globals;
    int nb_temps;
    int nb_indirects;
    int nb_ops;
    TCGType addr_type;
    int page_mask;
    uint8_t page_bits;
    uint8_t tlb_dyn_max_bits;
    uint8_t insn_start_words;
    TCGBar guest_mo;
    TCGRegSet reserved_regs;
    intptr_t current_frame_offset;
    intptr_t frame_start;
    intptr_t frame_end;
    TCGTemp *frame_temp;
    TranslationBlock *gen_tb;
    tcg_insn_unit *code_buf;
    tcg_insn_unit *code_ptr;
    void *code_gen_buffer;
    size_t code_gen_buffer_size;
    void *code_gen_ptr;
    void *data_gen_ptr;
    void *code_gen_highwater;
    CPUState *cpu;
    struct { struct TCGLabelQemuLdst *sqh_first; struct TCGLabelQemuLdst **sqh_last; } ldst_labels;
    struct TCGLabelPoolData *pool_labels;
    TCGLabel *exitreq_label;
    struct qemu_plugin_tb *plugin_tb;
    const struct DisasContextBase *plugin_db;
    struct qemu_plugin_insn *plugin_insn;
    GHashTable *const_table[6];
    TCGTempSet free_temps[6];
    TCGTemp temps[512];
    union { struct TCGOp *tqh_first; QTailQLink tqh_circ; } ops, free_ops;
    struct { struct TCGLabel *sqh_first; struct TCGLabel **sqh_last; } labels;
    TCGOp *emit_before_op;
    TCGTemp *reg_to_temp[32];
    uint16_t gen_insn_end_off[512];
    uint64_t *gen_insn_data;
    sigjmp_buf jmp_trans;
};
extern TCGContext *tcg_ctx;
extern const void *tcg_code_gen_epilogue;
extern uintptr_t tcg_splitwx_diff;
extern TCGv_env tcg_env;
_Bool 
    in_code_gen_buffer(const void *p);
void *tcg_malloc_internal(TCGContext *s, int size);
void tcg_pool_reset(TCGContext *s);
TranslationBlock *tcg_tb_alloc(TCGContext *s);
void tcg_region_reset_all(void);
size_t tcg_code_size(void);
size_t tcg_code_capacity(void);
void tcg_tb_insert(TranslationBlock *tb);
void tcg_tb_remove(TranslationBlock *tb);
TranslationBlock *tcg_tb_lookup(uintptr_t tc_ptr);
void tcg_tb_foreach(GTraverseFunc func, gpointer user_data);
size_t tcg_nb_tbs(void);
void tcg_func_start(TCGContext *s);
int tcg_gen_code(TCGContext *s, TranslationBlock *tb, uint64_t pc_start);
void tb_target_set_jmp_target(const TranslationBlock *, int,
                              uintptr_t, uintptr_t);
void tcg_set_frame(TCGContext *s, TCGReg reg, intptr_t start, intptr_t size);
typedef struct TCGArgConstraint {
    unsigned ct : 16;
    unsigned alias_index : 4;
    unsigned sort_index : 4;
    unsigned pair_index : 4;
    unsigned pair : 2;
    
   _Bool 
        oalias : 1;
    
   _Bool 
        ialias : 1;
    
   _Bool 
        newreg : 1;
    TCGRegSet regs;
} TCGArgConstraint;
enum {
    TCG_OPF_BB_EXIT = 0x01,
    TCG_OPF_BB_END = 0x02,
    TCG_OPF_CALL_CLOBBER = 0x04,
    TCG_OPF_SIDE_EFFECTS = 0x08,
    TCG_OPF_NOT_PRESENT = 0x20,
    TCG_OPF_VECTOR = 0x40,
    TCG_OPF_COND_BRANCH = 0x80
};
typedef struct TCGOpDef {
    const char *name;
    uint8_t nb_oargs, nb_iargs, nb_cargs, nb_args;
    uint8_t flags;
} TCGOpDef;
extern const TCGOpDef tcg_op_defs[];
extern const size_t tcg_op_defs_max;
_Bool 
    tcg_op_supported(TCGOpcode op, TCGType type, unsigned flags);
_Bool 
    tcg_op_deposit_valid(TCGType type, unsigned ofs, unsigned len);
void tcg_gen_call0(void *func, TCGHelperInfo *, TCGTemp *ret);
void tcg_gen_call1(void *func, TCGHelperInfo *, TCGTemp *ret, TCGTemp *);
void tcg_gen_call2(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *);
void tcg_gen_call3(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *);
void tcg_gen_call4(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *);
void tcg_gen_call5(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *);
void tcg_gen_call6(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *,
                   TCGTemp *, TCGTemp *);
void tcg_gen_call7(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *,
                   TCGTemp *, TCGTemp *, TCGTemp *);
TCGOp *tcg_emit_op(TCGOpcode opc, unsigned nargs);
void tcg_op_remove(TCGContext *s, TCGOp *op);
void tcg_remove_ops_after(TCGOp *op);
void tcg_optimize(TCGContext *s);
TCGLabel *gen_new_label(void);
typedef uintptr_t tcg_prologue_fn(CPUArchState *env, const void *tb_ptr);
extern tcg_prologue_fn *tcg_qemu_tb_exec;
void tcg_register_jit(const void *buf, size_t buf_size);
int tcg_can_emit_vec_op(TCGOpcode, TCGType, unsigned);
void tcg_expand_vec_op(TCGOpcode, TCGType, unsigned, TCGArg, ...);
uint64_t dup_const(unsigned vece, uint64_t c);
_Bool 
    tcg_can_emit_vecop_list(const TCGOpcode *, TCGType, unsigned);
void tcg_dump_ops(TCGContext *s, FILE *f, 
                                         _Bool 
                                              have_prefs);
void disas(FILE *out, const void *code, size_t size);
void target_disas(FILE *out, CPUState *cpu, const DisasContextBase *db);
void monitor_disas(Monitor *mon, CPUState *cpu, uint64_t pc,
                   int nb_insn, 
                               _Bool 
                                    is_physical);
char *plugin_disas(CPUState *cpu, const DisasContextBase *db,
                   uint64_t addr, size_t size);
const char *lookup_symbol(uint64_t orig_addr);
struct syminfo;
struct elf32_sym;
struct elf64_sym;
typedef const char *(*lookup_symbol_t)(struct syminfo *s, uint64_t orig_addr);
struct syminfo {
    lookup_symbol_t lookup_symbol;
    unsigned int disas_num_syms;
    union {
      struct elf32_sym *elf32;
      struct elf64_sym *elf64;
    } disas_symtab;
    const char *disas_strtab;
    struct syminfo *next;
};
extern struct syminfo *syminfos;
typedef struct _panda_cb_list panda_cb_list;
struct _panda_cb_list {
    panda_cb_with_context entry;
    void *owner;
    panda_cb_list *next;
    panda_cb_list *prev;
    
   _Bool 
        enabled;
    void* context;
};
panda_cb_list *panda_cb_list_next(panda_cb_list *plist);
void panda_enable_plugin(void *plugin);
void panda_disable_plugin(void *plugin);
typedef struct panda_plugin {
    char *name;
    void *plugin;
    
   _Bool 
        unload;
    
   _Bool 
        exported_symbols;
} panda_plugin;
panda_cb_with_context panda_get_cb_trampoline(panda_cb_type type);
void panda_register_callback(void *plugin, panda_cb_type type, panda_cb cb);
void panda_register_callback_with_context(void *plugin, panda_cb_type type, panda_cb_with_context cb, void* context);
void panda_disable_callback(void *plugin, panda_cb_type type, panda_cb cb);
void panda_disable_callback_with_context(void *plugin, panda_cb_type type, panda_cb_with_context cb, void* context);
void panda_enable_callback(void *plugin, panda_cb_type type, panda_cb cb);
void panda_enable_callback_with_context(void *plugin, panda_cb_type type, panda_cb_with_context cb, void* context);
void panda_unregister_callbacks(void *plugin);
_Bool 
    panda_load_plugin(const char *filename, const char *plugin_name);
_Bool 
    panda_add_arg(const char *plugin_name, const char *plugin_arg);
void * panda_get_plugin_by_name(const char *name);
void panda_unload_plugin_by_name(const char* name);
void panda_do_unload_plugin(int index);
void panda_unload_plugin(void *plugin);
void panda_unload_plugin_idx(int idx);
void panda_unload_plugins(void);
extern 
      _Bool 
           panda_update_pc;
extern 
      _Bool 
           panda_use_memcb;
extern panda_cb_list *panda_cbs[PANDA_CB_LAST];
extern 
      _Bool 
           panda_tb_chaining;
typedef enum OSFamilyEnum { OS_UNKNOWN, OS_WINDOWS, OS_LINUX, OS_FREEBSD } PandaOsFamily;
extern char *panda_os_name;
extern char *panda_os_family;
extern char *panda_os_variant;
extern uint32_t panda_os_bits;
extern PandaOsFamily panda_os_familyno;
_Bool 
    panda_break_exec(void);
_Bool 
    panda_flush_tb(void);
void panda_do_flush_tb(void);
void panda_do_break_exec(void);
void panda_enable_precise_pc(void);
void panda_disable_precise_pc(void);
void panda_enable_memcb(void);
void panda_disable_memcb(void);
void panda_enable_llvm(void);
void panda_enable_llvm_no_exec(void);
void panda_disable_llvm(void);
void panda_enable_llvm_helpers(void);
void panda_disable_llvm_helpers(void);
int panda_write_current_llvm_bitcode_to_file(const char* path);
uintptr_t panda_get_current_llvm_module(void);
void panda_disable_tb_chaining(void);
void panda_enable_tb_chaining(void);
void panda_memsavep(FILE *file);
int panda_vm_quit(void);
char* panda_get_rr_name(void);
typedef struct panda_arg {
    char *argptr;
    char *key;
    char *value;
} panda_arg;
typedef struct panda_arg_list {
    int nargs;
    panda_arg *list;
    char *plugin_name;
} panda_arg_list;
panda_arg_list *panda_get_args(const char *plugin_name);
void panda_free_args(panda_arg_list *args);
target_ulong panda_parse_ulong(panda_arg_list *args, const char *argname, target_ulong defval);
target_ulong panda_parse_ulong_req(panda_arg_list *args, const char *argname, const char *help);
target_ulong panda_parse_ulong_opt(panda_arg_list *args, const char *argname, target_ulong defval, const char *help);
uint32_t panda_parse_uint32(panda_arg_list *args, const char *argname, uint32_t defval);
uint32_t panda_parse_uint32_req(panda_arg_list *args, const char *argname, const char *help);
uint32_t panda_parse_uint32_opt(panda_arg_list *args, const char *argname, uint32_t defval, const char *help);
uint64_t panda_parse_uint64(panda_arg_list *args, const char *argname, uint64_t defval);
uint64_t panda_parse_uint64_req(panda_arg_list *args, const char *argname, const char *help);
uint64_t panda_parse_uint64_opt(panda_arg_list *args, const char *argname, uint64_t defval, const char *help);
double panda_parse_double(panda_arg_list *args, const char *argname, double defval);
double panda_parse_double_req(panda_arg_list *args, const char *argname, const char *help);
double panda_parse_double_opt(panda_arg_list *args, const char *argname, double defval, const char *help);
_Bool 
    panda_parse_bool(panda_arg_list *args, const char *argname);
_Bool 
    panda_parse_bool_req(panda_arg_list *args, const char *argname, const char *help);
_Bool 
    panda_parse_bool_opt(panda_arg_list *args, const char *argname, const char *help);
const char *panda_parse_string(panda_arg_list *args, const char *argname, const char *defval);
const char *panda_parse_string_req(panda_arg_list *args, const char *argname, const char *help);
const char *panda_parse_string_opt(panda_arg_list *args, const char *argname, const char *defval, const char *help);
extern gchar *panda_argv[32];
extern int panda_argc;
char** str_split(char *a_str, const char a_delim);
char* resolve_file_from_plugin_directory(const char* file_name_fmt, const char* name);
char *panda_plugin_path(const char *name);
char* panda_shared_library_path(const char* name);
void panda_require(const char *name);
void panda_require_from_library(const char *plugin_name, char **plugin_args, uint32_t num_args);
_Bool 
    panda_is_callback_enabled(void *plugin, panda_cb_type type, panda_cb cb);
int panda_init(int argc, char **argv, char **envp);
int panda_run(void);
void panda_stop(int code);
void panda_cont(void);
void _panda_set_library_mode(const 
                                  _Bool
                                      );
void panda_start_pandalog(const char *name);
int panda_snap(char *name);
int panda_delvm(char *name);
int panda_revert(char *name);
void panda_reset(void);
int panda_finish(void);
_Bool 
    panda_was_aborted(void);
void panda_set_qemu_path(char* filepath);
void panda_set_extra_plugin_path(char *filepath);
int panda_init_plugin(char *plugin_name, char **plugin_args, uint32_t num_args);
void panda_register_callback_helper(void* plugin, panda_cb_type type, panda_cb* cb);
void panda_enable_callback_helper(void *plugin, panda_cb_type type, panda_cb* cb);
void panda_disable_callback_helper(void *plugin, panda_cb_type type, panda_cb* cb);
uint64_t rr_get_guest_instr_count_external(void);
int panda_virtual_memory_read_external(CPUState *cpu, target_ulong addr, char *buf, int len);
int panda_virtual_memory_write_external(CPUState *cpu, target_ulong addr, char *buf, int len);
int panda_physical_memory_read_external(hwaddr addr, uint8_t *buf, int len);
int panda_physical_memory_write_external(hwaddr addr, uint8_t *buf, int len);
target_ulong panda_get_retval_external(const CPUState *cpu);
MemTxResult PandaPhysicalAddressToRamOffset_external(ram_addr_t* out, hwaddr addr, 
                                                                                  _Bool 
                                                                                       is_write);
_Bool 
    panda_in_kernel_external(const CPUState *cpu);
_Bool 
    panda_in_kernel_mode_external(const CPUState *cpu);
_Bool 
    panda_in_kernel_code_linux_external(CPUState *cpu);
target_ulong panda_current_ksp_external(CPUState *cpu);
target_ulong panda_current_sp_external(const CPUState *cpu);
target_ulong panda_current_sp_masked_pagesize_external(const CPUState *cpu, target_ulong pagesize);
target_ulong panda_virt_to_phys_external(CPUState *cpu, target_ulong addr);
void panda_setup_signal_handling(void (*sigfun) (int, void*, void *));
void map_memory(char* name, uint64_t size, uint64_t address);
void panda_init_monitor(void);
char* panda_monitor_run(char* buf);
CPUState* get_cpu(void);
unsigned long garray_len(GArray *list);
CPUArchState *panda_cpu_env(CPUState *cpu);
void panda_cleanup_record(void);
CPUState *panda_current_cpu(int index);
CPUState *panda_cpu_in_translate(void);
TranslationBlock *panda_get_tb(struct qemu_plugin_tb *tb);
int panda_get_memcb_status(void);
       
typedef struct {
    uint32_t max;
    uint32_t max_generic;
    uint32_t max_args;
} syscall_meta_t;
typedef enum {
    SYSCALL_ARG_U64 = 0x00,
    SYSCALL_ARG_U32,
    SYSCALL_ARG_U16,
    SYSCALL_ARG_S64 = 0x10,
    SYSCALL_ARG_S32,
    SYSCALL_ARG_S16,
    SYSCALL_ARG_BUF_PTR = 0x20,
    SYSCALL_ARG_STRUCT_PTR,
    SYSCALL_ARG_STR_PTR,
    SYSCALL_ARG_STRUCT = 0x30,
    SYSCALL_ARG_ARR
} syscall_argtype_t;
typedef struct {
    int no;
    const char *name;
    int nargs;
    syscall_argtype_t *argt;
    uint8_t *argsz;
    const char* const *argn;
    const char* const *argtn;
    
   _Bool 
        noreturn;
} syscall_info_t;
    typedef char gchar;
    void load_syscall_info(const gchar *arch, syscall_info_t **syscall_info, syscall_meta_t **syscall_meta);
       
       
struct syscall_ctx {
    int no;
    target_ptr_t asid;
    target_ptr_t retaddr;
    uint8_t args[17]
                [8];
    
   _Bool 
        double_return;
    int profile;
};
typedef struct syscall_ctx syscall_ctx_t;
target_long get_return_val(CPUState *env, int profile);
target_ptr_t mask_retaddr_to_pc(target_ptr_t retaddr, syscall_ctx_t *);
target_ptr_t calc_retaddr(CPUState *env, syscall_ctx_t*, target_ptr_t pc);
uint32_t get_32(CPUState *env, syscall_ctx_t*, uint32_t argnum);
int32_t get_s32(CPUState *env, syscall_ctx_t*, uint32_t argnum);
uint64_t get_64(CPUState *env, syscall_ctx_t*, uint32_t argnum);
int64_t get_s64(CPUState *env, syscall_ctx_t*, uint32_t argnum);
uint32_t get_return_32(CPUState *env, syscall_ctx_t*, uint32_t argnum);
int32_t get_return_s32(CPUState *env, syscall_ctx_t*, uint32_t argnum);
uint64_t get_return_64(CPUState *env, syscall_ctx_t*, uint32_t argnum);
int64_t get_return_s64(CPUState *env, syscall_ctx_t*, uint32_t argnum);
void sysinfo_load_profile(int profile, syscall_info_t **syscall_info, syscall_meta_t **syscall_meta);
typedef void (*on_all_sys_enter_t)(CPUState *cpu, target_ulong pc, target_ulong callno); typedef void (*on_all_sys_enter_with_context_t)(void* context, CPUState *cpu, target_ulong pc, target_ulong callno);;
typedef void (*on_all_sys_enter2_t)(CPUState *cpu, target_ulong pc, const syscall_info_t *call, const syscall_ctx_t *ctx); typedef void (*on_all_sys_enter2_with_context_t)(void* context, CPUState *cpu, target_ulong pc, const syscall_info_t *call, const syscall_ctx_t *ctx);;
typedef void (*on_all_sys_return_t)(CPUState *cpu, target_ulong pc, target_ulong callno); typedef void (*on_all_sys_return_with_context_t)(void* context, CPUState *cpu, target_ulong pc, target_ulong callno);;
typedef void (*on_all_sys_return2_t)(CPUState *cpu, target_ulong pc, const syscall_info_t *call, const syscall_ctx_t *ctx); typedef void (*on_all_sys_return2_with_context_t)(void* context, CPUState *cpu, target_ulong pc, const syscall_info_t *call, const syscall_ctx_t *ctx);;
typedef void (*on_unknown_sys_enter_t)(CPUState *cpu, target_ulong pc, target_ulong callno); typedef void (*on_unknown_sys_enter_with_context_t)(void* context, CPUState *cpu, target_ulong pc, target_ulong callno);;
typedef void (*on_unknown_sys_return_t)(CPUState *cpu, target_ulong pc, target_ulong callno); typedef void (*on_unknown_sys_return_with_context_t)(void* context, CPUState *cpu, target_ulong pc, target_ulong callno);;
       
typedef struct osi_proc_mem {
    target_ptr_t start_brk;
    target_ptr_t brk;
} OsiProcMem;
typedef struct osi_proc_handle_struct {
    target_ptr_t taskd;
    target_ptr_t asid;
} OsiProcHandle;
typedef struct osi_thread_struct {
    target_pid_t pid;
    target_pid_t tid;
} OsiThread;
typedef struct osi_page_struct {
    target_ptr_t start;
    target_ulong len;
} OsiPage;
typedef struct osi_module_struct {
    target_ptr_t modd;
    target_ptr_t base;
    target_ptr_t size;
    char *file;
    char *name;
    target_ulong offset;
    target_ulong flags;
} OsiModule;
typedef struct osi_proc_struct {
    target_ptr_t taskd;
    target_ptr_t pgd;
    target_ptr_t asid;
    target_pid_t pid;
    target_pid_t ppid;
    char *name;
    OsiPage *pages;
    uint64_t create_time;
} OsiProc;
static void (*free_osiprochandle_contents)(OsiProcHandle *) = 
                                                                                     ((void *)0)
                                                                                         ;
static void (*free_osithread_contents)(OsiThread *) = 
                                                                             ((void *)0)
                                                                                 ;
static void (*free_osipage_contents)(OsiPage *) = 
                                                                         ((void *)0)
                                                                             ;
typedef void (*on_all_sys_enter_t)(CPUState *cpu, target_ulong pc, target_ulong callno);
void ppp_add_cb_on_all_sys_enter(on_all_sys_enter_t);
_Bool 
    ppp_remove_cb_on_all_sys_enter(on_all_sys_enter_t);
typedef void (*on_all_sys_enter2_t)(CPUState *cpu, target_ulong pc, const syscall_info_t *call, const syscall_ctx_t *ctx);
void ppp_add_cb_on_all_sys_enter2(on_all_sys_enter2_t);
_Bool 
    ppp_remove_cb_on_all_sys_enter2(on_all_sys_enter2_t);
typedef void (*on_all_sys_return_t)(CPUState *cpu, target_ulong pc, target_ulong callno);
void ppp_add_cb_on_all_sys_return(on_all_sys_return_t);
_Bool 
    ppp_remove_cb_on_all_sys_return(on_all_sys_return_t);
typedef void (*on_all_sys_return2_t)(CPUState *cpu, target_ulong pc, const syscall_info_t *call, const syscall_ctx_t *ctx);
void ppp_add_cb_on_all_sys_return2(on_all_sys_return2_t);
_Bool 
    ppp_remove_cb_on_all_sys_return2(on_all_sys_return2_t);
typedef void (*on_unknown_sys_enter_t)(CPUState *cpu, target_ulong pc, target_ulong callno);
void ppp_add_cb_on_unknown_sys_enter(on_unknown_sys_enter_t);
_Bool 
    ppp_remove_cb_on_unknown_sys_enter(on_unknown_sys_enter_t);
typedef void (*on_unknown_sys_return_t)(CPUState *cpu, target_ulong pc, target_ulong callno);
void ppp_add_cb_on_unknown_sys_return(on_unknown_sys_return_t);
_Bool 
    ppp_remove_cb_on_unknown_sys_return(on_unknown_sys_return_t);
typedef void (*on_get_processes_t)(CPUState *, GArray **);
void ppp_add_cb_on_get_processes(on_get_processes_t);
_Bool 
    ppp_remove_cb_on_get_processes(on_get_processes_t);
typedef void (*on_get_process_handles_t)(CPUState *, GArray **);
void ppp_add_cb_on_get_process_handles(on_get_process_handles_t);
_Bool 
    ppp_remove_cb_on_get_process_handles(on_get_process_handles_t);
typedef void (*on_get_current_process_t)(CPUState *, OsiProc **);
void ppp_add_cb_on_get_current_process(on_get_current_process_t);
_Bool 
    ppp_remove_cb_on_get_current_process(on_get_current_process_t);
typedef void (*on_get_current_process_handle_t)(CPUState *, OsiProcHandle **);
void ppp_add_cb_on_get_current_process_handle(on_get_current_process_handle_t);
_Bool 
    ppp_remove_cb_on_get_current_process_handle(on_get_current_process_handle_t);
typedef void (*on_get_process_t)(CPUState *, const OsiProcHandle *, OsiProc **);
void ppp_add_cb_on_get_process(on_get_process_t);
_Bool 
    ppp_remove_cb_on_get_process(on_get_process_t);
typedef void (*on_get_proc_mem_t)(CPUState *cpu, const OsiProc *p, OsiProcMem **);
void ppp_add_cb_on_get_proc_mem(on_get_proc_mem_t);
_Bool 
    ppp_remove_cb_on_get_proc_mem(on_get_proc_mem_t);
typedef void (*on_get_modules_t)(CPUState *, GArray **);
void ppp_add_cb_on_get_modules(on_get_modules_t);
_Bool 
    ppp_remove_cb_on_get_modules(on_get_modules_t);
typedef void (*on_get_mappings_t)(CPUState *, OsiProc *, GArray**);
void ppp_add_cb_on_get_mappings(on_get_mappings_t);
_Bool 
    ppp_remove_cb_on_get_mappings(on_get_mappings_t);
typedef void (*on_get_file_mappings_t)(CPUState *, OsiProc *, GArray**);
void ppp_add_cb_on_get_file_mappings(on_get_file_mappings_t);
_Bool 
    ppp_remove_cb_on_get_file_mappings(on_get_file_mappings_t);
typedef void (*on_get_heap_mappings_t)(CPUState *, OsiProc *, GArray**);
void ppp_add_cb_on_get_heap_mappings(on_get_heap_mappings_t);
_Bool 
    ppp_remove_cb_on_get_heap_mappings(on_get_heap_mappings_t);
typedef void (*on_get_stack_mappings_t)(CPUState *, OsiProc *, GArray**);
void ppp_add_cb_on_get_stack_mappings(on_get_stack_mappings_t);
_Bool 
    ppp_remove_cb_on_get_stack_mappings(on_get_stack_mappings_t);
typedef void (*on_get_unknown_mappings_t)(CPUState *, OsiProc *, GArray**);
void ppp_add_cb_on_get_unknown_mappings(on_get_unknown_mappings_t);
_Bool 
    ppp_remove_cb_on_get_unknown_mappings(on_get_unknown_mappings_t);
typedef void (*on_get_mapping_by_addr_t)(CPUState *, OsiProc *, const target_ptr_t, OsiModule **);
void ppp_add_cb_on_get_mapping_by_addr(on_get_mapping_by_addr_t);
_Bool 
    ppp_remove_cb_on_get_mapping_by_addr(on_get_mapping_by_addr_t);
typedef void (*on_get_mapping_base_address_by_name_t)(CPUState *, OsiProc *, const char *, target_ptr_t *);
void ppp_add_cb_on_get_mapping_base_address_by_name(on_get_mapping_base_address_by_name_t);
_Bool 
    ppp_remove_cb_on_get_mapping_base_address_by_name(on_get_mapping_base_address_by_name_t);
typedef void (*on_has_mapping_prefix_t)(CPUState *, OsiProc *, const char *, 
                                                                            _Bool 
                                                                                 *);
void ppp_add_cb_on_has_mapping_prefix(on_has_mapping_prefix_t);
_Bool 
    ppp_remove_cb_on_has_mapping_prefix(on_has_mapping_prefix_t);
typedef void (*on_get_current_thread_t)(CPUState *, OsiThread **);
void ppp_add_cb_on_get_current_thread(on_get_current_thread_t);
_Bool 
    ppp_remove_cb_on_get_current_thread(on_get_current_thread_t);
typedef void (*on_get_process_pid_t)(CPUState *, const OsiProcHandle *, target_pid_t *);
void ppp_add_cb_on_get_process_pid(on_get_process_pid_t);
_Bool 
    ppp_remove_cb_on_get_process_pid(on_get_process_pid_t);
typedef void (*on_get_process_ppid_t)(CPUState *, const OsiProcHandle *, target_pid_t *);
void ppp_add_cb_on_get_process_ppid(on_get_process_ppid_t);
_Bool 
    ppp_remove_cb_on_get_process_ppid(on_get_process_ppid_t);
typedef void (*on_task_change_t)(CPUState *);
void ppp_add_cb_on_task_change(on_task_change_t);
_Bool 
    ppp_remove_cb_on_task_change(on_task_change_t);
       
char *osi_linux_fd_to_filename(CPUState *env, OsiProc *p, int fd);
target_ulong walk_page_table(CPUState *cpu, target_ulong virtual_address);
unsigned long long osi_linux_fd_to_pos(CPUState *env, OsiProc *p, int fd);
target_ptr_t ext_get_file_struct_ptr(CPUState *env, target_ptr_t task_struct, int fd);
target_ptr_t ext_get_file_dentry(CPUState *env, target_ptr_t file_struct);
target_ulong osi_linux_virt_to_phys(CPUState *cpu, target_ulong addr);
int osi_linux_virtual_memory_read(CPUState *cpu, target_ulong addr, uint8_t *buf, int len);
int osi_linux_virtual_memory_write(CPUState *cpu, target_ulong addr, uint8_t *buf, int len);
struct hook;
struct symbol;
typedef 
       _Bool 
            (*hook_func_t)(CPUState *, TranslationBlock *, struct hook* h);
typedef 
       _Bool 
            (*dynamic_symbol_hook_func_t)(CPUState *, TranslationBlock *, struct hook* h);
typedef union hooks_panda_cb {
    void (*before_tcg_codegen)(CPUState *env, TranslationBlock *tb, struct hook*);
    void (*before_block_translate)(CPUState *env, target_ptr_t pc, struct hook*);
    void (*block_translate)(CPUState *env, struct qemu_plugin_tb *tb, struct hook*);
    void (*after_block_translate)(CPUState *env, TranslationBlock *tb, struct hook*);
    
   _Bool 
        (*before_block_exec_invalidate_opt)(CPUState *env, TranslationBlock *tb, struct hook*);
    void (*before_block_exec)(CPUState *env, TranslationBlock *tb, struct hook*);
    void (*after_block_exec)(CPUState *env, TranslationBlock *tb, uint8_t exitCode, struct hook*);
    void (*start_block_exec)(CPUState *env, TranslationBlock *tb, struct hook*);
    void (*end_block_exec)(CPUState *env, TranslationBlock *tb, struct hook*);
} hooks_panda_cb;
enum kernel_mode{
    MODE_ANY,
    MODE_KERNEL_ONLY,
    MODE_USER_ONLY,
};
typedef struct hook {
    uint64_t addr;
    uint64_t asid;
    panda_cb_type type;
    hooks_panda_cb cb;
    enum kernel_mode km;
    
   _Bool 
        enabled;
    void* context;
} hook;
struct symbol_hook {
    char name[256];
    uint64_t offset;
    
   _Bool 
        hook_offset;
    char section[256];
    panda_cb_type type;
    hooks_panda_cb cb;
};
void add_hook(struct hook* h);
void enable_hooking();
void disable_hooking();
void add_symbol_hook(struct symbol_hook* h);
struct dynamic_symbol_hook {
    char library_name[256];
    char symbol[256];
    dynamic_symbol_hook_func_t cb;
};
       
target_ulong get_id(CPUState *cpu);
_Bool 
    id_is_initialized(void);
       
const syscall_info_t *get_syscall_info(uint32_t callno);
const syscall_meta_t *get_syscall_meta(void);
target_long get_syscall_retval(CPUState *cpu);
       
GArray *get_process_handles(CPUState *cpu);
OsiThread *get_current_thread(CPUState *cpu);
GArray *get_modules(CPUState *cpu);
GArray *get_mappings(CPUState *cpu, OsiProc *p);
OsiProcMem *get_proc_mem(CPUState *cpu, const OsiProc *p);
GArray *get_file_mappings(CPUState *cpu, OsiProc *p);
GArray *get_heap_mappings(CPUState *cpu, OsiProc *p);
GArray *get_stack_mappings(CPUState *cpu, OsiProc *p);
GArray *get_unknown_mappings(CPUState *cpu, OsiProc *p);
OsiModule *get_mapping_by_addr(CPUState *cpu, OsiProc *p, const target_ptr_t addr);
target_ptr_t get_mapping_base_address_by_name(CPUState *cpu, OsiProc *p, const char *name);
_Bool 
    has_mapping_prefix(CPUState *cpu, OsiProc *p, const char *prefix);
GArray *get_processes(CPUState *cpu);
OsiProc *get_current_process(CPUState *cpu);
OsiModule* get_one_module(GArray *osimodules, unsigned int idx);
OsiProc* get_one_proc(GArray *osiprocs, unsigned int idx);
void cleanup_garray(GArray *g);
_Bool 
    in_shared_object(CPUState *cpu, OsiProc *p);
OsiProcHandle *get_current_process_handle(CPUState *cpu);
OsiProc *get_process(CPUState *cpu, const OsiProcHandle *h);
target_pid_t get_process_pid(CPUState *cpu, const OsiProcHandle *h);
target_pid_t get_process_ppid(CPUState *cpu, const OsiProcHandle *h);
void notify_task_change(CPUState *cpu);