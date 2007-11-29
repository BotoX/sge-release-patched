#!/bin/sh
#
# SGE configuration script (Installation/Uninstallation/Upgrade/Downgrade)
# Scriptname: inst_qmaster.sh
# Module: qmaster installation functions
#
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

#set -x


#-------------------------------------------------------------------------
# GetCellRoot
#
GetCellRoot()
{
   if [ $AUTO = true ]; then
      SGE_CELL_ROOT=$SGE_CELL_ROOT
       $INFOTEXT -log "Using >%s< as SGE_CELL_ROOT." "$SGE_CELL_ROOT"
   else
      $CLEAR
      $INFOTEXT -u "\nGrid Engine cell root"
      $INFOTEXT -n "Enter the cell root <<<"
      INP=`Enter `
      eval SGE_CELL_ROOT=$INP
      $INFOTEXT -wait -auto $AUTO -n "\nUsing cell root >%s<. Hit <RETURN> to continue >> " $SGE_CELL_ROOT
      $CLEAR
   fi
}

#-------------------------------------------------------------------------
# GetCell
#
GetCell()
{
   is_done="false"

   if [ $AUTO = true ]; then
    SGE_CELL=$CELL_NAME
    SGE_CELL_VAL=$CELL_NAME
    $INFOTEXT -log "Using >%s< as CELL_NAME." "$CELL_NAME"

    if [ -f $SGE_ROOT/$SGE_CELL/common/bootstrap -a "$QMASTER" = "install" ]; then
       $INFOTEXT -log "The cell name you have used and the bootstrap already exists!"
       $INFOTEXT -log "It seems that you have already a installed system."
       $INFOTEXT -log "A installation may cause, that data can be lost!"
       $INFOTEXT -log "Please, check this directory and remove it, or use any other cell name"
       $INFOTEXT -log "Exiting installation now!"
       MoveLog
       exit 1
    fi 

   else
   while [ $is_done = "false" ]; do 
      $CLEAR
      $INFOTEXT -u "\nGrid Engine cells"
      if [ "$SGE_CELL" = "" ]; then
         $INFOTEXT -n "\nGrid Engine supports multiple cells.\n\n" \
                      "If you are not planning to run multiple Grid Engine clusters or if you don't\n" \
                      "know yet what is a Grid Engine cell it is safe to keep the default cell name\n\n" \
                      "   default\n\n" \
                      "If you want to install multiple cells you can enter a cell name now.\n\n" \
                      "The environment variable\n\n" \
                      "   \$SGE_CELL=<your_cell_name>\n\n" \
                      "will be set for all further Grid Engine commands.\n\n" \
                      "Enter cell name [default] >> "
         INP=`Enter default`
      else
         $INFOTEXT -n "\nGrid Engine supports multiple cells.\n\n" \
                      "If you are not planning to run multiple Grid Engine clusters or if you don't\n" \
                      "know yet what is a Grid Engine cell it is safe to keep the default cell name\n\n" \
                      "   default\n\n" \
                      "If you want to install multiple cells you can enter a cell name now.\n\n" \
                      "The environment variable\n\n" \
                      "   \$SGE_CELL=<your_cell_name>\n\n" \
                      "will be set for all further Grid Engine commands.\n\n" \
                      "Enter cell name [%s] >> " $SGE_CELL
         INP=`Enter $SGE_CELL`
      fi
      eval SGE_CELL=$INP
      SGE_CELL_VAL=`eval echo $SGE_CELL`
      if [ "$QMASTER" = "install" ]; then
         if [ -d $SGE_ROOT/$SGE_CELL/common ]; then
            $CLEAR
            $INFOTEXT "\nThe \"common\" directory in cell >%s< already exists!" $SGE_CELL_VAL
            $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "Do you want to select another cell name? (y/n) [y] >> "
            if [ $? = 0 ]; then
               is_done="false"
            else
               $INFOTEXT -n "You can overwrite or delete this directory. If you choose overwrite\n" \
                            "(YES option) only the \"bootstrap\" file will be deleted).\n" \
                            "Delete (NO option) - will delete the whole directory!\n"
               $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "Do you want to overwrite [y] or delete [n] the directory? (y/n) [y] >> "
               if [ $? = 0 ]; then
                  $INFOTEXT "Deleting bootstrap file!"
                  ExecuteAsAdmin rm -f $SGE_ROOT/$SGE_CELL_VAL/common/bootstrap
                  is_done="true"
               else
                  $INFOTEXT "Deleting directory \"%s\" now!" $SGE_ROOT/$SGE_CELL_VAL
                  ExecuteAsAdmin rm -rf $SGE_ROOT/$SGE_CELL_VAL
                  is_done="true"
               fi
            fi
         else
            is_done="true"
         fi
      else
         is_done="true"
      fi
   done

   $INFOTEXT -wait -auto $AUTO -n "\nUsing cell >%s<. \nHit <RETURN> to continue >> " $SGE_CELL_VAL
   $CLEAR
   fi
   export SGE_CELL

  HOST=`$SGE_UTILBIN/gethostname -aname`
  if [ "$HOST" = "" ]; then
     $INFOTEXT -e "can't get hostname of this machine. Installation failed."
     exit 1
  fi
}


#-------------------------------------------------------------------------
# GetQmasterSpoolDir()
#
GetQmasterSpoolDir()
{
   if [ $AUTO = true ]; then
      QMDIR=$QMASTER_SPOOL_DIR
      $INFOTEXT -log "Using >%s< as QMASTER_SPOOL_DIR." "$QMDIR"
   else
   euid=$1

   done=false
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nGrid Engine qmaster spool directory"
      $INFOTEXT "\nThe qmaster spool directory is the place where the qmaster daemon stores\n" \
                "the configuration and the state of the queuing system.\n\n"

      if [ $euid = 0 ]; then
         if [ $ADMINUSER = default ]; then
            $INFOTEXT "User >root< on this host must have read/write accessto the qmaster\n" \
                      "spool directory.\n"
         else
            $INFOTEXT "The admin user >%s< must have read/write access\n" \
                      "to the qmaster spool directory.\n" $ADMINUSER
         fi
      else
         $INFOTEXT "Your account on this host must have read/write access\n" \
                   "to the qmaster spool directory.\n"
      fi

      $INFOTEXT -n "If you will install shadow master hosts or if you want to be able to start\n" \
                   "the qmaster daemon on other hosts (see the corresponding section in the\n" \
                   "Grid Engine Installation and Administration Manual for details) the account\n" \
                   "on the shadow master hosts also needs read/write access to this directory.\n\n" \
                   "The following directory\n\n [%s]\n\n will be used as qmaster spool directory by default!\n" \
                   $SGE_ROOT_VAL/$SGE_CELL_VAL/spool/qmaster
                   QMDIR=$SGE_ROOT_VAL/$SGE_CELL_VAL/spool/qmaster

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to select another qmaster spool directory (y/n) [n] >> "

      if [ $? = 1 ]; then
         done=true
      else
         $INFOTEXT -n "Please enter a qmaster spool directory now! >>" 
         QMDIR=`Enter $SGE_ROOT_VAL/$SGE_CELL_VAL/spool/qmaster`
         done=true
      fi
   done
   fi
   export QMDIR
}



#--------------------------------------------------------------------------
# SetPermissions
#    - set permission for regular files to 644
#    - set permission for executables and directories to 755
#
SetPermissions()
{
   $CLEAR
   $INFOTEXT -u "\nVerifying and setting file permissions"
   $ECHO

   euid=`$SGE_UTILBIN/uidgid -euid`

   if [ $euid != 0 ]; then
      $INFOTEXT "You are not installing as user >root<\n"
      $INFOTEXT "Can't set the file owner/group and permissions\n"
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
      return 0
   else
      if [ "$AUTO" = "true" -a "$SET_FILE_PERMS" = "true" ]; then
         :
      else
         if [ "$WINDOWS_SUPPORT" = true ]; then
            $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                      "Did you install this version with >pkgadd< or did you already\n" \
                      "verify and set the file permissions of your distribution (enter: y)\n\n" \
                      "In some cases, eg: the binaries are stored on a NTFS or on any other\n" \
                      "filesystem, which provides additional file permissions, the UNIX file\n" \
                      "permissions can be wrong. In this case we would advise to verfiy and\n" \
                      "to set the file permissions (enter: n) (y/n) [n] >> "
         else
            $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                      "Did you install this version with >pkgadd< or did you already\n" \
                      "verify and set the file permissions of your distribution (y/n) [y] >> "
         fi
         if [ $? = 0 ]; then
            $INFOTEXT -wait -auto $AUTO -n "We do not verify file permissions. Hit <RETURN> to continue >> "
            $CLEAR
            return 0
         fi
      fi
   fi

   rm -f ./tst$$ 2> /dev/null > /dev/null
   touch ./tst$$ 2> /dev/null > /dev/null
   ret=$?
   rm -f ./tst$$ 2> /dev/null > /dev/null
   if [ $ret != 0 ]; then
      $INFOTEXT -u "\nVerifying and setting file permissions (continued)"

      $INFOTEXT "\nWe can't set file permissions on this machine, because user root\n" \
                  "has not the necessary privileges to change file permissions\n" \
                  "on this file system.\n\n" \
                  "Probably this file system is an NFS mount where user root is\n" \
                  "mapped to user >nobody<.\n\n" \
                  "Please login now at your file server and set the file permissions and\n" \
                  "ownership of the entire distribution with the command:\n\n" \
                  "   # \$SGE_ROOT/util/setfileperm.sh \$SGE_ROOT\n\n" 

      $INFOTEXT -wait -auto $AUTO -n "Please hit <RETURN> to continue once you set your file permissions >> "
      $CLEAR
      return 0
   else
      $CLEAR
      $INFOTEXT -u "\nVerifying and setting file permissions"
      $INFOTEXT "\nWe may now verify and set the file permissions of your Grid Engine\n" \
                "distribution.\n\n" \
                 "This may be useful since due to unpacking and copying of your distribution\n" \
                 "your files may be unaccessible to other users.\n\n" \
                 "We will set the permissions of directories and binaries to\n\n" \
                 "   755 - that means executable are accessible for the world\n\n" \
                 "and for ordinary files to\n\n" \
                 "   644 - that means readable for the world\n\n"

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
               "Do you want to verify and set your file permissions (y/n) [y] >> "
      ret=$?
   fi

   if [ $ret = 0 ]; then

      if [ $RESPORT = true ]; then
         resportarg="-resport"
      else
         resportarg="-noresport"
      fi

      if [ $ADMINUSER = default ]; then
         fileowner=root
      else
         fileowner=$ADMINUSER
      fi

      filegid=`$SGE_UTILBIN/uidgid -gid`

      $CLEAR

      util/setfileperm.sh -auto $SGE_ROOT

      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   else
      $INFOTEXT -wait -auto $AUTO -n "We will not verify your file permissions. Hit <RETURN> to continue >>"
   fi
   $CLEAR
}

