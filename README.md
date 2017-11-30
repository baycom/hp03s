# hp03s

hp03s - raspberry pi tool for hp03s sensors
Options:
 -a <altitude> : Set altitude in m (default 664.0)
 -D            : Debug, print raw messages and some other stuff
 -e <exec>     : Executable to be called for every message (try echo)
 -w <wait>     : Number of seconds to sleep in loop mode (default 0)
 -m <mode>     : 0: execute once, 1: execute in loop with sleep time


$ ./hp03s -a 573
Temp                : 35.66°C
Pressure at sealevel: 1002.23hPa
Pressure            : 936.00hPa@573.0m
Altitude estimated  : 663.98m
