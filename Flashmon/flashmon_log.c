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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#include "flashmon.h"
#include "flashmon_log.h"

#define MAX_RECEIVED_SIZE   32

struct proc_dir_entry *proc_file_flashmon_log;

static fmon_log log;

struct file_operations fops_flashmon_log = 
{  
	.owner = THIS_MODULE,
	.read = procfile_flashmon_log_read,
  .write = procfile_flashmon_log_write,
	.open = procfile_flashmon_log_open,
	.release = procfile_flashmon_log_close,
};

int flashmon_log_enable;

void fmon_log_enable(void)
{
	flashmon_log_enable = 1;
}

void fmon_log_disable(void)
{
	flashmon_log_enable = 0;
}

int fmon_log_get_state()
{
	return flashmon_log_enable;
}

/**
 * \fn int fmon_log_init(int size_max)
 * \brief Log initialization
 * \param size_max Max number of (circular) log entries
 */
int fmon_log_init(int size_max, int log_task)
{
	int i;
	
  log.size = size_max+1;    /* keep one slot open */
  log.start = 0;
  log.end = 0;
  log.log_task = log_task;
  
	log.elems = vmalloc(log.size*sizeof(fmon_log_entry));

	
	for(i=0; i<log.size; i++)
  {
		log.elems[i].type = FMON_NONE;
    /***
     * when using vmalloc here the kernel hangs and complain about oom on
     * some embedded systems.
     */
    if(log_task)
      log.elems[i].task_name = kmalloc(FMON_MAX_TASK_NAME_SIZE*sizeof(char), GFP_KERNEL);
  }
  
  /* /proc entry */
  proc_file_flashmon_log = proc_create(PROCFS_LOG_NAME, S_IWUGO | S_IRUGO, NULL, &fops_flashmon_log);
  
  /* set time zero */
  getnstimeofday(&log.zeroTime);
	
	return 0;
}

/**
 * \fn int fmon_log_exit(void)
 * \brief Log cleaning function called when removing flashmon
 */
int fmon_log_exit(void)
{
  int i;
  
  printk(PRINT_PREF "Dumping log.\n");
  
  for(i=0; i<log.size; i++)
    kfree(log.elems[i].task_name);
  
  vfree(log.elems);
  remove_proc_entry(PROCFS_LOG_NAME, NULL);
  return 0;
}

/**
 * \fn int fmon_log_reset(void)
 * \brief Reset the log
 */
int fmon_log_reset(void)
{
	int i;
	
	log.start = 0;
	log.end = 0;
	for(i=0; i<log.size; i++)
		log.elems[i].type = FMON_NONE;
		
	return 0;
}

/**
 * \fn int fmon_log_is_full(void)
 * \brief Return 1 if the log is full
 */
int fmon_log_is_full(void)
{
  return (log.end + 1) % log.size == log.start;
}

/**
 * \fn int fmon_log_is_empty(void)
 * \brief Return 1 if the log is empty
 */
int fmon_log_is_empty(void)
{
  return (log.end == log.start);
}

/**
 * \fn int fmon_insert_event(fmon_access_type event, uint64_t address)
 * \brief Add an event to the log
 * \param event Type of the event (R/W/E)
 * \param address Address target (flash page read / written number, block number for erase
 */
int fmon_insert_event(fmon_access_type event, uint64_t address)
{
  getnstimeofday(&(log.elems[log.end].timestamp));
  
  log.elems[log.end].type = event;
  log.elems[log.end].address = address;
  
  if(log.log_task)
    strcpy(log.elems[log.end].task_name, current->comm);
  
  log.end = (log.end+1)%log.size;
  if(log.end == log.start)
    log.start = (log.start + 1)%log.size;
      
  return 0;
}

/**
 * \fn ssize_t procfile_flashmon_log_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
 * \fn /proc/flashmon_log read function
 */
ssize_t procfile_flashmon_log_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
  int bytes_read, line_len, ok;
  static int fini = 0;
  char line[128];
  fmon_log_entry *cur;
  
  /* Log vide ? */
  if(fmon_log_is_empty())
    return 0;
    
  /* termine ? */
  if(fini)
  {
    fini = 0;
    return 0;
  }
    
  memset(buf, 0x00, size);
  line[0]='\0';
  ok=1;
  bytes_read = 0;
  cur = &log.elems[(log.start + (int)(*ppos)) % log.size];
  while(ok)
  {
    struct timespec ts_tmp;
    
    if(cur->type == FMON_NONE)
    {
      fini = 1;
      break;
    }
    
    ts_tmp = timespec_sub(cur->timestamp, log.zeroTime);
    sprintf(line, "%lu.%.9lu;", ts_tmp.tv_sec, ts_tmp.tv_nsec);
    if(cur->type == FMON_ERASE)
      sprintf(line, "%s%s", line, "E;");
    else if(cur->type == FMON_READ)
      sprintf(line, "%s%s", line, "R;");
    else if(cur->type == FMON_WRITE)
      sprintf(line, "%s%s", line, "W;");
    else if(cur->type == FMON_MTD_CACHEHIT)
      sprintf(line, "%s%s", line, "C;");
      
    sprintf(line, "%s%u", line, cur->address);
    
    if(log.log_task)
      sprintf(line, "%s;%s", line, cur->task_name);
    
    sprintf(line, "%s\n", line);
    
    line_len = strlen(line);
    
    if((line_len + 1 + bytes_read) > size)
      break;
    
    sprintf(buf, "%s%s", buf, line);
    bytes_read += line_len;
    (*ppos)++;
    
    cur = &log.elems[(log.start + (int)(*ppos)) % log.size];
    
    if(cur == &log.elems[log.end])
    {
      fini = 1;
      break;
    }
    
  }
  
  return bytes_read;
}

ssize_t procfile_flashmon_log_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
  char received[MAX_RECEIVED_SIZE];
  int ret;
  
  (void)ret;
  
  if(size > MAX_RECEIVED_SIZE)
    return size;
    
  ret = copy_from_user(received, buf, size);
  
  if(!strncmp(received, "reset", strlen("reset")))
  {
    fmon_log_reset();
  }
  else if(!strncmp(received, "timereset", strlen("timereset")))
  {
    fmon_log_reset();
    getnstimeofday(&log.zeroTime);
  }
  else if(!strncmp(received, "start", strlen("start")))
  {
    flashmon_log_enable = 1;
  }
  else if(!strncmp(received, "stop", strlen("stop")))
  {
    flashmon_log_enable = 0;
  }
  else
  {
    printk(PRINT_PREF "Unrecognized command : %s\n", received);
  }
  
  return size;
}

/**
 * \fn int procfile_flashmon_log_open(struct inode *inode, struct file *filp)
 * \brief /proc/flashmon_log open function
 */
int procfile_flashmon_log_open(struct inode *inode, struct file *filp)
{
  return 0;
}

/**
 * \fn int procfile_flashmon_log_close(struct inode *inode, struct file *filp)
 * \brief /proc/flashmon_log close function
 */
int procfile_flashmon_log_close(struct inode *inode, struct file *filp)
{
  return 0;
}
