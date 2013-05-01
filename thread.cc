//  Copyright (C) 2012 Henner Zeller <h.zeller@acm.org>
//    
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "thread.h"

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

void *Thread::PthreadCallRun(void *tobject) {
  reinterpret_cast<Thread*>(tobject)->Run();
  return NULL;
}

Thread::Thread() : started_(false) {}
Thread::~Thread() {
  if (!started_) return;
  int result = pthread_join(thread_, NULL);
  if (result != 0) {
    fprintf(stderr, "err code: %d %s\n", result, strerror(result));
  }
}

void Thread::Start(int realtime_priority) {
  assert(!started_);
  pthread_create(&thread_, NULL, &PthreadCallRun, this);

  if (realtime_priority > 0) {
    struct sched_param p;
    p.sched_priority = realtime_priority;
    pthread_setschedparam(thread_, SCHED_FIFO, &p);
  }

  started_ = true;
}
