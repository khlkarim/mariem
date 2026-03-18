#include "tinyfiledialogs.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define NEWTON_IMPLEMENTATION
#include "newton.h"

#define OUI_IMPLEMENTATION
#define OUI_USE_GLFW
#include "oui.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Oui context config
#define FONT "fonts/Geist-VariableFont_wght.ttf"
#define WINDOW_TITLE "UNTITLED"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// Miniaudio device config
#define DEVICE_CHANNELS 2
#define DEVICE_SAMPLE_RATE 48000
#define DEVICE_FORMAT ma_format_f32

// -------------------- StyleSheet --------------------
#define MENU_MARGIN 20
#define DEFAULT_FONT_SIZE 1
#define DEFAULT_LINE_HEIGHT 48
#define DEFAULT_LINK_THICKNESS 10
#define DEFAULT_BORDER_RESOLUTION 50
#define DEFAULT_TEXT_COLOR OUI_COLOR_WHITE
#define DEFAULT_NODE_COLOR OUI_COLOR_BLACK
#define DEFAULT_LINK_COLOR OUI_COLOR_BLACK
#define SELECTED_NODE_COLOR OUI_COLOR_WHITE
// -------------------- StyleSheet --------------------

// ---------------- Physics Parameters ----------------
#define DEFAULT_LINK_ELASTICITY 1.0
#define DEFAULT_LINK_RESTLENGTH 200
#define DEFAULT_NODE_MASSE 0.1 // this value is used to calculate the inv masse too
#define DEFAULT_NODE_RADIUS 25
#define CONSTRAINT_SOLVER_ITERATIONS 1
// ---------------- Physics Parameters ----------------

// ----------------------- Misc -----------------------
#define NUM_NODES 3
#define MAX_ENTITIES 100
#define DEFAULT_BPM 60
// ----------------------- Misc -----------------------

// ------------------ Input Handling ------------------
#define ON 1
#define OFF 0

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
#define NIL 0
typedef int EntityId;

typedef enum EntityType {
  ENTITY_TYPE_NONE,
  ENTITY_TYPE_NODE,
  ENTITY_TYPE_LINK,
  ENTITY_TYPE_COUNT
} EntityType;

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

  int isEnabled;
  int isVisible;
  char *buffer;
  OuiColor color;
  EntityId edgeA;
  EntityId edgeB;

  // TODO: actually this should the first child of the brush entity
  EntityId selectedResource;

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
  Entity entities[MAX_ENTITIES];
  int assertMatrix[MAX_ENTITIES * MAX_ENTITIES];
  int performMatrix[MAX_ENTITIES * MAX_ENTITIES];
  int count;
} EntityManager;

EntityId create_entity(EntityManager *entityManager);
Entity *get_entity(EntityManager *entityManager, EntityId id);
void delete_entity(EntityManager *entityManager, EntityId id);
void append_child(EntityManager *entityManager, EntityId pId, EntityId cId);
void remove_child(EntityManager *entityManager, EntityId pId, EntityId cId); // TODO: implement this when you need it
void link_assert_link(EntityManager *entityManager, EntityId selectedLinkId, EntityId assertedLinkId, int state);
void link_assert_node(EntityManager *entityManager, EntityId selectedLinkId, EntityId assertedNodeId, EntityId assertedResource);
void link_perform_link(EntityManager *entityManager, EntityId selectedLinkId, EntityId performedLinkId, int state);
void link_perform_node(EntityManager *entityManager, EntityId selectedLinkId, EntityId performedNodeId, EntityId performedResource);
// ---------------- Entity Management -----------------

// -------------------- App State ---------------------
// TODO: change the backgroundColor as an indicator of the input mode
typedef enum InputMode {
  INPUT_MODE_NONE,
  INPUT_MODE_ASSERT,
  INPUT_MODE_PERFORM,
  INPUT_MODE_INITIALIZE,
  INPUT_MODE_COUNT
} InputMode;

char *input_mode_labels[INPUT_MODE_COUNT] = {
    [INPUT_MODE_NONE] = "None",
    [INPUT_MODE_ASSERT] = "Assert",
    [INPUT_MODE_PERFORM] = "Perform",
    [INPUT_MODE_INITIALIZE] = "Initialize",
};

