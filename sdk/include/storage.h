#ifndef LIBSTORAGE_H

#define LIBSTORAGE_H



#include <stdint.h>

#include <stddef.h>



#ifdef __cplusplus

extern "C" {

#endif



typedef struct {

    void *mapped_ptr;

    size_t length;

    int fd;

} MappedFile;



int storage_init(void);

int storage_mmap_file(const char *file_path, size_t file_size, MappedFile *out_map);

void storage_munmap_file(MappedFile *map);

int storage_get_encryption_type(const char *path, char *out_type, size_t max_len);



#ifdef __cplusplus

}

#endif



#endif // LIBSTORAGE_H