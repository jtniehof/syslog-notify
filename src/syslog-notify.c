/*Takes syslog output from a named pipe and directs it to the libnotify
 *notification system
 *
 *  This file is part of syslog-notify.
 *  Copyright 2009-2011 syslog-notify project (see file AUTHORS)
 *
 *  syslog-notify is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  syslog-notify is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with syslog-notify.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libnotify/notify.h>
#define DEFAULT_FIFO "/var/spool/syslog-notify"
#define APP_NAME "syslog-notify"

/*File descriptor for the FIFO (and a dummy "write" descriptor)
 *Closing this is a signal to terminate
 */
int fd=0,wrfd=0;

/*Explains command line options
 *Called if can't parse options given
 *Input: argc/argv from main
 *Returns: nothing
 */
void PrintUsage(int argc, char* argv[]) {
  if (argc>=1) {
    fprintf(stderr,"%s: Sends syslog messages to Desktop Notifications via libnotify\n",argv[0]);
    fprintf(stderr,"Usage: %s [-n] [-f fifoname]\n",argv[0]);
    fprintf(stderr,"\t-n: Do not daemonize, but stay as foreground process\n");
    fprintf(stderr,
	    "\t-f fifoname: Get syslog data from fifoname, default\n\t\t%s \n",
	    DEFAULT_FIFO);
  }
}

/*Sends a single message to the notification daemon
 *Input: pointer to the title/summary
 *       pointer to the body of the message
 *Returns: nothing
 */
void SendMessage(const char* title,const char* message) {
  NotifyNotification *notification;
  GError *error=NULL;

  if(!notify_is_initted()) {
    /*Should probably send something to syslog before terminating*/
    close(fd);
    return;
  }
#ifdef NOTIFY_CHECK_VERSION
#if NOTIFY_CHECK_VERSION(0,7,0)
  notification=notify_notification_new(title,message,NULL);
#else
  notification=notify_notification_new(title,message,NULL,NULL);
#endif
#else
  notification=notify_notification_new(title,message,NULL,NULL);
#endif
  notify_notification_set_timeout(notification,NOTIFY_EXPIRES_DEFAULT);
  notify_notification_set_urgency(notification,NOTIFY_URGENCY_NORMAL);
  notify_notification_set_hint_string(notification,"x-canonical-append",
				      "allowed");
  if(!notify_notification_show(notification,&error)) {
    if(error) {
      /*TODO: actually handle errors!
       *human-readable message in error->message
       */
      g_error_free(error);
    }
    close(fd); /*Probably just want to quit*/
  }
  g_object_unref(notification);
}

/*Compare two strings for equality*/
gint CompareStrings(gconstpointer a, gconstpointer b) {
 /*No way to know for sure how long the string is, so just
  *try to limit the damage if there's no terminator
  */
  return strncmp(a,b,15);
}

/*Converts <, >, & in a string to HTML entities*/
void Entify(char* out,const char* in, size_t count) {
  const char *curr_in, *last_char;
  char* curr_out;

  last_char=out+count-1;

  for(curr_out=out,curr_in=in;
      curr_out<=last_char && *curr_in;
      curr_in++,curr_out++)
    switch(*curr_in) {
    case '<':
      if(curr_out+3<=last_char) {
	strncpy(curr_out,"&lt;",4);
	curr_out+=3;
      }
      else
	curr_out='\0';
      break;
    case '>':
      if(curr_out+3<=last_char) {
	strncpy(curr_out,"&gt;",4);
	curr_out+=3;
      }
      else
	curr_out='\0';
      break;
    case '&':
      if(curr_out+4<=last_char) {
	strncpy(curr_out,"&amp;",5);
	curr_out+=4;
      }
      else
	curr_out='\0';
      break;
    default:
      *curr_out=*curr_in;
    }

  if(curr_out<last_char)
    *curr_out='\0';
}

/*Copy a string from in to out (max count characters)
 *Perform any necessary translations (filtering bad
 *characters, entifying HTML)
 */
void Sanitize(char* out,const char* in,size_t count) {
  GList *caps;
  static int filter_html=-1;

  if(filter_html<0) {
    filter_html=0; /*Assume server doesn't handle markup*/
    caps=notify_get_server_caps();
    if(caps) {
      if(g_list_find_custom(caps,"body-markup",(GCompareFunc)CompareStrings))
	 filter_html=1;
      g_list_foreach(caps, (GFunc)g_free, NULL);
      g_list_free(caps);
    }
  }

  if(filter_html)
    Entify(out,in,count);
  else
    strncpy(out,in,count);
}

/*Parses a syslog line into a title and a message
 *May destroy contents of line, which MUST be zero-terminated
 */
