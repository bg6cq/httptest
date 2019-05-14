#!/bin/sh
#
# 从urllist.txt 读要监测的URL，出现异常时，记录日志，并可以向用户发短信（修改log里的发信息部分）
#

DEBUG=1
#几次连续错误，开始发通知
COUNT=2

RUNDIR=/usr/src/httptest/httpmonitor/
LOGFILE=${RUNDIR}/log.txt
URLFILE=${RUNDIR}/urllist.txt
HTTPTEST=${RUNDIR}/../httptest

log ()
{   	a=`date`;
	echo $a $1 >> ${LOGFILE}
        #sendsms $2 $1
}

grep -v ^# $URLFILE | while read url alfile user checkstr
do 

ALFILE=${RUNDIR}/${alfile}.alarmon

if [ $DEBUG ]; then echo checking $url; fi

if [ $checkstr ]; then
$HTTPTEST $url -r $checkstr  > /dev/null 2>/dev/null
else
$HTTPTEST $url  > /dev/null 2>/dev/null
fi
tmp=$?

if [ $DEBUG ]; then echo result $tmp; fi

if test $tmp == 0  ; then
	if test -f $ALFILE.count ; then 
		rm -f $ALFILE.count
	fi
	if test -f $ALFILE ; then 
		msg="${url}访问正常"
		if [ $DEBUG ]; then echo $msg; fi
		log $msg $user
		rm -f $ALFILE
	fi
else
	if ! test -f $ALFILE ; then 
		if test -f $ALFILE.count; then
			count=`cat $ALFILE.count`
		else 
			count=0
		fi
		if test $count -gt $COUNT; then
			msg="${url}访问异常"
			if [ $DEBUG ]; then echo $msg; fi
			log $msg $user
			touch $ALFILE
		else
			echo `expr $count + 1` > $ALFILE.count
		fi
	fi
fi
done

