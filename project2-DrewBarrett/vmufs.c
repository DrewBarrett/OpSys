#define FUSE_USE_VERSION 26
#define _GNU_SOURCE
#include <fuse.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>


#define BLOCKSIZE 512

static FILE* fsimage;

static unsigned int DIR_END_BLOCK, DIR_SIZE, FAT_START, FAT_SIZE, USER_BLOCKS;
static time_t TIMESTAMP;

static unsigned int frombcd(unsigned char in) {
    char* string;
    asprintf(&string, "%x", in);
    unsigned int ret = atoi(string);
    free(string);
    return ret;
}

uint8_t tobcd(int in) {
    char* string;
    asprintf(&string, "0x%d", in);
    uint8_t ret = strtol(string, NULL, 16);
    free(string);
    return ret;
}

int getTimestamp(uint8_t * timestamp) {
    time_t thet = time(NULL);
    struct tm *t = localtime(&thet);
    timestamp[0] = tobcd(20);
    timestamp[1] = tobcd(t->tm_year - 100);
    timestamp[2] = tobcd(t->tm_mon + 1);
    timestamp[3] = tobcd(t->tm_mday);
    timestamp[4] = tobcd(t->tm_hour);
    timestamp[5] = tobcd(t->tm_min);
    timestamp[6] = tobcd(t->tm_sec);
    timestamp[7] = tobcd((t->tm_wday + 6) % 7);
    return 0;
}

static int getFreeBlock() {
    fseek(fsimage, FAT_START * BLOCKSIZE, SEEK_SET);
    for (int i = 0; i < (FAT_SIZE * BLOCKSIZE) / 2; ++i) {
        uint16_t res;
        fread(&res, 2, 1, fsimage);
        if (res == 0xfffc) {
            return i;
        }
    }
    return -ENOSPC;
}

static int set_timestamp(unsigned char * finfo) {
    uint8_t timestamp[8];
    getTimestamp(timestamp);
    for (int i = 0; i < 8; ++i) {
        finfo[0x10 + i] = timestamp[i];
    }
    return 0;
}

static int set_file_size(unsigned char * finfo, unsigned int bytes) {
    size_t len = bytes;
    printf("Setting file size to %ld\n", len);
    finfo[0x19] = len >> 2;
    finfo[0x18] = len & 0xFF;
    return 0;
}

static size_t file_size(unsigned char * finfo) {
    size_t len;
    unsigned int fsizeblock = (finfo[0x19] << 2) | (finfo[0x18] & 0xFF);
    len = fsizeblock * BLOCKSIZE;
    printf("File size in blocks: %d, %x, %x\n", fsizeblock, finfo[0x18], finfo[0x19]);
    return len;
}

static unsigned int file_location(unsigned char * finfo) {
    unsigned int loc;
    loc = (finfo[0x3] << 2) | (finfo[0x2] & 0xFF);
    printf("File location in blocks: %d %x %x\n", loc, finfo[0x3], finfo[0x2]);
    return loc;
}
static unsigned char * file_info(const char * path) {
    fseek(fsimage, ((DIR_END_BLOCK * BLOCKSIZE) - (BLOCKSIZE * (DIR_SIZE - 1))), SEEK_SET);
    const int dircount = (DIR_SIZE * BLOCKSIZE) / 32;

    char * fname = path + 1;
    for (int i = 0; i < dircount; ++i) {
        unsigned char * direntry = (unsigned char *) malloc(sizeof(char) * 32);
        char name[13];
        fread(direntry, 32, 1, fsimage);
        if (direntry[0] != 0) {
            for (int i = 0; i < 12; ++i) {
                name[i] = direntry[i + 4];
                name[i + 1] = '\0';
            }
            printf("%s and %s\n", name, path);
            int res;
            if ((res = strcmp(name, fname)) == 0) {
                printf("We have a match!\n");
                return direntry;
            }
            printf("Difference: %d\n", res);
        }
    }
    return NULL;
}

static int set_file_info(const char * path, unsigned char * finfo) {
    fseek(fsimage, ((DIR_END_BLOCK * BLOCKSIZE) - (BLOCKSIZE * (DIR_SIZE - 1))), SEEK_SET);
    const int dircount = (DIR_SIZE * BLOCKSIZE) / 32;
    char * fname = path + 1;
    for (int i = 0; i < dircount; ++i) {
        unsigned char * direntry = (unsigned char *) malloc(sizeof(char) * 32);
        char name[13];
        fread(direntry, 32, 1, fsimage);
        if (direntry[0] != 0) {
            for (int i = 0; i < 12; ++i) {
                name[i] = direntry[i + 4];
                name[i + 1] = '\0';
            }
            printf("%s and %s\n", name, path);
            int res;
            if ((res = strcmp(name, fname)) == 0) {
                printf("We have a match!\n");
                fseek(fsimage, -32, SEEK_CUR);
                fwrite(finfo, 32, 1, fsimage);
            }
            printf("Difference: %d\n", res);
        }
    }
    return NULL;

} 

