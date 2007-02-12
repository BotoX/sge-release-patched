#ifndef __MSG_SGEOBJLIB_H
#define __MSG_SGEOBJLIB_H
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

#define MSG_ANSWER_NOANSWERLIST     _MESSAGE(64000, _("no answer list - gdi request failed"))
#define MSG_CKPTREFINJOB_SU         _MESSAGE(64001, _("Checkpointing object "SFQ" is still referenced in job " sge_U32CFormat"."))
#define MSG_GDI_INVALIDPRODUCTMODESTRING_S   _MESSAGE(64005, _("invalid security mode string "SFQ))
#define MSG_GDI_SWITCHFROMTO_SS              _MESSAGE(64007, _("switching from "SFQ" to "SFQ" feature set"))
#define MSG_HOSTREFINQUEUE_SS                _MESSAGE(64008, _("Host object "SFQ" is still referenced in cluster queue "SFQ"."))
#define MSG_HOSTREFINHGRP_SS                             _MESSAGE(64011, _("Host object "SFQ" is still referenced in host group "SFQ"."))
#define MSG_GDI_CONFIGNOARGUMENTGIVEN_S                  _MESSAGE(64012, _("no argument given in config option: "SFN))
#define MSG_GDI_CONFIGMISSINGARGUMENT_S                  _MESSAGE(64013, _("missing configuration attribute "SFQ))
#define MSG_GDI_CONFIGADDLISTFAILED_S                    _MESSAGE(64014, _("can't add "SFQ" to list"))
#define MSG_GDI_CONFIGARGUMENTNOTDOUBLE_SS               _MESSAGE(64016, _("value for attribute "SFN" "SFQ" is not a double"))
#define MSG_GDI_CONFIGARGUMENTNOTTIME_SS                 _MESSAGE(64017, _("value for attribute "SFN" "SFQ" is not time"))
#define MSG_GDI_CONFIGARGUMENTNOMEMORY_SS                _MESSAGE(64018, _("value for attribute "SFN" "SFQ" is not memory"))
#define MSG_GDI_CONFIGINVALIDQUEUESPECIFIED              _MESSAGE(64019, _("reading conf file: invalid queue type specified"))
#define MSG_GDI_CONFIGREADFILEERRORNEAR_SS               _MESSAGE(64020, _("reading conf file: "SFN" error near "SFQ))
#define MSG_GDI_READCONFIGFILESPECGIVENTWICE_SS          _MESSAGE(64021, _("reading config file: specifier "SFQ" given twice for "SFQ))
#define MSG_GDI_READCONFIGFILEUNKNOWNSPEC_SS             _MESSAGE(64022, _("reading conf file: unknown specifier "SFQ" for "SFN))
#define MSG_GDI_READCONFIGFILEEMPTYENUMERATION_S         _MESSAGE(64023, _("reading conf file: empty enumeration for "SFQ))
#define MSG_JOB_XISINVALIDJOBTASKID_S                    _MESSAGE(64024, _("ERROR! "SFQ" is an invalid job-task identifier"))
#define MSG_JOB_LONELY_TOPTION_S                          _MESSAGE(64025, _("found lonely '-t "SFN"' option (The -t option needs a leading job name)."))


