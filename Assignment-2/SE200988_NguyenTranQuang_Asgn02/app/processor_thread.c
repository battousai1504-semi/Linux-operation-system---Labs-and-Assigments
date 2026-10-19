/*
 * processor_thread.c
 * Author : Nguyen Tran Quang - SE200988
 */
#include <math.h>
#include "sms.h"

void detect_anomaly(long long ts, float temperature, float humidity,
		    float prev_temp, int has_prev)
{
	if (temperature > cfg.temp_thr)
		send_alarm(TEMP_HIGH, ts, temperature, cfg.temp_thr);

	if (humidity > cfg.humid_thr)
		send_alarm(HUMID_HIGH, ts, humidity, cfg.humid_thr);

	if (has_prev && fabsf(temperature - prev_temp) > TEMP_SPIKE_LIMIT)
		send_alarm(TEMP_SPIKE, ts, temperature - prev_temp, TEMP_SPIKE_LIMIT);
}

void *processor_thread(void *arg)
{
	SensorData d;
	float temps[AVG_WINDOW], hums[AVG_WINDOW];
	float prev_temp = 0.0f;
	int has_prev = 0, n = 0, idx = 0, i;
	(void)arg;

	pthread_setname_np(pthread_self(), "sms_processor");
	log_msg("[processor] started");

	while (cb_pop(&shared_buffer, &d) == 0) {
		float sum_t = 0.0f, sum_h = 0.0f, avg_t, avg_h;

		/* Cua so 10 mau gan nhat (mang vong nho) */
		temps[idx] = d.temp;
		hums[idx] = d.humid;
		idx = (idx + 1) % AVG_WINDOW;
		if (n < AVG_WINDOW)
			n++;
		for (i = 0; i < n; i++) {
			sum_t += temps[i];
			sum_h += hums[i];
		}
		avg_t = sum_t / n;
		avg_h = sum_h / n;

		detect_anomaly(d.ts_ms, d.temp, d.humid, prev_temp, has_prev);
		prev_temp = d.temp;
		has_prev = 1;

		log_msg("[processor] ts=%lld T=%.1f H=%.1f avgT=%.2f avgH=%.2f",
			d.ts_ms, d.temp, d.humid, avg_t, avg_h);
		if (cfg.verbose)
			printf("[SAMPLE] ts=%lld T=%.1f C H=%.1f %% | avg10 T=%.2f H=%.2f\n",
			       d.ts_ms, d.temp, d.humid, avg_t, avg_h);
	}
	log_msg("[processor] stopped");
	return NULL;
}
