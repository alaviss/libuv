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

/* The algorithm below has been adapted from Haiku's renice tool
 *
 * (c) 2001, 2002, François Revol (mmu_man), released under the MIT license.
 *
 * BeOS priorities:
 * Realtime  Highest  Default  Low
 * 120       99       10       1
 *
 * libuv priorities:
 *           -20      0       19
 */

static int beos_to_uv_priority(int priority) {
  if (priority > B_NORMAL_PRIORITY)
    return UV_PRIORITY_NORMAL -
      (priority - B_NORMAL_PRIORITY) *
      (UV_PRIORITY_NORMAL - UV_PRIORITY_HIGHEST) /
      (B_REAL_TIME_PRIORITY - B_NORMAL_PRIORITY);
  return UV_PRIORITY_NORMAL +
    (priority - B_NORMAL_PRIORITY) *
    (UV_PRIORITY_LOW - UV_PRIORITY_NORMAL) /
    (B_NORMAL_PRIORITY - B_LOWEST_ACTIVE_PRIORITY);
}

static int uv_to_beos_priority(int priority) {
  if (priority < UV_PRIORITY_NORMAL)
    return B_NORMAL_PRIORITY +
      (priority - UV_PRIORITY_NORMAL) *
      (B_REAL_TIME_DISPLAY_PRIORITY - B_NORMAL_PRIORITY) /
      (UV_PRIORITY_NORMAL - UV_PRIORITY_HIGHEST);
  return B_NORMAL_PRIORITY +
    (priority - UV_PRIORITY_NORMAL) *
    (B_NORMAL_PRIORITY - B_LOWEST_ACTIVE_PRIORITY) /
    (UV_PRIORITY_LOW - UV_PRIORITY_NORMAL);
}

int uv_os_getpriority(uv_pid_t pid, int* priority) {
  thread_info tinfo;
  status_t status;

  if (priority == NULL)
    return UV_EINVAL;

  status = get_thread_info(pid, &tinfo);
  if (status != B_OK)
    return UV__ERR(status);

  *priority = beos_to_uv_priority(tinfo.priority);
  return 0;
}

int uv_os_setpriority(uv_pid_t pid, int priority) {
  status_t status;

  if (priority < UV_PRIORITY_HIGHEST || priority > UV_PRIORITY_LOW)
    return UV_EINVAL;

  status = set_thread_priority(pid, uv_to_beos_priority(priority));
  if (status != B_OK)
    return UV__ERR(status);

  return 0;
}