void ParseLine(char* line,
	       char* out_title,int n_title,
	       char* out_message,int n_message) {
  char *start, *end, *title, *message;
  start=line;
  end=strchr(start,'\0');

  /*Skip date, time, system name*/
  if(end - start >= 16) {
    title=start+16;
    title=strchr(title,' ');
    if(!title)
      title=start;
    else
      title++;
  }
  else
    title=start;
  
  if(title==start) { /*Can't parse the line, just display it*/
    message=start;
    title=APP_NAME;
  }
  else {
    message=strchr(title,':');
    if(!message) { /*Another "can't parse"*/
      message=start;
      title=APP_NAME;
    }
    else {
      *message='\0';
      message++;
      while(message<end && *message==' ')
	message++;
    }
  }

  Sanitize(out_title,title,n_title);
  Sanitize(out_message,message,n_message);
}

/*Processes a buffer of message(s) from syslog
 *Sends to the notification daemon
 *Input: pointer to the message buffer
 *Returns: nothing
 */
void ProcessBuffer(char buffer[PIPE_BUF+1]) {
  /*Start/end of a line from syslog*/
  char *start, *end;
  /*Message title, and message itself*/
  char title[PIPE_BUF+1], message[PIPE_BUF+1];

  start=buffer;
  do {
    /*Break on a line*/
    end=strchr(start,'\n');
    if(end)
      *end='\0';
    else
      end=strchr(start,'\0');

    ParseLine(start,title,PIPE_BUF,message,PIPE_BUF);
    title[PIPE_BUF]='\0';
    message[PIPE_BUF]='\0';

    SendMessage(title,message);
    start=end+1;
  } while((end-buffer < PIPE_BUF) && *start != '\0');
}

/*Signal handler
 *Only handles TERM, INT, HUP and exits for all of them
 */
void handler(int sig) {
  switch(sig) {
  case SIGTERM:
  case SIGINT:
  case SIGHUP:
    close(fd);
    break;
  default:
    break;
  }
  return;
}

/*Closes files, uninits notify, generally cleans up for exit*/
void cleanup() {
  if(fd>0)
    close(fd);
  if(wrfd>0)
    close(wrfd);
  notify_uninit();
}

int main(int argc, char* argv[]) {
  const char* fifoname=DEFAULT_FIFO;
  int c,n_read,wrfd;
  int daemon=1;
  char buffer[PIPE_BUF+1]; /*Always room for a "guard" null*/

  memset(buffer,'\0',PIPE_BUF+1);
  
  /*Process options*/
  while((c=getopt(argc,argv,"f:n")) != -1) {
    switch (c) {
    case 'n':
      daemon=0;
      break;
    case 'f':
      fifoname=optarg;
      break;
    default:
      PrintUsage(argc,argv);
      exit(1);
    }
  }

  /*Open FIFO (for writing so don't EOF if syslog exits)*/
  if((fd=open(fifoname,O_RDONLY | O_NONBLOCK,0)) == -1) {
    perror("Unable to open FIFO for read");
    fprintf(stderr,"FIFO name was %s.\n",fifoname);
    exit(2);
  }
  wrfd=open(fifoname,O_WRONLY,0);

  /*Clear the FIFO of pending messages*/
  while((n_read=read(fd,buffer,PIPE_BUF))>0)
    if(!daemon)
      fprintf(stderr,"%s",buffer);
  if(n_read<0 && errno!=EAGAIN && errno!=EWOULDBLOCK) {
    perror("While clearing old messages");
    cleanup();
    exit(7);
  }

  /*Sleep on future reads*/
  fcntl(fd,F_SETFL,O_RDONLY);

  /*Open GTK connection*/
  if(!(notify_init(APP_NAME))) {
    cleanup();
    fprintf(stderr,"Unable to init notify library, exiting.\n");
    exit(3);
  }

  /*Register signal handlers*/
  if(signal(SIGTERM,handler) == SIG_IGN)
    signal(SIGTERM,SIG_IGN);
  if(daemon) {
    signal(SIGINT,SIG_IGN);
    signal(SIGHUP,SIG_IGN);
  }
  else {
    if(signal(SIGINT,handler) == SIG_IGN)
      signal(SIGINT,SIG_IGN);
    if(signal(SIGHUP,handler) == SIG_IGN)
      signal(SIGHUP,SIG_IGN);
  }

  /*Set up syslog handling*/

  /*Go daemon*/
  if(daemon) {
    switch(fork()) {
    case -1:
      perror("Unable to fork daemon process");
      cleanup();
      exit(4);
    case 0:
      close (STDIN_FILENO);
      close (STDOUT_FILENO);
      close (STDERR_FILENO);
      if (setsid()==-1) {
	perror("Unable to set new process context");
	cleanup();
	exit(5);
      }
      break;
    default:
      return 0;
    }
  }

  /*Loop on the FIFO*/
  while((n_read=read(fd,buffer,PIPE_BUF))>=0) {
    if(n_read) {
      buffer[n_read]='\0';
      if(!daemon)
	fprintf(stderr,"%s",buffer);
      ProcessBuffer(buffer);
    }
    else
      sleep(1); /*EOF; wait for a writer*/
  }
  cleanup();
  return 0;
}
