#if !defined(OUI_HEADER)
#define OUI_HEADER

#define OUI_COLOR_BLACK ((OuiColor){0.0, 0.0, 0.0, 255.0})
#define OUI_COLOR_RED ((OuiColor){255.0, 0.0, 0.0, 255.0})
#define OUI_COLOR_GREEN ((OuiColor){0.0, 255.0, 0.0, 255.0})
#define OUI_COLOR_BLUE ((OuiColor){0.0, 0.0, 255.0, 255.0})
#define OUI_COLOR_WHITE ((OuiColor){255.0, 255.0, 255.0, 255.0})

#define OUI_DEFAULT_FONT "fonts/arial.ttf"
#define OUI_DEFAULT_WINDOW_TITLE "Hi"
#define OUI_DEFAULT_WINDOW_WIDTH 800
#define OUI_DEFAULT_WINDOW_HEIGHT 600

typedef struct OuiVec2i {
  int x;
  int y;
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
  float borderResolution;

  OuiColor backgroundColor;
} OuiRectangle;

typedef struct OuiText {
  float fontSize;
  float lineHeight;
  float maxLineWidth;
  OuiVec2f startPos;
  float rotationXY;

  char *content;
  OuiColor fontColor;
} OuiText;

typedef struct OuiContext OuiContext;

typedef struct OuiConfig {
  char *font; // path to a .ttf file
  int windowWidth;
  int windowHeight;
  char *windowTitle;
} OuiConfig;

void oui_context_init(OuiContext *ouiContext, OuiConfig *ouiConfig);
void oui_context_destroy(OuiContext *ouiContext);

int oui_has_next_frame(OuiContext *ouiContext);
void oui_begin_frame(OuiContext *ouiContext);
void oui_end_frame(OuiContext *ouiContext);

void oui_draw_rectangle(OuiContext *ouiContext, OuiRectangle *rectangle);
void oui_draw_text(OuiContext *ouiContext, OuiText *text);

OuiVec2i oui_get_window_size(OuiContext *ouiContext);
int oui_get_window_width(OuiContext *ouiContext);
int oui_get_window_height(OuiContext *ouiContext);

#endif // OUI_HEADER

#if defined(OUI_USE_GLFW) && !defined(OUI_USED)
#define OUI_USED

#define MAX_RECTANGLES 10000
#define MAX_CERCLES 4 * MAX_RECTANGLES

typedef struct GLFWwindow GLFWwindow;

typedef struct FontCharacter {
  OuiVec2i Size;
  OuiVec2i Bearing;
  unsigned int Advance;
  unsigned int TextureID;
} FontCharacter;

struct OuiContext {
  GLFWwindow *window;

  int numCharacters;
  FontCharacter *fontCharacters;

  unsigned int rectangleInstanceVBO;
  unsigned int rectangleShaderProgram;
  unsigned int rectangleVAO, rectangleVBO;

  unsigned int cercleInstanceVBO;
  unsigned int cercleShaderProgram;
  unsigned int cercleVAO, cercleVBO;

  unsigned int textShaderProgram;
  unsigned int textVAO, textVBO;

  // TODO: would it be more effitient to store less data (introduce a minimal struct)?
  int countRectangles;
  OuiRectangle scheduledRectangles[MAX_RECTANGLES];

  int countCerlces;
  OuiRectangle scheduledCercles[MAX_CERCLES];
};

#endif // OUI_USE_GLFW

#if defined(OUI_USE_GLFW) && defined(OUI_IMPLEMENTATION) && !defined(OUI_IMPLEMENTED)
#define OUI_IMPLEMENTED

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include <ft2build.h>
#include FT_FREETYPE_H

//----------------------------------------
// Utils
//----------------------------------------

#define OUI_PI 3.14159265359

float oui_min(float a, float b) { return a < b ? a : b; }
float oui_max(float a, float b) { return a > b ? a : b; }
void framebuffer_size_callback(GLFWwindow *window, int width, int height);

#define OUI_UNIT_RECTANGLE_VERTEX_COUNT 4
#define OUI_UNIT_CERCLE_SEGMENT_COUNT 50
#define OUI_UNIT_CERCLE_INDEX_COUNT 3 * OUI_UNIT_CERCLE_SEGMENT_COUNT
#define OUI_UNIT_CERCLE_VERTEX_COUNT OUI_UNIT_CERCLE_SEGMENT_COUNT + 1

