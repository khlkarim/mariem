#include "tinyfiledialogs.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define NEWTON_IMPLEMENTATION
#include "newton.h"

#define OUI_IMPLEMENTATION
#define OUI_USE_GLFW
#include "oui.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Oui context config
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WINDOW_TITLE "UNTITLED"
#define FONT "fonts/arial.ttf"

// Miniaudio device config
#define DEVICE_CHANNELS 2
#define DEVICE_SAMPLE_RATE 48000
#define DEVICE_FORMAT ma_format_f32

// -------------------- StyleSheet --------------------
#define DEFAULT_FONT_SIZE 1
#define DEFAULT_MENU_MARGIN 20
#define DEFAULT_LINE_HEIGHT 48
#define DEFAULT_LINK_THICKNESS 8
#define DEFAULT_BORDER_RESOLUTION 50
#define DEFAULT_TEXT_COLOR OUI_COLOR_WHITE
/*#define DEFAULT_NODE_COLOR OUI_COLOR_BLACK
#define DEFAULT_LINK_COLOR OUI_COLOR_BLACK
#define SELECTED_NODE_COLOR OUI_COLOR_WHITE*/

#define ACSENT_COLOR
#define DEFAULT_NODE_COLOR OUI_COLOR_WHITE
#define DEFAULT_LINK_COLOR (OuiColor){150, 150, 150, 200}
// #define SELECTED_NODE_COLOR (OuiColor){27, 161, 226, 255}
#define SELECTED_NODE_COLOR (OuiColor){13 + 6 * 16, 2 + 11 * 16, 256, 255} // #6db2ff
// -------------------- StyleSheet --------------------

// ---------------- Physics Parameters ----------------
#define DEFAULT_RESTITUTION 1
#define DEFAULT_NODE_RADIUS 35
#define DEFAULT_LINK_ELASTICITY 1.0
#define DEFAULT_LINK_RESTLENGTH 200
#define CONSTRAINT_SOLVER_ITERATIONS 2
#define DEFAULT_NODE_MASSE 0.1 // this value is used to calculate the inv masse too
// ---------------- Physics Parameters ----------------

// ----------------------- Misc -----------------------
#define MAX_ENTITIES 2050
#define NUM_NODES 5
#define DEFAULT_BPM 120
#define BUFFER_SIZE 128
// ----------------------- Misc -----------------------

// ------------------ Input Handling ------------------
#define ON 1
#define OFF 0

#define RELEASED GLFW_RELEASE
#define PRESSED GLFW_PRESS
#define HELD GLFW_REPEAT

#define KEYBINDING_START_PLAYING GLFW_KEY_X
#define KEYBINDING_UNSELECT_BRUSH GLFW_KEY_ESCAPE
#define KEYBINDING_ASSERT_MODE GLFW_KEY_Q
#define KEYBINDING_PERFORM_MODE GLFW_KEY_P
#define KEYBINDING_INITIALIZE_MODE GLFW_KEY_I
#define KEYBINDING_CREATE_NODE GLFW_KEY_N
#define KEYBINDING_DELETE_SELECTED GLFW_KEY_D
#define KEYBINDING_DISABLE_PHYSICS GLFW_KEY_SPACE
#define KEYBINDING_OPEN_RESOURCE_MANAGER GLFW_KEY_S

#define NUM_KEYBOARD_KEYS (GLFW_KEY_LAST + 1)
static int keyboard_key_states[NUM_KEYBOARD_KEYS] = {0};

#define NUM_MOUSE_BUTTONS (GLFW_MOUSE_BUTTON_LAST + 1)
static int mouse_button_states[NUM_MOUSE_BUTTONS] = {0};

NtVec2f get_mouse_position(OuiContext *ouiContext);
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
void keyboard_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
// ------------------ Input Handling ------------------

// ---------------- Entity Management -----------------
#define NO 0
#define YES 1

#define NIL 0
typedef int EntityId;

typedef enum EntityType {
  ENTITY_TYPE_NONE,
  ENTITY_TYPE_NODE,
  ENTITY_TYPE_LINK,
  ENTITY_TYPE_RESOURCE,
  ENTITY_TYPE_COUNT
} EntityType;

typedef enum LinkState {
  LINK_STATE_NONE,
  LINK_STATE_ENABLED,
  LINK_STATE_DISABLED,
  LINK_STATE_COUNT
} LinkState;

OuiColor link_state_colors[LINK_STATE_COUNT] = {
    [LINK_STATE_NONE] = DEFAULT_LINK_COLOR,
    [LINK_STATE_ENABLED] = OUI_COLOR_BLUE,
    [LINK_STATE_DISABLED] = OUI_COLOR_RED,
};

typedef struct Entity {
  int used;
  EntityType type;

  float radius;
  NtVec2f position;
  NtVec2f velocity;

  float masse;
  NtVec2f force;
  float invMasse;

  float elasticity;
  float restLength;

  EntityId edgeA;
  EntityId edgeB;
  LinkState state;

  int isVisible;
  OuiColor color;
  char buffer[BUFFER_SIZE];

  float BPM;
  float hasBeenPlayingFor;

  float width;
  float height;

  float marginTop;
  float marginLeft;
  float marginRight;
  float marginBottom;

  float paddingTop;
  float paddingLeft;
  float paddingRight;
  float paddingBottom;

  EntityId firstChild;
  EntityId firstParent;
  EntityId nextSibling;
} Entity;

typedef struct EntityManager {
  int count;
  Entity entities[MAX_ENTITIES];

  int countNodes;
  EntityId nodeIds[MAX_ENTITIES];
  // at the start of each physics frame, this array gets filled with the ids of all the nodes in the scene

  int collisionLinkedList[MAX_ENTITIES * MAX_ENTITIES];
  // if i is a node, row i represents a linked list of the other nodes i collides with
  // the head of the linked list is stored in the cell collisionLinkedList[i][NIL]

  int initialState[MAX_ENTITIES];
  // this is the initial state of the graph

  int assertMatrix[MAX_ENTITIES * MAX_ENTITIES];
  // if i is a link and j is a node
  // => assertMatrix[i * MAX_ENTITIES + j] = the id of the resource that should be attributed to j, in order for i to be triggarable
  // if i is a link and j is a link
  // => assertMatrix[i * MAX_ENTITIES + j] = the state (LinkState) that j should be in, in order for i to be triggarable

  int performMatrix[MAX_ENTITIES * MAX_ENTITIES];
  // if i is a link and j is a node
  // => performMatrix[i * MAX_ENTITIES + j] = the id of the resource that should be attributed to j after i is triggered
  // if i is a link and j is a link
  // => performMatrix[i * MAX_ENTITIES + j] = the state (LinkState) that j should be in after i is triggared
} EntityManager;

EntityId create_entity(EntityManager *entityManager);
Entity *get_entity(EntityManager *entityManager, EntityId id);
void delete_entity(EntityManager *entityManager, EntityId id);
void append_child(EntityManager *entityManager, EntityId pId, EntityId cId);
void remove_child(EntityManager *entityManager, EntityId pId, EntityId cId);

EntityId get_initial_state_node(EntityManager *entityManager, EntityId nodeId);
LinkState get_initial_state_link(EntityManager *entityManager, EntityId linkId);
LinkState get_assert_link_link(EntityManager *entityManager, EntityId selectedLinkId, EntityId id);
EntityId get_assert_link_node(EntityManager *entityManager, EntityId selectedLinkId, EntityId id);
LinkState get_perform_link_link(EntityManager *entityManager, EntityId selectedLinkId, EntityId id);
EntityId get_perform_link_node(EntityManager *entityManager, EntityId selectedLinkId, EntityId id);

void set_initial_state_link(EntityManager *entityManager, EntityId linkId);
void set_initial_state_node(EntityManager *entityManager, EntityId nodeId, EntityId resourceId);
void set_assert_link_link(EntityManager *entityManager, EntityId selectedLinkId, EntityId assertedLinkId);
void set_perform_link_link(EntityManager *entityManager, EntityId selectedLinkId, EntityId performedLinkId);
void set_assert_link_node(EntityManager *entityManager, EntityId selectedLinkId, EntityId assertedNodeId, EntityId assertedResourceId);
void set_perform_link_node(EntityManager *entityManager, EntityId selectedLinkId, EntityId performedNodeId, EntityId performedResourceId);

typedef enum SortAxis {
  SORT_AXIS_X,
  SORT_AXIS_Y,
} SortAxis;

void my_swap(int *a, int *b);
int heap_pop(EntityManager *entityManager, SortAxis sortAxis);
void heap_push(EntityManager *entityManager, int val, SortAxis sortAxis);
int x_axis_comparator(EntityManager *entityManager, EntityId aId, EntityId bId);
int y_axis_comparator(EntityManager *entityManager, EntityId aId, EntityId bId);
void heap_sort(EntityManager *entityManager, SortAxis sortAxis);
// ---------------- Entity Management -----------------

// -------------------- App State ---------------------
typedef enum InputMode {
  INPUT_MODE_NONE,
  INPUT_MODE_ASSERT,
  INPUT_MODE_PERFORM,
  INPUT_MODE_INITIALIZE,
  INPUT_MODE_PLAYING,
  INPUT_MODE_COUNT
} InputMode;

char *input_mode_labels[INPUT_MODE_COUNT] = {
    [INPUT_MODE_NONE] = "None",
    [INPUT_MODE_ASSERT] = "Assert",
    [INPUT_MODE_PERFORM] = "Perform",
    [INPUT_MODE_INITIALIZE] = "Initialize",
    [INPUT_MODE_PLAYING] = "Playing",
};

typedef struct Clock {
  float t;
  float dt;
} Clock;

typedef struct AppState {
  Clock clock;
  int togglePhysics;
  InputMode inputMode;

  ma_engine audioEngine;
  OuiContext ouiContext;
  EntityManager entityManager;

  EntityId brushId;
  EntityId resourceManagerId;
  EntityId graphInterpreterId;

  EntityId fpsTextLabelId;
  EntityId inputModeTextLabelId;

  EntityId selectedNodeId;
  // used to create links

  EntityId selectedLinkId;
  // used for assert and perform statements
} AppState;

