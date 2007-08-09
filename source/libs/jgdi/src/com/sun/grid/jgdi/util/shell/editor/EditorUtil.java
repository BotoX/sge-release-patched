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

package com.sun.grid.jgdi.util.shell.editor;

import com.sun.grid.jgdi.JGDI;
import com.sun.grid.jgdi.configuration.AbstractUser;
import com.sun.grid.jgdi.configuration.ClusterQueue;
import com.sun.grid.jgdi.configuration.ClusterQueueImpl;
import com.sun.grid.jgdi.configuration.ComplexEntry;
import com.sun.grid.jgdi.configuration.ComplexEntryImpl;
import com.sun.grid.jgdi.configuration.Configuration;
import com.sun.grid.jgdi.configuration.GEObject;
import com.sun.grid.jgdi.configuration.Project;
import com.sun.grid.jgdi.configuration.SchedConf;
import com.sun.grid.jgdi.configuration.SchedConfImpl;
import com.sun.grid.jgdi.configuration.User;
import com.sun.grid.jgdi.configuration.UserSet;
import com.sun.grid.jgdi.configuration.UserSetImpl;
import com.sun.grid.jgdi.configuration.reflect.DefaultListPropertyDescriptor;
import com.sun.grid.jgdi.configuration.reflect.DefaultMapListPropertyDescriptor;
import com.sun.grid.jgdi.configuration.reflect.DefaultMapPropertyDescriptor;
import com.sun.grid.jgdi.configuration.reflect.PropertyDescriptor;
import com.sun.grid.jgdi.configuration.reflect.SimplePropertyDescriptor;
import java.lang.IllegalArgumentException;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.text.FieldPosition;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 *
 */
public class EditorUtil {
   public static final String VALUE_NONE = "NONE";
   public static final String VALUE_UNDEFINED = "UNDEFINED";
   public static final String VALUE_INFINITY = "INFINITY";
   public static final String VALUE_DEFAULT = "@/";
   
   public static final String QTYPES[] = { "BATCH", "INTERACTIVE" };
   public static final String QSMTYPES[] = { "load", "seqno" };
   private static final String userset_types[] = {
      "ACL",   /* US_ACL   */
      "DEPT",  /* US_DEPT  */
     };
   
   public static final int PROPERTIES_ALL = 0;
   public static final int PROPERTIES_READ_ONLY = 1;
   public static final int PROPERTIES_CONFIGURABLE = 2;
   
   
   public static String unifyAttrWithClientNames(String objectName, String name) {
      //Convert JGDI name to equivalent in the clients
      //CALENDAR
      if (objectName.equals("Calendar")) {
         if (name.equals("name")) {
            return "calendar_name";
         }
         if (name.equals("year")) {
            return "year_calendar";
         }
         if (name.equals("week")) {
            return "week_calendar";
         }        
      }
      //CHECKPOINT
      if (objectName.equals("Checkpoint")) {
         if (name.equals("name")) {
            return "ckpt_name";
         }
         if (name.equals("rest_command")) {
            return "restart_command";
         }
      }
      //CONFIGURATION
      if (objectName.equals("Configuration")) {
         if (name.equals("name")) {
            return "hostname";
         }
      }
      //HOSTGROUP
      if (objectName.equals("Hostgroup")) {
         if (name.equals("name")) {
            return "group_name";
         }
         if (name.equals("host")) {
            return "hostlist";
         }
      }
      //PARALLEL ENVIRONMENT
      if (objectName.equals("ParallelEnvironment")) {
         if (name.equals("name")) {
            return "pe_name";
         }
         if (name.equals("user")) {
            return "user_lists";
         }
         if (name.equals("xuser")) {
            return "xuser_lists";
         }
      }
      //QUEUE
      if (objectName.equals("ClusterQueue")) {
         if (name.equals("name")) {
            return "qname";
         }
         if (name.equals("pe")) {
            return "pe_list";
         }
         if (name.equals("job_slots")) {
            return "slots";
         }
         if (name.equals("ckpt")) {
            return "ckpt_list";
         }
         if (name.equals("owner")) {
            return "owner_list";
         }
         if (name.equals("acl")) {
            return "user_lists";
         }
         if (name.equals("xacl")) {
            return "xuser_lists";
         }
         if (name.equals("subordinate")) {
            return "subordinate_list";
         }
         if (name.equals("consumable_config")) {
            return "complex_values";
         }
      }
      //SCHEDCONF
      if (objectName.equals("SchedConf")) {
         if (name.equals("usage_weight")) {
            return "usage_weight_list";
         }
      }
      return name;
   }
   