SetSpoolingOptionsBerkeleyDB()
{
   SPOOLING_METHOD=berkeleydb
   SPOOLING_LIB=libspoolb
   SPOOLING_SERVER=none
   SPOOLING_DIR="spooldb"
   MKDIR="mkdir -p"
   params_ok=0
   if [ "$AUTO" = "true" ]; then
      SPOOLING_SERVER=$DB_SPOOLING_SERVER
      SPOOLING_DIR="$DB_SPOOLING_DIR"

      if [ "$SPOOLING_DIR" = "" ]; then
         $INFOTEXT -log "Please enter a Berkeley DB spooling directory!"
         MoveLog
         exit 1
      fi

      if [ "$SPOOLING_SERVER" = "" ]; then
         $INFOTEXT -log "Please enter a Berkeley DB spooling server!"
         MoveLog
         exit 1
      fi

      if [ -d "$SPOOLING_DIR" -a \( "$SPOOLING_SERVER" = "none" -o "$SPOOLING_SERVER" = "" \) ]; then
         $INFOTEXT -log "The spooling directory [%s] already exists! Exiting installation!" "$SPOOLING_DIR"
         MoveLog
         exit 1 
      fi
      SpoolingCheckParams
      params_ok=$?
   fi
   if [ "$QMASTER" = "install" ]; then
      $INFOTEXT -n "\nThe Berkeley DB spooling method provides two configurations!\n\n" \
                   " Local spooling:\n" \
                   " The Berkeley DB spools into a local directory on this host (qmaster host)\n" \
                   " This setup is faster, but you can't setup a shadow master host\n\n"
      $INFOTEXT -n " Berkeley DB Spooling Server:\n" \
                   " If you want to setup a shadow master host, you need to use\nBerkeley DB Spooling Server!\n" \
                   " In this case you have to choose a host with a configured RPC service.\nThe qmaster host" \
                   " connects via RPC to the Berkeley DB. This setup is more\nfailsafe," \
                   " but results in a clear potential security hole. RPC communication\n" \
                   " (as used by Berkeley DB) can be easily compromised. Please only use this\n" \
                   " alternative if your site is secure or if you are not concerned about\n" \
                   " security. Check the installation guide for further advice on how to achieve\n" \
                   " failsafety without compromising security.\n\n"

      if [ "$AUTO" = "true" ]; then
         if [ "$DB_SPOOLING_SERVER" = "none" ]; then
            is_server="false"
         else
            is_server="true"
         fi
      else
         $INFOTEXT -n -ask "y" "n" -def "n" "Do you want to use a Berkeley DB Spooling Server? (y/n) [n] >> "
         if [ $? = 0 ]; then
            $INFOTEXT -u "Berkeley DB Setup\n"
            $INFOTEXT "Please, log in to your Berkeley DB spooling host and execute \"inst_sge -db\""
            $INFOTEXT -auto $AUTO -wait -n "Please do not continue, before the Berkeley DB installation with\n" \
                                        "\"inst_sge -db\" is completed, continue with <RETURN>"
            is_server="true"
         else
            is_server="false"
            $INFOTEXT -n -auto $AUTO -wait "\nHit <RETURN> to continue >> "
            $CLEAR
         fi
      fi

      if [ $is_server = "true" ]; then
         SpoolingQueryChange
      else
         done="false"
         is_spool="false"

         while [ $is_spool = "false" ] && [ $done = "false" ]; do
            $CLEAR
            SpoolingQueryChange
            if [ -d $SPOOLING_DIR ]; then
               $INFOTEXT -n -ask "y" "n" -def "n" -auto $AUTO "The spooling directory already exists! Do you want to delete it? [n] >> "
               ret=$?               
               if [ $AUTO = true ]; then
                  $INFOTEXT -log "The spooling directory already exists!\n Please remove it or choose any other spooling directory!"
                  exit 1
               fi
 
               if [ $ret = 0 ]; then
                     RM="rm -r"
                     ExecuteAsAdmin $RM $SPOOLING_DIR
                     if [ -d $SPOOLING_DIR ]; then
                        $INFOTEXT "You are not the owner of this directory. You can't delete it!"
                     else
                        is_spool="true"
                     fi
               else
                  $INFOTEXT -wait "Please hit <ENTER>, to choose any other spooling directory!"
               fi
             else
                is_spool="true"
             fi

            CheckLocalFilesystem $SPOOLING_DIR
            ret=$?
            if [ $ret -eq 0 -a "$AUTO" = "false" ]; then
               $INFOTEXT "\nThe database directory\n\n" \
                            "   %s\n\n" \
                            "is not on a local filesystem. Please choose a local filesystem or\n" \
                            "configure the RPC Client/Server mechanism." $SPOOLING_DIR
               $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
            else
               done="true" 
            fi
            
            if [ $is_spool = "false" ]; then
               done="false"
            elif [ $done = "false" ]; then
               is_spool="false"
            fi

         done
       fi
   else
      if [ "$ARCH" = "darwin" ]; then
         ret=`ps ax | grep "berkeley_db_svc" | wc -l` 
      else
         ret=`ps -efa | grep "berkeley_db_svc" | wc -l` 
      fi
      if [ $ret -gt 1 ]; then
         $INFOTEXT "We found a running berkeley db server on this host!"
         if [ "$AUTO" = "true" ]; then
               if [ $SPOOLING_SERVER = "none" ]; then
                  $ECHO
                  ExecuteAsAdmin $MKDIR $SPOOLING_DIR
                  SPOOLING_ARGS="$SPOOLING_DIR"
               else
                  $INFOTEXT -log "We found a running berkeley db server on this host!"
                  $INFOTEXT -log "Please, check this first! Exiting Installation!"
                  MoveLog
                  exit 1
               fi
         else                 # $AUTO != true
            $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" "Do you want to use an other host for spooling? (y/n) [n] >>"
            if [ $? = 1 ]; then
               $INFOTEXT "Please enter the path to your Berkeley DB startup script! >>"
               TMP_STARTUP_SCRIPT=`Enter`
               SpoolingQueryChange
               EditStartupScript
            else
               exit 1
            fi
         fi 
      else
         while [ $params_ok -eq 0 ]; do
            do_loop="true"
            while [ $do_loop = "true" ]; do
               SpoolingQueryChange
               if [ -d $SPOOLING_DIR ]; then
                  $INFOTEXT -n -ask "y" "n" -def "n" -auto "$AUTO" "The spooling directory already exists! Do you want to delete it? (y/n) [n] >> "
                  ret=$?               
                  if [ "$AUTO" = true ]; then
                     $INFOTEXT -log "The spooling directory already exists!\n Please remove it or choose any other spooling directory!"
                     MoveLog
                     exit 1
                  fi
 
                  if [ $ret = 0 ]; then
                        RM="rm -r"
                        ExecuteAsAdmin $RM $SPOOLING_DIR
                        if [ -d $SPOOLING_DIR ]; then
                           $INFOTEXT "You are not the owner of this directory. You can't delete it!"
                        else
                           do_loop="false"
                        fi
                  else
                     $INFOTEXT -wait "Please hit <ENTER>, to choose any other spooling directory!"
                     SPOOLING_DIR="spooldb"
                  fi
                else
                   do_loop="false"
               fi
            done
            SpoolingCheckParams
            params_ok=$?
         done
      fi
   fi

   if [ "$SPOOLING_SERVER" = "none" ]; then
      $ECHO
      ExecuteAsAdmin $MKDIR $SPOOLING_DIR
      SPOOLING_ARGS="$SPOOLING_DIR"
   else
      SPOOLING_ARGS="$SPOOLING_SERVER:`basename $SPOOLING_DIR`"
   fi
}

SetSpoolingOptionsClassic()
{
   SPOOLING_METHOD=classic
   SPOOLING_LIB=libspoolc
   SPOOLING_ARGS="$SGE_ROOT_VAL/$COMMONDIR;$QMDIR"
}

SetSpoolingOptionsDynamic()
{
   if [ "$AUTO" = "true" ]; then
      if [ "$SPOOLING_METHOD" != "berkeleydb" -a "$SPOOLING_METHOD" != "classic" ]; then
         SPOOLING_METHOD="berkeleydb"
      fi
   else
      if [ "$BERKELEY" = "install" ]; then
         SPOOLING_METHOD="berkeleydb"
      else
         $INFOTEXT -n "Your SGE binaries are compiled to link the spooling libraries\n" \
                      "during runtime (dynamically). So you can choose between Berkeley DB \n" \
                      "spooling and Classic spooling method."
         $INFOTEXT -n "\nPlease choose a spooling method (berkeleydb|classic) [berkeleydb] >> "
         SPOOLING_METHOD=`Enter berkeleydb`
      fi
   fi

   $CLEAR

   case $SPOOLING_METHOD in 
      classic)
         SetSpoolingOptionsClassic
         ;;
      berkeleydb)
         SetSpoolingOptionsBerkeleyDB
         ;;
      *)
         $INFOTEXT "\nUnknown spooling method. Exit."
         $INFOTEXT -log "\nUnknown spooling method. Exit."
         exit 1
         ;;
   esac
}

