#!/bin/sh

if "$whoami" != "root"; then
    echo "this script requires root privilegs, abort."
    exit 1
fi

sync
stat yamui | grep Change
./yamui -m ${1:-1} -t ciao & sleep 4
killall yamui; killall -9 yamui
