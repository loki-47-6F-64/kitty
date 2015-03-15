#ifndef DOSSIER_MOVE_BY_COPY_H
#define DOSSIER_MOVE_BY_COPY_H

#include <utility>
namespace util {
/*
 * When a copy is made, it moves the object
 * This allows you to move an object when a move can't be done.
 */
template<class T>
class MoveByCopy {
public:
  typedef T move_type;
private:
  move_type _to_move;
public:

  MoveByCopy(move_type &&to_move) : _to_move(std::move(to_move)) { }

  MoveByCopy(const MoveByCopy &other) {
    this->operator=(other);
  }

  void operator=(const MoveByCopy &other) {
    this->_to_move = std::move(other._to_move);
  }

  operator move_type() {
    return std::move(_to_move);
  }
};

template<class T>
MoveByCopy<T> cmove(T &&movable) {
  return MoveByCopy<T>(std::move(movable));
}

}
#endif
