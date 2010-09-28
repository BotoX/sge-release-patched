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
 *   Copyright: 2003 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "test_sge_lock_main.h"

#include <unistd.h>
#include <stdio.h>
#include "sge_lock.h"


static void *thread_function(void *anArg);

int get_thrd_demand(void)
{
   long p = 2;  /* min num of threads */

#if defined(SOLARIS)
   p = sysconf(_SC_NPROCESSORS_ONLN);
#endif

   return (int)p;
}

void *(*get_thrd_func(void))(void *anArg)
{
   return thread_function;
}

void *get_thrd_func_arg(void)
{
   return NULL;
}

/****** test_sge_lock_multiple/thread_function() *********************************
*  NAME
*     thread_function() -- Thread function to execute 
*
*  SYNOPSIS
*     static void* thread_function(void *anArg) 
*
*  FUNCTION
*     Acquire multiple locks and sleep. Release the locks. After each 'sge_lock()'
*     and 'sge_unlock()' sleep to increase the probability of interlocked execution. 
*     Note that we deliberately test the boundaries of 'sge_locktype_t'.
*
*  INPUTS
*     void *anArg - thread function arguments 
*
*  RESULT
*     static void* - none
*
*  SEE ALSO
*     test_sge_lock_multiple/get_thrd_func()
*******************************************************************************/
static void *thread_function(void *anArg)
{
   DENTER(TOP_LAYER, "thread_function");

   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);
   sleep(3);

   SGE_LOCK(LOCK_MASTER_QUEUE_LST, LOCK_READ);
   sleep(3);

   SGE_LOCK(LOCK_MASTER_PROJECT_LST, LOCK_READ);
   sleep(3);

   DPRINTF(("Thread %u sleeping\n", sge_locker_id()));
   sleep(5);

   SGE_UNLOCK(LOCK_MASTER_PROJECT_LST, LOCK_READ);
   sleep(3);

   SGE_UNLOCK(LOCK_MASTER_QUEUE_LST, LOCK_READ);
   sleep(3);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
   sleep(3);

   DEXIT;
   return (void *)NULL;
} /* thread_function */
