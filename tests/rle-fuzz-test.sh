#!/bin/sh

#
# Test utility compression/decompression code with random data.
#
# Test circle:
# 1. Generate file with random data
# 2. Compress file
# 3. Decompress file
# 4. Compreare initial file with created on the previous step
# If all steps completed then clean current test data and go to step 1.
# Otherwise (if something goes wrong) leave current test data in place and
# go to step 1.
#

RLE=./rle

[ -z "$1" ] && NUM=1000000 || NUM=$1
[ -z "$2" ] && MAXSZ=65536 || MAXSZ=$2
[ -z "$2" ] && TMAX=3 || TMAX=$3

TMAX=$(($TMAX * 10))

for I in $(seq -w 1 $NUM); do
	# Step 1. Generate test input file
	SZ=$(($(od -An -N4 -tu4 -i /dev/urandom | head -n1) % $MAXSZ))
	dd if=/dev/urandom bs=$SZ count=1 of=test-$I-00.bin status=none

	# Step 2. Compress (endode) test file
	cat test-$I-00.bin | $RLE > test-$I-01.bin 2> test-$I-01.log &
	P=$!
	T=0
	while [ -d /proc/$P -a $T -lt $TMAX ]; do
		sleep 0.1
		T=$(($T + 1))
	done
	if [ -d /proc/$P ]; then
		kill -9 $P
		sleep 1
		echo "Killed due to timeout" >> test-$I-01.log
		echo "Test $I encoder (pid $P) killed due to timeout";
		continue
	fi

	# Step 3. Decompress (decode) test file
	cat test-$I-01.bin | $RLE -d > test-$I-02.bin 2> test-$I-02.log &
	P=$!
	T=0
	while [ -d /proc/$P -a $T -lt $TMAX ]; do
		sleep 0.1
		T=$(($T + 1))
	done
	if [ -d /proc/$P ]; then
		kill -9 $P
		sleep 1
		echo "Killed due timeout" >> test-$I-01.log
		echo "Test $I decoder (pid $P) killed due timeout";
		continue
	fi

	# Step 4. Compare input and output
	MD5_1=$(cat test-$I-00.bin | md5sum - | sed -e 's/ .*$//')
	MD5_2=$(cat test-$I-02.bin | md5sum - | sed -e 's/ .*$//')
	if [ "$MD5_1" != "$MD5_2" ]; then
		echo "Test $I failed due to MD5 missmatch"
		continue
	fi

	# Step 5. Cleanup current test data
	rm test-$I-*
done
