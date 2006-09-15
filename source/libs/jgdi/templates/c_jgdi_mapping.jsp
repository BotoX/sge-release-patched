<%
   com.sun.grid.cull.JavaHelper jh = (com.sun.grid.cull.JavaHelper)params.get("javaHelper");
   com.sun.grid.cull.CullDefinition cullDef = (com.sun.grid.cull.CullDefinition)params.get("cullDef");

   if( cullDef == null ) {
     throw new IllegalStateException("param cullDef not found");
   }
%>
#include <ctype.h>
#include <string.h>
#include "jni.h"
#include "basis_types.h"
#include "cull.h"
#include "commlib.h"
#include "sgermon.h"
#include "sge_all_listsL.h"
#include "sge_answer.h"
#include "sge_prog.h"
#include "sge_bootstrap.h"
#include "sge_gdi.h"
#include "sge_gdi_ctx.h"
#include "jgdi_common.h"



lDescr* get_descr(const char* name) {
<%
   java.util.Iterator iter = cullDef.getObjectNames().iterator();
   while( iter.hasNext() ) {
     String name = (String)iter.next();
     com.sun.grid.cull.CullObject cullObj = cullDef.getCullObject(name);
%>
   if( strcmp(name, "<%=name%>") == 0 ) return <%=name%>;
<%   
  } // end of while
%>
   return NULL;
}

lDescr* get_descr_for_classname(const char* classname ) {

<%
   iter = cullDef.getObjectNames().iterator();
   while( iter.hasNext() ) {
     String name = (String)iter.next();
     com.sun.grid.cull.CullObject cullObj = cullDef.getCullObject(name);
     String classname = jh.getFullClassName(cullObj);
     
     if(cullObj.getParentName() != null) {
        
        name = cullObj.getParentName();
     }
%>
   if( strcmp(classname, "<%=classname%>") == 0 ) return <%=name%>;
<%   
  } // end of while
%>
   return NULL;
}


<%
   iter = cullDef.getEnumNames().iterator();
   while(iter.hasNext()) {
      String name = (String)iter.next();
      com.sun.grid.cull.CullEnum aEnum = cullDef.getEnum(name);
%>

int get_<%=name%>(const char* a_<%=name%>_name) {

<%
     java.util.Iterator elemIter = aEnum.getElems().iterator();
     while( elemIter.hasNext() ) {
        String elemName = (String)elemIter.next();
%>
    if( strcmp("<%=elemName%>", a_<%=name%>_name) == 0 ) return <%=elemName%>;
<%     
    }
  }
%>

const char* get_classname_for_descr(const lDescr *descr) {

<%
   iter = cullDef.getObjectNames().iterator();
   while( iter.hasNext() ) {
     String name = (String)iter.next();
     com.sun.grid.cull.CullObject cullObj = cullDef.getCullObject(name);
     String classname = jh.getFullClassName(cullObj);
     classname = classname.replace('.', '/');
     
     if(cullObj.getParentName() != null) {
        
        name = cullObj.getParentName();
     }
     
%>
   if (descr == <%=name%>) {
      return "<%=classname%>";
   }
<%   
  } // end of while
%>
   return NULL;
}

int get_master_list_for_classname(const char* classname) {
<%
   iter = cullDef.getObjectNames().iterator();
   while( iter.hasNext() ) {
     String name = (String)iter.next();
     com.sun.grid.cull.CullObject cullObj = cullDef.getCullObject(name);
     String classname = jh.getFullClassName(cullObj);
%>
   if( strcmp(classname, "<%=classname%>") == 0 ) {
<%
     if(cullObj.isRootObject()) {
%>
     return <%=cullObj.getListName()%>;
<%   } else { %>     
     return -1;
<%   } %>      
   }
<%   
  } // end of while
%>
   return -1;
}
