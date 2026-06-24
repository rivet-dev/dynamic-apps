#!/bin/sh
# Copyright (C) 2020  Ali Abdallah <ali.abdallah@suse.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

XFCONFD_DIR=$1
TEST=$2
XFCONFD=$XFCONFD_DIR/xfconfd

XFCONF_RUN_IN_TEST_MODE=1; export XFCONF_RUN_IN_TEST_MODE;

exec_xfconfd()
{
	rm $XFCONFD_DIR/.xfconfd-test-pid >/dev/null 2>&1
	rm $XFCONFD_DIR/.xfconfd-sum >/dev/null 2>&1

	exec ${XFCONFD} &
	echo $! > $XFCONFD_DIR/.xfconfd-test-pid
	pid=`cat $XFCONFD_DIR/.xfconfd-test-pid`
	sum $XFCONFD > $XFCONFD_DIR/.xfconfd-sum
}

cleanup()
{
	pid=`cat $XFCONFD_DIR/.xfconfd-test-pid`

	rm $XFCONFD_DIR/.xfconfd-test-pid >/dev/null 2>&1
	rm $XFCONFD_DIR/.xfconfd-sum >/dev/null 2>&1

	while kill -0 $pid >/dev/null 2>&1; do
		kill -s TERM $pid >/dev/null 2>&1
		sleep 0.1
	done
}

prepare()
{
	if [ ! -f $XFCONFD ]; then
		exit 1
	fi

	# Start xfconfd only if it is not started already
	if [ ! -f $XFCONFD_DIR/.xfconfd-test-pid ]; then
		exec_xfconfd
	elif [ ! -f $XFCONFD_DIR/.xfconfd-sum ]; then
		cleanup
		exec_xfconfd
	else
		oldsum=`cat $XFCONFD_DIR/.xfconfd-sum`
		newsum=`sum ${XFCONFD}`

		# Did xfconfd changes ?
		if [ "$newsum" != "$oldsum" ]; then
			cleanup
			exec_xfconfd
		else
			pid=`cat $XFCONFD_DIR/.xfconfd-test-pid`
			kill -s 0 $pid >/dev/null 2>&1

			# Is it still running ?
			if [ $? != 0 ]; then
				cleanup
				exec_xfconfd
			fi
		fi
	fi
}

sigint()
{
	cleanup
	exit 1
}

trap 'sigint'  INT

# Last test, cleanup
TEST_NAME=$(basename $TEST)

if [ "$TEST_NAME" = "t-tests-end" ]; then
	cleanup
	exit 0
fi
# Prepare xfconfd
prepare

$TEST
ret=$?
# Test failed, cleanup
if [ $ret -ne 0 ]; then
  cleanup
fi
exit $ret
