#include <EGL/egl.h>

#include <GLES3/gl3.h>

#include <android/native_window.h>

#include <android/native_window_jni.h>

#include <stdlib.h>

#include <string.h>

#include <pthread.h>

#include <android/log.h>

#include "nacl_display.h"



#define LOG_TAG "libdisplay"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)



struct OpaqueNaclDisplayContext {

    ANativeWindow*  window;

    EGLDisplay      egl_display;

    EGLSurface      egl_surface;

    EGLContext      egl_context;

    GLuint          shader_program;

    GLuint          vbo;

    GLuint          vao;

    NaclDisplayConfig config;

    pthread_mutex_t mutex;



    // Waveform envelope data store

    float*          waveform_buffer;

    size_t          waveform_count;

};



// Simple flat color vertex/fragment shaders for high-frequency vector drawing

static const char* VERTEX_SHADER_SRC =

    "#version 300 es\n"

    "layout(location = 0) in vec2 inPosition;\n"

    "void main() {\n"

    "    gl_Position = vec4(inPosition.x, inPosition.y, 0.0, 1.0);\n"

    "}\n";



static const char* FRAGMENT_SHADER_SRC =

    "#version 300 es\n"

    "precision mediump float;\n"

    "out vec4 fragColor;\n"

    "uniform vec4 color;\n"

    "void main() {\n"

    "    fragColor = color;\n"

    "}\n";



static GLuint compile_shader(GLenum type, const char* src) {

    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &src, nullptr);

    glCompileShader(shader);

    GLint compiled;

    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {

        glDeleteShader(shader);

        return 0;

    }

    return shader;

}



NaclDisplayContext nacl_display_create(void* window_handle, const NaclDisplayConfig* config) {

    if (!window_handle || !config) return nullptr;



    ANativeWindow* window = (ANativeWindow*)window_handle;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (display == EGL_NO_DISPLAY) return nullptr;



    eglInitialize(display, nullptr, nullptr);



    const EGLint attribs[] = {

        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,

        EGL_BLUE_SIZE, 8,

        EGL_GREEN_SIZE, 8,

        EGL_RED_SIZE, 8,

        EGL_ALPHA_SIZE, 8,

        EGL_DEPTH_SIZE, 0,

        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,

        EGL_NONE

    };



    EGLConfig egl_config;

    EGLint num_configs;

    if (!eglChooseConfig(display, attribs, &egl_config, 1, &num_configs) || num_configs < 1) {

        eglTerminate(display);

        return nullptr;

    }



    // Set format dynamically on the Android window to align with hardware buffers

    EGLint format;

    eglGetConfigAttrib(display, egl_config, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(window, 0, 0, format);



    EGLSurface surface = eglCreateWindowSurface(display, egl_config, window, nullptr);

    if (surface == EGL_NO_SURFACE) {

        eglTerminate(display);

        return nullptr;

    }



    const EGLint context_attribs[] = {

        EGL_CONTEXT_CLIENT_VERSION, 3,

        EGL_NONE

    };

    EGLContext context = eglCreateContext(display, egl_config, EGL_NO_CONTEXT, context_attribs);

    if (context == EGL_NO_CONTEXT) {

        eglDestroySurface(display, surface);

        eglTerminate(display);

        return nullptr;

    }



    if (!eglMakeCurrent(display, surface, surface, context)) {

        eglDestroyContext(display, context);

        eglDestroySurface(display, surface);

        eglTerminate(display);

        return nullptr;

    }



    // Compile vector drawing shaders

    GLuint vs = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);

    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);

    GLuint program = glCreateProgram();

    glAttachShader(program, vs);

    glAttachShader(program, fs);

    glLinkProgram(program);

    glDeleteShader(vs);

    glDeleteShader(fs);



    OpaqueNaclDisplayContext* ctx = (OpaqueNaclDisplayContext*)calloc(1, sizeof(OpaqueNaclDisplayContext));

    ctx->window = window;

    ctx->egl_display = display;

    ctx->egl_surface = surface;

    ctx->egl_context = context;

    ctx->shader_program = program;

    ctx->config = *config;

    pthread_mutex_init(&ctx->mutex, nullptr);



    // Initialize vector VBO structures for high-frequency dynamic streaming

    glGenVertexArrays(1, &ctx->vao);

    glGenBuffers(1, &ctx->vbo);



    return ctx;

}



