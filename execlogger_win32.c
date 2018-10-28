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
 *   TIMF: "Windows" (FILETIME then QPC)
 *   ARGB: Argument list start (null string for process argument)
 *   ARGD: Process arguments
 *   ARGE: Argument list terminator
 *   PRES: Process result(32bits?)
 *   FILN: File name
 *   FILD: File data
 *   PIPO: Buffered pipe data (stdout)
 *   PIPE: Buffered pipe data (stderr)
 */

#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <process.h>
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

/* String I/O */
#define MAX_PARAMLEN 1024
static int /* Win32 error */
to_utf8(const wchar_t* in, char* out, size_t outlen){
    // https://docs.microsoft.com/en-us/windows/desktop/api/stringapiset/nf-stringapiset-widechartomultibyte
    int r = WideCharToMultiByte(CP_UTF8, 0, in,
                                -1, out, outlen, NULL, NULL);
    if(r == 0){
        return GetLastError();
    }else{
        return 0;
    }
}

static int /* Win32 error */
to_utf16(const char* in, wchar_t* out, size_t outlen /* element count */){
    // https://docs.microsoft.com/en-us/windows/desktop/api/stringapiset/nf-stringapiset-multibytetowidechar
    int r = MultiByteToWideChar(CP_UTF8, 0, in, -1, out, outlen);
    if(r == 0){
        return GetLastError();
    }else{
        return 0;
    }
}

static int /* Win32 error */
mbcs_to_utf16(const char* in, wchar_t* out, size_t outlen /* element count */){
    // https://docs.microsoft.com/en-us/windows/desktop/api/stringapiset/nf-stringapiset-multibytetowidechar
    int r = MultiByteToWideChar(CP_OEMCP, 0, in, -1, out, outlen);
    if(r == 0){
        return GetLastError();
    }else{
        return 0;
    }
}

static int /* Win32 error */
envgetstr(const char* name, char* out, size_t outlen){
    int r;
    wchar_t name_w[MAX_PARAMLEN];
    wchar_t buf[MAX_PARAMLEN];
    (void)to_utf16(name, name_w, MAX_PARAMLEN);
    // https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-getenvironmentvariable
    r = GetEnvironmentVariableW(name_w, buf, MAX_PARAMLEN);
    if(r == 0){
        return GetLastError();
    }else{
        return to_utf8(buf, out, outlen);
    }
}

static int /* Win32 error */
envputstr(const char* name, char* in){
    int r;
    wchar_t name_w[MAX_PARAMLEN];
    wchar_t buf[MAX_PARAMLEN];
    (void)to_utf16(name, name_w, MAX_PARAMLEN);
    (void)to_utf16(in, buf, MAX_PARAMLEN);
    // https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-setenvironmentvariable
    r = SetEnvironmentVariableW(L"RS_EXECLOG_IDENT", buf);
    if(r){
        return 0;
    }else{
        return GetLastError();
    }
}

/* LOGFILE */
FILE* theLogfile = NULL;
HANDLE sem_logfile = INVALID_HANDLE_VALUE;
char prev_ident[MAX_PARAMLEN];
char my_ident[MAX_PARAMLEN];

static void
logfile_put_blob(uint32_t tag, char* data, size_t len){
    uint64_t datalen = len;

    WaitForSingleObject(sem_logfile, INFINITE);
    if(tag == TAG_PIPO){
        data[len] = 0;
        printf("%s",data);
    }else if(tag == TAG_PIPE){
        data[len] = 0;
        fprintf(stderr,"%s",data);
    }
    (void)fwrite(&tag, sizeof(uint32_t), 1, theLogfile);
    (void)fwrite(&datalen, sizeof(uint64_t), 1, theLogfile);
    (void)fwrite(data, len, 1, theLogfile);
    ReleaseSemaphore(sem_logfile, 1, NULL);
}

static void
logfile_put_utf16(uint32_t tag, wchar_t* data, size_t count){
    const size_t bufsiz = count * sizeof(wchar_t) * 2;
    void* buf;
    int r;
    buf = malloc(bufsiz);
    r = to_utf8(data, buf, bufsiz);
    if(!r){
        logfile_put_blob(tag, buf, strnlen(buf, bufsiz));
    }
    free(buf);
}