static int file_exists(const char * path) {
    unsigned char * res;
    if ((res = file_info(path)) != NULL) {
        free(res);
        return 1;
    }
    return 0;
}

static int vmufs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_size = DIR_SIZE * BLOCKSIZE;
        stbuf->st_mtime = TIMESTAMP;
    } else {
        // look for the file
        unsigned char * finfo = file_info(path);
        if ((finfo = file_info(path)) != NULL) {
            stbuf->st_mode = S_IFREG | 0644;
            stbuf->st_size = file_size(finfo);
            unsigned int cent = (frombcd(finfo[0 + 0x10]) * 100) - 1900;
            struct tm tm = {
                .tm_year = frombcd(finfo[1 + 0x10]) + cent,
                .tm_mon = frombcd(finfo[2 + 0x10]) - 1,
                .tm_mday = frombcd(finfo[3 + 0x10]),
                .tm_hour = frombcd(finfo[4 + 0x10]),
                .tm_min = frombcd(finfo[5 + 0x10]),
                .tm_sec = frombcd(finfo[6 + 0x10]),
            };
            stbuf->st_mtime = mktime(&tm);
        }
        else 
            return -ENOENT;
    }
    return 0;
}

static int vmufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/") != 0)
        return -ENOENT;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    fseek(fsimage, ((DIR_END_BLOCK * BLOCKSIZE) - (BLOCKSIZE * (DIR_SIZE - 1))), SEEK_SET);
    const int dircount = (DIR_SIZE * BLOCKSIZE) / 32;
    printf("Dircount: %d\n", dircount);
    for (int i = 0; i < dircount; ++i) {
        unsigned char direntry[32];
        char name[13];
        fread(&direntry, 32, 1, fsimage);
        if (direntry[0] != 0) {
            for (int i = 0; i < 12; ++i) {
                name[i] = direntry[i + 4];
                name[i+1] = '\0';
            }
            struct stat stat;
            stat.st_mode = S_IFREG | 0644;
            filler(buf, name, &stat, 0);
        }
    }
    return 0;
}

static int vmufs_open(const char *path, struct fuse_file_info *fi) {
    if (file_exists(path))
        return 0;
    return -ENOENT;
}

static int vmufs_write(const char *path, const char *buf, size_t size,
        off_t offset, struct fuse_file_info *fi) {
    unsigned char * finfo = file_info(path);
    char * mybuff = buf;
    if (finfo == NULL) {
        return -ENOENT;
    }
    unsigned int floc = file_location(finfo);
    // update timestamp
    set_timestamp(finfo);
    int blocks = 1;
    size_t remaining = size;
    size_t todo;
    do {
        todo = remaining;
        while (offset > BLOCKSIZE) {
            uint16_t newloc;
            fseek(fsimage, (FAT_START * BLOCKSIZE) + (floc * 2), SEEK_SET);
            fread(&newloc, 2, 1, fsimage);
            // If it's end of chain, allocate a new one baby!
            if (newloc == 0xFFFA) {
                int res = getFreeBlock();
                if (res < 0) {
                    set_file_size(finfo, blocks);
                    set_file_info(path, finfo);
                    free(finfo);
                    return size - remaining;
                }
                fseek(fsimage, (FAT_START * BLOCKSIZE) + (floc * 2), SEEK_SET);
                fwrite(&res, 2, 1, fsimage);
                newloc = res;
                // mark block as used
                fseek(fsimage, (FAT_START * BLOCKSIZE) + (newloc * 2), SEEK_SET);
                uint16_t tw = 0xFFFA;
                fwrite(&tw, 2, 1, fsimage);
            }
            floc = newloc;
            printf("Next location in chain: %x\n", floc);
            blocks++;
            offset -= BLOCKSIZE;
        }
        if (offset + remaining > BLOCKSIZE) {
            remaining = todo;
            todo = BLOCKSIZE - offset;
        }
        remaining -= todo;
        fseek(fsimage, (floc * BLOCKSIZE) + offset, SEEK_SET);
        fwrite(mybuff, 1, todo, fsimage);
        mybuff += todo;
        offset = 0;
        if (remaining > 0) {
            // update floc with new block
            uint16_t newloc;
            fseek(fsimage, (FAT_START * BLOCKSIZE) + (floc * 2), SEEK_SET);
            fread(&newloc, 2, 1, fsimage);
            // If it's end of chain, allocate a new one baby!
            if (newloc == 0xFFFA) {
                int res = getFreeBlock();
                if (res < 0) {
                    set_file_size(finfo, blocks);
                    set_file_info(path, finfo);
                    free(finfo);
                    return size - remaining;
                }
                newloc = res;
                fseek(fsimage, (FAT_START * BLOCKSIZE) + (floc * 2), SEEK_SET);
                fwrite(&res, 2, 1, fsimage);
                // mark block as used
                fseek(fsimage, (FAT_START * BLOCKSIZE) + (newloc * 2), SEEK_SET);
                uint16_t tw = 0xFFFA;
                fwrite(&tw, 2, 1, fsimage);
            }
            floc = newloc;
            blocks++;
            printf("Next location in chain: %x\n", floc);
        }
    } while (remaining > 0);
    set_file_size(finfo, blocks);
    set_file_info(path, finfo);
    free(finfo);
    return size;
}