   static String unifyAttrWithClientNames(GEObject obj, String name) {
      String objectName = obj.getClass().getName();
      String prefix = "com.sun.grid.jgdi.configuration.";
      String suffix = "Impl";
      if (objectName.startsWith(prefix)) {
         objectName = objectName.substring(prefix.length(), objectName.length());
      }
      if (objectName.endsWith(suffix)) {
         objectName = objectName.substring(0, objectName.length()-suffix.length());
      }
      return unifyAttrWithClientNames(objectName, name);
   }
   
   static String unifyClientNamesWithAttr(String objectName, String name) {
      //Convert client name to equivalent name in JGDI
      //CALENDAR
      if (objectName.equals("Calendar")) {
         if (name.equals("calendar_name")) {
            return "name";
         }
         if (name.equals("year_calendar")) {
            return "year";
         }
         if (name.equals("week_calendar")) {
            return "week";
         }        
      }
      //CHECKPOINT
      if (objectName.equals("Checkpoint")) {
         if (name.equals("ckpt_name")) {
            return "name";
         }
         if (name.equals("restart_command")) {
            return "rest_command";
         }
      }
      //CONFIGURATION
      //TODO LP: Check if client recognizes name or hname in a configuration file - it does not
      if (objectName.equals("Configuration")) {
         if (name.equals("hostname")) {
            return "name";
         }
      }
      //HOSTGROUP
      if (objectName.equals("Hostgroup")) {
         if (name.equals("group_name")) {
            return "name";
         }
         if (name.equals("hostlist")) {
            return "host";
         }
      }
      //PARALLEL ENVIRONMENT
      if (objectName.equals("ParallelEnvironment")) {
         if (name.equals("pe_name")) {
            return "name";
         }
         if (name.equals("user_lists")) {
            return "user";
         }
         if (name.equals("xuser_lists")) {
            return "xuser";
         }
      }
      //QUEUE
      if (objectName.equals("ClusterQueue")) {
         if (name.equals("qname")) {
            return "name";
         }
         if (name.equals("pe_list")) {
            return "pe";
         }
         if (name.equals("slots")) {
            return "job_slots";
         }
         if (name.equals("ckpt_list")) {
            return "ckpt";
         }
         if (name.equals("owner_list")) {
            return "owner";
         }
         if (name.equals("user_lists")) {
            return "acl";
         }
         if (name.equals("xuser_lists")) {
            return "xacl";
         }
         if (name.equals("subordinate_list")) {
            return "subordinate";
         }
         if (name.equals("complex_values")) {
            return "consumable_config";
         }
      }
      //SCHEDCONF
      if (objectName.equals("SchedConf")) {
         if (name.equals("usage_weight_list")) {
            return "usage_weight";
         }
      }
      return name;
   }
   
   static String unifyClientNamesWithAttr(GEObject obj, String name) {
      String objectName = obj.getClass().getName();
      String prefix = "com.sun.grid.jgdi.configuration.";
      String suffix = "Impl";
      if (objectName.startsWith(prefix)) {
         objectName = objectName.substring(prefix.length(), objectName.length());
      }
      if (objectName.endsWith(suffix)) {
         objectName = objectName.substring(0, objectName.length()-suffix.length());
      }
      return unifyClientNamesWithAttr(objectName, name);
   }
   
   static String c2javaName(GEObject obj, String cName) {
      StringBuffer sb = new StringBuffer();
      String[] parts = cName.toLowerCase().split("_");
      for (int i=0; i<parts.length; i++) {
         sb.append(Character.toUpperCase(parts[i].charAt(0)));
         sb.append(parts[i].substring(1));
      }
      char c = Character.toLowerCase(sb.charAt(0));
      sb.deleteCharAt(0);
      sb.insert(0,c);
      //unify with inconsistencies in clients
      String name = unifyClientNamesWithAttr(obj, sb.toString());
      return name;
   }
    
