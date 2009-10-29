#!/bin/sh
##########################################################################
#___INFO__MARK_BEGIN__
##########################################################################
#
#  The Contents of this file are made available subject to the terms of
#  the Sun Industry Standards Source License Version 1.2
#
#  Sun Microsystems Inc., March, 2001
#
#
#  Sun Industry Standards Source License Version 1.2
#  =================================================
#  The contents of this file are subject to the Sun Industry Standards
#  Source License Version 1.2 (the "License"); You may not use this file
#  except in compliance with the License. You may obtain a copy of the
#  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
#
#  Software provided under this License is provided on an "AS IS" basis,
#  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
#  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
#  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
#  See the License for the specific provisions governing your rights and
#  obligations concerning the Software.
#
#  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
#
#  Copyright: 2001 by Sun Microsystems, Inc.
#
#  All Rights Reserved.
#
##########################################################################
#___INFO__MARK_END__

. $SGE_ROOT/hadoop/env.sh

if [ "$HADOOP_HOME" = "" ]; then
  echo Must specify \$HADOOP_HOME for jsv.sh
  exit 100
fi

if [ "$JAVA_HOME" = "" ]; then
  echo Must specify \$JAVA_HOME for jsv.sh
  exit 100
fi

HADOOP_CLASSPATH=$SGE_ROOT/util/resources/jsv/JSV.jar:$SGE_ROOT/hadoop/herd.jar
export HADOOP_CLASSPATH

"$HADOOP_HOME"/bin/hadoop --config $HADOOP_HOME/conf com.sun.grid.herd.HerdJsv