typedef struct Clock {
  float t;
  float dt;
} Clock;

typedef struct AppState {
  Clock clock;
  InputMode inputMode;
  ma_engine audioEngine;
  OuiContext ouiContext;
  EntityManager entityManager;

  float BPM;
  int isPlaying;
  float hasBeenPlayingFor;

  int togglePhysics;

  // special entities
  EntityId brushId;
  EntityId fpsTextLabelId;
  EntityId currentlyPlaying;
  EntityId selectedEntityId;
  EntityId selectedLinkId;
  EntityId resourceManagerId;
  EntityId inputModeTextLabelId;
} AppState;

void init(AppState *app);
void update(AppState *app);
void draw(AppState *app);
void clock_tick(Clock *clock);

void create_entities(AppState *app);
EntityId create_node(AppState *app);
EntityId create_sound(AppState *app, char *fileName);
EntityId create_link(AppState *app, EntityId edgeA, EntityId edgeB);

void handle_input(AppState *app);
void handle_key_bindings(AppState *app);
void handle_mouse_click(AppState *app);

int clicked_add_sound(AppState *app);
void resource_manager_click_handler(AppState *app);
EntityId get_clicked_link(AppState *app);
void select_link_handler(AppState *app);
EntityId get_clicked_node(AppState *app);
void select_node_handler(AppState *app);
void select_brush_handler(AppState *app);

void process_audio(AppState *app);
void apply_forces(AppState *app);
void resolve_collisions(AppState *app);

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

  AppState app = {0};
  init(&app);

  while (oui_has_next_frame(&app.ouiContext)) {
    oui_begin_frame(&app.ouiContext);

    update(&app);
    draw(&app);

    oui_end_frame(&app.ouiContext);
  }

  oui_context_destroy(&app.ouiContext);

  (void)argc;
  (void)argv;
  return 0;
}

