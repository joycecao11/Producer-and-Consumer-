#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

struct Agent {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->mutex   = uthread_mutex_create();
  agent->paper   = uthread_cond_create (agent->mutex);
  agent->match   = uthread_cond_create (agent->mutex);
  agent->tobacco = uthread_cond_create (agent->mutex);
  agent->smoke   = uthread_cond_create (agent->mutex);
  return agent;
}

//
// TODO
// You will probably need to add some procedures and struct etc.
uthread_cond_t tobaccoAndPaper;
uthread_cond_t matchAndPaper;
uthread_cond_t tobaccoAndMatch;
int t = 0;
int p = 0;
int m = 0;

/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};
int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked

void* smokerWithTobacco(void* av){
  uthread_mutex_lock(((struct Agent*) av)->mutex);
  VERBOSE_PRINT ("in smoker with tobacco\n");
  struct Agent* a = (struct Agent*) av;
  while(1){
    while(p==0 || m==0){
      VERBOSE_PRINT ("--->waiting for match and paper\n");
      uthread_cond_wait(matchAndPaper);
    }
    VERBOSE_PRINT ("Smoker with tobacco gets paper and match to smoke\n");
    t = 0;
    m = 0;
    p = 0;
    uthread_cond_signal(a->smoke);
    smoke_count[TOBACCO] ++;
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

void* smokerWithPaper(void* av){
  uthread_mutex_lock(((struct Agent*) av)->mutex);
  VERBOSE_PRINT ("in smoker with paper\n");
  struct Agent* a = (struct Agent*) av;
  while(1){
    while(t==0 || m==0){
      VERBOSE_PRINT ("--->waiting for tobacco and match\n");
      uthread_cond_wait(tobaccoAndMatch);
    }
    VERBOSE_PRINT ("Smoker with tobacco gets paper and match to smoke\n");
    t = 0;
    m = 0;
    p = 0;
    uthread_cond_signal(a->smoke);
    smoke_count[PAPER] ++;
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

void* smokerWithMatch(void* av){
  uthread_mutex_lock(((struct Agent*) av)->mutex);
  VERBOSE_PRINT ("in smoker with match\n");
  struct Agent* a = (struct Agent*) av;
  while(1){
    while(p==0 || t==0){
      VERBOSE_PRINT ("--->waiting for tobacco and paper\n");
      uthread_cond_wait(tobaccoAndPaper);
    }
    VERBOSE_PRINT ("Smoker with match gets paper and tobacco to smoke\n");
    t = 0;
    m = 0;
    p = 0;
    uthread_cond_signal(a->smoke);
    smoke_count[MATCH] ++;
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

void* waitForTobacco(void* av){
  uthread_mutex_lock(((struct Agent*) av)->mutex);
  VERBOSE_PRINT ("in wait for tobacco\n");
  struct Agent* a = (struct Agent*) av;
  while(1){
    VERBOSE_PRINT ("--->waiting for tobacco\n");
    uthread_cond_wait(a->tobacco);
    VERBOSE_PRINT ("--->signaled by tobacco\n");
    t = 1;
    if(p==1){
      VERBOSE_PRINT ("--->signaling tobacco and paper\n");
      uthread_cond_signal(tobaccoAndPaper);
    }
    else if(m==1){
      VERBOSE_PRINT ("--->signaling tobacco and match\n");
      uthread_cond_signal(tobaccoAndMatch);
    }
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}
void* waitForPaper(void* av){
  uthread_mutex_lock(((struct Agent*) av)->mutex);
  VERBOSE_PRINT ("in wait for paper\n");
  struct Agent* a = (struct Agent*) av;
  while(1){
    VERBOSE_PRINT ("--->waiting for paper\n");
    uthread_cond_wait(a->paper);
    VERBOSE_PRINT ("--->signaled by paper\n");
    p = 1;
    if(t==1){
      VERBOSE_PRINT ("--->signaling tobacco and paper\n");
      uthread_cond_signal(tobaccoAndPaper);
    }
    else if(m==1){
      VERBOSE_PRINT ("--->signaling match and paper\n");
      uthread_cond_signal(matchAndPaper);
    }
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}
void* waitForMatch(void* av){
  uthread_mutex_lock(((struct Agent*) av)->mutex);
  VERBOSE_PRINT ("in wait for match\n");
  struct Agent* a = (struct Agent*) av;
  while(1){
    VERBOSE_PRINT ("--->waiting for match\n");
    uthread_cond_wait(a->match);
    VERBOSE_PRINT ("--->signaled by match\n");
    m = 1;
    if(t==1){
      VERBOSE_PRINT ("--->signaling tobacco and match\n");
      uthread_cond_signal(tobaccoAndMatch);
    }
    else if(p==1){
      VERBOSE_PRINT ("--->signaling match and paper\n");
      uthread_cond_signal(matchAndPaper);
    }
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
  struct Agent* a = av;
  static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};

  uthread_mutex_lock (a->mutex);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      int r = random() % 3;
      signal_count [matching_smoker [r]] ++;
      int c = choices [r];
      if (c & MATCH) {
        VERBOSE_PRINT ("match available\n");
        uthread_cond_signal (a->match);
      }
      if (c & PAPER) {
        VERBOSE_PRINT ("paper available\n");
        uthread_cond_signal (a->paper);
      }
      if (c & TOBACCO) {
        VERBOSE_PRINT ("tobacco available\n");
        uthread_cond_signal (a->tobacco);
      }
      VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
      uthread_cond_wait (a->smoke);
    }
  uthread_mutex_unlock (a->mutex);
  return NULL;
}

int main (int argc, char** argv) {
  uthread_init (7);
  //create agent
  struct Agent*  a = createAgent();
  //init three dual-condition
  tobaccoAndPaper = uthread_cond_create(a->mutex);
  matchAndPaper = uthread_cond_create(a->mutex);
  tobaccoAndMatch = uthread_cond_create(a->mutex);

  // TODO
  uthread_create (waitForTobacco, a);
  uthread_create (waitForPaper, a);
  uthread_create (waitForMatch, a);
  uthread_create (smokerWithTobacco, a);
  uthread_create (smokerWithPaper, a);
  uthread_create (smokerWithMatch, a);


  uthread_join (uthread_create (agent, a), 0);
  //add more threads


  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);

}