   static String java2cName(GEObject obj, String javaName) {
      StringBuffer sb = new StringBuffer(javaName);
      char c;
      for (int i = 0; i<sb.length(); i++) {
         c = sb.charAt(i);
         if (Character.isUpperCase(c)) {
            sb.deleteCharAt(i);
            sb.insert(i,"_"+Character.toLowerCase(c));
            i++;
         }
      }
      //unify with inconsistencies in clients
      String name = unifyAttrWithClientNames(obj, sb.toString());
      return name;
   }
   
   static boolean doNotDisplayAttr(GEObject obj, PropertyDescriptor pd, int propScope) {
      switch (propScope) {
         case PROPERTIES_ALL:
            return doNotDisplayAttrAll(obj, pd);
         case PROPERTIES_CONFIGURABLE:
            return doNotDisplayConfigurableAttr(obj, pd);
         case PROPERTIES_READ_ONLY:
            return false;
         default:
            throw new IllegalArgumentException("Invalid property scope specifier!");
      }
   }
   
   /** 
    * Filters GEObject attributes so only those that are displayed by client 
    * are also displayed by JGDIShell (Used in qconf -s*) 
    */ 
   static boolean doNotDisplayAttrAll(GEObject obj, PropertyDescriptor pd) {
      String name = java2cName(obj, pd.getPropertyName());
      //ABSTRACT USER
      if (obj instanceof AbstractUser) {
         if (name.equals("usage") || name.equals("usage_time_stamp") ||
             name.equals("long_term_usage") || name.equals("debited_job_usage") ||
             name.equals("version") || name.equals("project")) {
            return true;
         }
      }
      //USER
      if (obj instanceof User) {
         if (name.equals("acl") || name.equals("xacl")) {
            return true;
         }
      }
      //USERSET
      if (obj instanceof UserSet) {
         if (name.equals("job_cnt")) {
            return true;
         }
      }
      //PROJECT
      if (obj instanceof Project) {
         if (name.equals("delete_time") || name.equals("default_project")) {
            return true;
         }
      }
      //QUEUE
      if (obj instanceof ClusterQueue) {
         if (name.equals("qinstances") || name.equals("tag")) {
            return true;
         }
      }
      //CONFIGURATION
      if (obj instanceof Configuration) {
         if (name.equals("version")) {
            return true;
         }
      }
      //SCHEDCONF
      if (obj instanceof SchedConf) {
         if (name.equals("weight_tickets_override")) {
            return true;
         }
      }
      return false;
   }
   
   /** Filters GEObject attributes so only those that are displayed by client 
    * are also displayed by JGDIShell (Used in qconf -a*,-m*) */ 
   static boolean doNotDisplayConfigurableAttr(GEObject obj, PropertyDescriptor pd) {
      String name = java2cName(obj, pd.getPropertyName());
      //ABSTRACT USER
      if (obj instanceof AbstractUser) {
         if (name.equals("project")) {
            return true;
         }
      }
      //USER
      if (obj instanceof User) {
         if (name.equals("acl") || name.equals("xacl")) {
            return true;
         }
      }
      //PROJECT
      if (obj instanceof Project) {
         if (name.equals("delete_time") || name.equals("default_project")) {
            return true;
         }
      }
      //QUEUE
      if (obj instanceof ClusterQueue) {
         if (name.equals("tag")) {
            return true;
         }
      }
      //SCHECONF
      if (obj instanceof SchedConf) {
         if (name.equals("weight_tickets_override")) {
            return true;
         }
      }
      return false;
   }
   
   private static Double parseDouble(String str) {
      Double d;
      if (str.compareToIgnoreCase(VALUE_NONE)==0) {
         d = null;
      } else if (str.compareToIgnoreCase(VALUE_INFINITY)==0) {
         d = new Double(Double.MAX_VALUE);
      } else {
         d = new Double(str);
      }
      return d;
   }
   
