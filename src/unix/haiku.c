/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#include <OS.h>

/* (c) 2001, 2002, François Revol (mmu_man), released under the MIT license.
 *
 * The algorithm for conversion between BeOS priority and Unix nice was taken
 * from Haiku's renice tool.
 *
 * BeOs priorities:
 * Realtime  High  Default  Low Prio
 * 120       99    10       1 (0 only for idle_thread)
 *
 * UNIX nice:
 *           -20    0       19
 */

#define NZERO 0

#define BZERO B_NORMAL_PRIORITY
#define BMIN  (B_REAL_TIME_DISPLAY_PRIORITY - 1)
#define BMAX  1

/* returns an equivalent UNIX nice for a given BeOS priority. */
static int priority_beos_to_unix(int priority) {
  if (priority > BZERO)
    return NZERO - ((priority - BZERO) * NZERO) / (BMIN - BZERO);
  return NZERO + ((BZERO - priority) * (NZERO - 1)) / (BZERO - BMAX);
}

/* returns an equivalent BeOS priority for a given UNIX nice. */
static int priority_unix_to_beos(int priority) {
  if (priority > NZERO)
    return BZERO - ((priority - NZERO) * (BZERO - BMAX)) / (NZERO - 1);
  return BZERO + ((NZERO - priority) * (BMIN - BZERO)) / (NZERO);
}

int uv_os_getpriority(uv_pid_t pid, int* priority) {
  thread_info tinfo;
  status_t status;

  if (priority == NULL)
    return UV_EINVAL;

  status = get_thread_info(pid, &tinfo);
  if (status != B_OK)
    return UV__ERR(status);

  *priority = priority_beos_to_unix(tinfo.priority)
  return 0;
}

int uv_os_setpriority(uv_pid_t pid, int priority) {
  if (priority < UV_PRIORITY_HIGHEST || priority > UV_PRIORITY_LOW)
    return UV_EINVAL;

  status = set_thread_priority(pid, priority_unix_to_beos(priority));
  if (status != B_OK)
    return UV__ERR(status);

  return 0;
}
