#!/bin/bash
filename="README"

#starting server in other directory
cd .. 
bin/myserver 1231 &
bin/myserver 1232 &
bin/myserver 1233 &
bin/myserver 1234 &
cd src
../bin/myclient "server-info.txt" 4 $filename

diff -q "../"$filename $filename

killall myserver