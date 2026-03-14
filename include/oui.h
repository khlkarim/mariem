#if !defined(OUI_HEADER)
#define OUI_HEADER

//----------------------------------------
// Windowing & Rendering Context
//----------------------------------------

typedef struct OuiContext OuiContext;

typedef struct OuiConfig {
  char *font; // path to .ttf file
  int windowWidth;
  int windowHeight;
  char *windowTitle;
} OuiConfig;

OuiContext *oui_context_create(OuiConfig *ouiConfig);
void oui_context_destroy(OuiContext *ouiContext);

int oui_get_window_width(OuiContext *ouiContext);
int oui_get_window_height(OuiContext *ouiContext);

int oui_has_next_frame(OuiContext *ouiContext);
void oui_begin_frame(OuiContext *ouiContext);
void oui_end_frame(OuiContext *ouiContext);

//----------------------------------------
// Windowing & Rendering Context
//----------------------------------------

//----------------------------------------
// Drawing stuff
//----------------------------------------

typedef struct OuiVec2i {
  float x;
  float y;
} OuiVec2i;

typedef struct OuiVec2f {
  float x;
  float y;
} OuiVec2f;

typedef struct OuiVec3f {
  float x;
  float y;
  float z;
} OuiVec3f;

typedef struct OuiVec4f {
  float x;
  float y;
  float z;
  float w;
} OuiVec4f;

typedef struct OuiColor {
  float r;
  float g;
  float b;
  float a;
} OuiColor;

typedef struct OuiVertex {
  OuiVec3f position;
  OuiColor color;
} OuiVertex;

typedef struct OuiRectangle {
  float width;
  float height;
  float rotationXY;
  OuiVec2f centerPos;

  float borderRadius;
  int borderResolution;

  OuiColor backgroundColor;
} OuiRectangle;

typedef struct OuiText {
  float startX;
  float startY;
  float rotationXY;

  float lineHeight;
  float maxLineWidth;

  int fontSize;
  OuiColor fontColor;

  char *content;
} OuiText;

void oui_draw_rectangle(OuiContext *ouiContext, OuiRectangle *rectangle);
void oui_draw_text(OuiContext *ouiContext, OuiText *text);

//----------------------------------------
// Drawing stuff
//----------------------------------------
#endif // OUI_HEADER

#if defined(OUI_USE_GLFW) && !defined(OUI_USED)
#define OUI_USED

typedef struct GLFWwindow GLFWwindow;

typedef struct FTCharater {
  unsigned int TextureID; // ID handle of the glyph texture
  OuiVec2i Size;          // Size of glyph
  OuiVec2i Bearing;       // Offset from baseline to left/top of glyph
  unsigned int Advance;   // Offset to advance to next glyph
} FTCharacter;

struct OuiContext {
  GLFWwindow *window;

  int num_characters;
  FTCharacter *fontCharacters;
};

#endif // OUI_USE_GLFW

#if defined(OUI_USE_GLFW) && defined(OUI_IMPLEMENTATION) && !defined(OUI_IMPLEMENTED)
#define OUI_IMPLEMENTED

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include <ft2build.h>
#include FT_FREETYPE_H

//----------------------------------------
// Utils (implemented at the end of the file)
//----------------------------------------

#define MAX_VERTICES 1000
#define MAX_INDICES 1000

#define PI 3.14159265359

float min(float a, float b) { return a < b ? a : b; }
float max(float a, float b) { return a > b ? a : b; }
float safe_div(float x, float y) { return y == 0 ? 0 : x / y; }
float clamp(float x, float a, float b) { return max(a, min(x, b)); }

static void glfw_flush_shader_compilation_errors(unsigned int shader, int is_shader_program);

static unsigned int glfw_compile_shader_program(
    const char *vertexShaderSource,
    const char *fragmentShaderSource);

static char *glfw_rectangle_get_vertex_shader(OuiRectangle *rectangle);
static char *glfw_rectangle_get_fragment_shader(OuiRectangle *rectangle);
static unsigned int glfw_rectangle_compile_shader_program(OuiRectangle *rectangle);

static void rectangle_compute_vertices(
    OuiContext *ouiContext,
    OuiRectangle *rectangle,
    OuiVertex *vertices, int *count);

static void rectangle_compute_indices(
    OuiContext *ouiContext,
    OuiRectangle *rectangle,
    unsigned int *indices, int *count);

static char *glfw_text_get_vertex_shader(OuiText *text);
static char *glfw_text_get_fragment_shader(OuiText *text);
static unsigned int glfw_text_compile_shader_program(OuiText *text);

