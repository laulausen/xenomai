/**
 * @file
 * Hardware driver for 8255 chip
 * @note Copyright (C) 1999 David A. Schleef <ds@schleef.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#ifndef __COMEDI_8255_H__
#define __COMEDI_8255_H__

#include <comedi/comedi_driver.h>

typedef int (*comedi_8255_cb_t)(int, int, int, unsigned long);

typedef struct subd_8255_struct {
	unsigned long cb_arg;
	comedi_8255_cb_t cb_func;
	unsigned int status;
	int have_irq;
	int io_bits; 
} subd_8255_t;

#ifdef CONFIG_XENO_DRIVERS_COMEDI_8255

#define _8255_SIZE 4

#define _8255_DATA 0
#define _8255_CR 3

#define CR_C_LO_IO	0x01
#define CR_B_IO		0x02
#define CR_B_MODE	0x04
#define CR_C_HI_IO	0x08
#define CR_A_IO		0x10
#define CR_A_MODE(a)	((a)<<5)
#define CR_CW		0x80


void subdev_8255_init(comedi_subd_t *subd, subd_8255_t *subd_8255);
void subdev_8255_interrupt(comedi_dev_t *dev, subd_8255_t *subd_8255);

#else /* !CONFIG_XENO_DRIVERS_COMEDI_8255 */

#define subdev_8255_init(x, y)		do { } while(0)
#define subdev_8255_interrupt(x, y)	do { } while(0)

#endif /* CONFIG_XENO_DRIVERS_COMEDI_8255 */

#endif /* !__COMEDI_8255_H__ */
