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
/* Error and trace handling for shepherd 
 * Error/Trace files will go to the actual directory at first time 
 * shepherd_error() and trace() are called 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>

#if defined(DARWIN) || defined(FREEBSD)
#  include <sys/param.h>
#  include <sys/mount.h>
#elif defined(LINUX)
#  include <sys/vfs.h>
#else
#  include <sys/types.h>
#  include <sys/statvfs.h>
#endif

#include "sge_dstring.h"
#include "basis_types.h"
#include "err_trace.h"
#include "sge_time.h"
#include "sge_uidgid.h"
#include "config_file.h"
#include "qlogin_starter.h"
#include "sge_unistd.h"

#if defined(INTERIX)
#  include "misc.h"
#endif

static FILE *shepherd_error_fp=NULL;
static FILE *shepherd_exit_status_fp=NULL;
static FILE *shepherd_trace_fp = NULL;

typedef enum st_shepherd_file_def {
   st_trace,
   st_exit_status,
   st_error
} st_shepherd_file_t;

static char *g_shepherd_file_name[3] = {"trace", "exit_status", "error"};
static char  g_shepherd_file_path[3][SGE_PATH_MAX];

static char g_job_owner[SGE_PATH_MAX] = "";
static bool g_keep_files_open = true; /* default: Open files at start and keep
                                                  them open for writing */

extern pid_t coshepherd_pid;
extern int   shepherd_state;  /* holds exit status for shepherd_error() */
int foreground = 1;           /* usability of stderr/out */

/* Forward declaration of static functions */

static void  dup_fd(int *src_fd);
static int   set_cloexec(int fd);
static int   sh_str2file(const char *header_str, const char *str, FILE *fp);
static FILE* shepherd_trace_init_intern(st_shepherd_file_t shepherd_file);
static void  shepherd_trace_chown_intern(const char* job_owner, FILE* fp,
                                         st_shepherd_file_t shepherd_file);
static bool  nfs_mounted(const char *path);
static void  shepherd_panic(const char *s);


/******************************************************************************
* "Public" functions 
*******************************************************************************/
/****** shepherd_trace_init/shepherd_trace_exit *******************************
*  NAME
*     shepherd_trace_init() -- Begin shepherd's tracing.
*     shepherd_trace_exit() -- End shepherd's tracing. 
*
*  SYNOPSIS
*    void shepherd_trace_init( void )
*    void shepherd_trace_exit( void )
*
*  FUNCTION
*     Init: Creates or opens trace file.
*     Exit: Closes trace file. Call it as shepherd's last function.
*
*  INPUTS
*     void - none
*
*  RESULT
*     void - none
*******************************************************************************/
void shepherd_trace_init( void )
{
	if( !shepherd_trace_fp ) {
		shepherd_trace_fp = shepherd_trace_init_intern( st_trace );
	}
}

void shepherd_trace_exit( void )
{
	if( shepherd_trace_fp ) {
		fclose( shepherd_trace_fp );
      shepherd_trace_fp=NULL;
	}
	shepherd_error_exit();
}

/****** shepherd_trace_chown **************************************************
*  NAME
*     shepherd_trace_chown() -- Change owner of trace file.
*
*  SYNOPSIS
*    void shepherd_trace_chown( const char* job_owner )
*
*  FUNCTION
*     Init: Creates or opens trace file.
*     Exit: Closes trace file. Call it as shepherd's last function.
*
*  INPUTS
*     void - none
*
*  RESULT
*     void - none
*******************************************************************************/
void shepherd_trace_chown( const char* job_owner )
{
	shepherd_trace_chown_intern( job_owner, shepherd_trace_fp, st_trace );
}

/****** err_trace/shepherd_trace_exit() **************************************
*  NAME
*     shepherd_trace_exit() -- End shepherd's tracing. 
*
*  SYNOPSIS
*     static FILE* shepherd_trace_exit( )
*
*  FUNCTION
*     Closes the shepherd's trace file.
*     Call it as shepherd's last function.
*
*  INPUTS
*     void - none
*
*  RESULT
*     void - none
*******************************************************************************/
void shepherd_error_init( )
{
	if( !shepherd_error_fp ) {
		shepherd_error_fp = shepherd_trace_init_intern( st_error );
	}
	if( !shepherd_exit_status_fp ) {
		shepherd_exit_status_fp = shepherd_trace_init_intern( st_exit_status );
	}
}

