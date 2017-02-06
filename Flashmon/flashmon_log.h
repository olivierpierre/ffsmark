/*
 * FLASHMON flash memory monitoring tool (Version 2.1)
 * Revision Authors: Pierre Olivier<pierre.olivier@univ-ubs.fr>, Jalil Boukhobza <boukhobza@univ-brest.fr>
 * Contributors: Pierre Olivier, Ilyes Khetib, Crina Arsenie
 *
 * Copyright (c) of University of Occidental Britanny (UBO) <boukhobza@univ-brest.fr>, 2010-2012.
 *
 *	This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * UNIVERSITY OF OCCIDENTAL BRITANNY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. 
 * See the GNU General Public License for more details.
 *
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FLASHMON_LOG_H
#define FLASHMON_LOG_H

#define PROCFS_LOG_NAME           "flashmon_log"
#define FMON_MAX_TASK_NAME_SIZE   16              /* TODO is there some kind of MAX_TASK_NAME macro ? */

/* Log events types */
typedef enum
{
  FMON_READ,
  FMON_WRITE,
  FMON_ERASE,
  FMON_MTD_CACHEHIT,
  FMON_NONE
}fmon_access_type;

/* A log entry */
typedef struct s_fmon_log_entry
{
  struct timespec timestamp;
  fmon_access_type type;
  uint32_t address;
  char *task_name;
} fmon_log_entry;

/* The log */
typedef struct s_fmon_log
{
  struct timespec zeroTime;
  int log_task;
  int size;
  int start;
  int end;
  fmon_log_entry *elems;
} fmon_log;

ssize_t procfile_flashmon_log_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
ssize_t procfile_flashmon_log_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos);
int procfile_flashmon_log_open(struct inode *inode, struct file *filp);
int procfile_flashmon_log_close(struct inode *inode, struct file *filp);
int fmon_insert_event(fmon_access_type event, uint64_t address);
int fmon_log_init(int size_max, int log_task);
int fmon_log_exit(void);
int fmon_log_reset(void);
void fmon_log_enable(void);
void fmon_log_disable(void);
int fmon_log_get_state(void);

#endif /* FLASHMON_LOG_H */
