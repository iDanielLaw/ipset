# Create sets and inet6 rules which call set match and SET target
0 ./iptables.sh inet6 start
# Send probe packet from 1002:1002:1002:1002::64,tcp:1025
0 sendip -p ipv6 -6d ::1 -6s 1002:1002:1002:1002::64 -p tcp -td 80 -ts 1025 ::1
# Check that proper sets matched and target worked
0 ./check_klog.sh 1002:1002:1002:1002::64 tcp 1025 ipport list
# Send probe packet from 1002:1002:1002:1002::64,udp:1025 
0 sendip -p ipv6 -6d ::1 -6s 1002:1002:1002:1002::64 -p udp -ud 80 -us 1025 ::1
# Check that proper sets matched and target worked
0 ./check_klog.sh 1002:1002:1002:1002::64 udp 1025 ipport list
# Send probe packet from 1002:1002:1002:1002::1,tcp:1025
0 sendip -p ipv6 -6d ::1 -6s 1002:1002:1002:1002::1 -p tcp -td 80 -ts 1025 ::1
# Check that proper sets matched and target worked
0 ./check_klog.sh 1002:1002:1002:1002::1 tcp 1025 ip1 list
# Send probe packet from 1002:1002:1002:1002::32,tcp:1025
0 sendip -p ipv6 -6d ::1 -6s 1002:1002:1002:1002::32 -p tcp -td 80 -ts 1025 ::1
# Check that proper sets matched and target worked
0 ./check_klog.sh 1002:1002:1002:1002::32 tcp 1025 ip2
# Destroy sets and rules
0 ./iptables.sh inet6 stop
# eof
