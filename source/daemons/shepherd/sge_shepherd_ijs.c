/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/resource.h>

#if defined(DARWIN)
#  include <termios.h>
#  include <sys/ttycom.h>
#  include <sys/ioctl.h>
#elif defined(HP11) || defined(HP1164)
#  include <termios.h>
#elif defined(INTERIX)
#  include <termios.h>
#  include <sys/ioctl.h>
#else
#  include <termio.h>
#endif

#include "basis_types.h"
#include "err_trace.h"
#include "sge_stdlib.h"
#include "sge_dstring.h"
#include "sge_pty.h"
#include "sge_ijs_comm.h"
#include "sge_ijs_threads.h"
#include "sge_io.h"
#include "sge_fileio.h"

#include "sge_unistd.h"

#define THISCOMPONENT   "pty_shepherd"
#define OTHERCOMPONENT  "pty_qrsh"

#define RESPONSE_MSG_TIMEOUT 120

/* copy from shepherd.c */
#define CKPT_REST_KERNEL 0x080     /* set for all restarted kernel ckpt jobs */
/* end copy from shepherd.c */

/* 
 * Compile with EXTENSIVE_TRACING defined to get lots of trace messages form
 * the two worker threads.
 */
#undef EXTENSIVE_TRACING

extern int received_signal;

int my_error_printf(const char *fmt, ...);

static int                  g_ptym           = -1;
static int                  g_fd_pipe_in[2]  = {-1,-1};
static int                  g_fd_pipe_out[2] = {-1, -1};
static int                  g_fd_pipe_err[2] = {-1, -1};
static int                  g_fd_pipe_to_child[2] = {-1, -1};
static COMMUNICATION_HANDLE *g_comm_handle   = NULL;
static THREAD_HANDLE        g_thread_main;
static int                  g_ckpt_pid            = 0;
static int                  g_ckpt_type           = 0;
static int                  g_timeout             = 0;
static int                  g_ckpt_interval       = 0;
static char                 *g_childname          = NULL;
static int                  g_raised_event        = 0;
static struct rusage        *g_rusage             = NULL;
static int                  *g_exit_status        = NULL;
static int                  g_job_pid             = 0;
static bool                 g_usage_filled_in     = false;

char                        *g_hostname           = NULL;
extern bool                 g_csp_mode;  /* defined in shepherd.c */

void handle_signals_and_methods(
   int npid,
   int pid,
   int *postponed_signal,
   pid_t ctrl_pid[3],
   int ckpt_interval,
   int ckpt_type,
   int *ckpt_cmd_pid,
   int *rest_ckpt_interval,
   int timeout,
   int *migr_cmd_pid,
   int ckpt_pid,
   int *kill_job_after_checkpoint,
   int status,
   int *inArena,
   int *inCkpt,
   char *childname,
   int *job_status,
   int *job_pid);

/****** append_to_buf() ******************************************************
*  NAME
*     append_to_buf() -- copies data from pty (or pipe) to a buffer
*
*  SYNOPSIS
*     int append_to_buf(int fd, char *pbuf, int *buf_bytes)
*
*  FUNCTION
*     Reads data from the pty or pipe and appends it to the given buffer.
*
*  INPUTS
*     int  fd          - file descriptor to read from
*     char *pbuf       - working buffer, must be of size BUFSIZE
*     int  *buf_bytes  - number of bytes already in the buffer
*
*  OUTPUTS
*     int *buf_bytes   - number of bytes in the buffer
*
*  RESULT
*     int - >0: OK, number of bytes read from the fd
*           =0: EINTR or EAGAIN occured, just call select() and read again
*           -1: error occcured, connection was closed
*
*  NOTES
*     MT-NOTE: 
*
*  SEE ALSO
*******************************************************************************/
int append_to_buf(int fd, char *pbuf, int *buf_bytes)
{
   int nread = 0;

   if (fd >= 0) {
      nread = read(fd, &pbuf[*buf_bytes], BUFSIZE-1-(*buf_bytes));

      if (nread < 0 && (errno == EINTR || errno == EAGAIN)) {
         nread = 0;
      } else if (nread <= 0) {
         nread = -1;
      } else {
         *buf_bytes += nread;
      }
   }
   return nread;
}