#define MSG_JOB_JLPPNULL                  _MESSAGE(64028, _("jlpp == NULL in job_add_job()"))                                                        
#define MSG_JOB_JEPNULL                   _MESSAGE(64029, _("jep == NULL in job_add_job()"))
#define MSG_JOB_JOBALREADYEXISTS_S        _MESSAGE(64030, _("can't add job "SFN" - job already exists") )
#define MSG_JOB_NULLNOTALLOWEDT           _MESSAGE(64031, _("job rejected: 0 is an invalid task id"))
#define MSG_JOB_NOIDNOTALLOWED            _MESSAGE(64032, _("job rejected: Job comprises no tasks in its id lists") )
#define MSG_JOB_JOB_ID_U                  _MESSAGE(64033, _(sge_U32CFormat))
#define MSG_JOB_JOB_JATASK_ID_UU          _MESSAGE(64034, _(sge_U32CFormat"."sge_U32CFormat))
#define MSG_JOB_JOB_JATASK_PETASK_ID_UUS  _MESSAGE(64035, _(sge_U32CFormat"."sge_U32CFormat" task "SFN))
#define MSG_JOB_NODISPLAY_S               _MESSAGE(64036, _("no DISPLAY variable found with interactive job "SFN))
#define MSG_JOB_EMPTYDISPLAY_S            _MESSAGE(64037, _("empty DISPLAY variable delivered with interactive job "SFN))
#define MSG_JOB_LOCALDISPLAY_SS           _MESSAGE(64038, _("local DISPLAY variable "SFQ" delivered with interactive job "SFN))
#define MSG_ERRORPARSINGVALUEFORNM_S  _MESSAGE(64040, _("error parsing value "SFQ))
#define MSG_PARSE_STARTTIMETOOLONG    _MESSAGE(64041, _("Starttime specifier field length exceeds maximum"))
#define MSG_PARSE_INVALIDSECONDS      _MESSAGE(64042, _("Invalid format of seconds field."))
#define MSG_PARSE_INVALIDHOURMIN      _MESSAGE(64043, _("Invalid format of date/hour-minute field."))
#define MSG_PARSE_INVALIDMONTH        _MESSAGE(64044, _("Invalid month specifica tion."))
#define MSG_PARSE_INVALIDDAY          _MESSAGE(64045, _("Invalid day specificati on."))
#define MSG_PARSE_INVALIDHOUR         _MESSAGE(64046, _("Invalid hour specification."))
#define MSG_PARSE_INVALIDMINUTE       _MESSAGE(64047, _("Invalid minute specification."))
#define MSG_PARSE_INVALIDSECOND       _MESSAGE(64048, _("Invalid seconds specification."))
#define MSG_PARSE_NODATEFROMINPUT     _MESSAGE(64049, _("Couldn't generate date from input. Perhaps a date before 1970 was specified."))
#define MSG_PARSE_NODATE                                _MESSAGE(64050, _("no date specified"))
#define MSG_PEREFINJOB_SU                               _MESSAGE(64059, _("Pe "SFQ" is still referenced in job "sge_U32CFormat"."))
#define MSG_GDI_INITIALPORTIONSTRINGNODECIMAL_S          _MESSAGE(64060, _("Numerical value invalid!\nThe initial portion of string "SFQ" contains no decimal number"))
#define MSG_GDI_RANGESPECIFIERWITHUNKNOWNTRAILER_SS      _MESSAGE(64061, _("Range specifier "SFQ" has unknown trailer "SFQ))
#define MSG_GDI_UNEXPECTEDRANGEFOLLOWINGUNDEFINED        _MESSAGE(64062, _("unexpected range following \"UNDEFINED\""))
#define MSG_GDI_UNEXPECTEDUNDEFINEDFOLLOWINGRANGE        _MESSAGE(64063, _("unexpected \"UNDEFINED\" following range"))
#define MSG_GDI_NEGATIVSTEP                              _MESSAGE(64064, _("Negative or zero step in range is not allowed" ))
#define MSG_PARSE_NOALLOCATTRLIST                        _MESSAGE(64065, _("unable to alloc space for attrib. list"))
#define MSG_PARSE_NOALLOCATTRELEM                        _MESSAGE(64068, _("unable to alloc space for attrib. element"))
#define MSG_NONE_NOT_ALLOWED_S                           _MESSAGE(64079, _("The keyword \"none\" is not allowed in "SFQ))
#define MSG_NOTEXISTING_ATTRIBUTE_SS                     _MESSAGE(64080, _(SFQ" references not existing complex attribute "SFQ))
#define MSG_WRONGTYPE_ATTRIBUTE_SS                       _MESSAGE(64081, _("String, CString, ReString or Host attributes are not allowed in "SFQ": " SFQ))
#define MSG_US_INVALIDUSERNAME                           _MESSAGE(64083, _("userset contains invalid (null) user name"))

/*
 * sge_event.c
 */