EntityId create_entity(EntityManager *entityManager) {
  //  TODO: zero set the entity behore returning it maybe

  assert(
      entityManager != NULL &&
      entityManager->count >= 1 &&
      entityManager->count <= MAX_ENTITIES);

  // reserve an empty slot
  for (int i = 1; i < entityManager->count; i++) {
    Entity *e = &(entityManager->entities[i]);

    if (!e->used) {
      e->used = ON;
      return i;
    }
  }

  // create a new slot
  if (entityManager->count < MAX_ENTITIES) {
    Entity *e = &(entityManager->entities[entityManager->count]);

    e->used = ON;
    entityManager->count++;
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

  entityManager->entities[id].used = OFF;
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
  glfwSetInputMode(ouiContext->window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
  glfwSetKeyCallback(ouiContext->window, keyboard_key_callback);
  glfwSetMouseButtonCallback(ouiContext->window, mouse_button_callback);

  // initialize entityManager
  entityManager->count = 1; // at least the NIL entity exists
  create_entities(app);

  // initialize audioEngine
  ma_result result;
  app->BPM = DEFAULT_BPM;
  result = ma_engine_init(NULL, audioEngine);

  if (result != MA_SUCCESS) {
    printf("Failed to initialize audio engine.");
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
  process_audio(app);
  apply_forces(app);
  resolve_collisions(app);
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
    e->position.x = rand() % (int)windowWidth - windowWidth / 2;
    e->position.y = rand() % (int)windowHeight - windowHeight / 2;

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
  brush->color.a = 100;
  brush->width = 400;
  brush->height = 100;
  brush->paddingLeft = 10;
  brush->marginLeft = 10;
  brush->marginBottom = 10;

  app->resourceManagerId = create_entity(entityManager);
  Entity *resourceManager = get_entity(entityManager, app->resourceManagerId);
  resourceManager->radius = 10;
  resourceManager->color = OUI_COLOR_BLACK;
  resourceManager->color.a = 100;
  resourceManager->marginTop = 20;
  resourceManager->marginLeft = 20;
  resourceManager->marginRight = 20;
  resourceManager->marginBottom = 20;
  resourceManager->paddingLeft = 50;
  resourceManager->paddingRight = 50;
  resourceManager->paddingTop = 30;
  resourceManager->paddingBottom = 30;
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
  if (state == ON) {
    keyboard_key_states[KEYBINDING_OPEN_RESOURCE_MANAGER] = OFF;
    resourceManager->isVisible = 1 - resourceManager->isVisible;
  }

  state = keyboard_key_states[KEYBINDING_CREATE_NODE];
  if (state == ON) {
    keyboard_key_states[KEYBINDING_CREATE_NODE] = OFF;
    create_node(app);
  }

  state = keyboard_key_states[KEYBINDING_DELETE_SELECTED];
  if (state == ON) {
    keyboard_key_states[KEYBINDING_DELETE_SELECTED] = OFF;
    delete_entity(entityManager, app->selectedEntityId);
    app->selectedEntityId = NIL;
  }

  state = keyboard_key_states[KEYBINDING_DISABLE_PHYSICS];
  if (state == ON) {
    keyboard_key_states[KEYBINDING_DISABLE_PHYSICS] = OFF;
    app->togglePhysics = 1 - app->togglePhysics;
  }

  state = keyboard_key_states[KEYBINDING_ASSERT_MODE];
  if (state == ON) {
    keyboard_key_states[KEYBINDING_ASSERT_MODE] = OFF;
    app->inputMode = INPUT_MODE_ASSERT;
  }

  state = keyboard_key_states[KEYBINDING_PERFORM_MODE];
  if (state == ON) {
    keyboard_key_states[KEYBINDING_PERFORM_MODE] = OFF;
    app->inputMode = INPUT_MODE_PERFORM;
  }

  state = keyboard_key_states[KEYBINDING_INITIALIZE_MODE];
  if (state == ON) {
    keyboard_key_states[KEYBINDING_INITIALIZE_MODE] = OFF;
    app->inputMode = INPUT_MODE_INITIALIZE;
  }

  state = keyboard_key_states[KEYBINDING_UNSELECT_BRUSH];
  if (state == ON) {
    keyboard_key_states[KEYBINDING_UNSELECT_BRUSH] = OFF;
    Entity *brush = get_entity(entityManager, app->brushId);
    brush->selectedResource = NIL;
  }
}

void handle_mouse_click(AppState *app) {
  resource_manager_click_handler(app);
  select_brush_handler(app);
  select_node_handler(app);
  select_link_handler(app);
  return;

  // find the id of the cliked node
  // based on that do a switch or if else statement

  // what did you click on?
  // based on that trigger a routine
  // how do you know which routine to trigger? => enum Event listeners & switch statement
  // when you click on a node check if a brush is selected

  if (app == NULL) {
    return;
  }

  float dt = app->clock.dt;
  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);
  NtVec2f mousePos = get_mouse_position(ouiContext);

  Entity *brush = get_entity(entityManager, app->brushId);
  Entity *resourceManager = get_entity(entityManager, app->resourceManagerId);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  int state;
  state = mouse_button_states[GLFW_MOUSE_BUTTON_LEFT];
  if (state == ON) {
    // is the resource manager open?
    if (resourceManager->isVisible) {
      mouse_button_states[GLFW_MOUSE_BUTTON_LEFT] = OFF;
      // did the user click the add sound button?
      // or did he click the remove sound button?

      // Add sound button hitbox
      float margin = MENU_MARGIN;
      float paddingLeftRight = 50;
      float paddingTopBottom = 50;

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
        char *title = "Add sound";

        char *fileName = tinyfd_openFileDialog(
            title, /* NULL or "" */
            NULL,  /* NULL or "" , ends with / to set only a directory */
            0,     /* 0 (2 in the following example) */
            NULL,  /* NULL or char const * lFilterPatterns[2]={"*.png","*.jpg"}; */
            NULL,  /* NULL or "image files" */
            0);    /* 0 or 1 */
                   /* in case of multiple files, the separator is | */
                   /* returns NULL on cancel */

        printf("selected file: %s\n", fileName);
        if (fileName) {
          create_sound(app, fileName);
        }
      }
    } else {
      // check if the user clicked:
      //  the brush tool
      //  a node
      //  a link

      // did the user click the brush tool?
      // brush tool hit box
      float brushGap = 10;
      float brushRadius = 50;
      float brushPaddingLeft = 10;
      float brushToolWidth = 400;
      float brushToolHeight = 100;
      float brushToolMarginLeft = 10;
      float brushToolMarginBottom = 10;
      NtVec2f brushToolCenter = {
          .x = -windowWidth / 2 + brushToolMarginLeft + brushToolWidth / 2,
          .y = -windowHeight / 2 + brushToolMarginBottom + brushToolHeight / 2,
      };

      if (
          mousePos.x >= brushToolCenter.x - brushToolWidth / 2 &&
          mousePos.x <= brushToolCenter.x + brushToolWidth / 2 &&
          mousePos.y >= brushToolCenter.y - brushToolHeight / 2 &&
          mousePos.y <= brushToolCenter.y + brushToolHeight / 2) {
        printf("clicked brush box\n");

        // which brush element did you press
        EntityId resourceId = resourceManager->firstChild;
        float baseX = -windowWidth / 2 + brushToolMarginLeft + brushPaddingLeft + brushRadius;
        float baseY = -windowHeight / 2 + brushToolMarginBottom + brushToolHeight / 2;

        while (resourceId != NIL) {
          Entity *resource = get_entity(entityManager, resourceId);

          if (nwt_distance(mousePos, (NtVec2f){baseX, baseY}) <= brushRadius) {
            printf("pressed %d\n", resourceId);
            brush->selectedResource = resourceId;
          }

          baseX += brushRadius + brushGap;
          resourceId = resource->nextSibling;
        }
      } else {
        // did the user select a brush?

        // generate a mouse repeat action to enable dragging
        // if you didnt receive a mouse release event you are still dragging
        // reason: glfw doesnt support GLFW_REPEAT for mouse events
        mouse_button_states[GLFW_MOUSE_BUTTON_LEFT] = GLFW_REPEAT;

        // find the clicked node, if there is any
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

        // there is no currently selected node => select the clicked node
        if (app->selectedEntityId == NIL) {
          // if there is a brush color selected attribute it to this node
          // otherwise select it
          if (brush->selectedResource != NIL) {
            Entity *selectedBrush = get_entity(entityManager, brush->selectedResource);
            Entity *clicked = get_entity(entityManager, clickedNode);
            clicked->color = selectedBrush->color;
          } else {
            app->selectedEntityId = clickedNode;

            Entity *selectedEntity = get_entity(entityManager, app->selectedEntityId);
            selectedEntity->color = SELECTED_NODE_COLOR;
          }
        }
        // if there is a selected node and the user clicked away
        // or reclicked the currently selected entity => unselect it
        else if (
            clickedNode == NIL ||
            clickedNode == app->selectedEntityId) {
          Entity *selectedEntity = get_entity(entityManager, app->selectedEntityId);
          selectedEntity->color = DEFAULT_NODE_COLOR;

          app->selectedEntityId = NIL;
        }
        // there is a currectly selected node
        // and the user clicked another node => create a link (and change focus) TODO: the selected nodes should be an array
        else if (
            clickedNode != NIL &&
            app->selectedEntityId != NIL &&
            app->selectedEntityId != clickedNode) {
          create_link(app, app->selectedEntityId, clickedNode);

          Entity *selectedEntity = get_entity(entityManager, app->selectedEntityId);
          selectedEntity->color = DEFAULT_NODE_COLOR;

          app->selectedEntityId = clickedNode;

          selectedEntity = get_entity(entityManager, app->selectedEntityId);
          selectedEntity->color = SELECTED_NODE_COLOR;
        }
      }
    }
  } else if (state == GLFW_REPEAT) {
    // detect hold action
    float dragSpeed = 10 * dt * 1000;
    Entity *draggedNode = get_entity(entityManager, app->selectedEntityId);

    NtVec2f dir = nwt_sub(mousePos, draggedNode->position);
    float dirLength = nwt_length(dir);

    if (dirLength > 0) {
      dir = nwt_div_s(dir, dirLength);

      draggedNode->force = nwt_add(draggedNode->force, nwt_mult_s(dir, dragSpeed));
      printf("%f, %f\n", draggedNode->force.x, draggedNode->force.y);
    }
  }
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
  if (state == ON) {
    // did the user click the add sound button?
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
      resourceManager->isVisible = OFF;
      return;
    }

    // if he clicked the resourceManager
    // consume the click event
    mouse_button_states[GLFW_MOUSE_BUTTON_LEFT] = OFF;

    // check of the user clicked the Add sound button
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

      char *title = "Add sound";
      char *fileName = tinyfd_openFileDialog(
          title, /* NULL or "" */
          NULL,  /* NULL or "" , ends with / to set only a directory */
          0,     /* 0 (2 in the following example) */
          NULL,  /* NULL or char const * lFilterPatterns[2]={"*.png","*.jpg"}; */
          NULL,  /* NULL or "image files" */
          0);    /* 0 or 1 */
                 /* in case of multiple files, the separator is | */
                 /* returns NULL on cancel */

      printf("selected file: %s\n", fileName);
      if (fileName) {
        create_sound(app, fileName);
      }

      return;
    }

    // check if an X button was clicked
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
  if (state != ON) {
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
        printf("clicked %d\n", i);
        break;
      }
    }
  }

  if (clickedLink != NIL) {
    // if a link was efectively clicked consume the event
    mouse_button_states[GLFW_MOUSE_BUTTON_LEFT] = OFF;
  }

  return clickedLink;
}

