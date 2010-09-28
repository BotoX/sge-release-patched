#ifndef _READ_WRITE_HOST_GROUP_H
#define _READ_WRITE_HOST_GROUP_H
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

int
read_host_group_work(lList **alpp, lList **clpp, int fields[], lListElem *ep,
int spool, int flag, int *tag, int parsing_type);

lListElem *cull_read_in_host_group(const char *dirname, const char *filename, int spool, int flag, int *tag, int fields[]);

char *write_host_group(int spool, int how, const lListElem *hostGroupElement);

int
read_centry_work(lList **alpp, lList **clpp, int fields[], lListElem *ep,
int spool, int flag, int *tag, int parsing_type);

char *write_centry(int spool, int how, const lListElem *hostGroupElement);

#endif /* _READ_WRITE_HOST_GROUP_H */
