-------------
Introduction:
-------------
Directory Monitor is a tool to monitor the changes in the directories mainly addition,
updation and deletions of files and inform those changes via MQTT protocol. Once started,
the process will run endlessly untill it receives the MQTT message as given below. Directory which is to
be monitored can be specified in the command line as well as mentioned in the MQTT message in format:

# for starting directory monitoring:
        mosquitto_pub -h localhost -p 1883 -t "DIR_MONITOR/config" -m "{\"cmd_code\":\"start_dir_monitoring\",\"msg\":{\"directories\":[\"<dirname1>\",\"<dirname2>\"]}}"

# for stopping directory monitoring:
        mosquitto_pub -h localhost -p 1883 -t "DIR_MONITOR/config" -m "{\"cmd_code\":\"stop_dir_monitoring\",\"msg\":{\"directories\":[\"<dirname1>\",\"<dirname2>\"]}}"

# for killing the program:
        mosquitto_pub -h localhost -p 1883 -t "DIR_MONITOR/config" -m "{\"cmd_code\":\"kill_dir_monitoring\"}"

# for subscribing:
        mosquitto_sub -h localhost -p 1883 -t "DIR_MONITOR/#"

Please Note MQTT topics are hardcoded. For logging to specific directory changes,
subscribe to topic 'DIR_MONITOR/<dir-name>' where dir-name is the directory name to be
monitored.

----------------
Compile and run:
----------------
1. For installing libmosquitto:
        sudo apt-get install libmosquitto-dev mosquitto

2. For installing json-c libraries:
        sudo apt-get install libjson-c-dev

   issue 'cmake -Bbuild' in the mosquitto folder and then 'make' inside build folder.
   Same is for json-c library.

3. Building the application is also same. Go to the project root folder and
   then type 'cmake -Bbuild' and then inside build folder 'make'

4. You can set MQTT host address at the compile time
   by -DHOST=<host address> or by default it uses localhost and port 1883.

5. By default, application log level is set to DEBUG level. You can set log level
   at compile time in cmake options by -DLOG_LEVEL=<val>.
   5-debug, 4-info, 3-warn, 2-error, 1-fatal

7. After building the project, you can run as:
        ./bin/app <dir1> <dir2> <dir3> ... <dirn>
