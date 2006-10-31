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
<%
  java.beans.BeanInfo beanInfo = (java.beans.BeanInfo)params.get("beanInfo");
  Class beanClass = beanInfo.getBeanDescriptor().getBeanClass();
  String classname = beanInfo.getBeanDescriptor().getBeanClass().getName();
  
  {
      int i = classname.lastIndexOf('.');
      if(i>0) {
          classname = classname.substring(i+1);
      }
  }  
  
  classname = classname.replace('$', '_');
  
  java.beans.PropertyDescriptor [] props = beanInfo.getPropertyDescriptors();
  
  com.sun.grid.javaconv.CWrapperHelper ch = new com.sun.grid.javaconv.CWrapperHelper(beanInfo.getBeanDescriptor().getBeanClass());
  
  java.util.Iterator iter = null;
%>
jclass <%=classname%>_find_class(JNIEnv *env, lList** alpp);
<%  
  // ---------------------------------------------------------------------------
  // ------------ CONSTRUCTORS -------------------------------------------------
  // ---------------------------------------------------------------------------
  
   iter = ch.getConstructorNames().iterator();
   while(iter.hasNext()) {
      String constructorName = (String)iter.next();
      java.lang.reflect.Constructor constructor = ch.getConstructor(constructorName);
%>
int <%=classname%>_<%=constructorName%>(JNIEnv *env, jobject*obj <%
   
      Class [] parameters = constructor.getParameterTypes();
      for(int i = 0; i < parameters.length; i++) {
         if( String.class.equals(parameters[i])) {
            /* For strings we want a const char* not a jstring */
          %>, const char* p<%=i%> <%
         } else {
         %>, <%=ch.getCType(parameters[i])%> p<%=i%> <%
         }
      }
%>, lList **alpp);
<%   } // end of while(iter.hasNext())
   
  // ---------------------------------------------------------------------------
  // ------------ Static Fields ------------------------------------------------
  // ---------------------------------------------------------------------------

 iter = ch.getStaticFieldNames().iterator();
 while(iter.hasNext()) {
    String fieldName = (String)iter.next();
    java.lang.reflect.Field field = ch.getStaticField(fieldName);
  
    
%>
int <%=classname%>_static_<%=fieldName%>(JNIEnv *env, <%=ch.getCType(field.getType())%> *res, lList **alpp);
<%    
 } // end of while
  
  // ---------------------------------------------------------------------------
  // ------------ Static METHODS -----------------------------------------------
  // ---------------------------------------------------------------------------
   
   iter = ch.getStaticMethodNames().iterator();
   while(iter.hasNext()) {
      String methodName = (String)iter.next();
      java.lang.reflect.Method method = ch.getStaticMethod(methodName);
      
%>
int <%=classname%>_static_<%=methodName%>(JNIEnv *env<%
   
      Class [] parameters = method.getParameterTypes();      
      for(int i = 0; i < parameters.length; i++) {
         if( String.class.equals(parameters[i])) {
          %>, const char* p<%=i%> <%
         } else {
         %>, <%=ch.getCType(parameters[i])%> p<%=i%> <%
         }
      }

      if (!Void.TYPE.equals(method.getReturnType())) {
         %>, <%=ch.getCType(method.getReturnType())%><%
           %>* result<%
         if(method.getReturnType().isArray()) {
           %>, int* len<%  
         }
      }
%>, lList **alpp);
<%   } // end of while(iter.hasNext())
   
  // ---------------------------------------------------------------------------
  // ------------ METHODS ------------------------------------------------------
  // ---------------------------------------------------------------------------
   
   iter = ch.getMethodNames().iterator();
   while(iter.hasNext()) {
      String methodName = (String)iter.next();
      java.lang.reflect.Method method = ch.getMethod(methodName);
      
%>
int <%=classname%>_<%=methodName%>(JNIEnv *env, <%=ch.getCType(beanClass)%> obj <%
   
      Class [] parameters = method.getParameterTypes();
      for(int i = 0; i < parameters.length; i++) {
         if( String.class.equals(parameters[i])) {
          %>, const char* p<%=i%> <%
         } else {
         %>, <%=ch.getCType(parameters[i])%> p<%=i%> <%
         }
      }

      if (!Void.TYPE.equals(method.getReturnType())) {
         %>, <%=ch.getCType(method.getReturnType())%><%
           %>* result<%
         if(method.getReturnType().isArray()) {
           %>, int* len<%  
         }
      }
%>, lList **alpp);
<%   } // end of while(iter.hasNext())
%>