/****** send_buf() ***********************************************************
*  NAME
*     send_buf() -- sends the content of the buffer over the commlib
*
*  SYNOPSIS
*     int send_buf(char *pbuf, int buf_bytes, int message_type)
*
*  FUNCTION
*     Sends the content of the buffer over to the commlib to the receiver.
*
*  INPUTS
*     char *pbuf        - buffer to send
*     int  buf_bytes   - number of bytes in the buffer to send
*     int  message_tye - type of the message that is to be sent over
*                        commlib. Can be STDOUT_DATA_MSG or
*                        STDERR_DATA_MSG, depending on where the data 
*                        came from (pty and stdout = STDOUT_DATA_MSG,
*                        stderr = STDERR_DATA_MSG)
*
*  RESULT
*     int - 0: OK
*           1: could'nt write all data
*
*  NOTES
*     MT-NOTE: 
*
*  SEE ALSO
*******************************************************************************/
int send_buf(char *pbuf, int buf_bytes, int message_type)
{
   int ret = 0;
   dstring err_msg = DSTRING_INIT;

   if (comm_write_message(g_comm_handle, g_hostname, 
      OTHERCOMPONENT, 1, (unsigned char*)pbuf, 
      (unsigned long)buf_bytes, message_type, &err_msg) != buf_bytes) {
      shepherd_trace("couldn't write all data: %s\n",
                     sge_dstring_get_string(&err_msg));
      ret = 1;
   } else {
#ifdef EXTENSIVE_TRACING
      shepherd_trace("successfully wrote all data: %s\n", 
                     sge_dstring_get_string(&err_msg));
#endif
   }

   sge_dstring_free(&err_msg);
   return ret;
}

