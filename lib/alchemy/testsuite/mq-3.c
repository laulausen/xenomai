#include <stdio.h>
#include <stdlib.h>
#include <copperplate/init.h>
#include <copperplate/traceobj.h>
#include <alchemy/task.h>
#include <alchemy/queue.h>

#define NMESSAGES  10

static struct traceobj trobj;

static int tseq[] = {
	11, 1, 2, 3, 12, 8, 14,
	13, 4, 5, 6, 7, 9, 10
};

static RT_QUEUE q;

static void main_task(void *arg)
{
	int ret, msg, n;

	traceobj_enter(&trobj);

	traceobj_mark(&trobj, 1);

	ret = rt_queue_create(&q, "QUEUE", NMESSAGES * sizeof(int), NMESSAGES, Q_FIFO);
	traceobj_assert(&trobj, ret == 0);

	traceobj_mark(&trobj, 2);

	for (msg = 0; msg < NMESSAGES; msg++) {
		ret = rt_queue_write(&q, &msg, sizeof(int), Q_NORMAL);
		traceobj_assert(&trobj, ret == 0);
	}

	traceobj_mark(&trobj, 3);

	ret = rt_queue_write(&q, &msg, sizeof(int), Q_URGENT);
	traceobj_assert(&trobj, ret == -ENOMEM);

	rt_task_sleep(1000000);

	ret = rt_queue_write(&q, &msg, sizeof(int), Q_URGENT);
	traceobj_assert(&trobj, ret == 0);

	traceobj_mark(&trobj, 4);

	ret = rt_queue_read(&q, &msg, sizeof(msg), TM_INFINITE);
	traceobj_assert(&trobj, ret == sizeof(int) && msg == 10);

	traceobj_mark(&trobj, 5);

	for (n = 1; n < NMESSAGES; n++) { /* peer task read #0 already. */
		ret = rt_queue_read(&q, &msg, sizeof(msg), TM_INFINITE);
		traceobj_assert(&trobj, ret == sizeof(int) && msg == n);
	}

	traceobj_mark(&trobj, 6);

	ret = rt_queue_delete(&q);
	traceobj_assert(&trobj, ret == 0);

	traceobj_mark(&trobj, 7);

	traceobj_exit(&trobj);
}

static void peer_task(void *arg)
{
	int ret, msg;

	traceobj_enter(&trobj);

	traceobj_mark(&trobj, 8);

	ret = rt_queue_read(&q, &msg, sizeof(msg), TM_INFINITE);
	traceobj_assert(&trobj, ret == sizeof(int) && msg == 0);

	traceobj_mark(&trobj, 14);

	rt_task_sleep(1000000);

	traceobj_mark(&trobj, 9);

	ret = rt_queue_read(&q, &msg, sizeof(msg), TM_INFINITE);
	traceobj_assert(&trobj, ret == -EIDRM || ret == -EINVAL);

	traceobj_mark(&trobj, 10);

	traceobj_exit(&trobj);
}

int main(int argc, char *argv[])
{
	RT_TASK t_main, t_peer;
	int ret;

	copperplate_init(argc, argv);

	traceobj_init(&trobj, argv[0], sizeof(tseq) / sizeof(int));

	traceobj_mark(&trobj, 11);

	ret = rt_task_spawn(&t_main, "main_task", 0,  50, 0, main_task, NULL);
	traceobj_assert(&trobj, ret == 0);

	traceobj_mark(&trobj, 12);

	ret = rt_task_spawn(&t_peer, "peer_task", 0,  49, 0, peer_task, NULL);
	traceobj_assert(&trobj, ret == 0);

	traceobj_mark(&trobj, 13);

	traceobj_join(&trobj);

	traceobj_verify(&trobj, tseq, sizeof(tseq) / sizeof(int));

	exit(0);
}