void shepherd_error_exit( void )
{
	if( shepherd_error_fp ) {
		fclose( shepherd_error_fp );
      shepherd_error_fp=NULL;
	}
	if( shepherd_exit_status_fp ) {
		fclose( shepherd_exit_status_fp );
      shepherd_exit_status_fp=NULL;
	}	
}

void shepherd_error_chown( const char* job_owner )
{
	shepherd_trace_chown_intern( job_owner, shepherd_error_fp, st_error );
	shepherd_trace_chown_intern( job_owner, shepherd_exit_status_fp, st_exit_status );
}

/*-----------------------------------------------------------------*/
int shepherd_trace(const char *str) 
{
   dstring ds;
   char    buffer[128];
   char    header_str[256];
   int     ret=1;
	struct stat statbuf;

	/* File was closed (e.g. by an exec()) but fp was not set to NULL */
	if( shepherd_trace_fp 
	    && fstat( fileno( shepherd_trace_fp ), &statbuf )==-1
		 && errno==EBADF ) {
		shepherd_trace_fp = NULL;
	}
	
	if( !shepherd_trace_fp ) {
		shepherd_trace_fp = shepherd_trace_init_intern( st_trace );
	}

	if( shepherd_trace_fp ) {
   	sge_dstring_init(&ds, buffer, sizeof(buffer));

   	sprintf(header_str, "%s ["uid_t_fmt":"pid_t_fmt"]: ",
			sge_ctime(0, &ds), geteuid(), getpid());

   	ret = sh_str2file(header_str, str, shepherd_trace_fp );

   	if (foreground) {
      	printf("%s%s\n", header_str, str);
      	fflush(stdout);
   	}
      /* There are cases where we have to open and close the files 
       * for every write.
       */
      if(!g_keep_files_open) {
         shepherd_trace_exit();
      }
		ret=0;	
	}
   return ret;
}

/*-----------------------------------------------------------------*/

int shepherd_trace_sprintf(const char *format, ...)
{
   int ret = 1;
   dstring message = DSTRING_INIT;
   va_list ap;

   va_start(ap, format);
   if (format) {
      sge_dstring_vsprintf(&message, format, ap);
      ret = shepherd_trace(sge_dstring_get_string(&message));
      sge_dstring_free(&message);
   }
   return ret;
}

/*-----------------------------------------------------------------*/


void shepherd_error(const char *str) 
{
   shepherd_error_impl(str, 1);
}

void shepherd_error_impl(const char *str, int do_exit) 
{
   dstring ds;
   char    buffer[128];
   char    header_str[256];
	struct stat statbuf;

	/* File was closed (e.g. by an exec()) but fp was not set to NULL */
	if( shepherd_error_fp 
	    && fstat( fileno( shepherd_error_fp ), &statbuf )==-1
		 && errno==EBADF ) {
		shepherd_error_fp = NULL;
	}
	if( !shepherd_error_fp ) {
		shepherd_error_fp = shepherd_trace_init_intern( st_error );
	}
	if( shepherd_error_fp ) {
   	sge_dstring_init(&ds, buffer, sizeof(buffer));
     
   	sprintf(header_str, "%s ["uid_t_fmt":"pid_t_fmt"]: ",
			sge_ctime(0, &ds), geteuid(), getpid());

	   sh_str2file(header_str, str, shepherd_error_fp);
	}

   if (foreground)
      fprintf(stderr, "%s%s\n", header_str, str);

	/* File was closed (e.g. by an exec()) but fp was not set to NULL */
	if( shepherd_exit_status_fp 
	    && fstat( fileno( shepherd_exit_status_fp ), &statbuf )==-1
	    && errno==EBADF ) {
		shepherd_exit_status_fp = NULL;
	}
	if( !shepherd_exit_status_fp ) {
		shepherd_exit_status_fp = shepherd_trace_init_intern( st_exit_status );
	}
	if( shepherd_exit_status_fp ) {
   	sprintf(header_str, "%d", shepherd_state);
   	sh_str2file(header_str, NULL, shepherd_exit_status_fp);
	}
	
   if (coshepherd_pid > 0) {
      sge_switch2start_user();
      kill(coshepherd_pid, SIGTERM);
      sge_switch2admin_user();
   }   
     
   if(search_conf_val("qrsh_control_port") != NULL) {
      char buffer[1024];
      strcpy(buffer, "1:");
      strncat(buffer, str, 1021);
      write_to_qrsh(buffer);  
   }
   if (do_exit) {
		/* close all trace files before exit */
		shepherd_trace_exit( );
      exit(shepherd_state);
	}
   /* There are cases where we have to open and close the files 
    * for every write.
    */
   if(!g_keep_files_open) {
      shepherd_error_exit();
   }
   return;
}