void clock_tick(Clock *clock);

void init(AppState *app);
void update(AppState *app);
void draw(AppState *app);
void deinit(AppState **app);

void create_entities(AppState *app);
EntityId create_node(AppState *app);
EntityId create_link(AppState *app, EntityId edgeA, EntityId edgeB);
EntityId create_resource(AppState *app, char *fileName); // TODO: who owns the string

void handle_input(AppState *app);
void handle_mouse_click(AppState *app);
void handle_key_bindings(AppState *app);

void brush_mouse_event_handler(AppState *app);
void resource_manager_click_handler(AppState *app);

EntityId get_clicked_link(AppState *app);
EntityId get_clicked_node(AppState *app);
void link_mouse_event_handler(AppState *app);
void node_mouse_event_handler(AppState *app);

void process_audio(AppState *app);

void handle_physics(AppState *app);
void detect_collisions(AppState *app);
void resolve_collisions(AppState *app);
void apply_forces(AppState *app);

void draw_node(AppState *app, EntityId id);
void draw_link(AppState *app, EntityId id);
void draw_label(AppState *app, EntityId id);

void draw_fps(AppState *app);
void draw_brush(AppState *app);
void draw_input_mode(AppState *app);
void draw_resource_manager(AppState *app);
// -------------------- App State ---------------------

int main(int argc, char **argv) {
  srand(time(NULL));

  /* if you want the app to live on the stack
   * (this is not feasible if MAX_ENTITIES exceeds a certain threachold)
   *
   * AppState app = {0};
   * AppState *pApp = &app;
   * */

  // if you want the app to live on the heap
  // make sure to zero initialize the entityManager (we are using calloc here)
  // because it uses the Entity.used field as boolean to track allocations
  // (you can initialize it manually if you want but i am not sure wish other parts will break TODO:)
  AppState *pApp = (AppState *)calloc(1, sizeof(AppState));
  init(pApp);

  while (oui_has_next_frame(&(pApp->ouiContext))) {
    oui_begin_frame(&(pApp->ouiContext));

    update(pApp);
    draw(pApp);

    oui_end_frame(&(pApp->ouiContext));
  }

  // remove me when living on the stack
  deinit(&pApp);

  (void)argc;
  (void)argv;
  return 0;
}

void init(AppState *app) {
  if (app == NULL) {
    return;
  }

  Clock *clock = &(app->clock);
  ma_engine *audioEngine = &(app->audioEngine);
  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  app->togglePhysics = ON;
  app->inputMode = INPUT_MODE_INITIALIZE;

  // initialize clock
  clock->t = glfwGetTime();
  clock->dt = 0.0;

  // initialize oui
  OuiConfig ouiConfig = {
      .font = FONT,
      .windowTitle = WINDOW_TITLE,
      .windowWidth = WINDOW_WIDTH,
      .windowHeight = WINDOW_HEIGHT,
  };

  oui_context_init(ouiContext, &ouiConfig);
  glfwSetKeyCallback(ouiContext->window, keyboard_key_callback);
  glfwSetMouseButtonCallback(ouiContext->window, mouse_button_callback);
  glfwSetInputMode(ouiContext->window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);

  // initialize entityManager
  entityManager->count = 1; // at least the NIL entity exists
  create_entities(app);

  // initialize audioEngine
  ma_result result;
  result = ma_engine_init(NULL, audioEngine);

  if (result != MA_SUCCESS) { // TODO: handle this failure
    nob_log(NOB_ERROR, "Failed to initialize audio engine.");
    return;
  }
}

void update(AppState *app) {
  if (app == NULL) {
    return;
  }

  Clock *clock = &app->clock;
  clock_tick(clock);

  handle_input(app);
  handle_physics(app);
  process_audio(app);
  // TODO: compute layout here for example (right before drawing)
}

void draw(AppState *app) {
  if (app == NULL) {
    return;
  }

  EntityManager *entityManager = &(app->entityManager);

  for (int i = 1; i < entityManager->count; i++) {
    Entity *e = get_entity(entityManager, i);

    if (e->type == ENTITY_TYPE_NODE) {
      draw_node(app, i);
    } else if (e->type == ENTITY_TYPE_LINK) {
      draw_link(app, i);
    }
  }

  draw_fps(app);
  draw_brush(app);
  draw_input_mode(app);
  draw_resource_manager(app);
}

void deinit(AppState **app) {
  if (app == NULL || *app == NULL) {
    return;
  }

  oui_context_destroy(&((*app)->ouiContext));
  ma_engine_uninit(&((*app)->audioEngine));

  free(*app);
  *app = NULL;
}

void handle_input(AppState *app) {
  if (app == NULL) {
    return;
  }

  handle_key_bindings(app);
  handle_mouse_click(app);
}

void handle_key_bindings(AppState *app) {
  if (app == NULL) {
    return;
  }

  EntityManager *entityManager = &(app->entityManager);
  Entity *resourceManager = get_entity(entityManager, app->resourceManagerId);

  int state;
  state = keyboard_key_states[KEYBINDING_OPEN_RESOURCE_MANAGER];
  if (state == PRESSED) {
    resourceManager->isVisible = resourceManager->isVisible == YES ? NO : YES;
    keyboard_key_states[KEYBINDING_OPEN_RESOURCE_MANAGER] = RELEASED;
  }

  state = keyboard_key_states[KEYBINDING_CREATE_NODE];
  if (state == PRESSED) {
    create_node(app);
    // dont consume the press event to spam create nodes
    // TODO: this results in too many nodes being created
    // keyboard_key_states[KEYBINDING_CREATE_NODE] = RELEASED;
  }

  state = keyboard_key_states[KEYBINDING_DELETE_SELECTED];
  if (state == PRESSED) {
    if (app->selectedNodeId) {
      delete_entity(entityManager, app->selectedNodeId);
      app->selectedNodeId = NIL;
    } else if (app->selectedNodeId) {
      delete_entity(entityManager, app->selectedLinkId);
      app->selectedLinkId = NIL;
    }

    keyboard_key_states[KEYBINDING_DELETE_SELECTED] = RELEASED;
  }

  state = keyboard_key_states[KEYBINDING_DISABLE_PHYSICS];
  if (state == PRESSED) {
    app->togglePhysics = app->togglePhysics == ON ? OFF : ON;
    keyboard_key_states[KEYBINDING_DISABLE_PHYSICS] = RELEASED;
  }

  state = keyboard_key_states[KEYBINDING_ASSERT_MODE];
  if (state == PRESSED) {
    app->inputMode = INPUT_MODE_ASSERT;
    keyboard_key_states[KEYBINDING_ASSERT_MODE] = RELEASED;
  }

  state = keyboard_key_states[KEYBINDING_PERFORM_MODE];
  if (state == PRESSED) {
    app->inputMode = INPUT_MODE_PERFORM;
    keyboard_key_states[KEYBINDING_PERFORM_MODE] = RELEASED;
  }

  state = keyboard_key_states[KEYBINDING_INITIALIZE_MODE];
  if (state == PRESSED) {
    app->inputMode = INPUT_MODE_INITIALIZE;
    keyboard_key_states[KEYBINDING_INITIALIZE_MODE] = RELEASED;
  }

  state = keyboard_key_states[KEYBINDING_UNSELECT_BRUSH];
  if (state == PRESSED) {
    Entity *brush = get_entity(entityManager, app->brushId);
    brush->firstChild = NIL;

    keyboard_key_states[KEYBINDING_UNSELECT_BRUSH] = RELEASED;
  }

  state = keyboard_key_states[KEYBINDING_START_PLAYING];
  if (state == PRESSED) {
    Entity *graphInterpreter = get_entity(entityManager, app->graphInterpreterId);

    app->inputMode = app->inputMode == INPUT_MODE_INITIALIZE ? INPUT_MODE_PLAYING : INPUT_MODE_INITIALIZE;
    graphInterpreter->firstChild = app->inputMode == INPUT_MODE_PLAYING ? app->selectedNodeId : NIL;

    for (int i = 1; i < entityManager->count; i++) {
      Entity *e = get_entity(entityManager, i);

      if (e->type == ENTITY_TYPE_NODE) {
        e->firstChild = get_initial_state_node(entityManager, i);
      } else if (e->type == ENTITY_TYPE_LINK) {
        e->state = get_initial_state_link(entityManager, i);
      }
    }

    keyboard_key_states[KEYBINDING_START_PLAYING] = RELEASED;
  }
}

void handle_mouse_click(AppState *app) {
  resource_manager_click_handler(app);
  brush_mouse_event_handler(app);
  link_mouse_event_handler(app);
  node_mouse_event_handler(app);
}

