#!/usr/bin/env python

"""This script will attempt to put more bytes into the syslog-notify
FIFO than will fit in the pipe buffer. This tests the handling of messages
split across reads from the FIFO."""

PIPE_BUF = 4096 #get this from pipebuf_size.c
FIFO = '/var/spool/syslog-notify'

msg = "Jan  1 00:00:00 test cram_buffer.py: " \
    "This is a message to fill the buffer, number {0:03d}\n"
length = len(msg) - 4 #format string is longer than its replacement.
if PIPE_BUF % length == 0:
    print("Buffer is integral multiple of message, try again")
else:
    count = int(PIPE_BUF / length) + 1
    print('Spamming syslog-notify, last number will be {0}'.format(count - 1))
    messages = ''.join((msg.format(i) for i in range(count)))
    with open(FIFO, 'w') as f:
        f.write(messages)