//----------------------------------------
// Utils
//----------------------------------------

/* Updates the framebuffer size when the window resizes */
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

OuiContext *oui_context_create(OuiConfig *ouiConfig) {
  printf("Initializing GLFW...\n");
  if (!glfwInit()) {
    printf("Failed to initialize GLFW\n");
    return NULL;
  }

  char *defaultFont = "fonts/arial.ttf";
  int defaultWindowWidth = 800;
  int defaultWindowHeight = 600;
  char *defaultWindowTitle = "Default Window Title";

  char *font = ouiConfig == NULL || ouiConfig->font == NULL ? defaultFont : ouiConfig->font;
  int windowWidth = ouiConfig == NULL ? defaultWindowWidth : ouiConfig->windowWidth;
  int windowHeight = ouiConfig == NULL ? defaultWindowHeight : ouiConfig->windowHeight;
  char *windowTitle = ouiConfig == NULL ? defaultWindowTitle : ouiConfig->windowTitle;

  printf("Creating a window...\n");
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

  GLFWwindow *window = glfwCreateWindow(
      windowWidth,
      windowHeight,
      windowTitle,
      NULL,
      NULL);

  if (!window) {
    printf("Failed to create GLFW window\n");
    glfwTerminate();
    return NULL;
  }

  printf("Creating a renderer...\n");
  OuiContext *ouiContext = (OuiContext *)malloc(sizeof(OuiContext));

  ouiContext->window = window;
  glfwMakeContextCurrent(ouiContext->window);
  glfwSetFramebufferSizeCallback(ouiContext->window, framebuffer_size_callback);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  ouiContext->num_characters = 128;
  ouiContext->fontCharacters = (FTCharacter *)malloc(ouiContext->num_characters * sizeof(FTCharacter));

  FTCharacter null_character = {0};
  for (int i = 0; i < ouiContext->num_characters; i++) {
    ouiContext->fontCharacters[i] = null_character;
  }

  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    printf("Could not init FreeType Library\n");
    return ouiContext;
  }

  FT_Face face;
  if (FT_New_Face(ft, font, 0, &face)) {
    printf("Failed to load font: %s\n", font);
    return ouiContext;
  }

  FT_Set_Pixel_Sizes(face, 0, 48);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

  for (unsigned char c = 0; c < 128; c++) {
    // load character glyph
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      printf("Failed to load Glyph\n");
      continue;
    }

    // generate texture
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width,
                 face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE,
                 face->glyph->bitmap.buffer);

    // set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // now store character for later use
    FTCharacter character = {
        texture,
        {face->glyph->bitmap.width, face->glyph->bitmap.rows},
        {face->glyph->bitmap_left, face->glyph->bitmap_top},
        face->glyph->advance.x};

    ouiContext->fontCharacters[c] = character;
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  return ouiContext;
}

void oui_context_destroy(OuiContext *ouiContext) {
  if (ouiContext == NULL || ouiContext->window == NULL) {
    return;
  }

  glfwDestroyWindow(ouiContext->window);
  glfwTerminate();
  ouiContext->window = NULL;
}

int oui_has_next_frame(OuiContext *ouiContext) {
  if (ouiContext == NULL) {
    return 0;
  }

  return !glfwWindowShouldClose(ouiContext->window);
}

int oui_get_window_width(OuiContext *ouiContext) {
  int screenWidth, screenHeight;
  glfwGetFramebufferSize(ouiContext->window, &screenWidth, &screenHeight);

  return screenWidth;
}

int oui_get_window_height(OuiContext *ouiContext) {
  int screenWidth, screenHeight;
  glfwGetFramebufferSize(ouiContext->window, &screenWidth, &screenHeight);

  return screenHeight;
}

void oui_begin_frame(OuiContext *ouiContext) {
  glClearColor(0.39f, 0.58f, 0.93f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void oui_end_frame(OuiContext *ouiContext) {
  if (ouiContext == NULL) {
    return;
  }

  glfwSwapBuffers(ouiContext->window);
  glfwPollEvents();
}

static void glfw_flush_shader_compilation_errors(unsigned int shader, int is_shader_program) {
  int success;
  char infoLog[512];

  if (is_shader_program) {
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
  } else {
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  }

  if (!success) {
    if (is_shader_program) {
      glGetProgramInfoLog(shader, 512, NULL, infoLog);
    } else {
      glGetShaderInfoLog(shader, 512, NULL, infoLog);
    }
    printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED: shader_id: %d, infoLog: "
           "%s\n",
           shader, infoLog);
  }
}

static unsigned int glfw_compile_shader_program(
    const char *vertexShaderSource,
    const char *fragmentShaderSource) {
  assert(
      vertexShaderSource != NULL &&
      fragmentShaderSource != NULL &&
      "Vertex and fragment shaders are required");

  unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader(vertexShader);
  glfw_flush_shader_compilation_errors(vertexShader, 0);

  unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);
  glfw_flush_shader_compilation_errors(fragmentShader, 0);

  unsigned int shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  glfw_flush_shader_compilation_errors(shaderProgram, 1);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return shaderProgram;
}

