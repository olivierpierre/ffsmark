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

/**
 * \file core.c
 * \brief Kernel module for tracing flash page reads and writes, and 
 * block erase operations
 * \date 03/22/2013
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>	/* probes */
#include <linux/init.h>
#include <linux/vmalloc.h>	/* vmalloc() */
#include <linux/mtd/mtd.h>	/* mtd_info and erase_info structures */
#include <linux/proc_fs.h>	/* /proc entry */
#include <linux/fs.h>
#include <linux/mtd/nand.h>	/* nand_write and nand_read */
#include <linux/sched.h>	/* userspace signal */
#include <linux/signal.h>
#include <linux/pid.h>		/* userpace process task struct */
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/mtd/partitions.h>

#include "flashmon.h"
#include "flashmon_log.h"
#include "flashmon_finder.h"

#define PROCFS_NAME         "flashmon"
#define MAX_RECEIVED_SIZE   32


/* Various global variables */
uint64_t FLASH_SIZE = -1;
int NAND_PAGE_SIZE = -1;	/* Flash page size (bytes) */
int PAGE_PER_BLOCK = -1;	/* Number of pages per block */
int BLOCK_NUM = -1;		/* Number of blocks */
int NAND_BLOCK_SIZE = -1;	/* block size in bytes */

/* (Flash block access) counters */
uint32_t *read_tab;		/* Reads */
uint32_t *write_tab;		/* Writes */
uint32_t *erase_tab;		/* Erase operations */

/* Monitoring enabled ? */
int flashmon_enabled = 1;

/* Used to log the time of events */
struct timespec tv;

/* /proc entries pointers */
struct proc_dir_entry *proc_file_flashmon;

/* Define module parameters */
int PROG_PID = 0;		/* Userspace PID to notify */
int TRACED_PART = -1;
int LOG_TASK = 1;		/* TODO put 0 here by default */
int LOG_MTD_CACHE_HITS = 1;

#ifdef CONFIG_MTD_NAND_FLASHMON_LOG
int LOG_MODE = CONFIG_MTD_NAND_FLASHMON_LOG;
#else
int LOG_MODE = 1024;		/* Log events in /var/log/messages, 0 
				 * disable log, and a positive integer enable 
				 * log and specifies the size in terms of 
				 * number of entries for the log. The size of 
				 * one log entry is sizeof(struct s_fmon_log)*/
#endif

module_param (PROG_PID, int, 0);
MODULE_PARM_DESC (PROG_PID, "Userspace PID to notify");
module_param (LOG_MODE, int, 0);
MODULE_PARM_DESC (LOG_MODE, "Log mode 1=on 0=off");
module_param (TRACED_PART, int, 0);
MODULE_PARM_DESC (TRACED_PART, "Traced partition index, -1=all");
module_param (LOG_TASK, int, 0);
MODULE_PARM_DESC (LOG_TASK,
		  "Insert for each event in the log the name of the current task at the time of the event");
module_param (LOG_MTD_CACHE_HITS, int, 0);
MODULE_PARM_DESC (LOG_MTD_CACHE_HITS, "Log MTD cache hits");	/* todo more info here */

uint64_t traced_part_offset;
uint64_t traced_part_size;
int first_part_block, last_part_block;

/* Prototypes managing /proc entries */
ssize_t procfile_flashmon_read (struct file *file, char __user * buf,
				size_t size, loff_t * ppos);
ssize_t procfile_flashmon_write (struct file *file, const char __user * buf,
				 size_t size, loff_t * ppos);
int procfile_flashmon_open (struct inode *inode, struct file *filp);
int procfile_flashmon_close (struct inode *inode, struct file *filp);

/**
 * \fn void fire_signal(void)
 * \brief Sends a SIGALRM (14) signal to userspace, targetting the 
 * process PROG_PID
 */
