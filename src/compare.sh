for f in $*; do
echo $f; tar -zxOf ../srpc.tgz $f | diff - $f
done
