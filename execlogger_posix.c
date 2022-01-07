/*
 * execlogger
 *
 * ENVs:
 *   RS_EXECLOG_LOGDIR
 *     Full path to logdir
 *
 *   RS_EXECLOG_CONTEXT
 *     Value that will be copied into CNTX attrib.
 *
 *   RS_EXECLOG_IDENT
 *     Value that will be copied into CNTX attrib.
 *
 * ATTRIBs:
 *   IDNP: Previous 7bit ASCII Ident value, copied from IDENT envvar.
 *   IDNT: 7bit ASCII Ident value, generated every time.
 *   WRKD: Woking directory
 *   CNTX: 7bit ASCII Context value, copied from CONTEXT envvar.
 *   TIMF: FIXME
 *   ARGB: Argument list start (null string for process argument)
 *   ARGD: Process arguments
 *   ARGE: Argument list terminator
 *   PRES: Process result(32bits?)
 *   FILN: File name
 *   FILD: File data
 *   PIPO: Buffered pipe data (stdout)
 *   PIPE: Buffered pipe data (stderr)
 */

#include <sys/types.h>
#include <fcntl.h>
#include <wait.h>
#include <unistd.h>
#include <time.h>
#include <spawn.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define FOURCC(a,b,c,d) \
    (uint32_t)(((uint32_t)a) + \
               ((uint32_t)b<<8) + \
               ((uint32_t)c<<16) + \
               ((uint32_t)d<<24))


#define TAG_IDNP FOURCC('I','D','N','P')
#define TAG_IDNT FOURCC('I','D','N','T')
#define TAG_WRKD FOURCC('W','R','K','D')
#define TAG_CNTX FOURCC('C','N','T','X')
#define TAG_TIMF FOURCC('T','I','M','F')
#define TAG_ARGB FOURCC('A','R','G','B')
#define TAG_ARGD FOURCC('A','R','G','D')
#define TAG_ARGE FOURCC('A','R','G','E')
#define TAG_PRES FOURCC('P','R','E','S')
#define TAG_FILN FOURCC('F','I','L','N')
#define TAG_FILD FOURCC('F','I','L','D')
#define TAG_PIPO FOURCC('P','I','P','O')
#define TAG_PIPE FOURCC('P','I','P','E')

/* Clocking */
uint64_t
time_current(void){
    int r;
    struct timespec tim;
    r = clock_gettime(CLOCK_MONOTONIC, &tim);
    if(r){
        return 0;
    }
    return tim.tv_sec * 1000 * 1000 * 1000 + tim.tv_nsec;
}

/* String I/O */
#define MAX_PARAMLEN 4096
static int /* errno */
envgetstr(const char* name, char* out, size_t outlen){
    int r;
    char* p;
    p = getenv(name);
    if(! p){
        return ENOENT;
    }
    strncpy(out, p, outlen);
    return 0;
}

static int /* errno */
envputstr(char* ident){ // FIXME: Fix Win32 as well
    if(setenv("RS_EXECLOG_IDENT", ident, 1) < 0){
        return errno;
    }else{
        return 0;
    }
}

/* LOGFILE */
FILE* theLogfile = NULL;
sem_t sem_logfile;
char prev_ident[MAX_PARAMLEN];
char my_ident[MAX_PARAMLEN];

static void
logfile_put_blob(uint32_t tag, char* data, size_t len){
    int r;
    uint64_t datalen = len;

    for(;;){
        errno = 0;
        r = sem_wait(&sem_logfile);
        if(r == -1){
            if(errno == EINTR){
                continue;
            }
            abort();
        }else if(r == 0){
            break;
        }
    }

    if(tag == TAG_PIPO){
        data[len] = 0;
        printf("%s",data);
    }else if(tag == TAG_PIPE){
        data[len] = 0;
        fprintf(stderr,"%s",data);
    }

    if(theLogfile){
        (void)fwrite(&tag, sizeof(uint32_t), 1, theLogfile);
        (void)fwrite(&datalen, sizeof(uint64_t), 1, theLogfile);
        (void)fwrite(data, len, 1, theLogfile);
    }

    sem_post(&sem_logfile);
}

static int /* Win32 error */
logfile_open(void){
    char pth[MAX_PARAMLEN];
    char curdir[4096];
    char* p;
    size_t curdir_len;
    char logpath[MAX_PARAMLEN];
    int r;

    sem_init(&sem_logfile, 0, 1);

    p = getcwd(curdir, 4096);
    if(! p){
        return errno;
    }
    curdir_len = strnlen(curdir, 4096);

    r = envgetstr("RS_EXECLOG_IDENT", prev_ident, MAX_PARAMLEN);
    if(r){
        prev_ident[0] = 0;
    }
    r = envgetstr("RS_EXECLOG_LOGDIR", pth, MAX_PARAMLEN);
    if(r){
        return r;
    }
    snprintf(my_ident,MAX_PARAMLEN,"rslog-%016llx-%ld",time_current(),
             (long)getpid());
    r = envputstr(my_ident);
    if(r){
        return r;
    }

    snprintf(logpath,MAX_PARAMLEN,"%s/%s.bin",pth,my_ident);

    theLogfile = fopen(logpath, "w+b");
    if(!theLogfile){
        return 0xdeadbeef;
    }

    logfile_put_blob(TAG_IDNP, prev_ident, strlen(prev_ident));
    logfile_put_blob(TAG_IDNT, my_ident, strlen(my_ident));
    logfile_put_blob(TAG_WRKD, curdir, curdir_len);

    return 0;
}

static void
logfile_close(void){
    if(theLogfile){
        fclose(theLogfile);
    }
}

/* ARGS */
#define MAX_ARGSIZE (1024*1024*16)
static char procname[4096];
static char** newargs = NULL;
static int newargc = 0;

