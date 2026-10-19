/*
 * sms_app.c 
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include "sms.h"

struct config cfg = {
	.interval_ms = 500,
	.temp_thr = 40.0f,
	.humid_thr = 80.0f,
	.duration_s = 60,
	.verbose = 0,
	.use_fifo = 1,
	.dev_path = DEFAULT_DEV,
	.log_path = DEFAULT_LOG,
};
volatile sig_atomic_t running = 1;
CircularBuffer shared_buffer;

static void on_signal(int sig)
{
	(void)sig;
	running = 0; 
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [OPTIONS]\n"
		"  -i <ms>    Sensor sampling interval (default: 500ms)\n"
		"  -t <C>     Temperature alert threshold (default: 40.0)\n"
		"  -h <%%>     Humidity alert threshold (default: 80.0)\n"
		"  -d <sec>   Run duration in seconds, 0 = infinite (default: 60)\n"
		"  -v         Verbose mode: print each sample to stdout\n"
		"  -s <pol>   Sensor thread policy: fifo | other (default: fifo)\n"
		"  -D <dev>   Device path (default: %s, 'sim' = no driver)\n"
		"  -L <file>  Log file (default: %s)\n",
		prog, DEFAULT_DEV, DEFAULT_LOG);
}

/* Doi chuoi sang so, sai dinh dang -> -1 */
static int to_int(const char *s, int *out)
{
	char *end;
	long v = strtol(s, &end, 10);
	if (*s == '\0' || *end != '\0')
		return -1;
	*out = (int)v;
	return 0;
}

static int to_float(const char *s, float *out)
{
	char *end;
	*out = strtof(s, &end);
	return (*s == '\0' || *end != '\0') ? -1 : 0;
}

static int parse_args(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "i:t:h:d:vs:D:L:")) != -1) {
		switch (opt) {
		case 'i':
			if (to_int(optarg, &cfg.interval_ms) || cfg.interval_ms < 1)
				return -1;
			break;
		case 't':
			if (to_float(optarg, &cfg.temp_thr))
				return -1;
			break;
		case 'h':
			if (to_float(optarg, &cfg.humid_thr))
				return -1;
			break;
		case 'd':
			if (to_int(optarg, &cfg.duration_s) || cfg.duration_s < 0)
				return -1;
			break;
		case 'v':
			cfg.verbose = 1;
			break;
		case 's':
			if (strcmp(optarg, "fifo") == 0)
				cfg.use_fifo = 1;
			else if (strcmp(optarg, "other") == 0)
				cfg.use_fifo = 0;
			else
				return -1;
			break;
		case 'D':
			cfg.dev_path = optarg;
			break;
		case 'L':
			cfg.log_path = optarg;
			break;
		default:
			return -1;
		}
	}
	return 0;
}

static void print_summary(long uptime)
{
	int size;
	unsigned long produced, consumed, overflow;

	cb_get_stats(&shared_buffer, &size, &produced, &consumed, &overflow);
	printf("\n===== SMS SUMMARY =====\n");
	printf("uptime          : %ld s\n", uptime);
	printf("samples         : produced=%lu consumed=%lu in_buffer=%d overflow=%lu\n",
	       produced, consumed, size, overflow);
	printf("alerts          : TEMP_HIGH=%lu HUMID_HIGH=%lu TEMP_SPIKE=%lu\n",
	       alert_count[TEMP_HIGH], alert_count[HUMID_HIGH], alert_count[TEMP_SPIKE]);
	printf("dropped logs    : %lu\n", dropped_logs);
	print_jitter_report(stdout);
	log_msg("[main] summary: uptime=%ld s samples=%lu overflow=%lu alerts TEMP_HIGH=%lu HUMID_HIGH=%lu TEMP_SPIKE=%lu",
		uptime, produced, overflow, alert_count[TEMP_HIGH],
		alert_count[HUMID_HIGH], alert_count[TEMP_SPIKE]);
}

int main(int argc, char *argv[])
{
	pthread_t t_sensor, t_proc, t_alert, t_logger;
	pthread_attr_t attr;
	struct sigaction sa;
	time_t start;

	/* ===== INIT ===== */
	if (parse_args(argc, argv) != 0) {
		usage(argv[0]);
		return 1;
	}
	if (sensor_open(cfg.dev_path, cfg.interval_ms) != 0)
		return 2;
	if (cb_init(&shared_buffer) != 0 || alert_init() != 0 ||
	    logger_init(cfg.log_path) != 0) {
		logger_cleanup();
		alert_cleanup();
		sensor_close();
		return 3;
	}

	/* Khoa bo nho trong RAM, tranh page fault lam tre Sensor Thread */
	if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
		perror("WARNING: mlockall");

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;        
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, THREAD_STACK);
	if (pthread_create(&t_logger, &attr, logger_thread, NULL) ||
	    pthread_create(&t_alert, &attr, alert_thread, NULL) ||
	    pthread_create(&t_proc, &attr, processor_thread, NULL) ||
	    pthread_create(&t_sensor, &attr, sensor_thread, NULL)) {
		fprintf(stderr, "ERROR: pthread_create failed\n");
		exit(4);                        /* he dieu hanh tu thu hoi tai nguyen */
	}

	/* ===== RUNNING ===== */
	printf("[main] SMS running (pid %d): interval=%d ms, T>%.1f, H>%.1f, duration=%d s. Ctrl+C to stop.\n",
	       getpid(), cfg.interval_ms, cfg.temp_thr, cfg.humid_thr, cfg.duration_s);
	log_msg("[main] started pid=%d dev=%s", getpid(), cfg.dev_path);
	start = time(NULL);
	while (running) {
		sleep(1);
		if (cfg.duration_s > 0 && time(NULL) - start >= cfg.duration_s)
			running = 0;
	}

	/* ===== SHUTDOWN: dung theo chieu dong du lieu ===== */
	printf("\n[main] shutting down...\n");
	pthread_join(t_sensor, NULL);   
	cb_stop(&shared_buffer);         
	pthread_join(t_proc, NULL);
	alert_stop();          
	pthread_join(t_alert, NULL);
	print_summary((long)(time(NULL) - start));
	logger_stop();                
	pthread_join(t_logger, NULL);

	pthread_attr_destroy(&attr);
	logger_cleanup();
	alert_cleanup();
	cb_destroy(&shared_buffer);
	sensor_close();
	munlockall();
	printf("[main] all threads stopped, exit 0\n");
	return 0;
}