/****** pty_to_commlib() *******************************************************
*  NAME
*     pty_to_commlib() -- pty_to_commlib thread entry point and main loop
*
*  SYNOPSIS
*     void* pty_to_commlib(void *t_conf)
*
*  FUNCTION
*     Entry point and main loop of the pty_to_commlib thread.
*     Reads data from the pty and writes it to the commlib.
*
*  INPUTS
*     void *t_conf - pointer to cl_thread_settings_t struct of the thread
*
*  RESULT
*     void* - always NULL
*
*  NOTES
*     MT-NOTE: 
*
*  SEE ALSO
*******************************************************************************/
static void* pty_to_commlib(void *t_conf)
{
   int                  do_exit = 0;
   int                  fd_max = 0;
   int                  ret;
   fd_set               read_fds;
   struct timeval       timeout;
   char                 *stdout_buf = NULL;
   char                 *stderr_buf = NULL;
   int                  stdout_bytes = 0;
   int                  stderr_bytes = 0;
   int                  postponed_signal = 0;
   pid_t                ctrl_pid[3];
   int                  ckpt_cmd_pid = -999;
   int                  migr_cmd_pid = -999;
   int                  rest_ckpt_interval = g_ckpt_interval;
   int                  kill_job_after_checkpoint = 0;
   int                  status = 0;
   int                  inArena, inCkpt = 0;
   int                  job_id = 0;
   int                  i;
   int                  npid;
   struct rusage        tmp_usage;
   bool                 b_select_timeout = false;
   dstring              err_msg = DSTRING_INIT;

   /* Report to thread lib that this thread starts up now */
   thread_func_startup(t_conf);
   /* Set this thread to be cancelled (=killed) at any time. */
   thread_setcancelstate(1);

   /* Allocate working buffers, BUFSIZE = 64k */
   stdout_buf = sge_malloc(BUFSIZE);
   stderr_buf = sge_malloc(BUFSIZE);

   /* Write info that we already have a checkpoint in the arena */
   if (g_ckpt_type & CKPT_REST_KERNEL) {
      inArena = 1;
      create_checkpointed_file(1);
   } else {
      inArena = 0;
   }

   /* Initialize some variables */
   for (i=0; i<3; i++) {
      ctrl_pid[i] = -999;
   }
   memset(&tmp_usage, 0, sizeof(tmp_usage));

   /* The main loop of this thread */
   while (do_exit == 0) {
      /* If a signal was received, process it */
#ifdef EXTENSIVE_TRACING
      shepherd_trace("pty_to_commlib: calling wait3() to check if a child process exited.");
#endif
#if defined(CRAY) || defined(NECSX4) || defined(NECSX5) || defined(INTERIX)
      npid = waitpid(-1, &status, WNOHANG);
#else
      npid = wait3(&status, WNOHANG, &tmp_usage);
#endif
#ifdef EXTENSIVE_TRACING
      shepherd_trace("pty_to_commlib: wait3() returned npid = %d, status = %d, "
                     "errno = %d, %s.", status, npid, errno, strerror(errno));
      shepherd_trace("pty_to_commlib: received_signal = %d", received_signal);
#endif

      /* We always want to handle signals and methods, so we set npid = -1
       * - except when wait3() returned a valid pid.
       */
      if (npid > 0) {
         shepherd_trace("pty_to_commlib: our child exited -> exiting");
         do_exit = 1;
      }
      if (npid == 0) {
         npid = -1;
      }

      /* Do all the handling for signals, checkpointing and migrating and methods. */
      handle_signals_and_methods(
         npid,
         g_job_pid,
         &postponed_signal,
         ctrl_pid,
         g_ckpt_interval,
         g_ckpt_type,
         &ckpt_cmd_pid,
         &rest_ckpt_interval,
         g_timeout,
         &migr_cmd_pid,
         g_ckpt_pid,
         &kill_job_after_checkpoint,
         status,
         &inArena,
         &inCkpt,
         g_childname,
         g_exit_status,
         &job_id);

      /* Fill fd_set for select */
      FD_ZERO(&read_fds);
      /* Do this only if commlib doesn't have many messages to send in it's
       * queue. Otherwise, don't read from pty and don't send to commlib.
       */
      comm_trigger(g_comm_handle, 0, &err_msg);
      if (g_ptym != -1) {
         FD_SET(g_ptym, &read_fds);
      }
      if (g_fd_pipe_out[0] != -1) {
         FD_SET(g_fd_pipe_out[0], &read_fds);
      }
      if (g_fd_pipe_err[0] != -1) {
         FD_SET(g_fd_pipe_err[0], &read_fds);
      }
      fd_max = MAX(g_ptym, g_fd_pipe_out[0]);
      fd_max = MAX(fd_max, g_fd_pipe_err[0]);

#ifdef EXTENSIVE_TRACING
      shepherd_trace("pty_to_commlib: g_ptym = %d, g_fd_pipe_out[0] = %d, "
                     "g_fd_pipe_err[0] = %d, fd_max = %d",
                     g_ptym, g_fd_pipe_out[0], g_fd_pipe_err[0], fd_max);
#endif
      /* Fill timeout struct for select */
      b_select_timeout = false;
      if (do_exit == 1) {
         /* If we know that we have to exit, don't wait for data,
          * just peek into the fds and read all data available from the buffers. */
         timeout.tv_sec  = 0;
         timeout.tv_usec = 0;
      } else {
         if ((stdout_bytes > 0 && stdout_bytes < 256)
             || cl_com_messages_in_send_queue(g_comm_handle) > 0) {
            /* 
             * If we just received few data, wait another 10 milliseconds if new 
             * data arrives to avoid sending data over the network in too small junks.
             * Also retry to send the messages that are still in the queue ASAP.
             */
            timeout.tv_sec  = 0;
            timeout.tv_usec = 10000;
         } else {
            /* Standard timeout is one second */
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
         }
      }
#ifdef EXTENSIVE_TRACING
      shepherd_trace("pty_to_commlib: doing select() with %d seconds, "
                     "%d usec timeout", timeout.tv_sec, timeout.tv_usec);
#endif
      /* Wait blocking for data from pty or pipe */
      errno = 0;
      ret = select(fd_max+1, &read_fds, NULL, NULL, &timeout);
      thread_testcancel(t_conf);
#ifdef EXTENSIVE_TRACING
      shepherd_trace("pty_to_commlib: select() returned %d", ret);
#endif
      if (ret < 0) {
         /* select error */
#ifdef EXTENSIVE_TRACING
         shepherd_trace("pty_to_commlib: select() returned %d, reason: %d, %s",
                        ret, errno, strerror(errno));
#endif
         if (errno == EINTR) {
            /* If we have a buffer, send it now (as we don't want to care about
             * how long we acutally waited), then just continue, the top of the 
             * loop will handle signals.
             * b_select_timeout tells the bottom of the loop to send the buffer.
             */

            /* ATTENTION: Don't call shepherd_trace() here, because on some
             * architectures it causes another EINTR, leading to a infinte loop.
             */
            b_select_timeout = true;
         } else {
            shepherd_trace("pty_to_commlib: select() error -> exiting");
            do_exit = 1;
         }
      } else if (ret == 0) {
         /* timeout, if we have a buffer, send it now */
#ifdef EXTENSIVE_TRACING
         shepherd_trace("pty_to_commlib: select() timeout");
#endif
         b_select_timeout = true;
      } else {
         /* at least one fd is ready to read from */
         ret = 1;
         /* now we can be sure that our child has started the job,
          * we can close the pipe_to_child now
          */
         if (g_fd_pipe_to_child[1] != -1) {
            shepherd_trace("pty_to_commlib: closing pipe to child");
            close(g_fd_pipe_to_child[1]);
            g_fd_pipe_to_child[1] = -1;
         }
         if (g_ptym != -1 && FD_ISSET(g_ptym, &read_fds)) {
#ifdef EXTENSIVE_TRACING
            shepherd_trace("pty_to_commlib: reading from ptym");
#endif
            ret = append_to_buf(g_ptym, stdout_buf, &stdout_bytes);
         }
         if (ret >= 0 && g_fd_pipe_out[0] != -1
             && FD_ISSET(g_fd_pipe_out[0], &read_fds)) {
#ifdef EXTENSIVE_TRACING
            shepherd_trace("pty_to_commlib: reading from pipe_out");
#endif
            ret = append_to_buf(g_fd_pipe_out[0], stdout_buf, &stdout_bytes);
         }
         if (ret >= 0 && g_fd_pipe_err[0] != -1
             && FD_ISSET(g_fd_pipe_err[0], &read_fds)) {
#ifdef EXTENSIVE_TRACING
            shepherd_trace("pty_to_commlib: reading from pipe_err");
#endif
            ret = append_to_buf(g_fd_pipe_err[0], stderr_buf, &stderr_bytes);
         }
#ifdef EXTENSIVE_TRACING
         shepherd_trace("pty_to_commlib: appended %d bytes to buffer", ret);
#endif
         if (ret < 0) {
            /* A fd was closed, likely our child has exited, we can exit, too. */
            shepherd_trace("pty_to_commlib: append_to_buf() returned %d, "
                           "errno %d, %s -> exiting", ret, errno, strerror(errno));
            do_exit = 1;
         }
      }
      /* Always send stderr buffer immediately */
      if (stderr_bytes != 0) {
         ret = send_buf(stderr_buf, stderr_bytes, STDERR_DATA_MSG);
         if (ret == 0) {
            stderr_bytes = 0;
         } else {
            shepherd_trace("pty_to_commlib: send_buf() returned %d "
                           "-> exiting", ret);
            do_exit = 1;
         }
      }
      /* 
       * Send stdout_buf if there is enough data in it
       * OR if there was a select timeout (don't wait too long to send data)
       * OR if we will exit the loop now and there is data in the stdout_buf
       */
      if (stdout_bytes >= 256 ||
          (b_select_timeout == true && stdout_bytes > 0) ||
          (do_exit == 1 && stdout_bytes > 0)) {
#ifdef EXTENSIVE_TRACING
         shepherd_trace("pty_to_commlib: sending stdout buffer");
#endif
         ret = send_buf(stdout_buf, stdout_bytes, STDOUT_DATA_MSG);
         stdout_bytes = 0;
         if (ret != 0) {
            shepherd_trace("pty_to_commlib: send_buf() failed -> exiting");
            do_exit = 1;
         }
         comm_flush_write_messages(g_comm_handle, &err_msg);
      }
   }

   shepherd_trace("pty_to_commlib: shutting down thread");
   FREE(stdout_buf);
   FREE(stderr_buf);
   thread_func_cleanup(t_conf);

/* TODO: This could cause race conditions in the main thread, replace with pthread_condition */
   shepherd_trace("pty_to_commlib: raising event for main thread");
   g_raised_event = 2;
   thread_trigger_event(&g_thread_main);
   sge_dstring_free(&err_msg);

   shepherd_trace("pty_to_commlib: leaving commlib_to_pty thread");
   return NULL;
}