void
fire_signal (void)
{
  /* Pid and task struct of the targetted process */
  struct task_struct *ts;
  struct pid *p;

  /* No PID -> do nothing */
  if (!PROG_PID)
    return;

  p = find_get_pid (PROG_PID);

  /* Process not found */
  if (p == NULL)
    {
      printk (PRINT_PREF "Error : process (%d) not found\n", PROG_PID);
      return;
    }

  /* Get the task struct */
  ts = pid_task (p, PIDTYPE_PID);

  /* Send signal */
  send_sig_info (14, (struct siginfo *) (1), ts);

  return;
}

/* Fops for the /proc entries : */
struct file_operations fops_flashmon = {
  .owner = THIS_MODULE,
  .read = procfile_flashmon_read,
  .write = procfile_flashmon_write,
  .open = procfile_flashmon_open,
  .release = procfile_flashmon_close,
};

static int
jgeneric_read_page (struct mtd_info *mtd, struct nand_chip *chip,
		    uint8_t * buf, int page)
{
  loff_t from = page * NAND_PAGE_SIZE;
  int block = page / PAGE_PER_BLOCK;

  if (!flashmon_enabled)
    {
      jprobe_return ();
      return 0;
    }

  if (TRACED_PART != -1)
    {
      int traced_part_hit = (from >= traced_part_offset)
	&& (from < (traced_part_offset + traced_part_size));
      if (!traced_part_hit)
	{
	  jprobe_return ();
	  return 0;
	}
    }

  if (page == chip->pagebuf)
    {
      if (LOG_MTD_CACHE_HITS)
	fmon_insert_event (FMON_MTD_CACHEHIT, (uint64_t) page);
      jprobe_return ();
      return 0;
    }

  read_tab[block]++;
  if (LOG_MODE && fmon_log_get_state ())
    fmon_insert_event (FMON_READ, (uint64_t) page);

  fire_signal ();
  jprobe_return ();
  return 0;
}

static int
jgeneric_write_page (struct mtd_info *mtd, struct nand_chip *chip,
		     const uint8_t * buf, int page, int cached, int raw)
{
  loff_t to = page * NAND_PAGE_SIZE;
  int block = page / PAGE_PER_BLOCK;

  if (!flashmon_enabled)
    {
      jprobe_return ();
      return 0;
    }

  if (TRACED_PART != -1)
    {
      int traced_part_hit = (to >= traced_part_offset)
	&& (to < (traced_part_offset + traced_part_size));
      if (!traced_part_hit)
	{
	  jprobe_return ();
	  return 0;
	}
    }

  write_tab[block]++;

  if (LOG_MODE && fmon_log_get_state ())
    fmon_insert_event (FMON_WRITE, (uint64_t) page);

  fire_signal ();
  jprobe_return ();
  return 0;
}

//~ static int jnand_write (struct mtd_info * mtd, loff_t to, size_t len, 
	//~ size_t * retlen, const u_char * buf)
//~ {
	//~ int first_page_hit, last_page_hit, nb_pages_hit, i, block;
	//~ uint64_t tmp;
	//~ 
  //~ if(!flashmon_enabled)
  //~ {
    //~ jprobe_return();
    //~ return 0;
  //~ }
	//~ 
	//~ if(TRACED_PART != -1)
	//~ {
		//~ int traced_part_hit = (to >= traced_part_offset) && (to < (traced_part_offset+traced_part_size));
		//~ if(!traced_part_hit)
		//~ {
			//~ jprobe_return();
			//~ return 0;
		//~ }
	//~ }
	//~ 
	//~ /* compute first page hit */
	//~ tmp = to;
	//~ do_div(tmp, NAND_PAGE_SIZE);
	//~ first_page_hit = (int)tmp;
	//~ 
	//~ /* compute last page hit */
	//~ tmp = to + len - 1;
	//~ do_div(tmp, NAND_PAGE_SIZE);
	//~ last_page_hit = (int)tmp;
	//~ 
	//~ /* compute num. of page hit */
	//~ nb_pages_hit = last_page_hit - first_page_hit + 1;
	//~ 
	//~ for(i=0; i<nb_pages_hit; i++)
	//~ {
		//~ int page = first_page_hit+i;
		//~ block=page/PAGE_PER_BLOCK;
		//~ write_tab[block]++;
		//~ if(LOG_MODE && fmon_log_get_state())
			//~ fmon_insert_event(FMON_WRITE, (uint64_t)page);
	//~ }
	//~ 
  //~ fire_signal();
