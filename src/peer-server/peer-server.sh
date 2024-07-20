#!/bin/bash
# before starting, please run this: sudo sysctl -w net.ipv4.ip_unprivileged_port_start=80
trap "" 1
cd "${0%/*}" # cd to directory containing this script
mkdir -p logs
cd logs
(
trap "" 1
while true; do
    echo "Starting peer server..."
    ../pledge-1.8.com -v rwc:. -p "stdio rpath wpath cpath recvfd sendfd inet dns" ../peer-server.bin.x86_64 --email modeless@gmail.com --common-name peer-server.thelongestyard.link --TURN-server relay1.expressturn.com:3478 --TURN-username efDP9ZW8NKWYK75MZH --TURN-password BUujhcPttcpCb9Ib
    echo "Peer server crashed. Restarting in 2 seconds..."
    sleep 2
done
) > peer-server.log 2>&1 &
