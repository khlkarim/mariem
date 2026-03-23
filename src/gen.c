#define NOB_IMPLEMENTATION
#include "nob.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BUFFER_SIZE 1024

#define NUM_RESOURCES 10
#define NUM_NODES 10
#define NUM_LINKS 10

#define NUM_ASSERTS 10
#define NUM_PERFORMS 10

#define OUTPUT_DIRECTORY "examples/generated/"
#define OUTPUT_FILE "examples/generated/02.txt"

#define RESOURCE_DIRECTORIES_COUNT 2
const char *RESOURCE_DIRECTORIES[] = {
    "./data",
};

typedef struct MyData {
  size_t count;
  size_t stopAt;
  char selectedFile[BUFFER_SIZE];
} MyData;

void create_resource(FILE *outputFile, const char **resourceDirectories, int numResourceDirectories);
void create_node(FILE *outputFile, int numResources);
void create_link(FILE *outputFile, int numNodes);

void define_assert_statement(FILE *outputFile, int numLinks, int numNodes, int numResources);
void define_perform_statement(FILE *outputFile, int numLinks, int numNodes, int numResources);

bool count_files(Nob_Walk_Entry entry);
bool get_file_name_at(Nob_Walk_Entry entry);
int pick_a_random_file(const char **directories, size_t numDirectories, char *resultBuffer, size_t bufferSize);

int main(int argc, char **argv) {
  srand(time(NULL));

  size_t count = 0;
  if (!nob_walk_dir(OUTPUT_DIRECTORY, count_files, .data = &count)) {
    return 1;
  }

  char outputFileName[BUFFER_SIZE];
  snprintf(outputFileName, BUFFER_SIZE, OUTPUT_DIRECTORY "%zu.txt", count + 1);

  FILE *outputFile = fopen(outputFileName, "w");

  // create the resources
  for (int i = 0; i < NUM_RESOURCES; i++) {
    create_resource(outputFile, RESOURCE_DIRECTORIES, RESOURCE_DIRECTORIES_COUNT);
  }

  // create the resources
  for (int i = 0; i < NUM_NODES; i++) {
    create_node(outputFile, NUM_RESOURCES);
  }

  // create the links
  for (int i = 0; i < NUM_LINKS; i++) {
    create_link(outputFile, NUM_NODES);
  }

  // define assert statements
  for (int i = 0; i < NUM_LINKS; i++) {
    define_assert_statement(outputFile, NUM_LINKS, NUM_NODES, NUM_RESOURCES);
  }

  // define perform statements
  for (int i = 0; i < NUM_LINKS; i++) {
    define_perform_statement(outputFile, NUM_LINKS, NUM_NODES, NUM_RESOURCES);
  }

  fclose(outputFile);
  (void)argc;
  (void)argv;
  return 0;
}

void create_resource(FILE *outputFile, const char **resourceDirectories, int numResourceDirectories) {
  if (outputFile == NULL || resourceDirectories == NULL) {
    return;
  }

  char fileName[BUFFER_SIZE];
  pick_a_random_file(
      resourceDirectories,
      numResourceDirectories,
      fileName, BUFFER_SIZE);

  float colorR = rand() % 256;
  float colorG = rand() % 256;
  float colorB = rand() % 256;
  float colorA = 256;

  fprintf(outputFile, "R %f %f %f %f %s\n", colorR, colorG, colorB, colorA, fileName);
}

void create_node(FILE *outputFile, int numResources) {
  if (outputFile == NULL) {
    return;
  }

  int isNeedle = rand() % 2;
  int hasResource = rand() % 2;
  int resourceId = hasResource == 1 ? (rand() % numResources) + 1 : 0;

  fprintf(outputFile, "N %d %d\n", resourceId, isNeedle);
}

