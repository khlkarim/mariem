#if !defined(OUI_HEADER)
#define OUI_HEADER

#define OUI_DEFAULT_WINDOW_WIDTH 800
#define OUI_DEFAULT_WINDOW_HEIGHT 600
#define OUI_DEFAULT_BASE_FONT_SIZE 48.0
#define OUI_DEFAULT_FONT "fonts/arial.ttf"
#define OUI_DEFAULT_WINDOW_TITLE "Default window title"

#define OUI_COLOR_BLACK ((OuiColor){0.0, 0.0, 0.0, 255.0})
#define OUI_COLOR_RED ((OuiColor){255.0, 0.0, 0.0, 255.0})
#define OUI_COLOR_GREEN ((OuiColor){0.0, 255.0, 0.0, 255.0})
#define OUI_COLOR_BLUE ((OuiColor){0.0, 0.0, 255.0, 255.0})
#define OUI_COLOR_WHITE ((OuiColor){255.0, 255.0, 255.0, 255.0})

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

typedef struct OuiText {
  OuiVec2f startPos;

  float fontSize;
  OuiColor fontColor;

  float lineHeight;
  float maxLineWidth;

  char *content;
} OuiText;

typedef struct OuiRectangle {
  float width;
  float height;
  float rotationXY;
  OuiVec2f centerPos;
  float borderRadius;

  float borderResolution;
  // this attribute is no longer used in the current implementation
  // but dont remove it, unless you wanna spend an hour debugging a blackscreen
  //  TODO: deprecate this

  OuiColor backgroundColor;
} OuiRectangle;

typedef struct OuiContext OuiContext;

typedef struct OuiConfig {
  char *font;
  // path to a .ttf file

  float baseFontSize;
  // this is the base pixel height the font bitmaps get loaded as
  // setting the fontSize attribute of the OuiText struct
  // sets the height of the letters to: baseFontSize * text.fontSize

  int windowWidth;
  int windowHeight;
  char *windowTitle;
  OuiColor backgroundColor;
} OuiConfig;

void oui_context_init(OuiContext *ouiContext, OuiConfig *ouiConfig);
void oui_context_destroy(OuiContext *ouiContext);

int oui_has_next_frame(OuiContext *ouiContext);
void oui_begin_frame(OuiContext *ouiContext);
void oui_end_frame(OuiContext *ouiContext);

void oui_draw_text(OuiContext *ouiContext, OuiText *text);
void oui_draw_rectangle(OuiContext *ouiContext, OuiRectangle *rectangle);

void oui_text_to_rectangle(OuiContext *ouiContext, OuiText *text, OuiRectangle *rectangle);
// Sets the width, height and centerPos attributes of rectangle in such a way that
// it would span the space occupied by the text when its drawn, and can serve as a background for it

void oui_rectangle_to_text(OuiContext *ouiContext, OuiRectangle *rectangle, OuiText *text);
// Sets the startPos and maxLineWidth attributes of text in such a way that
// it would be displayed inside the rectangle when its drawn

int oui_get_window_width(OuiContext *ouiContext);
int oui_get_window_height(OuiContext *ouiContext);

#endif // OUI_HEADER

#if defined(OUI_USE_GLFW) && !defined(OUI_USE_)
#define OUI_USE_

#include "stb_truetype.h"

#define MAX_CHARACTERS 128
typedef struct GLFWwindow GLFWwindow;

typedef enum OuiStatus {
  OUI_STATUS_FAILURE,
  OUI_STATUS_SUCCESS,
} OuiStatus;

struct OuiContext {
  OuiStatus initStatus;

  GLFWwindow *window;
  OuiColor backgroundColor;

  unsigned int textVAO, textVBO;
  unsigned int textShaderProgram;

  unsigned int cercleInstanceVBO;
  unsigned int cercleShaderProgram;
  unsigned int cercleVAO, cercleVBO;

  unsigned int rectangleInstanceVBO;
  unsigned int rectangleShaderProgram;
  unsigned int rectangleVAO, rectangleVBO;

  float baseFontSize;
  int characterCount;
  int firstCharacterCodePoint;

  unsigned int fontTexture;
  stbtt_aligned_quad alignedQuads[MAX_CHARACTERS];
  stbtt_packedchar packedCharacters[MAX_CHARACTERS];
};

#endif // OUI_USE_GLFW