static char *glfw_rectangle_get_vertex_shader(OuiRectangle *rectangle) {
  char *vertexShaderSource =
      "#version 330 core\n"
      "layout (location = 0) in vec3 aPos;\n"
      "layout (location = 1) in vec4 aColor;\n"
      "out vec4 vColor;\n"
      "void main()\n"
      "{\n"
      "   vColor = aColor;\n"
      "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
      "}\0";

  return vertexShaderSource;
}

static char *glfw_rectangle_get_fragment_shader(OuiRectangle *rectangle) {
  char *fragmentShaderSource =
      "#version 330 core\n"
      "out vec4 FragColor;\n"
      "in vec4 vColor;\n"
      "void main()\n"
      "{\n"
      "   FragColor = vColor;\n"
      "}\n\0";

  return fragmentShaderSource;
}

static unsigned int glfw_rectangle_compile_shader_program(OuiRectangle *rectangle) {
  const char *vertexShaderSource = glfw_rectangle_get_vertex_shader(rectangle);
  const char *fragmentShaderSource = glfw_rectangle_get_fragment_shader(rectangle);

  return glfw_compile_shader_program(vertexShaderSource, fragmentShaderSource);
}

void oui_draw_rectangle(OuiContext *ouiContext, OuiRectangle *rectangle) {
  if (ouiContext == NULL || rectangle == NULL) {
    return;
  }

  int num_vertices, num_indices;
  OuiVertex vertices[MAX_VERTICES] = {0};
  unsigned int indices[MAX_INDICES] = {0};

  rectangle_compute_vertices(ouiContext, rectangle, vertices, &num_vertices);
  rectangle_compute_indices(ouiContext, rectangle, indices, &num_indices);

  const unsigned int shaderProgram = glfw_rectangle_compile_shader_program(rectangle);

  // TODO: dont do what u dont gotta do
  unsigned int VBO, VAO, EBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, num_vertices * sizeof(OuiVertex), vertices,
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindVertexArray(0);

  glUseProgram(shaderProgram);
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);

  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteProgram(shaderProgram);
}