void create_link(FILE *outputFile, int numNodes) {
  if (outputFile == NULL) {
    return;
  }

  int state = rand() % 3;
  int aId = (rand() % numNodes) + 1;
  int bId = (rand() % numNodes) + 1;

  while (aId == bId) {
    aId = (rand() % numNodes) + 1;
    bId = (rand() % numNodes) + 1;
  }

  fprintf(outputFile, "L %d %d %d\n", aId, bId, state);
}

void define_assert_statement(FILE *outputFile, int numLinks, int numNodes, int numResources) {
  if (outputFile == NULL) {
    return;
  }

  int isAssertNode = rand() % 2;

  if (isAssertNode == 1) {
    int linkId = (rand() % numLinks) + 1;
    int nodeId = (rand() % numNodes) + 1;
    int resourceId = (rand() % numResources) + 1;

    fprintf(outputFile, "A N %d %d %d\n", linkId, nodeId, resourceId);
  } else {
    int linkId = (rand() % numLinks) + 1;
    int otherLinkId = (rand() % numLinks) + 1;
    int state = (rand() % 2) + 1;

    fprintf(outputFile, "A L %d %d %d\n", linkId, otherLinkId, state);
  }
}

void define_perform_statement(FILE *outputFile, int numLinks, int numNodes, int numResources) {
  if (outputFile == NULL) {
    return;
  }

  int isPerformNode = rand() % 2;

  if (isPerformNode == 1) {
    int linkId = (rand() % numLinks) + 1;
    int nodeId = (rand() % numNodes) + 1;
    int resourceId = (rand() % numResources) + 1;

    fprintf(outputFile, "P N %d %d %d\n", linkId, nodeId, resourceId);
  } else {
    int linkId = (rand() % numLinks) + 1;
    int otherLinkId = (rand() % numLinks) + 1;
    int state = (rand() % 2) + 1;

    fprintf(outputFile, "P L %d %d %d\n", linkId, otherLinkId, state);
  }
}

int pick_a_random_file(const char **directories, size_t numDirectories, char *resultBuffer, size_t bufferSize) {
  if (directories == NULL || resultBuffer == NULL) {
    return 0;
  }

  size_t count = 0;

  for (size_t i = 0; i < numDirectories; i++) {
    size_t curr = 0;

    if (!nob_walk_dir(directories[i], count_files, .data = &curr)) {
      return 1;
    }

    count += curr;
    nob_log(INFO, "%s contains %zu files", RESOURCE_DIRECTORIES[i], curr);
  }

  nob_log(NOB_INFO, "accumulated file count %zu", count);

  MyData data = {
      .count = 0,
      .stopAt = (rand() % count) + 1,
  };

  for (size_t i = 0; i < numDirectories; i++) {
    size_t curr = 0;

    if (!nob_walk_dir(directories[i], count_files, .data = &curr)) {
      return 1;
    }

    if (curr >= data.stopAt) {
      if (!nob_walk_dir(directories[i], get_file_name_at, .data = &data)) {
        return 1;
      }

      break;
    } else {
      data.stopAt -= curr;
    }
  }

  snprintf(resultBuffer, bufferSize, "%s", data.selectedFile);
  nob_log(NOB_INFO, "selected file: %s", data.selectedFile);

  return 0;
}

bool count_files(Nob_Walk_Entry entry) {
  size_t *count = (size_t *)entry.data;

  nob_log(INFO, "%*s%s", (int)entry.level * 2, "", entry.path);
  if (entry.type == FILE_REGULAR) {
    *count += 1;
  }

  return true;
}

bool get_file_name_at(Nob_Walk_Entry entry) {
  MyData *data = (MyData *)entry.data;
  nob_log(INFO, "%*s%s", (int)entry.level * 2, "", entry.path);

  if (entry.type == FILE_REGULAR) {
    data->count += 1;

    if (data->count == data->stopAt) {
      snprintf(data->selectedFile, BUFFER_SIZE, "%s", entry.path);
    }
  }

  return true;
}