void resource_manager_click_handler(AppState *app) {
  if (app == NULL) {
    return;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);
  Entity *resourceManager = get_entity(entityManager, app->resourceManagerId);

  if (!resourceManager->isVisible) {
    return;
  }

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);
  NtVec2f mousePos = get_mouse_position(ouiContext);

  int state;
  state = mouse_button_states[GLFW_MOUSE_BUTTON_LEFT];
  // the resourceManager listens for one mouse event: click

  if (state == PRESSED) {
    // did the user click the add sound button,
    // or did he click the remove sound button?

    // Add sound button hitbox
    float margin = resourceManager->marginBottom;
    float paddingLeftRight = resourceManager->paddingLeft;
    float paddingTopBottom = resourceManager->paddingTop;
    float resourceManagerWidth = windowWidth - 2 * margin;
    float resourceManagerHeight = windowHeight - 2 * margin;

    float resourceGap = 30;
    float resourceHeight = 100;
    float resourcePaddingRight = 10;

    // if the user clicked outside the resource manager
    // close it and dont consume the click event
    if (
        !(mousePos.x >= -resourceManagerWidth / 2 &&
          mousePos.x <= resourceManagerWidth / 2 &&
          mousePos.y >= -resourceManagerHeight / 2 &&
          mousePos.y <= resourceManagerHeight / 2)) {
      resourceManager->isVisible = NO;
      return;
    }

    // if he clicked the resourceManager
    // consume the click event
    mouse_button_states[GLFW_MOUSE_BUTTON_LEFT] = RELEASED;

    // check if the user clicked the add sound button
    float buttonHeight = 100;
    float buttonWidth = windowWidth - margin - paddingLeftRight;

    NtVec2f buttonCenter = {
        .x = 0,
        .y = -windowHeight / 2 + margin + paddingTopBottom + buttonHeight / 2,
    };

    if (
        mousePos.x >= buttonCenter.x - buttonWidth / 2 &&
        mousePos.x <= buttonCenter.x + buttonWidth / 2 &&
        mousePos.y >= buttonCenter.y - buttonHeight / 2 &&
        mousePos.y <= buttonCenter.y + buttonHeight / 2) {

      // TODO: get the file from a different thread
      char *title = "Add sound";

      size_t size;
      long path_max;
      path_max = pathconf(".", _PC_PATH_MAX);
      if (path_max == -1)
        size = 1024;
      else if (path_max > 10240)
        size = 10240;
      else
        size = path_max;

      char *buf;
      char *ptr;

      for (buf = ptr = NULL; ptr == NULL; size *= 2) {
        if ((buf = realloc(buf, size)) == NULL) {
          break;
        }

        ptr = getcwd(buf, size);
        if (ptr == NULL && errno != ERANGE) {
          break;
        }
      }

      if (buf != NULL) {
        nob_log(NOB_INFO, "cwd: %s", buf);

        char *fileName = tinyfd_openFileDialog(
            title,
            buf,
            0,
            NULL,
            NULL,
            0);

        nob_log(NOB_INFO, "selected file: %s", fileName);
        if (fileName) {
          create_resource(app, fileName);
        }
      }

      return;
    }

    // check if an X button was clicked
    // and if it was remove its corresponding resource
    EntityId resourceId = resourceManager->firstChild;

    float characterHeight = 25;
    float characterWidth = 25;
    float baseX = windowWidth / 2 - margin - paddingLeftRight - resourcePaddingRight - characterWidth;
    float baseY = windowHeight / 2 - margin - paddingTopBottom - resourceHeight / 2;

    while (resourceId != NIL) {
      Entity *resource = get_entity(entityManager, resourceId);

      if (
          mousePos.x >= baseX &&
          mousePos.x <= baseX + characterWidth &&
          mousePos.y >= baseY - characterHeight / 2 &&
          mousePos.y <= baseY + characterHeight / 2) {
        remove_child(entityManager, app->resourceManagerId, resourceId);
        return;
      }

      baseY -= resourceHeight + resourceGap;
      resourceId = resource->nextSibling;
    }
  }
}

EntityId get_clicked_link(AppState *app) {
  if (app == NULL) {
    return NIL;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);
  NtVec2f mousePos = get_mouse_position(ouiContext);

  int state;
  state = mouse_button_states[GLFW_MOUSE_BUTTON_LEFT];
  // if a link was efectively clicked
  // this function handles the consumption of mouse events

  if (state != PRESSED) {
    return NIL;
  }

  EntityId clickedLink = NIL;

  for (int i = 1; i < entityManager->count; i++) {
    Entity *e = get_entity(entityManager, i);

    if (e->type == ENTITY_TYPE_LINK) {
      float linkHeight = DEFAULT_LINK_THICKNESS;

      Entity *a = get_entity(entityManager, e->edgeA);
      Entity *b = get_entity(entityManager, e->edgeB);

      NtVec2f diff = nwt_sub(a->position, b->position);
      float distance = nwt_length(diff);
      NtVec2f normalizedDiff = nwt_normalize(diff);

      NtVec2f bOffset = nwt_add(b->position, nwt_mult_s(normalizedDiff, b->radius));
      float linkLength = distance - a->radius - b->radius;

      NtVec2f mid = nwt_add(bOffset, nwt_mult_s(normalizedDiff, linkLength / 2));

      NtVec2f orthogonal = {
          .x = -normalizedDiff.y,
          .y = normalizedDiff.x};
      nwt_normalize(orthogonal);

      // we transform to a different frame of reference
      // and we project on it
      float firstComponent = nwt_dot(nwt_sub(mousePos, mid), normalizedDiff);
      float secondComponent = nwt_dot(nwt_sub(mousePos, mid), orthogonal);

      if (
          firstComponent >= -linkLength / 2 &&
          firstComponent <= linkLength / 2 &&
          secondComponent >= -linkHeight / 2 &&
          secondComponent <= linkHeight / 2) {
        clickedLink = i;
        break;
      }
    }
  }

  if (clickedLink != NIL) {
    nob_log(NOB_INFO, "Detected a link click: %d", clickedLink);
    // consume the mouse event
    mouse_button_states[GLFW_MOUSE_BUTTON_LEFT] = RELEASED;
  }

  return clickedLink;
}

void link_mouse_event_handler(AppState *app) {
  if (app == NULL) {
    return;
  }

  InputMode mode = app->inputMode;
  EntityId selectedLinkId = app->selectedLinkId;
  EntityManager *entityManager = &(app->entityManager);

  int state;
  state = mouse_button_states[GLFW_MOUSE_BUTTON_LEFT];
  // a link listens for one mouse event: click

  if (state == PRESSED) {
    EntityId clickedLinkId = get_clicked_link(app);

    // what mode are we in?

    // in initialization mode
    // if a link is clicked you change focus to it
    // and if no link was clicked (clickedLinkId == NIL), you lose focus from it
    if (mode == INPUT_MODE_INITIALIZE) {
      // if it is already selected change its state
      if (app->selectedLinkId == clickedLinkId) {
        set_initial_state_link(entityManager, clickedLinkId);
      }
      app->selectedLinkId = clickedLinkId;
    }
    // in assert mode
    // when a link is clicked, you toggle its state in the assertMatrix
    else if (mode == INPUT_MODE_ASSERT) {
      set_assert_link_link(entityManager, selectedLinkId, clickedLinkId);
    }
    // in perform mode
    // when a link is clicked you toggle its state in the the performMatrix
    else if (mode == INPUT_MODE_PERFORM) {
      set_perform_link_link(entityManager, selectedLinkId, clickedLinkId);
    }
  }
}

EntityId get_clicked_node(AppState *app) {
  if (app == NULL) {
    return NIL;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);
  NtVec2f mousePos = get_mouse_position(ouiContext);

  int state;
  state = mouse_button_states[GLFW_MOUSE_BUTTON_LEFT];
  // if a node was efectively clicked
  // this function handles the consumption of the mouse event

  if (state != PRESSED) {
    return NIL;
  }

  float minDist = -1;
  EntityId clickedNode = NIL;

  for (int i = 1; i < entityManager->count; i++) {
    Entity *e = get_entity(entityManager, i);

    if (e->type == ENTITY_TYPE_NODE) {
      float distance = nwt_distance(mousePos, e->position);

      if (distance <= e->radius) {
        if (minDist == -1) {
          minDist = distance;
        }
        if (distance <= minDist) {
          clickedNode = i;
        }
      }
    }
  }

  if (clickedNode != NIL) {
    nob_log(NOB_INFO, "Detected a node click: %d", clickedNode);
    nob_log(NOB_INFO, "Produced a HELD mouse event");
    // GLFW doesn't generate GLFW_REPEAT events for the mouse
    // so we have to generate one when a node is first clicked to implement node dragging
    mouse_button_states[GLFW_MOUSE_BUTTON_LEFT] = HELD;
  }

  return clickedNode;
}

void set_node_resource_using_brush(AppState *app, EntityId nodeId) {
  if (app == NULL) {
    return;
  }

  InputMode mode = app->inputMode;
  EntityId selectedLinkId = app->selectedLinkId;
  EntityManager *entityManager = &(app->entityManager);
  Entity *brush = get_entity(entityManager, app->brushId);
  // Entity *node = get_entity(entityManager, nodeId);

  if (mode == INPUT_MODE_INITIALIZE) {
    // dont call remove_child
    // brush->firstChild is an element of the linkedlist owned by the resourceManager
    // its not owned by the brush or the node
    // node->firstChild = brush->firstChild;
    set_initial_state_node(
        entityManager,
        nodeId,
        brush->firstChild);
  } else if (mode == INPUT_MODE_ASSERT) {
    set_assert_link_node(
        entityManager,
        selectedLinkId,
        nodeId,
        brush->firstChild);
  } else if (mode == INPUT_MODE_PERFORM) {
    set_perform_link_node(
        entityManager,
        selectedLinkId,
        nodeId,
        brush->firstChild);
  }
}

void node_mouse_event_handler(AppState *app) {
  if (app == NULL) {
    return;
  }

  float dt = app->clock.dt;
  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);
  NtVec2f mousePos = get_mouse_position(ouiContext);

  int state;
  state = mouse_button_states[GLFW_MOUSE_BUTTON_LEFT];
  // a node listens for two mouse events: click and drag

  // in ALL input modes
  // if there is an active drag event
  // the selected nodes will be affected by it
  if (state == HELD) {
    Entity *e = get_entity(entityManager, app->selectedNodeId);
    float dragSpeed = 10 * dt * 1000;

    NtVec2f dir = nwt_sub(mousePos, e->position);
    float dirLength = nwt_length(dir);

    if (dirLength > 0) {
      dir = nwt_div_s(dir, dirLength);
      e->force = nwt_add(e->force, nwt_mult_s(dir, dragSpeed));
    }
  }
  // if there is a click event, find the clicked node
  else if (state == PRESSED) {
    EntityId clickedNode = get_clicked_node(app);
    Entity *brush = get_entity(entityManager, app->brushId);

    // in ALL input modes
    // if there was a click event but no node was clicked
    // the selected nodes should be unfocused
    if (
        clickedNode == NIL ||
        clickedNode == app->selectedNodeId) {
      app->selectedNodeId = NIL;
    }
    // otherwise if a node was clicked,
    // the action performed depends on: the input mode and whether or not the brush is selected
    else {
      // if no brush is selected
      // in ALL input modes, the clicked node gets selected
      if (brush->firstChild == NIL) {
        // and a link gets created between it and the previously selected node
        create_link(app, app->selectedNodeId, clickedNode);
        app->selectedNodeId = clickedNode;
      } else {
        set_node_resource_using_brush(app, clickedNode);
      }
    }
  }
}