static const OuiVertex OUI_UNIT_RECTANGLE_VERTICES[] = {
    {{-0.5, 0.5, 0.0}, {0}},
    {{0.5, -0.5, 0.0}, {0}},
    {{-0.5, -0.5, 0.0}, {0}},
    {{0.5, 0.5, 0.0}, {0}},
};

static const OuiVertex OUI_UNIT_RECTANGLE[] = {
    OUI_UNIT_RECTANGLE_VERTICES[0],
    OUI_UNIT_RECTANGLE_VERTICES[1],
    OUI_UNIT_RECTANGLE_VERTICES[2],

    OUI_UNIT_RECTANGLE_VERTICES[0],
    OUI_UNIT_RECTANGLE_VERTICES[1],
    OUI_UNIT_RECTANGLE_VERTICES[3],
};

static OuiVertex OUI_UNIT_CERCLE[OUI_UNIT_CERCLE_INDEX_COUNT] = {0};
static void oui__compute_unit_cercle(void);

static void oui__glfw_flush_shader_compilation_errors(unsigned int shader, int isShaderProgram);
static unsigned int oui__glfw_compile_shader_program(const char *vertexShaderSource, const char *fragmentShaderSource);

static char *oui__glfw_rectangle_get_vertex_shader(OuiRectangle *rectangle);
static char *oui__glfw_rectangle_get_fragment_shader(OuiRectangle *rectangle);
static unsigned int oui__glfw_rectangle_compile_shader_program(OuiRectangle *rectangle);

static char *oui__glfw_cercle_get_vertex_shader(void);
static unsigned int oui__glfw_cercle_compile_shader_program(void);

static char *oui__glfw_text_get_vertex_shader(OuiText *text);
static char *oui__glfw_text_get_fragment_shader(OuiText *text);
static unsigned int oui__glfw_text_compile_shader_program(OuiText *text);

static void oui__glfw_create_window(OuiContext *ouiContext, OuiConfig *ouiConfig);
static void oui__freetype_load_font(OuiContext *ouiContext, OuiConfig *ouiConfig);
static void oui__initialize_gpu_objects(OuiContext *ouiContext, OuiConfig *ouiConfig);

//----------------------------------------
// Utils
//----------------------------------------

//----------------------------------------
// OuiContext
//----------------------------------------

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

void oui_context_init(OuiContext *ouiContext, OuiConfig *ouiConfig) {
  if (!glfwInit()) {
    printf("Failed to initialize GLFW\n");
    return;
  }

  oui__glfw_create_window(ouiContext, ouiConfig);
  oui__freetype_load_font(ouiContext, ouiConfig);
  oui__initialize_gpu_objects(ouiContext, ouiConfig);
}

static void oui__glfw_create_window(OuiContext *ouiContext, OuiConfig *ouiConfig) {
  if (ouiContext == NULL) {
    return;
  }

  int windowWidth = ouiConfig == NULL ? OUI_DEFAULT_WINDOW_WIDTH : ouiConfig->windowWidth;
  int windowHeight = ouiConfig == NULL ? OUI_DEFAULT_WINDOW_HEIGHT : ouiConfig->windowHeight;
  char *windowTitle = ouiConfig == NULL ? OUI_DEFAULT_WINDOW_TITLE : ouiConfig->windowTitle;

  glfwWindowHint(GLFW_SAMPLES, 4); // anti-aliasing
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(
      windowWidth,
      windowHeight,
      windowTitle,
      NULL,
      NULL);

  if (!window) {
    printf("Failed to create GLFW window\n");
    glfwTerminate();
    return;
  }

  ouiContext->window = window;
  glfwMakeContextCurrent(ouiContext->window);
  glfwSetFramebufferSizeCallback(ouiContext->window, framebuffer_size_callback);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
}