static int /* Win32 error */
logfile_open(void){
    DWORD curdir_len;
    char pth[MAX_PARAMLEN];
    wchar_t curdir[4096];
    char logpath[MAX_PARAMLEN];
    LARGE_INTEGER ts;
    int r;

    curdir_len = GetCurrentDirectoryW(4096, curdir);
    if(!curdir_len){
        fprintf(stderr, "WARNING: Too long current path.");
        return GetLastError();
    }

    r = envgetstr("RS_EXECLOG_IDENT", prev_ident, MAX_PARAMLEN);
    if(r){
        prev_ident[0] = 0;
    }
    r = envgetstr("RS_EXECLOG_LOGDIR", pth, MAX_PARAMLEN);
    if(r){
        return r;
    }
    QueryPerformanceCounter(&ts);
    snprintf(my_ident,MAX_PARAMLEN,"rslog-%016llx-%d",ts.QuadPart,
             GetCurrentProcessId());
    r = envputstr("RS_EXECLOG_IDENT", my_ident);
    if(r){
        return r;
    }

    snprintf(logpath,MAX_PARAMLEN,"%s/%s.bin",pth,my_ident);

    theLogfile = fopen(logpath, "w+b");
    if(!theLogfile){
        return 0xdeadbeef;
    }
    sem_logfile = CreateSemaphoreA(NULL, 1, 1, NULL);

    logfile_put_blob(TAG_IDNP, prev_ident, strlen(prev_ident));
    logfile_put_blob(TAG_IDNT, my_ident, strlen(my_ident));
    logfile_put_utf16(TAG_WRKD, curdir, curdir_len);

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
wchar_t procname[4096];
wchar_t* newargs = NULL;

typedef struct argq_head_s {
    wchar_t* filename;
    struct argq_head_s* next;
} argq_head_t;

static argq_head_t*
argq_push(argq_head_t* cur, const wchar_t* filename /* Copied */){
    argq_head_t* next = malloc(sizeof(argq_head_t));
    next->filename = _wcsdup(filename);
    next->next = cur;
    return next;
}

static void
argq_free_entry(argq_head_t* e){
    free(e->filename);
    free(e);
}

static void
argq_free_chain(argq_head_t* e){
    if(e){
        argq_free_chain(e->next);
        argq_free_entry(e);
    }
}

static argq_head_t*
args_proc_rspfile(argq_head_t* q, FILE* fp /* seek anywhere */){
    char* buf_mbcs;
    char* p;
    wchar_t* pw;
    wchar_t* buf_utf16;
    wchar_t* buf;
    wchar_t** lis;
    int nargs;
    int r,i;
    long inlen;
    int is_utf16 = 0;

    /* Calc file size */
    fseek(fp, 0, SEEK_END);
    inlen = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if(inlen > (1024*1024*100)){
        fprintf(stderr, "WARNING: Ignored too large rspfile.\n");
        return q;
    }

    buf_mbcs = calloc(1, inlen + 2);
    buf_utf16 = calloc(1, inlen * 3 + 1);

    /* Read response file */
    fread(buf_mbcs, inlen, 1, fp);
    buf_mbcs[inlen] = 0;
    buf_mbcs[inlen+1] = 0;
    /* UTF16 check */
    if(inlen >= 4){
        if((buf_mbcs[0] == 0xff) && (buf_mbcs[1] == 0xfe)){
            /* Check for BOM(UTF16LE?) */
            buf_mbcs[0] = 0x20;
            buf_mbcs[1] = 0x20;
            is_utf16 = 1;
        }else if((buf_mbcs[0] == 0) || (buf_mbcs[1] == 0) || (buf_mbcs[2] == 0) || (buf_mbcs[3] == 0)){
            /* Guesstimate */
            is_utf16 = 1;
        }
    }
    if(is_utf16){
        memcpy(buf_utf16, buf_mbcs, inlen);
        /* Filter-out CR+LF */
        for(pw = buf_utf16;*pw;pw++){
            switch(*pw){
                case L'\r':
                case L'\n':
                    *pw = 0x20;
                    break;
                default:
                    break;
            }
        }
    }else{
        /* Filter-out CR+LF */
        for(p = buf_mbcs;*p;p++){
            switch(*p){
                case '\r':
                case '\n':
                    *p = 0x20;
                    break;
                default:
                    break;
            }
        }
        r = mbcs_to_utf16(buf_mbcs, buf_utf16, inlen * 3);

        if(r){
            fprintf(stderr, "WARNING: Ignored invalid ASCII file.\n");
            goto endgame;
        }
    }
    
    /* Skip whitespace */
    for(buf = buf_utf16;*buf;buf++){
        // FIXME: Use libc here
        switch(*buf){
            case L'\r':
            case L'\n':
            case L' ':
                break;
            default:
                goto next;
        }
    }
next:
    lis = CommandLineToArgvW(buf, &nargs);

    if(lis){
        for(i=0;i!=nargs;i++){
            fwprintf(stderr, L"ARG: %s\n", lis[i]);
            logfile_put_utf16(TAG_ARGD, lis[i], wcslen(lis[i]));
            if(lis[i][0] == L'@'){
                q = argq_push(q, &lis[i][1]);
            }
        }
    }

    LocalFree(lis);

endgame:
    free(buf_mbcs);
    free(buf_utf16);
    return q;
}

static void
args_record_queue(argq_head_t* q_init){
    FILE* fp;
    argq_head_t* cur = q_init;
    argq_head_t* queue = NULL;

    while(cur){
        fwprintf(stderr, L"Response file: %s\n", cur->filename);
        fp = _wfopen(cur->filename, L"r");
        if(fp){
            logfile_put_utf16(TAG_ARGB, cur->filename, wcslen(cur->filename));
            queue = args_proc_rspfile(queue, fp);
            logfile_put_blob(TAG_ARGE, NULL, 0);
            fclose(fp);
        }else{
            fwprintf(stderr, L"WARNING: Response file not found.\n");
        }
        cur = cur->next;
    }
    argq_free_chain(q_init);
    if(queue){ // To silence warning
        args_record_queue(queue);
    }
}

static void
args_record(void){
    // https://docs.microsoft.com/en-us/windows/desktop/api/shellapi/nf-shellapi-commandlinetoargvw
    wchar_t** lis;
    int nargs;
    int i;
    argq_head_t* queue = NULL;

    lis = CommandLineToArgvW(GetCommandLineW(), &nargs);

    if(lis){
        logfile_put_blob(TAG_ARGB, NULL, 0);
        for(i=0;i!=nargs;i++){
            logfile_put_utf16(TAG_ARGD, lis[i], wcslen(lis[i]));
            if(lis[i][0] == L'@'){
                queue = argq_push(queue, &lis[i][1]);
            }
        }
        logfile_put_blob(TAG_ARGE, NULL, 0);
    }

    LocalFree(lis);

    args_record_queue(queue);
}

static int /* Win32 error */
args_gen(void){
    FILE* check;
    wchar_t* cmdline;
    wchar_t modname[4096];
    wchar_t myname[4096];
    wchar_t exec_drive[_MAX_DRIVE];
    wchar_t exec_dir[_MAX_DIR*40];
    wchar_t exec_fname[_MAX_FNAME];
    wchar_t exec_ext[_MAX_EXT];
    wchar_t* cur;
    wchar_t* origargs;
    const buflen = MAX_ARGSIZE;
    int r;

    /* Calc replacement executable name */
    r = GetModuleFileNameW(NULL,modname,4096);
    if(r == 0){
        return GetLastError();
    }
    /* 1st try: rs-exectrace/ORIG.exe */
    _wsplitpath(modname, exec_drive, exec_dir, exec_fname, exec_ext);
    wcscat(exec_dir, L"rs-exectrace");
    _wmakepath(myname, exec_drive, exec_dir, exec_fname, exec_ext);
    fwprintf(stderr, L"Trying %s\n",myname);
    check = _wfopen(myname, L"rb");
    if(check){
        fclose(check);
        fwprintf(stderr, L"OK %s\n",myname);
    }else{
        /* 2nd try: ORIG.traced.exe */
        wcscpy(myname, modname);
        wcscat(myname, L".traced.exe");
    }

    newargs = malloc(MAX_ARGSIZE * sizeof(wchar_t));
    wcscpy(newargs, myname);
    wcscpy(procname, myname);

    /* Seek to end */
    for(cur=newargs;*cur;cur++){ }

    /* Seek to original args */
    cmdline = GetCommandLineW();
    // FIXME: We cannot be based on spaced path
    for(origargs=cmdline;*origargs!=0x20;origargs++){ }

    /* Fill-in original args */
    wcscpy(cur, origargs);

    return 0;
}

static void
args_term(void){
    free(newargs);
}

static wchar_t*
args_get(void){
    return newargs;
}

static wchar_t*
args_getprocname(void){
    return procname;
}

/* EXECPIPE */
#define PIPEBUFLEN (1024*1024*16)
typedef struct {
    HANDLE h;
    HANDLE t;
    HANDLE e;
    uint32_t tag;
}readerparams_t;

readerparams_t rp1;
readerparams_t rp2;

static DWORD
thr_readpipe(void* param){
    void* pipebuf;
    DWORD r,e;
    DWORD len;
    readerparams_t rp;
    rp = *(readerparams_t *)param;
    pipebuf = malloc(PIPEBUFLEN);
    for(;;){
        r = ReadFile(rp.h, pipebuf, PIPEBUFLEN,
                     &len, NULL);
        if(!r){
            e = GetLastError();
            CloseHandle(rp.h);
            SetEvent(rp.e);
            if(e == ERROR_BROKEN_PIPE){
                free(pipebuf);
                return 0;
            }else{
                free(pipebuf);
                return 1;
            }
        }else{
            logfile_put_blob(rp.tag, pipebuf, len);
        }
    }
}

static int /* Win32 error */
newpipe(HANDLE out[2]){ /* read, write */
    SECURITY_ATTRIBUTES sat;
    HANDLE tmp;

    sat.nLength = sizeof(SECURITY_ATTRIBUTES);
    sat.bInheritHandle = TRUE;
    sat.lpSecurityDescriptor = NULL;

    if(!CreatePipe(&out[0],&tmp,&sat,0)){
        return GetLastError();
    }
    DuplicateHandle(GetCurrentProcess(),tmp,
                    GetCurrentProcess(),&out[1],0,
                    TRUE,DUPLICATE_SAME_ACCESS);
    CloseHandle(tmp);

    return 0;
}


static int /* Win32 error */
launch(HANDLE* prochandle){
    HANDLE h1[2];
    HANDLE h2[2];

    int r;

    STARTUPINFOW su;
    STARTUPINFOW newproc;
    PROCESS_INFORMATION pi;

    newpipe(h1);
    newpipe(h2);

    // https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-getstartupinfoa
    GetStartupInfoW(&su);

    ZeroMemory(&newproc,sizeof(newproc));
    ZeroMemory(&pi,sizeof(pi));

    newproc.cb = sizeof(STARTUPINFOW);
    newproc.dwFlags = STARTF_USESTDHANDLES;

    newproc.hStdInput = INVALID_HANDLE_VALUE;
    newproc.hStdOutput = h1[1];
    newproc.hStdError = h2[1];

    rp1.e = CreateEvent(NULL, FALSE, FALSE, NULL);
    rp1.h = h1[0];
    rp1.tag = TAG_PIPO;
    rp2.e = CreateEvent(NULL, FALSE, FALSE, NULL);
    rp2.h = h2[0];
    rp2.tag = TAG_PIPE;

    rp1.t = (HANDLE)_beginthread(thr_readpipe, 0, &rp1);
    rp2.t = (HANDLE)_beginthread(thr_readpipe, 0, &rp2);

    // https://docs.microsoft.com/en-us/windows/desktop/api/processthreadsapi/nf-processthreadsapi-createprocessw
#if 0
    printf("Launch: [");
    wprintf(L"%s", args_get());
    printf("]\n");
#endif
#if 0
    r = CreateProcessW(NULL, args_get(), NULL, NULL, 
                       TRUE /* Inherit handles */,
                       0, NULL, NULL, 
                       &newproc, &pi);
#endif
    r = CreateProcessW(args_getprocname(), GetCommandLineW(), NULL, NULL, 
                       TRUE /* Inherit handles */,
                       0, NULL, NULL, 
                       &newproc, &pi);
    CloseHandle(h1[1]);
    CloseHandle(h2[1]);

    if(r == 0){
        return GetLastError();
    }

    CloseHandle(pi.hThread);
    *prochandle = pi.hProcess;

    return 0;
}

static int /* Return value */
mainloop(HANDLE proc){
    BOOL b;
    DWORD code;
    HANDLE waits[3];
    int r;

    code = -1;
    for(;;){ // FIXME: Do not have to loop here.
        waits[0] = proc;
        waits[1] = rp1.e;
        waits[2] = rp2.e;
        r = WaitForMultipleObjects(3, waits, TRUE, INFINITE);
        b = GetExitCodeProcess(proc, &code);
        if(b){
            if(code != STILL_ACTIVE){
                fprintf(stderr, "TRACE Exit: %d\n",code);
            }
        }
        return code;
    }
}


/* MAIN */

int
main(int ac, char** av){
    int r;
    int ret;
    HANDLE launched;
    ret = 0;
    r = logfile_open();
    if(r){
        fprintf(stderr, "Logfile open: Win32 err %d\n",r);
        return r;
    }

    args_record();
    args_gen();

    r = launch(&launched);
    if(r){
        ret = r;
        fprintf(stderr, "Launch: Win32 err %d\n",r);
    }else{
        ret = mainloop(launched);
    }

    args_term();
    logfile_close();
    return ret;
}