void shepherd_write_exit_status( const char *exit_status )
{
	struct stat statbuf;
	int old_euid=0;

	if( exit_status ) {
		/* set euid=0. Local files: root can write to every file.
		 * NFS files: everyone is allowed to write to exit_status file.
		 */
		if( getuid()==0 ) {
			old_euid = geteuid();
         seteuid(0);
		}	
		/* File was closed (e.g. by an exec()) but fp was not set to NULL */
		if( shepherd_exit_status_fp 
	    	 && fstat( fileno( shepherd_exit_status_fp ), &statbuf )==-1
	    	 && errno==EBADF ) {
			shepherd_exit_status_fp = NULL;
		}
		if( !shepherd_exit_status_fp ) {
			shepherd_exit_status_fp = shepherd_trace_init_intern( st_exit_status );
		}
		if( shepherd_exit_status_fp ) {
   		sh_str2file(exit_status, NULL, shepherd_exit_status_fp);
		} else {
         shepherd_trace("could not write exit_status file\n");
      }
		if( old_euid!=0 ) {
			seteuid( old_euid );
		}
      /* There are cases where we have to open and close the files 
       * for every write.
       */
      if(!g_keep_files_open) {
         shepherd_error_exit();
      }
	}
}

void shepherd_error_sprintf(const char *format, ...)
{
   dstring message = DSTRING_INIT;
   va_list ap;

   va_start(ap, format);
   if (format) {
      sge_dstring_vsprintf(&message, format, ap);
      shepherd_error_impl((char*)sge_dstring_get_string(&message), 1);
      sge_dstring_free(&message);
   }
}

int is_shepherd_trace_fd( int fd )
{
   if(   (shepherd_trace_fp && fd==fileno( shepherd_trace_fp ))
      || (shepherd_error_fp && fd==fileno( shepherd_error_fp ))
      || (shepherd_exit_status_fp && fd==fileno( shepherd_exit_status_fp ))) {
      return 1;
   } else {
      return 0;
   }
}

/*********************************************/
/* return number entries in exit status file */
/*********************************************/
int count_exit_status(void)
{
   int n = 0;
   SGE_STRUCT_STAT sbuf;
   char buf[1024];

   if (!SGE_STAT("exit_status", &sbuf) && sbuf.st_size) {
      FILE *fp;
      int dummy;

      if ((fp = fopen("exit_status", "r"))) {
         while (fgets(buf, sizeof(buf), fp)) 
            if (sscanf(buf, "%d\n", &dummy)==1)
               n++;
         fclose(fp);
      } 
   }

   return n;
}


/**********************************/
/* Static functions */

static void dup_fd( int *src_fd )
{
	int dup_fd[3];

	dup_fd[0]=dup( *src_fd );
	dup_fd[1]=dup( *src_fd );
	dup_fd[2]=dup( *src_fd );

	close( *src_fd );
	close( dup_fd[0] );
	close( dup_fd[1] );

	*src_fd = dup_fd[2];
}
	
static int set_cloexec( int fd )
{
	int oldflags;

	oldflags = fcntl( fd, F_GETFD, 0 );
	if( oldflags < 0 ) {
		return 0;
	} 

	oldflags |= FD_CLOEXEC;
   fcntl( fd, F_SETFD, oldflags );

	return 1;
}

/*-----------------------------------------------------------------*/
static int sh_str2file(const char *header_str, const char *str, FILE* fp ) 
{
	int ret=1;

	if( fp ) {
   	if (!str && !header_str)
      	fprintf(fp, "function sh_str2file() called with NULL arguments\n");
   	else if (!header_str && str)
      	fprintf(fp, "%s\n", str);
   	else if (header_str && !str)
      	fprintf(fp, "%s\n", header_str);
   	else
      	fprintf(fp, "%s%s\n", header_str, str);

		fflush( fp );
		ret = 0;
	}
   return ret;
}

