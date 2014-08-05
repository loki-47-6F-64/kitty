#ifndef DOSSIER_ERR_H
#define DOSSIER_ERR_H


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
  LIB_GAI,
  LIB_SYS,
  LIB_SSL
} code_t;

extern thread_local code_t code;

};
#endif