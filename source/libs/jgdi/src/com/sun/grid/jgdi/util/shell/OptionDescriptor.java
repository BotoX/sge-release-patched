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
package com.sun.grid.jgdi.util.shell;

import java.lang.reflect.Method;

/**
 * Describes command option parameters as cache entry
 */
public class OptionDescriptor {
   private AbstractCommand command;
   private int mandatoryArgCount;
   private int optionalArgCount;
   private Method method;
   
   /**
    * Constructor
    * @param mandatory - number of mandatory arguments
    * @param optional - number of optional arguments
    * @param command - current command
    * @param method - command method to execute
    * @throws java.lang.InstantiationException 
    * @throws java.lang.IllegalAccessException 
    */
   public OptionDescriptor(int mandatory, int optional, AbstractCommand command, Method method) throws InstantiationException,IllegalAccessException  {
      mandatoryArgCount = mandatory;
      optionalArgCount = optional;
      this.method = method;
      this.command = command;
   }
   
   /**
    * Getter method
    * @return value
    */
   public int getMandatoryArgCount() {
      return mandatoryArgCount;
   }
   
   /**
    * Getter method
    * @return value
    */
   public int getOptionalArgCount() {
      return optionalArgCount;
   }
   
   /**
    * Gets sum of mandatory and optional arguments
    * @return int sum of mandatory and optional arguments
    */
   public int getMaxArgCount() {
      return mandatoryArgCount + optionalArgCount;
   }
   
   /**
    * Getter method
    * @return value
    */
   public boolean isWithoutArgs() {
      return (mandatoryArgCount == 0 && optionalArgCount == 0) ? true : false;
   }
   
   /**
    * Getter method
    * @return value
    */
   public boolean isMultiple() {
       return (getMaxArgCount() > 1) ? true : false;
   }
   
   /**
    * Getter method
    * @return value
    */
   public AbstractCommand getCommand() {
      return command;
   }

   /**
    * Getter method
    * @return value
    */
   public Method getMethod() {
      return method;
   }
}