/****** err_trace/shepherd_trace_init_intern() *******************************
*  NAME
*     shepherd_trace_init_intern() -- Initialize shepherd's tracing. 
*
*  SYNOPSIS
*     static FILE* shepherd_trace_init( char *trace_file_path,
*													 char *trace_file_name )
*
*  FUNCTION
*     Opens the shepherd's trace file and sets the FD_CLOEXEC-flag so it will
*     be closed automatically in an exec()-call.
*     Must be called with euid=admin user to work properly!
*
*  INPUTS
*     char *trace_file_path - either the whole path of the trace file (including
*                             the file itself)
*                             or NULL to retrieve the file pointer of an already
*                             opened trace file.
*     char *trace_file_name - the name of the trace file itself. Ignored when
*                             *trace_file_path is NULL.
* 
*  RESULT
*     FILE* - If successfully opened, the file pointer of shepherd's trace file.
*           - Otherwise NULL.
*******************************************************************************/
static FILE* shepherd_trace_init_intern( st_shepherd_file_t shepherd_file )
{
   static char path[SGE_PATH_MAX];
	static int  called=0;
	char        tmppath[SGE_PATH_MAX];
   int         fd = -1;
   FILE        *fp = NULL;
   dstring     ds;
   char        buffer[SGE_PATH_MAX+128];
   SGE_STRUCT_STAT statbuf;
   int         do_chown=0;
   int         old_euid=-1;

  	/* 
  	 *  after changing into the jobs cwd we need an 
  	 *  absolute path to the error/trace file 
  	 */
	if( !called ) { 
   	getcwd(path, sizeof(path)); 
		called++;
	}

  	sprintf(tmppath, "%s/%s",path, g_shepherd_file_name[shepherd_file]);
   strncpy(g_shepherd_file_path[shepherd_file], tmppath, SGE_PATH_MAX);

	/* If the file does not exist, create it. Otherwise just open it. */
	if( SGE_STAT( tmppath, &statbuf )) {
	   fd = open( tmppath, O_RDWR | O_CREAT | O_APPEND, 0644 );
      if (fd<0) {
         sge_dstring_init(&ds, buffer, sizeof(buffer));
         sge_dstring_sprintf(&ds, "creat(%s) failed: %s", tmppath, strerror(errno));
         shepherd_panic(buffer);
      }

		if( getuid()==0 ) {
         /* We must give the file to the job owner later */
			do_chown = 1;
		} else {
         /* We are not root, so we have to own all files anyway. */
         do_chown = 0;
		}
	} else {
      /* The file already exists. We get here when
       * a) a exec() failed or
       * b) after the execution of prolog/job, when the job/epilog
       * tries to init the error/exit status files.
       *
       * In a root system we can just open the file, because we are either
       * root or the job user who owns the file.
       * In a admin user system we must set our euid to root to open it, then
       * it is the same as the root system.
       * In a test user system we are the owner of the file and can open it.
       *
       * When we are root (masked or not), we gave this file to the
       * prolog user/job user right after its creation. But we can have
       * 3 different users for prolog, job and epilog, so we must give
       * the file here to the next user.
       * This must be done via shepherd_trace_chown() in the shepherd
       * before we switch to this user there.
       * It can't be done here because we don't know if we are in 
       * case a) (exec failed) or case b) (after execution of prolog/job).
       */
      if( getuid()==0 ) {
         old_euid = geteuid();
         seteuid(0);
      }

      fd = open( tmppath, O_RDWR | O_APPEND );
      if (fd<0) {
         sge_dstring_init(&ds, buffer, sizeof(buffer));
         sge_dstring_sprintf(&ds, "open(%s) failed: %s", tmppath, strerror(errno));
         shepherd_panic(buffer);
      }

      if( old_euid>0 ) {
         seteuid( old_euid );
      }
      do_chown = 0;
	}

	/* Something went wrong. */
	if( fd<0 ) {
		return NULL;
	}

	/* To avoid to block stdin, stdout or stderr, dup the fd until it is >= 3 */
	if( fd<3 ) {
		dup_fd( &fd );
	}

	/* Set FD_CLOEXEC flag to automatically close the file in an exec() */
	if( !set_cloexec( fd )) {
      shepherd_panic("set_cloexec() failed");
		return NULL;
	}

	/* Now open a FILE* from the file descriptor, so we can use fprintf() */
	fp = fdopen( fd, "a" );
   if( !fp ) {
      sge_dstring_init(&ds, buffer, sizeof(buffer));
      sge_dstring_sprintf(&ds, "can't open %s file \"%s\": %s\n",
				  g_shepherd_file_name[shepherd_file], tmppath, strerror(errno));
      shepherd_panic(buffer);
      return NULL;
   }
	if( do_chown && strlen( g_job_owner )>0 ) {
		shepherd_trace_chown_intern( g_job_owner, fp, shepherd_file );
	}
	return fp;
}