static void oui__freetype_load_font(OuiContext *ouiContext, OuiConfig *ouiConfig) {
  if (ouiContext == NULL) {
    return;
  }

  char *font = ouiConfig == NULL || ouiConfig->font == NULL ? OUI_DEFAULT_FONT : ouiConfig->font;

  // TODO: try to get rid of this alloc
  ouiContext->numCharacters = 128;
  ouiContext->fontCharacters = (FontCharacter *)calloc(ouiContext->numCharacters, sizeof(FontCharacter));

  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    printf("Could not init FreeType Library\n");
    return;
  }

  FT_Face face;
  if (FT_New_Face(ft, font, 0, &face)) {
    printf("Failed to load font: %s\n", font);
    return;
  }

  FT_Set_Pixel_Sizes(face, 0, 48);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

  for (unsigned char c = 0; c < ouiContext->numCharacters; c++) {
    // load character glyph
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      printf("Failed to load Glyph: %d\n", c);
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
    FontCharacter character = {
        .TextureID = texture,
        .Size = {face->glyph->bitmap.width, face->glyph->bitmap.rows},
        .Bearing = {face->glyph->bitmap_left, face->glyph->bitmap_top},
        .Advance = face->glyph->advance.x,
    };

    ouiContext->fontCharacters[c] = character;
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);
}

static void oui__initialize_gpu_objects(OuiContext *ouiContext, OuiConfig *ouiConfig) {
  if (ouiContext == NULL) {
    return;
  }

  glEnable(GL_BLEND);
  glEnable(GL_MULTISAMPLE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // ---------------------- text -----------------------
  ouiContext->textShaderProgram = oui__glfw_text_compile_shader_program(NULL);
  glGenVertexArrays(1, &(ouiContext->textVAO));
  glGenBuffers(1, &(ouiContext->textVBO));

  glBindVertexArray(ouiContext->textVAO);
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->textVBO);

  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  // ---------------------- text -----------------------

  // -------------------- Rectangle --------------------
  ouiContext->rectangleShaderProgram = oui__glfw_rectangle_compile_shader_program(NULL);

  // per-instance data buffer
  glGenBuffers(1, &(ouiContext->rectangleInstanceVBO));
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->rectangleInstanceVBO);
  glBufferData(GL_ARRAY_BUFFER, MAX_RECTANGLES * sizeof(OuiRectangle), NULL, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glGenVertexArrays(1, &(ouiContext->rectangleVAO));

  // data shared between all the instances
  glGenBuffers(1, &(ouiContext->rectangleVBO));

  glBindVertexArray(ouiContext->rectangleVAO);
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->rectangleVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(OUI_UNIT_RECTANGLE), OUI_UNIT_RECTANGLE, GL_STATIC_DRAW);

  // shared attributes
  glEnableVertexAttribArray(0); // position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(1); // color
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)(3 * sizeof(float)));

  // per-instance attributes
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->rectangleInstanceVBO);

  int stride = sizeof(OuiRectangle), offset = 0;
  int sizes[4] = {
      3, // (width, height, rotationXY)
      2, // centerPos
      2, // (borderRadius, borderRadius)
      4, // backgroundColor
  };

  for (int i = 2; i <= 5; i++) {
    glEnableVertexAttribArray(i);

    glVertexAttribPointer(
        i,
        sizes[i - 2],
        GL_FLOAT,
        GL_FALSE,
        stride,
        (void *)(offset * sizeof(float)));

    glVertexAttribDivisor(i, 1);
    offset += sizes[i - 2];
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  // -------------------- Rectangle --------------------

  // --------------------- Cercle ----------------------
  ouiContext->cercleShaderProgram = oui__glfw_cercle_compile_shader_program();

  // per-instance data buffer
  glGenBuffers(1, &(ouiContext->cercleInstanceVBO));
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->cercleInstanceVBO);
  glBufferData(GL_ARRAY_BUFFER, MAX_CERCLES * sizeof(OuiRectangle), NULL, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glGenVertexArrays(1, &(ouiContext->cercleVAO));

  // data shared between all the instances
  glGenBuffers(1, &(ouiContext->cercleVBO));

  oui__compute_unit_cercle();
  glBindVertexArray(ouiContext->cercleVAO);
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->cercleVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(OUI_UNIT_CERCLE), OUI_UNIT_CERCLE, GL_STATIC_DRAW);

  // shared attributes
  glEnableVertexAttribArray(0); // position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(1); // color
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)(3 * sizeof(float)));

  // per-instance attributes
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->cercleInstanceVBO);

  offset = 0;
  for (int i = 2; i <= 5; i++) {
    glEnableVertexAttribArray(i);

    glVertexAttribPointer(
        i,
        sizes[i - 2],
        GL_FLOAT,
        GL_FALSE,
        stride,
        (void *)(offset * sizeof(float)));

    glVertexAttribDivisor(i, 1);
    offset += sizes[i - 2];
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  // --------------------- Cercle ----------------------
}

