/**
 * Comedi for RTDM, output command test program
 *
 * Copyright (C) 1997-2000 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Alexis Berlemont <alexis.berlemont@free.fr>
 *
 * Xenomai is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>

#include <native/task.h>

#include <comedi/comedi.h>

/* Default command's parameters */

/* For write operation, we consider 
   the default subdevice index is 1 */
#define ID_SUBD 1
/* For simplicity sake, a maximum channel 
   count is defined */
#define MAX_NB_CHAN 10
/* Four channels used by default */
#define NB_CHAN 4
/* One hundred triggered scans by default */
#define NB_SCAN 100

#define FILENAME "comedi0"

#define BUF_SIZE 10000

static unsigned char buf[BUF_SIZE];
static char *filename = FILENAME;
static char *str_chans = "0,1,2,3";
static unsigned int chans[MAX_NB_CHAN];
static int verbose = 0;
static int real_time = 0;
static int use_mmap = 0;

static RT_TASK rt_task_desc;

/* The command to send by default */
comedi_cmd_t cmd = {
    idx_subd: ID_SUBD,
    flags: 0,
    start_src: TRIG_NOW,
    start_arg: 0,
    scan_begin_src: TRIG_TIMER,
    scan_begin_arg: 2000000, /* in ns */
    convert_src: TRIG_TIMER,
    convert_arg: 500000, /* in ns */
    scan_end_src: TRIG_COUNT,
    scan_end_arg: 0,
    stop_src: TRIG_COUNT,
    stop_arg: NB_SCAN,
    nb_chan: 0,
    chan_descs: chans,
};

struct option cmd_write_opts[] = {
	{ "verbose", no_argument, NULL, 'v' },
	{ "real-time", no_argument, NULL, 'r' },
	{ "device", required_argument, NULL, 'd' },
	{ "subdevice", required_argument, NULL, 's' },
	{ "scan-count", required_argument, NULL, 'S' },
	{ "channels", required_argument, NULL, 'c' },
	{ "mmap", no_argument, NULL, 'm' },
	{ "help", no_argument, NULL, 'h' },
	{ 0 },
};

void do_print_usage(void)
{
    fprintf(stdout,"usage:\tcmd_write [OPTS]\n");
    fprintf(stdout,"\tOPTS:\t -v, --verbose: verbose output\n");
    fprintf(stdout,"\t\t -d, --device: device filename (comedi0, comedi1, ...)\n");
    fprintf(stdout,"\t\t -s, --subdevice: subdevice index\n");
    fprintf(stdout,"\t\t -S, --scan-count: count of scan to perform\n");
    fprintf(stdout,"\t\t -c, --channels: channels to use (ex.: -c 0,1)\n");
    fprintf(stdout,"\t\t -m, --mmap: mmap the buffer\n");
    fprintf(stdout,"\t\t -h, --help: print this help\n");
}