int nacl_display_update_waveform_data(NaclDisplayContext context, const float* data, size_t count) {

    if (!context || !data || count == 0) return -1;



    pthread_mutex_lock(&context->mutex);

    context->waveform_buffer = (float*)realloc(context->waveform_buffer, count * sizeof(float));

    memcpy(context->waveform_buffer, data, count * sizeof(float));

    context->waveform_count = count;

    pthread_mutex_unlock(&context->mutex);



    return 0;

}



void nacl_display_render_frame(NaclDisplayContext context) {

    if (!context) return;



    pthread_mutex_lock(&context->mutex);

    if (context->waveform_count == 0 || !context->waveform_buffer) {

        pthread_mutex_unlock(&context->mutex);

        return;

    }



    // Build the vertices dynamically to map waveform envelopes to screen space

    size_t vertex_count = context->waveform_count * 2;

    float* vertices = (float*)malloc(vertex_count * 2 * sizeof(float)); // 2D vectors: (x, y)



    float x_step = 2.0f / (float)(context->waveform_count - 1);

    for (size_t i = 0; i < context->waveform_count; ++i) {

        float x = -1.0f + (float)i * x_step;

        float half_height = context->waveform_buffer[i] * 0.8f; // Scale slightly for safety margins



        // Top line vertex

        vertices[i * 4 + 0] = x;

        vertices[i * 4 + 1] = half_height;



        // Bottom line vertex (creates vertical symmetric bars)

        vertices[i * 4 + 2] = x;

        vertices[i * 4 + 3] = -half_height;

    }

    pthread_mutex_unlock(&context->mutex);



    // Clear background to user preferred canvas color

    glClearColor(context->config.clear_color[0],

                 context->config.clear_color[1],

                 context->config.clear_color[2],

                 context->config.clear_color[3]);

    glClear(GL_COLOR_BUFFER_BIT);



    glUseProgram(context->shader_program);



    // Bind dynamic waveform buffer and stream to hardware

    glBindVertexArray(context->vao);

    glBindBuffer(GL_ARRAY_BUFFER, context->vbo);

    glBufferData(GL_ARRAY_BUFFER, vertex_count * 2 * sizeof(float), vertices, GL_DYNAMIC_DRAW);



    glEnableVertexAttribArray(0);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);



    // Set overlay lines color dynamically (Green-reactive waveform)

    GLint color_loc = glGetUniformLocation(context->shader_program, "color");

    glUniform4f(color_loc, 0.0f, 1.0f, 0.0f, 1.0f); // Bright neon green overlay



    // Render as individual vertical line segments

    glDrawArrays(GL_LINES, 0, (GLsizei)vertex_count);



    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);



    // Swap buffer to commit the overlay directly to SurfaceFlinger

    eglSwapBuffers(context->egl_display, context->egl_surface);



    free(vertices);

}



void nacl_display_destroy(NaclDisplayContext context) {

    if (!context) return;



    eglMakeCurrent(context->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    eglDestroyContext(context->egl_display, context->egl_context);

    eglDestroySurface(context->egl_display, context->egl_surface);

    eglTerminate(context->egl_display);



    glDeleteProgram(context->shader_program);

    glDeleteBuffers(1, &context->vbo);

    glDeleteVertexArrays(1, &context->vao);



    pthread_mutex_destroy(&context->mutex);

    free(context->waveform_buffer);

    free(context);

}