#--------------------------------------------------------------------------
# SetSpoolingOptions sets / queries options for the spooling framework
#
SetSpoolingOptions()
{
   $INFOTEXT -u "\nSetup spooling"
   COMPILED_IN_METHOD=`ExecuteAsAdmin $SPOOLINIT method`
   $INFOTEXT -log "Setting spooling method to %s" $COMPILED_IN_METHOD
   case "$COMPILED_IN_METHOD" in 
      classic)
         SetSpoolingOptionsClassic
         ;;
      berkeleydb)
         SetSpoolingOptionsBerkeleyDB
         ;;
      dynamic)
         SetSpoolingOptionsDynamic
         ;;
      *)
         $INFOTEXT "\nUnknown spooling method. Exit."
         $INFOTEXT -log "\nUnknown spooling method. Exit."
         exit 1
         ;;
   esac
}


#-------------------------------------------------------------------------
# Ask the installer for the hostname resolving method
# (IGNORE_FQND=true/false)
#
SelectHostNameResolving()
{
   if [ $AUTO = "true" ]; then
     IGNORE_FQDN_DEFAULT=$HOSTNAME_RESOLVING
     $INFOTEXT -log "Using >%s< as IGNORE_FQDN_DEFAULT." "$IGNORE_FQDN_DEFAULT"
     $INFOTEXT -log "If it's >true<, the domainname will be ignored."
     
   else
     $CLEAR
     $INFOTEXT -u "\nSelect default Grid Engine hostname resolving method"
     $INFOTEXT "\nAre all hosts of your cluster in one DNS domain? If this is\n" \
               "the case the hostnames\n\n" \
               "   >hostA< and >hostA.foo.com<\n\n" \
               "would be treated as equal, because the DNS domain name >foo.com<\n" \
               "is ignored when comparing hostnames.\n\n"

     $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
               "Are all hosts of your cluster in a single DNS domain (y/n) [y] >> "
     if [ $? = 0 ]; then
        IGNORE_FQDN_DEFAULT=true
        $INFOTEXT "Ignoring domainname when comparing hostnames."
     else
        IGNORE_FQDN_DEFAULT=false
        $INFOTEXT "The domainname is not ignored when comparing hostnames."
     fi
     $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
     $CLEAR
   fi

   if [ "$IGNORE_FQDN_DEFAULT" = "false" ]; then
      GetDefaultDomain
   else
      CFG_DEFAULT_DOMAIN=none
   fi
}


#-------------------------------------------------------------------------
# SetProductMode
#
SetProductMode()
{
   if [ $AFS = true ]; then
      AFS_PREFIX="afs"
   else
      AFS_PREFIX=""
   fi

   if [ $CSP = true ]; then

      case $SGE_ARCH in

      aix* | hp11*)
          SEC_COUNT=`strings -a $SGE_BIN/sge_qmaster | grep "AIMK_SECURE_OPTION_ENABLED" | wc -l`
          ;;
         *)
          SEC_COUNT=`strings $SGE_BIN/sge_qmaster | grep "AIMK_SECURE_OPTION_ENABLED" | wc -l`
          ;;
      esac

      if [ $SEC_COUNT -ne 1 ]; then
         $INFOTEXT "\n>sge_qmaster< binary is not compiled with >-secure< option!\n"
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to cancel the installation >> "
         exit 1
      else
         CSP_PREFIX="csp"
      fi  
   else
      CSP_PREFIX=""
   fi

      if [ $AFS = "false" ]; then
         if [ $CSP = "false" ]; then
            PRODUCT_MODE="none"
         else
            PRODUCT_MODE="${CSP_PREFIX}"
         fi
      else
         PRODUCT_MODE="${AFS_PREFIX}"
      fi
}


