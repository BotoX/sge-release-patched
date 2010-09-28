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

#if defined(DARWIN) || defined(FREEBSD)
#  include <sys/param.h>
#  include <sys/mount.h>
#elif defined(LINUX)
#  include <sys/vfs.h>
#else
#  include <sys/types.h>
#  include <sys/statvfs.h>
#endif

#include <stdio.h>

int main(int argc, char *argv[]) {

  if (argc < 2) {
     printf("Usage: fstype <directory>\n");
     return 1;
  }
   else {  
#if defined(LINUX) || defined(DARWIN) || defined(FREEBSD)
   struct statfs buf;
   statfs(argv[1], &buf);
#else   
   struct statvfs buf;
   statvfs(argv[1], &buf);
#endif
  
#if defined (DARWIN) || defined(FREEBSD)
   printf("%s\n", buf.f_fstypename);
#elif defined(LINUX)
   if (buf.f_type == 0x6969)
      printf("nfs\n");
   else   
      printf("%lx\n", (long unsigned int)buf.f_type);
#else
   printf("%s\n", buf.f_basetype);
#endif
   }
   return 0;
}
