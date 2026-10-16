/*
 * logger_thread.c - Logger Thread và hàng đợi log (log_queue = POSIX mq)
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 *
 * Mọi thread gọi log_post() -> mq_send (không chờ) vào "/sms_log".
 * Logger Thread mq_timedreceive -> ghi file với timestamp đầy đủ.
 *   - Flush định kỳ mỗi LOG_FLUSH_SEC giây (không flush sau mỗi dòng)
 *   - Rotate khi file > 1 MB: sms.log -> sms.log.1, tạo file mới
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "logger_thread.h"

static mqd_t s_tx = (mqd_t)-1;      /* đầu gửi, O_NONBLOCK         */
static mqd_t s_rx = (mqd_t)-1;      /* đầu nhận của Logger, blocking */
static atomic_ulong s_posted;
static atomic_ulong s_dropped;

/*
 * log_queue_open - tạo POSIX mq cho log (xóa queue cũ còn sót nếu có)
 * Return: 0 nếu thành công, -1 nếu lỗi (errno giữ nguyên)
 */
int log_queue_open(void)
{
    struct mq_attr attr = { .mq_maxmsg = SMS_MQ_MAXMSG, .mq_msgsize = sizeof(LogMsg) };

    mq_unlink(SMS_LOG_MQ_NAME);
    s_rx = mq_open(SMS_LOG_MQ_NAME, O_CREAT | O_EXCL | O_RDONLY, 0600, &attr);
    if (s_rx == (mqd_t)-1)
        return -1;
    s_tx = mq_open(SMS_LOG_MQ_NAME, O_WRONLY | O_NONBLOCK);
    if (s_tx == (mqd_t)-1) {
        mq_close(s_rx);
        s_rx = (mqd_t)-1;
        return -1;
    }
    return 0;
}

/*
 * log_queue_close - đóng và xóa log queue (gọi sau khi Logger đã join)
 * Return: không có
 */
void log_queue_close(void)
{
    if (s_tx != (mqd_t)-1)
        mq_close(s_tx);
    if (s_rx != (mqd_t)-1)
        mq_close(s_rx);
    s_tx = s_rx = (mqd_t)-1;
    mq_unlink(SMS_LOG_MQ_NAME);
}

/*
 * log_post - gửi 1 dòng log cho Logger Thread (printf-style, không chặn lâu)
 * @fmt: chuỗi định dạng
 * Nếu queue đầy: thử lại tối đa 10 lần, mỗi lần chờ 2 ms, sau đó bỏ (đếm dropped).
 * Return: không có
 */
void log_post(const char *fmt, ...)
{
    LogMsg msg;
    va_list ap;
    int i;

    if (s_tx == (mqd_t)-1)
        return;
    memset(&msg, 0, sizeof(msg));
    va_start(ap, fmt);
    vsnprintf(msg.text, sizeof(msg.text), fmt, ap);
    va_end(ap);

    for (i = 0; i < 10; i++) {
        if (mq_send(s_tx, (const char *)&msg, sizeof(msg), 0) == 0) {
            atomic_fetch_add(&s_posted, 1);
            return;
        }
        if (errno != EAGAIN)
            break;
        struct timespec ts = { 0, 2 * 1000000L };
        nanosleep(&ts, NULL);
    }
    atomic_fetch_add(&s_dropped, 1);
}

/*
 * logger_send_shutdown - gửi message shutdown (thử lại tới khi Logger nhận được)
 * Return: 0 nếu gửi được, -1 nếu thất bại sau ~2 giây
 */
int logger_send_shutdown(void)
{
    LogMsg msg;
    int i;

    memset(&msg, 0, sizeof(msg));
    msg.shutdown = 1;
    for (i = 0; i < 1000; i++) {
        if (mq_send(s_tx, (const char *)&msg, sizeof(msg), 0) == 0)
            return 0;
        if (errno != EAGAIN)
            return -1;
        struct timespec ts = { 0, 2 * 1000000L };
        nanosleep(&ts, NULL);
    }
    return -1;
}

/* Getter thống kê */
unsigned long log_posted_count(void)  { return atomic_load(&s_posted); }
unsigned long log_dropped_count(void) { return atomic_load(&s_dropped); }