#define MSG_EVENT_ADDOBJECTX_USS                   _MESSAGE(64090, _(sge_U32CFormat". EVENT ADD "SFN" "SFN))
#define MSG_EVENT_DELOBJECTX_USS                   _MESSAGE(64091, _(sge_U32CFormat". EVENT DEL "SFN" "SFN))
#define MSG_EVENT_MODOBJECTX_USS                   _MESSAGE(64092, _(sge_U32CFormat". EVENT MOD "SFN" "SFN))
#define MSG_EVENT_OBJECTLISTXELEMENTS_USI          _MESSAGE(64093, _(sge_U32CFormat". EVENT "SFN" LIST %d Elements"))
#define MSG_EVENT_MESSAGE_US                       _MESSAGE(64094, _(sge_U32CFormat". EVENT "SFN))

#define MSG_EVENT_MODSCHEDDPRIOOFJOBXTOY_USI       _MESSAGE(64100, _(sge_U32CFormat". EVENT MODIFY SCHEDULING PRIORITY OF JOB "SFN" TO %d"))
#define MSG_EVENT_JOBXUSAGE_US                     _MESSAGE(64101, _(sge_U32CFormat". EVENT JOB "SFN" USAGE"))
#define MSG_EVENT_JOBXFINALUSAGE_US                _MESSAGE(64102, _(sge_U32CFormat". EVENT JOB "SFN" FINAL USAGE"))
#define MSG_EVENT_UNSUSPENDQUEUEXONSUBORDINATE_US  _MESSAGE(64103, _(sge_U32CFormat". EVENT UNSUSPEND QUEUE "SFN" ON SUBORDINATE"))
#define MSG_EVENT_SUSPENDQUEUEXONSUBORDINATE_US    _MESSAGE(64104, _(sge_U32CFormat". EVENT SUSPEND QUEUE "SFN" ON SUBORDINATE"))
#define MSG_EVENT_JOBXFINISH_US                    _MESSAGE(64105, _(sge_U32CFormat". EVENT JOB "SFN" FINISH"))
#define MSG_EVENT_DELQINSTANCE_USS                 _MESSAGE(64106, _(sge_U32CFormat". EVENT DEL QUEUE INSTANCE "SFN"@"SFN))
#define MSG_EVENT_ADDQINSTANCE_USS                 _MESSAGE(64107, _(sge_U32CFormat". EVENT ADD QUEUE INSTANCE "SFN"@"SFN))
#define MSG_EVENT_MODQINSTANCE_USS                 _MESSAGE(64108, _(sge_U32CFormat". EVENT MOD QUEUE INSTANCE "SFN"@"SFN))
#define MSG_EVENT_SHARETREEXNODESYLEAFS_UII        _MESSAGE(64109, _(sge_U32CFormat". EVENT SHARETREE %d nodes %d leafs"))

