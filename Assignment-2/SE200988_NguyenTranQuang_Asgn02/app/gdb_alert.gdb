# gdb_alert.gdb - ASSIGNMENT-02 Task 3, Kịch bản B: debug alert logic
# Author: Nguyen Tran Quang - SE200988
# Chạy: gdb -q -batch -x gdb_alert.gdb --args ./sms_app_x86 -d 8 -s other
set debuginfod enabled off
set pagination off
set confirm off
set print thread-events on

break processor_check_alarms
run

echo \n===== [1] Processor Thread dung tai processor_check_alarms =====\n
info threads
thread apply all bt

echo \n===== [2] Gia tri that va nguong =====\n
print temperature
print humidity
print p->cfg->temp_th

echo \n===== [3] Gia lap cam bien vuot nguong: set variable temperature=45.0 =====\n
set variable temperature = 45.0
print temperature

echo \n===== [4] Dat breakpoint o Alert Thread roi chay tiep =====\n
delete 1
break alert_handle
continue

echo \n===== [5] Alert Thread da thuc day sau mq_receive =====\n
info threads
bt
print *msg
print alarm_type_name(msg->type)
print ctx->counts
finish
print ctx->counts

echo \n===== [6] Bo breakpoint, chay tiep toi khi het -d 8 giay =====\n
delete
continue