#if defined(OUI_USE_GLFW) && defined(OUI_IMPLEMENTATION) && !defined(OUI_IMPLEMENTED)
#define OUI_IMPLEMENTED

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

//----------------------------------------
// Utils
//----------------------------------------

#define OUI_PI 3.14159265359
#define OUI_UNUSED(value) (void)(value)

float oui_min(float a, float b) { return a <= b ? a : b; }
float oui_max(float a, float b) { return a >= b ? a : b; }
float oui_clamp(float x, float min, float max) { return oui_max(min, oui_min(max, x)); }
void oui_framebuffer_size_callback(GLFWwindow *window, int width, int height);

#define OUI_UNIT_RECTANGLE_VERTEX_COUNT 4
#define OUI_UNIT_CERCLE_SEGMENT_COUNT 50
#define OUI_UNIT_CERCLE_INDEX_COUNT 3 * OUI_UNIT_CERCLE_SEGMENT_COUNT
#define OUI_UNIT_CERCLE_VERTEX_COUNT OUI_UNIT_CERCLE_SEGMENT_COUNT + 1

static const OuiVertex OUI_UNIT_RECTANGLE_VERTICES[] = {
    {{-0.5, 0.5, 0.0}, {0, 0, 0, 0}},
    {{0.5, -0.5, 0.0}, {0, 0, 0, 0}},
    {{-0.5, -0.5, 0.0}, {0, 0, 0, 0}},
    {{0.5, 0.5, 0.0}, {0, 0, 0, 0}},
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

void oui_framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
  OUI_UNUSED(window);
}

void oui_context_init(OuiContext *ouiContext, OuiConfig *ouiConfig) {
  if (ouiContext == NULL) {
    return;
  }

  if (!glfwInit()) {
    ouiContext->initStatus = OUI_STATUS_FAILURE;
    printf("Failed to initialize GLFW\n");
    return;
  }

  ouiContext->initStatus = OUI_STATUS_SUCCESS;
  oui__glfw_create_window(ouiContext, ouiConfig);
  oui__freetype_load_font(ouiContext, ouiConfig);
  oui__initialize_gpu_objects(ouiContext, ouiConfig);
  ouiContext->backgroundColor = ouiConfig->backgroundColor;
}

static void oui__glfw_create_window(OuiContext *ouiContext, OuiConfig *ouiConfig) {
  if (ouiContext == NULL) {
    return;
  }

  int windowWidth = ouiConfig == NULL ? OUI_DEFAULT_WINDOW_WIDTH : ouiConfig->windowWidth;
  int windowHeight = ouiConfig == NULL ? OUI_DEFAULT_WINDOW_HEIGHT : ouiConfig->windowHeight;
  char *windowTitle = ouiConfig == NULL ? OUI_DEFAULT_WINDOW_TITLE : ouiConfig->windowTitle;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_SAMPLES, 4); // anti-aliasing
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *window = glfwCreateWindow(
      windowWidth,
      windowHeight,
      windowTitle,
      NULL,
      NULL);

  if (!window) {
    ouiContext->initStatus = OUI_STATUS_FAILURE;
    printf("Failed to create GLFW window\n");
    glfwTerminate();
    return;
  }

  ouiContext->window = window;
  glfwMakeContextCurrent(ouiContext->window);
  glfwSetFramebufferSizeCallback(ouiContext->window, oui_framebuffer_size_callback);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
}