void select_link_handler(AppState *app) {
  if (app == NULL) {
    return;
  }

  InputMode mode = app->inputMode;
  EntityManager *entityManager = &(app->entityManager);
  EntityId clickedLinkId = get_clicked_link(app);

  // what mode are we in?

  // in initialization mode
  // if a link is clicked you change focus to it
  if (mode == INPUT_MODE_INITIALIZE && clickedLinkId != NIL) {
    if (app->selectedLinkId != NIL) {
      Entity *e = get_entity(entityManager, app->selectedLinkId);
      e->color = DEFAULT_NODE_COLOR;
    }
    app->selectedLinkId = clickedLinkId;
    Entity *e = get_entity(entityManager, clickedLinkId);
    e->color = SELECTED_NODE_COLOR;
  }
  // in assert mode
  // when a link is clicked you change the assertMatrix
  else if (mode == INPUT_MODE_ASSERT) {
    Entity *e = get_entity(entityManager, clickedLinkId);
    link_assert_link(entityManager, app->selectedLinkId, clickedLinkId, 1 - e->isEnabled);
  }
  // in perform mode
  // when a link is clicked you change the performMatrix
  else if (mode == INPUT_MODE_PERFORM) {
    Entity *e = get_entity(entityManager, clickedLinkId);
    link_perform_link(entityManager, app->selectedLinkId, clickedLinkId, 1 - e->isEnabled);
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
  if (state != ON) {
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
    // if a node was efectively clicked consume the event
    // generate a drag mouse event, and glfw will automatically trigger the release event later
    mouse_button_states[GLFW_MOUSE_BUTTON_LEFT] = GLFW_REPEAT;
  }

  return clickedNode;
}

void select_node_handler(AppState *app) {
  if (app == NULL) {
    return;
  }

  float dt = app->clock.dt;
  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);
  NtVec2f mousePos = get_mouse_position(ouiContext);

  int state;
  state = mouse_button_states[GLFW_MOUSE_BUTTON_LEFT];

  // if the user dragging, check if he is dragging a node
  // and displace it appropriately
  if (state == GLFW_REPEAT) {
    float dragSpeed = 10 * dt * 1000;
    Entity *draggedNode = get_entity(entityManager, app->selectedEntityId);

    NtVec2f dir = nwt_sub(mousePos, draggedNode->position);
    float dirLength = nwt_length(dir);

    if (dirLength > 0) {
      dir = nwt_div_s(dir, dirLength);

      draggedNode->force = nwt_add(draggedNode->force, nwt_mult_s(dir, dragSpeed));
      printf("%f, %f\n", draggedNode->force.x, draggedNode->force.y);
    }

    return;
  }

  if (state != ON) {
    return;
  }

  EntityId nodeId = get_clicked_node(app);
  Entity *brush = get_entity(entityManager, app->brushId);

  // there is no currently selected node
  if (app->selectedEntityId == NIL) {
    // if there is a brush color selected,
    // attribute it to this node
    if (brush->selectedResource != NIL) {
      Entity *selectedBrush = get_entity(entityManager, brush->selectedResource);
      Entity *clicked = get_entity(entityManager, nodeId);
      clicked->color = selectedBrush->color;
    } else {
      // otherwise select it
      app->selectedEntityId = nodeId;

      Entity *selectedEntity = get_entity(entityManager, app->selectedEntityId);
      selectedEntity->color = SELECTED_NODE_COLOR;
    }
  }
  // if there is a selected node and the user clicked away
  // or reclicked the currently selected entity => unselect it
  else if (
      nodeId == NIL ||
      nodeId == app->selectedEntityId) {
    Entity *selectedEntity = get_entity(entityManager, app->selectedEntityId);
    selectedEntity->color = DEFAULT_NODE_COLOR;

    app->selectedEntityId = NIL;
  }
  // there is a currectly selected node
  // and the user clicked another node => create a link (and change focus) TODO: the selected nodes should be an array
  else if (
      nodeId != NIL &&
      app->selectedEntityId != NIL &&
      app->selectedEntityId != nodeId) {
    create_link(app, app->selectedEntityId, nodeId);

    Entity *selectedEntity = get_entity(entityManager, app->selectedEntityId);
    selectedEntity->color = DEFAULT_NODE_COLOR;

    app->selectedEntityId = nodeId;

    selectedEntity = get_entity(entityManager, app->selectedEntityId);
    selectedEntity->color = SELECTED_NODE_COLOR;
  }
}

