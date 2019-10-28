#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <unistd.h>

#include "api.h"

#define WIN_W   512
#define WIN_H   512

#define FPS     60
#define TEX_W   256
#define TEX_H   256

static GLFWwindow *window; 

static float vertices[6][4] = {
    {-1, -1, 0, 0},
    {1, -1, 1, 0},
    {1, 1, 1, 1},
    {-1, -1, 0, 0},
    {1, 1, 1, 1},
    {-1, 1, 0, 1},
};

static unsigned char buf[TEX_W * TEX_H * 4];
void init();
void update();

static void glfw_err_callback(int error, const char *desc)
{
    fprintf(stderr, "> <  GLFW: (%d) %s\n", error, desc);
}

static void glfw_fbsz_callback(GLFWwindow *window, int w, int h)
{
    glViewport(0, 0, w, h);

    float xs = (float)w / TEX_W;
    float ys = (float)h / TEX_H;
    float s = (xs < ys ? xs : ys);
    float x = s / xs;
    float y = s / ys;
    for (int i = 0; i < 6; i++) {
        vertices[i][0] = x;
        vertices[i][1] = y;
    }
#define FLIP(_a, _b) (vertices[_a][_b] = -vertices[_a][_b])
    FLIP(0, 0); FLIP(0, 1); FLIP(1, 1);
    FLIP(3, 0); FLIP(3, 1); FLIP(5, 0);
#undef FLIP
    glBufferData(GL_ARRAY_BUFFER,
        24 * sizeof(float), vertices, GL_STREAM_DRAW);
}

static inline GLuint load_shader(GLenum type, const char *source)
{
    GLuint shader_id = glCreateShader(type);
    glShaderSource(shader_id, 1, &source, NULL);
    glCompileShader(shader_id);

    GLint status;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &status);
    char msg_buf[1024];
    glGetShaderInfoLog(shader_id, sizeof(msg_buf) - 1, NULL, msg_buf);
    fprintf(stderr, "OvO  Compilation log for %s shader\n",
        (type == GL_VERTEX_SHADER ? "vertex" :
         type == GL_FRAGMENT_SHADER ? "fragment" : "unknown (!)"));
    fputs(msg_buf, stderr);
    fprintf(stderr, "=v=  End\n");
    if (status != GL_TRUE) {
        fprintf(stderr, "> <  Shader compilation failed\n");
        return 0;
    }

    return shader_id;
}

int main()
{
    // -- Initialization --

    glfwSetErrorCallback(glfw_err_callback);

    if (!glfwInit()) {
        fprintf(stderr, "> <  Cannot initialize GLFW\n");
        return 2;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    window = glfwCreateWindow(WIN_W, WIN_H, "MIKAN", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "> <  Cannot create GLFW window\n");
        return 2;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "> <  Cannot initialize GLEW\n");
        return 2;
    }

    // -- OpenGL setup --

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

#define GLSL(__source) "#version 120\n" #__source

    const char *vshader_source = GLSL(
        attribute vec2 screen_pos;
        attribute vec2 texture_pos;
        varying vec2 texture_pos_;
        void main()
        {
            gl_Position = vec4(screen_pos, 0.0, 1.0);
            texture_pos_ = texture_pos;
        }
    );

    const char *fshader_source = GLSL(
        varying vec2 texture_pos_;
        uniform sampler2D tex;
        void main()
        {
            gl_FragColor = texture2D(tex, texture_pos_);
        }
    );

    GLuint vshader = load_shader(GL_VERTEX_SHADER, vshader_source);
    GLuint fshader = load_shader(GL_FRAGMENT_SHADER, fshader_source);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vshader);
    glAttachShader(prog, fshader);
    glLinkProgram(prog);
    glUseProgram(prog);

    GLuint screen_pos_attrib_index = glGetAttribLocation(prog, "screen_pos");
    glEnableVertexAttribArray(screen_pos_attrib_index);
    glVertexAttribPointer(screen_pos_attrib_index, 2, GL_FLOAT, GL_FALSE,
        4 * sizeof(float), 0);

    GLuint texture_pos_attrib_index = glGetAttribLocation(prog, "texture_pos");
    glEnableVertexAttribArray(texture_pos_attrib_index);
    glVertexAttribPointer(texture_pos_attrib_index, 2, GL_FLOAT, GL_FALSE,
        4 * sizeof(float), (const void *)(2 * sizeof(float)));

    GLuint tex;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform1i(glGetUniformLocation(prog, "tex"), 0);

    glBufferData(GL_ARRAY_BUFFER,
        24 * sizeof(float), vertices, GL_STREAM_DRAW);

    for (int i = 0; i < TEX_H; i++)
    for (int j = 0; j < TEX_W; j++)
        buf[(i * TEX_W + j) * 4 + 3] = 255;

    // -- Event/render loop --

    init();

    float last_time = glfwGetTime(), cur_time;

    glfwSetFramebufferSizeCallback(window, glfw_fbsz_callback);

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

        glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        while ((cur_time = glfwGetTime()) < last_time + 1.0f / FPS) usleep(1);
        last_time = cur_time;

        update();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_W, TEX_H,
            0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    return 0;
}

void register_update(simply_fun update)
{
}

void pix(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b)
{
    *((uint32_t *)buf + y * TEX_W + x) = r | (g << 8) | (b << 16);
}

uint32_t buttons()
{
    return
        (glfwGetKey(window, GLFW_KEY_UP) << 0) |
        (glfwGetKey(window, GLFW_KEY_DOWN) << 1) |
        (glfwGetKey(window, GLFW_KEY_LEFT) << 2) |
        (glfwGetKey(window, GLFW_KEY_RIGHT) << 3);
}