void brush_mouse_event_handler(AppState *app) {
  if (app == NULL) {
    return;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);
  Entity *brush = get_entity(entityManager, app->brushId);
  Entity *resourceManager = get_entity(entityManager, app->resourceManagerId);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);
  NtVec2f mousePos = get_mouse_position(ouiContext);

  int state;
  state = mouse_button_states[GLFW_MOUSE_BUTTON_LEFT];
  // the brush only listens for one mouse event: click

  if (state == PRESSED) {
    // brush tool hit box
    float brushGap = 10;
    float brushRadius = 50;
    float brushPaddingLeft = brush->paddingLeft;
    float brushToolWidth = brush->width;
    float brushToolHeight = brush->height;
    float brushToolMarginLeft = brush->marginLeft;
    float brushToolMarginBottom = brush->marginBottom;

    NtVec2f brushToolCenter = {
        .x = -windowWidth / 2 + brushToolMarginLeft + brushToolWidth / 2,
        .y = -windowHeight / 2 + brushToolMarginBottom + brushToolHeight / 2,
    };

    if (
        mousePos.x >= brushToolCenter.x - brushToolWidth / 2 &&
        mousePos.x <= brushToolCenter.x + brushToolWidth / 2 &&
        mousePos.y >= brushToolCenter.y - brushToolHeight / 2 &&
        mousePos.y <= brushToolCenter.y + brushToolHeight / 2) {
      // if the brush was clicked, consume the event
      mouse_button_states[GLFW_MOUSE_BUTTON_LEFT] = RELEASED;
      // unselect the selected node (the one being dragged), but dont unselect the link
      app->selectedNodeId = NIL;

      // and check if the current resource needs to change
      EntityId resourceId = resourceManager->firstChild;
      float baseX = -windowWidth / 2 + brushToolMarginLeft + brushPaddingLeft + brushRadius;
      float baseY = -windowHeight / 2 + brushToolMarginBottom + brushToolHeight / 2;

      while (resourceId != NIL) {
        Entity *resource = get_entity(entityManager, resourceId);

        if (nwt_distance(mousePos, (NtVec2f){baseX, baseY}) <= brushRadius) {
          brush->firstChild = resourceId;
          nob_log(NOB_INFO, "Changed brush resource to %d", resourceId);
        }

        baseX += brushRadius + brushGap;
        resourceId = resource->nextSibling;
      }
    }
  }
}

void handle_physics(AppState *app) {
  detect_collisions(app);
  resolve_collisions(app);
  apply_forces(app);
}

void flag_collision_x_axis(AppState *app, EntityId aId, EntityId bId) {
  if (app == NULL) {
    return;
  }

  EntityManager *entityManager = &(app->entityManager);

  // add bId to the linked list on aId's row
  entityManager->collisionLinkedList[aId * MAX_ENTITIES + bId] = entityManager->collisionLinkedList[aId * MAX_ENTITIES];
  entityManager->collisionLinkedList[aId * MAX_ENTITIES] = bId;

  // add aId to the linked list on bId's row
  entityManager->collisionLinkedList[bId * MAX_ENTITIES + aId] = entityManager->collisionLinkedList[bId * MAX_ENTITIES];
  entityManager->collisionLinkedList[bId * MAX_ENTITIES] = aId;
}

void detect_collisions(AppState *app) {
  if (
      app == NULL ||
      app->togglePhysics == OFF) {
    return;
  }

  EntityManager *entityManager = &(app->entityManager);

  entityManager->countNodes = 0;
  for (int i = 1; i < entityManager->count; i++) {
    Entity *e = get_entity(entityManager, i);

    if (e->type == ENTITY_TYPE_NODE) {
      entityManager->nodeIds[entityManager->countNodes++] = i;
      entityManager->collisionLinkedList[i * MAX_ENTITIES] = NIL;
    }
  }

  heap_sort(entityManager, SORT_AXIS_X);

  // broad phase
  // TODO: sliding window: if a is behind you and you intersect with it, then you intersect will all the elements between you and a
  for (int i = 0; i < entityManager->countNodes - 1; i++) {
    for (int j = i + 1; j < entityManager->countNodes; j++) {
      EntityId aId = entityManager->nodeIds[i];
      EntityId bId = entityManager->nodeIds[j];

      Entity *e1 = get_entity(entityManager, aId);
      Entity *e2 = get_entity(entityManager, bId);

      if (fabs(e1->position.x - e2->position.x) <= e1->radius + e2->radius) {
        flag_collision_x_axis(app, aId, bId);
      } else {
        break;
      }
    }
  }

  // narrow phase
  for (int i = 0; i < entityManager->countNodes; i++) {
    EntityId id = entityManager->nodeIds[i];
    Entity *e1 = get_entity(entityManager, id);

    EntityId prevId = NIL;
    EntityId currId = entityManager->collisionLinkedList[id * MAX_ENTITIES];

    while (currId != NIL) {
      Entity *e2 = get_entity(entityManager, currId);

      NtVec2f delta = nwt_sub(e1->position, e2->position);
      float deltaLength = nwt_length(delta);

      float r1 = e1->radius;
      float r2 = e2->radius;
      float restLength = r1 + r2;

      if (deltaLength <= restLength) {
        entityManager->collisionLinkedList[id * MAX_ENTITIES + prevId] = currId;
        prevId = currId;
      }

      currId = entityManager->collisionLinkedList[id * MAX_ENTITIES + currId];
    }
    entityManager->collisionLinkedList[id * MAX_ENTITIES + prevId] = NIL;
  }
}

