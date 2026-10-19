/*
 * alert_thread.c
 * Author : Nguyen Tran Quang - SE200988
 */
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <string.h>
#include "sms.h"

unsigned long alert_count[3];             
static mqd_t alarm_q = (mqd_t)-1;
static const char *alarm_names[] = { "TEMP_HIGH", "HUMID_HIGH", "TEMP_SPIKE" };

int alert_init(void)
{
	struct mq_attr attr = { 0 };

	attr.mq_maxmsg = 10;
	attr.mq_msgsize = sizeof(Alarm);
	mq_unlink(ALARM_QUEUE_NAME);              
	alarm_q = mq_open(ALARM_QUEUE_NAME, O_CREAT | O_RDWR, 0600, &attr);
	if (alarm_q == (mqd_t)-1) {
		perror("mq_open alarm_queue");
		return -1;
	}
	return 0;
}

void send_alarm(int type, long long ts, float value, float threshold)
{
	Alarm a;
	memset(&a, 0, sizeof(a));
	a.type = type;
	a.ts_ms = ts;
	a.value = value;
	a.threshold = threshold;

	if (mq_send(alarm_q, (const char *)&a, sizeof(a), 0) != 0)
		log_msg("[processor] mq_send alarm failed: %s", strerror(errno));
}

void alert_stop(void)
{
	send_alarm(ALARM_STOP, 0, 0.0f, 0.0f);
}

void alert_cleanup(void)
{
	if (alarm_q != (mqd_t)-1) {
		mq_close(alarm_q);
		mq_unlink(ALARM_QUEUE_NAME);
	}
}

void *alert_thread(void *arg)
{
	Alarm a;
	(void)arg;

	pthread_setname_np(pthread_self(), "sms_alert");
	log_msg("[alert] started");

	while (1) {
		if (mq_receive(alarm_q, (char *)&a, sizeof(a), NULL) < 0) {
			if (errno == EINTR)
				continue;
			log_msg("[alert] mq_receive error: %s", strerror(errno));
			break;
		}
		if (a.type == ALARM_STOP)
			break;

		alert_count[a.type]++;
		fprintf(stderr, "[ALERT][%lld] TYPE: %s VALUE: %.1f THRESHOLD: %.1f\n",
			a.ts_ms, alarm_names[a.type], a.value, a.threshold);
		log_msg("[alert] %s value=%.1f threshold=%.1f ts=%lld",
			alarm_names[a.type], a.value, a.threshold, a.ts_ms);
	}
	log_msg("[alert] stopped");
	return NULL;
}
