#ifndef __SGERMON_H
#define __SGERMON_H
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

#ifndef NO_SGE_COMPILE_DEBUG

#include "rmon.h"


#define DENTER_MAIN(layer, program)        \
   static const char SGE_FUNC[] = "main";  \
   static const int xaybzc = layer;        \
                                           \
   rmon_mopen(&argc,argv,program);         \
   if (rmon_condition(xaybzc, TRACE))      \
      rmon_menter (SGE_FUNC)
 
#define DENTER(layer, function)             \
   static const char SGE_FUNC[] = function; \
   static const int xaybzc = layer;         \
                                            \
   if (rmon_condition(xaybzc, TRACE))       \
      rmon_menter (SGE_FUNC)

#define DRETURN(ret)                            \
   if (rmon_condition(xaybzc, TRACE))           \
      rmon_mexit(SGE_FUNC, __FILE__, __LINE__); \
   return ret

#define DEXIT                                   \
   if (rmon_condition(xaybzc, TRACE))           \
      rmon_mexit(SGE_FUNC, __FILE__, __LINE__)

#define DTRACE                                \
   if (rmon_condition(xaybzc, TRACE))         \
      rmon_mtrace(SGE_FUNC, __FILE__, __LINE__)

#define DLOCKPRINTF(msg)             \
   if (rmon_condition(xaybzc, LOCK)) \
      rmon_mprintf msg

#define DPRINTF(msg)                        \
   if (rmon_condition(xaybzc, INFOPRINT))   \
      rmon_mprintf msg

#define DTIMEPRINTF(msg)                \
   if (rmon_condition(xaybzc, TIMING))  \
      rmon_mprintf msg

#define DSPECIALPRINTF(msg)              \
   if (rmon_condition(xaybzc, SPECIAL))  \
      rmon_mprintf msg

#define ISTRACE (rmon_condition(xaybzc, TRACE))

#define TRACEON  (rmon_is_enabled() && !rmon_mliszero(&DEBUG_ON))

#define DCLOSE

#define SGE_EXIT(x) sge_exit(x)

#else /* NO_SGE_COMPILE_DEBUG */

#define DENTER_MAIN( layer, program )
#define DENTER( layer, function)
#define DEXIT
#define DTRACE
#define DLOCKPRINTF(x)
#define DPRINTF(x)
#define DTIMEPRINTF(x)
#define DSPECIALPRINTF(x)
#define DCLOSE
#define TRACEON
#define ISTRACE
#define SGE_EXIT(x)     sge_exit(x)

#endif /* NO_SGE_COMPILE_DEBUG */

#endif /* __SGERMON_H */
