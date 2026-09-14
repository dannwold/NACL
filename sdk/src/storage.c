#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <fcntl.h>

#include <sys/mman.h>

#include <sys/stat.h>

#include "storage.h"



int storage_init(void) {

    printf("[libstorage] Storage tracking framework activated.\n");

    return 0;

}



int storage_mmap_file(const char *file_path, size_t file_size, MappedFile *out_map) {

    if (!file_path || !out_map) return -1;



    int fd = open(file_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    if (fd < 0) {

        perror("[libstorage] mmap file open failed");

        return -1;

    }



    struct stat st;

    if (fstat(fd, &st) == 0 && st.st_size < (off_t)file_size) {

        if (ftruncate(fd, file_size) == -1) {

            perror("[libstorage] ftruncate extension failed");

            close(fd);

            return -1;

        }

    }



    void *mapped = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (mapped == MAP_FAILED) {

        perror("[libstorage] mmap call failed");

        close(fd);

        return -1;

    }



    out_map->mapped_ptr = mapped;

    out_map->length = file_size;

    out_map->fd = fd;



    printf("[libstorage] Successfully mapped '%s' (Length: %zu bytes) to RAM at %p\n",

           file_path, file_size, mapped);

    return 0;

}



void storage_munmap_file(MappedFile *map) {

    if (!map || !map->mapped_ptr) return;



    munmap(map->mapped_ptr, map->length);

    close(map->fd);

    memset(map, 0, sizeof(MappedFile));

}



int storage_get_encryption_type(const char *path, char *out_type, size_t max_len) {

    if (!path || !out_type || max_len < 3) return -1;



    if (strstr(path, "/data/user_de/") != NULL) {

        strncpy(out_type, "DE", max_len);

    } else if (strstr(path, "/data/user/") != NULL || strstr(path, "/data/data/") != NULL) {

        strncpy(out_type, "CE", max_len);

    } else {

        strncpy(out_type, "UNKNOWN", max_len);

    }

    return 0;

}