   private static Integer parseInteger(String key, String str) {
      if (str.compareToIgnoreCase(VALUE_NONE)==0) {
         return null;
      } else if (str.compareToIgnoreCase(VALUE_UNDEFINED)==0) {
         return null;
      } else if (str.compareToIgnoreCase(VALUE_INFINITY)==0) {
         return new Integer(Integer.MAX_VALUE);
      //CLUSTERQUEUE - QTYPE
      } else if (key.equalsIgnoreCase("qtype")) {
         return getQtypeValue(str);
      //USERSET - TYPE
      /*} else if (key.equalsIgnoreCase("type")) {
         return getUserSetTypeValue(str);*/
      //SCHEDCONF - queue_sort_method
      } else if (key.equalsIgnoreCase("queueSortMethod")) {
         return getQSMtypeValue(str);
      } else {
         return new Integer(str);
      }
   }
   
   private static Long parseLong(String str) {
      //TIME
      if (str.matches("[0-9][0-9]*:[0-9][0-9]*:[0-9][0-9]*")) {
         return parseTime(str);
      }
      return new Long(Long.parseLong(str));
   }
   
   static Integer getQtypeValue(String str) {
      return getTypeMappedValue(QTYPES, str);
   }
   
   private static String getQtypeString(int val) {
      return getTypeMappedString(QTYPES, val);
   }
   
   static Integer getUserSetTypeValue(String str) {
      return getTypeMappedValue(userset_types, str);
   }
   
   private static String getUserSetTypeString(int val) {
      return getTypeMappedString(userset_types, val);
   }
   
   private static Integer getTypeMappedValue(String[] types, String str) {
      str = str.toUpperCase();
      int val = 0;
      int bitmask = 1;
      int pos=-1;
      for (int i = 0; i < types.length ; i++) {
         if ((pos = str.indexOf(types[i])) !=-1) {
            val |= bitmask;
            str = str.substring(0, pos) + str.substring(pos+types[i].length());
         }
         bitmask <<=1;
      }
      //Check there is nothing else in the line
      if (str.trim().length()>0) {
         throw new IllegalArgumentException("Unknown type value \""+str.trim()+"\"");
      }
      return new Integer(val);
   }
   
   private static String getTypeMappedString(String[] types, int val) {
      String str = "";
      int bitmask = 1;
      int pos=-1;
      for (int i = 0; i < types.length ; i++) {
         if ((val & bitmask) == bitmask) {
            str += types[i] + " ";
         }
         bitmask <<=1;
      }
      return str.trim();
   }
   
   static Integer getQSMtypeValue(String str) {
      str = str.toUpperCase();
      int val = 0;
      int bitmask = 1;
      int pos=-1;
      for (int i = 0; i < QSMTYPES.length ; i++) {
         if (str.equalsIgnoreCase(QSMTYPES[i])) {
            return new Integer(i);
         }
      }
      throw new IllegalArgumentException("Unknown qsm type value \""+str.trim()+"\"");
   }
   
   private static String getQSMtypeString(int val) {
      String str = "";
      int bitmask = 1;
      int pos=-1;
      if (val < 0 || val >= QSMTYPES.length) {
         throw new IllegalArgumentException("Unknown qsm type value \""+str.trim()+"\"");
      }
      return QSMTYPES[val];
   }
   
   private static Long parseTime(String time) {
      String[] elems = time.split(":");
      return new Long(Long.parseLong(elems[0])*3600000+Long.parseLong(elems[1])*60000+Long.parseLong(elems[2])*1000);
   }
   
   private static Boolean parseBoolean(String str) {
      if (str.compareToIgnoreCase("TRUE")==0 || str.compareTo("1")==0) {
         return Boolean.TRUE;
      } else if (str.compareToIgnoreCase("FALSE")==0 || str.compareTo("0")==0) {
         return Boolean.FALSE;
      }
      return null;
   }
   