void resolve_collisions(AppState *app) {
  if (app == NULL) {
    return;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  float containerWidth = windowWidth;
  float containerHeight = windowHeight;

  int numIterations = CONSTRAINT_SOLVER_ITERATIONS;
  for (int iter = 0; iter < numIterations; iter++) {
    for (int i = 0; i < entityManager->countNodes; i++) {
      EntityId id = entityManager->nodeIds[i];
      Entity *e = get_entity(entityManager, id);

      if (e->type != ENTITY_TYPE_NODE)
        continue;

      e->position.x = oui_max(oui_min(e->position.x, containerWidth / 2 - e->radius), -containerWidth / 2 + e->radius);
      e->position.y = oui_max(oui_min(e->position.y, containerHeight / 2 - e->radius), -containerHeight / 2 + e->radius);
    }

    for (int i = 0; i < entityManager->countNodes; i++) {
      EntityId id = entityManager->nodeIds[i];
      Entity *e1 = get_entity(entityManager, id);

      EntityId currId = entityManager->collisionLinkedList[id * MAX_ENTITIES];

      while (currId != NIL) {
        Entity *e2 = get_entity(entityManager, currId);

        NtVec2f delta = nwt_sub(e1->position, e2->position);
        float deltaLength = nwt_length(delta);

        float r1 = e1->radius;
        float r2 = e2->radius;
        float restLength = r1 + r2;

        if (deltaLength == 0) {
          NtVec2f randomDisplacement = {
              .x = DEFAULT_NODE_RADIUS * ((float)rand() / INT_MAX),
              .y = DEFAULT_NODE_RADIUS * ((float)rand() / INT_MAX),
          };

          e1->position = nwt_add(e1->position, randomDisplacement);
          e2->position = nwt_add(e2->position, nwt_mult_s(randomDisplacement, -1));
        } else if (deltaLength < restLength) {
          // if i dont do this it the velocities dont get calculated correctly
          // why?? (maybe because the apply_forces function doesnt check if the nodes are about to seperate)
          float posError = (deltaLength - restLength) * 0.5;
          e1->position = nwt_sub(e1->position, nwt_mult_s(delta, posError / deltaLength));
          e2->position = nwt_add(e2->position, nwt_mult_s(delta, posError / deltaLength));
        }

        currId = entityManager->collisionLinkedList[id * MAX_ENTITIES + currId];
      }
    }
  }
}

void apply_forces(AppState *app) {
  if (
      app == NULL ||
      app->togglePhysics == OFF) {
    return;
  }

  float dt = app->clock.dt;
  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  float containerWidth = windowWidth;
  float containerHeight = windowHeight;

  // apply link forces
  for (int j = 1; j < entityManager->count; j++) {
    Entity *e = get_entity(entityManager, j);

    if (e->type != ENTITY_TYPE_LINK)
      continue;

    Entity *a = get_entity(entityManager, e->edgeA);
    Entity *b = get_entity(entityManager, e->edgeB);

    float restLength = e->restLength;
    NtVec2f delta = nwt_sub(a->position, b->position);
    float deltaLength = nwt_distance(a->position, b->position);

    // TODO: implement force damping
    if (deltaLength != restLength) {
      float posError = (deltaLength - restLength) * 0.5;

      NtVec2f springForceA = nwt_mult_s(delta, -e->elasticity * posError / deltaLength);
      NtVec2f springForceB = nwt_mult_s(delta, e->elasticity * posError / deltaLength);

      a->force = nwt_add(a->force, springForceA);
      b->force = nwt_add(b->force, springForceB);
    }
  }

  for (int i = 0; i < entityManager->countNodes; i++) {
    EntityId id = entityManager->nodeIds[i];
    Entity *e1 = get_entity(entityManager, id);

    // wall collisions
    float r = e1->radius;

    if ((e1->position.x <= -containerWidth / 2 + r && e1->velocity.x < 0) ||
        (e1->position.x >= containerWidth / 2 - r && e1->velocity.x > 0)) {
      e1->velocity.x *= -1;
    }

    if ((e1->position.y <= -containerHeight / 2 + r && e1->velocity.y < 0) ||
        (e1->position.y >= containerHeight / 2 - r && e1->velocity.y > 0)) {
      e1->velocity.y *= -1;
    }

    EntityId currId = entityManager->collisionLinkedList[id * MAX_ENTITIES];

    while (currId != NIL) {
      Entity *e2 = get_entity(entityManager, currId);

      NtVec2f delta = nwt_sub(e1->position, e2->position);
      float deltaLength = nwt_length(delta);

      float r1 = e1->radius;
      float r2 = e2->radius;
      float restLength = r1 + r2;

      if (deltaLength <= restLength) {
        float e = DEFAULT_RESTITUTION;
        float m1 = e1->masse, m2 = e2->masse;
        NtVec2f u1 = e1->velocity, u2 = e2->velocity;
        NtVec2f p1 = e1->position, p2 = e2->position;
        NtVec2f n = nwt_normalize(delta);

        // formula source: https://www.youtube.com/watch?v=zJLTEt_JFYg
        float coef1 = ((1 + e) * m2 / ((m1 + m2) * restLength)) * nwt_dot(nwt_sub(u2, u1), nwt_sub(p1, p2));
        // float coef2 = ((1 + e) * m1 / ((m1 + m2) * restLength)) * nwt_dot(nwt_sub(u1, u2), nwt_sub(p2, p1));

        e1->velocity = nwt_add(e1->velocity, nwt_mult_s(n, coef1));
        //  e2->velocity = nwt_add(e2->velocity, nwt_mult_s(n, -coef2));
      }

      currId = entityManager->collisionLinkedList[id * MAX_ENTITIES + currId];
    }

    e1->velocity = nwt_add(e1->velocity, nwt_mult_s(e1->force, e1->invMasse * dt));
    e1->position = nwt_add(e1->position, nwt_mult_s(e1->velocity, dt));
    e1->force = NT_ZERO;
  }
}

void heap_push(EntityManager *entityManager, int val, SortAxis sortAxis) {
  if (entityManager->countNodes >= MAX_ENTITIES) {
    nob_log(NOB_WARNING, "Can't push into heap, capacity exceeded, count: %d\n", entityManager->count);
    return;
  }

  int curr = entityManager->countNodes;
  entityManager->countNodes++;

  // insert the value at the end of the heap
  entityManager->nodeIds[curr] = val;
  // nob_log(NOB_INFO, "Pushing: %d", val);

  if (sortAxis == SORT_AXIS_X) {
    while (
        // while the element has a parent, compare with it
        // if parent is bigger swap => smallest to the top => min heap
        curr > 0 &&
        x_axis_comparator(entityManager, curr, (int)(curr / 2))) {
      my_swap(&(entityManager->nodeIds[curr]), &(entityManager->nodeIds[(int)(curr / 2)]));
      curr /= 2;
    }
  } else {
    while (
        // while the element has a parent, compare with it
        // if parent is bigger swap => smallest to the top => min heap
        curr > 0 &&
        y_axis_comparator(entityManager, curr, (int)(curr / 2))) {
      my_swap(&(entityManager->nodeIds[curr]), &(entityManager->nodeIds[(int)(curr / 2)]));
      curr /= 2;
    }
  }
}

int heap_pop(EntityManager *entityManager, SortAxis sortAxis) {
  if (entityManager == NULL || entityManager->countNodes == 0) {
    return NIL;
  }

  // bring the last element to the front
  int val = entityManager->nodeIds[0];
  entityManager->nodeIds[0] = entityManager->nodeIds[entityManager->countNodes - 1];
  entityManager->countNodes--;

  int curr = 0;
  while (curr < entityManager->countNodes) {
    int curr_left = curr * 2;
    int curr_right = curr * 2 + 1;

    // while you have children
    if (curr_left >= entityManager->countNodes) {
      break;
    }

    int curr_next = curr_left;
    if (curr_right < entityManager->countNodes) {
      if (sortAxis == SORT_AXIS_X) {
        curr_next = x_axis_comparator(entityManager, curr_left, curr_right) ? curr_next : curr_right;
      } else {
        curr_next = y_axis_comparator(entityManager, curr_left, curr_right) ? curr_next : curr_right;
      }
    }

    if (sortAxis == SORT_AXIS_X) {
      // if ur child is smaller swap
      if (x_axis_comparator(entityManager, curr_next, curr)) {
        my_swap(&(entityManager->nodeIds[curr]), &(entityManager->nodeIds[curr_next]));
        curr = curr_next;
      } else {
        break;
      }
    } else {
      // if ur child is smaller swap
      if (y_axis_comparator(entityManager, curr_next, curr)) {
        my_swap(&(entityManager->nodeIds[curr]), &(entityManager->nodeIds[curr_next]));
        curr = curr_next;
      } else {
        break;
      }
    }
  }

  return val;
}

void heap_sort(EntityManager *entityManager, SortAxis sortAxis) {
  if (entityManager == NULL) {
    return;
  }

  int n = entityManager->countNodes;
  int items[n];

  for (int i = 0; i < n; i++) {
    items[i] = entityManager->nodeIds[i];
  }

  entityManager->countNodes = 0;
  for (int i = 0; i < n; i++) {
    heap_push(entityManager, items[i], sortAxis);
  }

  for (int i = 0; i < n; i++) {
    items[i] = heap_pop(entityManager, sortAxis);
  }

  entityManager->countNodes = n;
  for (int i = 0; i < n; i++) {
    entityManager->nodeIds[i] = items[i];
  }
}

int x_axis_comparator(EntityManager *entityManager, EntityId aId, EntityId bId) {
  if (entityManager == NULL) {
    return 1;
  }

  Entity *a = get_entity(entityManager, entityManager->nodeIds[aId]);
  Entity *b = get_entity(entityManager, entityManager->nodeIds[bId]);

  return a->position.x < b->position.x;
}

int y_axis_comparator(EntityManager *entityManager, EntityId aId, EntityId bId) {
  if (entityManager == NULL) {
    return 1;
  }

  Entity *a = get_entity(entityManager, entityManager->nodeIds[aId]);
  Entity *b = get_entity(entityManager, entityManager->nodeIds[bId]);

  return a->position.y < b->position.y;
}

void my_swap(int *a, int *b) {
  int temp = *b;
  *b = *a;
  *a = temp;
}

void process_audio(AppState *app) {
  if (
      app == NULL ||
      app->graphInterpreterId == NIL) {
    return;
  }

  float dt = app->clock.dt;
  ma_engine *audioEngine = &(app->audioEngine);
  EntityManager *entityManager = &(app->entityManager);
  Entity *graphInterpreter = get_entity(entityManager, app->graphInterpreterId);

  if (app->inputMode == INPUT_MODE_PLAYING) {
    // on each frame
    // accumulate time and if it reachs the BPM
    // check if you can perform a transition
    // the node that is currently playing is the firstChild of the graph interpreter
    float beatDuration = 1 / (graphInterpreter->BPM / 60);
    graphInterpreter->hasBeenPlayingFor += dt;

    if (graphInterpreter->hasBeenPlayingFor >= beatDuration) {
      // iterate over all the link that bind to this node
      // compare their asserts to the current state of the graph
      // if the assert of a transition is valid, check the perform statement it imposes
      // execute them and then transition to the other end if the link (the graph is directed)
      EntityId currentNode = graphInterpreter->firstChild;
      nob_log(NOB_INFO, "Current node: %d", currentNode);

      nob_log(NOB_INFO, "Interating over the links:");
      for (int i = 1; i < entityManager->count; i++) {
        Entity *link = get_entity(entityManager, i);

        if (
            link->type == ENTITY_TYPE_LINK &&
            link->edgeA == currentNode && currentNode != NIL) {
          nob_log(NOB_INFO, " Found a link connected to the currentNode: %d", i);

          // check if the assert is true
          int assertIsValid = YES;
          nob_log(NOB_INFO, " Checking if its assert is valid");

          for (int j = 1; j < entityManager->count; j++) {
            if (i == j) {
              continue;
            }

            Entity *e = get_entity(entityManager, j);

            if (e->type == ENTITY_TYPE_NODE) {
              EntityId assertedResourceId = get_assert_link_node(entityManager, i, j);

              if (
                  assertedResourceId != NIL &&
                  e->firstChild != assertedResourceId) {
                nob_log(NOB_INFO, "   Assert is not valid because of node %d", j);
                assertIsValid = NO;
              }
            } else if (e->type == ENTITY_TYPE_LINK) {
              LinkState assertedLinkState = get_assert_link_link(entityManager, i, j);

              if (
                  assertedLinkState != LINK_STATE_NONE &&
                  e->state != assertedLinkState) {
                nob_log(NOB_INFO, "   Assert is not valid because of link %d", j);
                assertIsValid = NO;
              }
            }
          }

          if (assertIsValid) {
            nob_log(NOB_INFO, " Transitioning from %d to %d using the %d", link->edgeA, link->edgeB, i);

            for (int j = 1; j < entityManager->count; j++) {
              Entity *e = get_entity(entityManager, j);

              if (e->type == ENTITY_TYPE_NODE) {
                EntityId performedResourceId = get_perform_link_node(entityManager, i, j);

                if (performedResourceId != NIL) {
                  e->firstChild = performedResourceId;
                }
              } else if (e->type == ENTITY_TYPE_LINK) {
                LinkState performedLinkState = get_perform_link_link(entityManager, i, j);

                if (performedLinkState != LINK_STATE_NONE) {
                  e->state = performedLinkState;
                }
              }
            }

            graphInterpreter->firstChild = link->edgeB;
            break; // if you find a link that can be triggered, stop the search and trigger it
          }
        }
      }

      ma_engine_play_sound(audioEngine, "wav/a1.wav", NULL);
      graphInterpreter->hasBeenPlayingFor = 0;
    }
  }
}

void create_entities(AppState *app) {
  if (app == NULL) {
    return;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  int numNodes = NUM_NODES;
  float masse = DEFAULT_NODE_MASSE;
  float radius = DEFAULT_NODE_RADIUS;
  OuiColor color = DEFAULT_NODE_COLOR;

  for (int i = 0; i < numNodes; i++) {
    EntityId id = create_entity(entityManager);
    Entity *e = get_entity(entityManager, id);

    e->type = ENTITY_TYPE_NODE;

    e->radius = radius;
    e->position.x = rand() % (2 * (int)windowWidth) - windowWidth;
    e->position.y = rand() % (2 * (int)windowHeight) - windowHeight;

    e->velocity.x = rand() % 200 - 100;
    e->velocity.y = rand() % 200 - 100;

    e->masse = masse;
    e->invMasse = 1.0 / masse;

    e->color = color;
  }

  app->fpsTextLabelId = create_entity(entityManager);
  Entity *fpsTextLabel = get_entity(entityManager, app->fpsTextLabelId);
  fpsTextLabel->marginTop = 50;
  fpsTextLabel->marginRight = 15;

  app->inputModeTextLabelId = create_entity(entityManager);
  Entity *inputModeTextLabel = get_entity(entityManager, app->inputModeTextLabelId);
  inputModeTextLabel->marginTop = 50;
  inputModeTextLabel->marginLeft = 10;

  app->brushId = create_entity(entityManager);
  Entity *brush = get_entity(entityManager, app->brushId);
  brush->isVisible = 1;
  brush->radius = 10;
  brush->color = OUI_COLOR_BLACK;
  brush->width = 400;
  brush->height = 100;
  brush->paddingLeft = 10;
  brush->marginLeft = 10;
  brush->marginBottom = 10;

  app->resourceManagerId = create_entity(entityManager);
  Entity *resourceManager = get_entity(entityManager, app->resourceManagerId);
  resourceManager->radius = 10;
  resourceManager->color = OUI_COLOR_BLACK;
  resourceManager->marginTop = 20;
  resourceManager->marginLeft = 20;
  resourceManager->marginRight = 20;
  resourceManager->marginBottom = 20;
  resourceManager->paddingLeft = 50;
  resourceManager->paddingRight = 50;
  resourceManager->paddingTop = 30;
  resourceManager->paddingBottom = 30;

  app->graphInterpreterId = create_entity(entityManager);
  Entity *graphInterpreter = get_entity(entityManager, app->graphInterpreterId);
  graphInterpreter->BPM = DEFAULT_BPM;
}

EntityId create_node(AppState *app) {
  if (app == NULL) {
    return NIL;
  }

  EntityManager *entityManager = &(app->entityManager);

  EntityId nodeId = create_entity(entityManager);
  Entity *node = get_entity(entityManager, nodeId);

  node->type = ENTITY_TYPE_NODE;
  node->radius = DEFAULT_NODE_RADIUS;
  node->masse = DEFAULT_NODE_MASSE;
  node->invMasse = 1.0 / DEFAULT_NODE_MASSE;
  node->color = DEFAULT_NODE_COLOR;

  return nodeId;
}

EntityId create_link(AppState *app, EntityId a, EntityId b) {
  if (app == NULL || a == NIL || b == NIL) {
    return NIL;
  }

  EntityManager *entityManager = &(app->entityManager);

  for (int i = 1; i < entityManager->count; i++) {
    Entity *e = get_entity(entityManager, i);

    if (e->type == ENTITY_TYPE_LINK) {
      if (e->edgeA == a && e->edgeB == b) {
        nob_log(NOB_WARNING, "Link already exists");
        return NIL;
      }
    }
  }

  EntityId linkId = create_entity(entityManager);
  Entity *link = get_entity(entityManager, linkId);

  link->type = ENTITY_TYPE_LINK;
  link->color = DEFAULT_LINK_COLOR;
  link->elasticity = DEFAULT_LINK_ELASTICITY;
  link->restLength = DEFAULT_LINK_RESTLENGTH;

  link->edgeA = a;
  link->edgeB = b;

  return linkId;
}

EntityId create_resource(AppState *app, char *fileName) {
  if (app == NULL) {
    return NIL;
  }

  EntityManager *entityManager = &(app->entityManager);
  EntityId resourceId = create_entity(entityManager);
  Entity *resource = get_entity(entityManager, resourceId);

  resource->type = ENTITY_TYPE_RESOURCE;
  snprintf(resource->buffer, BUFFER_SIZE, "%s", fileName);
  // resource->buffer = fileName; // TODO: use after free?
  resource->color = (OuiColor){
      .r = rand() % 256,
      .g = rand() % 256,
      .b = rand() % 256,
      .a = 255,
  };

  append_child(entityManager, app->resourceManagerId, resourceId);
  return resourceId;
}

void draw_label(AppState *app, EntityId id) {
  if (app == NULL) {
    return;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  Entity *e = get_entity(entityManager, id);
  OuiText text = {0};

  text.fontSize = DEFAULT_FONT_SIZE;
  text.lineHeight = DEFAULT_LINE_HEIGHT;

  text.startPos = (OuiVec2f){
      .x = e->position.x,
      .y = e->position.y,
  };

  text.content = e->buffer;
  text.fontColor = DEFAULT_TEXT_COLOR;

  oui_draw_text(ouiContext, &text);
}

void draw_fps(AppState *app) {
  if (app == NULL) {
    return;
  }

  Clock *clock = &(app->clock);
  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  float marginTop = 50;
  float marginRight = 15;
  float characterWidth = 25;

  const int bufferSize = 32;
  char fpsString[bufferSize];
  // snprintf(fpsString, bufferSize, "%.2f FPS", 1 / clock->dt);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  Entity *fpsTextLabel = get_entity(entityManager, app->fpsTextLabelId);
  snprintf(fpsTextLabel->buffer, BUFFER_SIZE, "%.2f FPS", 1 / clock->dt);

  // int fpsStringLength = strlen(fpsString);
  int fpsStringLength = strlen(fpsTextLabel->buffer);
  fpsTextLabel->position.y = (float)windowHeight / 2 - marginTop;
  fpsTextLabel->position.x = (float)windowWidth / 2 - characterWidth * fpsStringLength - marginRight;

  // fpsTextLabel->buffer = fpsString;
  draw_label(app, app->fpsTextLabelId);
  // fpsTextLabel->buffer = NULL;
}

void draw_input_mode(AppState *app) {
  if (app == NULL) {
    return;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  float marginTop = 50;
  float marginLeft = 10;

  const int bufferSize = 32;
  char inputModeString[bufferSize];
  // snprintf(inputModeString, bufferSize, "%s", input_mode_labels[app->inputMode]);
  snprintf(inputModeString, bufferSize, "%s", input_mode_labels[app->inputMode]);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  Entity *inputModeTextLabel = get_entity(entityManager, app->inputModeTextLabelId);
  snprintf(inputModeTextLabel->buffer, BUFFER_SIZE, "%s", input_mode_labels[app->inputMode]);

  inputModeTextLabel->position.x = -(float)windowWidth / 2 + marginLeft;
  inputModeTextLabel->position.y = (float)windowHeight / 2 - marginTop;

  // inputModeTextLabel->buffer = inputModeString;
  draw_label(app, app->inputModeTextLabelId);
  // inputModeTextLabel->buffer = NULL;
}

void draw_node(AppState *app, EntityId id) {
  if (app == NULL) {
    return;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  InputMode mode = app->inputMode;
  EntityId selectedLinkId = app->selectedLinkId;
  EntityId selectedNodeId = app->selectedNodeId;

  EntityId graphInterpreterId = app->graphInterpreterId;
  Entity *graphInterpreter = get_entity(entityManager, graphInterpreterId);

  Entity *e = get_entity(entityManager, id);
  OuiRectangle rectangle = {0};

  rectangle.width = 2 * e->radius;
  rectangle.height = 2 * e->radius;

  rectangle.centerPos = (OuiVec2f){
      .x = e->position.x,
      .y = e->position.y,
  };

  rectangle.borderRadius = e->radius;
  rectangle.borderResolution = DEFAULT_BORDER_RESOLUTION;

  if (id == selectedNodeId) {
    rectangle.backgroundColor = SELECTED_NODE_COLOR;
  } else if (id == graphInterpreter->firstChild) {
    rectangle.backgroundColor = OUI_COLOR_RED;
  } else {
    EntityId resourceId = NIL;

    if (mode == INPUT_MODE_INITIALIZE) {
      resourceId = get_initial_state_node(entityManager, id);
    } else if (mode == INPUT_MODE_ASSERT) {
      resourceId = get_assert_link_node(entityManager, selectedLinkId, id);
    } else if (mode == INPUT_MODE_PERFORM) {
      resourceId = get_perform_link_node(entityManager, selectedLinkId, id);
    } else if (mode == INPUT_MODE_PLAYING) {
      resourceId = e->firstChild;
    }

    if (resourceId == NIL) {
      rectangle.backgroundColor = DEFAULT_NODE_COLOR;
    } else {
      Entity *resource = get_entity(entityManager, resourceId);
      rectangle.backgroundColor = resource->color;
    }
  }

  float characterWidth = 30;
  float lineHeight = 35;
  OuiText text = {0};
  snprintf(e->buffer, BUFFER_SIZE, "%d", id);
  text.startPos.x = rectangle.centerPos.x - characterWidth / 2;
  text.startPos.y = rectangle.centerPos.y - lineHeight / 2;
  text.fontColor = OUI_COLOR_BLACK;
  text.fontSize = DEFAULT_FONT_SIZE;
  text.lineHeight = DEFAULT_LINE_HEIGHT;
  text.maxLineWidth = 100;

  oui_draw_rectangle(ouiContext, &rectangle);

  text.content = e->buffer;
  oui_draw_text(ouiContext, &text);
  text.content = NULL;
}

void draw_link(AppState *app, EntityId id) {
  if (app == NULL) {
    return;
  }

  float arrowSize = 10;
  float arrowBorderRadius = 3;

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  InputMode mode = app->inputMode;
  EntityId selectedLinkId = app->selectedLinkId;

  Entity *e = get_entity(entityManager, id);
  Entity *a = get_entity(entityManager, e->edgeA);
  Entity *b = get_entity(entityManager, e->edgeB);

  // Draw the link's body
  NtVec2f diff = nwt_sub(a->position, b->position);
  float distance = nwt_length(diff);
  NtVec2f normalizedDiff = nwt_normalize(diff);

  NtVec2f bOffset = nwt_add(b->position, nwt_mult_s(normalizedDiff, b->radius));
  float linkLength = distance - a->radius - b->radius;

  NtVec2f mid = nwt_add(bOffset, nwt_mult_s(normalizedDiff, linkLength / 2));

  float cos = nwt_dot(normalizedDiff, NT_X);
  float sin = nwt_dot(normalizedDiff, NT_Y);

  OuiRectangle rectangle = {0};

  rectangle.width = linkLength;
  rectangle.height = DEFAULT_LINK_THICKNESS;

  if (sin > 0) {
    rectangle.rotationXY = acos(cos);
  } else {
    rectangle.rotationXY = NT_TAU - acos(cos);
  }

  rectangle.centerPos = (OuiVec2f){
      .x = mid.x,
      .y = mid.y,
  };

  rectangle.borderRadius = 0;
  rectangle.borderRadius = arrowBorderRadius;
  rectangle.borderResolution = 0;

  if (id == selectedLinkId) {
    rectangle.backgroundColor = SELECTED_NODE_COLOR;
  } else {
    LinkState state = LINK_STATE_NONE;

    if (mode == INPUT_MODE_INITIALIZE) {
      state = get_initial_state_node(entityManager, id);
    } else if (mode == INPUT_MODE_ASSERT) {
      state = get_assert_link_link(entityManager, selectedLinkId, id);
    } else if (mode == INPUT_MODE_PERFORM) {
      state = get_perform_link_link(entityManager, selectedLinkId, id);
    } else if (mode == INPUT_MODE_PLAYING) {
      state = e->state;
    }

    rectangle.backgroundColor = link_state_colors[state];
  }
  oui_draw_rectangle(ouiContext, &rectangle);

  // Draw the arrow
  rectangle.width = arrowSize;
  rectangle.height = DEFAULT_LINK_THICKNESS;
  // rectangle.borderRadius = arrowBorderRadius;

  // the direction orthogonal to the current link direction (normalizedDiff rotated by 90 degrees)
  NtVec2f orthogonal = {
      .x = -normalizedDiff.y,
      .y = normalizedDiff.x};
  nwt_normalize(orthogonal);

  // rotation relative to the current link direction
  rectangle.rotationXY += -NT_PI / 4;

  rectangle.centerPos.x = b->position.x + normalizedDiff.x * (b->radius + rectangle.width / 2 + 1) - 3 * orthogonal.x;
  rectangle.centerPos.y = b->position.y + normalizedDiff.y * (b->radius + rectangle.width / 2 + 1) - 3 * orthogonal.y;
  oui_draw_rectangle(ouiContext, &rectangle);

  // rotation relative to the current link direction
  rectangle.rotationXY += NT_PI / 2;
  rectangle.centerPos.x = b->position.x + normalizedDiff.x * (b->radius + rectangle.width / 2 + 1) + 3 * orthogonal.x;
  rectangle.centerPos.y = b->position.y + normalizedDiff.y * (b->radius + rectangle.width / 2 + 1) + 3 * orthogonal.y;
  oui_draw_rectangle(ouiContext, &rectangle);
  // Draw the arrow
}

void draw_brush(AppState *app) {
  if (app == NULL) {
    return;
  }

  float gap = 10;
  float paddingLeft = 10;
  float marginLeft = 10;
  float marginBottom = 10;
  float brushRadius = 50;
  float paletteWidth = 400;
  float paletteHeight = 100;

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  EntityId brushId = app->brushId;
  Entity *brush = get_entity(entityManager, brushId);

  if (!brush->isVisible) {
    return;
  }

  EntityId resourceManagerId = app->resourceManagerId;
  Entity *resourceManager = get_entity(entityManager, resourceManagerId);

  if (resourceManager->firstChild == NIL) {
    return;
  }

  OuiRectangle rectangle = {0};

  // draw the container's background
  rectangle.height = paletteHeight;
  rectangle.borderRadius = brush->radius;
  rectangle.backgroundColor = brush->color;
  rectangle.centerPos.x = -windowWidth / 2 + marginLeft + paletteWidth / 2;
  rectangle.centerPos.y = -windowHeight / 2 + marginBottom + paletteHeight / 2;

  rectangle.width = paletteWidth;

  int resourceCount = resourceManager->firstChild != NIL;
  EntityId resourceId = resourceManager->firstChild;
  while (resourceId != NIL) {
    Entity *resource = get_entity(entityManager, resourceId);
    resourceCount++;
    resourceId = resource->nextSibling;
  }

  if (resourceCount > 0) {
    rectangle.width = resourceCount * (brushRadius + gap) + paddingLeft;
    rectangle.centerPos.x = -windowWidth / 2 + marginLeft + rectangle.width / 2;
  }
  oui_draw_rectangle(ouiContext, &rectangle);

  // draw the already loaded resources
  resourceId = resourceManager->firstChild;
  float baseX = -windowWidth / 2 + marginLeft + paddingLeft + brushRadius;
  float baseY = -windowHeight / 2 + marginBottom + paletteHeight / 2;

  while (resourceId != NIL) {
    Entity *resource = get_entity(entityManager, resourceId);

    rectangle.width = brushRadius + 5;
    rectangle.height = brushRadius + 5;
    rectangle.centerPos.x = baseX;
    rectangle.centerPos.y = baseY;
    rectangle.borderRadius = brushRadius + 5;
    rectangle.backgroundColor = OUI_COLOR_WHITE;

    if (brush->firstChild == resourceId) {
      oui_draw_rectangle(ouiContext, &rectangle);
    }

    rectangle.width = brushRadius;
    rectangle.height = brushRadius;
    rectangle.borderRadius = brushRadius;
    rectangle.backgroundColor = resource->color;
    rectangle.backgroundColor.a = 255;
    oui_draw_rectangle(ouiContext, &rectangle);

    baseX += brushRadius + gap;
    resourceId = resource->nextSibling;
  }
}

void draw_resource_manager(AppState *app) {
  if (app == NULL) {
    return;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  EntityId resourceManagerId = app->resourceManagerId;
  Entity *resourceManager = get_entity(entityManager, resourceManagerId);

  if (!resourceManager->isVisible) {
    return;
  }

  float fontSize = 0.8;

  float gap = 30;
  float paddingTopBottom = 30;
  float paddingLeftRight = 50;
  float margin = DEFAULT_MENU_MARGIN;

  float resourceGap = 30;
  float resourceHeight = 100;
  float resourcePaddingLeft = 10;
  float resourceColorBorderRadius = 50;

  OuiText text = {0};
  OuiRectangle rectangle = {0};

  // draw the menu's background
  rectangle.width = windowWidth - 2 * margin;
  rectangle.height = windowHeight - 2 * margin;
  rectangle.centerPos.x = resourceManager->position.x;
  rectangle.centerPos.y = resourceManager->position.y;
  rectangle.borderRadius = resourceManager->radius;
  rectangle.backgroundColor = resourceManager->color;
  oui_draw_rectangle(ouiContext, &rectangle);

  // draw the already loaded resources
  EntityId resourceId = resourceManager->firstChild;

  text.fontSize = fontSize;
  text.fontColor = OUI_COLOR_WHITE;
  text.lineHeight = DEFAULT_LINE_HEIGHT;
  float baseX = resourceManager->position.x;
  float baseY = windowHeight / 2 - margin - paddingTopBottom - resourceHeight / 2;

  while (resourceId != NIL) {
    Entity *resource = get_entity(entityManager, resourceId);

    // draw the resource's background
    rectangle.height = resourceHeight;
    rectangle.width = windowWidth - 2 * paddingLeftRight;
    rectangle.centerPos.x = baseX;
    rectangle.centerPos.y = baseY;
    rectangle.borderRadius = resourceManager->radius;
    rectangle.backgroundColor = resourceManager->color;
    oui_draw_rectangle(ouiContext, &rectangle);

    // draw the resource's color value
    rectangle.width = resourceColorBorderRadius;
    rectangle.height = resourceColorBorderRadius;
    rectangle.centerPos.x = -windowWidth / 2 + paddingLeftRight + resourcePaddingLeft + resourceColorBorderRadius;
    rectangle.centerPos.y = baseY;
    rectangle.borderRadius = resourceColorBorderRadius;
    rectangle.backgroundColor = resource->color;
    oui_draw_rectangle(ouiContext, &rectangle);

    // draw the resource's label
    text.startPos.x = -windowWidth / 2 + paddingLeftRight + resourcePaddingLeft + 2 * resourceColorBorderRadius + resourceGap;
    text.startPos.y = baseY - 14;
    text.content = resource->buffer;
    oui_draw_text(ouiContext, &text);
    text.content = NULL;

    float characterWidth = 25;
    text.content = "X";
    text.startPos.x = windowWidth / 2 - margin - paddingLeftRight - resourcePaddingLeft - characterWidth;
    oui_draw_text(ouiContext, &text);
    text.content = NULL;

    baseY -= resourceHeight + gap;
    resourceId = resource->nextSibling;
  }

  // draw the add sound button
  rectangle.width = windowWidth - 2 * paddingLeftRight;
  rectangle.height = resourceHeight;
  rectangle.centerPos.x = 0;
  rectangle.centerPos.y = -windowHeight / 2 + margin + paddingTopBottom + resourceHeight / 2;
  rectangle.borderRadius = resourceManager->radius;
  rectangle.backgroundColor = resourceManager->color;
  oui_draw_rectangle(ouiContext, &rectangle);

  text.content = "Add sound";
  text.startPos.x = -100;
  text.startPos.y = -windowHeight / 2 + margin + paddingTopBottom + resourceHeight / 2 - 14;
  oui_draw_text(ouiContext, &text);
  text.content = NULL;
}

EntityId create_entity(EntityManager *entityManager) {
  //  TODO: zero set the entity behore returning it maybe or make sure to protect the NIL value in the setters

  assert(
      entityManager != NULL &&
      entityManager->count >= 1 &&
      entityManager->count <= MAX_ENTITIES);

  // reserve an empty slot
  for (int i = 1; i < entityManager->count; i++) {
    Entity *e = &(entityManager->entities[i]);

    if (!e->used) {
      e->used = YES;
      nob_log(NOB_INFO, "Created a new entity: reused an old slot (count = %d)", entityManager->count);

      return i;
    }
  }

  // create a new slot
  if (entityManager->count < MAX_ENTITIES) {
    Entity *e = &(entityManager->entities[entityManager->count]);

    e->used = YES;
    entityManager->count++;
    nob_log(NOB_INFO, "Created a new entity: created a new slot (count = %d)", entityManager->count);

    return entityManager->count - 1;
  }

  // buy more ram
  return NIL;
}

Entity *get_entity(EntityManager *entityManager, EntityId id) {
  assert(
      entityManager != NULL &&
      entityManager->count >= 1 &&
      entityManager->count <= MAX_ENTITIES);

  if (
      id <= NIL ||
      id >= entityManager->count ||
      !entityManager->entities[id].used) {
    return &(entityManager->entities[NIL]);
  }

  return &(entityManager->entities[id]);
}

void delete_entity(EntityManager *entityManager, EntityId id) {
  assert(
      entityManager != NULL &&
      entityManager->count >= 1 &&
      entityManager->count <= MAX_ENTITIES);

  if (id <= NIL || id >= entityManager->count) {
    return;
  }

  Entity *deleteMe = get_entity(entityManager, id);

  // if id is a node, delete all the links that connect to it
  if (deleteMe->type == ENTITY_TYPE_NODE) {
    for (int i = 1; i < entityManager->count; i++) {
      Entity *e = get_entity(entityManager, i);

      if (e->type == ENTITY_TYPE_LINK && (e->edgeA == id || e->edgeB == id)) {
        // TODO: the link can be the selected which would make app->selectedLinkId invalid
        memset(&(entityManager->entities[i]), 0, sizeof(Entity));
      }
    }
  }

  memset(&(entityManager->entities[id]), 0, sizeof(Entity));
  nob_log(NOB_INFO, "Deleted an entity: %d", id);
}

void append_child(EntityManager *entityManager, EntityId pId, EntityId cId) {
  if (
      entityManager == NULL ||
      pId == NIL || cId == NIL) {
    return;
  }

  Entity *child = get_entity(entityManager, cId);
  Entity *parent = get_entity(entityManager, pId);

  if (parent->firstChild == NIL) {
    parent->firstChild = cId;
  } else {
    Entity *iter = get_entity(entityManager, NIL);
    EntityId iterId = parent->firstChild;

    int dontLoopForever = 0;
    while (
        iterId != NIL &&
        dontLoopForever < MAX_ENTITIES) {
      iter = get_entity(entityManager, iterId);
      iterId = iter->nextSibling;
      dontLoopForever++;
    }

    iter->nextSibling = cId;
  }

  if (child->firstParent == NIL) {
    child->firstParent = pId;
  } else {
    Entity *iter = get_entity(entityManager, NIL);
    EntityId iterId = child->firstParent;

    int dontLoopForever = 0;
    while (
        iterId != NIL &&
        dontLoopForever < MAX_ENTITIES) {
      iter = get_entity(entityManager, iterId);
      iterId = iter->nextSibling;
      dontLoopForever++;
    }

    iter->nextSibling = pId;
  }

  nob_log(NOB_INFO, "Established a parent-child relationship between %d and %d", pId, cId);
}

void remove_child(EntityManager *entityManager, EntityId pId, EntityId cId) {
  if (
      entityManager == NULL ||
      pId == NIL || cId == NIL) {
    return;
  }

  Entity *child = get_entity(entityManager, cId);
  Entity *parent = get_entity(entityManager, pId);

  if (parent->firstChild == cId) {
    parent->firstChild = child->nextSibling;
    child->nextSibling = NIL;
  } else if (parent->firstChild != NIL) {
    EntityId prevId = parent->firstChild;
    Entity *prev = get_entity(entityManager, prevId);

    EntityId currId = prev->nextSibling;
    Entity *curr = get_entity(entityManager, currId);

    int dontLoopForever = 0;
    while (
        currId != NIL &&
        dontLoopForever < MAX_ENTITIES) {
      curr = get_entity(entityManager, currId);

      if (currId == cId) {
        prev->nextSibling = curr->nextSibling;
        curr->nextSibling = NIL;
        break;
      }

      prev = curr;
      currId = curr->nextSibling;
      dontLoopForever++;
    }
  }

  if (child->firstParent == pId) {
    child->firstParent = parent->nextSibling;
    parent->nextSibling = NIL;
  } else if (child->firstParent != NIL) {
    EntityId prevId = child->firstParent;
    Entity *prev = get_entity(entityManager, prevId);

    EntityId currId = prev->nextSibling;
    Entity *curr = get_entity(entityManager, currId);

    int dontLoopForever = 0;
    while (
        currId != NIL &&
        dontLoopForever < MAX_ENTITIES) {
      curr = get_entity(entityManager, currId);

      if (currId == pId) {
        prev->nextSibling = curr->nextSibling;
        curr->nextSibling = NIL;
        break;
      }

      prev = curr;
      currId = curr->nextSibling;
      dontLoopForever++;
    }
  }

  nob_log(NOB_INFO, "Removed the parent-child relationship between %d and %d", pId, cId);
}

void set_initial_state_node(EntityManager *entityManager, EntityId nodeId, EntityId resourceId) {
  if (
      entityManager == NULL ||
      nodeId == NIL) {
    return;
  }

  entityManager->initialState[nodeId] = resourceId;
}

void set_initial_state_link(EntityManager *entityManager, EntityId linkId) {
  if (
      entityManager == NULL ||
      linkId == NIL) {
    return;
  }

  LinkState prevState = entityManager->initialState[linkId];
  entityManager->initialState[linkId] = (prevState + 1) % LINK_STATE_COUNT;
}

void set_assert_link_link(EntityManager *entityManager, EntityId selectedLinkId, EntityId assertedLinkId) {
  if (
      entityManager == NULL ||
      selectedLinkId == NIL ||
      assertedLinkId == NIL) {
    return;
  }

  int rowSize = MAX_ENTITIES;
  int prevState = entityManager->assertMatrix[selectedLinkId * rowSize + assertedLinkId];
  entityManager->assertMatrix[selectedLinkId * rowSize + assertedLinkId] = (prevState + 1) % LINK_STATE_COUNT;
}

void set_perform_link_link(EntityManager *entityManager, EntityId selectedLinkId, EntityId performedLinkId) {
  if (
      entityManager == NULL ||
      selectedLinkId == NIL ||
      performedLinkId == NIL) {
    return;
  }

  int rowSize = MAX_ENTITIES;
  int prevState = entityManager->performMatrix[selectedLinkId * rowSize + performedLinkId];
  entityManager->performMatrix[selectedLinkId * rowSize + performedLinkId] = (prevState + 1) % LINK_STATE_COUNT;
}

void set_assert_link_node(EntityManager *entityManager, EntityId selectedLinkId, EntityId assertedNodeId, EntityId assertedResourceId) {
  if (
      entityManager == NULL ||
      selectedLinkId == NIL ||
      assertedNodeId == NIL) {
    return;
  }

  int rowSize = MAX_ENTITIES;
  entityManager->assertMatrix[selectedLinkId * rowSize + assertedNodeId] = assertedResourceId;
}

void set_perform_link_node(EntityManager *entityManager, EntityId selectedLinkId, EntityId performedNodeId, EntityId performedResourceId) {
  if (
      entityManager == NULL ||
      selectedLinkId == NIL ||
      performedNodeId == NIL) {
    return;
  }

  int rowSize = MAX_ENTITIES;
  entityManager->performMatrix[selectedLinkId * rowSize + performedNodeId] = performedResourceId;
}

EntityId get_initial_state_node(EntityManager *entityManager, EntityId nodeId) {
  if (
      entityManager == NULL ||
      nodeId == NIL) {
    return NIL;
  }

  return entityManager->initialState[nodeId];
}

LinkState get_initial_state_link(EntityManager *entityManager, EntityId linkId) {
  if (
      entityManager == NULL ||
      linkId == NIL) {
    return LINK_STATE_NONE;
  }

  return entityManager->initialState[linkId];
}

LinkState get_assert_link_link(EntityManager *entityManager, EntityId selectedLinkId, EntityId id) {
  if (
      entityManager == NULL ||
      selectedLinkId == NIL ||
      id == NIL) {
    return LINK_STATE_NONE;
  }

  int rowSize = MAX_ENTITIES;
  return entityManager->assertMatrix[selectedLinkId * rowSize + id];
}

LinkState get_perform_link_link(EntityManager *entityManager, EntityId selectedLinkId, EntityId id) {
  if (
      entityManager == NULL ||
      selectedLinkId == NIL ||
      id == NIL) {
    return LINK_STATE_NONE;
  }

  int rowSize = MAX_ENTITIES;
  return entityManager->performMatrix[selectedLinkId * rowSize + id];
}

EntityId get_assert_link_node(EntityManager *entityManager, EntityId selectedLinkId, EntityId id) {
  if (
      entityManager == NULL ||
      selectedLinkId == NIL ||
      id == NIL) {
    return NIL;
  }

  int rowSize = MAX_ENTITIES;
  return entityManager->assertMatrix[selectedLinkId * rowSize + id];
}

EntityId get_perform_link_node(EntityManager *entityManager, EntityId selectedLinkId, EntityId id) {
  if (
      entityManager == NULL ||
      selectedLinkId == NIL ||
      id == NIL) {
    return NIL;
  }

  int rowSize = MAX_ENTITIES;
  return entityManager->performMatrix[selectedLinkId * rowSize + id];
}

void clock_tick(Clock *clock) {
  if (clock == NULL) {
    return;
  }

  float now = glfwGetTime();
  clock->dt = now - clock->t;
  clock->t = now;
}

NtVec2f get_mouse_position(OuiContext *ouiContext) {
  double xpos, ypos;
  glfwGetCursorPos(ouiContext->window, &xpos, &ypos);

  int screenWidth = oui_get_window_width(ouiContext);
  int screenHeight = oui_get_window_height(ouiContext);

  NtVec2f mousePos = {
      .x = 2 * (float)xpos - (float)screenWidth / 2,
      .y = -2 * (float)ypos + (float)screenHeight / 2};

  return mousePos;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
  mouse_button_states[button] = action;

  (void)window;
  (void)mods;
}

void keyboard_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
  keyboard_key_states[key] = action;

  (void)window;
  (void)scancode;
  (void)mods;
}
