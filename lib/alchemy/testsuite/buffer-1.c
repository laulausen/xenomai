#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <copperplate/init.h>
#include <copperplate/traceobj.h>
#include <alchemy/task.h>
#include <alchemy/buffer.h>

static struct traceobj trobj;

static RT_TASK t_bgnd, t_fgnd;

static RT_BUFFER buffer;

static void foreground_task(void *arg)
{
	ssize_t ret;
	char buf[6];
	int n = 0;

	traceobj_enter(&trobj);

	/*
	 * We should get a short read (1 bytes), then a full read (2
	 * bytes), and back again.
	 */
	for (;;) {
		memset(buf,0, sizeof(buf));
		ret = rt_buffer_read(&buffer, buf, 2, TM_INFINITE);
		switch (ret) {
		case 1:
			traceobj_assert(&trobj, buf[0] == ((n / 2) % 26) + 'A');
			break;
		case 2:
			traceobj_assert(&trobj, atoi(buf) == ((n / 2) % 10) * 11);
			break;
		default:
			traceobj_assert(&trobj, 0);
		}
		n++;
	}

	traceobj_exit(&trobj);
}

static void background_task(void *arg)
{
	char c = 'A', s[3];
	ssize_t ret;
	int n = 0;

	traceobj_enter(&trobj);

	for (;;) {
		ret = rt_buffer_write(&buffer, &c, 1, TM_INFINITE);
		traceobj_assert(&trobj, ret == 1);
		c++;
		if (c > 'Z')
			c = 'A';
		sprintf(s, "%.2d", 11 * n);
		ret = rt_buffer_write(&buffer, s, 2, TM_INFINITE);
		traceobj_assert(&trobj, ret == 2);
		n = (n + 1) % 10;
	}

	traceobj_exit(&trobj);
}

int main(int argc, char *const argv[])
{
	int ret;

	copperplate_init(&argc, &argv);

	traceobj_init(&trobj, argv[0], 0);

	ret = rt_buffer_create(&buffer, NULL, 2, B_FIFO);
	traceobj_assert(&trobj, ret == 0);

	ret = rt_task_shadow(NULL, "main_task", 30, 0);
	traceobj_assert(&trobj, ret == 0);

	ret = rt_task_create(&t_fgnd, "FGND", 0,  20, 0);
	traceobj_assert(&trobj, ret == 0);

	ret = rt_task_start(&t_fgnd, foreground_task, NULL);
	traceobj_assert(&trobj, ret == 0);

	ret = rt_task_create(&t_bgnd, "BGND", 0,  10, 0);
	traceobj_assert(&trobj, ret == 0);

	ret = rt_task_start(&t_bgnd, background_task, NULL);
	traceobj_assert(&trobj, ret == 0);

	ret = rt_task_sleep(1500000000ULL);
	traceobj_assert(&trobj, ret == 0);

	ret = rt_task_delete(&t_bgnd);
	traceobj_assert(&trobj, ret == 0);

	ret = rt_task_delete(&t_fgnd);
	traceobj_assert(&trobj, ret == 0);

	ret = rt_buffer_delete(&buffer);
	traceobj_assert(&trobj, ret == 0);

	traceobj_join(&trobj);

	exit(0);
}
