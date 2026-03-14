#define NEWTON_IMPLEMENTATION
#include "newton.h"

#define OUI_IMPLEMENTATION
#define OUI_USE_GLFW
#include "oui.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_LINKS 1
#define NUM_NODES 3
#define NUM_ENTITIES 1000
#define MAX_ENTITIES 1000

#define LINK_THICKNESS 8
#define NODE_BORDER_RESOLUTION 50
#define CONSTRAINT_SOLVER_ITERATIONS 2

typedef enum Action {
  ACTION_NONE,
  ACTION_CREATE_NODE,
  ACTION_DELETE_NODE,
  ACTION_CREATE_LINK,
  ACTION_DELETE_LINK,
  ACTION_COUNT
} Action;

char *actions_labels[ACTION_COUNT] = {
    [ACTION_NONE] = "none",
    [ACTION_CREATE_NODE] = "create a node",
    [ACTION_DELETE_NODE] = "delete a node",
    [ACTION_CREATE_LINK] = "create a link",
    [ACTION_DELETE_LINK] = "delete a link",
};

typedef enum EntityType {
  ENTITY_TYPE_NONE,
  ENTITY_TYPE_NODE,
  ENTITY_TYPE_LINK,
  ENTITY_TYPE_ACTIONS_MENU,
  ENTITY_TYPE_UI_CONTROLS_LIST,
  ENTITY_TYPE_UI_CONTROLS_LIST_OPTION,
  ENTITY_TYPE_UI_CONTROLS_TEXT_INPUT,
  ENTITY_TYPE_COUNT
} EntityType;

typedef int EntityId;

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

  int isBeingDragged;
  NtVec2f targetPosition;

  int isEnabled;
  int isVisible;
  Action actions[ACTION_COUNT];

  OuiColor color;
  EntityId firstChild;
  EntityId nextSibling;
} Entity;

typedef struct EntityManager {
  Entity entities[MAX_ENTITIES];
  int count;
} EntityManager;

Entity *create_entity(EntityManager *entityManager);
Entity *get_entity(EntityManager *entityManager, EntityId id);
void delete_entity(EntityManager *entityManager, EntityId id);

/*void create_entities(OuiContext *ouiContext, Entity *entities, int count);
void update_entities(OuiContext *ouiContext, Entity *entities, int count, float dt);
void draw_entities(OuiContext *ouiContext, Entity *entities, int count);*/

void create_entities(OuiContext *ouiContext, EntityManager *entityManager);
void update_entities(OuiContext *ouiContext, EntityManager *entityManager, float dt);
void draw_entities(OuiContext *ouiContext, EntityManager *entityManager);

NtVec2f get_mouse_position(OuiContext *ouiContext);
void process_input(OuiContext *ouiContext, Entity *entities, int count, float dt);
void apply_forces(OuiContext *ouiContext, Entity *entities, int count, float dt);
void resolve_collisions(OuiContext *ouiContext, Entity *entities, int count, float dt);

void draw_node(OuiContext *ouiContext, Entity *entities, int count, EntityId id);
void draw_link(OuiContext *ouiContext, Entity *entities, int count, EntityId id);
void draw_list(OuiContext *ouiContext, Entity *entities, int count, EntityId id);

int main() {
  srand(time(NULL));

  OuiConfig ouiConfig = {
      .windowTitle = "test",
      .windowWidth = 480,
      .windowHeight = 480,
  };

  OuiContext *ouiContext = oui_context_create(&ouiConfig);

  EntityManager entityManager = {0};
  create_entities(ouiContext, &entityManager);

  float prev_t = glfwGetTime(), curr_t, delta_t;
  while (oui_has_next_frame(ouiContext)) {
    curr_t = glfwGetTime();
    delta_t = curr_t - prev_t;
    prev_t = curr_t;

    oui_begin_frame(ouiContext);

    update_entities(ouiContext, &entityManager, delta_t);
    draw_entities(ouiContext, &entityManager);

    oui_end_frame(ouiContext);
  }

  oui_context_destroy(ouiContext);
  return 0;
}

