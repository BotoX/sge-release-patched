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
package com.sun.grid.cull;

import com.sun.grid.cull.template.Template;
import com.sun.grid.cull.template.TemplateFactory;
import java.io.File;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 *
 * @author  richard.hierlmeier@sun.com
 */
public class CullTemplateConverter extends AbstractCullConverter {
   

   private TemplateFactory fac;
   private File templateFile;
   private File outputFile;
   
   
   /** Creates a new instance of CullTemplateConverter */
   public CullTemplateConverter(File buildDir, String classpath, File outputFile, File template ) {      
      fac = new TemplateFactory(buildDir, classpath );
      this.outputFile = outputFile;
      this.templateFile = template;
   }

   public void convert(CullDefinition cullDef) throws java.io.IOException {
      
      
      
      if( !outputFile.exists() || outputFile.lastModified() < templateFile.lastModified() ) {         
      
         Template template = fac.createTemplate(templateFile);

         Printer p = new Printer(outputFile);

         JavaHelper jh = new JavaHelper(cullDef);

         Map params = new HashMap();
         params.put("cullDef", cullDef);
         params.put("javaHelper", jh );

         if(super.iterateObjects()) {
            Iterator iter = cullDef.getObjectNames().iterator();
            while( iter.hasNext() ) {
               String name = (String)iter.next();
               CullObject obj = cullDef.getCullObject(name);
               params.put("cullObj", obj);
               template.print(p, params);
            }
         } else {
            template.print(p,params);
         }
         p.flush();
         p.close();
      }
   }

}