#define MSG_OBJECT_INVALID_OBJECT_TYPE_SI          _MESSAGE(64167, _(SFN": invalid object type %d"))
#define MSG_HGRP_UNKNOWNHOST                       _MESSAGE(64168, _("unable to resolve host "SFQ))
#define MSG_CUSER_NOREMOTE_USER_S                  _MESSAGE(64169, _("attribute \'"SFQ"\' not available"))
#define MSG_SGETEXT_NO_INTERFACE_S                 _MESSAGE(64174, _("no valid checkpoint interface "SFN))
#define MSG_OBJ_CKPTENV_SSS                        _MESSAGE(64175, _("parameter "SFN" of ckpt environment "SFQ": "SFN))
#define MSG_CKPT_XISNOTASIGNALSTRING_S             _MESSAGE(64176, _(SFQ" is not a signal string (like HUP, INT, " "WINCH, ..)"))
#define MSG_PE_STARTPROCARGS_SS                    _MESSAGE(64177, _("parameter start_proc_args of pe "SFQ": "SFN))
#define MSG_PE_STOPPROCARGS_SS                     _MESSAGE(64178, _("parameter stop_proc_args of pe "SFQ": "SFN))
#define MSG_ANSWERWITHOUTDIAG                      _MESSAGE(64179, _("error without diagnosis message"))
#define MSG_PEREFDOESNOTEXIST_S                    _MESSAGE(64180, _("Pe "SFQ" does not exist"))
#define MSG_CKPTREFDOESNOTEXIST_S                  _MESSAGE(64181, _("Ckpt "SFQ" does not exist"))
#define MSG_PEREFINQUEUE_SS                        _MESSAGE(64182, _("Pe "SFQ" is still referenced in queue "SFQ"."))
#define MSG_CKPTREFINQUEUE_SS                      _MESSAGE(64183, _("Ckpt "SFQ" is still referenced in queue "SFQ"."))
#define MSG_INVALID_CENTRY_TYPE_S                  _MESSAGE(64184, _("Invalid complex attribute type ("SFQ")"))
#define MSG_INVALID_CENTRY_RELOP_S                 _MESSAGE(64185, _("Invalid complex attribute for relation operator ("SFQ")"))
#define MSG_INVALID_CENTRY_REQUESTABLE_S           _MESSAGE(64186, _("Invalid complex attribute for requestable ("SFQ")"))
#define MSG_CENTRYREFINQUEUE_SS                    _MESSAGE(64187, _("Complex attribute "SFQ" is still referenced in queue "SFQ"."))
#define MSG_CENTRYREFINHOST_SS                     _MESSAGE(64188, _("Complex attribute "SFQ" is still referenced in host "SFQ"."))
#define MSG_CENTRYREFINSCONF_S                     _MESSAGE(64189, _("Complex attribute "SFQ" is still referenced in scheduler configuration."))
#define MSG_PARSE_DUPLICATEHOSTINFILESPEC          _MESSAGE(64190,_("ERROR! two files are specified for the same host"))
#define MSG_INVALID_CENTRY_DEFAULT_S               _MESSAGE(64202, _("Complex "SFQ" cannot have a default value"))
#define MSG_INVALID_CENTRY_CONSUMABLE_RELOP_S      _MESSAGE(64203, _("Consumable "SFQ" can have only <= as an relational operator"))
#define MSG_INVALID_CENTRY_CONSUMABLE_TYPE_SS      _MESSAGE(64204, _("Complex "SFQ" of type "SFQ" cannot be a consumable"))
#define MSG_INVALID_CENTRY_TYPE_RELOP_S            _MESSAGE(64205, _("Complex "SFQ" has an invalid relational operator")) 
#define MSG_INVALID_CENTRY_PARSE_DEFAULT_SS        _MESSAGE(64206, _("Complex "SFQ"'s default value should be of type "SFQ))
#define MSG_INVALID_CENTRY_CONSUMABLE_REQ1_S       _MESSAGE(64207, _("Consumable "SFQ" needs a default value because it is not requestable."))
#define MSG_INVALID_CENTRY_CONSUMABLE_REQ2_S       _MESSAGE(64208, _("Consumable "SFQ" cannot have a default value because it is forced.")) 
#define MSG_INVALID_CENTRY_TYPE_CHANGE_S           _MESSAGE(64209, _("The type of the complex "SFQ" cannot be changed due to its built in status"))

#define MSG_CQUEUE_NONDEFNOTALLOWED    _MESSAGE(64210, _("Queue domain/instance values are only allowed for cluster queues."))
#define MSG_CQUEUE_PRIORITYNOTINRANGE  _MESSAGE(64211, _("Priority not in range -20 to +20"))
#define MSG_CQUEUE_UNKNOWNUSERSET_S    _MESSAGE(64213, _("Userset "SFQ" does not exist") )
#define MSG_CQUEUE_UNKNOWNPROJECT_S    _MESSAGE(64214, _("Project "SFQ" does not exist"))
#define MSG_CQUEUE_UNKNOWNCALENDAR_S   _MESSAGE(64215, _("Calendar "SFQ" does not exist"))
#define MSG_CQUEUE_UNKNOWNINITSTATE_S  _MESSAGE(64216, _("Invalid value "SFQ" as initial state"))
#define MSG_CQUEUE_UNKNOWNSTARTMODE_S  _MESSAGE(64217, _("Invalid value "SFQ" as shell start mode"))