#-------------------------------------------------------------------------
# Make directories needed by qmaster
#
MakeDirsMaster()
{
   $INFOTEXT -u "\nMaking directories"
   $ECHO
   $INFOTEXT -log "Making directories"
   Makedir $SGE_CELL_VAL
   Makedir $COMMONDIR
   Makedir $QMDIR
   Makedir $QMDIR/job_scripts

   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# Adding Bootstrap information
#
AddBootstrap()
{
   TOUCH=touch
   $INFOTEXT "Dumping bootstrapping information"
   $INFOTEXT -log "Dumping bootstrapping information"
   ExecuteAsAdmin rm -f $COMMONDIR/bootstrap
   ExecuteAsAdmin $TOUCH $COMMONDIR/bootstrap
   ExecuteAsAdmin chmod 666 $COMMONDIR/bootstrap
   PrintBootstrap >> $COMMONDIR/bootstrap
   ExecuteAsAdmin chmod 444 $COMMONDIR/bootstrap
}

#-------------------------------------------------------------------------
# PrintBootstrap: print SGE default configuration
#
PrintBootstrap()
{
   $ECHO "# Version: $SGE_VERSION"
   $ECHO "#"
   if [ $ADMINUSER != default ]; then
      $ECHO "admin_user              $ADMINUSER"
   else
      $ECHO "admin_user              none"
   fi
   $ECHO "default_domain          $CFG_DEFAULT_DOMAIN"
   $ECHO "ignore_fqdn             $IGNORE_FQDN_DEFAULT"
   $ECHO "spooling_method         $SPOOLING_METHOD"
   $ECHO "spooling_lib            $SPOOLING_LIB"
   $ECHO "spooling_params         $SPOOLING_ARGS"
   $ECHO "binary_path             $SGE_ROOT_VAL/bin"
   $ECHO "qmaster_spool_dir       $QMDIR"
   $ECHO "security_mode           $PRODUCT_MODE"
   $ECHO "listener_threads        4"
   $ECHO "worker_threads          4"
   $ECHO "scheduler_threads       1"
   if [ "$SGE_ENABLE_JMX" = "true" ]; then
      $ECHO "jvm_threads             1"
   else
      $ECHO "jvm_threads             0"
   fi
}


#-------------------------------------------------------------------------
# Initialize the spooling database (or directory structure)
#
InitSpoolingDatabase()
{
   $INFOTEXT "Initializing spooling database"
   $INFOTEXT -log "Initializing spooling database"
   ExecuteAsAdmin $SPOOLINIT $SPOOLING_METHOD $SPOOLING_LIB "$SPOOLING_ARGS" init

   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# AddConfiguration
#
AddConfiguration()
{
   useold=false

   if [ $useold = false ]; then
      GetConfiguration
      #TruncCreateAndMakeWriteable $COMMONDIR/configuration
      #PrintConf >> $COMMONDIR/configuration
      #SetPerm $COMMONDIR/configuration
      TMPC=/tmp/configuration
      TOUCH=touch
      rm -f $TMPC
      ExecuteAsAdmin $TOUCH $TMPC
      PrintConf >> $TMPC
      SetPerm $TMPC
      ExecuteAsAdmin $SPOOLDEFAULTS configuration $TMPC
      rm -f $TMPC
   fi
}

#-------------------------------------------------------------------------
# PrintConf: print SGE default configuration
#
PrintConf()
{
   $ECHO "# Version: $SGE_VERSION"
   $ECHO "#"
   $ECHO "# DO NOT MODIFY THIS FILE MANUALLY!"
   $ECHO "#"
   $ECHO "conf_version           0"
   $ECHO "execd_spool_dir        $CFG_EXE_SPOOL"
   $ECHO "mailer                 $MAILER"
   $ECHO "xterm                  $XTERM"
   $ECHO "load_sensor            none"
   $ECHO "prolog                 none"
   $ECHO "epilog                 none"
   $ECHO "shell_start_mode       posix_compliant"
   $ECHO "login_shells           sh,ksh,csh,tcsh"
   $ECHO "min_uid                0"
   $ECHO "min_gid                0"
   $ECHO "user_lists             none"
   $ECHO "xuser_lists            none"
   $ECHO "projects               none"
   $ECHO "xprojects              none"
   $ECHO "enforce_project        false"
   $ECHO "enforce_user           auto"
   $ECHO "load_report_time       00:00:40"
   $ECHO "max_unheard            00:05:00"
   $ECHO "reschedule_unknown     00:00:00"
   $ECHO "loglevel               log_warning"
   $ECHO "administrator_mail     $CFG_MAIL_ADDR"
   if [ "$AFS" = true ]; then
      $ECHO "set_token_cmd          /path_to_token_cmd/set_token_cmd"
      $ECHO "pag_cmd                /usr/afsws/bin/pagsh"
      $ECHO "token_extend_time      24:0:0"
   else
      $ECHO "set_token_cmd          none"
      $ECHO "pag_cmd                none"
      $ECHO "token_extend_time      none"
   fi
   $ECHO "shepherd_cmd           none"
   $ECHO "qmaster_params         none"
   if [ "$WINDOWS_SUPPORT" = "true" -a "$WIN_DOMAIN_ACCESS" = "true" ]; then
      $ECHO "execd_params           enable_windomacc=true"
   elif [ "$WINDOWS_SUPPORT" = "true" -a "$WIN_DOMAIN_ACCESS" = "false" ]; then
      $ECHO "execd_params           enable_windomacc=false"
   else
      $ECHO "execd_params           none"
   fi
   $ECHO "reporting_params       accounting=true reporting=false flush_time=00:00:15 joblog=false sharelog=00:00:00"
   $ECHO "finished_jobs          100"
   $ECHO "gid_range              $CFG_GID_RANGE"
   $ECHO "qlogin_command         $QLOGIN_COMMAND"
   $ECHO "qlogin_daemon          $QLOGIN_DAEMON"
   $ECHO "rlogin_daemon          $RLOGIN_DAEMON"
   $ECHO "max_aj_instances       2000"
   $ECHO "max_aj_tasks           75000"
   $ECHO "max_u_jobs             0"
   $ECHO "max_jobs               0"
   $ECHO "max_advance_reservations 0"
   $ECHO "auto_user_oticket      0"
   $ECHO "auto_user_fshare       0"
   $ECHO "auto_user_default_project none"
   $ECHO "auto_user_delete_time  86400"
   $ECHO "delegated_file_staging false"
   $ECHO "reprioritize           0"
   if [ "$SGE_JVM_LIB_PATH" != "" ]; then
      $ECHO "libjvm_path            $SGE_JVM_LIB_PATH"
   fi
   if [ "$SGE_ADDITIONAL_JVM_ARGS" != "" ]; then
      $ECHO "additional_jvm_args            $SGE_ADDITIONAL_JVM_ARGS"
   fi
}


#-------------------------------------------------------------------------
# AddLocalConfiguration
#
AddLocalConfiguration()
{
   $CLEAR
   $INFOTEXT -u "\nCreating local configuration"

      ExecuteAsAdmin mkdir /tmp/$$
      TMPH=/tmp/$$/$HOST
      ExecuteAsAdmin rm -f $TMPH
      ExecuteAsAdmin touch $TMPH
      PrintLocalConf 1 >> $TMPH
      ExecuteAsAdmin $SPOOLDEFAULTS local_conf $TMPH $HOST
      ExecuteAsAdmin rm -rf /tmp/$$
}


#-------------------------------------------------------------------------
# GetConfiguration: get some parameters for global configuration
#
GetConfiguration()
{

   GetGidRange

   #if [ $fast = true ]; then
   #   CFG_EXE_SPOOL=$SGE_ROOT_VAL/$SGE_CELL_VAL/spool
   #   CFG_MAIL_ADDR=none
   #   return 0
   #fi
   if [ $AUTO = true ]; then
     CFG_EXE_SPOOL=$EXECD_SPOOL_DIR
     CFG_MAIL_ADDR=$ADMIN_MAIL
     $INFOTEXT -log "Using >%s< as EXECD_SPOOL_DIR." "$CFG_EXE_SPOOL"
     $INFOTEXT -log "Using >%s< as ADMIN_MAIL." "$ADMIN_MAIL"
   else
   done=false
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nGrid Engine cluster configuration"
      $INFOTEXT "\nPlease give the basic configuration parameters of your Grid Engine\n" \
                "installation:\n\n   <execd_spool_dir>\n\n"

      if [ $ADMINUSER != default ]; then
            $INFOTEXT "The pathname of the spool directory of the execution hosts. User >%s<\n" \
                      "must have the right to create this directory and to write into it.\n" "$ADMINUSER"
      elif [ $euid = 0 ]; then
            $INFOTEXT "The pathname of the spool directory of the execution hosts. User >root<\n" \
                      "must have the right to create this directory and to write into it.\n"
      else
            $INFOTEXT "The pathname of the spool directory of the execution hosts. You\n" \
                      "must have the right to create this directory and to write into it.\n"
      fi

      $INFOTEXT -n "Default: [%s] >> " $SGE_ROOT_VAL/$SGE_CELL_VAL/spool

      CFG_EXE_SPOOL=`Enter $SGE_ROOT_VAL/$SGE_CELL_VAL/spool`

      $CLEAR
      $INFOTEXT -u "\nGrid Engine cluster configuration (continued)"
      $INFOTEXT -n "\n<administrator_mail>\n\n" \
                   "The email address of the administrator to whom problem reports are sent.\n\n" \
                   "It's is recommended to configure this parameter. You may use >none<\n" \
                   "if you do not wish to receive administrator mail.\n\n" \
                   "Please enter an email address in the form >user@foo.com<.\n\n" \
                   "Default: [none] >> "

      CFG_MAIL_ADDR=`Enter none`

      $CLEAR

      $INFOTEXT "\nThe following parameters for the cluster configuration were configured:\n\n" \
                "   execd_spool_dir        %s\n" \
                "   administrator_mail     %s\n" $CFG_EXE_SPOOL $CFG_MAIL_ADDR

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to change the configuration parameters (y/n) [n] >> "
      if [ $? = 1 ]; then
         done=true
      fi
   done
   fi
   export CFG_EXE_SPOOL
}


#-------------------------------------------------------------------------
# GetGidRange
#
GetGidRange()
{
   done=false
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nGrid Engine group id range"
      $INFOTEXT "\nWhen jobs are started under the control of Grid Engine an additional group id\n" \
                "is set on platforms which do not support jobs. This is done to provide maximum\n" \
                "control for Grid Engine jobs.\n\n" \
                "This additional UNIX group id range must be unused group id's in your system.\n" \
                "Each job will be assigned a unique id during the time it is running.\n" \
                "Therefore you need to provide a range of id's which will be assigned\n" \
                "dynamically for jobs.\n\n" \
                "The range must be big enough to provide enough numbers for the maximum number\n" \
                "of Grid Engine jobs running at a single moment on a single host. E.g. a range\n" \
                "like >20000-20100< means, that Grid Engine will use the group ids from\n" \
                "20000-20100 and provides a range for 100 Grid Engine jobs at the same time\n" \
                "on a single host.\n\n" \
                "You can change at any time the group id range in your cluster configuration.\n"

      $INFOTEXT -n "Please enter a range >> "

      CFG_GID_RANGE=`Enter $GID_RANGE`

      if [ "$CFG_GID_RANGE" != "" ]; then
         $INFOTEXT -wait -auto $AUTO -n "\nUsing >%s< as gid range. Hit <RETURN> to continue >> " \
                   "$CFG_GID_RANGE"
         $CLEAR
         done=true
      fi
     if [ $AUTO = true -a "$GID_RANGE" = "" ]; then
        $INFOTEXT -log "Please enter a valid GID Range. Installation failed!"
        MoveLog
        exit 1
     fi
 
     $INFOTEXT -log "Using >%s< as gid range." "$CFG_GID_RANGE"  
   done
}


#-------------------------------------------------------------------------
# AddActQmaster: create act_qmaster file
#
AddActQmaster()
{
   $INFOTEXT "Creating >act_qmaster< file"

   TruncCreateAndMakeWriteable $COMMONDIR/act_qmaster
   $ECHO $HOST >> $COMMONDIR/act_qmaster
   SetPerm $COMMONDIR/act_qmaster
}


#-------------------------------------------------------------------------
# AddDefaultComplexes
#
AddDefaultComplexes()
{
   $INFOTEXT "Adding default complex attributes"
   ExecuteAsAdmin $SPOOLDEFAULTS complexes $SGE_ROOT_VAL/util/resources/centry

}


#-------------------------------------------------------------------------
# AddCommonFiles
#    Copy files from util directory to common dir
#
AddCommonFiles()
{
   for f in sge_aliases qtask sge_request; do
      if [ $f = sge_aliases ]; then
         $INFOTEXT "Adding >%s< path aliases file" $f
      elif [ $f = qtask ]; then
         $INFOTEXT "Adding >%s< qtcsh sample default request file" $f
      else
         $INFOTEXT "Adding >%s< default submit options file" $f
      fi
      ExecuteAsAdmin cp util/$f $COMMONDIR
      ExecuteAsAdmin chmod $FILEPERM $COMMONDIR/$f
   done

}

AddJMXFiles() {
   if [ "$SGE_JMX_PORT" != "" ]; then
      jmx_dir=$COMMONDIR/jmx
      ExecuteAsAdmin mkdir -p $jmx_dir
      
      $INFOTEXT "Adding >jmx/%s< jmx remote access file" jmxremote.access
      ExecuteAsAdmin cp util/jmxremote.access $jmx_dir/jmxremote.access
      ExecuteAsAdmin chmod $FILEPERM $jmx_dir/jmxremote.access
      
      $INFOTEXT "Adding >jmx/%s< jmx remote password file" jmxremote.password
      ExecuteAsAdmin cp util/jmxremote.password $jmx_dir/jmxremote.password
      ExecuteAsAdmin chmod 600 $jmx_dir/jmxremote.password
     
      $INFOTEXT "Adding >jmx/%s< jmx logging configuration" logging.properties
      ExecuteAsAdmin cp util/logging.properties.template $jmx_dir/logging.properties
      ExecuteAsAdmin chmod 644 $jmx_dir/logging.properties

      $INFOTEXT "Adding >jmx/%s< java policies configuration" java.policy
      ExecuteAsAdmin cp util/java.policy.template $jmx_dir/java.policy
      ExecuteAsAdmin chmod 644 $jmx_dir/java.policy

      $INFOTEXT "Adding >jmx/%s< jaas configuration" jaas.config
      ExecuteAsAdmin cp util/jaas.config.template $jmx_dir/jaas.config
      ExecuteAsAdmin chmod 644 $jmx_dir/jaas.config

      ExecuteAsAdmin touch /tmp/management.properties.$$
      Execute sed -e "s#@@SGE_JMX_PORT@@#$SGE_JMX_PORT#g" \
                  -e "s#@@SGE_ROOT@@#$SGE_ROOT#g" \
                  -e "s#@@SGE_CELL@@#$SGE_CELL#g" \
                  util/management.properties.template > /tmp/management.properties.$$
      ExecuteAsAdmin mv /tmp/management.properties.$$ $jmx_dir/management.properties
      ExecuteAsAdmin chmod $FILEPERM $jmx_dir/management.properties
      $INFOTEXT "Adding >jmx/%s< jmx configuration" management.properties
      
   fi
}

#-------------------------------------------------------------------------
# AddPEFiles
#    Copy files from PE template directory to qmaster spool dir
#
AddPEFiles()
{
   $INFOTEXT "Adding default parallel environments (PE)"
   $INFOTEXT -log "Adding default parallel environments (PE)"
   ExecuteAsAdmin $SPOOLDEFAULTS pes $SGE_ROOT_VAL/util/resources/pe
}


#-------------------------------------------------------------------------
# AddDefaultUsersets
#
AddDefaultUsersets()
{
      $INFOTEXT "Adding SGE default usersets"
      ExecuteAsAdmin $SPOOLDEFAULTS usersets $SGE_ROOT_VAL/util/resources/usersets
}


#-------------------------------------------------------------------------
# CreateSettingsFile: Create resource files for csh/sh
#
CreateSettingsFile()
{
   $INFOTEXT "Creating settings files for >.profile/.cshrc<"

   if [ $RECREATE_SETTINGS = "false" ]; then
      if [ -f $SGE_ROOT/$SGE_CELL/common/settings.sh ]; then
         ExecuteAsAdmin $RM $SGE_ROOT/$SGE_CELL/common/settings.sh
      fi
  
      if [ -f $SGE_ROOT/$SGE_CELL/common/settings.csh ]; then
         ExecuteAsAdmin $RM $SGE_ROOT/$SGE_CELL/common/settings.csh
      fi
   fi

   ExecuteAsAdmin util/create_settings.sh $SGE_ROOT_VAL/$COMMONDIR

   SetPerm $SGE_ROOT_VAL/$COMMONDIR/settings.sh
   SetPerm $SGE_ROOT_VAL/$COMMONDIR/settings.csh

   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
}


#--------------------------------------------------------------------------
# InitCA Create CA and initialize it for deamons and users
#
InitCA()
{

   if [ "$CSP" = true -o \( "$WINDOWS_SUPPORT" = "true" -a "$WIN_DOMAIN_ACCESS" = "true" \) ]; then
      # Initialize CA, make directories and get DN info
      #
      SGE_CA_CMD=util/sgeCA/sge_ca
      if [ "$AUTO" = "true" ]; then
         if [ "$CSP_RECREATE" = "true" ]; then
            $SGE_CA_CMD -init -days 365 -auto $FILE
            #if [ -f "$CSP_USERFILE" ]; then
            #   $SGE_CA_CMD -usercert $CSP_USERFILE
            #fi
         fi
      else
         $SGE_CA_CMD -init -days 365
      fi

      #  TODO: CAErrUsage no longer available, error handling ???:w
      
      $INFOTEXT -auto $AUTO -wait -n "Hit <RETURN> to continue >> "
      $CLEAR
   fi
}


#--------------------------------------------------------------------------
# StartQmaster
#
StartQmaster()
{
   $INFOTEXT -u "\nGrid Engine qmaster startup"
   $INFOTEXT "\nStarting qmaster daemon. Please wait ..."
   $SGE_STARTUP_FILE -qmaster

   CheckRunningDaemon sge_qmaster
   run=$?
   if [ $run -ne 0 ]; then
      $INFOTEXT "sge_qmaster daemon didn't start. Please check your\n" \
                "configuration! Installation failed!"
      $INFOTEXT -log "sge_qmaster daemon didn't start. Please check your\n" \
                     "autoinstall configuration file! Installation failed!"
      if [ $AUTO = true ]; then
         MoveLog
      fi

      exit 1
   fi
   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# AddHosts
#
AddHosts()
{
   if [ "$AUTO" = "true" ]; then
      for h in $ADMIN_HOST_LIST; do
        if [ -f $h ]; then
           $INFOTEXT -log "Adding ADMIN_HOSTS from file %s" $h
           for tmp in `cat $h`; do
             $INFOTEXT -log "Adding ADMIN_HOST %s" $tmp
             $SGE_BIN/qconf -ah $tmp
           done
        else
             $INFOTEXT -log "Adding ADMIN_HOST %s" $h
             $SGE_BIN/qconf -ah $h
        fi
      done
      
      for h in $SUBMIT_HOST_LIST; do
        if [ -f $h ]; then
           $INFOTEXT -log "Adding SUBMIT_HOSTS from file %s" $h
           for tmp in `cat $h`; do
             $INFOTEXT -log "Adding SUBMIT_HOST %s" $tmp
             $SGE_BIN/qconf -as $tmp
           done
        else
             $INFOTEXT -log "Adding SUBMIT_HOST %s" $h
             $SGE_BIN/qconf -as $h
        fi
      done  

      for h in $SHADOW_HOST; do
        if [ -f $h ]; then
           $INFOTEXT -log "Adding SHADOW_HOSTS from file %s" $h
           for tmp in `cat $h`; do
             $INFOTEXT -log "Adding SHADOW_HOST %s" $tmp
             $SGE_BIN/qconf -ah $tmp
           done
        else
             $INFOTEXT -log "Adding SHADOW_HOST %s" $h
             $SGE_BIN/qconf -ah $h
        fi
      done
  
   else
      $INFOTEXT -u "\nAdding Grid Engine hosts"
      $INFOTEXT "\nPlease now add the list of hosts, where you will later install your execution\n" \
                "daemons. These hosts will be also added as valid submit hosts.\n\n" \
                "Please enter a blank separated list of your execution hosts. You may\n" \
                "press <RETURN> if the line is getting too long. Once you are finished\n" \
                "simply press <RETURN> without entering a name.\n\n" \
                "You also may prepare a file with the hostnames of the machines where you plan\n" \
                "to install Grid Engine. This may be convenient if you are installing Grid\n" \
                "Engine on many hosts.\n\n"

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to use a file which contains the list of hosts (y/n) [n] >> "
      ret=$?
      if [ $ret = 0 ]; then
         AddHostsFromFile execd
         ret=$?
      fi

      if [ $ret = 1 ]; then
         AddHostsFromTerminal execd
      fi

      $INFOTEXT -wait -auto $AUTO -n "Finished adding hosts. Hit <RETURN> to continue >> "
      $CLEAR

      # Adding later shadow hosts to the admin host list
      $INFOTEXT "\nIf you want to use a shadow host, it is recommended to add this host\n" \
                "to the list of administrative hosts.\n\n" \
                "If you are not sure, it is also possible to add or remove hosts after the\n" \
                "installation with <qconf -ah hostname> for adding and <qconf -dh hostname>\n" \
                "for removing this host\n\nAttention: This is not the shadow host installation" \
                "procedure.\n You still have to install the shadow host separately\n\n"
      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                "Do you want to add your shadow host(s) now? (y/n) [y] >> "
      ret=$?
      if [ "$ret" = 0 ]; then
         $CLEAR
         $INFOTEXT -u "\nAdding Grid Engine shadow hosts"
         $INFOTEXT "\nPlease now add the list of hosts, where you will later install your shadow\n" \
                   "daemon.\n\n" \
                   "Please enter a blank separated list of your execution hosts. You may\n" \
                   "press <RETURN> if the line is getting too long. Once you are finished\n" \
                   "simply press <RETURN> without entering a name.\n\n" \
                   "You also may prepare a file with the hostnames of the machines where you plan\n" \
                   "to install Grid Engine. This may be convenient if you are installing Grid\n" \
                   "Engine on many hosts.\n\n"

         $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                   "Do you want to use a file which contains the list of hosts (y/n) [n] >> "
         ret=$?
         if [ $ret = 0 ]; then
            AddHostsFromFile shadowd 
            ret=$?
         fi

         if [ $ret = 1 ]; then
            AddHostsFromTerminal shadowd
         fi

         $INFOTEXT -wait -auto $AUTO -n "Finished adding hosts. Hit <RETURN> to continue >> "
      fi
      $CLEAR

   fi

   $INFOTEXT -u "\nCreating the default <all.q> queue and <allhosts> hostgroup"
   echo
   $INFOTEXT -log "Creating the default <all.q> queue and <allhosts> hostgroup"
   TMPL=/tmp/hostqueue$$
   TMPL2=${TMPL}.q
   rm -f $TMPL $TMPL2
   if [ -f $TMPL -o -f $TMPL2 ]; then
      $INFOTEXT "\nCan't delete template files >%s< or >%s<" "$TMPL" "$TMPL2"
   else
      PrintHostGroup @allhosts > $TMPL
      Execute $SGE_BIN/qconf -Ahgrp $TMPL
      Execute $SGE_BIN/qconf -sq > $TMPL
      Execute sed -e "/qname/s/template/all.q/" \
                  -e "/hostlist/s/NONE/@allhosts/" \
                  -e "/pe_list/s/NONE/make/" $TMPL > $TMPL2
      Execute $SGE_BIN/qconf -Aq $TMPL2
      rm -f $TMPL $TMPL2        
   fi

   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
   $CLEAR

}


#-------------------------------------------------------------------------
# AddSubmitHosts
#
AddSubmitHosts()
{
   CERT_COPY_HOST_LIST=""
   if [ "$AUTO" = "true" ]; then
      CERT_COPY_HOST_LIST=$SUBMIT_HOST_LIST
      for h in $SUBMIT_HOST_LIST; do
        if [ -f $h ]; then
           $INFOTEXT -log "Adding SUBMIT_HOSTS from file %s" $h
           for tmp in `cat $h`; do
             $INFOTEXT -log "Adding SUBMIT_HOST %s" $tmp
             $SGE_BIN/qconf -as $tmp
           done
        else
             $INFOTEXT -log "Adding SUBMIT_HOST %s" $h
             $SGE_BIN/qconf -as $h
        fi
      done  
   else
      $INFOTEXT -u "\nAdding Grid Engine submit hosts"
      $INFOTEXT "\nPlease now add the list of hosts, which should become submit hosts.\n" \
                "Please enter a blank separated list of your submit hosts. You may\n" \
                "press <RETURN> if the line is getting too long. Once you are finished\n" \
                "simply press <RETURN> without entering a name.\n\n" \
                "You also may prepare a file with the hostnames of the machines where you plan\n" \
                "to install Grid Engine. This may be convenient if you are installing Grid\n" \
                "Engine on many hosts.\n\n"

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to use a file which contains the list of hosts (y/n) [n] >> "
      ret=$?
      if [ $ret = 0 ]; then
         AddHostsFromFile submit 
         ret=$?
      fi

      if [ $ret = 1 ]; then
         AddHostsFromTerminal submit 
      fi

      $INFOTEXT -wait -auto $AUTO -n "Finished adding hosts. Hit <RETURN> to continue >> "
      $CLEAR
   fi
}


#-------------------------------------------------------------------------
# AddHostsFromFile: Get a list of hosts and add them as
# admin and submit hosts
#
AddHostsFromFile()
{
   hosttype=$1
   file=$2
   done=false
   while [ $done = false ]; do
      $CLEAR
      if [ "$hosttype" = "execd" ]; then
         $INFOTEXT -u "\nAdding admin and submit hosts from file"
      elif [ "$hosttype" = "submit" ]; then
         $INFOTEXT -u "\nAdding submit hosts"
      else
         $INFOTEXT -u "\nAdding admin hosts from file"
      fi
      $INFOTEXT -n "\nPlease enter the file name which contains the host list: "
      file=`Enter none`
      if [ "$file" = "none" -o ! -f "$file" ]; then
         $INFOTEXT "\nYou entered an invalid file name or the file does not exist."
         $INFOTEXT -auto $autoinst -ask "y" "n" -def "y" -n \
                   "Do you want to enter a new file name (y/n) [y] >> "
         if [ $? = 1 ]; then
            return 1
         fi
      else
         if [ "$hosttype" = "execd" ]; then
            for h in `cat $file`; do
               $SGE_BIN/qconf -ah $h
               $SGE_BIN/qconf -as $h
            done
         elif [ "$hosttype" = "submit" ]; then
            for h in $hlist; do
               $SGE_BIN/qconf -as $h
               CERT_COPY_HOST_LIST="$CERT_COPY_HOST_LIST $h" 
            done
         else
            for h in `cat $file`; do
               $SGE_BIN/qconf -ah $h
            done
         fi
         done=true
      fi
   done
}

#-------------------------------------------------------------------------
# AddHostsFromTerminal
#    Get a list of hosts and add the mas admin and submit hosts
#
AddHostsFromTerminal()
{
   hosttype=$1
   stop=false
   while [ $stop = false ]; do
      $CLEAR
      if [ "$hosttype" = "execd" ]; then
         $INFOTEXT -u "\nAdding admin and submit hosts"
      elif [ "$hosttype" = "submit" ]; then
         $INFOTEXT -u "\nAdding submit hosts"
      else
         $INFOTEXT -u "\nAdding admin hosts"
      fi
      $INFOTEXT "\nPlease enter a blank seperated list of hosts.\n\n" \
                "Stop by entering <RETURN>. You may repeat this step until you are\n" \
                "entering an empty list. You will see messages from Grid Engine\n" \
                "when the hosts are added.\n"

      $INFOTEXT -n "Host(s): "

      hlist=`Enter ""`
      if [ "$hosttype" = "execd" ]; then
         for h in $hlist; do
            $SGE_BIN/qconf -ah $h
            $SGE_BIN/qconf -as $h
         done
      elif [ "$hosttype" = "submit" ]; then
         for h in $hlist; do
            $SGE_BIN/qconf -as $h
            CERT_COPY_HOST_LIST="$CERT_COPY_HOST_LIST $h"
         done
      else
         for h in $hlist; do
            $SGE_BIN/qconf -ah $h
         done
      fi
      if [ "$hlist" = "" ]; then
         stop=true
      else
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      fi
  done
}


#-------------------------------------------------------------------------
# PrintHostGroup:  print an empty hostgroup
#
PrintHostGroup()
{
   $ECHO "group_name  $1"
   $ECHO "hostlist    NONE"
}


#-------------------------------------------------------------------------
# GetQmasterPort: get communication port SGE_QMASTER_PORT
#
GetQmasterPort()
{

    if [ $RESPORT = true ]; then
       comm_port_max=1023
    else
       comm_port_max=65500
    fi

    CheckServiceAndPorts service $SGE_QMASTER_SRV

    if [ "$SGE_QMASTER_PORT" != "" ]; then
      $INFOTEXT -u "\nGrid Engine TCP/IP communication service"

      if [ $SGE_QMASTER_PORT -ge 1 -a $SGE_QMASTER_PORT -le $comm_port_max ]; then
         $INFOTEXT "\nUsing the environment variable\n\n" \
                   "   \$SGE_QMASTER_PORT=%s\n\n" \
                     "as port for communication.\n\n" $SGE_QMASTER_PORT
                      export SGE_QMASTER_PORT
                      $INFOTEXT -log "Using SGE_QMASTER_PORT >%s<." $SGE_QMASTER_PORT
         if [ $ret = 0 ]; then
            $INFOTEXT "This overrides the preset TCP/IP service >sge_qmaster<.\n"
         fi
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
         return
      else
         $INFOTEXT "\nThe environment variable\n\n" \
                   "   \$SGE_QMASTER_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please set the environment variable \$SGE_QMASTER_PORT and restart\n" \
                   "the installation or configure the service >sge_qmaster<." $SGE_QMASTER_PORT $comm_port_max
         $INFOTEXT -log "Your \$SGE_QMASTER_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please check your configuration file and restart\n" \
                   "the installation or configure the service >sge_qmaster<." $SGE_QMASTER_PORT $comm_port_max
      fi
   fi         
      $INFOTEXT -u "\nGrid Engine TCP/IP service >sge_qmaster<"
   if [ $ret != 0 ]; then
      $INFOTEXT "\nThere is no service >sge_qmaster< available in your >/etc/services< file\n" \
                "or in your NIS/NIS+ database.\n\n" \
                "You may add this service now to your services database or choose a port number.\n" \
                "It is recommended to add the service now. If you are using NIS/NIS+ you should\n" \
                "add the service at your NIS/NIS+ server and not to the local >/etc/services<\n" \
                "file.\n\n" \
                "Please add an entry in the form\n\n" \
                "   sge_qmaster <port_number>/tcp\n\n" \
                "to your services database and make sure to use an unused port number.\n"

      $INFOTEXT -wait -auto $AUTO -n "Please add the service now or press <RETURN> to go to entering a port number >> "

      # Check if $SGE_SERVICE service is available now
      service_available=false
      done=false
      while [ $done = false ]; do
         CheckServiceAndPorts service $SGE_QMASTER_SRV
         if [ $ret != 0 ]; then
            $CLEAR
            $INFOTEXT -u "\nNo TCP/IP service >sge_qmaster< yet"
            $INFOTEXT -n "\nIf you have just added the service it may take a while until the service\n" \
                         "propagates in your network. If this is true we can again check for\n" \
                         "the service >sge_qmaster<. If you don't want to add this service or if\n" \
                         "you want to install Grid Engine just for testing purposes you can enter\n" \
                         "a port number.\n"

              if [ $AUTO != "true" ]; then
                 $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                      "Check again (enter [n] to specify a port number) (y/n) [y] >> "
                 ret=$?
              else
                 $INFOTEXT -log "Setting SGE_QMASTER_PORT"
                 ret=1
              fi

            if [ $ret = 0 ]; then
               :
            else
               if [ $AUTO = true ]; then
                  $INFOTEXT -log "Please use an unused port number!"
                  exit 1
               fi

               $INFOTEXT -n "Please enter an unused port number >> "
               INP=`Enter $SGE_QMASTER_PORT`

               chars=`echo $INP | wc -c`
               chars=`expr $chars - 1`
               digits=`expr $INP : "[0-9][0-9]*"`
               if [ "$chars" != "$digits" ]; then
                  $INFOTEXT "\nInvalid input. Must be a number."
               elif [ $INP -le 1 -o $INP -ge $comm_port_max ]; then
                  $INFOTEXT "\nInvalid port number. Must be in range [1..%s]." $comm_port_max
               elif [ $INP -le 1024 -a $euid != 0 ]; then
                  $INFOTEXT "\nYou are not user >root<. You need to use a port above 1024."
               else
                  CheckServiceAndPorts port ${INP}

                  if [ $ret = 0 ]; then
                     $INFOTEXT "\nFound service with port number >%s< in >/etc/services<. Choose again." "$INP"
                  else
                     done=true
                  fi
               fi
               if [ $done = false ]; then
                  $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
               fi
            fi
            export SGE_QMASTER_PORT
         else
            done=true
            service_available=true
         fi
      done

      if [ $service_available = false ]; then
         SGE_QMASTER_PORT=$INP
         export SGE_QMASTER_PORT
         $INFOTEXT "\nUsing port >%s< for sge_qmaster daemon.\n" $SGE_QMASTER_PORT
         $INFOTEXT -log "Using port >%s< for sge_qmaster daemon." $SGE_QMASTER_PORT
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
      else
         unset SGE_QMASTER_PORT
         $INFOTEXT "\nService >sge_qmaster< is now available.\n"
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
      fi
   else
      $INFOTEXT "\nUsing the service\n\n" \
                "   sge_qmaster\n\n" \
                "for communication with Grid Engine.\n"
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
   fi

}

########################################################
#
# Version to convert a version string in X.Y.Z-* or
# X.Y.X_NN format to XYZNN format so can be treated as a
# number.
#
# $1 = version string
# Returns numerical version
#
########################################################
JavaVersionString2Num () {

   # Minor and micro default to 0 if not specified.
   major=`echo $1 | awk -F. '{print $1}'`
   minor=`echo $1 | awk -F. '{print $2}'`
   if [ ! -n "$minor" ]; then
      minor="0"
   fi
   micro=`echo $1 | awk -F. '{print $3}'`
   if [ ! -n "$micro" ]; then
      micro="0"
   fi

   # The micro version may further be extended to include a patch number.
   # This is typically of the form <micro>_NN, where NN is the 2-digit
   # patch number.  However it can also be of the form <micro>-XX, where
   # XX is some arbitrary non-digit sequence (eg., "rc").  This latter
   # form is typically used for internal-only release candidates or
   # development builds.
   #
   # For these internal builds, we drop the -XX and assume a patch number 
   # of "00".  Otherwise, we extract that patch number.
   #
   patch="00"
   dash=`echo $micro | grep "-"`
   if [ $? -eq 0 ]; then
      # Must be internal build, so drop the trailing variant.
      micro=`echo $micro | awk -F- '{print $1}'`
   fi

   underscore=`echo $micro | grep "_"`
   if [ $? -eq 0 ]; then
      # Extract the seperate micro and patch numbers, ignoring anything
      # after the 2-digit patch.
      patch=`echo $micro | awk -F_ '{print substr($2, 1, 2)}'`
      micro=`echo $micro | awk -F_ '{print $1}'`
   fi

   echo "${major}${minor}${micro}${patch}"

} # versionString2Num


#---------------------------------------------------------------------------
#  SetLibJvmPath
#
#     sets the env variable SGE_JVM_LIB_PATH
SetLibJvmPath() {
   
   MIN_JAVA_VERSION=1.5.0
   NUM_MIN_JAVA_VERSION=`JavaVersionString2Num $MIN_JAVA_VERSION`
   
   if [ "$JAVA_HOME" != "" ]; then
      java_home=$JAVA_HOME
   else
      java_home=/usr/java
   fi
   
   # set JRE_HOME 
   isdone=false
   while [ $isdone != true ]; do
      $INFOTEXT -n "Please enter JAVA_HOME or press enter [%s] >> " "$java_home"
      INP=`Enter $java_home`
      if [ "$INP" = "" -a ! -x $INP/bin/java ]; then
         $INFOTEXT "\nInvalid input. Must be a valid JAVA_HOME path."
         continue
      fi
      if [ "$INP" = "" -o ! -x $INP/bin/java ]; then
         $INFOTEXT "\nInvalid input. Must be a valid JAVA_HOME path."
      else
         java_home=$INP
         isdone=true
      fi
   done
   
   if [ -d $java_home/jre ]; then
      java_home=$java_home/jre
   fi

   JAVA_VERSION=`$java_home/bin/java -version 2>&1 | head -1`
   JAVA_VERSION=`echo $JAVA_VERSION | awk '{print $3}' | sed -e "s/\"//g"`
   NUM_JAVA_VERSION=`JavaVersionString2Num $JAVA_VERSION`
   
   if [ $NUM_JAVA_VERSION -lt $NUM_MIN_JAVA_VERSION ]; then
      $INFOTEXT "Warning: Cannot start jvm thread: Invalid java version (%s)), we need %s or higher" $JAVA_VERSION $MIN_JAVA_VERSION
      return 1
   fi
   
   case $ARCH in
      sol-sparc64) 
         jvm_lib_path=$java_home/lib/sparcv9/server/libjvm.so
         ;;
      sol-amd64)   
         jvm_lib_path=$java_home/lib/amd64/server/libjvm.so
         ;;
      sol-x86)     
         jvm_lib_path=$java_home/lib/i386/server/libjvm.so
         ;;
      lx*-amd64)   
         jvm_lib_path=$java_home/lib/amd64/server/libjvm.so
         ;;
      lx*-x86)     
         jvm_lib_path=$java_home/lib/i386/server/libjvm.so
         ;;
      darwin-ppc)
         jvm_lib_path=$java_home/../Libraries/libjvm.dylib
         ;;
      darwin-x86)  
         jvm_lib_path=$java_home/../Libraries/libjvm.dylib
         ;;
      *) 
         $INFOTEXT "Warning: Cannot start jvm thread: Have no java support for $ARCH"
         return 1
         ;;
   esac
   
   if [ ! -f "$jvm_lib_path" ]; then
      jvm_lib_path=""
      $INFOTEXT "\nWarning: Cannot start jvm thread: jvm library %s not found" "$jvm_lib_path"
      return 1
   fi
   export jvm_lib_path
   return 0
}

