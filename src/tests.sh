echo starting echoserver >/dev/tty
./echoserver&
echo running conntest >/dev/tty
./conntest -a 10
echo running sinktest \(It takes a while ... \) >/dev/tty
./sinktest >/dev/null
echo running echoclient >/dev/tty
./echoclient <echoclient.c | diff - echoclient.c
echo running sinkclient >/dev/tty
./sinkclient <sinkclient.c
echo running sgenclient >/dev/tty
./sgenclient -l 10000 >/dev/null
echo running mthclient >/dev/tty
./mthclient -t 4 -l 10000
kill %1