#define MSG_ATTR_VALUEMULDEFINED_S     _MESSAGE(64218, _("Value for "SFQ" is multiply defined"))
#define MSG_ATTR_NONEWATTRSETTING_S    _MESSAGE(64219, _("Cannot create new attribute setting for host "SFQ))
#define MSG_ATTR_RESULTUNAMBIGUOUS_S   _MESSAGE(64220, _("Modification would result in ambiguous configuration"))
#define MSG_ATTR_NOCONFVALUE           _MESSAGE(64221, _("No default/hostgroup/host value found"))
#define MSG_ATTR_MISSINGCOMMA_S        _MESSAGE(64222, _("All list elements must be separated by commas: "SFQ))
#define MSG_ATTR_MISSINGBRACKET_S      _MESSAGE(64223, _("The last character of each list item must be enclosed in brackets: "SFQ))
#define MSG_ATTR_EQUALSIGNEXPRECTED    _MESSAGE(64224, _("\'=\' has to sepatate host or group from value"))
#define MSG_ATTR_TRAILINGCOMMA_S       _MESSAGE(64225, _("There is an extra comma at the end of the list: "SFQ))
#define MSG_ATTR_PARSINGERROR_S        _MESSAGE(64226, _("Error during parsing of attribute value "SFQ))
#define MSG_ATTR_NOVALUEGIVEN          _MESSAGE(64227, _("No value specified"))

#define MSG_CQUEUE_NODEFVALUE_S        _MESSAGE(64228, _(SFQ" has no default value"))
#define MSG_CQUEUE_MULVALNOTALLOWED_S  _MESSAGE(64229, _("Multiple values for one queue domain/host ("SFN") are not allowed"))
#define MSG_CQUEUE_WRONGCHARINPRIO     _MESSAGE(64231, _("Invalid character"))
#define MSG_QINSTANCE_ALARM            _MESSAGE(64233, _("load alarm"))
#define MSG_QINSTANCE_SUSPALARM        _MESSAGE(64234, _("suspend alarm"))
#define MSG_QINSTANCE_DISABLED         _MESSAGE(64235, _("disabled"))
#define MSG_QINSTANCE_SUSPENDED        _MESSAGE(64236, _("suspended"))
#define MSG_QINSTANCE_UNKNOWN          _MESSAGE(64237, _("unknown"))
#define MSG_QINSTANCE_ERROR            _MESSAGE(64238, _("error"))
#define MSG_QINSTANCE_SUSPOSUB         _MESSAGE(64239, _("suspended on subordinate"))
#define MSG_QINSTANCE_CALDIS           _MESSAGE(64240, _("calendar disabled"))
#define MSG_QINSTANCE_CALSUSP          _MESSAGE(64241, _("calendar suspended"))
#define MSG_QINSTANCE_CONFAMB          _MESSAGE(64242, _("configuration ambiguous"))
#define MSG_QINSTANCE_ORPHANED         _MESSAGE(64243, _("orphaned"))

