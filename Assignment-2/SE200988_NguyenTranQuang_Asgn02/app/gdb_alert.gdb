# gdb_alert.gdb - GDB kich ban B: gia lap nhiet do vuot nguong
set pagination off
set print thread-events off
break detect_anomaly
run

echo \n===== [1] Processor Thread dung tai detect_anomaly =====\n
info threads
thread apply all bt 3
echo \n===== [2] Gia tri that va nguong =====\n
print temperature
print humidity
print cfg.temp_thr
print alert_count

echo \n===== [3] set variable temperature = 45.0 =====\n
set variable temperature = 45.0
print temperature
delete 1
break alert_thread.c:73
continue

echo \n===== [4] Alert Thread thuc day sau mq_receive =====\n
info threads
print a
print alarm_names[a.type]
next
print alert_count
delete
continue
