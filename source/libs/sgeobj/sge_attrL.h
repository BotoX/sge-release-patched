#ifndef __SGE_HOSTATTRL_H
#define __SGE_HOSTATTRL_H

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

enum {
   ASTR_href = ASTR_LOWERBOUND,
   ASTR_value                    
};

LISTDEF(ASTR_Type)
   JGDI_MAP_OBJ(ASTR_href, ASTR_value)
   SGE_HOST(ASTR_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_STRING(ASTR_value, CULL_DEFAULT | CULL_SUBLIST)
LISTEND 

NAMEDEF(ASTRN)
   NAME("ASTR_href")
   NAME("ASTR_value")
NAMEEND

#define ASTRS sizeof(ASTRN)/sizeof(char*)

enum {
   AULNG_href = AULNG_LOWERBOUND,
   AULNG_value                    
};

LISTDEF(AULNG_Type)
   JGDI_MAP_OBJ(AULNG_href, AULNG_value)
   SGE_HOST(AULNG_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_ULONG(AULNG_value, CULL_DEFAULT | CULL_SUBLIST)
LISTEND 

NAMEDEF(AULNGN)
   NAME("AULNG_href")
   NAME("AULNG_value")
NAMEEND

#define AULNGS sizeof(AULNGN)/sizeof(char*)

enum {
   ABOOL_href = ABOOL_LOWERBOUND,
   ABOOL_value                    
};

LISTDEF(ABOOL_Type)
   JGDI_MAP_OBJ(ABOOL_href, ABOOL_value)
   SGE_HOST(ABOOL_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_BOOL(ABOOL_value, CULL_DEFAULT | CULL_SUBLIST)
LISTEND 

NAMEDEF(ABOOLN)
   NAME("ABOOL_href")
   NAME("ABOOL_value")
NAMEEND

#define ABOOLS sizeof(ABOOLN)/sizeof(char*)

enum {
   ATIME_href = ATIME_LOWERBOUND,
   ATIME_value                    
};

LISTDEF(ATIME_Type)
   JGDI_MAP_OBJ(ATIME_href, ATIME_value)
   SGE_HOST(ATIME_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_STRING(ATIME_value, CULL_DEFAULT | CULL_SUBLIST)
LISTEND 

NAMEDEF(ATIMEN)
   NAME("ATIME_href")
   NAME("ATIME_value")
NAMEEND

#define ATIMES sizeof(ATIMEN)/sizeof(char*)

enum {
   AMEM_href = AMEM_LOWERBOUND,
   AMEM_value                    
};

LISTDEF(AMEM_Type)
   JGDI_MAP_OBJ(AMEM_href, AMEM_value)
   SGE_HOST(AMEM_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_STRING(AMEM_value, CULL_DEFAULT | CULL_SUBLIST)
LISTEND 

NAMEDEF(AMEMN)
   NAME("AMEM_href")
   NAME("AMEM_value")
NAMEEND

#define AMEMS sizeof(AMEMN)/sizeof(char*)

enum {
   AINTER_href = AINTER_LOWERBOUND,
   AINTER_value                    
};

LISTDEF(AINTER_Type)
   JGDI_MAP_OBJ(AINTER_href, AINTER_value)
   SGE_HOST(AINTER_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_STRING(AINTER_value, CULL_DEFAULT | CULL_SUBLIST)
LISTEND 

NAMEDEF(AINTERN)
   NAME("AINTER_href")
   NAME("AINTER_value")
NAMEEND

#define AINTERS sizeof(AINTERN)/sizeof(char*)

enum {
   ASTRING_href = ASTRING_LOWERBOUND,
   ASTRING_value                    
};

LISTDEF(ASTRING_Type)
   SGE_HOST(ASTRING_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_STRING(ASTRING_value, CULL_DEFAULT | CULL_SUBLIST)
LISTEND 

NAMEDEF(ASTRINGN)
   NAME("ASTRING_href")
   NAME("ASTRING_value")
NAMEEND

#define ASTRINGS sizeof(ASTRINGN)/sizeof(char*)

enum {
   ASTRLIST_href = ASTRLIST_LOWERBOUND,
   ASTRLIST_value
};

LISTDEF(ASTRLIST_Type)
   JGDI_MAP_OBJ(ASTRLIST_href, ASTRLIST_value)
   SGE_HOST(ASTRLIST_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_LIST(ASTRLIST_value, ST_Type, CULL_DEFAULT | CULL_SUBLIST)
LISTEND

NAMEDEF(ASTRLISTN)
   NAME("ASTRLIST_href")
   NAME("ASTRLIST_value")
NAMEEND

#define ASTRLISTS sizeof(ASTRLISTN)/sizeof(char*)

enum {
   AUSRLIST_href = AUSRLIST_LOWERBOUND,
   AUSRLIST_value
};

LISTDEF(AUSRLIST_Type)
   JGDI_MAP_OBJ(AUSRLIST_href, AUSRLIST_value)
   SGE_HOST(AUSRLIST_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_LIST(AUSRLIST_value, US_Type, CULL_DEFAULT | CULL_SUBLIST)
LISTEND

NAMEDEF(AUSRLISTN)
   NAME("AUSRLIST_href")
   NAME("AUSRLIST_value")
NAMEEND

#define AUSRLISTS sizeof(AUSRLISTN)/sizeof(char*)

enum {
   APRJLIST_href = APRJLIST_LOWERBOUND,
   APRJLIST_value
};

LISTDEF(APRJLIST_Type)
   JGDI_MAP_OBJ(APRJLIST_href, APRJLIST_value)
   SGE_HOST(APRJLIST_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_LIST(APRJLIST_value, PR_Type, CULL_DEFAULT | CULL_SUBLIST)
LISTEND

NAMEDEF(APRJLISTN)
   NAME("APRJLIST_href")
   NAME("APRJLIST_value")
NAMEEND

#define APRJLISTS sizeof(APRJLISTN)/sizeof(char*)

enum {
   ACELIST_href = ACELIST_LOWERBOUND,
   ACELIST_value
};

LISTDEF(ACELIST_Type)
   JGDI_MAP_OBJ(ACELIST_href, ACELIST_value)
   SGE_HOST(ACELIST_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_LIST(ACELIST_value, CE_Type, CULL_DEFAULT | CULL_SUBLIST)
LISTEND

NAMEDEF(ACELISTN)
   NAME("ACELIST_href")
   NAME("ACELIST_value")
NAMEEND

#define ACELISTS sizeof(ACELISTN)/sizeof(char*)

enum {
   ASOLIST_href = ASOLIST_LOWERBOUND,
   ASOLIST_value
};

LISTDEF(ASOLIST_Type)
   JGDI_MAP_OBJ(ASOLIST_href, ASOLIST_value)
   SGE_HOST(ASOLIST_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_LIST(ASOLIST_value, SO_Type, CULL_DEFAULT | CULL_SUBLIST)
LISTEND

NAMEDEF(ASOLISTN)
   NAME("ASOLIST_href")
   NAME("ASOLIST_value")
NAMEEND

#define ASOLISTS sizeof(ASOLISTN)/sizeof(char*)

enum {
   AQTLIST_href = AQTLIST_LOWERBOUND,
   AQTLIST_value
};

LISTDEF(AQTLIST_Type)
   JGDI_MAP_OBJ(AQTLIST_href, AQTLIST_value)
   SGE_HOST(AQTLIST_href, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_ULONG(AQTLIST_value, CULL_DEFAULT | CULL_SUBLIST)
LISTEND

NAMEDEF(AQTLISTN)
   NAME("AQTLIST_href")
   NAME("AQTLIST_value")
NAMEEND

#define AQTLISTS sizeof(AQTLISTN)/sizeof(char*)

/* *INDENT-ON* */  

#ifdef  __cplusplus
}
#endif
#endif   