#define MSG_QINSTANCE_NALARM           _MESSAGE(64244, _("no load alarm"))
#define MSG_QINSTANCE_NSUSPALARM       _MESSAGE(64245, _("no suspend alarm"))
#define MSG_QINSTANCE_NDISABLED        _MESSAGE(64246, _("enabled"))
#define MSG_QINSTANCE_NSUSPENDED       _MESSAGE(64247, _("unsuspended"))
#define MSG_QINSTANCE_NUNKNOWN         _MESSAGE(64248, _("not unknown"))
#define MSG_QINSTANCE_NERROR           _MESSAGE(64249, _("no error"))
#define MSG_QINSTANCE_NSUSPOSUB        _MESSAGE(64250, _("no suspended on subordinate"))
#define MSG_QINSTANCE_NCALDIS          _MESSAGE(64251, _("calendar enabled"))
#define MSG_QINSTANCE_NCALSUSP         _MESSAGE(64252, _("calendar unsuspended"))
#define MSG_QINSTANCE_NCONFAMB         _MESSAGE(64253, _("not configuration ambiguous"))
#define MSG_QINSTANCE_NORPHANED        _MESSAGE(64254, _("not orphaned"))
#define MSG_QINSTANCE_MISSLOTS_S       _MESSAGE(64255, _("missing \"slots\" entry in consumable actual list of queue "SFQ))
#define MSG_CALENDAR_REFINQUEUE_SS     _MESSAGE(64256, _("Calendar "SFQ" is still referenced in queue "SFQ))
#define MSG_STR_INVALIDSTR             _MESSAGE(64257, _("Encountered invalid id"))
#define MSG_QINSTANCE_INVALIDACTION    _MESSAGE(64258, _("Invalid action" ))
#define MSG_QINSTANCE_INVALIDOPTION    _MESSAGE(64259, _("Invalid option flag"))
#define MSG_ID_UNABLETOCREATE          _MESSAGE(64260, _("Unable to create id element"))
#define MSG_CQUEUE_SUBITSELF_S         _MESSAGE(64261, _("Cluster queue "SFQ" can't get subordinated by itself"))
#define MSG_CQUEUE_UNKNOWNSUB_SS       _MESSAGE(64262, _("Subordinated cluster queue "SFQ" referenced in "SFQ" does not exist"))
#define MSG_QREF_QUNKNOWN_S            _MESSAGE(64263, _("Job was rejected because job requests unknown queue "SFQ))
#define MSG_QREF_QNOTREQUESTABLE       _MESSAGE(64264, _("Job was rejected because job requests a queue while queues are not configured as requestable"))
#define MSG_ATTRIB_PARSATTRFAILED_SS         _MESSAGE(64265, _("failed parsing attribute "SFQ": "SFN))
#define MSG_OBJECT_NO_LIST_TO_MOD_TYPE_SI    _MESSAGE(64266, _("%s: has no master list to modify %d"))
#define MSG_INVALID_CENTRY_PARSE_URGENCY_SS  _MESSAGE(64267, _("The following error occoured for complex "SFQ", while parsing the urgency value: "SFQ))
#define MSG_ULONG_INCORRECTSTRING      _MESSAGE(64268, _("error parsing unsigned long value from string "SFQ))
#define MSG_INT_INCORRECTSTRING        _MESSAGE(64269, _("error parsing signed int value from string "SFQ))
#define MSG_CHAR_INCORRECTSTRING       _MESSAGE(64270, _("error parsing character value from string "SFQ))
#define MSG_LONG_INCORRECTSTRING       _MESSAGE(64271, _("error parsing signed long from string "SFQ))
#define MSG_DOUBLE_INCORRECTSTRING     _MESSAGE(64272, _("error parsing double value from string "SFQ))
#define MSG_FLOAT_INCORRECTSTRING      _MESSAGE(64273, _("error parsing float value from string "SFQ))
#define MSG_QTYPE_INCORRECTSTRING      _MESSAGE(64274, _("error parsing queue type from string "SFQ))
#define MSG_QSTATE_UNKNOWNCHAR_CS      _MESSAGE(64275, _("unknown queue state: %c found in string: %s"))
#define MSG_QSTAT_WRONGGCHAR_C         _MESSAGE(64276, _("wrong character \'%c\' for -g option"))
#define MSG_CQUEUE_UNKNOWNCENTRY_S     _MESSAGE(64277, _("Complex attribute "SFQ" does not exist"))
#define MSG_OBJECT_ALREADYEXIN_SSS     _MESSAGE(64278, _("No modification because "SFQ" already exists in "SFQ" of "SFQ))
#define MSG_OBJECT_VALUEMISSING        _MESSAGE(64279, _("Value missing"))
#define MSG_CONF_USING_SS              _MESSAGE(64300, _("using "SFQ" for "SFN))
#define MSG_CONF_NOCONFIGFROMMASTER    _MESSAGE(64301, _("could not get configuration from qmaster - using defaults"))
#define MSG_CONF_INVALIDPARAM_SSI      _MESSAGE(64302, _("invalid setting for "SFQ", attribute "SFQ" - using default %d"))

#define MSG_CENTRY_QINOTALLOWED        _MESSAGE(64303, _("Queue instance names are not allowed in -l resource requests"))
#define MSG_OBJECT_VALUENOTULONG_S     _MESSAGE(64304, _(SFQ" is not an u_long32 value"))
#define MSG_CQUEUE_INVALIDDOMSETTING_SS _MESSAGE(64305, _("Did not modify "SFQ" for host group "SFQ" which does not exist."))
#define MSG_HGRP_INVALIDHOSTGROUPNAME_S _MESSAGE(64306, _("host group name "SFQ" is not valid"))
#define MSG_WEIGHTFACTNONUMB_SS         _MESSAGE(64307, _(SFQ" uses "SFQ" as weighting factor (only numbers are allowed)"))
#define MSG_MULTIPLEWEIGHTFACT_S        _MESSAGE(64308, _(SFQ" may not use multiple weighting factors"))
#define MSG_CONF_NR_DYNAMIC_EVENT_CLIENT_EXCEEDS_MAX_FILEDESCR_U _MESSAGE(64309, _("nr of dynamic event clients exceeds max file descriptor limit, setting MAX_DYN_EC="sge_U32CFormat))