static void oui__freetype_load_font(OuiContext *ouiContext, OuiConfig *ouiConfig) {
  if (
      ouiContext == NULL ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
    return;
  }

  ouiContext->characterCount = 95;
  ouiContext->firstCharacterCodePoint = 32;
  ouiContext->baseFontSize = ouiConfig == NULL || ouiConfig->baseFontSize == 0 ? OUI_DEFAULT_BASE_FONT_SIZE : ouiConfig->baseFontSize;
  char *font = ouiConfig == NULL || ouiConfig->font == NULL ? OUI_DEFAULT_FONT : ouiConfig->font;

  float bitmapWidth = 512;
  float bitmapHeight = 512;
  size_t bufferSize = 1 << 20;
  size_t bitmapSize = 512 * 512;
  float pixelHeight = ouiContext->baseFontSize;

  unsigned char tempBitmap[bitmapSize];
  unsigned char rawFileBytes[bufferSize];

  FILE *fontFile = fopen(font, "rb");
  fread(rawFileBytes, 1, bufferSize, fontFile);

  stbtt_fontinfo fontInfo = {0};
  if (!stbtt_InitFont(&fontInfo, rawFileBytes, 0)) {
    printf("stbtt_InitFont() Failed!\n");
  }

  stbtt_pack_context ctx;

  stbtt_PackBegin(
      &ctx,
      tempBitmap,
      bitmapWidth,
      bitmapHeight,
      0,
      1, NULL);

  stbtt_PackFontRange(
      &ctx,
      rawFileBytes,
      0,
      pixelHeight,
      32,
      95,
      ouiContext->packedCharacters);

  stbtt_PackEnd(&ctx);

  for (int i = 0; i < 96; i++) {
    float unusedX, unusedY;

    stbtt_GetPackedQuad(
        ouiContext->packedCharacters,
        bitmapWidth,
        bitmapHeight,
        i,
        &unusedX, &unusedY,
        &ouiContext->alignedQuads[i],
        1);
  }

  glGenTextures(1, &(ouiContext->fontTexture));
  glBindTexture(GL_TEXTURE_2D, ouiContext->fontTexture);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, bitmapWidth,
               bitmapHeight, 0, GL_RED, GL_UNSIGNED_BYTE, tempBitmap);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void oui__initialize_gpu_objects(OuiContext *ouiContext, OuiConfig *ouiConfig) {
  if (
      ouiContext == NULL ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
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
  glBufferData(GL_ARRAY_BUFFER, sizeof(OuiRectangle), NULL, GL_DYNAMIC_DRAW);
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
  glBufferData(GL_ARRAY_BUFFER, sizeof(OuiRectangle), NULL, GL_DYNAMIC_DRAW);
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

  OUI_UNUSED(ouiConfig);
}

void oui_context_destroy(OuiContext *ouiContext) {
  if (
      ouiContext == NULL ||
      ouiContext->window == NULL ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
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
  if (
      ouiContext == NULL ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
    return 0;
  }

  return !glfwWindowShouldClose(ouiContext->window);
}

void oui_begin_frame(OuiContext *ouiContext) {
  if (
      ouiContext == NULL ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
    return;
  }

  float r = oui_clamp(ouiContext->backgroundColor.r, 0, 255) / 255.0;
  float g = oui_clamp(ouiContext->backgroundColor.g, 0, 255) / 255.0;
  float b = oui_clamp(ouiContext->backgroundColor.b, 0, 255) / 255.0;

  glClearColor(r, g, b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void oui_end_frame(OuiContext *ouiContext) {
  if (
      ouiContext == NULL ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
    return;
  }

  glfwSwapBuffers(ouiContext->window);
  glfwPollEvents();
}

int oui_get_window_width(OuiContext *ouiContext) {
  if (
      ouiContext == NULL ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
    return 0;
  }

  int screenWidth = 0, screenHeight = 0;
  glfwGetFramebufferSize(ouiContext->window, &screenWidth, &screenHeight);
  return screenWidth;
}

int oui_get_window_height(OuiContext *ouiContext) {
  if (
      ouiContext == NULL ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
    return 0;
  }

  int screenWidth = 0, screenHeight = 0;
  glfwGetFramebufferSize(ouiContext->window, &screenWidth, &screenHeight);
  return screenHeight;
}

//----------------------------------------
// OuiContext
//----------------------------------------

//----------------------------------------
// Rectangle
//----------------------------------------

void oui_draw_rectangle(OuiContext *ouiContext, OuiRectangle *rectangle) {
  if (
      rectangle == NULL ||
      ouiContext == NULL ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
    return;
  }

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  unsigned int
      rectangleShader = ouiContext->rectangleShaderProgram,
      rectangleVAO = ouiContext->rectangleVAO;

  unsigned int
      cercleShader = ouiContext->cercleShaderProgram,
      cercleVAO = ouiContext->cercleVAO;

  int isRounded = rectangle->borderRadius > 0;
  float maxBorderRadius = oui_min(rectangle->width / 2.0, rectangle->height / 2.0);
  rectangle->borderRadius = oui_clamp(rectangle->borderRadius, 0.0, maxBorderRadius);

  float r, g, b, a;
  r = rectangle->backgroundColor.r;
  g = rectangle->backgroundColor.g;
  b = rectangle->backgroundColor.b;
  a = rectangle->backgroundColor.a;

  rectangle->backgroundColor.r = oui_clamp(rectangle->backgroundColor.r, 0.0, 255.0) / 255.0;
  rectangle->backgroundColor.g = oui_clamp(rectangle->backgroundColor.g, 0.0, 255.0) / 255.0;
  rectangle->backgroundColor.b = oui_clamp(rectangle->backgroundColor.b, 0.0, 255.0) / 255.0;
  rectangle->backgroundColor.a = oui_clamp(rectangle->backgroundColor.a, 0.0, 255.0) / 255.0;

  if (!isRounded) {
    glUseProgram(rectangleShader);

    glUniform2f(
        glGetUniformLocation(rectangleShader, "uWindowSize"),
        windowWidth, windowHeight);

    glBindVertexArray(rectangleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ouiContext->rectangleInstanceVBO);

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0, sizeof(OuiRectangle),
        rectangle);

    glDrawArrays(
        GL_TRIANGLES,
        0, 6);

    glBindVertexArray(0);

    rectangle->backgroundColor.r = r;
    rectangle->backgroundColor.g = g;
    rectangle->backgroundColor.b = b;
    rectangle->backgroundColor.a = a;
    return;
  }

  if (
      isRounded &&
      rectangle->width == rectangle->height &&
      rectangle->borderRadius == maxBorderRadius) {

    glUseProgram(cercleShader);
    glUniform2f(
        glGetUniformLocation(cercleShader, "uWindowSize"),
        windowWidth, windowHeight);

    glBindVertexArray(cercleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ouiContext->cercleInstanceVBO);

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0, sizeof(OuiRectangle),
        rectangle);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        OUI_UNIT_CERCLE_INDEX_COUNT);

    glBindVertexArray(0);

    rectangle->backgroundColor.r = r;
    rectangle->backgroundColor.g = g;
    rectangle->backgroundColor.b = b;
    rectangle->backgroundColor.a = a;
    return;
  }

  OuiRectangle rect1 = *rectangle;
  rect1.height -= 2 * rectangle->borderRadius;
  OuiRectangle rect2 = *rectangle;
  rect2.width -= 2 * rectangle->borderRadius;

  glUseProgram(rectangleShader);
  glUniform2f(
      glGetUniformLocation(rectangleShader, "uWindowSize"),
      windowWidth, windowHeight);

  glBindVertexArray(rectangleVAO);
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->rectangleInstanceVBO);

  glBufferSubData(
      GL_ARRAY_BUFFER,
      0, sizeof(OuiRectangle),
      &rect1);

  glDrawArrays(
      GL_TRIANGLES,
      0, 6);

  glBufferSubData(
      GL_ARRAY_BUFFER,
      0, sizeof(OuiRectangle),
      &rect2);

  glDrawArrays(
      GL_TRIANGLES,
      0, 6);

  glBindVertexArray(0);

  glUseProgram(cercleShader);
  glUniform2f(
      glGetUniformLocation(cercleShader, "uWindowSize"),
      windowWidth, windowHeight);

  glBindVertexArray(cercleVAO);
  glBindBuffer(GL_ARRAY_BUFFER, ouiContext->cercleInstanceVBO);
  for (int i = 0; i < OUI_UNIT_RECTANGLE_VERTEX_COUNT; i++) {
    OuiRectangle cercle = {0};
    cercle = *rectangle;

    float x = (rectangle->width - 2 * rectangle->borderRadius) * OUI_UNIT_RECTANGLE_VERTICES[i].position.x;
    float y = (rectangle->height - 2 * rectangle->borderRadius) * OUI_UNIT_RECTANGLE_VERTICES[i].position.y;

    cercle.centerPos.x = x * cos(rectangle->rotationXY) - y * sin(rectangle->rotationXY) + rectangle->centerPos.x;
    cercle.centerPos.y = x * sin(rectangle->rotationXY) + y * cos(rectangle->rotationXY) + rectangle->centerPos.y;

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0, sizeof(OuiRectangle),
        &cercle);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        OUI_UNIT_CERCLE_INDEX_COUNT);
  }
  glBindVertexArray(0);

  rectangle->backgroundColor.r = r;
  rectangle->backgroundColor.g = g;
  rectangle->backgroundColor.b = b;
  rectangle->backgroundColor.a = a;
}

