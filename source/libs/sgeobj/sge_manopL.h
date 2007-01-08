#ifndef _SGE_MANOPL_H_
#define _SGE_MANOPL_H_

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

#include "sge_boundaries.h"
#include "cull.h"

#ifdef  __cplusplus
extern "C" {
#endif

/* *INDENT-OFF* */

/* 
 * This is the list type we use to hold the manager list 
 * and the operator list in the qmaster. 
 */

enum {
   MO_name = MO_LOWERBOUND
};

LISTDEF(MO_Type)
   JGDI_PRIMITIVE_OBJ(MO_name)
   SGE_STRING(MO_name, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SPOOL | CULL_CONFIGURE)
LISTEND

DERIVED_LISTDEF(Manager_Type, MO_Type)
   JGDI_ROOT_OBJ(Manager, SGE_MANAGER_LIST, ADD | DELETE | GET_LIST)
   JGDI_EVENT_OBJ(ADD(sgeE_MANAGER_ADD) | MODIFY(sgeE_MANAGER_MOD) | DELETE(sgeE_MANAGER_DEL) | GET_LIST(sgeE_MANAGER_LIST))
DERIVED_LISTEND

DERIVED_LISTDEF(Operator_Type, MO_Type)
   JGDI_ROOT_OBJ(Operator, SGE_OPERATOR_LIST, ADD | DELETE | GET_LIST)
   JGDI_EVENT_OBJ(ADD(sgeE_OPERATOR_ADD) | MODIFY(sgeE_OPERATOR_MOD) | DELETE(sgeE_OPERATOR_DEL) | GET_LIST(sgeE_OPERATOR_LIST))
DERIVED_LISTEND

NAMEDEF(MON)
   NAME("MO_name")
NAMEEND

/* *INDENT-ON* */ 

#define MOS sizeof(MON)/sizeof(char*)
#ifdef  __cplusplus
}
#endif
#endif                          /* _SGE_MANOPL_H_ */