static void
args_record(int ac, char** av){
    int i;

    logfile_put_blob(TAG_ARGB, NULL, 0);
    for(i=0;i!=ac;i++){
        logfile_put_blob(TAG_ARGD, av[i], strlen(av[i]));
    }
    logfile_put_blob(TAG_ARGE, NULL, 0);
}

#if defined(__CYGWIN__) || defined(__linux__)
static int /* errno */
execname(char* buf, size_t buflen){
    int r;
    size_t wcnt;
    if(buflen == 0){
        return EINVAL;
    }
    /* readlink(2) does not terminate string */
    wcnt = buflen-1;
    r = readlink("/proc/self/exe", buf, wcnt);
    if(r == -1){
        return errno;
    }
    buf[r] = 0;
    return 0;
}

#else
#error IMPLEMENT execname!
#endif

static int /* errno */
args_gen(int ac, char** av){
    int r;
    char myname[4096];
    size_t mynamelen;
    FILE* check;
    /* No change to av, ac */
    newargs = av;
    newargc = ac;

    /* Get realname */
    r = execname(myname, 4096);
    if(r){
        return r;
    }

    /* FIXME: Do this more seriously */
    /* try: ORIG.traced */
    mynamelen = strnlen(myname, 4096);
    if(mynamelen <= 1){
        return ENOENT;
    }
    /* Allow Cygwin ".exe" extension */
    if(strncasecmp(&myname[mynamelen-4], ".exe", 4) == 0){
        memcpy(&myname[mynamelen-4], ".traced.exe\0", 12);
    }else{
        memcpy(&myname[mynamelen], ".traced\0", 8);
    }

    memcpy(procname, myname, 4096);

    return 0;
}

static void
args_term(void){
    // do nothing
}

static char**
args_get(void){
    return newargs;
}

static int
args_getcount(void){
    return newargc;
}

static char*
args_getprocname(void){
    return procname;
}

/* EXECPIPE */
#define PIPEBUFLEN (1024*1024*16)
typedef struct {
    int fd;
    uint32_t tag;
    pthread_t t;
}readerparams_t;

readerparams_t rp1;
readerparams_t rp2;

static void*
thr_readpipe(void* param){
    void* pipebuf;
    ssize_t r;
    int e;
    readerparams_t rp;
    rp = *(readerparams_t *)param;
    pipebuf = malloc(PIPEBUFLEN);
    for(;;){
        errno = 0;
        r = read(rp.fd, pipebuf, PIPEBUFLEN);
        e = errno;
        if(r<0){
            close(rp.fd);
            if(e == EINTR){
                continue;
            }else{
                return (void*)1;
            }
        }else{
            if(r){
                logfile_put_blob(rp.tag, pipebuf, r);
            }else{
                //fprintf(stderr, "Warn: Zero read\n");
                return (void*)2;
            }
        }
    }
}

static int /* errno */
newpipe(int out[2]){ /* read, write */
    int r;
    r = pipe(out);
    if(r){
        return errno;
    }
    return 0;
}


static int /* errno */
launch(pid_t* out_pid){
    int h1[2];
    int h2[2];
    int r,e;
    pthread_attr_t attr;
    posix_spawn_file_actions_t act;


    newpipe(h1);
    newpipe(h2);

#if 0
    printf("Launch: [");
    wprintf(L"%s", args_get());
    printf("]\n");
#endif

    posix_spawn_file_actions_init(&act);
    posix_spawn_file_actions_addclose(&act, h1[0]);
    posix_spawn_file_actions_addclose(&act, h2[0]);
    posix_spawn_file_actions_addopen(&act, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&act, h1[1], 1);
    posix_spawn_file_actions_adddup2(&act, h2[1], 2);
    posix_spawn_file_actions_addclose(&act, h1[1]);
    posix_spawn_file_actions_addclose(&act, h2[1]);

    errno = 0;
    r = posix_spawn(out_pid, args_getprocname(), &act,
                    NULL, args_get(), environ);
    e = errno;
    posix_spawn_file_actions_destroy(&act);

    close(h1[1]);
    close(h2[1]);


    if(r){
        return e;
    }else{
        pthread_attr_init(&attr);

        rp1.fd = h1[0];
        rp1.tag = TAG_PIPO;
        rp2.fd = h2[0];
        rp2.tag = TAG_PIPE;

        r = pthread_create(&rp1.t, &attr, thr_readpipe, &rp1);
        r = pthread_create(&rp2.t, &attr, thr_readpipe, &rp2);

        return 0;
    }
}

static int /* Return value */
mainloop(pid_t pid){
    int r;
    int code;
    int st;
    pid_t p;

    code = -1;
    for(;;){
        p = waitpid(pid, &st, 0);
        //printf("Waitpid(%d) %d,%d\n",pid, p, st);
        if(p < 0){
            continue;
        }
        /* Wait for writer threads */
        pthread_join(rp1.t, NULL);
        pthread_join(rp2.t, NULL);
        if(WIFEXITED(st)){
            return WEXITSTATUS(st);
        }
        if(WIFSIGNALED(st)){
            return WTERMSIG(st);
        }
        /* Something wrong */
        return 1;
    }
}


/* MAIN */

int
main(int ac, char** av){
    int r;
    int ret;
    pid_t launched;
    ret = 0;
    r = logfile_open();
    if(r){
        fprintf(stderr, "WARNING: Logfile open err %d\n",r);
    }

    args_record(ac, av);
    args_gen(ac, av);

    r = launch(&launched);
    if(r){
        ret = r;
        fprintf(stderr, "Launch: err %d\n",r);
    }else{
        //printf("Launched = %d\n",launched);
        ret = mainloop(launched);
    }

    args_term();
    logfile_close();
    return ret;
}