#---------------------------------------------------------------------------
#  GetJMXPort
#
#     sets the env variable SGE_LIBJVM_PATH, SGE_ADDITIONAL_JVM_ARGS, SGE_JMX_PORT
#
GetJMXPort() {

   $INFOTEXT -u "\nGrid Engine JMX MBean server"

   jmx_port_min=1
   jmx_port_max=65500

   if [ $AUTO = "true" ]; then

      if [ "$SGE_ENABLE_JMX" = "false" ]; then
            $INFOTEXT -log "\nJMX MBean server in qmaster disabled"
      else       
         if [ ! -f "$SGE_JVM_LIB_PATH" ]; then
            $INFOTEXT -log "\nWarning: Cannot start jvm thread: jvm library %s not found" "$SGE_JVM_LIB_PATH"
            MoveLog
            exit 1
         else   
            $INFOTEXT -log "\nUsing jvm library >%s<" "$SGE_JVM_LIB_PATH"
         fi

         if [ "$SGE_JMX_PORT" != "" ]; then
            if [ $SGE_JMX_PORT -ge $jmx_port_min -a $SGE_JMX_PORT -le $jmx_port_max ]; then
               $INFOTEXT -log "Using SGE_JMX_PORT >%s<." $SGE_JMX_PORT
            else
               $INFOTEXT -log "Your \$SGE_JMX_PORT=%s\n\n" \
                         "has an invalid value (it must be in range %s..%s).\n\n" \
                         "Please check your configuration file and restart\n" \
                         "the installation or configure the service >sge_qmaster<." $SGE_JMX_PORT $jmx_port_min $jmx_port_max
               MoveLog
               exit 1
            fi
         fi
      fi 

   else

      sge_jvm_lib_path=""
      sge_jmx_port=""
      sge_additional_jvm_args=""
      enable_jmx=false
      alldone=false
      while [ $alldone = false ]; do
         $INFOTEXT -ask "y" "n" -def "n" -n \
              "\nEnable the JMX MBean server in qmaster (y/n) [n] >> "
         ret=$?
         if [ $ret -eq 0 ]; then
            enable_jmx=true
         else   
            enable_jmx=false
         fi   

         if [ "$enable_jmx" = "false" ]; then
            $INFOTEXT "\nJMX MBean server in qmaster disabled"
            $INFOTEXT -wait -n "Hit <RETURN> to continue >> "
            $CLEAR
            return
         fi

         $INFOTEXT -e "Please give some basic parameters for JMX MBean server\n" \
         "We will ask for\n" \
         "   - JAVA_HOME\n" \
         "   - additional JVM arguments (optional)\n" \
         "   - JMX MBean server port\n"
         
         # set sge_jvm_lib_path
         SetLibJvmPath
         sge_jvm_lib_path=$jvm_lib_path

         # set SGE_ADDITIONAL_JVM_ARGS
         $INFOTEXT -n "Please enter additional JVM arguments (optional) >> "
         INP=`Enter "$sge_additional_jvm_args"`
         sge_additional_jvm_args="$INP"

         done=false
         while [ $done != true ]; do
            $INFOTEXT -n "Please enter an unused port number for the JMX MBean server >> "
            INP=`Enter $sge_jmx_port`
            if [ "$INP" = "" ]; then
               $INFOTEXT "\nInvalid input. Must be a number."
               continue
            fi
            chars=`echo $INP | wc -c`
            chars=`expr $chars - 1`
            digits=`expr $INP : "[0-9][0-9]*"`
            if [ "$chars" != "$digits" ]; then
               $INFOTEXT "\nInvalid input. Must be a number."
            elif [ $INP -lt $jmx_port_min -o $INP -gt $jmx_port_max ]; then
               $INFOTEXT "\nInvalid port number. Must be in range [%s..%s]." $jmx_port_min $jmx_port_max
            elif [ $INP -le 1024 -a $euid != 0 ]; then
               $INFOTEXT "\nYou are not user >root<. You need to use a port above 1024."
            else
               done=true
            fi
         done
         sge_jmx_port=$INP

         # show all parameters and redo if needed
         $INFOTEXT "\nUsing the following JMX MBean server settings."
         $INFOTEXT "   libjvm_path              >%s<" "$sge_jvm_lib_path"
         $INFOTEXT "   Additional JVM arguments >%s<" "$sge_additional_jvm_args"
         $INFOTEXT "   JMX port                 >%s<\n" "$sge_jmx_port"

         $INFOTEXT -ask "y" "n" -def "y" -n \
            "Do you want to use these data (y/n) [y] >> "
         if [ $? = 0 ]; then
            alldone=true
            SGE_ENABLE_JMX=$enable_jmx
            SGE_JVM_LIB_PATH=$sge_jvm_lib_path
            SGE_ADDITIONAL_JVM_ARGS=$sge_additional_jvm_args
            if [ "$ARCH" = "sol-amd64" -o "$ARCH" = "sol-sparc64" ]; then
               SGE_ADDITIONAL_JVM_ARGS="-d64 $SGE_ADDITIONAL_JVM_ARGS"
            fi   
            SGE_JMX_PORT=$sge_jmx_port
            export SGE_JVM_LIB_PATH SGE_JMX_PORT SGE_ADDITIONAL_JVM_ARGS SGE_ENABLE_JMX
         else
            $CLEAR
         fi
      done
   fi

   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
   $CLEAR
 
}