static void rectangle_compute_vertices(
    OuiContext *ouiContext,
    OuiRectangle *rectangle,
    OuiVertex *vertices, int *count) {
  if (ouiContext == NULL || rectangle == NULL || vertices == NULL) {
    *count = 0;
    return;
  }

  OuiVertex default_vertices[4] = {
      {{-0.5, 0.5, 0.0}, {0}},  // top left
      {{0.5, 0.5, 0.0}, {0}},   // top right
      {{0.5, -0.5, 0.0}, {0}},  // bottom right
      {{-0.5, -0.5, 0.0}, {0}}, // bottom left
  };

  float rectWidth = rectangle->width;
  float rectHeight = rectangle->height;

  float centerX = rectangle->centerPos.x;
  float centerY = rectangle->centerPos.y;

  OuiColor bgColor = {
      .r = clamp(rectangle->backgroundColor.r, 0, 255) / 256,
      .g = clamp(rectangle->backgroundColor.g, 0, 255) / 256,
      .b = clamp(rectangle->backgroundColor.b, 0, 255) / 256,
      .a = clamp(rectangle->backgroundColor.a, 0, 255) / 256,
  };

  int is_rounded = rectangle->borderRadius > 0 && rectangle->borderResolution > 0;

  if (is_rounded) {
    int segments = rectangle->borderResolution;

    float raduis = rectangle->borderRadius;
    float reducedWidth = rectWidth - 2 * raduis;
    float reducedHeight = rectHeight - 2 * raduis;

    int vRectangles = 8;
    int vCercles = 4 * (1 + segments);
    *count = vRectangles + vCercles;

    for (int i = 0; i < vRectangles; i++) {
      vertices[i].position.x = centerX;
      vertices[i].position.y = centerY;

      if (i < 4) {
        vertices[i].position.x += default_vertices[i].position.x * reducedWidth;
        vertices[i].position.y += default_vertices[i].position.y * rectHeight;
      } else {
        vertices[i].position.x += default_vertices[i % 4].position.x * rectWidth;
        vertices[i].position.y += default_vertices[i % 4].position.y * reducedHeight;
      }

      vertices[i].position.z = 0.0;
      vertices[i].color = bgColor;
    }

    for (int baseIdx = vRectangles; baseIdx < *count; baseIdx += 1 + segments) {
      vertices[baseIdx].position.x =
          centerX +
          reducedWidth *
              default_vertices[(baseIdx - vRectangles) / (1 + segments)].position.x;

      vertices[baseIdx].position.y =
          centerY +
          reducedHeight *
              default_vertices[(baseIdx - vRectangles) / (1 + segments)].position.y;

      vertices[baseIdx].position.z = 0.0;

      vertices[baseIdx].color = bgColor;

      float theta = 0.0f, deltaTheta = 2 * PI / segments;
      for (int i = 1; i < segments + 1; i++, theta += deltaTheta) {
        vertices[baseIdx + i].position.x =
            vertices[baseIdx].position.x +
            raduis * cos((double)theta);

        vertices[baseIdx + i].position.y =
            vertices[baseIdx].position.y +
            raduis * sin((double)theta);

        vertices[baseIdx + i].position.z = 0.0;
        vertices[baseIdx + i].color = bgColor;
      }
    }

  } else {
    *count = 4;

    for (int i = 0; i < *count; i++) {
      vertices[i].position.x = centerX + default_vertices[i].position.x * rectWidth;
      vertices[i].position.y = centerY + default_vertices[i].position.y * rectHeight;
      vertices[i].position.z = 0.0;

      vertices[i].color = bgColor;
    }
  }

  int screenWidth, screenHeight;
  glfwGetWindowSize(ouiContext->window, &screenWidth, &screenHeight);

  for (int i = 0; i < *count; i++) {
    float x = vertices[i].position.x - centerX;
    float y = vertices[i].position.y - centerY;

    // apply rotation
    vertices[i].position.x = x * cos(rectangle->rotationXY) - y * sin(rectangle->rotationXY) + centerX;
    vertices[i].position.y = x * sin(rectangle->rotationXY) + y * cos(rectangle->rotationXY) + centerY;
    vertices[i].position.z = 0.0;

    // transform to screen space
    vertices[i].position.x = safe_div(vertices[i].position.x, screenWidth);
    vertices[i].position.y = safe_div(vertices[i].position.y, screenHeight);
  }
}

static void rectangle_compute_indices(
    OuiContext *ouiContext,
    OuiRectangle *rectangle,
    unsigned int *indices, int *count) {
  if (ouiContext == NULL || rectangle == NULL) {
    *count = 0;
    return;
  }

  int is_rounded = rectangle->borderRadius > 0 && rectangle->borderResolution > 0;

  if (is_rounded) {
    int num_segments = rectangle->borderResolution;
    *count = 12 + 4 * (3 * num_segments);

    // first rectangle
    indices[0] = 0;
    indices[1] = 2;
    indices[2] = 1;
    indices[3] = 0;
    indices[4] = 3;
    indices[5] = 2;

    // second rectangle
    indices[6] = 4;
    indices[7] = 6;
    indices[8] = 5;
    indices[9] = 4;
    indices[10] = 7;
    indices[11] = 6;

    int verticesBaseIdx = 8;
    for (int baseIdx = 12; baseIdx < *count; baseIdx += 3 * num_segments) {
      int centerIdx = verticesBaseIdx + ((baseIdx - 12) / (3 * num_segments)) * (1 + num_segments);

      for (int i = 0; i < 3 * num_segments; i += 3) {
        indices[baseIdx + i] = centerIdx;

        int currVertexIdx = centerIdx + i / 3 + 1;
        indices[baseIdx + i + 1] = currVertexIdx;
        indices[baseIdx + i + 2] = currVertexIdx + 1;

        if (currVertexIdx - centerIdx == num_segments)
          indices[baseIdx + i + 2] = centerIdx + 1;
      }
    }
  } else {
    *count = 6;
    indices[0] = 0;
    indices[1] = 2;
    indices[2] = 1;
    indices[3] = 0;
    indices[4] = 3;
    indices[5] = 2;
  }
}