static void shepherd_panic(const char *s)
{
   FILE *panic_fp;
   char panic_file[255];

   sprintf(panic_file, "/tmp/shepherd."pid_t_fmt, getpid());
   panic_fp = fopen(panic_file, "a");
   if (panic_fp) {
      dstring ds;
      char buffer[128];
      sge_dstring_init(&ds, buffer, sizeof(buffer));
      fprintf(panic_fp, "%s ["uid_t_fmt":"uid_t_fmt" "pid_t_fmt"]: PANIC: %s\n",
           sge_ctime(0, &ds), getuid(), geteuid(), getpid(), s);
      fclose(panic_fp);
   }
}

/* In an admin user system, this function must be called as admin user! */
static void shepherd_trace_chown_intern( const char* job_owner, FILE* fp,
                                         st_shepherd_file_t shepherd_file )
{
	int   fd;
	uid_t jobuser_id;
	int   old_euid;
   char buffer[1024];

	if( fp && strlen( job_owner )>0 ) {
		fd = fileno( fp );
      
      /* If uid != 0, the system is installed as a test user system.
       * We don't have to change any file ownerships there. */
		if( getuid()==0 ) {
			/* root */
			strcpy( g_job_owner, job_owner );
			if( sge_user2uid( job_owner, &jobuser_id, 1 )==0 )
      	{
				/* Now try to give the file to the job user. root (and later
             * the admin user) will still be able to write to it through
             * the open file descriptor.
             * We must do this as root, because only root has the permissons
             * to change the ownership of a file. 
 	 	 	 	 */
				old_euid = geteuid();
				seteuid( 0 );
           
            /* Have to use chown() here, because fchown() has some bugs
             * on True64 and Irix.*/
            if(chown(g_shepherd_file_path[shepherd_file], jobuser_id, -1 )!=0) {
			 		/* chown failed. This means that user root is a normal user
                * for the file system (due to NFS rights). So we have no
                * other chance than open the file for writing for everyone. 
                * We must do this as file owner = admin user.
	    	 	 	 */
					seteuid( old_euid );
	   			if( fchmod( fd, 0666 )==-1) {
                  sprintf(buffer, "can't fchmod(fd, 0666): %s\n", strerror(errno));
                  shepherd_panic(buffer);
                  return;
					}
   			} else {
               /* chown worked. But when we can chown but are on a NFS
                * mounted drive (which means root has all privileges on this
                * mounted drive), we cannot append from two places
                * (shepherd and son/job) to the shepherd files. So we
                * have to open and close the files for every write, which
                * is possible because we are root with all privileges
                * and we give the ownership to the
                * prolog/pe_start/job/pe_stop/epilog user.
                */
               if(g_keep_files_open && nfs_mounted(".")) {
                  g_keep_files_open = false;
               }
            }
				seteuid( old_euid );
			} else {
				/* Can't get jobuser_id -> grant access for everyone */
				if (fchmod( fd, 0666 )==-1) {
               sprintf(buffer, "could not fchmod(): %s", strerror(errno));
               shepherd_panic(buffer);
            }
			}
		}
	}	
}

static bool nfs_mounted(const char *path)
{
   bool ret=true;

#if defined(LINUX) || defined(DARWIN) || defined(FREEBSD)
   struct statfs buf;
   statfs(path, &buf);
#elif defined(INTERIX)
   struct statvfs buf;
   wl_statvfs(path, &buf);
#else  
   struct statvfs buf;
   statvfs(path, &buf);
#endif
  
#if defined (DARWIN) || defined(FREEBSD)
   ret = (strcmp("nfs", buf.f_fstypename)==0);
#elif defined(LINUX)
   ret = (buf.f_type == 0x6969);
#elif defined(INTERIX)
   ret = (strncasecmp("nfs", buf.f_fstypename, 3)==0);
#else
   ret = (strncmp("nfs", buf.f_basetype, 3)==0);
#endif
   return ret;
}