#-------------------------------------------------------------------------
# GetExecdPort: get communication port SGE_EXECD_PORT
#
GetExecdPort()
{

    if [ $RESPORT = true ]; then
       comm_port_max=1023
    else
       comm_port_max=65500
    fi
    CheckServiceAndPorts service $SGE_EXECD_SRV

    if [ "$SGE_EXECD_PORT" != "" ]; then
      $INFOTEXT -u "\nGrid Engine TCP/IP communication service"

      if [ $SGE_EXECD_PORT -ge 1 -a $SGE_EXECD_PORT -le $comm_port_max ]; then
         $INFOTEXT "\nUsing the environment variable\n\n" \
                   "   \$SGE_EXECD_PORT=%s\n\n" \
                     "as port for communication.\n\n" $SGE_EXECD_PORT
                      export SGE_EXECD_PORT
                      $INFOTEXT -log "Using SGE_EXECD_PORT >%s<." $SGE_EXECD_PORT
         if [ $ret = 0 ]; then
            $INFOTEXT "This overrides the preset TCP/IP service >sge_execd<.\n"
         fi
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
         return
      else
         $INFOTEXT "\nThe environment variable\n\n" \
                   "   \$SGE_EXECD_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please set the environment variable \$SGE_EXECD_PORT and restart\n" \
                   "the installation or configure the service >sge_execd<." $SGE_EXECD_PORT $comm_port_max
         $INFOTEXT -log "Your \$SGE_EXECD_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please check your configuration file and restart\n" \
                   "the installation or configure the service >sge_execd<." $SGE_EXECD_PORT $comm_port_max
      fi
   fi         
      $INFOTEXT -u "\nGrid Engine TCP/IP service >sge_execd<"
   if [ $ret != 0 ]; then
      $INFOTEXT "\nThere is no service >sge_execd< available in your >/etc/services< file\n" \
                "or in your NIS/NIS+ database.\n\n" \
                "You may add this service now to your services database or choose a port number.\n" \
                "It is recommended to add the service now. If you are using NIS/NIS+ you should\n" \
                "add the service at your NIS/NIS+ server and not to the local >/etc/services<\n" \
                "file.\n\n" \
                "Please add an entry in the form\n\n" \
                "   sge_execd <port_number>/tcp\n\n" \
                "to your services database and make sure to use an unused port number.\n"

         $INFOTEXT "Make sure to use a different port number for the Executionhost\n" \
                   "as on the qmaster machine\n"
         $INFOTEXT "The qmaster port SGE_QMASTER_PORT = %s\n" $SGE_QMASTER_PORT

      $INFOTEXT -wait -auto $AUTO -n "Please add the service now or press <RETURN> to go to entering a port number >> "

      # Check if $SGE_SERVICE service is available now
      service_available=false
      done=false
      while [ $done = false ]; do
         CheckServiceAndPorts service $SGE_EXECD_SRV
         if [ $ret != 0 ]; then
            $CLEAR
            $INFOTEXT -u "\nNo TCP/IP service >sge_execd< yet"
            $INFOTEXT -n "\nIf you have just added the service it may take a while until the service\n" \
                         "propagates in your network. If this is true we can again check for\n" \
                         "the service >sge_execd<. If you don't want to add this service or if\n" \
                         "you want to install Grid Engine just for testing purposes you can enter\n" \
                         "a port number.\n"

              if [ $AUTO != "true" ]; then
                 $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                      "Check again (enter [n] to specify a port number) (y/n) [y] >> "
                 ret=$?
              else
                 $INFOTEXT -log "Setting SGE_EXECD_PORT"
                 ret=1 
              fi

            if [ $ret = 0 ]; then
               :
            else
               $INFOTEXT -n "Please enter an unused port number >> "
               INP=`Enter $SGE_EXECD_PORT`

               if [ $AUTO = true ]; then
                  $INFOTEXT -log "Please use an unused port number!"
                  exit 1
               fi


               if [ $INP = $SGE_QMASTER_PORT ]; then
                  $INFOTEXT "Please use any other port number!!!"
                  $INFOTEXT "This %s port number is used by sge_qmaster" $SGE_QMASTER_PORT
                  if [ $AUTO = "true" ]; then
                     $INFOTEXT -log "Please use any other port number!!!"
                     $INFOTEXT -log "This %s port number is used by sge_qmaster" $SGE_QMASTER_PORT
                     $INFOTEXT -log "Installation failed!!!"

                     exit 1
                  fi
               fi
               chars=`echo $INP | wc -c`
               chars=`expr $chars - 1`
               digits=`expr $INP : "[0-9][0-9]*"`
               if [ "$chars" != "$digits" ]; then
                  $INFOTEXT "\nInvalid input. Must be a number."
               elif [ $INP -le 1 -o $INP -ge $comm_port_max ]; then
                  $INFOTEXT "\nInvalid port number. Must be in range [1..%s]." $comm_port_max
               elif [ $INP -le 1024 -a $euid != 0 ]; then
                  $INFOTEXT "\nYou are not user >root<. You need to use a port above 1024."
               else
                  #ser=`awk '{ print $2 }' /etc/services | grep "^${INP}/tcp"`
                  #cat /etc/services | grep -v "^#" | grep ${INP}
                  CheckServiceAndPorts port ${INP}
                  if [ $ret = 0 ]; then
                     $INFOTEXT "\nFound service with port number >%s< in >/etc/services<. Choose again." "$INP"
                  else
                     done=true
                  fi
                  if [ $INP = $SGE_QMASTER_PORT ]; then
                     done=false
                  fi
               fi
               if [ $done = false ]; then
                  $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
               fi
            fi
         else
            done=true
            service_available=true
         fi
      done

      if [ $service_available = false ]; then
         SGE_EXECD_PORT=$INP
         export SGE_EXECD_PORT
         $INFOTEXT "\nUsing port >%s< for sge_execd daemon.\n" $SGE_EXECD_PORT
         $INFOTEXT -log "Using port >%s< for sge_execd daemon." $SGE_EXECD_PORT
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
      else
         unset SGE_EXECD_PORT
         $INFOTEXT "\nService >sge_execd< is now available.\n"
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
      fi
   else
      $INFOTEXT "\nUsing the service\n\n" \
                "   sge_execd\n\n" \
                "for communication with Grid Engine.\n"
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
   fi

   export SGE_EXECD_PORT
}