Entity *create_entity(EntityManager *entityManager) {
  assert(entityManager != NULL && entityManager->count <= MAX_ENTITIES);
  Entity *e = &(entityManager->entities[0]);

  for (int i = 1; i < entityManager->count; i++) {
    e = &(entityManager->entities[i]);

    if (!e->used) {
      e->used = 1;
      return e;
    }
  }

  if (entityManager->count < MAX_ENTITIES) {
    e = &(entityManager->entities[entityManager->count]);
    entityManager->count++;

    e->used = 1;
    return e;
  }

  return e;
}

Entity *get_entity(EntityManager *entityManager, EntityId id) {
  assert(entityManager != NULL && entityManager->count <= MAX_ENTITIES);

  if (id < 0 || id >= entityManager->count) {
    return &(entityManager->entities[0]);
  }

  return &(entityManager->entities[id]);
}

void delete_entity(EntityManager *entityManager, EntityId id) {
  assert(entityManager != NULL && entityManager->count <= MAX_ENTITIES);

  if (id < 0 || id >= entityManager->count) {
    return;
  }

  entityManager->entities[id].used = 0;
}

void create_entities(OuiContext *ouiContext, Entity *entities, int count) {
  OuiColor colors[3] = {
      {255, 0.0, 0.0, 255},
      {0.0, 255.0, 0.0, 255},
      {0.0, 0.0, 255.0, 255},
  };

  int numNodes = NUM_NODES, numLinks = NUM_LINKS;

  int screenWidth = oui_get_window_width(ouiContext);
  int screenHeight = oui_get_window_height(ouiContext);

  float masse = 0.1;
  float radius = 50;
  float baseX = -(float)screenWidth / 2, baseY = (float)screenHeight / 2;

  for (int i = 0; i < numNodes; i++) {
    Entity *e = &entities[i];

    e->type = ENTITY_TYPE_NODE;

    e->radius = radius;
    e->position.x = rand() % 400 - 200;
    e->position.y = rand() % 400 - 200;

    e->velocity.x = rand() % 200 - 100;
    e->velocity.y = rand() % 200 - 100;

    e->masse = masse;
    e->invMasse = 1.0 / masse;

    e->color = colors[i % 3];

    baseX += 2 * radius + 10;
    if (baseX > (float)screenWidth * 3 / 4) {
      baseX = -(float)screenWidth / 2;
      baseY -= 2 * radius + 10;
    }
  }

  for (int i = numNodes; i - numNodes < numLinks; i++) {
    Entity *e = &entities[i];

    e->type = ENTITY_TYPE_LINK;

    e->elasticity = 0.1;
    e->restLength = 100;

    e->firstChild = 0;
    entities[e->firstChild].nextSibling = 1;

    e->color = (OuiColor){255, 255, 255, 255};
  }

  // ui controls
  int baseIdx = numNodes + numLinks;
  Entity *userActionList = &(entities[baseIdx]);

  userActionList->type = ENTITY_TYPE_UI_CONTROLS_LIST;
  userActionList->isVisible = 0;
  userActionList->radius = 20;
  userActionList->color.a = 100;

  for (Action i = 0; i < ACTION_COUNT; i++) {
    int currIdx = baseIdx + i + 1;
    Entity *userActionListOption = &(entities[currIdx]);

    userActionListOption->type = ENTITY_TYPE_UI_CONTROLS_LIST_OPTION;
    userActionListOption->actions[i] = 1;

    if (i == 0) {
      userActionList->firstChild = currIdx;
    }
    if (i < ACTION_COUNT - 1) {
      userActionListOption->nextSibling = currIdx + 1;
    }
  }
}

void update_entities(OuiContext *ouiContext, Entity *entities, int count, float delta_t) {
  process_input(ouiContext, entities, count, delta_t);
  apply_forces(ouiContext, entities, count, delta_t);
  resolve_collisions(ouiContext, entities, count, delta_t);
}