static char *glfw_text_get_vertex_shader(OuiText *text) {
  char *vertexShaderSource =
      "#version 330 core\n"
      "layout (location = 0) in vec4 vertex;\n"
      "out vec2 TexCoords;\n"
      "void main()\n"
      "{\n"
      "   TexCoords = vertex.zw;\n"
      "   gl_Position = vec4(vertex.xy, 0.0, 1.0);\n"
      "}\0";

  return vertexShaderSource;
}

static char *glfw_text_get_fragment_shader(OuiText *text) {
  char *fragmentShaderSource =
      "#version 330 core\n"
      "in vec2 TexCoords;\n"
      "out vec4 color;\n"
      "uniform sampler2D text;"
      "uniform vec3 textColor;"
      "void main()\n"
      "{\n"
      "   vec4 sampled = vec4(vec3(1.0), texture(text, TexCoords).r);\n"
      "   color = vec4(textColor, 1.0) * sampled;\n"
      "}\n\0";

  return fragmentShaderSource;
}

static unsigned int glfw_text_compile_shader_program(OuiText *text) {
  const char *vertexShaderSource = glfw_text_get_vertex_shader(text);
  const char *fragmentShaderSource = glfw_text_get_fragment_shader(text);

  return glfw_compile_shader_program(vertexShaderSource, fragmentShaderSource);
}

void oui_draw_text(OuiContext *ouiContext, OuiText *text) {
  if (ouiContext == NULL || text == NULL) {
    return;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  unsigned int shader = glfw_text_compile_shader_program(text);

  unsigned int VAO, VBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);

  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  glUseProgram(shader);
  glUniform3f(
      glGetUniformLocation(shader, "textColor"),
      text->fontColor.r,
      text->fontColor.g,
      text->fontColor.b);

  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(VAO);

  int screenWidth, screenHeight;
  glfwGetWindowSize(ouiContext->window, &screenWidth, &screenHeight);

  int x = text->startX, y = text->startY;

  for (int c = 0; text->content[c] != '\0'; c++) {
    FTCharacter ch = ouiContext->fontCharacters[(int)text->content[c]];

    float xpos = x + ch.Bearing.x * text->fontSize;
    float ypos = y - (ch.Size.y - ch.Bearing.y) * text->fontSize;

    float w = ch.Size.x * text->fontSize;
    float h = ch.Size.y * text->fontSize;

    xpos /= screenWidth;
    ypos /= screenHeight;
    w /= screenWidth;
    h /= screenHeight;

    float vertices[6][4] = {
        {xpos, ypos + h, 0.0f, 0.0f},
        {xpos, ypos, 0.0f, 1.0f},
        {xpos + w, ypos, 1.0f, 1.0f},
        {xpos, ypos + h, 0.0f, 0.0f},
        {xpos + w, ypos, 1.0f, 1.0f},
        {xpos + w, ypos + h, 1.0f, 0.0f}};

    // render glyph texture over quad
    glBindTexture(GL_TEXTURE_2D, ch.TextureID);

    // update content of VBO memory
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // render quad
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
    x += (ch.Advance >> 6) * text->fontSize; // bitshift by 6 to get value in pixels (2^6 = 64)
    float nextCharWidth = text->content[c + 1] == '\0' ? 0 : (ouiContext->fontCharacters[(int)text->content[c + 1]].Advance >> 6) * text->fontSize;

    if (x + nextCharWidth > text->maxLineWidth) {
      x = text->startX;
      y -= text->lineHeight;
    }
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);

  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(shader);
}

void oui_layout_vertically(OuiContext *ouiContext, OuiRectangle *rectangles, int count) {
  if (ouiContext == NULL || rectangles == NULL) {
    return;
  }

  float anchorX = rectangles[0].centerPos.x;
  float anchorY = rectangles[0].centerPos.y - rectangles[0].height / 2;

  for (int i = 1; i < count; i++) {
    anchorY -= rectangles[i].height / 2;

    rectangles[i].centerPos.x = anchorX;
    rectangles[i].centerPos.y = anchorY;

    anchorY -= rectangles[i].height / 2;
  }
}

void oui_layout_horizontally(OuiContext *ouiContext, OuiRectangle *rectangles, int count) {
  if (ouiContext == NULL || rectangles == NULL) {
    return;
  }

  int anchorX = rectangles[0].centerPos.x + rectangles[0].width / 2;
  int anchorY = rectangles[0].centerPos.y;

  for (int i = 1; i < count; i++) {
    anchorX += rectangles[i].width / 2;

    rectangles[i].centerPos.x = anchorX;
    rectangles[i].centerPos.y = anchorY;

    anchorX += rectangles[i].width / 2;
  }
}

#endif // OUI_GLFW
