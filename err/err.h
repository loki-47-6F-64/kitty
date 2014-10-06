#ifndef DOSSIER_ERR_H
#define DOSSIER_ERR_H

#include <string>
namespace err {
/*
 * Thread safe error functions
 */
const char *current();

typedef enum {
  OK,
  TIMEOUT,
  BREAK, // Break from eachByte()
  FILE_CLOSED,
  OUT_OF_BOUNDS,
  INPUT_OUTPUT,
  UNAUTHORIZED,
  LIB_GAI,
  LIB_SYS,
  LIB_SSL
} code_t;

extern thread_local code_t code;

void set(const char *error);
void set(std::string &error);
void set(std::string &&error);
};
#endif