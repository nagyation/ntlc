#include "stdio.h"
#include "stdlib.h"
#include "errno.h"
#include "string.h"

/* #define DEBUG */

#ifdef DEBUG
#define printf_dbg(str, ...) printf(str, ##__VA_ARGS__)
#else
#define printf_dbg(str, ...)
#endif

static uint16_t files_cnt = 1;
static uint64_t max_file_size = 1024 * 1024;  /* 1M set as default */
static FILE **file_list = NULL;
static char **file_name_list = NULL;
static char *file_name =NULL;
static void print_help(void)
{
    printf("log output to multiple files and rotate with max size \n"    \
           "-f {file name} file name to save log to\n"                   \
           "-n {file count} file count to save to\n"                     \
           "-s {size per file} file size, defaults to 1M: use B,K,M,G\n" \
        );
}

static void parse_args(int argc, char *argv[])
{
    int i;
    if (argc < 3) {
        printf("wrong arguments\n");
        print_help();
        exit(-1);
    }
    
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-f")) {
            printf_dbg("filename arg\n");
            if (i + 1 >= argc) {
                printf("wrong arguments\n");
                print_help();
                exit(-1);
            }
            printf_dbg("file: %s\n", argv[i+1]);
            file_name = strdup(argv[++i]);
            if (!file_name) {
                perror("couldn't duplicate string\n");
                exit(-1);
            }
        } else if (!strcmp(argv[i], "-n")) {
            int j;
            printf_dbg("file count\n");
            if (i + 1 >= argc) {
                printf("wrong arguments\n");
                print_help();
                exit(-1);
            }
            files_cnt = atoi(argv[++i]);
            if (files_cnt == 0) {
                printf("files count can't be zero\n");
                exit(-1);
            }
        } else if (!strcmp(argv[i], "-s")) {
            char *e;
            printf_dbg("file count\n");
            if (i + 1 >= argc) {
                printf("wrong arguments\n");
                print_help();
                exit(-1);
            }
            unsigned long long fs = strtoll(argv[++i], &e, 10);
            if (fs == 0) {
                printf("file size can't be zero\n");
                exit(-1);
            }

            switch (*e) {
            case 'G':
                fs *= 1024;
            case 'M':
                fs *= 1024;
            case 'K':
                fs *= 1024;
            case 'B':
            default:
                max_file_size = fs;
                break;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int nchar;
    int i;
    FILE *curr_file = NULL;
    uint64_t curr_file_size = 0;
    int curr_file_index = 0;

    parse_args(argc, argv);

    file_name_list = calloc(files_cnt, sizeof(FILE *));
    if (!file_name_list) {
        ret = -1;
        perror("[ntlc]: couldn't allocate file names\n");
        goto err;
    }

    for (i = 0; i < files_cnt; i++) {
        file_name_list[i] = calloc(1, strlen(file_name) + 10);
        if (!file_name_list[i]) {
            ret = -1;
            perror("[ntlc]: couldn't allocate file name");
            goto err;
        }
        sprintf(file_name_list[i], "%s%d", file_name, i);
    }

    file_list = calloc(files_cnt, sizeof(FILE *));
    if (!file_list) {
        ret = -1;
        perror("[ntlc]: couldn't allocate files\n");
        goto err;
    }

    for (i = 0; i < files_cnt; i ++) {
        file_list[i] = fopen(file_name_list[i], "w");
        if (!file_list[i]) {
            ret = -1;
            fprintf(stderr, "[ntlc]: couldn't open file: %s with err %s\n", file_name_list[i], strerror(errno));
            goto err;
        }
    }

    curr_file = file_list[0];

    while (1) {
        nchar = getchar();
        if (feof(stdin) != 0) {
            goto err;
        }

        if (ferror(stdin) != 0) {
            ret = -1;
            perror("read char failed");
            goto err;
        }
        /* printf_dbg("%c", nchar); */
        ret = putc(nchar, curr_file);
        if (ret == nchar) {
            curr_file_size++;
        } else {
            fprintf(stderr, "[ntlc]: couldn't write to file: %s, %s\n", file_name_list[curr_file_index], strerror(errno));
            goto err;
        }
        if (curr_file_size % (1024 * 1024) == 0) {
            printf_dbg("current size %llu ?? %llu\n", curr_file_size, max_file_size);
        }
        if (curr_file_size == max_file_size) {
            printf_dbg("reached max\n");
            curr_file_index = (curr_file_index + 1) % files_cnt;
            fflush(curr_file); /* flush the old buffer before going to the next */
            curr_file = file_list[curr_file_index];
            rewind(curr_file);
            curr_file_size = 0;
            printf_dbg("current file %s\n", file_name_list[curr_file_index]);
        }
    }

err:
    if (file_list) {
        fflush(NULL);
        free(file_list);
    }
    return ret;

}
