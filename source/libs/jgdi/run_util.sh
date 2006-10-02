#!/bin/sh

GRIDENGINE_SOURCE=""

DIR=`dirname $0`
. $DIR/run_util.properties

if [ -r $DIR/run_util_private.properties ]; then 
   . $DIR/run_util_private.properties
fi   

log() {
   if [ $verbose -ne 0 ]; then
      echo "$*"
   fi
}

set_arch() {
   for i in ${GRIDENGINE_SOURCE}/dist $SGE_ROOT ; do
      if [ -x "$i/util/arch" ]; then
         ARCH=`$i/util/arch`
         export ARCH
         break
      fi   
   done   
}

setup_env() {
   
   
   mode=run
   verbose=0
   JVM_ARGS=""
   APPL_ARGS=""
   
   while [ $# -gt 0 ]; do
     case $1 in
       -v) verbose=1;;
       -d) mode=debug;;
       -j) mode=java_debug;;
       -noexit) 
            if [ "$mode" != "java_debug" ]; then
               JVM_ARGS="$JVM_ARGS -noexit"
            fi;;
       -D*) if [ "$mode" = "java_debug" ]; then
               JVM_ARGS="$JVM_ARGS $1"
            else
               JVM_ARGS="$JVM_ARGS -X$1"
            fi;;
        -X*)if [ "$mode" = "java_debug" ]; then
               JVM_ARGS="$JVM_ARGS $1"
            else
               JVM_ARGS="$JVM_ARGS -X$1"
            fi;;
       -cp) shift; JVM_ARGS="$JVM_ARGS -cp $1";;
       *)  APPL_ARGS="$APPL_ARGS $1"
     esac
     shift
   done
   
   export verbose
   
   set_arch

   JAVA_DATA_MODEL=""
   case $ARCH in
       solaris64) SRC_ARCH=SOLARIS64; 
                  JAVA_ARCH=sparcv9;
                  JAVA_DATA_MODEL=-d64;
                  DEBUGGER=dbx;;
     sol-sparc64) SRC_ARCH=SOLARIS64;
                  JAVA_ARCH=sparcv9;
                  JAVA_DATA_MODEL=-d64;
                  DEBUGGER=dbx;;
       sol-amd64) SRC_ARCH=SOLARISAMD64;
                  JAVA_ARCH=amd64;
                  JAVA_DATA_MODEL=-d64;
                  DEBUGGER=dbx;;
        lx26-x86) SRC_ARCH=LINUX86_26;
                  JAVA_ARCH=i386;
                  DEBUGGER=gdb;;
        lx24-x86) SRC_ARCH=LINUX86_24;
                  JAVA_ARCH=i386;
                  DEBUGGER=gdb;;
      lx24-amd64) SRC_ARCH=LINUXAMD64_24;
                  JAVA_ARCH=amd64;
                  DEBUGGER=gdb;;
           aix51) SRC_ARCH=AIX51;
                  JAVA_ARCH=aix;
                  DEBUGGER=dbx;;
      darwin-ppc) SRC_ARCH=DARWIN_PPC;
                  JAVA_ARCH="";
                  DEBUGGER=dbx;;
           tru64) SRC_ARCH=ALPHA5;
                  JAVA_ARCH=alpha;
                  DEBUGGER=dbx;;
        *) echo "ARCH $ARCH is not supported"; exit 1;;
   esac
   
   
   if [ "$mode" = "java_debug" ]; then
      JVM_ARGS="$JAVA_DATA_MODEL $JVM_ARGS -Xnoagent -Xdebug -Xrunjdwp:transport=dt_socket,server=y,suspend=y,address=9000"
   fi
   
   log "JVM_ARGS=$JVM_ARGS"
   log "APPL_ARGS=$APPL_ARGS"
   
   if [ "$LD_LIBRARY_PATH" = "" ]; then 
      LD_LIBRARY_PATH=${GRIDENGINE_SOURCE}/${SRC_ARCH}:${JAVA_HOME}/jre/lib/$JAVA_ARCH/server
   else 
      LD_LIBRARY_PATH="${GRIDENGINE_SOURCE}/${SRC_ARCH}:${JAVA_HOME}/jre/lib/$JAVA_ARCH/server:$LD_LIBRARY_PATH"
   fi
   export LD_LIBRARY_PATH
   log "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"   
}

run_native_debugger() {
   case $DEBUGGER in
      dbx)
        ldd ${GRIDENGINE_SOURCE}/${SRC_ARCH}/jgdi_test
        dbx -c "runargs $JVM_ARGS $APPL_ARGS" ${GRIDENGINE_SOURCE}/${SRC_ARCH}/jgdi_test;
        ;;
      gdb)
        echo "file ${GRIDENGINE_SOURCE}/${SRC_ARCH}/jgdi_test" >  /tmp/gdb_init.$$;
        echo "set args $JVM_ARGS $APPL_ARGS" >> /tmp/gdb_init.$$;
        #echo "set environment LD_LIBRARY_PATH $LD_LIBRARY_PATH" >> /tmp/gdb_init.$$
        ldd ${GRIDENGINE_SOURCE}/${SRC_ARCH}/jgdi_test
        gdb -x /tmp/gdb_init.$$;
        #rm /tmp/gdb_init.$$;
        ;;
      *) echo "Unsupported debugger $DEBUGGER"; exit 1;;
   esac
}

run_java() {
   ${JAVA_HOME}/bin/java $JVM_ARGS $APPL_ARGS
}

run_prog() {
   ${GRIDENGINE_SOURCE}/${SRC_ARCH}/jgdi_test $JVM_ARGS $APPL_ARGS
}

run() {
   setup_env $*
   case "$mode" in
          debug) run_native_debugger;;
     java_debug) run_java;;
              *) run_prog;;
   esac
}




