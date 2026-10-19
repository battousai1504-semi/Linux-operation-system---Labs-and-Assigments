/*
 * logger_thread.c 
 * Author : Nguyen Tran Quang - SE200988
 */
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "sms.h"

unsigned long dropped_logs;      
static mqd_t log_q_send = (mqd_t)-1; 
static mqd_t log_q_recv = (mqd_t)-1;     
static FILE *log_fp;
static const char *log_path;

int logger_init(const char *path)
{
	struct mq_attr attr = { 0 };

	attr.mq_maxmsg = 10;
	attr.mq_msgsize = LOG_MSG_LEN;
	mq_unlink(LOG_QUEUE_NAME);
	log_q_send = mq_open(LOG_QUEUE_NAME, O_CREAT | O_WRONLY | O_NONBLOCK, 0600, &attr);
	log_q_recv = mq_open(LOG_QUEUE_NAME, O_RDONLY);
	if (log_q_send == (mqd_t)-1 || log_q_recv == (mqd_t)-1) {
		perror("mq_open log_queue");
		return -1;
	}
	log_path = path;
	log_fp = fopen(path, "a");
	if (log_fp == NULL) {
		perror("fopen log file");
		return -1;
	}
	return 0;
}


void log_msg(const char *fmt, ...)
{
	char msg[LOG_MSG_LEN];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	if (mq_send(log_q_send, msg, strlen(msg) + 1, 0) != 0)
		__atomic_fetch_add(&dropped_logs, 1, __ATOMIC_RELAXED);   /* nhieu thread cung tang */
}

void logger_stop(void)
{
	/* Message rong = lenh dung. Queue day thi doi 1 ms roi thu lai. */
	while (mq_send(log_q_send, "", 0, 0) != 0 && errno == EAGAIN) {
		struct timespec ms = { 0, 1000000 };
		nanosleep(&ms, NULL);
	}
}

void logger_cleanup(void)
{
	if (log_fp)
		fclose(log_fp);
	log_fp = NULL;
	if (log_q_send != (mqd_t)-1)
		mq_close(log_q_send);
	if (log_q_recv != (mqd_t)-1)
		mq_close(log_q_recv);
	mq_unlink(LOG_QUEUE_NAME);
}

static void rotate_log(void)
{
	char old_name[256];

	fclose(log_fp);
	snprintf(old_name, sizeof(old_name), "%s.1", log_path);
	rename(log_path, old_name);
	log_fp = fopen(log_path, "w");
}

void *logger_thread(void *arg)
{
	char msg[LOG_MSG_LEN], stamp[32];
	time_t last_flush = time(NULL);
	struct timespec now;
	struct tm tm;
	ssize_t n;
	(void)arg;

	pthread_setname_np(pthread_self(), "sms_logger");
	fprintf(log_fp, "----- sms_app start -----\n");

	while (1) {
		n = mq_receive(log_q_recv, msg, sizeof(msg), NULL);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (n == 0)               
			break;

		clock_gettime(CLOCK_REALTIME, &now);
		localtime_r(&now.tv_sec, &tm);
		strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm);
		fprintf(log_fp, "%s.%03ld %s\n", stamp, now.tv_nsec / 1000000, msg);

		if (ftell(log_fp) > LOG_MAX_BYTES) {
			rotate_log();
			if (log_fp == NULL)
				break;
		}
		if (time(NULL) - last_flush >= LOG_FLUSH_SEC) {
			fflush(log_fp);
			last_flush = time(NULL);
		}
	}
	if (log_fp) {
		fprintf(log_fp, "----- sms_app stop -----\n");
		fflush(log_fp);
	}
	return NULL;
}