/****** commlib_to_pty() *******************************************************
*  NAME
*     commlib_to_pty() -- commlib_to_pty thread entry point and main loop
*
*  SYNOPSIS
*     void* commlib_to_pty(void *t_conf)
*
*  FUNCTION
*     Entry point and main loop of the commlib_to_pty thread.
*     Reads data from the commlib and writes it to the pty.
*
*  INPUTS
*     void *t_conf - pointer to cl_thread_settings_t struct of the thread
*
*  RESULT
*     void* - always NULL
*
*  NOTES
*     MT-NOTE:
*
*  SEE ALSO
*******************************************************************************/
static void* commlib_to_pty(void *t_conf)
{
   recv_message_t       recv_mess;
   int                  b_was_connected = 0;
   bool                 b_sent_to_child = false;
   int                  ret;
   int                  do_exit = 0;
   int                  fd_write = -1;
   dstring              err_msg = DSTRING_INIT;

   /* report to thread lib that thread starts up now */
   thread_func_startup(t_conf);
   thread_setcancelstate(1);

   if (g_ptym != -1) {
      fd_write = g_ptym;
   } else if (g_fd_pipe_in[1] != -1) {
      fd_write = g_fd_pipe_in[1];
   } else {
      do_exit = 1;
      shepherd_trace("commlib_to_pty: no valid handle for stdin available. Exiting!");
   }

   while (do_exit == 0) {
      /* wait blocking for a message from commlib */
      recv_mess.cl_message = NULL;
      recv_mess.data       = NULL;
      sge_dstring_free(&err_msg);
      sge_dstring_sprintf(&err_msg, "");

#ifdef EXTENSIVE_TRACING
      shepherd_trace("commlib_to_pty: waiting in commlib_trigger() for data");
#endif
      ret = cl_commlib_trigger(g_comm_handle, 1);
      ret = comm_recv_message(g_comm_handle, CL_FALSE, &recv_mess, &err_msg);

      /* 
       * Check if the thread was cancelled. Exit thread if it was.
       * It shouldn't be neccessary to do the check here, as the cancel state 
       * of the thread is 1, i.e. the thread may be cancelled at any time,
       * but this doesn't work on some architectures (Darwin, older Solaris).
       */
      thread_testcancel(t_conf);

#ifdef EXTENSIVE_TRACING
      shepherd_trace("commlib_to_pty: comm_recv_message() returned %d, err_msg: %s",
                     ret, sge_dstring_get_string(&err_msg));
#endif

      if (g_raised_event != 0) {
         shepherd_trace("commlib_to_pty: received event -> exiting");
         return NULL;
      }

      if (ret != COMM_RETVAL_OK) {
         /* handle error cases */
         switch (ret) {
            case COMM_NO_SELECT_DESCRIPTORS: 
               /*
                * As long as we're not connected, this return value is expected.
                * If we were already connected, it means the connection was closed.
                */
               if (b_was_connected == 1) {
                  shepherd_trace("commlib_to_pty: was connected, "
                                 "but lost connection -> exiting");
                  do_exit = 1;
               }
               break;
            case COMM_CONNECTION_NOT_FOUND:
               if (b_was_connected == 0) {
                  shepherd_trace("commlib_to_pty: our server is not running -> exiting");
                  /*
                   * On some architectures (e.g. Darwin), the child doesn't recognize
                   * when the pipe gets closed, so we have to send the information
                   * that the parent will exit soon to the child.
                   * "noshell" may be 0 or 1, everything else indicates an error.
                   */
                  if (b_sent_to_child == false) {
                     if (write(g_fd_pipe_to_child[1], "noshell = 9", 11) != 11) {
                        shepherd_trace("commlib_to_pty: error in communicating "
                           "with child -> exiting");
                     } else {
                        b_sent_to_child = true;
                     }
                  }
                  do_exit = 1;
               }
               break;
            case COMM_INVALID_PARAMETER:
               shepherd_trace("commlib_to_pty: communication handle or "
                        "message buffer is invalid -> exiting");
               do_exit = 1;
               break;
            case COMM_CANT_TRIGGER:
               shepherd_trace("commlib_to_pty: can't trigger communication, likely the "
                              "communication was shut down by other thread -> exiting");
               shepherd_trace("commlib_to_pty: err_msg: %s", 
                              sge_dstring_get_string(&err_msg));
               do_exit = 1;
               break;
            case COMM_CANT_RECEIVE_MESSAGE:
               if (check_client_alive(g_comm_handle, &err_msg) != COMM_RETVAL_OK) {
                  shepherd_trace("commlib_to_pty: not connected any more -> exiting.");
                  do_exit = 1;
               } else {
#ifdef EXTENSIVE_TRACING
                  shepherd_trace("commlib_to_pty: can't receive message, reason: %s "
                                 "-> trying again", 
                                 sge_dstring_get_string(&err_msg));
#endif
               }
               b_was_connected = 1;
               break;
            case COMM_GOT_TIMEOUT:
#ifdef EXTENSIVE_TRACING
               shepherd_trace("commlib_to_pty: got timeout -> trying again");
#endif
               b_was_connected = 1;
               break;
            case COMM_SELECT_INTERRUPT:
               /* Don't do tracing here */
               /* shepherd_trace("commlib_to_pty: interrupted select"); */
               b_was_connected = 1;
               break;
            default:
               /* Unknown error, just try again */
#ifdef EXTENSIVE_TRACING
               shepherd_trace("commlib_to_pty: comm_recv_message() returned %d -> "
                              "trying again\n", ret);
#endif
               b_was_connected = 1;
               break;
         }
      } else {  /* if (ret == COMM_RETVAL_OK) { */
         /* We received a message, 'parse' it */
         switch (recv_mess.type) {
            case STDIN_DATA_MSG:
               /* data message, write data to stdin of child */
#ifdef EXTENSIVE_TRACING
               shepherd_trace("commlib_to_pty: received data message");
               shepherd_trace("commlib_to_pty: writing data to stdin of child, "
                              "length = %d", recv_mess.cl_message->message_length-1);
#endif

               if (sge_writenbytes(fd_write,  
                          recv_mess.data, 
                          (int)(recv_mess.cl_message->message_length-1)) 
                       != (int)(recv_mess.cl_message->message_length-1)) {
                  shepherd_trace("commlib_to_pty: error writing to stdin of "
                                 "child: %d, %s", errno, strerror(errno));
               }
               b_was_connected = 1;
               break;

            case WINDOW_SIZE_CTRL_MSG:
               /* control message, set size of pty */
               shepherd_trace("commlib_to_pty: received window size message, "
                  "changing window size");
               ioctl(g_ptym, TIOCSWINSZ, &(recv_mess.ws));
               b_was_connected = 1;
               break;
            case SETTINGS_CTRL_MSG:
               /* control message */
               shepherd_trace("commlib_to_pty: recevied settings message\n");
               /* Forward the settings to the child process.
                * This is also tells the child process that it can start
                * the job 'in' the pty now.
                */
               shepherd_trace("commlib_to_pty: writing to child %d bytes: %s",
                              strlen(recv_mess.data), recv_mess.data);
               if (write(g_fd_pipe_to_child[1], recv_mess.data, 
                         strlen(recv_mess.data)) != strlen(recv_mess.data)) {
                  shepherd_trace("commlib_to_pty: error in communicating "
                     "with child -> exiting");
                  do_exit = 1;
               } else {
                  b_sent_to_child = true;
               }
               b_was_connected = 1;
               break;
            default:
               shepherd_trace("commlib_to_pty: received unknown message");
               break;
         }
      }
      comm_free_message(&recv_mess, &err_msg);
   }

   sge_dstring_free(&err_msg);

   shepherd_trace("commlib_to_pty: leaving commlib_to_pty function\n");
   thread_func_cleanup(t_conf);
/* TODO: pthread_condition, see other thread*/

   shepherd_trace("commlib_to_pty: raising event for main thread");
   g_raised_event = 3;
   thread_trigger_event(&g_thread_main);

   shepherd_trace("commlib_to_pty: leaving commlib_to_pty thread");
   return NULL;
}