int main(int argc, char *argv[])
{
    int ret = 0, len, ofs;
    unsigned int i,scan_size = 0, cnt = 0;
    unsigned long buf_size;
    void *map = NULL;
    comedi_desc_t dsc;

    /* Computes arguments */
    while((ret = getopt_long(argc,
			   argv,
			   "vrd:s:S:c:mh", cmd_write_opts, NULL))>=0) {
	switch(ret) {
	case 'v':
	    verbose = 1;
	    break;
	case 'r':
	    real_time = 1;
	    break;
	case 'd':
	    filename = optarg;
	    break;
	case 's':
	    cmd.idx_subd = strtoul(optarg, NULL, 0);
	    break;
	case 'S':
	    cmd.stop_arg = strtoul(optarg, NULL, 0);
	    break;
	case 'c':
	    str_chans = optarg;
	    break;
	case 'm':
	    use_mmap = 1;
	    break;
	case 'h':
	default:
	    do_print_usage();
	    return 0;
	}
    }

    /* Recovers the channels to compute */
    do {
	cmd.nb_chan++;
	len = strlen(str_chans);
	ofs = strcspn(str_chans, ",");
	if(sscanf(str_chans, "%u", &chans[cmd.nb_chan - 1]) == 0) {
	    fprintf(stderr, "cmd_write: bad channels argument\n");
	    return -EINVAL;
	}	
	str_chans += ofs+1;    
    }while(len != ofs);

    /* Updates the command structure */
    cmd.scan_end_arg = cmd.nb_chan;

    if(real_time != 0) {

	if(verbose != 0)
	    printf("cmd_write: switching to real-time mode\n");

	/* Prevents any memory-swapping for this program */
	ret = mlockall(MCL_CURRENT | MCL_FUTURE);
	if (ret < 0) {
	    ret = errno;
	    fprintf(stderr, "cmd_write: mlockall failed (ret=%d)\n", ret);
	    goto out_main;
	}

	/* Turns the current process into an RT task */
	ret = rt_task_shadow(&rt_task_desc, NULL, 1, 0);
	if (ret < 0) {
	    fprintf(stderr, "cmd_write: rt_task_shadow failed (ret=%d)\n",
		    ret);
	    goto out_main;
	}
    }

    /* Opens the device */
    ret = comedi_open(&dsc, filename);
    if(ret < 0) {
	fprintf(stderr,
		"cmd_write: comedi_open %s failed (ret=%d)\n",
		FILENAME, ret);
	return ret;
    }

    if(verbose != 0) {
	printf("cmd_write: device %s opened (fd=%d)\n", 
	       filename, dsc.fd);
	printf("cmd_write: basic descriptor retrieved\n");
	printf("\t subdevices count = %d\n", dsc.nb_subd);
	printf("\t read subdevice index = %d\n", dsc.idx_read_subd);
	printf("\t write subdevice index = %d\n", dsc.idx_write_subd);
    }

    /* Allocates a buffer so as to get more info (subd, chan, rng) */
    dsc.sbdata = (comedi_root_t*) malloc(dsc.sbsize);
    if(dsc.sbdata == NULL) {
	fprintf(stderr,"cmd_write: malloc failed \n");
	return -ENOMEM;    
    }

    /* Gets this data */
    ret = comedi_fill_desc(&dsc);
    if(ret<0) {
	fprintf(stderr,
		"cmd_write: comedi_get_desc failed (ret=%d)\n",ret);
	goto out_main;
    }

    if(verbose != 0)
	printf("cmd_write: complex descriptor retrieved\n");

    /* Gets the size of a single acquisition */
    for(i=0;i < cmd.nb_chan; i++) {
	comedi_chinfo_t *info;

	ret = comedi_get_chinfo(&dsc,
				cmd.idx_subd,cmd.chan_descs[i],&info);
	if(ret < 0) {
	    fprintf(stderr,
		    "cmd_write: comedi_get_chinfo failed (ret=%d)\n",
		    ret);
	    goto out_main;
	}

	if(verbose != 0) {
	    printf("cmd_write: channel %x\n", cmd.chan_descs[i]);
	    printf("\t ranges count = %d\n", info->nb_rng);
	    printf("\t range's size = %d (bits)\n", info->nb_bits);      
	}

	scan_size += info->nb_bits / 8;
    }

    if(verbose != 0) {
	printf("cmd_write: scan size = %u\n", scan_size);
	printf("cmd_write: size to write  = %u\n", 
	       scan_size * cmd.stop_arg);
    }

    /* Cancels any former command which might be in progress */
    comedi_snd_cancel(&dsc, cmd.idx_subd);

    if(use_mmap != 0) {

	/* Gets the buffer size to map */
	ret = comedi_get_bufsize(&dsc, cmd.idx_subd, &buf_size);
	if(ret < 0) {
	    fprintf(stderr, 
		    "cmd_write: comedi_get_bufsize() failed (ret=%d)\n", 
		    ret);
	    goto out_main;
	}
  
	if(verbose != 0)
	    printf("cmd_write: buffer size = %lu bytes\n", buf_size);


	/* Maps the analog input subdevice buffer */
	ret = comedi_mmap(&dsc, cmd.idx_subd, buf_size, &map);
	if(ret < 0) {
	    fprintf(stderr, 
		    "cmd_write: comedi_mmap() failed (ret=%d)\n", ret);
	    goto out_main;
	}

	if(verbose != 0)
	    printf("cmd_write: mmap performed successfully (map=0x%p)\n",
		   map);
    }

    /* Sends the command to the output device */
    ret = comedi_snd_command(&dsc, &cmd);
    if(ret<0) {
	fprintf(stderr,"cmd_write: comedi_snd_command failed (ret=%d)\n",ret);
	goto out_main;
    }

    if(verbose != 0)
	printf("cmd_write: command successfully sent\n");

    /* Sets up the buffer to be written */
    for(i = 0; i < BUF_SIZE; i++)
	buf[i] = i;

    ret = rt_task_set_mode(0, T_PRIMARY, NULL);
    if(ret < 0) {
	fprintf(stderr,"cmd_read: rt_task_set_mode failed (ret=%d)\n",ret);	
	goto out_main;
    }

    if(use_mmap == 0) {

	/* Sends data */
	while(cnt < scan_size * cmd.stop_arg) {
	    unsigned int tmp = (scan_size * cmd.stop_arg - cnt) > BUF_SIZE ? 
		BUF_SIZE : (scan_size * cmd.stop_arg - cnt);

	    ret = comedi_sys_write(dsc.fd, buf, tmp);
	    if(ret < 0) {
		fprintf(stderr,
			"cmd_write: comedi_write failed (ret=%d)\n",ret);
		goto out_main;
	    }

	    cnt += ret;
	}
    }
    else {
	unsigned long front = 0;

	/* Sends data through the shared buffer */
	while(cnt < cmd.stop_arg * scan_size) {

	    /* If the buffer is full, wait for an event
	       (Note that comedi_poll() also retrieves the data amount
	       to read; in our case it is useless as we have to update
	       the data read counter) */
	    if(front == 0) {
		ret = comedi_poll(&dsc, cmd.idx_subd, COMEDI_INFINITE);
		if(ret < 0) {
		    fprintf(stderr,
			    "cmd_write: comedi_mark_bufrw() failed (ret=%d)\n",
			    ret);
		    goto out_main;
		}
		else 
		    front = (unsigned long) ret;
	    }

	    /* Updates the variable front according to the data amount
	       we still have to send */
	    if(front > (scan_size * cmd.stop_arg - cnt))
		front = scan_size * cmd.stop_arg - cnt;

	    /* Performs the copy 
	     (Usually, such an operation should be avoided: the shared
	     buffer should be used without any intermediate buffer,
	     the "mmaped" buffer is interesting for saving data copy) */
	    memcpy(map + (cnt % buf_size), 
		   buf + (cnt % BUF_SIZE), front);

	    /* Updates the counter */
	    cnt += front;

	    /* Retrieves and update the buffer's state
	       (In output case, we recover how many bytes can be
	       written into the shared buffer) */
	    ret = comedi_mark_bufrw(&dsc, cmd.idx_subd, front, &front);
	    if(ret < 0) {
		fprintf(stderr, 
			"cmd_write: comedi_mark_bufrw() failed (ret=%d)\n",
			ret);
		goto out_main;
	    }	   
	}
    }

    if(verbose != 0)
	printf("cmd_write: %d bytes successfully written\n", cnt);

    ret = 0;

    out_main:
    
    /* Frees the buffer used as device descriptor */
    free(dsc.sbdata);
    
    /* Releases the file descriptor */
    comedi_close(&dsc);

    return ret;
}
