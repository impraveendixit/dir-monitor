#!/bin/bash

loop_func() {
        while :
        do
                touch $1/test1.txt $1/test2.txt $1/test3.txt $1/test4.txt
                sleep 1
                echo "TEXT2" >>$1/test1.txt
                sleep 0.2
                rm -f $1/test1.txt $1/test2.txt $1/test3.txt $1/test4.txt
                sleep 0.2
        done
}

loop_func $1 &
loop_func $2 &
loop_func $3 &
loop_func $4 &
loop_func $5

#mosquitto_sub -h localhost -p 1883 -t \#

exit ?