static int vmufs_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
    unsigned char * finfo = file_info(path);
    char * mybuff = buf;
    if (finfo == NULL) {
        return -ENOENT;
    }
    size_t len = file_size(finfo);
    unsigned int floc = file_location(finfo);
    free(finfo);
    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        size_t remaining = size;
        size_t todo;
        do {
            todo = remaining;
            while (offset > BLOCKSIZE) {
                uint16_t newloc;
                fseek(fsimage, (FAT_START * BLOCKSIZE) + (floc * 2), SEEK_SET);
                fread(&newloc, 2, 1, fsimage);
                floc = newloc;
                printf("Next location in chain: %x\n", floc);
                offset -= BLOCKSIZE;
            }
            if (offset + remaining > BLOCKSIZE) {
                // We will have to find the next block, yuck
                remaining = todo;
                todo = BLOCKSIZE - offset;
            }
            remaining -= todo;
            fseek(fsimage, (floc * BLOCKSIZE) + offset, SEEK_SET);
            fread(mybuff, 1, todo, fsimage);
            mybuff += todo;
            offset = 0;
            if (remaining > 0) {
                // update floc with new block
                uint16_t newloc;
                fseek(fsimage, (FAT_START * BLOCKSIZE) + (floc * 2), SEEK_SET);
                fread(&newloc, 2, 1, fsimage);
                floc = newloc;
                printf("Next location in chain: %x\n", floc);
            }
        } while(remaining > 0);
    } else {
        size = 0;
    }
    return size;
}


static int vmufs_create(const char * path, mode_t mode, 
        struct fuse_file_info *finfo) {
    // First thing we do is look for an open block, we will mark it used
    //  later
    int res = getFreeBlock();
    if (res < 0) {
        return res;
    }
    uint16_t loc = res;
    fseek(fsimage, ((DIR_END_BLOCK * BLOCKSIZE) - (BLOCKSIZE * (DIR_SIZE - 1))), SEEK_SET);
    const int dircount = (DIR_SIZE * BLOCKSIZE) / 32;
    for (int i = 0; i < dircount; ++i) {
        unsigned char direntry[32];
        fread(&direntry, 32, 1, fsimage);
        if (direntry[0] == 0) {
            // we have an empty directory spot
            unsigned char newdirentry[32];
            for (int i = 0; i < 32; i++) {
                newdirentry[i] = 0;
            }
            newdirentry[0] = 0x33;
            // Write floc
            newdirentry[2] = loc & 0xFF;
            newdirentry[3] = loc >> 2;
            char * fname = path + 1;
            for (int i = 0; i < 12; i++) {
                newdirentry[i+4] = fname[i];
                if (fname[i] == '\0')
                    break;
            }
            uint8_t timestamp[8];
            getTimestamp(timestamp);
            for (int i = 0; i < 8; i++) {
                newdirentry[i+16] = timestamp[i];
            }
            newdirentry[0x18] = 1;
            fseek(fsimage, -32, SEEK_CUR);
            fwrite(&newdirentry, 32, 1, fsimage);
            
            // mark block as used
            fseek(fsimage, (FAT_START * BLOCKSIZE) + (loc * 2), SEEK_SET);
            uint16_t tw = 0xFFFA;
            fwrite(&tw, 2, 1, fsimage);
            return 0;
        }
    }
    return -ENOSPC;
}