//~ 
  //~ jprobe_return();
  //~ return 0;
//~ }

/**
 * YAFFS2 use nand_read_oob
 */
static int
jnand_read_oob (struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops)
{
  int first_page_hit, last_page_hit, nb_pages_hit, i, block;
  uint64_t tmp;
  struct nand_chip *chip = mtd->priv;
  size_t len;

  if (ops->datbuf == NULL)
    {
      jprobe_return ();
      return 0;
    }

  if (!flashmon_enabled)
    {
      jprobe_return ();
      return 0;
    }

  if (TRACED_PART != -1)
    {
      int traced_part_hit = (from >= traced_part_offset)
	&& (from < (traced_part_offset + traced_part_size));
      if (!traced_part_hit)
	{
	  jprobe_return ();
	  return 0;
	}
    }

  /* Set the length */
  len = ops->len;

  /* compute first page hit */
  tmp = from;
  do_div (tmp, NAND_PAGE_SIZE);
  first_page_hit = (int) tmp;

  /* compute last page hit */
  tmp = from + len - 1;
  do_div (tmp, NAND_PAGE_SIZE);
  last_page_hit = (int) tmp;

  /* compute num. of page hit */
  nb_pages_hit = last_page_hit - first_page_hit + 1;

  for (i = 0; i < nb_pages_hit; i++)
    {
      int page = first_page_hit + i;
      /* is the page in the page buffer ? */
      if (page == chip->pagebuf)
	continue;
      block = page / PAGE_PER_BLOCK;
      read_tab[block]++;
      if (LOG_MODE && fmon_log_get_state ())
	fmon_insert_event (FMON_READ, (uint64_t) page);
    }

  fire_signal ();

  jprobe_return ();
  return 0;
}

static int
jnand_read (struct mtd_info *mtd, loff_t from, size_t len,
	    size_t * retlen, uint8_t * buf)
{
  int first_page_hit, last_page_hit, nb_pages_hit, i, block;
  uint64_t tmp;
  struct nand_chip *chip = mtd->priv;

  if (!flashmon_enabled)
    {
      jprobe_return ();
      return 0;
    }

  if (TRACED_PART != -1)
    {
      int traced_part_hit = (from >= traced_part_offset)
	&& (from < (traced_part_offset + traced_part_size));
      if (!traced_part_hit)
	{
	  jprobe_return ();
	  return 0;
	}
    }

  /* compute first page hit */
  tmp = from;
  do_div (tmp, NAND_PAGE_SIZE);
  first_page_hit = (int) tmp;

  /* compute last page hit */
  tmp = from + len - 1;
  do_div (tmp, NAND_PAGE_SIZE);
  last_page_hit = (int) tmp;

  /* compute num. of page hit */
  nb_pages_hit = last_page_hit - first_page_hit + 1;

  for (i = 0; i < nb_pages_hit; i++)
    {
      int page = first_page_hit + i;
      /* is the page in the page buffer ? */
      if (page == chip->pagebuf)
	{
	  if (LOG_MTD_CACHE_HITS)
	    fmon_insert_event (FMON_MTD_CACHEHIT, (uint64_t) page);
	  continue;
	}
      block = page / PAGE_PER_BLOCK;
      read_tab[block]++;
      if (LOG_MODE && fmon_log_get_state ())
	fmon_insert_event (FMON_READ, (uint64_t) page);
    }

  fire_signal ();

  jprobe_return ();
  return 0;
}