/*
 * open_log - mở file log ở chế độ append, buffer 16 KB (flush thủ công)
 * @path: đường dẫn, @bytes: nhận kích thước hiện tại của file
 * Return: FILE* hoặc NULL
 */
static FILE *open_log(const char *path, long *bytes)
{
    FILE *fp = fopen(path, "a");

    if (!fp)
        return NULL;
    setvbuf(fp, NULL, _IOFBF, 16 * 1024);
    fseek(fp, 0, SEEK_END);
    *bytes = ftell(fp);
    return fp;
}

/*
 * write_line - ghi 1 dòng kèm timestamp, rotate nếu vượt 1 MB
 * @ctx: logger, @fpp: con trỏ FILE* (có thể đổi khi rotate), @bytes: kích thước file
 * @text: nội dung
 * Return: không có
 */
static void write_line(LoggerCtx *ctx, FILE **fpp, long *bytes, const char *text)
{
    char ts[32];
    int n;

    if (!*fpp)
        return;
    now_wall_str(ts, sizeof(ts));
    n = fprintf(*fpp, "%s %s\n", ts, text);
    if (n > 0)
        *bytes += n;
    ctx->lines++;

    if (*bytes > LOG_ROTATE_BYTES) {
        char old[256];

        fflush(*fpp);
        fclose(*fpp);
        snprintf(old, sizeof(old), "%s.1", ctx->path);
        rename(ctx->path, old);                 /* sms.log -> sms.log.1 */
        *fpp = open_log(ctx->path, bytes);      /* file mới, bytes = 0  */
        ctx->rotations++;
        if (*fpp) {
            now_wall_str(ts, sizeof(ts));
            *bytes += fprintf(*fpp, "%s [logger] log rotated (old file: %s)\n", ts, old);
        }
    }
}

/*
 * logger_thread_main - vòng lặp của Logger Thread
 * @arg: LoggerCtx*
 * Return: NULL
 */
void *logger_thread_main(void *arg)
{
    LoggerCtx *ctx = arg;
    LogMsg msg;
    struct timespec next_flush, now;
    long bytes = 0;
    FILE *fp;
    char line[LOG_MSG_MAX + 64];

    pthread_setname_np(pthread_self(), "sms_logger");
    fp = open_log(ctx->path, &bytes);
    if (!fp)
        fprintf(stderr, "[logger] ERROR: cannot open %s: %s\n", ctx->path, strerror(errno));

    write_line(ctx, &fp, &bytes, "[logger] thread started");
    clock_gettime(CLOCK_REALTIME, &next_flush);   /* mq_timedreceive dùng REALTIME */
    next_flush.tv_sec += LOG_FLUSH_SEC;

    for (;;) {
        ssize_t n = mq_timedreceive(s_rx, (char *)&msg, sizeof(msg), NULL, &next_flush);

        if (n < 0) {
            if (errno == ETIMEDOUT) {             /* tới hạn flush định kỳ */
                if (fp)
                    fflush(fp);
                ctx->flushes++;
                next_flush.tv_sec += LOG_FLUSH_SEC;
                continue;
            }
            if (errno == EINTR)
                continue;
            fprintf(stderr, "[logger] mq_timedreceive: %s\n", strerror(errno));
            break;
        }
        if (msg.shutdown)
            break;
        msg.text[LOG_MSG_MAX - 1] = '\0';
        write_line(ctx, &fp, &bytes, msg.text);

        clock_gettime(CLOCK_REALTIME, &now);      /* log dày -> vẫn flush đúng hạn */
        if (now.tv_sec >= next_flush.tv_sec) {
            if (fp)
                fflush(fp);
            ctx->flushes++;
            next_flush.tv_sec = now.tv_sec + LOG_FLUSH_SEC;
        }
    }

    snprintf(line, sizeof(line),
             "[logger] thread stopped (lines=%lu, flushes=%lu, rotations=%lu, dropped=%lu)",
             ctx->lines, ctx->flushes, ctx->rotations, log_dropped_count());
    write_line(ctx, &fp, &bytes, line);
    if (fp) {
        fflush(fp);
        fclose(fp);
    }
    return NULL;
}