void oui_context_destroy(OuiContext *ouiContext) {
  if (ouiContext == NULL || ouiContext->window == NULL) {
    return;
  }

  glDeleteVertexArrays(1, &(ouiContext->textVAO));
  glDeleteVertexArrays(1, &(ouiContext->cercleVAO));
  glDeleteVertexArrays(1, &(ouiContext->rectangleVAO));

  glDeleteBuffers(1, &(ouiContext->textVBO));
  glDeleteBuffers(1, &(ouiContext->cercleVBO));
  glDeleteBuffers(1, &(ouiContext->rectangleVBO));

  glDeleteProgram(ouiContext->textShaderProgram);
  glDeleteProgram(ouiContext->cercleShaderProgram);
  glDeleteProgram(ouiContext->rectangleShaderProgram);

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

void oui_begin_frame(OuiContext *ouiContext) {
  glClearColor(0.39f, 0.58f, 0.93f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void oui_end_frame(OuiContext *ouiContext) {
  if (ouiContext == NULL) {
    return;
  }

  OuiVec2f windowSize = {
      .x = oui_get_window_width(ouiContext),
      .y = oui_get_window_height(ouiContext),
  };

  // draw the scheduled rectangles
  unsigned int
      shader = ouiContext->rectangleShaderProgram,
      rectangleVAO = ouiContext->rectangleVAO;

  glUseProgram(shader);
  glUniform2f(
      glGetUniformLocation(shader, "uWindowSize"),
      windowSize.x, windowSize.y);

  glBindVertexArray(rectangleVAO);
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->rectangleInstanceVBO);
  glBufferSubData(
      GL_ARRAY_BUFFER,
      0, ouiContext->countRectangles * sizeof(OuiRectangle),
      &(ouiContext->scheduledRectangles[0]));

  glDrawArraysInstanced(
      GL_TRIANGLES,
      0, 6,
      ouiContext->countRectangles);

  glBindVertexArray(0);
  ouiContext->countRectangles = 0;

  // draw the scheduled cercles
  unsigned int
      cercleShader = ouiContext->cercleShaderProgram,
      cercleVAO = ouiContext->cercleVAO;

  glUseProgram(cercleShader);
  glUniform2f(
      glGetUniformLocation(cercleShader, "uWindowSize"),
      windowSize.x, windowSize.y);

  glBindVertexArray(cercleVAO);
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->cercleInstanceVBO);
  glBufferSubData(
      GL_ARRAY_BUFFER,
      0, ouiContext->countCerlces * sizeof(OuiRectangle),
      &(ouiContext->scheduledCercles[0]));

  glDrawArraysInstanced(GL_TRIANGLES, 0, OUI_UNIT_CERCLE_INDEX_COUNT, ouiContext->countCerlces);

  glBindVertexArray(0);
  ouiContext->countCerlces = 0;

  glfwSwapBuffers(ouiContext->window);
  glfwPollEvents();
}

OuiVec2i oui_get_window_size(OuiContext *ouiContext) {
  OuiVec2i size = {0};

  if (ouiContext == NULL) {
    return size;
  }

  glfwGetFramebufferSize(ouiContext->window, &(size.x), &(size.y));
  return size;
}

int oui_get_window_width(OuiContext *ouiContext) {
  if (ouiContext == NULL) {
    return 0;
  }

  int screenWidth = 0, screenHeight = 0;
  glfwGetFramebufferSize(ouiContext->window, &screenWidth, &screenHeight);
  return screenWidth;
}

int oui_get_window_height(OuiContext *ouiContext) {
  if (ouiContext == NULL) {
    return 0;
  }

  int screenWidth = 0, screenHeight = 0;
  glfwGetFramebufferSize(ouiContext->window, &screenWidth, &screenHeight);
  return screenHeight;
}

void oui_flush_draw_calls(OuiContext *ouiContext) {
  if (ouiContext == NULL) {
    return;
  }

  OuiVec2f windowSize = {
      .x = oui_get_window_width(ouiContext),
      .y = oui_get_window_height(ouiContext),
  };

  // draw the scheduled rectangles
  unsigned int
      shader = ouiContext->rectangleShaderProgram,
      rectangleVAO = ouiContext->rectangleVAO;

  glUseProgram(shader);
  glUniform2f(
      glGetUniformLocation(shader, "uWindowSize"),
      windowSize.x, windowSize.y);

  glBindVertexArray(rectangleVAO);
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->rectangleInstanceVBO);
  glBufferSubData(
      GL_ARRAY_BUFFER,
      0, ouiContext->countRectangles * sizeof(OuiRectangle),
      &(ouiContext->scheduledRectangles[0]));

  glDrawArraysInstanced(
      GL_TRIANGLES,
      0, 6,
      ouiContext->countRectangles);

  glBindVertexArray(0);
  ouiContext->countRectangles = 0;

  // draw the scheduled cercles
  unsigned int
      cercleShader = ouiContext->cercleShaderProgram,
      cercleVAO = ouiContext->cercleVAO;

  glUseProgram(cercleShader);
  glUniform2f(
      glGetUniformLocation(cercleShader, "uWindowSize"),
      windowSize.x, windowSize.y);

  glBindVertexArray(cercleVAO);
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->cercleInstanceVBO);
  glBufferSubData(
      GL_ARRAY_BUFFER,
      0, ouiContext->countCerlces * sizeof(OuiRectangle),
      &(ouiContext->scheduledCercles[0]));

  glDrawArraysInstanced(GL_TRIANGLES, 0, OUI_UNIT_CERCLE_INDEX_COUNT, ouiContext->countCerlces);

  glBindVertexArray(0);
  ouiContext->countCerlces = 0;
}