/** 
 * \fn static int jnand_erase(struct mtd_info *mtd, struct erase_info *instr)
 * \brief Handler on the erase function (nand_erase)
 */
static int
jnand_erase (struct mtd_info *mtd, struct erase_info *instr)
{
  int blk_size;
  int addr;
  int blk_num;

  if (!flashmon_enabled)
    {
      jprobe_return ();
      return 0;
    }

  if (TRACED_PART != -1)
    {
      int traced_part_hit = (instr->addr >= traced_part_offset)
	&& (instr->addr < (traced_part_offset + traced_part_size));
      if (!traced_part_hit)
	{
	  jprobe_return ();
	  return 0;
	}
    }

  blk_size = (int) (mtd->erasesize);
  addr = (int) (instr->addr);
  blk_num = addr / blk_size;

  /* +1 on corresponding entry */
  if (blk_num < BLOCK_NUM)
    erase_tab[blk_num]++;
  else
    printk (PRINT_PREF "Warning : accessed block %d > %d\n", blk_num,
	    BLOCK_NUM);

  if (LOG_MODE && fmon_log_get_state ())
    fmon_insert_event (FMON_ERASE, (uint64_t) blk_num);

  fire_signal ();
  jprobe_return ();
  return 0;
}

/**
 * Jprobes :
 * \def my_jprobe_read
 * Jprobe on nand_read() (read)
 * \def my_jprobe_write
 * Jprobe on nand_write(), (write)
 * \def my_jprobe_erase
 * Jprobe on erase function nand_erase()
 */

/* Read page with OOB */
static struct jprobe my_jprobe_read_oob = {
  .entry = jnand_read_oob,
  .kp = {
	 .symbol_name = "nand_read_oob",
	 },
};

/* Read page */
static struct jprobe my_jprobe_read = {
  .entry = jgeneric_read_page,
};

/* Write page */
static struct jprobe my_jprobe_write = {
  .entry = jgeneric_write_page,
};

/* Erase block */
static struct jprobe my_jprobe_erase = {
  .entry = jnand_erase,
};

/**
 * \fn static int __init mod_init(void)
 * \brief Mondule initialization
 * We do a lot of things here, might want to break that code into 
 * several functions ...
 */
