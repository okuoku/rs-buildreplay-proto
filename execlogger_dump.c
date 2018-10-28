#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define FOURCC(a,b,c,d) \
    (uint32_t)(((uint32_t)a) + \
               ((uint32_t)b<<8) + \
               ((uint32_t)c<<16) + \
               ((uint32_t)d<<24))

#define TAG_PIPO FOURCC('P','I','P','O')
#define TAG_PIPE FOURCC('P','I','P','E')

static void
usage(void){
    fprintf(stderr, "usage: execlog_dump <IN.bin> <OUT.txt>\n");
}

static void
filtercrlf(char* buf, size_t len){
    size_t i;
    for(i=0;i!=len;i++){
        switch(buf[i]){
            case '\r':
            case '\n':
                fprintf(stderr, "WARNING: Escaped CRLF\n");
                buf[i] = 0x20;
                break;
            default:
                break;
        }
    }
}


int
main(int ac, char** av){
    FILE* in;
    uint64_t inlen;
    uint64_t offs;
    FILE* out;
    uint32_t tag;
    uint64_t len;
    void* buf;
    if(ac != 3){
        usage();
        return 1;
    }

    in = fopen(av[1], "rb");
    if(!in){
        fprintf(stderr, "Open err: %s\n", av[1]);
        return 2;
    }

    out = fopen(av[2], "w+b");
    if(!out){
        fprintf(stderr, "Open err: %s\n", av[2]);
        return 2;
    }


    /* Calc file size */
    fseek(in, 0, SEEK_END);
    inlen = ftell(in);
    fseek(in, 0, SEEK_SET);

    fprintf(stderr, "IN: %s size = %lld\n", av[1], inlen);

    /* Dump loop */
    offs = 0;
    while(offs < inlen){
        (void)fread(&tag, sizeof(uint32_t), 1, in);
        (void)fread(&len, sizeof(uint64_t), 1, in);
        offs += sizeof(uint32_t);
        offs += sizeof(uint64_t);
        if(len > (1024*1024*1024)){
            fprintf(stderr, "Too long TLV region %lld\n", offs);
            fclose(out);
            return 2;
        }
        switch(tag){
            case TAG_PIPO:
            case TAG_PIPE:
                /* Skip */
                offs += len;
                fseek(in, (long)offs, SEEK_SET);
                break;
            default:
                buf = malloc((size_t)len);
                fread(buf, (size_t)len, 1, in);
                filtercrlf(buf, (size_t)len);
                fwrite(&tag, sizeof(uint32_t), 1, out);
                fprintf(out, ":");
                fwrite(buf, (size_t)len, 1, out);
                fprintf(out, "\n");
                free(buf);
                offs += len;
                break;
        }

    }
    fclose(out);

    fprintf(stderr, "OUT: %s size = %lld\n", av[2], offs);

    return 0;
}