//----------------------------------------
// OuiContext
//----------------------------------------

//----------------------------------------
// Rectangle
//----------------------------------------

void oui_draw_rectangle(OuiContext *ouiContext, OuiRectangle *rectangle) {
  // TODO: if there is no memory space to schedule a rectangle
  // draw it now, rather than discard it

  if (ouiContext == NULL || rectangle == NULL) {
    return;
  }

  int isRounded = rectangle->borderRadius > 0;

  float maxBorderRadius = oui_min(rectangle->width / 2.0, rectangle->height / 2.0);
  rectangle->borderRadius = oui_min(rectangle->borderRadius, maxBorderRadius);

  if (!isRounded) {
    if (ouiContext->countRectangles >= MAX_RECTANGLES) {
      return;
    }

    OuiRectangle *slot = &(ouiContext->scheduledRectangles[ouiContext->countRectangles]);
    ouiContext->countRectangles++;
    *slot = *rectangle;

    return;
  }

  // its so rounded it becomes a cercle
  if (
      isRounded &&
      rectangle->borderRadius == maxBorderRadius &&
      rectangle->width == rectangle->height) {

    OuiRectangle *slot = &(ouiContext->scheduledCercles[ouiContext->countCerlces]);
    ouiContext->countCerlces++;
    *slot = *rectangle;

    return;
  }

  // draw rounded rectangle: 2 sharp rectangles and 4 cercles
  if (ouiContext->countRectangles > MAX_RECTANGLES - 2) {
    return;
  }
  if (ouiContext->countCerlces > MAX_CERCLES - 4) {
    return;
  }

  OuiRectangle *slot1 = &(ouiContext->scheduledRectangles[ouiContext->countRectangles]);

  *slot1 = *rectangle;
  slot1->height -= 2 * rectangle->borderRadius;

  ouiContext->countRectangles++;

  OuiRectangle *slot2 = &(ouiContext->scheduledRectangles[ouiContext->countRectangles]);

  *slot2 = *rectangle;
  slot2->width -= 2 * rectangle->borderRadius;

  ouiContext->countRectangles++;

  for (int i = 0; i < OUI_UNIT_RECTANGLE_VERTEX_COUNT; i++) {
    OuiRectangle *slot = &(ouiContext->scheduledCercles[ouiContext->countCerlces]);
    ouiContext->countCerlces++;
    *slot = *rectangle;

    float x = (rectangle->width - 2 * rectangle->borderRadius) * OUI_UNIT_RECTANGLE_VERTICES[i].position.x;
    float y = (rectangle->height - 2 * rectangle->borderRadius) * OUI_UNIT_RECTANGLE_VERTICES[i].position.y;

    slot->centerPos.x = x * cos(rectangle->rotationXY) - y * sin(rectangle->rotationXY) + rectangle->centerPos.x;
    slot->centerPos.y = x * sin(rectangle->rotationXY) + y * cos(rectangle->rotationXY) + rectangle->centerPos.y;
  }

  oui_flush_draw_calls(ouiContext);
}