static int __init
mod_init (void)
{
  int ret, i, old_read_func;
  struct mtd_info *mtd, *master, *mtd2;	/* Information on traced device */
  struct mtd_part *part, *part2;
  struct pid *p;
  uint64_t tmp_blk_num;
  unsigned int readfunc;
  unsigned int writefunc;
  unsigned int erasefunc;

  printk (PRINT_PREF "Flashmon 2.1 module loading ...\n");
  printk (PRINT_PREF "===============================\n");

	/**
	 * First we try to find the functions involved in flash accesses. The
	 * "finder" module is in charge of this task (see finder.c)
	 */
  old_read_func = find_funcs (&readfunc, &writefunc, &erasefunc);
  if (old_read_func < 0)
    {
      printk (PRINT_PREF "Error finder\n");
      return -1;
    }

  /* set jprobe entry addrs */
#ifdef __LP64__
  my_jprobe_erase.kp.addr = (void *) (0xffffffff00000000 | erasefunc);
  my_jprobe_write.kp.addr = (void *) (0xffffffff00000000 | writefunc);
  my_jprobe_read.kp.addr = (void *) (0xffffffff00000000 | readfunc);
#else
  my_jprobe_erase.kp.addr = (void *) erasefunc;
  my_jprobe_write.kp.addr = (void *) writefunc;
  my_jprobe_read.kp.addr = (void *) readfunc;
#endif

  /* old version of read_page_hwecc & co, fallback to nand_read_page */
  if (old_read_func == 1)
    {
      printk (PRINT_PREF "Old kernel version, falling back on nand_read "
	      "for the probed read function");
      my_jprobe_read.entry = jnand_read;
    }

  /* Get infos on traced flash device : */
  mtd = get_mtd_device (NULL, 0);
  if (mtd == NULL)
    {
      printk (PRINT_PREF "Error : Cannot get mtd device\n");
      return -1;
    }
  part = PART (mtd);
  master = part->master;

  FLASH_SIZE = master->size;
  NAND_PAGE_SIZE = master->writesize;
  PAGE_PER_BLOCK = master->erasesize / master->writesize;
  NAND_BLOCK_SIZE = PAGE_PER_BLOCK * NAND_PAGE_SIZE;
  tmp_blk_num = FLASH_SIZE;
  do_div (tmp_blk_num, (uint64_t) master->erasesize);
  BLOCK_NUM = (int) tmp_blk_num;

  /* It's important to allocate the arrays __before__ registering the 
   * probes !
   * Allocation + init arrays */
  read_tab = (int *) vmalloc ((BLOCK_NUM + 1) * sizeof (int));
  write_tab = (int *) vmalloc ((BLOCK_NUM + 1) * sizeof (int));
  erase_tab = (int *) vmalloc ((BLOCK_NUM + 1) * sizeof (int));
  for (i = 0; i < BLOCK_NUM; i++)
    {
      read_tab[i] = 0;
      write_tab[i] = 0;
      erase_tab[i] = 0;
    }

  /* This is also true for log data objects */
  if (LOG_MODE > 0)
    {
      fmon_log_enable ();
      fmon_log_init (LOG_MODE, LOG_TASK);
#ifdef __LP64__
      printk (PRINT_PREF "The size of one log entry is %lu bytes\n",
	      sizeof (fmon_log_entry));
      printk (PRINT_PREF
	      "The log size is set to %d entries, total %lu bytes (%lu KB)\n",
	      LOG_MODE, LOG_MODE * sizeof (fmon_log_entry),
	      LOG_MODE * sizeof (fmon_log_entry) / 1024);
#else
      printk (PRINT_PREF "The size of one log entry is %u bytes\n",
	      sizeof (fmon_log_entry));
      printk (PRINT_PREF
	      "The log size is set to %d entries, total %u bytes (%u KB)\n",
	      LOG_MODE, LOG_MODE * sizeof (fmon_log_entry),
	      LOG_MODE * sizeof (fmon_log_entry) / 1024);
#endif /* __LP64__ */
    }
  else
    fmon_log_disable ();

  /* Parse partitions */
  if (TRACED_PART != -1)
    {
      uint64_t tmp_div;

      mtd2 = get_mtd_device (NULL, TRACED_PART);
      if (mtd == NULL)
	{
	  printk (PRINT_PREF "Error : Cannot get traced mtd partition\n");
	  fmon_log_exit ();
	  return -1;
	}

      part2 = PART (mtd2);
      traced_part_size = mtd2->size;
      traced_part_offset = part2->offset;

      tmp_div = traced_part_offset;
      do_div (tmp_div, NAND_BLOCK_SIZE);
      first_part_block = (int) tmp_div;

      tmp_div = (traced_part_offset + traced_part_size);
      do_div (tmp_div, NAND_BLOCK_SIZE);
      last_part_block = (int) tmp_div;

      printk (PRINT_PREF "B1 : %d\nBLast : %d\n", first_part_block,
	      last_part_block);

      put_mtd_device (mtd2);

      printk (PRINT_PREF "Traced partition index : %d\n", TRACED_PART);
      printk (PRINT_PREF "\tSize : %llu (%d MB)\n", traced_part_size,
	      ((int) traced_part_size / 1024) / 1024);
      printk (PRINT_PREF "\tOffset : %llu\n", traced_part_offset);

    }
  else
    {
      printk (PRINT_PREF "All partition traced\n");
    }

	/** fallback to nand_read ? yaffs2 does not use it so put a probe and
	 * nand_read_oob
	 */
  if (old_read_func == 1)
    {
      ret = register_jprobe (&my_jprobe_read_oob);
      if (ret < 0)
	{
	  printk (PRINT_PREF
		  "Error : register_jprobe (read_oob) failed : %d\n", ret);
	  fmon_log_exit ();
	  return -1;
	}
    }
  ret = register_jprobe (&my_jprobe_read);
  if (ret < 0)
    {
      printk (PRINT_PREF "Error : register_jprobe (read) failed : %d\n", ret);
      fmon_log_exit ();
      return -1;
    }
  ret = register_jprobe (&my_jprobe_write);
  if (ret < 0)
    {
      printk (PRINT_PREF "Error : register_jprobe (write) failed : %d\n",
	      ret);
      fmon_log_exit ();
      return -1;
    }
  ret = register_jprobe (&my_jprobe_erase);
  if (ret < 0)
    {
      printk (PRINT_PREF "Error : register_jprobe (erase) failed : %d\n",
	      ret);
      fmon_log_exit ();
      return -1;
    }

  if (old_read_func == 1)
    {
      printk (PRINT_PREF "Read OOB Jprobe on : %p, handler addr : %p\n",
	      my_jprobe_read_oob.kp.addr, my_jprobe_read_oob.entry);
    }

  printk (PRINT_PREF "Read Jprobe on : %p, handler addr : %p\n",
	  my_jprobe_read.kp.addr, my_jprobe_read.entry);

  printk (PRINT_PREF "Write Jprobe on : %p, handler addr : %p\n",
	  my_jprobe_write.kp.addr, my_jprobe_write.entry);

  printk (PRINT_PREF "Erase Jprobe on : %p, handler addr : %p\n",
	  my_jprobe_erase.kp.addr, my_jprobe_erase.entry);

  printk (PRINT_PREF "Flash device :\n");
  printk (PRINT_PREF "\tTotal size : %llu bytes (%d MB)\n", FLASH_SIZE,
	  (int) (FLASH_SIZE / 1024 / 1024));
  printk (PRINT_PREF "\tBlocks num : %d blocks\n", BLOCK_NUM);
  printk (PRINT_PREF "\tPages num : %d pages\n", BLOCK_NUM * PAGE_PER_BLOCK);
  printk (PRINT_PREF "\tBlock size : %d bytes (%d KB)\n", master->erasesize,
	  master->erasesize / 1024);
  printk (PRINT_PREF "\tPage size : %d bytes (%d KB)\n", NAND_PAGE_SIZE,
	  NAND_PAGE_SIZE / 1024);
  printk (PRINT_PREF "\tPages per block : %d pages\n", PAGE_PER_BLOCK);

  /* No PID ? */
  if (PROG_PID == 0)
    printk (PRINT_PREF "No PID for userland notification\n");

  if (PROG_PID != 0)
    {
      p = find_get_pid (PROG_PID);
      if (p == NULL)
	{
	  printk (PRINT_PREF "WARNING : Incorrect PID\n");
	  PROG_PID = 0;
	}
    }

  /* /proc entry creation : */
  proc_file_flashmon =
    proc_create (PROCFS_NAME, S_IWUGO | S_IRUGO, NULL, &fops_flashmon);
  if (proc_file_flashmon == NULL)
    {
      remove_proc_entry (PROCFS_NAME, NULL);
      printk (PRINT_PREF "ERROR : Unable to create /proc/%s\n", PROCFS_NAME);
      fmon_log_exit ();
      return -ENOMEM;
    }
  printk (PRINT_PREF "/proc/%s created\n", PROCFS_NAME);

  put_mtd_device (mtd);

  printk (PRINT_PREF "Flashmon module loaded\n");

  return 0;
}

