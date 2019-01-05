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

#include <string.h> /* strlen() */

#include <FindDirectory.h> /* find_path() */
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


void uv_loadavg(double avg[3]) {
  avg[0] = 0;
  avg[1] = 0;
  avg[2] = 0;
}


int uv_exepath(char* buffer, size_t* size) {
  status_t status;

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  status = find_path(B_APP_IMAGE_SYMBOL, B_FIND_PATH_IMAGE_PATH, NULL, buffer,
                     *size);
  if (status != B_OK)
    return UV__ERR(status);

  *size = strlen(buffer);

  return 0;
}


uint64_t uv_get_free_memory(void) {
  status_t status;
  system_info sinfo;

  status = get_system_info(&sinfo);
  if (status != B_OK)
    return 0;

  return (sinfo.max_pages - sinfo.used_pages) * B_PAGE_SIZE;
}


uint64_t uv_get_total_memory(void) {
  status_t status;
  system_info sinfo;

  status = get_system_info(&sinfo);
  if (status != B_OK)
    return 0;

  return sinfo.max_pages * B_PAGE_SIZE;
}


int uv_resident_set_memory(size_t* rss) {
  area_info area;
  ssize_t cookie;
  status_t status;
  thread_info thread;

  status = get_thread_info(find_thread(NULL), &thread);
  if (status != B_OK)
    return UV__ERR(status);

  cookie = 0;
  *rss = 0;
  while (get_next_area_info(thread.team, &cookie, &area) == B_OK)
    *rss += area.ram_size;

  return 0;
}


int uv_uptime(double* uptime) {
  /* system_time() returns time since booting in microseconds */
  *uptime = (double)system_time() / 1000000;
  return 0;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  cpu_topology_node_info* topology_infos;
  int i;
  status_t status;
  system_info system;
  uint32_t topology_count;
  uint64_t cpuspeed;

  if (cpu_infos == NULL || count == NULL)
    return UV_EINVAL;

  status = get_cpu_topology_info(NULL, &topology_count);
  if (status != B_OK)
    return UV__ERR(status);

  topology_infos = uv__malloc(topology_count * sizeof(cpu_topology_node_info));
  if (topology_infos == NULL)
    return UV_ENOMEM;

  status = get_cpu_topology_info(topology_infos, &topology_count);
  if (status != B_OK) {
    uv__free(topology_infos);
    return UV__ERR(status);
  }

  cpuspeed = 0;
  for (i = 0; i < (int)topology_count; i++)
    if (topology_infos[i].type == B_TOPOLOGY_CORE) {
      cpuspeed = topology_infos[i].data.core.default_frequency;
      break;
    }

  uv__free(topology_infos);

  status = get_system_info(&system);
  if (status != B_OK) {
    return UV__ERR(status);
  }

  *cpu_infos = uv__calloc(system.cpu_count, sizeof(**cpu_infos));
  if (*cpu_infos == NULL) {
    return UV_ENOMEM;
  }

  *count = system.cpu_count;

  /* CPU time is not exposed by Haiku, neither does the model name. */
  for (i = 0; i < (int)system.cpu_count; i++)
    (*cpu_infos)[i].speed = cpuspeed;

  return 0;
}

void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  uv__free(cpu_infos);
}