#define MSG_QINSTANCE_HOSTFORQUEUEDOESNOTEXIST_SS   _MESSAGE(64310, _("can't create queue "SFQ": host "SFQ" is not known"))
#define MSG_PE_INVALIDCHARACTERINPE_S   _MESSAGE(64311, _("Invalid character in pe name of pe "SFQ))
#define MSG_PE_UNKNOWN_URGENCY_SLOT_SS  _MESSAGE(64312, _("unknown urgency_slot_setting "SFQ" for PE "SFQ))

#ifdef SGE_PQS_API
#define MSG_PQS_NODYNAMICLIBRARY_S       _MESSAGE(64313, _("No dynamic library specified for pe_qsort_args for PE "SFQ))
#define MSG_PQS_UNABLETOOPENLIBRARY_SSS  _MESSAGE(64314, _("Unable to open "SFQ" library in pe_qsort_args for PE "SFQ" - "SFQ))
#define MSG_PQS_NOFUNCTIONNAME_S         _MESSAGE(64315, _("No function name specified in pe_qsort_args for PE "SFQ))
#define MSG_PQS_UNABLELOCATESYMBOL_SSSS  _MESSAGE(64316, _("Unable to locate "SFQ" symbol in "SFQ" library for pe_qsort_args in PE "SFQ" - "SFQ))
#endif

#define MSG_CQUEUE_CQUEUEISNULL_SSSII      _MESSAGE(64317, _("cqueue_list_locate_qinstance("SFQ"): cqueue == NULL("SFQ", "SFQ", %d, %d"))
#define MSG_CQUEUE_FULLNAMEISNULL        _MESSAGE(64318, _("cqueue_list_locate_qinstance(): full_name == NULL"))
#define MSG_PE_REJECTINGURGENCYSLOTS_S  _MESSAGE(64319, _("rejecting invalid urgency_slots setting "SFQ))
#define MSG_CALENDAR_CALCTERMINATED    _MESSAGE(64320, _("Calendar calculation terminated due to inf. loop!"))

#define MSG_OBJECT_STRUCTURE_ERROR     _MESSAGE(64321, _("corrupted cull structure or reduced element"))
#define MSG_OBJECT_ULONG_NOT_NULL            _MESSAGE(64322, _("object attribute "SFQ" may not be 0"))
#define MSG_OBJECT_ULONG_NULL            _MESSAGE(64323, _("object attribute "SFQ" may only be 0"))
#define MSG_OBJECT_DOUBLE_NULL            _MESSAGE(64324, _("object attribute "SFQ" may only be 0"))
#define MSG_OBJECT_STRING_NOT_NULL            _MESSAGE(64325, _("object attribute "SFQ" may not be NULL"))
#define MSG_OBJECT_VARIABLENAME_NOT_EMPTY            _MESSAGE(64326, _("variable names may not be empty"))

#define MSG_PATH_TOOLONG_I _MESSAGE(64330, _("a path or filename may not exceed %d characters"))
#define MSG_PATH_ALIAS_INVALID_PATH _MESSAGE(64331, _("paths may not be empty strings"))
#define MSG_HOSTNAME_NOT_EMPTY _MESSAGE(64332, _("hostnames may not be empty string"))