/**
 * \fn ssize_t procfile_flashmon_read(struct file *file, 
 * 		char __user *buf, size_t size, loff_t *ppos)
 * \param buf User buffer to fill
 * \return 1 When read is complete
 * \brief /proc entry read function
 */
ssize_t
procfile_flashmon_read (struct file * file, char __user * buf, size_t size,
			loff_t * ppos)
{
  int i, ret;
  static int fini = 0;
  ret = 0;

  /* Cleanup the buffer */
  memset (buf, 0x00, size);

  if (TRACED_PART == -1)
    {
      for (i = *(ppos); (i < BLOCK_NUM) && (ret < (size - 6)); i++)
	ret =
	  sprintf (buf, "%s%u %u %u\n", buf, read_tab[i], write_tab[i],
		   erase_tab[i]);
    }
  else
    {
      for (i = *(ppos); (i < BLOCK_NUM) && (ret < (size - 6)); i++)
	if (i >= first_part_block && i < last_part_block)
	  ret =
	    sprintf (buf, "%s%u %u %u\n", buf, read_tab[i], write_tab[i],
		     erase_tab[i]);
    }

  *(ppos) = i;
  fini++;

  if (ret == 0)
    fini = 0;

  return ret;
}

/**
 * \fn static void __exit mod_exit(void)
 * \brief Module exit (cleanup)
 */