int parent_loop(char *hostname, int port, int ptym, 
                int *fd_pipe_in, int *fd_pipe_out, int *fd_pipe_err, 
                int *fd_pipe_to_child, 
                int ckpt_pid, int ckpt_type, int timeout, 
                int ckpt_interval, char *childname,
                char *user_name, int *exit_status, struct rusage *rusage,
                int job_pid, dstring *err_msg)
{
   int               ret;
   THREAD_LIB_HANDLE *thread_lib_handle     = NULL;
   THREAD_HANDLE     *thread_pty_to_commlib = NULL;
   THREAD_HANDLE     *thread_commlib_to_pty = NULL;
   cl_raw_list_t     *cl_com_log_list = NULL;

   g_hostname            = strdup(hostname);
   g_ptym                = ptym;
   g_fd_pipe_in[0]       = fd_pipe_in[0];
   g_fd_pipe_in[1]       = fd_pipe_in[1];
   g_fd_pipe_out[0]      = fd_pipe_out[0];
   g_fd_pipe_out[1]      = fd_pipe_out[1];
   g_fd_pipe_err[0]      = fd_pipe_err[0];
   g_fd_pipe_err[1]      = fd_pipe_err[1];
   g_fd_pipe_to_child[0] = fd_pipe_to_child[0];
   g_fd_pipe_to_child[1] = fd_pipe_to_child[1];
   g_ckpt_pid            = ckpt_pid;
   g_ckpt_type           = ckpt_type;
   g_timeout             = timeout;
   g_ckpt_interval       = ckpt_interval;
   g_childname           = childname;
   g_rusage              = rusage;
   g_exit_status         = exit_status;
   g_job_pid             = job_pid;

   memset(g_rusage, 0, sizeof(*g_rusage));

   shepherd_trace("main: starting parent_loop");

   ret = comm_init_lib(err_msg);
   if (ret != 0) {
      shepherd_trace("main: can't open communication library");
      return 1;
   }
   /*
    * Get log list of communication before a connection is opened.
    */
   cl_com_log_list = cl_com_get_log_list();

   /*
    * Open the connection port so we can connect to our server
    */
   shepherd_trace("main: opening connection to qrsh/qlogin client");
   ret = comm_open_connection(false, port, THISCOMPONENT, g_csp_mode, user_name,
                              &g_comm_handle, err_msg);
   if (ret != COMM_RETVAL_OK) {
      shepherd_trace("main: can't open commlib stream, err_msg = %s", 
                     sge_dstring_get_string(err_msg) != NULL ? 
                     sge_dstring_get_string(err_msg): "<null>");
      return 1;
   }

   /*
    * register at qrsh/qlogin client, which is the server of the communication.
    * The answer of the server (a WINDOW_SIZE_CTRL_MSG) will be handled in the
    * commlib_to_pty thread.
    */
   shepherd_trace("main: sending REGISTER_CTRL_MSG");
   ret = (int)comm_write_message(g_comm_handle, g_hostname, OTHERCOMPONENT, 1, 
                      (unsigned char*)" ", 1, REGISTER_CTRL_MSG, err_msg);
   if (ret == 0) {
      /* No bytes written - error */
      shepherd_trace("main: can't send REGISTER_CTRL_MSG, comm_write_message() "
                     "returned: %s", sge_dstring_get_string(err_msg));
   /* Don't exit here, the error handling is done in the commlib_to_tty-thread */
   /* Most likely, the qrsh client is not running, so it's not the shepherds fault,
    * we shouldn't return 1 (which leads to a shepherd_exit which sets the whole
    * queue in error!).
    */
   /*   return 1;*/
   }

   /*
    * Setup thread list, setup this main thread so it can be triggered
    * and create two worker threads
    */
   ret = thread_init_lib(&thread_lib_handle);

   /* 
    * Tell the thread library that there is a main thread running
    * that can receive events.
    */
   ret = register_thread(thread_lib_handle, &g_thread_main, "main thread");
   if (ret != CL_RETVAL_OK) {
      shepherd_trace("main: registering main thread failed: %d", ret);
      return 1;
   }
   ret = create_thread(thread_lib_handle,
                       &thread_pty_to_commlib,
                       cl_com_log_list,
                       "pty_to_commlib thread",
                       2,
                       pty_to_commlib);
   if (ret != CL_RETVAL_OK) {
      shepherd_trace("main: creating pty_to_commlib thread failed: %d", ret);
   }

   ret = create_thread(thread_lib_handle,
                       &thread_commlib_to_pty,
                       cl_com_log_list,
                       "commlib_to_pty thread",
                       3,
                       commlib_to_pty);
   if (ret != CL_RETVAL_OK) {
      shepherd_trace("main: creating commlib_to_pty thread failed: %d", ret);
   }
   /* From here on, the two worker threads are doing all the work.
    * This main thread is just waiting until one of the to worker threads
    * wants to exit and sends an event to the main thread.
    * A worker thread wants to exit when either the communciation to
    * the server was shut down or the user application (likely the shell)
    * exited.
    */
   shepherd_trace("main: created both worker threads, now waiting for event");
   while (g_raised_event == 0) {
      ret = thread_wait_for_event(&g_thread_main, 0, 0);
   }
   shepherd_trace("main: received event %d, g_raised_event = %d\n", 
                  ret, g_raised_event);

   /* 
    * One of the threads sent an event, so shut down both threads now
    */
   shepherd_trace("main: shutting down pty_to_commlib thread");

   /* 
    * It seems that tracing between thread_shutdown() and thread_join()
    * could cause deadlocks!
    */
   ret = thread_shutdown(thread_pty_to_commlib);
   ret = thread_shutdown(thread_commlib_to_pty);

   close(g_ptym);

   close(g_fd_pipe_in[0]);
   close(g_fd_pipe_in[1]);
   close(g_fd_pipe_out[0]);
   close(g_fd_pipe_out[1]);
   close(g_fd_pipe_err[0]);
   close(g_fd_pipe_err[1]);

   /*
    * Wait until the pty_to_commlib threads have shut down
    */
   thread_join(thread_pty_to_commlib);
   shepherd_trace("main: joined pty_to_commlib thread");

#if 0
   /* This causes problems on some architectures - why? */
   DPRINTF(("main: cl_thread_join(thread_commlib_to_pty)\n"));
   thread_join(thread_commlib_to_pty);
#endif
   /*
    * Wait for all messages to be sent
    * TODO: This shouldn't be neccessary, the shutdown of the
    *       connection should do a flush.
    *       Test it, and if it works, remove this code.
    */
   if (g_comm_handle != NULL) {
      comm_flush_write_messages(g_comm_handle, err_msg);
   }

   /* 
    * Get usage of child processes, if it was not already retrieved 
    */
   {
      int npid;
      int status;
      struct rusage tmp_usage;

      memset(&tmp_usage, 0, sizeof(tmp_usage));

      if (!g_usage_filled_in) {
   #if defined(CRAY) || defined(NECSX4) || defined(NECSX5) || defined(INTERIX)
         npid = waitpid(-1, &status, 0);
   #else
         npid = wait3(&status, 0, &tmp_usage);
         shepherd_trace("main: status = %d, npid = %d", status, npid);
         if (npid == -1) {
            shepherd_trace("main: npid == -1, errno = %d, %s", errno, strerror(errno));
         } else {
            shepherd_trace("main: copying tmp_usage to g_rusage");
            memcpy(g_rusage, &tmp_usage, sizeof(tmp_usage));
            if ((npid == g_job_pid) && ((WIFSIGNALED(status) || WIFEXITED(status)))) {
               shepherd_trace("%s exited with exit status %d", 
                                      childname, WEXITSTATUS(status));
               *g_exit_status = status;
               g_job_pid = -999;
            }   
         }
   #endif
      }
   }
#if 0
{
struct timeb ts;
ftime(&ts);
shepherd_trace("+++++ timestamp: %d.%03d ++++", (int)ts.time, (int)ts.millitm);
}
#endif
   /* From here on, only the main thread is running */
   thread_cleanup_lib(&thread_lib_handle);

   /* The communication will be freed in close_parent_loop() */
   sge_dstring_free(err_msg);
   return 0;
}