void select_brush_handler(AppState *app) {
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
  if (state != ON) {
    return;
  }

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
    printf("clicked brush box\n");
    mouse_button_states[GLFW_MOUSE_BUTTON_LEFT] = OFF;

    // which brush element did you press
    EntityId resourceId = resourceManager->firstChild;
    float baseX = -windowWidth / 2 + brushToolMarginLeft + brushPaddingLeft + brushRadius;
    float baseY = -windowHeight / 2 + brushToolMarginBottom + brushToolHeight / 2;

    while (resourceId != NIL) {
      Entity *resource = get_entity(entityManager, resourceId);

      if (nwt_distance(mousePos, (NtVec2f){baseX, baseY}) <= brushRadius) {
        printf("pressed %d\n", resourceId);
        brush->selectedResource = resourceId;
      }

      baseX += brushRadius + brushGap;
      resourceId = resource->nextSibling;
    }
  }
}

void process_audio(AppState *app) {
  if (
      app == NULL ||
      !app->isPlaying ||
      app->currentlyPlaying == NIL) {
    return;
  }

  EntityManager *entityManager = &(app->entityManager);

  int beatDuration = 1 / (app->BPM / 60);
  app->hasBeenPlayingFor += app->clock.dt;

  if (app->hasBeenPlayingFor > beatDuration) {
    Entity *e;
    EntityId next = NIL;

    for (int i = 1; i < entityManager->count; i++) {
      e = get_entity(entityManager, i);

      if (e->type == ENTITY_TYPE_LINK) {
        if (e->edgeA == app->currentlyPlaying) {
          next = e->edgeB;
          break;
        }
      }
    }

    if (next != NIL) {
      app->currentlyPlaying = next;
      e = get_entity(entityManager, next);

      if (e->buffer) {
        ma_engine_play_sound(&app->audioEngine, e->buffer, NULL);
      }
    } else {
      app->currentlyPlaying = NIL;
    }
    app->hasBeenPlayingFor = 0;
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

  // collision forces
  for (int i = 1; i < entityManager->count; i++) {
    Entity *e1 = get_entity(entityManager, i);

    if (e1->type != ENTITY_TYPE_NODE)
      continue;

    // wall collisions
    float r = e1->radius;

    if ((e1->position.x - r < -containerWidth / 2 && e1->velocity.x < 0) ||
        (e1->position.x + r > containerWidth / 2 && e1->velocity.x > 0)) {
      e1->velocity.x *= -1;
    }

    if ((e1->position.y - r < -containerHeight / 2 && e1->velocity.y < 0) ||
        (e1->position.y + r > containerHeight / 2 && e1->velocity.y > 0)) {
      e1->velocity.y *= -1;
    }

    // node on node violence
    for (int j = i + 1; j < entityManager->count; j++) {
      Entity *e2 = get_entity(entityManager, j);

      if (e2->type != ENTITY_TYPE_NODE)
        continue;

      NtVec2f delta = nwt_sub(e1->position, e2->position);
      float deltaLength = nwt_length(delta);

      float r1 = e1->radius;
      float r2 = e2->radius;
      float restLength = r1 + r2;

      if (deltaLength < restLength) {
        float e = 1.0;
        float m1 = e1->masse, m2 = e2->masse;
        NtVec2f u1 = e1->velocity, u2 = e2->velocity;
        NtVec2f p1 = e1->position, p2 = e2->position;
        NtVec2f n = nwt_normalize(delta);

        // formula source: https://www.youtube.com/watch?v=zJLTEt_JFYg
        float coef1 = ((1 + e) * m2 / ((m1 + m2) * restLength)) * nwt_dot(nwt_sub(u2, u1), nwt_sub(p1, p2));
        float coef2 = ((1 + e) * m1 / ((m1 + m2) * restLength)) * nwt_dot(nwt_sub(u1, u2), nwt_sub(p2, p1));

        e1->velocity = nwt_add(e1->velocity, nwt_mult_s(n, coef1));
        e2->velocity = nwt_add(e2->velocity, nwt_mult_s(n, -coef2));
      }
    }

    e1->velocity = nwt_add(e1->velocity, nwt_mult_s(e1->force, e1->invMasse * dt));
    e1->position = nwt_add(e1->position, nwt_mult_s(e1->velocity, dt));
    e1->force = NT_ZERO;
  }
}

void resolve_collisions(AppState *app) {
  if (
      app == NULL ||
      app->togglePhysics == OFF) {
    return;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

  int numIterations = CONSTRAINT_SOLVER_ITERATIONS;

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  float containerWidth = windowWidth;
  float containerHeight = windowHeight;

  for (int i = 0; i < numIterations; i++) {
    // first constraint
    for (int j = 1; j < entityManager->count; j++) {
      Entity *e = get_entity(entityManager, j);

      if (e->type != ENTITY_TYPE_NODE)
        continue;

      e->position.x = oui_max(oui_min(e->position.x, containerWidth / 2), -containerWidth / 2);
      e->position.y = oui_max(oui_min(e->position.y, containerHeight / 2), -containerHeight / 2);
    }

    // second constraint
    for (int j = 1; j < entityManager->count; j++) {
      Entity *e1 = get_entity(entityManager, j);

      if (e1->type != ENTITY_TYPE_NODE)
        continue;

      for (int k = j + 1; k < entityManager->count; k++) {
        Entity *e2 = get_entity(entityManager, k);

        if (e2->type != ENTITY_TYPE_NODE)
          continue;

        NtVec2f delta = nwt_sub(e1->position, e2->position);
        float deltaLength = nwt_length(delta);

        float r1 = e1->radius;
        float r2 = e2->radius;
        float restLength = r1 + r2;

        if (deltaLength < restLength) {
          // if i dont do this it the velocities dont get calculated correctly
          // why??
          float posError = (deltaLength - restLength) * 0.5;
          e1->position = nwt_sub(e1->position, nwt_mult_s(delta, posError / deltaLength));
          e2->position = nwt_add(e2->position, nwt_mult_s(delta, posError / deltaLength));
        }
      }
    }

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
  }
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
  if (app == NULL) {
    return NIL;
  }

  EntityManager *entityManager = &(app->entityManager);

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

EntityId create_sound(AppState *app, char *fileName) {
  if (app == NULL) {
    return NIL;
  }

  EntityManager *entityManager = &(app->entityManager);

  EntityId soundId = create_entity(entityManager);
  Entity *sound = get_entity(entityManager, soundId);

  sound->buffer = fileName; // TODO: use after free?
  sound->color = (OuiColor){
      .r = rand() % 256,
      .g = rand() % 256,
      .b = rand() % 256,
      .a = rand() % 256,
  };

  append_child(entityManager, app->resourceManagerId, soundId);
  return soundId;
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
  snprintf(fpsString, bufferSize, "%.2f FPS", 1 / clock->dt);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  Entity *fpsTextLabel = get_entity(entityManager, app->fpsTextLabelId);

  int fpsStringLength = strlen(fpsString);
  fpsTextLabel->position.y = (float)windowHeight / 2 - marginTop;
  fpsTextLabel->position.x = (float)windowWidth / 2 - characterWidth * fpsStringLength - marginRight;

  fpsTextLabel->buffer = fpsString;
  draw_label(app, app->fpsTextLabelId);
  fpsTextLabel->buffer = NULL;
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
  snprintf(inputModeString, bufferSize, "%s", input_mode_labels[app->inputMode]);

  float windowWidth, windowHeight;
  windowWidth = oui_get_window_width(ouiContext);
  windowHeight = oui_get_window_height(ouiContext);

  Entity *inputModeTextLabel = get_entity(entityManager, app->inputModeTextLabelId);

  inputModeTextLabel->position.x = -(float)windowWidth / 2 + marginLeft;
  inputModeTextLabel->position.y = (float)windowHeight / 2 - marginTop;

  inputModeTextLabel->buffer = inputModeString;
  draw_label(app, app->inputModeTextLabelId);
  inputModeTextLabel->buffer = NULL;
}

void draw_node(AppState *app, EntityId id) {
  if (app == NULL) {
    return;
  }

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

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

  rectangle.backgroundColor = e->color;
  oui_draw_rectangle(ouiContext, &rectangle);
}

void draw_link(AppState *app, EntityId id) {
  if (app == NULL) {
    return;
  }

  float arrowSize = 10;
  float arrowBorderRadius = 3;

  OuiContext *ouiContext = &(app->ouiContext);
  EntityManager *entityManager = &(app->entityManager);

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
  rectangle.borderResolution = 0;

  rectangle.backgroundColor = e->color;
  oui_draw_rectangle(ouiContext, &rectangle);
  // Draw the link's body

  // Draw the arrow
  rectangle.width = arrowSize;
  rectangle.height = DEFAULT_LINK_THICKNESS;
  rectangle.borderRadius = arrowBorderRadius;

  // the direction orthogonal to the current link direction
  NtVec2f orthogonal = {
      .x = -normalizedDiff.y,
      .y = normalizedDiff.x};
  nwt_normalize(orthogonal);

  // rotation relative to the current link direction
  rectangle.rotationXY += -NT_PI / 4;

  rectangle.centerPos.x = b->position.x + normalizedDiff.x * (b->radius + rectangle.width / 2 - 2) + 5 * orthogonal.x;
  rectangle.centerPos.y = b->position.y + normalizedDiff.y * (b->radius + rectangle.width / 2 - 2) + 5 * orthogonal.y;
  oui_draw_rectangle(ouiContext, &rectangle);

  // rotation relative to the current link direction
  rectangle.rotationXY += NT_PI / 2;
  rectangle.centerPos.x = b->position.x + normalizedDiff.x * (b->radius + rectangle.width / 2 - 2) - 5 * orthogonal.x;
  rectangle.centerPos.y = b->position.y + normalizedDiff.y * (b->radius + rectangle.width / 2 - 2) - 5 * orthogonal.y;
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

  OuiRectangle rectangle = {0};

  // draw the container's background
  rectangle.width = paletteWidth;
  rectangle.height = paletteHeight;
  rectangle.borderRadius = brush->radius;
  rectangle.backgroundColor = brush->color;
  rectangle.centerPos.x = -windowWidth / 2 + marginLeft + paletteWidth / 2;
  rectangle.centerPos.y = -windowHeight / 2 + marginBottom + paletteHeight / 2;
  oui_draw_rectangle(ouiContext, &rectangle);

  // draw the already loaded resources
  EntityId resourceId = resourceManager->firstChild;
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

    if (brush->selectedResource == resourceId) {
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
  float margin = MENU_MARGIN;
  float paddingTopBottom = 30;
  float paddingLeftRight = 50;

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