static void __exit
mod_exit (void)
{
  /* Remove probes */

  unregister_jprobe (&my_jprobe_read);
  unregister_jprobe (&my_jprobe_write);
  unregister_jprobe (&my_jprobe_erase);
  if (my_jprobe_read_oob.kp.addr != NULL)
    {
      unregister_jprobe (&my_jprobe_read_oob);
      printk (PRINT_PREF "Jprobe on %p removed\n",
	      my_jprobe_read_oob.kp.addr);
    }

  printk (PRINT_PREF "Jprobe on %p removed\n", my_jprobe_read.kp.addr);
  printk (PRINT_PREF "Jprobe on %p removed\n", my_jprobe_write.kp.addr);
  printk (PRINT_PREF "Jprobe on %p removed\n", my_jprobe_erase.kp.addr);

  /* Remove /proc entry */
  remove_proc_entry (PROCFS_NAME, NULL);
  printk (PRINT_PREF "/proc/%s removed\n", PROCFS_NAME);

  /* Cleanup */
  vfree (read_tab);
  vfree (write_tab);
  vfree (erase_tab);

  if (LOG_MODE > 0)
    fmon_log_exit ();

}

/**
 * \fn int procfile_flashmon_open(struct inode *inode, struct file *filp)
 * \brief Open /proc entry
 */
int
procfile_flashmon_open (struct inode *inode, struct file *filp)
{
  /* Success !! */
  return 0;
}

 /**
  * \fn int procfile_flashmon_close(struct inode *inode, struct file *filp)
  * \brief Close /proc entry
  */
int
procfile_flashmon_close (struct inode *inode, struct file *filp)
{
  /* Success */
  return 0;
}

ssize_t
procfile_flashmon_write (struct file * file, const char __user * buf,
			 size_t size, loff_t * ppos)
{
  char received[MAX_RECEIVED_SIZE];
  int i, ret;

  (void) ret;

  if (size > MAX_RECEIVED_SIZE)
    return size;

  ret = copy_from_user (received, buf, size);

  if (!strncmp (received, "reset", strlen ("reset")))
    {
      for (i = 0; i < BLOCK_NUM; i++)
	{
	  read_tab[i] = 0;
	  write_tab[i] = 0;
	  erase_tab[i] = 0;
	}
      fmon_log_reset ();
    }
  else if (!strncmp (received, "start", strlen ("start")))
    {
      flashmon_enabled = 1;
      if (LOG_MODE > 0)
	fmon_log_enable ();
    }
  else if (!strncmp (received, "stop", strlen ("stop")))
    {
      flashmon_enabled = 0;
      if (LOG_MODE > 0)
	fmon_log_disable ();
    }
  else
    {
      printk (PRINT_PREF "Unrecognized command : %s\n", received);
    }

  return size;
}

module_init (mod_init) module_exit (mod_exit) MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Pierre Olivier <pierre.olivier@univ-brest.fr>");
MODULE_DESCRIPTION
  ("Trace informations about flash page reads / writes, and flash block erase operations");