void draw_entities(OuiContext *ouiContext, Entity *entities, int count) {
  for (int i = 0; i < count; i++) {
    Entity *e = &entities[i];

    if (e->type == ENTITY_TYPE_NODE) {
      draw_node(ouiContext, entities, count, i);
    } else if (e->type == ENTITY_TYPE_LINK) {
      draw_link(ouiContext, entities, count, i);
    } else if (e->type == ENTITY_TYPE_UI_CONTROLS_LIST) {
      draw_list(ouiContext, entities, count, i);
    }
  }
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

Action get_pressed_action(OuiContext *ouiContext, Entity *entities, int count, NtVec2f clickPos) {
  Entity *e = NULL;
  for (int i = 0; i < count; i++) {
    e = &(entities[i]);

    if (e->type == ENTITY_TYPE_UI_CONTROLS_LIST) {
      break;
    }
  }

  int margin = 20;
  int screenWidth = oui_get_window_width(ouiContext);
  int screenHeight = oui_get_window_height(ouiContext);

  int baseY = (float)screenHeight / 2 - 100;
  OuiText optionLabel = {0};

  Entity *listOption = &(entities[e->firstChild]);
  while (listOption) {
    optionLabel.startX = -(float)screenWidth / 2 + 2 * margin;
    optionLabel.startY = baseY;
    baseY -= 120;

    optionLabel.lineHeight = 48;
    optionLabel.maxLineWidth = screenWidth - 4 * margin;

    for (Action i = 0; i < ACTION_COUNT && fabs(clickPos.y - optionLabel.startY) < 60; i++) {
      if (listOption->actions[i]) {
        return i;
      }
    }

    if (listOption->nextSibling == 0) {
      listOption = NULL;
    } else {
      listOption = &(entities[listOption->nextSibling]);
    }
  }

  return ACTION_NONE;
}

void handle_action(Action action) {
  printf("%s\n", actions_labels[action]);
}

void process_input(OuiContext *ouiContext, Entity *entities, int count, float delta_t) {
  int state;
  Entity *actionsMenu = NULL;
  NtVec2f mousePos = get_mouse_position(ouiContext);

  float minDist = -1;
  Entity *draggedNode = NULL, *closestNode = NULL;

  for (int i = 0; i < count; i++) {
    Entity *e = &(entities[i]);

    if (e->type == ENTITY_TYPE_NODE) {
      if (e->isBeingDragged) {
        draggedNode = e;
      } else {
        float distance = nwt_distance(mousePos, e->position);

        if (distance <= e->radius) {
          if (minDist == -1) {
            minDist = distance;
          }
          if (distance <= minDist) {
            closestNode = e;
          }
        }
      }
    } else if (e->type == ENTITY_TYPE_UI_CONTROLS_LIST) {
      actionsMenu = e;
    }
  }

  state = keyboard_key_states[GLFW_KEY_Q];
  if (state == 1 && actionsMenu) {
    actionsMenu->isVisible = 1 - actionsMenu->isVisible;
    keyboard_key_states[GLFW_KEY_Q] = 0;
  }

  state = mouse_button_states[GLFW_MOUSE_BUTTON_LEFT];

  if (state == 1 && actionsMenu && actionsMenu->isVisible) {
    Action action = get_pressed_action(ouiContext, entities, count, mousePos);
    handle_action(action);
  } else if (state == 1) {
    if (!draggedNode) {
      closestNode->isBeingDragged = 1;
      draggedNode = closestNode;
    }

    if (draggedNode) {
      float speed = 10 * delta_t * 1000;

      NtVec2f dir = nwt_sub(mousePos, draggedNode->position);
      float dirLength = nwt_length(dir);

      float damp = 0.001 * dirLength;
      dir = nwt_div_s(dir, dirLength);
      draggedNode->force = nwt_add(draggedNode->force, nwt_mult_s(dir, speed * damp));
      printf("%f, %f\n", draggedNode->force.x, draggedNode->force.y);
    }
  } else if (state == 0) {
    if (draggedNode) {
      draggedNode->isBeingDragged = 0;
    }
  }
}

void apply_forces(OuiContext *ouiContext, Entity *entities, int count, float delta_t) {
  float containerWidth = oui_get_window_width(ouiContext);
  float containerHeight = oui_get_window_height(ouiContext);

  // collision forces
  for (int i = 0; i < count; i++) {
    Entity *e1 = &entities[i];
    float r = e1->radius;

    if (e1->type != ENTITY_TYPE_NODE)
      continue;

    // wall collisions
    if ((e1->position.x - r < -containerWidth / 2 && e1->velocity.x < 0) ||
        (e1->position.x + r > containerWidth / 2 && e1->velocity.x > 0)) {
      e1->velocity.x *= -1;
    }

    if ((e1->position.y - r < -containerHeight / 2 && e1->velocity.y < 0) ||
        (e1->position.y + r > containerHeight / 2 && e1->velocity.y > 0)) {
      e1->velocity.y *= -1;
    }

    // node on node violence
    for (int j = i + 1; j < count; j++) {
      Entity *e2 = &entities[j];

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

    e1->velocity = nwt_add(e1->velocity, nwt_mult_s(e1->force, e1->invMasse * delta_t));
    e1->position = nwt_add(e1->position, nwt_mult_s(e1->velocity, delta_t));
    e1->force = NT_ZERO;
  }
}

void resolve_collisions(OuiContext *ouiContext, Entity *entities, int count, float delta_t) {
  int numIterations = CONSTRAINT_SOLVER_ITERATIONS;

  float containerWidth = oui_get_window_width(ouiContext);
  float containerHeight = oui_get_window_height(ouiContext);

  for (int i = 0; i < numIterations; i++) {
    // first constraint
    for (int j = 0; j < count; j++) {
      Entity *e = &entities[j];

      if (e->type != ENTITY_TYPE_NODE)
        continue;

      e->position.x = max(min(e->position.x, containerWidth / 2), -containerWidth / 2);
      e->position.y = max(min(e->position.y, containerHeight / 2), -containerHeight / 2);
    }

    // second constraint
    for (int j = 0; j < count; j++) {
      Entity *e1 = &entities[j];

      if (e1->type != ENTITY_TYPE_NODE)
        continue;

      for (int k = j + 1; k < count; k++) {
        Entity *e2 = &entities[k];

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

    for (int j = 0; j < count; j++) {
      Entity *e = &entities[j];

      if (e->type != ENTITY_TYPE_LINK)
        continue;

      Entity *a = &entities[e->firstChild];
      Entity *b = &(entities[entities[e->firstChild].nextSibling]);

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

void draw_node(OuiContext *ouiContext, Entity *entities, int count, EntityId id) {
  Entity *e = &(entities[id]);

  OuiRectangle rectangle = {0};

  rectangle.width = 2 * e->radius;
  rectangle.height = 2 * e->radius;

  rectangle.centerPos = (OuiVec2f){
      .x = e->position.x,
      .y = e->position.y,
  };

  rectangle.borderRadius = e->radius;
  rectangle.borderResolution = NODE_BORDER_RESOLUTION;

  rectangle.backgroundColor = e->color;

  oui_draw_rectangle(ouiContext, &rectangle);
}

void draw_link(OuiContext *ouiContext, Entity *entities, int count, EntityId id) {
  Entity *e = &(entities[id]);

  Entity *a = &(entities[e->firstChild]);
  Entity *b = &(entities[entities[e->firstChild].nextSibling]);

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
  rectangle.height = LINK_THICKNESS;

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
}

void draw_list(OuiContext *ouiContext, Entity *entities, int count, EntityId id) {
  Entity *e = &(entities[id]);

  if (!e->isVisible)
    return;

  int margin = 20;
  int screenWidth = oui_get_window_width(ouiContext);
  int screenHeight = oui_get_window_height(ouiContext);

  OuiRectangle rectangle = {0};

  rectangle.width = screenWidth - 2 * margin;
  rectangle.height = screenHeight - 2 * margin;

  rectangle.centerPos = (OuiVec2f){
      .x = e->position.x,
      .y = e->position.y,
  };

  rectangle.borderRadius = e->radius;
  rectangle.borderResolution = NODE_BORDER_RESOLUTION;

  rectangle.backgroundColor = e->color;

  oui_draw_rectangle(ouiContext, &rectangle);

  int baseY = (float)screenHeight / 2 - 100;
  OuiText optionLabel = {0};

  Entity *listOption = &(entities[e->firstChild]);
  while (listOption) {
    optionLabel.startX = -(float)screenWidth / 2 + 2 * margin;
    optionLabel.startY = baseY;
    baseY -= 120;

    optionLabel.lineHeight = 48;
    optionLabel.maxLineWidth = screenWidth - 4 * margin;

    optionLabel.fontSize = 1;
    optionLabel.fontColor = (OuiColor){255, 255, 255, 255};

    for (Action i = 0; i < ACTION_COUNT; i++) {
      if (listOption->actions[i]) {
        optionLabel.content = actions_labels[i];
      }
    }

    oui_draw_text(ouiContext, &optionLabel);

    if (listOption->nextSibling == 0) {
      listOption = NULL;
    } else {
      listOption = &(entities[listOption->nextSibling]);
    }
  }
}