#-------------------------------------------------------------------------
# GetDefaultDomain
#
GetDefaultDomain()
{
   done=false

   if [ $AUTO = "true" ]; then
      $INFOTEXT -log "Using >%s< as default domain." $DEFAULT_DOMAIN
      CFG_DEFAULT_DOMAIN=$DEFAULT_DOMAIN
      done=true
   fi

   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nDefault domain for hostnames"

      $INFOTEXT "\nSometimes the primary hostname of machines returns the short hostname\n" \
                  "without a domain suffix like >foo.com<.\n\n" \
                  "This can cause problems with getting load values of your execution hosts.\n" \
                  "If you are using DNS or you are using domains in your >/etc/hosts< file or\n" \
                  "your NIS configuration it is usually safe to define a default domain\n" \
                  "because it is only used if your execution hosts return the short hostname\n" \
                  "as their primary name.\n\n" \
                  "If your execution hosts reside in more than one domain, the default domain\n" \
                  "parameter must be set on all execution hosts individually.\n"

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                "Do you want to configure a default domain (y/n) [y] >> "
      if [ $? = 0 ]; then
         $INFOTEXT -n "\nPlease enter your default domain >> "
         CFG_DEFAULT_DOMAIN=`Enter ""`
         if [ "$CFG_DEFAULT_DOMAIN" != "" ]; then
            $INFOTEXT -wait -auto $AUTO -n "\nUsing >%s< as default domain. Hit <RETURN> to continue >> " \
                      $CFG_DEFAULT_DOMAIN
            $CLEAR
            done=true
         fi
      else
         CFG_DEFAULT_DOMAIN=none
         done=true
      fi
   done
}