int close_parent_loop(int exit_status)
{
   int     ret = 0;
   char    sz_exit_status[21]; 
   dstring err_msg = DSTRING_INIT;

   /*
    * Send UNREGISTER_CTRL_MSG
    */
   snprintf(sz_exit_status, 20, "%d", exit_status);
   shepherd_trace("sending UNREGISTER_CTRL_MSG with exit_status = \"%s\"", 
                          sz_exit_status);
   shepherd_trace("sending to host: %s", 
                          g_hostname != NULL ? g_hostname : "<null>");
   ret = (int)comm_write_message(g_comm_handle, g_hostname,
      OTHERCOMPONENT, 1, (unsigned char*)sz_exit_status, strlen(sz_exit_status), 
      UNREGISTER_CTRL_MSG, &err_msg);

   if (ret != strlen(sz_exit_status)) {
      shepherd_trace("comm_write_message returned: %s", 
                             sge_dstring_get_string(&err_msg));
      shepherd_trace("close_parent_loop: comm_write_message() returned %d "
                             "instead of %d!!!", ret, strlen(sz_exit_status));
   }

   /*
    * Wait for UNREGISTER_RESPONSE_CTRL_MSG
    */
   {
      int                  count = 0;
      recv_message_t       recv_mess;

      shepherd_trace("waiting for UNREGISTER_RESPONSE_CTRL_MSG");
      while (count < RESPONSE_MSG_TIMEOUT) {
         ret = comm_recv_message(g_comm_handle, CL_TRUE, &recv_mess, &err_msg);
         if (ret == COMM_GOT_TIMEOUT) {
            count++;
         } else if (recv_mess.type == UNREGISTER_RESPONSE_CTRL_MSG) {
            shepherd_trace("Received UNREGISTER_RESPONSE_CTRL_MSG");
            break;
         } else {
            shepherd_trace("No connection or timeout while waiting for message");
            break;
         }
         comm_free_message(&recv_mess, &err_msg);
      }
   }
   /* Now we are completely logged of from the server and can shut down */

   /* 
    * Tell the communication to shut down immediately, don't wait for 
    * the next timeout
    */
   shepherd_trace("main: cl_com_ignore_timeouts");
   comm_ignore_timeouts(true, &err_msg);

   /*
    * Do cleanup
    */
   shepherd_trace("main: cl_com_cleanup_commlib()");
   comm_shutdown_connection(g_comm_handle, &err_msg);
   shepherd_trace("main: comm_cleanup_lib()");
   ret = comm_cleanup_lib(&err_msg);
   if (ret != COMM_RETVAL_OK) {
      shepherd_trace("main: error in comm_cleanup_lib(): %d", ret);
   }

   FREE(g_hostname);
   sge_dstring_free(&err_msg);
   return 0;
}