//----------------------------------------
// Rectangle
//----------------------------------------

//----------------------------------------
// Text
//----------------------------------------

void oui_draw_text(OuiContext *ouiContext, OuiText *text) {
  if (
      ouiContext == NULL ||
      ouiContext->fontCharacters == NULL ||
      text == NULL || text->content == NULL) {
    return;
  }

  int windowWidth, windowHeight;
  glfwGetWindowSize(ouiContext->window, &windowWidth, &windowHeight);

  if (windowWidth == 0 || windowHeight == 0) {
    return;
  }

  unsigned int shader = ouiContext->textShaderProgram;

  unsigned int
      VAO = ouiContext->textVAO,
      VBO = ouiContext->textVBO;

  glUseProgram(shader);
  glUniform3f(
      glGetUniformLocation(shader, "textColor"),
      text->fontColor.r,
      text->fontColor.g,
      text->fontColor.b);

  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(VAO);

  int x = text->startPos.x, y = text->startPos.y;

  for (int i = 0; text->content[i] != '\0'; i++) {
    char c = text->content[i];
    if (c >= ouiContext->numCharacters) {
      continue;
    }

    FontCharacter ch = ouiContext->fontCharacters[(int)c];

    float xpos = x + ch.Bearing.x * text->fontSize;
    float ypos = y - (ch.Size.y - ch.Bearing.y) * text->fontSize;

    float w = ch.Size.x * text->fontSize;
    float h = ch.Size.y * text->fontSize;

    xpos /= windowWidth;
    ypos /= windowHeight;
    w /= windowWidth;
    h /= windowHeight;

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
    float nextCharWidth = 0.0;

    if (text->content[c + 1] != '\0') {
      FontCharacter nextChar = ouiContext->fontCharacters[(int)text->content[c + 1]];
      nextCharWidth = (nextChar.Advance >> 6) * text->fontSize;
    }

    if (text->maxLineWidth > 0 && x + nextCharWidth > text->maxLineWidth) {
      x = text->startPos.x;
      y -= text->lineHeight;
    }
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

//----------------------------------------
// Text
//----------------------------------------

//----------------------------------------
// Utils
//----------------------------------------

void oui__compute_unit_cercle(void) {
  OuiVertex vertices[OUI_UNIT_CERCLE_SEGMENT_COUNT + 1] = {0};

  float r = 1.0;
  float theta = 0.0, thetaStep = 2 * OUI_PI / OUI_UNIT_CERCLE_SEGMENT_COUNT;

  for (int i = 1; i <= OUI_UNIT_CERCLE_SEGMENT_COUNT; i++, theta += thetaStep) {
    vertices[i].position.x = r * cos(theta);
    vertices[i].position.y = r * sin(theta);

    vertices[i].color.r = oui_max(0.0, cos(theta));
    vertices[i].color.g = oui_max(0.0, sin(theta));
    vertices[i].color.a = 1.0;
  }

  for (int i = 0; i < 3 * OUI_UNIT_CERCLE_SEGMENT_COUNT; i += 3) {
    OUI_UNIT_CERCLE[i] = vertices[0];

    int currVertexIdx = i / 3 + 1;
    OUI_UNIT_CERCLE[i + 1] = vertices[currVertexIdx];
    OUI_UNIT_CERCLE[i + 2] = vertices[currVertexIdx + 1];

    if (currVertexIdx == OUI_UNIT_CERCLE_SEGMENT_COUNT)
      OUI_UNIT_CERCLE[i + 2] = vertices[1];
  }
}

static char *oui__glfw_rectangle_get_fragment_shader(OuiRectangle *rectangle) {
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

static char *oui__glfw_rectangle_get_vertex_shader(OuiRectangle *rectangle) {
  char *vertexShaderSource =
      "#version 330 core\n"

      "layout (location = 0) in vec3 aPos;\n"
      "layout (location = 1) in vec4 aColorUNUSED;\n"

      "layout (location = 2) in vec3 aWidthHeightRotation;\n"
      "layout (location = 3) in vec2 aCenterPos;\n"
      "layout (location = 4) in vec2 aBorderUNUSED;\n"
      "layout (location = 5) in vec4 aBgColor;\n"

      "out vec4 vColor;\n"
      "uniform vec2 uWindowSize;\n"

      "void main()\n"
      "{\n"
      "   vColor = aBgColor / 256.0;\n"

      "   vec2 p = aPos.xy;\n"
      "   float w = aWidthHeightRotation.x;\n"
      "   float h = aWidthHeightRotation.y;\n"
      "   float r = aWidthHeightRotation.z;\n"

      "   // scale\n"
      "   p *= vec2(w, h);\n"

      "   // rotate\n"
      "   vec2 tmp = p;\n"
      "   p.x = tmp.x * cos(r) - tmp.y * sin(r);\n"
      "   p.y = tmp.x * sin(r) + tmp.y * cos(r);\n"

      "   // translate\n"
      "   p += aCenterPos;\n"

      "   // transform to device coordinates\n"
      "   p /= uWindowSize / 2.0;\n"

      "   gl_Position = vec4(p, 0.0, 1.0);\n"
      "}\0";

  return vertexShaderSource;
}

static char *oui__glfw_cercle_get_vertex_shader(void) {
  char *vertexShaderSource =
      "#version 330 core\n"

      "layout (location = 0) in vec3 aPos;\n"
      "layout (location = 1) in vec4 aColorUNUSED;\n"

      "layout (location = 2) in vec3 aWidthHeightRotationUNUSED;\n"
      "layout (location = 3) in vec2 aCenterPos;\n"
      "layout (location = 4) in vec2 aBorder;\n"
      "layout (location = 5) in vec4 aBgColor;\n"

      "out vec4 vColor;\n"
      "uniform vec2 uWindowSize;\n"

      "void main()\n"
      "{\n"
      "   vColor = aBgColor / 256.0;\n"

      "   vec2 p = aCenterPos + aPos.xy * aBorder.x;\n"
      "   p /= uWindowSize / 2.0;\n"

      "   gl_Position = vec4(p, 0.0, 1.0);\n"
      "}\0";

  return vertexShaderSource;
}

static char *oui__glfw_text_get_vertex_shader(OuiText *text) {
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

static char *oui__glfw_text_get_fragment_shader(OuiText *text) {
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

static void oui__glfw_flush_shader_compilation_errors(unsigned int shader, int is_shader_program) {
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

    printf(
        "ERROR::SHADER::VERTEX::COMPILATION_FAILED: shader_id: %d, infoLog: %s\n",
        shader, infoLog);
  }
}

static unsigned int oui__glfw_compile_shader_program(
    const char *vertexShaderSource,
    const char *fragmentShaderSource) {
  assert(
      vertexShaderSource != NULL &&
      fragmentShaderSource != NULL &&
      "Vertex and fragment shaders are required");

  unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader(vertexShader);
  oui__glfw_flush_shader_compilation_errors(vertexShader, 0);

  unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);
  oui__glfw_flush_shader_compilation_errors(fragmentShader, 0);

  unsigned int shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  oui__glfw_flush_shader_compilation_errors(shaderProgram, 1);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return shaderProgram;
}

static unsigned int oui__glfw_rectangle_compile_shader_program(OuiRectangle *rectangle) {
  const char *vertexShaderSource = oui__glfw_rectangle_get_vertex_shader(rectangle);
  const char *fragmentShaderSource = oui__glfw_rectangle_get_fragment_shader(rectangle);

  return oui__glfw_compile_shader_program(vertexShaderSource, fragmentShaderSource);
}

static unsigned int oui__glfw_cercle_compile_shader_program(void) {
  const char *vertexShaderSource = oui__glfw_cercle_get_vertex_shader();
  const char *fragmentShaderSource = oui__glfw_rectangle_get_fragment_shader(NULL);

  return oui__glfw_compile_shader_program(vertexShaderSource, fragmentShaderSource);
}

static unsigned int oui__glfw_text_compile_shader_program(OuiText *text) {
  const char *vertexShaderSource = oui__glfw_text_get_vertex_shader(text);
  const char *fragmentShaderSource = oui__glfw_text_get_fragment_shader(text);

  return oui__glfw_compile_shader_program(vertexShaderSource, fragmentShaderSource);
}

//----------------------------------------
// Utils
//----------------------------------------

#endif // OUI_USE_GLFW
