/*
 * sensor_thread.c
 * Author : Nguyen Tran Quang - SE200988
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include "sms.h"
#include "sms_sensor.h"

static int sensor_fd = -1;         
static long jitter_us[MAX_JITTER];
static int jitter_n;
static const char *policy_name = "SCHED_OTHER";


int sensor_open(const char *path, int interval_ms)
{
	if (strcmp(path, SIM_DEV) == 0) {
		printf("[sensor] simulation mode, no driver\n");
		return 0;
	}
	sensor_fd = open(path, O_RDONLY);
	if (sensor_fd < 0) {
		fprintf(stderr, "ERROR: cannot open %s: %s\n", path, strerror(errno));
		return -1;
	}
	if (ioctl(sensor_fd, SMS_SET_INTERVAL, &interval_ms) < 0)
		fprintf(stderr, "WARNING: ioctl SMS_SET_INTERVAL failed: %s\n", strerror(errno));
	return 0;
}

void sensor_close(void)
{
	if (sensor_fd >= 0)
		close(sensor_fd);
	sensor_fd = -1;
}

static void simulate_sample(SensorData *d)
{
	static float t = 30.0f, h = 60.0f;
	struct timespec now;

	t += (rand() % 21 - 10) / 10.0f;
	if (rand() % 20 == 0)
		t += (rand() % 2) ? 6.0f : -6.0f;
	h += (rand() % 41 - 20) / 10.0f;
	if (t < 15.0f) t = 15.0f;
	if (t > 45.0f) t = 45.0f;
	if (h < 30.0f) h = 30.0f;
	if (h > 90.0f) h = 90.0f;

	clock_gettime(CLOCK_MONOTONIC, &now);
	d->ts_ms = now.tv_sec * 1000LL + now.tv_nsec / 1000000;
	d->temp = t;
	d->humid = h;
}

static int read_sample(SensorData *d)
{
	char line[64];
	ssize_t n;

	if (sensor_fd < 0) {
		simulate_sample(d);
		return 0;
	}

	n = pread(sensor_fd, line, sizeof(line) - 1, 0);
	if (n < 0) {
		if (errno == EAGAIN)
			log_msg("[sensor] driver not ready (EAGAIN), skip this cycle");
		else if (errno == EINTR)
			log_msg("[sensor] read interrupted (EINTR), skip this cycle");
		else
			log_msg("[sensor] read error: %s", strerror(errno));
		return -1;
	}
	if (n == 0) {
		log_msg("[sensor] EOF from driver, skip this cycle");
		return -1;
	}
	line[n] = '\0';
	if (sscanf(line, "%lld,%f,%f", &d->ts_ms, &d->temp, &d->humid) != 3) {
		log_msg("[sensor] bad line: %s", line);
		return -1;
	}
	return 0;
}

static void add_ms(struct timespec *t, int ms)
{
	t->tv_nsec += (long)ms * 1000000L;
	while (t->tv_nsec >= 1000000000L) {
		t->tv_nsec -= 1000000000L;
		t->tv_sec++;
	}
}

static long diff_us(const struct timespec *a, const struct timespec *b)
{
	return (a->tv_sec - b->tv_sec) * 1000000L + (a->tv_nsec - b->tv_nsec) / 1000;
}

void *sensor_thread(void *arg)
{
	struct timespec expected, actual;
	SensorData d;
	(void)arg;

	pthread_setname_np(pthread_self(), "sms_sensor");

	if (cfg.use_fifo) {
		struct sched_param sp = { .sched_priority = SENSOR_PRIORITY };
		int err = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
		if (err == 0)
			policy_name = "SCHED_FIFO";
		else
			fprintf(stderr, "WARNING: cannot set SCHED_FIFO (%s), using SCHED_OTHER\n",
				strerror(err));
	}
	log_msg("[sensor] started, policy=%s, interval=%d ms", policy_name, cfg.interval_ms);

	clock_gettime(CLOCK_MONOTONIC, &expected);
	while (running) {
		add_ms(&expected, cfg.interval_ms);
		if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &expected, NULL) != 0)
			continue;                     /* bi signal danh thuc (Ctrl+C) */
		clock_gettime(CLOCK_MONOTONIC, &actual);

		if (jitter_n < MAX_JITTER)
			jitter_us[jitter_n++] = diff_us(&actual, &expected);

		if (read_sample(&d) != 0)
			continue;
		if (cb_push(&shared_buffer, &d) != 0)
			log_msg("[sensor] buffer full, drop sample ts=%lld", d.ts_ms);
	}
	log_msg("[sensor] stopped");
	return NULL;
}

static int cmp_long(const void *a, const void *b)
{
	long x = *(const long *)a, y = *(const long *)b;
	return (x > y) - (x < y);
}

void print_jitter_report(FILE *out)
{
	long sum = 0;
	int i, b0 = 0, b1 = 0, b2 = 0, b3 = 0;

	fprintf(out, "===== JITTER REPORT (%s, interval %d ms) =====\n",
		policy_name, cfg.interval_ms);
	if (jitter_n == 0) {
		fprintf(out, "no samples\n");
		return;
	}
	qsort(jitter_us, jitter_n, sizeof(long), cmp_long);
	for (i = 0; i < jitter_n; i++) {
		long j = jitter_us[i];
		sum += j;
		if (j < 100)        b0++;
		else if (j < 500)   b1++;
		else if (j < 1000)  b2++;
		else                b3++;
	}
	fprintf(out, "samples : %d\n", jitter_n);
	fprintf(out, "min     : %ld us\n", jitter_us[0]);
	fprintf(out, "avg     : %.1f us\n", (double)sum / jitter_n);
	fprintf(out, "max     : %ld us\n", jitter_us[jitter_n - 1]);
	fprintf(out, "p99     : %ld us\n", jitter_us[jitter_n * 99 / 100]);
	fprintf(out, "histogram:\n");
	fprintf(out, "  0-100us    : %d\n", b0);
	fprintf(out, "  100-500us  : %d\n", b1);
	fprintf(out, "  500-1000us : %d\n", b2);
	fprintf(out, "  >1000us    : %d\n", b3);
}