SetScheddConfig()
{
   $CLEAR

   $INFOTEXT -u "Scheduler Tuning"
   $INFOTEXT -n "\nThe details on the different options are described in the manual. \n"
   done="false"
 
   while [ $done = "false" ]; do
      $INFOTEXT -u "Configurations"
      $INFOTEXT -n "1) Normal\n          Fixed interval scheduling, report scheduling information,\n" \
                   "          actual + assumed load\n"
      $INFOTEXT -n "2) High\n          Fixed interval scheduling, report limited scheduling information,\n" \
                   "          actual load\n"
      $INFOTEXT -n "3) Max\n          Immediate Scheduling, report no scheduling information,\n" \
                   "          actual load\n"

      $INFOTEXT -auto $AUTO -n "Enter the number of your prefered configuration and hit <RETURN>! \n" \
                   "Default configuration is [1] >> "
      SCHEDD=`Enter 1`

      if [ $AUTO = "false" ]; then
         SCHEDD_CONF=$SCHEDD
      fi

      if [ $SCHEDD_CONF = "1" ]; then
         is_selected="Normal"
      elif [ $SCHEDD_CONF = "2" ]; then
         is_selected="High"
      elif [ $SCHEDD_CONF = "3" ]; then
         is_selected="Max"
      else
         SCHEDD_CONF=1
         is_selected="Normal"
      fi

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "\nWe're configuring the scheduler with >%s< settings!\n Do you agree? (y/n) [y] >> " $is_selected
      if [ $? = 0 ]; then
         done="true"
      fi
   done

   if [ $AUTO = "true" ]; then
      $INFOTEXT -log "Setting scheduler configuration to >%s< setting! " $is_selected
   fi

   case $SCHEDD_CONF in

   1)
    $SGE_BIN/qconf -Msconf ./util/install_modules/inst_schedd_normal.conf
    ;;

   2)
    $SGE_BIN/qconf -Msconf ./util/install_modules/inst_schedd_high.conf
    ;;

   3)
    $SGE_BIN/qconf -Msconf ./util/install_modules/inst_schedd_max.conf
    ;;
   esac
   $CLEAR
}


GiveBerkelyHints()
{
  $INFOTEXT "If you are using a Berkely DB Server, please add the bdb_checkpoint.sh\n" \
            "script to your crontab. This script is used for transaction\n" \
            "checkpointing and cleanup in SGE installations with a\n" \
            "Berkeley DB RPC Server. You will find this script in:\n" \
            "$SGE_ROOT/util/\n\n" \
            "It must be added to the crontab of the user (%s), who runs the\n" \
            "berkeley_db_svc on the server host. \n\n" \
            "e.g. * * * * * <full path to scripts> <sge-root dir> <sge-cell> <bdb-dir>\n" $ADMINUSER
}


WindowsSupport()
{
   $CLEAR
   $INFOTEXT -u "Windows Execution Host Support"
   $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n "\nAre you going to install Windows Execution Hosts? (y/n) [n] >> "

   if [ $? = 0 ]; then
      WINDOWS_SUPPORT=true
      WindowsDomainUserAccess
   fi
}


WindowsDomainUserAccess()
{
   $CLEAR
   #The windows domain user access handling has changed -> WIN_DOMAIN_ACCESS always has to be true
   #$INFOTEXT -u "Windows Domain User Access"
   #$INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "\nDo you want to use Windows Domain Users (answer: y)\n" \
   #                                               "or are you going to use local Windows Users (answer: n) (y/n) [y] >> "
   #if [ $? = 0 ]; then
      WIN_DOMAIN_ACCESS=true
   #fi
}


AddWindowsAdmin()
{
   if [ "$WINDOWS_SUPPORT" = "true" ]; then
      $INFOTEXT -u "Windows Administrator Name"
      $INFOTEXT "\nFor a later execution host installation it is recommended to add the\n" \
                "Windows Administrator name to the SGE manager list\n"
      $INFOTEXT -n "Please, enter the Windows Administrator name [Default: %s] >> " $WIN_ADMIN_NAME

      WIN_ADMIN_NAME=`Enter $WIN_ADMIN_NAME`

      $SGE_BIN/qconf -am $WIN_ADMIN_NAME
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
   fi
}
