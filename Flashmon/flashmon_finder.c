/*
 * FLASHMON flash memory monitoring tool (Version 2.1)
 * Revision Authors: Pierre Olivier<pierre.olivier@univ-ubs.fr>, Jalil Boukhobza <boukhobza@univ-brest.fr>
 * Contributors: Pierre Olivier, Ilyes Khetib, Crina Arsenie
 *
 * Copyright (c) of University of Occidental Britanny (UBO) <boukhobza@univ-brest.fr>, 2010-2013.
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
 * 
 * The finder module is in charge of identifying the functions for 
 * flash read, write and erase operations on various platforms
 */

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/version.h>

#include "flashmon.h"
#include "flashmon_finder.h"

int find_funcs(unsigned int *readfunc, unsigned int *writefunc, unsigned int *erasefunc)
{
	struct mtd_info *child, *master;
	struct mtd_part *part;
	struct nand_chip *chip;
	int old_read_func = 0;
	
		/* Let's get /dev/mtd0 */
	child = get_mtd_device(NULL, 0);
	if(child == NULL)
	{
	  printk(PRINT_PREF "Error : Cannot get /dev/mtd0\n");
	  return -1;
	}
	
	/* get the master then the chip object */
	part = PART(child);
	master = part->master;
	put_mtd_device(child);
	chip = master->priv;
	
	if(chip == NULL)
	{
		printk(PRINT_PREF "Error : cant get the chip object\n");
		return -1;
	}
  
#ifdef __LP64__
	*writefunc = (int)((uint64_t)chip->write_page);
	*erasefunc = (int)((uint64_t)master->erase);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
	*readfunc = (int)((uint64_t)chip->ecc.read_page);
#else
	*readfunc = (int)master->read;
	old_read_func = 1;
#endif /* kernel version */
#else
	*writefunc = (int)chip->write_page;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
	*erasefunc = (int)master->_erase;
#else
	*erasefunc = (int)master->erase;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
	*readfunc = (int)chip->ecc.read_page;
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
	*readfunc = (int)master->_read;
#else
	*readfunc = (int)master->read;
#endif
	old_read_func = 1;
#endif /* kernel version */
#endif  /* __LP64__ */
	

	
	printk(PRINT_PREF "Read : %x, Write : %x, Erase : %x\n", (unsigned int)*readfunc, (unsigned int)*writefunc, (unsigned int)*erasefunc);
	
	if(chip->ecc.read_page == NULL)
	{
		printk(PRINT_PREF "Error finding flash read function\n");
		return -1;
	}
	if(chip->write_page == NULL)
	{
		printk(PRINT_PREF "Error finding flash write function\n");
		return -1;
	}
	
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
	if(master->_erase == NULL)
	{
		printk(PRINT_PREF "Error finding flash erase function\n");
		return -1;
	}
#else
	if(master->erase == NULL)
	{
		printk(PRINT_PREF "Error finding flash erase function\n");
		return -1;
	}
#endif
	
	return old_read_func;
}
