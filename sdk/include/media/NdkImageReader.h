#ifndef NDK_IMAGE_READER_H
#define NDK_IMAGE_READER_H

#include <stdint.h>
#include <media/NdkMediaFormat.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AImageReader AImageReader;
typedef struct AImage AImage;

typedef struct AImageReader_ImageListener {
    void* context;
    void (*onImageAvailable)(void* context, AImageReader* reader);
} AImageReader_ImageListener;

#ifdef __cplusplus
}
#endif

#endif // NDK_IMAGE_READER_H