static int vmufs_unlink(const char * path) {
    unsigned char * finfo = file_info(path);
    if (finfo == NULL) {
        return -ENOENT;
    }
    unsigned int floc = file_location(finfo);
    uint16_t clear = 0xFFFC;
    do {
        uint16_t newloc;
        fseek(fsimage, (FAT_START * BLOCKSIZE) + (floc * 2), SEEK_SET);
        fread(&newloc, 2, 1, fsimage);
        fseek(fsimage, (FAT_START * BLOCKSIZE) + (floc * 2), SEEK_SET);
        fwrite(&clear, 2, 1, fsimage);
        floc = newloc;
    } while (floc != 0xFFFA);
    fseek(fsimage, -2, SEEK_CUR);
    fwrite(&clear, 2, 1, fsimage);
    for (int i = 0; i < 32; ++i) {
        finfo[i] = 0;
    }
    set_file_info(path, finfo);
    free(finfo);
    return 0;
}

static int vmufs_rename(const char * from, const char * to) {
    unsigned char * finfo = file_info(from);
    for (int i = 0; i < 12; i++) {
        finfo[i+4] = to[i + 1];
        if (to[i + 1] == '\0')
            break;
    }
    uint8_t timestamp[8];
    getTimestamp(timestamp);
    for (int i = 0; i < 8; i++) {
        finfo[i+16] = timestamp[i];
    }
    set_file_info(from, finfo);
    free(finfo);
    return 0;
}

static int vmufs_utimens(const char * path, const struct timespect * tv) {
    unsigned char * finfo = file_info(path);
    uint8_t timestamp[8];
    getTimestamp(timestamp);
    for (int i = 0; i < 8; i++) {
        finfo[i+16] = timestamp[i];
    }
    set_file_info(path, finfo);
    free(finfo);
    return 0;
}

static int vmufs_truncate(const char * path, off_t size) {
    vmufs_unlink(path);
    vmufs_create(path, 0644, NULL);
    return 0;
}

static struct fuse_operations vmufs_oper = {
    .getattr    = vmufs_getattr,
    .readdir    = vmufs_readdir,
    .open       = vmufs_open,
    .read       = vmufs_read,
    .write      = vmufs_write,
    .create     = vmufs_create,
    .unlink     = vmufs_unlink,
    .rename     = vmufs_rename,
    .utimens    = vmufs_utimens,
    .truncate   = vmufs_truncate,
};

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s image.img /mountpoint\n", argv[0]);
        return -1;
    }
    // open fs image
    if ((fsimage = fopen(argv[1], "r+")) == NULL) {
        printf("ERROR: Unable to open %s for reading\n", argv[1]);
        return -errno;
    }

    // parse root block
    fseek(fsimage, BLOCKSIZE * -1, SEEK_END);
    for (int i = 0; i < 16; ++i) {
        unsigned int f;
        fread(&f, 1, 1, fsimage);
        if ((f & 0xff) != 0x55) {
            printf("This is not a valid vmufs image!\n");
            return -22;
        }
    }

    // Get directory info
    fseek(fsimage, 32, SEEK_CUR);
    unsigned char timestamp[8];
    fread(timestamp, 8, 1, fsimage);
    unsigned int cent = (frombcd(timestamp[0]) * 100) - 1900;
    printf("timestamp1 %x %d\n", timestamp[1], frombcd(timestamp[1]));
    struct tm tm = {
        .tm_year = frombcd(timestamp[1]) + cent,
        .tm_mon = frombcd(timestamp[2]) - 1,
        .tm_mday = frombcd(timestamp[3]),
        .tm_hour = frombcd(timestamp[4]),
        .tm_min = frombcd(timestamp[5]),
        .tm_sec = frombcd(timestamp[6]),
    };
    TIMESTAMP = mktime(&tm);
    fseek(fsimage, 14, SEEK_CUR);
    fread(&FAT_START, 2, 1, fsimage);
    fread(&FAT_SIZE, 2, 1, fsimage);
    fread(&DIR_END_BLOCK, 2, 1, fsimage);
    fread(&DIR_SIZE, 2, 1, fsimage);
    fseek(fsimage, 2, SEEK_CUR);
    fread(&USER_BLOCKS, 2, 1, fsimage);

    // copy args for fuse
    char *fuseargs[argc-1];
    fuseargs[0] = argv[0];
    for (int i = 2; i <= argc; ++i) {
        fuseargs[i-1] = argv[i];
    }

    if (FAT_SIZE < 1 || DIR_END_BLOCK - (DIR_SIZE - 1) > 254 ||
            FAT_START > 254 || DIR_SIZE < 1){
        printf("This is not a vlid vmufs image!\n");
        return -22;
    }

    return fuse_main(argc - 1, fuseargs, &vmufs_oper, NULL);
}