//----------------------------------------
// Rectangle
//----------------------------------------

//----------------------------------------
// Text
//----------------------------------------

void oui_draw_text(OuiContext *ouiContext, OuiText *text) {
  if (
      text == NULL ||
      ouiContext == NULL ||
      text->content == NULL ||
      ouiContext->characterCount == 0 ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
    return;
  }

  int windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  if (windowWidth == 0 || windowHeight == 0) {
    return;
  }

  unsigned int shader = ouiContext->textShaderProgram;

  unsigned int
      VAO = ouiContext->textVAO,
      VBO = ouiContext->textVBO;

  float r = oui_clamp(text->fontColor.r, 0.0, 255.0) / 255.0;
  float g = oui_clamp(text->fontColor.g, 0.0, 255.0) / 255.0;
  float b = oui_clamp(text->fontColor.b, 0.0, 255.0) / 255.0;

  glUseProgram(shader);
  glUniform3f(
      glGetUniformLocation(shader, "textColor"),
      r, g, b);

  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(VAO);

  glBindTexture(GL_TEXTURE_2D, ouiContext->fontTexture);

  float vertices[6][4];
  int order[6] = {0, 1, 2, 0, 2, 3};
  float x = text->startPos.x, y = text->startPos.y;

  for (int i = 0; text->content[i] != '\0'; i++) {
    unsigned char c = text->content[i];

    if (c == '\n') {
      y -= text->lineHeight;
      x = text->startPos.x;
      continue;
    }

    if (
        c < ouiContext->firstCharacterCodePoint ||
        c > ouiContext->firstCharacterCodePoint + ouiContext->characterCount - 1) {
      continue;
    }

    stbtt_packedchar *packedChar = &ouiContext->packedCharacters[c - ouiContext->firstCharacterCodePoint];
    stbtt_aligned_quad *alignedQuad = &ouiContext->alignedQuads[c - ouiContext->firstCharacterCodePoint];

    OuiVec2f glyphSize = {
        (packedChar->x1 - packedChar->x0) * text->fontSize,
        (packedChar->y1 - packedChar->y0) * text->fontSize,
    };

    OuiVec2f glyphBoundingBoxBottomLeft = {
        x + (packedChar->xoff * text->fontSize),
        y - (packedChar->yoff + packedChar->y1 - packedChar->y0) * text->fontSize,
    };

    OuiVec2f glyphVertices[4] = {
        {glyphBoundingBoxBottomLeft.x + glyphSize.x, glyphBoundingBoxBottomLeft.y + glyphSize.y},
        {glyphBoundingBoxBottomLeft.x, glyphBoundingBoxBottomLeft.y + glyphSize.y},
        {glyphBoundingBoxBottomLeft.x, glyphBoundingBoxBottomLeft.y},
        {glyphBoundingBoxBottomLeft.x + glyphSize.x, glyphBoundingBoxBottomLeft.y},
    };

    OuiVec2f glyphTextureCoords[4] = {
        {alignedQuad->s1, alignedQuad->t0},
        {alignedQuad->s0, alignedQuad->t0},
        {alignedQuad->s0, alignedQuad->t1},
        {alignedQuad->s1, alignedQuad->t1},
    };

    for (int j = 0; j < 6; j++) {
      vertices[j][0] = glyphVertices[order[j]].x / (windowWidth / 2.0);
      vertices[j][1] = glyphVertices[order[j]].y / (windowHeight / 2.0);
      vertices[j][2] = glyphTextureCoords[order[j]].x;
      vertices[j][3] = glyphTextureCoords[order[j]].y;
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    x += packedChar->xadvance * text->fontSize;

    unsigned char nextC = text->content[i + 1];
    if (nextC != '\0') {
      stbtt_packedchar *nextPackedChar = &ouiContext->packedCharacters[nextC - ouiContext->firstCharacterCodePoint];
      float nextX = x + nextPackedChar->xadvance * text->fontSize;

      if (
          text->maxLineWidth > 0 &&
          nextX - text->startPos.x > text->maxLineWidth) {
        y -= text->lineHeight;
        x = text->startPos.x;
      }
    }
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

//----------------------------------------
// Text
//----------------------------------------

//----------------------------------------
// Rectangle <=> Text
//----------------------------------------

void oui_text_to_rectangle(OuiContext *ouiContext, OuiText *text, OuiRectangle *rectangle) {
  if (
      ouiContext == NULL ||
      ouiContext->characterCount == 0 ||
      ouiContext->initStatus == OUI_STATUS_FAILURE ||
      text == NULL || text->content == NULL || rectangle == NULL) {
    return;
  }

  rectangle->width = 0;
  rectangle->height = 0;
  rectangle->centerPos.x = 0;
  rectangle->centerPos.y = 0;

  OuiVec2f topLeft = {0};
  OuiVec2f bottomRight = {0};

  float x = text->startPos.x, y = text->startPos.y;
  for (int i = 0; text->content[i] != '\0'; i++) {
    unsigned char c = text->content[i];

    if (c == '\n') {
      y -= text->lineHeight;
      x = text->startPos.x;
      continue;
    }

    if (
        c < ouiContext->firstCharacterCodePoint ||
        c > ouiContext->firstCharacterCodePoint + ouiContext->characterCount - 1) {
      continue;
    }

    stbtt_packedchar *packedChar = &ouiContext->packedCharacters[c - ouiContext->firstCharacterCodePoint];

    OuiVec2f glyphSize = {
        (packedChar->x1 - packedChar->x0) * text->fontSize,
        (packedChar->y1 - packedChar->y0) * text->fontSize,
    };

    OuiVec2f glyphBoundingBoxBottomLeft = {
        x + (packedChar->xoff * text->fontSize),
        y - (packedChar->yoff + packedChar->y1 - packedChar->y0) * text->fontSize,
    };

    OuiVec2f glyphVertices[4] = {
        {glyphBoundingBoxBottomLeft.x + glyphSize.x, glyphBoundingBoxBottomLeft.y + glyphSize.y},
        {glyphBoundingBoxBottomLeft.x, glyphBoundingBoxBottomLeft.y + glyphSize.y},
        {glyphBoundingBoxBottomLeft.x, glyphBoundingBoxBottomLeft.y},
        {glyphBoundingBoxBottomLeft.x + glyphSize.x, glyphBoundingBoxBottomLeft.y},
    };

    if (i == 0) {
      topLeft.x = glyphVertices[1].x;
      topLeft.y = glyphVertices[1].y;

      bottomRight.x = glyphVertices[3].x;
      bottomRight.y = glyphVertices[3].y;
    }

    x += packedChar->xadvance * text->fontSize;
    topLeft.y = oui_max(topLeft.y, glyphVertices[1].y);
    bottomRight.x = oui_max(bottomRight.x, glyphVertices[3].x);
    bottomRight.y = oui_min(bottomRight.y, glyphVertices[3].y);

    unsigned char nextC = text->content[i + 1];
    if (nextC != '\0') {
      stbtt_packedchar *nextPackedChar = &ouiContext->packedCharacters[nextC - ouiContext->firstCharacterCodePoint];
      float nextX = x + nextPackedChar->xadvance * text->fontSize;

      if (
          text->maxLineWidth > 0 &&
          nextX - text->startPos.x > text->maxLineWidth) {
        y -= text->lineHeight;
        x = text->startPos.x;
      }
    }
  }

  rectangle->width = bottomRight.x - topLeft.x;
  rectangle->height = topLeft.y - bottomRight.y;
  rectangle->centerPos.x = topLeft.x + rectangle->width / 2.0;
  rectangle->centerPos.y = topLeft.y - rectangle->height / 2.0;
}

void oui_rectangle_to_text(OuiContext *ouiContext, OuiRectangle *rectangle, OuiText *text) {
  if (
      ouiContext == NULL ||
      text == NULL || rectangle == NULL ||
      ouiContext->initStatus == OUI_STATUS_FAILURE) {
    return;
  }

  text->maxLineWidth = rectangle->width;
  text->startPos.x = rectangle->centerPos.x - rectangle->width / 2.0;
  text->startPos.y = rectangle->centerPos.y + rectangle->height / 2.0;
}

//----------------------------------------
// Rectangle <=> Text
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

  OUI_UNUSED(rectangle);
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
      "   vColor = aBgColor;\n"

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

  OUI_UNUSED(rectangle);
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
      "   vColor = aBgColor;\n"

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

  OUI_UNUSED(text);
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
      "   // color = vec4(textColor, 1.0);\n"
      "}\n\0";

  OUI_UNUSED(text);
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
