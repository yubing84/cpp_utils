#ifndef CONDITION_VARIABLE_HPP
#define CONDITION_VARIABLE_HPP

#include "Mutex.hpp"
#include <pthread.h>  // pthread_cond

class ConditionVariable
{

public:

  ConditionVariable(const Mutex* const mutex);
  ~ConditionVariable();

  void wait(const int interval = 0);
  void signal();
  void broadcast();

private:

  ConditionVariable(const ConditionVariable&);
  ConditionVariable& operator=(const ConditionVariable&);

  const Mutex* const m_mutex;
  pthread_cond_t m_condVar;

};

#endif // CONDITION_VARIABLE_HPP