#define MSG_EVENT_INVALIDNAME _MESSAGE(64340, _("invalid event client name"))
#define MSG_EVENT_ONLYADMINMAYSTARTSPECIALEVC _MESSAGE(64341, _("only admin user or root may start special event clients"))
#define MSG_EVENT_INVALIDDTIME_II _MESSAGE(64342, _("invalid event delivery time %d. It has to be between 1 and %d"))
#define MSG_EVENT_FLUSHDELAYCANNOTBEGTDTIME _MESSAGE(64343, _("event flush delay may not be greater than event delivery time"))
#define MSG_EVENT_INVALIDBUSYHANDLING _MESSAGE(64344, _("invalid value for event client busy handling"))
#define MSG_EVENT_INVALIDSESSIONKEY _MESSAGE(64345, _("invalid session key for event client"))
#define MSG_EVENT_INVALIDEVENT _MESSAGE(64346, _("invalid event id in event client subscription"))
#define MSG_EVENT_INVALIDID _MESSAGE(64347, _("invalid event client id"))
#define MSG_EVENT_INVALIDUPDATEFUNCTION _MESSAGE(64348, _("invalid event client update function"))

#define MSG_JOB_SCRIPTLENGTHDOESNOTMATCH _MESSAGE(64350, _("Script lenght does not match declared length"))
#define MSG_INVALIDJOB_REQUEST_S _MESSAGE(64352, _("invalid "SFQ" value in job request"))

#define MSG_JOB_NAMETOOLONG_I _MESSAGE(64353, _("job name too long, maximum allowed length is %d characters"))

#define MSG_INVALIDJATASK_REQUEST _MESSAGE(64354, _("invalid task list in job start request"))

#define MSG_INVALID_GDIL _MESSAGE(64355, _("invalid granted destination identifier list"))

#define MSG_INVALID_QINSTANCE_NAME_S   _MESSAGE(64356, _("invalid queue instance name "SFQ))
#define MSG_JOB_NOJOBNAME              _MESSAGE(64357, _("job rejected cause there is no job_name in the request"))


#define MSG_RESOURCEQUOTA_NORULEDEFINED         _MESSAGE(64369, _("no resource quota rule specified for modification"))
#define MSG_RESOURCEQUOTA_NOVALIDEXPANDEDLIST   _MESSAGE(64370, _("no valid expanded list"))

#define MSG_RESOURCEQUOTA_DYNAMICLIMITNOTSUPPORTED   _MESSAGE(64375, _("dynamic limits are only supported for per host rules"))
#define MSG_RESOURCEQUOTA_NORULES                    _MESSAGE(64377, _("resource quota set has no rules"))
#define MSG_RESOURCEQUOTA_NONAME                     _MESSAGE(64378, _("resource quota set has no name"))
#define MSG_CENTRYREFINRQS_SS                   _MESSAGE(64379, _("Complex attribute "SFQ" is still referenced in resource quota set "SFQ"."))


#define MSG_CQUEUE_DEFOVERWRITTEN_SSSSS          _MESSAGE(64380, _("default value of "SFQ" is overwritten for hostgroup "SFQ" in queue "SFQ". Not all hosts of "SFQ" are contained in the hostlist specification of queue "SFQ"."))
#define MSG_CQUEUE_UNUSEDATTRSETTING_SS          _MESSAGE(64381, _("unused setting for attribute "SFQ" and host "SFQ" in queue "SFQ"."))

#define MSG_EVAL_EXPRESSION_PARSE_ERROR          _MESSAGE(64382, _("Parse error on position %d of the expression "SFQ"."))
#define MSG_EVAL_EXPRESSION_LONG_VALUE           _MESSAGE(64384, _("Invalid lenght of value exeed the limit %d characters."))
#define MSG_EVAL_EXPRESSION_LONG_EXPRESSION      _MESSAGE(64385, _("Invalid lenght of expression exeed the limit %d characters."))

#define MSG_AR_MISSING_VALUE_S                   _MESSAGE(64390, _("missing value for "SFQ" in advance reservation request"))
#define MSG_AR_START_END_DURATION_INVALID        _MESSAGE(64391, _("difference between end and start time does not correspond to duration"))
#define MSG_AR_START_LATER_THAN_END              _MESSAGE(64392, _("start time is later than end time"))
#define MSG_AR_START_IN_PAST                     _MESSAGE(64393, _("start time is in the past"))
#define MSG_AR_XISINVALIDARID_U                  _MESSAGE(64394, _("ERROR! "sge_U32CFormat" is an invalid advance reservation identifier"))

#endif /* __MSG_SGEOBJLIB_H */
