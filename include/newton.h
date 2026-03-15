#if !defined(NEWTON_HEADER)
#define NEWTON_HEADER

#define NT_PI 3.14159265359
#define NT_TAU 6.28318530718
#define NT_GRAVITY 9.8

typedef struct NtVec2f {
  float x;
  float y;
} NtVec2f;

#define NT_ZERO ((NtVec2f){.x = 0.0, .y = 0.0})
#define NT_ONE ((NtVec2f){.x = 1.0, .y = 1.0})
#define NT_X ((NtVec2f){.x = 1.0, .y = 0.0})
#define NT_Y ((NtVec2f){.x = 0.0, .y = 1.0})

float nwt_length(NtVec2f v);
NtVec2f nwt_normalize(NtVec2f v);

NtVec2f nwt_add(const NtVec2f a, const NtVec2f b);
NtVec2f nwt_sub(const NtVec2f a, const NtVec2f b);
NtVec2f nwt_mult(const NtVec2f a, const NtVec2f b);
NtVec2f nwt_div(const NtVec2f a, const NtVec2f b);

NtVec2f nwt_add_s(const NtVec2f a, const float s);
NtVec2f nwt_sub_s(const NtVec2f a, const float s);
NtVec2f nwt_mult_s(const NtVec2f a, const float s);
NtVec2f nwt_div_s(const NtVec2f a, const float s);

float nwt_dot(const NtVec2f a, const NtVec2f b);
float nwt_distance(const NtVec2f a, const NtVec2f b);

#endif

#if defined(NEWTON_IMPLEMENTATION) && !defined(NEWTON_IMPLEMENTED)
#define NEWTON_IMPLEMENTED

#include <assert.h>
#include <math.h>

float nwt_length(NtVec2f v) {
  return sqrt(v.x * v.x + v.y * v.y);
}

NtVec2f nwt_normalize(NtVec2f v) {
  return nwt_div_s(v, nwt_length(v));
}

NtVec2f nwt_add(const NtVec2f a, const NtVec2f b) {
  NtVec2f res = {
      .x = a.x + b.x,
      .y = a.y + b.y,
  };

  return res;
}

NtVec2f nwt_sub(const NtVec2f a, const NtVec2f b) {
  NtVec2f res = {
      .x = a.x - b.x,
      .y = a.y - b.y,
  };

  return res;
}

NtVec2f nwt_mult(const NtVec2f a, const NtVec2f b) {
  NtVec2f res = {
      .x = a.x * b.x,
      .y = a.y * b.y,
  };

  return res;
}

NtVec2f nwt_div(const NtVec2f a, const NtVec2f b) {
  assert(b.x != 0 && b.y != 0 && "Division by zero");

  NtVec2f res = {
      .x = a.x / b.x,
      .y = a.y / b.y,
  };

  return res;
}

NtVec2f nwt_add_s(const NtVec2f a, const float s) {
  NtVec2f res = {
      .x = a.x + s,
      .y = a.y + s,
  };

  return res;
}

NtVec2f nwt_sub_s(const NtVec2f a, const float s) {
  NtVec2f res = {
      .x = a.x - s,
      .y = a.y - s,
  };

  return res;
}

NtVec2f nwt_mult_s(const NtVec2f a, const float s) {
  NtVec2f res = {
      .x = a.x * s,
      .y = a.y * s,
  };

  return res;
}

NtVec2f nwt_div_s(const NtVec2f a, const float s) {
  assert(s != 0 && "Division by zero");

  NtVec2f res = {
      .x = a.x / s,
      .y = a.y / s,
  };

  return res;
}

float nwt_dot(const NtVec2f a, const NtVec2f b) {
  return a.x * b.x + a.y * b.y;
}

float nwt_distance(const NtVec2f a, const NtVec2f b) {
  return nwt_length(nwt_sub(a, b));
}

#endif
