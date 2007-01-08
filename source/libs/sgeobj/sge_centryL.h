#ifndef __SGE_COMPLEXL_H
#define __SGE_COMPLEXL_H

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
 * This is the list type we use to hold the complex list in qmaster.
 *
 * We also use it for the queue information which administrator defined 
 * complexes aply to this queue. In this case CX_entries is unused. 
 * At the moment this applies only for the gdi. Internal the old list is
 * used.  
 */

/* relops in CE_relop */
enum {
   CMPLXEQ_OP = 1,
   CMPLXGE_OP,
   CMPLXGT_OP,
   CMPLXLT_OP,
   CMPLXLE_OP,
   CMPLXNE_OP
};

enum {
   REQU_NO = 1,
   REQU_YES,
   REQU_FORCED
};

/* bit mask for CE_dominant */
enum {
   DOMINANT_LAYER_GLOBAL = 0x0001,
   DOMINANT_LAYER_HOST = 0x0002,
   DOMINANT_LAYER_QUEUE = 0x0004,
   DOMINANT_LAYER_RQS = 0x0008,
   DOMINANT_LAYER_MASK = 0x00ff,        /* all layers */

   DOMINANT_TYPE_VALUE = 0x0100,        /* value from complex template */
   DOMINANT_TYPE_FIXED = 0x0200,        /* fixed value from object
                                         * configuration */
   DOMINANT_TYPE_LOAD = 0x0400,         /* load value */
   DOMINANT_TYPE_CLOAD = 0x0800,        /* corrected load value */
   DOMINANT_TYPE_CONSUMABLE = 0x1000,   /* consumable */
   DOMINANT_TYPE_MASK = 0xff00          /* all types */
};

/* tag level*/
enum{
   NO_TAG = 0,
   QUEUE_TAG,
   HOST_TAG,
   GLOBAL_TAG,
   PE_TAG,     /* not really used as a tag */
   RQS_TAG,
   MAX_TAG
};

#define CENTRY_LEVEL_TO_CHAR(level) "NQHGPLM"[level]

enum {
   CE_name = CE_LOWERBOUND,
   CE_shortcut,
   CE_valtype,
   CE_stringval,
   CE_doubleval,
   CE_relop,
   CE_consumable,
   CE_default,
   CE_dominant,
   CE_pj_stringval,          /* per job */
   CE_pj_doubleval,
   CE_pj_dominant,
   CE_requestable,
   CE_tagged,
   CE_urgency_weight
};

LISTDEF(CE_Type)
   JGDI_ROOT_OBJ(ComplexEntry, SGE_CENTRY_LIST, ADD | MODIFY | DELETE | GET | GET_LIST)
   JGDI_EVENT_OBJ(ADD(sgeE_CENTRY_ADD) | MODIFY(sgeE_CENTRY_MOD) | DELETE(sgeE_CENTRY_DEL) | GET_LIST(sgeE_CENTRY_LIST))
   SGE_STRING(CE_name, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SPOOL | CULL_SUBLIST | CULL_PRIMARY_KEY | CULL_CONFIGURE)          /* full name of attribute */
   SGE_STRING_D(CE_shortcut, CULL_HASH | CULL_UNIQUE | CULL_SPOOL | CULL_CONFIGURE, "NONE")      /* shortcut name of attribute */
   SGE_ULONG_D(CE_valtype, CULL_DEFAULT | CULL_SPOOL | CULL_CONFIGURE, TYPE_INT)        /* type */
   SGE_STRING(CE_stringval, CULL_DEFAULT | CULL_SPOOL | CULL_SUBLIST | CULL_CONFIGURE)     /* non overwritten value */
   SGE_DOUBLE(CE_doubleval, CULL_DEFAULT | CULL_JGDI_HIDDEN)    /* parsed CE_stringval */
   SGE_ULONG(CE_relop, CULL_DEFAULT | CULL_SPOOL | CULL_CONFIGURE)          /* relational operator */
   SGE_BOOL(CE_consumable, CULL_DEFAULT | CULL_SPOOL | CULL_CONFIGURE)      /* flag consumable */
   SGE_STRING(CE_default, CULL_DEFAULT | CULL_SPOOL | CULL_CONFIGURE)      /* default request for consumable */
   SGE_ULONG(CE_dominant, CULL_DEFAULT | CULL_JGDI_HIDDEN)      /* monitoring facility */
   SGE_STRING(CE_pj_stringval, CULL_DEFAULT | CULL_JGDI_HIDDEN) /* per job string value */
   SGE_DOUBLE(CE_pj_doubleval, CULL_DEFAULT | CULL_JGDI_HIDDEN) /* per job parsed CE_stringval */
   SGE_ULONG(CE_pj_dominant, CULL_DEFAULT | CULL_JGDI_HIDDEN)   /* per job monitoring facility */
   SGE_ULONG(CE_requestable, CULL_DEFAULT | CULL_SPOOL | CULL_CONFIGURE)
   SGE_ULONG(CE_tagged, CULL_DEFAULT | CULL_JGDI_HIDDEN)        /* used to tag resource request, which can be fulfilled */
   SGE_STRING(CE_urgency_weight, CULL_DEFAULT | CULL_SPOOL | CULL_CONFIGURE) /* static weighting factor */
LISTEND 

NAMEDEF(CEN)
   NAME("CE_name")
   NAME("CE_shortcut")
   NAME("CE_valtype")
   NAME("CE_stringval")
   NAME("CE_doubleval")
   NAME("CE_relop")
   NAME("CE_consumable")
   NAME("CE_default")
   NAME("CE_dominant")
   NAME("CE_pj_stringval")
   NAME("CE_pj_doubleval")
   NAME("CE_pj_dominant")
   NAME("CE_requestable")
   NAME("CE_tagged")
   NAME("CE_urgency_weight")
NAMEEND

/* *INDENT-ON* */ 

#define CES sizeof(CEN)/sizeof(char*)
#ifdef  __cplusplus
}
#endif
#endif                          /* __SGE_COMPLEXL_H */
