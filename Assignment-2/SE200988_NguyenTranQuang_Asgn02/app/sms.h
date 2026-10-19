/*
 * sms.h - Khai bao chung cho sms_app (ASSIGNMENT-02)
 * Author : Nguyen Tran Quang - SE200988
 */
#ifndef SMS_H
#define SMS_H

#include <pthread.h>
#include <signal.h>
#include <stdio.h>


#define DEFAULT_DEV       "/dev/sms_sensor"
#define DEFAULT_LOG       "/tmp/sms.log"
#define SIM_DEV           "sim"
#define ALARM_QUEUE_NAME  "/sms_alarm"
#define LOG_QUEUE_NAME    "/sms_log"
#define BUFFER_SIZE       64  
#define AVG_WINDOW        10            
#define TEMP_SPIKE_LIMIT  5.0f       
#define LOG_MSG_LEN       200
#define LOG_MAX_BYTES     (1024 * 1024) 
#define LOG_FLUSH_SEC     5
#define SENSOR_PRIORITY   50     
#define THREAD_STACK      (256 * 1024)
#define MAX_JITTER        20000


typedef struct {
	long long ts_ms;
	float temp;
	float humid;
} SensorData;

enum { TEMP_HIGH = 0, HUMID_HIGH = 1, TEMP_SPIKE = 2, ALARM_STOP = 3 };

typedef struct {
	int type;
	long long ts_ms;
	float value;
	float threshold;
} Alarm;

typedef struct {
	SensorData *data;        
	int head, tail, count;
	int stopped;
	unsigned long produced, consumed, overflow;
	pthread_mutex_t lock;
	pthread_cond_t not_empty;
} CircularBuffer;

struct config {
	int interval_ms;
	float temp_thr;
	float humid_thr;
	int duration_s;
	int verbose;
	int use_fifo;
	const char *dev_path;
	const char *log_path;
};


extern struct config cfg;
extern volatile sig_atomic_t running;
extern CircularBuffer shared_buffer;

/* circular_buffer.c */
int  cb_init(CircularBuffer *cb);
void cb_destroy(CircularBuffer *cb);
int  cb_push(CircularBuffer *cb, const SensorData *d);   /* 0 = ok, -1 = day */
int  cb_pop(CircularBuffer *cb, SensorData *d);          /* 0 = ok, -1 = da dung */
void cb_stop(CircularBuffer *cb);
void cb_get_stats(CircularBuffer *cb, int *size, unsigned long *produced,
		  unsigned long *consumed, unsigned long *overflow);

/* sensor_thread.c */
int  sensor_open(const char *path, int interval_ms);
void sensor_close(void);
void *sensor_thread(void *arg);
void print_jitter_report(FILE *out);

/* processor_thread.c */
void *processor_thread(void *arg);
void detect_anomaly(long long ts, float temperature, float humidity,
		    float prev_temp, int has_prev);

/* alert_thread.c */
extern unsigned long alert_count[3];
int  alert_init(void);
void send_alarm(int type, long long ts, float value, float threshold);
void alert_stop(void);
void alert_cleanup(void);
void *alert_thread(void *arg);

/* logger_thread.c */
extern unsigned long dropped_logs;
int  logger_init(const char *path);
void log_msg(const char *fmt, ...);
void logger_stop(void);
void logger_cleanup(void);
void *logger_thread(void *arg);

#endif