   static Object translateObjectToStringValue(String key, Object o) {
      if (o == null) {
         return VALUE_NONE;
      }
      if (o instanceof List && ((List)o).size()==0) {
         return VALUE_NONE;
      }
      if (o instanceof Boolean) {
         return String.valueOf(o).toUpperCase();
      }
      if (o instanceof ComplexEntryImpl) {
         ComplexEntry ce = ((ComplexEntryImpl)o);
         return ce.getName()+"="+ce.getStringval();
      }
      if (o instanceof UserSetImpl) {
         UserSet us = ((UserSetImpl)o);
         return us.getName();
      }
      if (o instanceof String) {
         String str = (String) o;
         if (str.trim().length()==0) {
            return null;
         }
      }
      //TODO LP: Will need object type as well
      //CLUSTERQUEUE - QTYPE 
      if (key.equalsIgnoreCase("qtype")) {
         if (o instanceof Integer) {
            return getQtypeString(((Integer)o).intValue());
         }
      //USERSET - TYPE 
      } else if (key.equalsIgnoreCase("type")) {
         if (o instanceof Integer) {
            return getUserSetTypeString(((Integer)o).intValue());
         }
      //SCHEDCONF - queue_sort_method
      } else if (key.equals("queueSortMethod")) {
         if (o instanceof Integer) {
            return getQSMtypeString(((Integer)o).intValue());
         }
      }
      return o;
   }
   
   static Object translateStringValueToObject(Object o) {
      if (o instanceof String) {
         String str = (String) o;
         if (str.equalsIgnoreCase(VALUE_NONE) || str.trim().length()==0) {
            return null;
         }
      }
      return o;
   }
   
   static Object getParsedValueAsObject(JGDI jgdi, String key, String type, String value) {
      if (type.equals("int")) {
         return parseInteger(key, value);
      } else if (type.equals("long")) {
         return parseLong(value);
      } else if (type.equals("double")) {
         return parseDouble(value);
      } else if (type.equals("boolean")) {
         return parseBoolean(value);
      } else if (type.equals("java.lang.String")) {
         return translateStringValueToObject(value);
      } else if (type.equals("com.sun.grid.jgdi.configuration.ConfigurationElement")) {
         //jgdi.getCo
      } else if (type.startsWith("com.sun.grid.jgdi.configuration.")) {
         if (translateStringValueToObject(value) == null) {
            return null;
         }
         Object newObj = null;
         try {
            //Used in tests
            if (jgdi == null) {
               Class cls = Class.forName(type+"Impl");
               Constructor c = cls.getConstructor(new Class[] {String.class});
               newObj = c.newInstance(new Object[] {value});
            } else {
               String [] elems = type.split("\\.");
               String name = elems[elems.length-1];
               String tmp = null;
               //LP temp fix for User and Project
               if (name.equals("AbstractUser")) {
                  if (key.equals("projects") || key.equals("xprojects")) {
                     name = "Project";
                  } else if (key.equals("acl") || key.equals("xacl")) {
                     name = "User";
                  } else {
                     throw new UnsupportedOperationException("Cannot decide target class for AbstractUser with key=\""+key+"\" value=\""+value+"\"");
                  }
               } else if (name.equals("ComplexEntry")) {
                  elems = value.split("=");
                  value = elems[0];
                  //tmp = elems[1]; - is set in GEObjectEditor.updateOneMapListEntry
               }
               Class cls = jgdi.getClass();
               //TODO LP: Check if anywhere used JGDI.getJob(int jid) and handle it
               Method m = cls.getDeclaredMethod("get"+name, new Class[] {String.class});
               newObj = m.invoke(jgdi, new Object[] {value});
               if (newObj == null) {
                  throw new IllegalArgumentException(name + " \"" + value + "\"" + " does not exist");
               }
               if (name.equals("ComplexEntry")) {
                  ((ComplexEntry)newObj).setStringval(tmp);
               }
            }
         } catch (ClassNotFoundException ex) {
            ex.printStackTrace();
         } catch (InstantiationException ex) {
            ex.printStackTrace();
         } catch (IllegalAccessException ex) {
            ex.printStackTrace();
         } catch (SecurityException ex) {
            ex.printStackTrace();
         } catch (NoSuchMethodException ex) {
            ex.printStackTrace();
         } catch (InvocationTargetException ex) {
            ex.printStackTrace();
         }
         return newObj;
      } else {
         throw new IllegalArgumentException("Unknown data type=\""+type+"\"");
      }
      return null;
   }
   
   static Object getPropertyValue(GEObject obj, PropertyDescriptor pd) {
      if (pd instanceof SimplePropertyDescriptor) {
         Object value = ((SimplePropertyDescriptor)pd).getValue(obj);
         //return EditorUtil.translateObjectToStringValue(pd.getPropertyName(), ((SimplePropertyDescriptor)pd).getValue(obj););
         //SchedConf special formatting for double values 
         if (obj instanceof SchedConf && value instanceof Double) {
            DecimalFormat df = new DecimalFormat("#0.000000", new DecimalFormatSymbols(Locale.US));
            return df.format(value);
         }
         return value;
      } else if (pd instanceof DefaultListPropertyDescriptor) {
         return convertList2String(obj, (DefaultListPropertyDescriptor)pd);
      } else if (pd instanceof DefaultMapPropertyDescriptor) {
         return convertMap2String(obj, (DefaultMapPropertyDescriptor)pd);
      } else if (pd instanceof DefaultMapListPropertyDescriptor) {
         return convertMapList2String(obj, (DefaultMapListPropertyDescriptor)pd);
      }
      System.err.println("WARNING: "+pd.getPropertyName()+ " type="+pd.getPropertyType()+" unknown="+pd.getClass().getName()+" returning NULL.");
      return null;
   }
   
   private static String convertList2String(GEObject obj, DefaultListPropertyDescriptor pd) {
      StringBuffer sb = new StringBuffer();
      for (int i=0; i < pd.getCount(obj); i++) {
         sb.append(" " + EditorUtil.translateObjectToStringValue(pd.getPropertyName(), pd.get(obj, i)));
      }
      if (sb.length() == 0) {
         return EditorUtil.VALUE_NONE;
      }
      sb.deleteCharAt(0);
      return sb.toString();//+" List";
   }
   
   private static String convertMap2String(GEObject obj, DefaultMapPropertyDescriptor pd) {
      StringBuffer sb = new StringBuffer();
      Object val;
      
      //SchedConf special case with hardcoded sorting cpu,mem,io
      if (obj instanceof SchedConf) {
         Set keys = pd.getKeys(obj);
         if (keys.size() != 3) {
            throw new IllegalArgumentException("Author only expected 3 keys [cpu, mem, io], but you got "+keys.size());
         }
         DecimalFormat df = new DecimalFormat("#0.000000", new DecimalFormatSymbols(Locale.US));
         return "cpu="+df.format(pd.get(obj,"cpu")) +
                ",mem="+df.format(pd.get(obj,"mem")) +
                ",io="+df.format(pd.get(obj,"io"));
      }
      
      for (Iterator iter = pd.getKeys(obj).iterator(); iter.hasNext(); ) {
         String key = (String) iter.next();
         val = EditorUtil.translateObjectToStringValue(pd.getPropertyName(), pd.get(obj, key));
         if (key.equals(EditorUtil.VALUE_DEFAULT)) {
            sb.insert(0,"," + val);
         } else {
            sb.append(",[" + key + "=" + val+"]");
         }
      }
      if (sb.length() == 0) {
         return EditorUtil.VALUE_NONE;
      }
      sb.deleteCharAt(0);
      return sb.toString();//+" Map";
   }
   
   private static String convertMapList2String(GEObject obj, DefaultMapListPropertyDescriptor pd) {
      StringBuffer sb = new StringBuffer();
      Object val;
      for (Iterator iter = pd.getKeys(obj).iterator(); iter.hasNext(); ) {
         String key = (String) iter.next();
         StringBuffer temp = new StringBuffer();
         for (int i=0; i < pd.getCount(obj,key); i++) {
            val = pd.get(obj, key, i);
            //ClusterQueue - ComplexEntryImpl
            if (val instanceof ComplexEntryImpl) {
               val = ((ComplexEntryImpl)val).getName()+"="+((ComplexEntryImpl)val).getStringval();
            } else {
               val = EditorUtil.translateObjectToStringValue(pd.getPropertyName(), val);
            }
            //TODO LP: Decide the default behaviour
            //temp.append(val + ",");
            temp.append(val + " ");
         }
         if (temp.length() == 0) {
            continue;
         }
         temp.deleteCharAt(temp.length()-1);
         if (key.equals(EditorUtil.VALUE_DEFAULT)) {
            //Due to missing method in Java 1.4, we need to convert temp to String
            sb.insert(0,temp.toString());
         } else {
            sb.append(",[" + key + "=" + temp + "]");
         }
      }
      if (sb.length() == 0) {
         return EditorUtil.VALUE_NONE;
      }
      return sb.toString();//+" MapList";
   }
}
