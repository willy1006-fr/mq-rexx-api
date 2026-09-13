// MA95.C : WebSphere MQ Support Pac MA95 - REXX support for z/OS, Windows, AIX and Linux
//
//   (C) Copyright IBM Corporation 1997, 2012
//
// This is a nearly full implementation of the MQ API with these
//      omissions :
//
//      INQ and SET only take a single attribute.
//
//      MQCONNX not implemented
//
//      MQBEGIN not implemented
//
// The MQ API is extended in several ways:
//
//
//      * The QM Handle is not externally referenced, as this is held
//            within this program
//
//      * Structures that get updated are managed via seperate
//            input and output areas, so avoiding the need call
//            to do all that tedious rebuilding for each
//
//      * Output areas contain .ZLIST which contains a list of all the
//            components within the output stem (without the dot)
//
//      * A Browse function is provided which just returns the
//            message data
//
//      * A Header Extraction function will interprete Dead Letter
//           and Transmission Headers from suitable messages
//
//      * Expansion of Events into components (like the Header Extraction)
//
//      * Interpretation of Trigger Messages and Trigger Data
//
//      * Function to only set up MQ Constants
//
//
//   In order to use this Rexx/MQ Interface, initialization function
//        must be called before usage.
//
//   For Windows, AIX and Linux this is done by a RxFuncAdd('RXMQINIT','RXMQx','RXMQINIT')
//        call to let Rexx know about the DLL, and then a
//        rcc = RXMQINIT() to call the initialization function.
//        As part of this initialization function, all the other
//        RXMQ functions are registered with Rexx.
//
//   There is no equivalent termination function, as the MQ routines
//        attach to process termination to stop access at this
//        time, and a Termination operation would interfere
//        with this processing. However, a function called
//        RXMQTERM will deregister the interface functions WITHOUT
//        doing end-of-process actions.
//
//   All the RXMQ functions return a standard Rexx Return string.
//        This is structured so that the numeric Return Code
//        (which may be negative) is obtained by a word(RCC,1) call.
//
//   The Return Code for an operation can be negative to
//        show that this DLL has detected the error, otherwise
//        it will be the MQ Completion Code
//
//        The Return String is in text format as follows:
//
//            Word 1 : Return Code
//            Word 2 : MQ Completion Code (or 0 if MQ not done)
//            Word 2 : MQ Reason     Code (or 0 if MQ not done)
//            Word 4 : RXMQ... function being run
//            Word > : OK or an helpful error message
//
//   In addition, the current (ie: the settings last set) values are
//      available in these variables:
//
//   Windows, AIX, Linux:
//          RXMQN.LASTRC    -> current operation Return Code
//          RXMQN.LASTCC    -> current operation MQ Completion Code
//          RXMQN.LASTAC    -> current operation MQ Reason     Code
//          RXMQN.LASTOP    -> current operation RXMQ function
//          RXMQN.LASTMSG   -> current operation Return String
//          or
//          RXMQT.LASTRC    -> current operation Return Code
//          RXMQT.LASTCC    -> current operation MQ Completion Code
//          RXMQT.LASTAC    -> current operation MQ Reason     Code
//          RXMQT.LASTOP    -> current operation RXMQ function
//          RXMQT.LASTMSG   -> current operation Return String
//   MVS:
//          RXMQV.LASTRC    -> current operation Return Code
//          RXMQV.LASTCC    -> current operation MQ Completion Code
//          RXMQV.LASTAC    -> current operation MQ Reason     Code
//          RXMQV.LASTOP    -> current operation RXMQ function
//          RXMQV.LASTMSG   -> current operation Return String
//   Both:
//          RXMQ.LASTRC     -> current operation Return Code
//          RXMQ.LASTCC     -> current operation MQ Completion Code
//          RXMQ.LASTAC     -> current operation MQ Reason     Code
//          RXMQ.LASTOP     -> current operation RXMQ function
//          RXMQ.LASTMSG    -> current operation Return String
//
//   As a nice little bonus, there are also defined lots of variables
//        called RXMQ.RCMAP.nn , where nn is a MQAC number, whose
//        value is the name of the Reason Code (MQRC_ERROR_THING).
//        These variables can then be used in the Rexx Exec to get
//        the name from the Reason code via a
//                 interpret 'name = RXMQ.RCMAP.'word(rcc,1)
//        statement.
//
//        A TERM function leaves the MQ Rexx Variables known; if removed via
//        a DROP call, they may be restored via RXMQCONS (registering if
//        apt via RxFuncAdd('RXMQCONS',RXQMx','RXMQCONS') if needed).
//
//
//   Tracing MAY be enabled (if this module has been compiled with
//        the relevant setting of the TRACE macro) by setting
//        values into a Rexx Variable call RXMQTRACE. The settings are:
//
//                              CONN  -> mqconn
//                              DISC  -> mqdisc
//                              OPEN  -> mqopen
//                              CLOSE -> mqclose
//                              GET   -> mqget
//                              PUT   -> mqput
//                              PUT1  -> mqput1
//                              INQ   -> mqinq
//                              SET   -> mqset
//                              CMIT  -> mqcmit
//                              BACK  -> mqback
//                              SUB   -> mqsub
//                              MH    -> mqcrtmh
//                              DMH   -> mqdltmh
//                              SMP   -> mqsetmp
//                              IMP   -> mqinqmp
//                              DMP   -> mqdltmp
//                              BMH   -> mqbufmh
//                              MBF   -> mqmhbuf
//
//                              BRO   -> Browse extension
//                              HXT   -> Header extraction extension
//                              EVENT -> Event determination extension
//                              TM    -> Trigger message extension
//                              COM   -> Command interface
//                              MQV   -> Debug a RXMQV
//
//                              INIT  -> initialization processing
//                              TERM  -> Deregistration processing
//
//                              *     -> Trace everything!!!
//
//   Support  : This program is not supported in any way by IBM.
//              It is distributed only to show techniques for
//              Message Queueing (if so distributed).
//              It may be freely modified and adapted by your
//              installation.
//
//
//
 
//
// Standard Header file includes
//
#ifdef __MVS__
#ifndef _EXT
#define _EXT 1
#endif
#endif
 #include <string.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <ctype.h>
#define __USE_MINGW_ANSI_STDIO 1
 #include <inttypes.h>
 #include <errno.h>
#ifndef __MVS__
 #include <windows.h>
#endif
 
// Required for Win MINGW only
#define _int64 __int64
 
// For MVS FTYPE is required before rexx.h
#define FTYPE extern int APIENTRY
 
//
// Rexx Header file includes
//
#define INCL_RXSHV
#define INCL_RXFUNC
#include <rexx.h>
 
//
// WebSphere MQ Header file includes
//
 
#include <cmqc.h>
#include <cmqcfc.h>
 
#ifdef __MVS__
//
// REXX Functions for MVS
// And redefine entry points so that externalized entries are in assembler stub
//
  #define RXMQINIT CPPMINIT
  #define RXMQTERM CPPMTERM
  #define RXMQCONS CPPMCONS
  #define RXMQCONN CPPMCONN
  #define RXMQOPEN CPPMOPEN
  #define RXMQCLOS CPPMCLOS
  #define RXMQDISC CPPMDISC
  #define RXMQCMIT CPPMCMIT
  #define RXMQBACK CPPMBACK
  #define RXMQPUT  CPPMPUT
  #define RXMQPUT1 CPPMPUT1
  #define RXMQGET  CPPMGET
  #define RXMQINQ  CPPMINQ
  #define RXMQSET  CPPMSET
  #define RXMQSUB  CPPMSUB
  #define RXMQBRWS CPPMBRWS
  #define RXMQHXT  CPPMHXT
  #define RXMQEVNT CPPMEVNT
  #define RXMQTM   CPPMTM
  #define RXMQC    CPPMC
  #define RXMQV    CPPMV
  #define RXMQVC   CPPMVC
  #define RXMQMH   CPPMMH
  #define RXMQDMH  CPPMDMH
  #define RXMQSMP  CPPMSMP
  #define RXMQIMP  CPPMIMP
  #define RXMQDMP  CPPMDMP
  #define RXMQBMH  CPPMBMH
  #define RXMQMBF  CPPMMBF
//
// Function to print to stdout (REXX terminal or standard output)
//
#define PRINTF(s) RXSAY(s, strlen(s));
#else
#define PRINTF(s) printf(s); \
                  fflush(NULL) ;  // Required to flush buffer
#endif
 
#define RXMQPARM ( PSZ       afuncname /*Rexx Func name       */\
                 , MQLONG    aargc     /*     Number of parms */\
                 , PRXSTRING aargv     /*     Parms           */\
                 , PSZ       aqname    /*     Cur Queue       */\
                 , PRXSTRING aretstr   /*     Return String   */\
                 )
 
//
// Global definitions
//
//  MINQS     -> Start scan of Queues allowed to process (not 0!)
//  MAXQS     -> maximum number of Queues allowed to process
//
 #define MINQS 1
 #define MAXQS 100
 #define MAXCOMMLEN 5000
#define RXMQ_MAX_ZERO_LENGTH_STRING_LIST_COUNT 4096U
 
 #define RXMQANCHOR "RXMQANCHOR"
 #define RXMQeyecatcher "RXMQ"
 typedef struct _RXMQCB {
     MQCHAR4    StrucId                      ; // Structure identifier
     MQULONG    tracebits                    ; // Variable to contain current trace status
     char       QMname[MQ_Q_MGR_NAME_LENGTH] ; // QM name
     MQHCONN    QMh                          ; // Connection handle
     MQHOBJ     Qh[MAXQS + 1]                ; // Queue handle
 } RXMQCB;

 typedef struct _RXMQREGENTRY {
     struct _RXMQREGENTRY *next              ;
     RXMQCB                 anchor            ;
 } RXMQREGENTRY;

 static RXMQREGENTRY *anchor_registry_head = NULL ;
#ifdef __MVS__
 static cs_t anchor_registry_lock_word = 0 ;
#else
 static volatile LONG anchor_registry_lock_word = 0 ;
#endif

 static void anchor_registry_lock ( void )
 {
#ifdef __MVS__
  cs_t oldword ;

  do
    {
     oldword = 0 ;
    }
  while (__cs(&oldword, &anchor_registry_lock_word, 1) != 0) ;
#else
  while (InterlockedCompareExchange(&anchor_registry_lock_word,
                                    1, 0) != 0) ;
#endif

  return ;
 }

 static void anchor_registry_unlock ( void )
 {
#ifdef __MVS__
  cs_t oldword ;

  do
    {
     oldword = 1 ;
    }
  while (__cs(&oldword, &anchor_registry_lock_word, 0) != 0) ;
#else
  InterlockedExchange(&anchor_registry_lock_word, 0) ;
#endif

  return ;
 }

 static RXMQCB *anchor_registry_lookup ( RXMQCB * candidate )
 {
  RXMQREGENTRY * entry   = NULL ;
  RXMQCB       * trusted = NULL ;

  anchor_registry_lock() ;
  entry = anchor_registry_head ;
  while ( entry != NULL )
    {
     if ( &entry->anchor == candidate )
       {
        trusted = &entry->anchor ;
        break ;
       }
     entry = entry->next ;
    }
  anchor_registry_unlock() ;

  return trusted ;
 }

 static void anchor_registry_add ( RXMQREGENTRY * entry )
 {
  anchor_registry_lock() ;
  entry->next = anchor_registry_head ;
  anchor_registry_head = entry ;
  anchor_registry_unlock() ;

  return ;
 }
 
//
// Trace/Return variables
//
#ifdef __MVS__
  #define PREFIX   "RXMQV."
  #define TRACEVAR "RXMQVTRACE"
  #define RCMAP    "RXMQV.RCMAP.%lu"
#endif
 
#ifdef _RXMQN
  #define PREFIX   "RXMQN."
  #define TRACEVAR "RXMQNTRACE"
  #define RCMAP    "RXMQN.RCMAP.%lu"
  #define THISDLL  "RXMQN"
 //
 // This is a table of functions to be registered with Rexx
 //
  static PSZ funcs[] = {"RXMQV"       ,
                            "RXMQCONN"    ,  "RXMQNCONN"   ,
                            "RXMQDISC"    ,  "RXMQNDISC"   ,
                            "RXMQOPEN"    ,  "RXMQNOPEN"   ,
                            "RXMQCLOS"    ,  "RXMQNCLOSE"  ,
                            "RXMQGET"     ,  "RXMQNGET"    ,
                            "RXMQPUT"     ,  "RXMQNPUT"    ,
                            "RXMQINQ"     ,  "RXMQNINQ"    ,
                            "RXMQSET"     ,  "RXMQNSET"    ,
                            "RXMQMH"      ,  "RXMQNMH"     ,
                            "RXMQDMH"     ,  "RXMQNDMH"    ,
                            "RXMQSMP"     ,  "RXMQNSMP"    ,
                            "RXMQIMP"     ,  "RXMQNIMP"    ,
                            "RXMQDMP"     ,  "RXMQNDMP"    ,
                            "RXMQBMH"     ,  "RXMQNBMH"    ,
                            "RXMQMBF"     ,  "RXMQNMBF"    ,
                            "RXMQSUB"     ,  "RXMQNSUB"    ,
                            "RXMQCMIT"    ,  "RXMQNCMIT"   ,
                            "RXMQBACK"    ,  "RXMQNBACK"   ,
                            "RXMQBRWS"    ,  "RXMQNBROWSE" ,
                            "RXMQHXT"     ,  "RXMQNHXT"    ,
                            "RXMQEVNT"    ,  "RXMQNEVENT"  ,
                            "RXMQTM"      ,  "RXMQNTM"     ,
                            "RXMQPUT1"    ,  "RXMQNPUT1"   ,
                            "RXMQC"       ,  "RXMQNC"      ,
                            "RXMQCONS"    ,  "RXMQNCONS"   ,
                            "RXMQTERM"    ,  "RXMQNTERM"
              } ;
 static MQULONG numfuncs = sizeof(funcs)/sizeof(char *) ;
#endif
 
#ifdef _RXMQT
  #define PREFIX   "RXMQT."
  #define RCMAP    "RXMQT.RCMAP.%lu"
  #define TRACEVAR "RXMQTTRACE"
  #define THISDLL  "RXMQT"
//
// This is a table of functions to be registered with Rexx
//
 static PSZ funcs[] = {"RXMQV"       ,
                           "RXMQCONN"    ,  "RXMQTCONN"   ,
                           "RXMQDISC"    ,  "RXMQTDISC"   ,
                           "RXMQOPEN"    ,  "RXMQTOPEN"   ,
                           "RXMQCLOS"    ,  "RXMQTCLOSE"  ,
                           "RXMQGET"     ,  "RXMQTGET"    ,
                           "RXMQPUT"     ,  "RXMQTPUT"    ,
                           "RXMQINQ"     ,  "RXMQTINQ"    ,
                           "RXMQSET"     ,  "RXMQTSET"    ,
                           "RXMQSUB"     ,  "RXMQTSUB"    ,
                           "RXMQMH"      ,  "RXMQTMH"     ,
                           "RXMQDMH"     ,  "RXMQTDMH"    ,
                           "RXMQSMP"     ,  "RXMQTSMP"    ,
                           "RXMQIMP"     ,  "RXMQTIMP"    ,
                           "RXMQDMP"     ,  "RXMQTDMP"    ,
                           "RXMQBMH"     ,  "RXMQTBMH"    ,
                           "RXMQMBF"     ,  "RXMQTMBF"    ,
                           "RXMQCMIT"    ,  "RXMQTCMIT"   ,
                           "RXMQBACK"    ,  "RXMQTBACK"   ,
                           "RXMQBRWS"    ,  "RXMQTBROWSE" ,
                           "RXMQHXT"     ,  "RXMQTHXT"    ,
                           "RXMQEVNT"    ,  "RXMQTEVENT"  ,
                           "RXMQTM"      ,  "RXMQTTM"     ,
                           "RXMQPUT1"    ,  "RXMQTPUT1"   ,
                           "RXMQC"       ,  "RXMQTC"      ,
                           "RXMQCONS"    ,  "RXMQTCONS"   ,
                           "RXMQTERM"    ,  "RXMQTTERM"
                          } ;
 static MQULONG numfuncs = sizeof(funcs)/sizeof(char *) ;
#endif
 
//
// Define type for output messages struct
//
 typedef struct _RETMSG
                {
                 int    retcode  ;  // Internal (minus) retcode
                 char * retmsgc  ;  // Text part of message
                } RETMSG, *PRETMSG         ;
 
//
// Define global structure defaults
//
 MQOD    od_default    = {MQOD_DEFAULT}   ;
 MQSD    sd_default    = {MQSD_DEFAULT}   ;
 MQMD2   md_default    = {MQMD2_DEFAULT}  ;
 MQPMO   pmo_default   = {MQPMO_DEFAULT}  ;
 MQGMO   gmo_default   = {MQGMO_DEFAULT}  ;
 MQCMHO  cmho_default  = {MQCMHO_DEFAULT} ;
 MQDMHO  dmho_default  = {MQDMHO_DEFAULT} ;
 MQSMPO  smpo_default  = {MQSMPO_DEFAULT} ;
 MQIMPO  impo_default  = {MQIMPO_DEFAULT} ;
 MQDMPO  dmpo_default  = {MQDMPO_DEFAULT} ;
 MQBMHO  bmho_default  = {MQBMHO_DEFAULT} ;
 MQMHBO  mhbo_default  = {MQMHBO_DEFAULT} ;
 MQPD    pd_default    = {MQPD_DEFAULT}   ;
 
//
// Encoding of numeric values built internally by the RXMQ wrappers.
//
// On z/OS, MQFLOAT32 and MQFLOAT64 are IEEE values because the
// module is compiled with FLOAT(IEEE). Therefore MQENC_NATIVE,
// whose floating-point component is MQENC_FLOAT_S390 on z/OS,
// does not describe the generated floating-point bytes.
//
#ifdef __MVS__
#define RXMQ_NUMERIC_ENCODING                                   \
        (MQENC_INTEGER_NORMAL |                                 \
         MQENC_DECIMAL_NORMAL |                                 \
         MQENC_FLOAT_IEEE_NORMAL)
#else
#define RXMQ_NUMERIC_ENCODING MQENC_NATIVE
#endif
#ifdef __MVS__
#define RXMQ_FLOAT_ENCODING MQENC_FLOAT_IEEE_NORMAL
#else
#define RXMQ_FLOAT_ENCODING \
        (MQENC_NATIVE & MQENC_FLOAT_MASK)
#endif
//
//  Global debug variable for controlling current trace status
//
 
  #define ZERO  0x00000000
  #define CONN  0x80000000
  #define DISC  0x40000000
  #define OPEN  0x20000000
  #define CLOSE 0x10000000
  #define GET   0x08000000
  #define PUT   0x04000000
  #define PUT1  0x02000000
  #define INQ   0x01000000
  #define SET   0x00800000
  #define CMIT  0x00400000
  #define BACK  0x00200000
  #define BRO   0x00100000
  #define HXT   0x00080000
  #define EVENT 0x00040000
  #define TM    0x00020000
  #define MQV   0x00010000
  #define COM   0x00008000
  #define SUB   0x00004000
  #define INIT  0x00000020
  #define TERM  0x00000010
  #define MH    0x00002000
  #define DMH   0x00001000
  #define SMP   0x00000800
  #define IMP   0x00000400
  #define DMP   0x00000200
  #define BMH   0x00000100
  #define MBF   0x00000080
  #define ALL   0xFFFFFFFF
 
//
//  The TRACE macro is used to enable tracing. This may be perminently
//            enabled/disabled, or dynamically under the control
//            of the RXMQTRACE Rexx variable.
//
//
//  TRACE settings :
//
//  TRACE  -> if ...    for dynamic tracing based on RXMQTRACE (TRACX for values)
//  TRACE  -> printf    for static trace everything debugging
//  TRACE  -> ;         for stop all tracing
//
//  TRACES -> printf    for static trace everything
//  TRACES -> ;         for stop all tracing
//
//  DUMPCB -> if ...    for dynamic tracing based on RXMQTRACE (control blocks)
//  DUMPCB -> printf    for static trace everything debugging
//  DUMPCB -> ;         for stop all tracing
//
//
//  The setting are thus:
//
// #define TRACE(p1,p2) if ( p1 ) printf p2
// #define TRACE(p1,p2) printf p2
// #define TRACE(p1,p2)
//
// #define TRACX(p1,p2) if ( p1 ) DumpDump p2
// #define TRACX(p1,p2) DumpDump p2
// #define TRACX(p1,p2)
//
// #define TRACES(p1) printf p1
// #define TRACES(p1)
//
// #define DUMPCB(p1,p2) if ( p1 ) DumpControlBlock( p2 )
// #define DUMPCB(p1,p2) DumpControlBlock( p2 )
// #define DUMPCB(p1,p2)
 
#define TRACE(p1,p2) if ( p1 ) printf p2   ;
#define TRACX(p1,p2) if ( p1 ) DumpDump p2 ;
#define TRACES(p1)
#define DUMPCB(p1,p2) if ( p1 ) DumpControlBlock( p2 ) ;
 
//
// Set of functions to support trace
//
//
// Print a string in hexadecimal form:
// <01234567 89ABCDEF ...>
//
 void DumpHex (MQBYTE * string, MQULONG size)
 {
  MQULONG i                      ;
  printf( "<%.2X", string[0] )   ; // print 1st char
  for(i=1; i < size; i++)
    {
     if (i%4 == 0) printf(" ")   ; // word separator
     printf("%.2X",string[i])    ; // only print hex
    }
  printf(">")                    ;
 }
 
//
// Print a string in character form (if they are printable):
//  *abcdefghABCDEFGH...*
//
 void DumpChars (MQBYTE * string, MQULONG size)
 {
  MQULONG i                      ;
  printf(" *")                   ;
  for(i=0; i < size; i++) printf("%c", ( isprint(string[i]) ? string[i] : '.' ) )  ;
  printf("* ")                   ;
 }
 
//
// Print a string as hex and char:
// <01234567 89ABCDEF ...> *abcdefghABCDEFGH...*
//
 void DumpDump (MQBYTE * string, MQULONG size)
 {
  MQULONG i                            ;
  if (size > 32 ) printf( "\n" )       ;
  for(i=0; i < size; i+=32)
  {
   DumpHex  ( &string[i], (size-i > 32) ? 32 : size-i ) ;
   DumpChars( &string[i], (size-i > 32) ? 32 : size-i ) ;
   if (size > 32 ) printf( "\n" )      ;
  }
 }
 
//
// Set of functions to format internal & MQ control blocks.
//
  void DumpChar (char * name, char byte)
  {
   char      format[100] ; // format variable
 
   strcpy( format, name )                          ;
   strcat( format,"%c <%X>\n" )                    ; // use exact size and left justify
   printf( format, byte, byte )                    ;
  }
 
 void DumpString (char * name, char * string, MQULONG size)
 {
  MQULONG   i           ; // counter
  char      format[100] ; // format variable
 
  strcpy( format, name)                           ;
  strcat( format,"%-*.*s<%.2X")                   ; // use exact size and left justify
  printf( format, size, size, string, string[0] ) ;
  for(i=1; i < size; i++)
    {
     if (i%4 == 0) printf(" ")        ; // word separator
     printf("%.2X" ,string[i] )       ; // only print hex
    }
  printf( ">\n")                      ;
 }
 
 void DumpBytes (char * name, MQBYTE * string, MQULONG size)
 {
  printf( name )            ; // variable name
  DumpHex  ( string, size ) ;
  DumpChars( string, size ) ;
  if (strlen(name) != 0) printf( "\n") ; // only when full line
 }
 
 void DumpLongHex (char * name, MQLONG value)
 {
  char      format[100] ; // format variable
 
  strcpy( format, name)                     ;
  strcat( format,"<%0*"PRIX32">\n")         ; // use exact size and prefix with zeros
  printf( format, 2*sizeof(MQLONG), value ) ;
 }
 
 void DumpLoLoHex (char * name, MQINT64 value)
 {
  char      format[100] ; // format variable
 
  strcpy( format, name)                            ;
  strcat( format,"%"PRIX64" <%0*.*llX>\n")              ; // peculiarity here, but works
  printf( format, value, 2*sizeof(MQINT64), value );
 }
 
 void DumpLongDec (char * name, MQLONG value)
 {
  char      format[100] ; // format variable
 
  strcpy( format, name)                                       ;
  strcat( format,"%"PRId32" <%0*lX>\n")                       ; // use exact size and prefix with zeros
  printf( format, value, (int32_t)(2*sizeof(MQLONG)), value ) ;
 }
 
 void DumpPointer (char * name, MQPTR value)
 {
  char      format[100] ; // format variable
 
  strcpy( format, name)                           ;
  strcat( format,"%p <%*p>\n")                    ; // use exact size
  printf( format, value, 2*sizeof(MQPTR), value ) ;
 }
 
 void DumpControlBlock (void * cbptr)
 {
  int       i       ; // counter
  RXMQCB  * rxmqcbp ; // pointer to RXMQCB
  MQOD    * odp     ; // pointer to MQOD
  MQMD2   * mdp     ; // pointer to MQMD
  MQGMO   * gmop    ; // pointer to MQGMO
  MQPMO   * pmop    ; // pointer to MQPMO
  MQCMHO  * cmhop   ; // pointer to MQCMHO
  MQDMHO  * dmhop   ; // pointer to MQDMHO
  MQSMPO  * smpop   ; // pointer to MQSMPO
  MQIMPO  * impop   ; // pointer to MQIMPO
  MQDMPO  * dmpop   ; // pointer to MQDMPO
  MQBMHO  * bmhop   ; // pointer to MQBMHO
  MQMHBO *  mhbop   ; // pointer to MQMHBO
  MQPD    * pdp     ; // pointer to MQPD
  MQSD    * sdp     ; // pointer to MQSD
 
  if (memcmp(cbptr, RXMQeyecatcher, sizeof(MQCHAR4)) == 0)       // Format RXMQCB
   {
    rxmqcbp = (RXMQCB *) cbptr ;
    printf( "\n")  ;
    DumpString  ( "RXMQCB StrucId           :", rxmqcbp->StrucId,   sizeof(MQCHAR4)  ) ;
    DumpLongHex ( "       tracebits         :", rxmqcbp->tracebits                   ) ;
    DumpString  ( "       QMname            :", rxmqcbp->QMname ,   sizeof(MQCHAR48) ) ;
    DumpLongHex ( "       QMh               :", rxmqcbp->QMh                         ) ;
 
    for(i=0; i <= MAXQS; i++)
      if (rxmqcbp->Qh[i])
        printf("       Qh[%.2d]            :<%0*"PRIX32">\n",
               i, (int)(2*sizeof(MQHOBJ)), (uint32_t)rxmqcbp->Qh[i] ) ;
    printf( "\n") ;
   }
 
  if (memcmp(cbptr, MQOD_STRUC_ID, sizeof(MQCHAR4)) == 0)      // Format MQOD
    {
     odp = (MQOD *) cbptr;
     printf( "\n") ;
     DumpString  ( "MQOD StrucId             :", odp->StrucId, sizeof(MQCHAR4) )              ;
     DumpLongDec ( "     Version             :", odp->Version )                               ;
     DumpLongDec ( "     ObjectType          :", odp->ObjectType )                            ;
     DumpString  ( "     ObjectName          :", odp->ObjectName, sizeof(MQCHAR48) )          ;
     DumpString  ( "     ObjectQMgrName      :", odp->ObjectQMgrName, sizeof(MQCHAR48) )      ;
     DumpString  ( "     DynamicQName        :", odp->DynamicQName, sizeof(MQCHAR48) )        ;
     DumpString  ( "     AlternateUserId     :", odp->AlternateUserId, sizeof(MQCHAR12) )     ;
     DumpLongDec ( "     RecsPresent         :", odp->RecsPresent )                           ;
     DumpLongDec ( "     KnownDestCount      :", odp->KnownDestCount )                        ;
     DumpLongDec ( "     UnknownDestCount    :", odp->UnknownDestCount )                      ;
     DumpLongDec ( "     InvalidDestCount    :", odp->InvalidDestCount )                      ;
     DumpLongDec ( "     ObjectRecOffset     :", odp->ObjectRecOffset )                       ;
     DumpLongDec ( "     ResponseRecOffset   :", odp->ResponseRecOffset )                     ;
     DumpPointer ( "     ObjectRecPtr        :", odp->ObjectRecPtr )                          ;
     DumpPointer ( "     ResponseRecPtr      :", odp->ResponseRecPtr )                        ;
     DumpBytes   ( "     AlternateSecurityId :", odp->AlternateSecurityId, sizeof(MQBYTE40) ) ;
     DumpString  ( "     ResolvedQName       :", odp->ResolvedQName, sizeof(MQCHAR48) )       ;
     DumpString  ( "     ResolvedQMgrName    :", odp->ResolvedQMgrName, sizeof(MQCHAR48) )    ;
     DumpPointer ( "     ObjectString.Ptr    :", odp->ObjectString.VSPtr )                    ;
     DumpLongDec ( "     ObjectString.Off    :", odp->ObjectString.VSOffset )                 ;
     DumpLongDec ( "     ObjectString.Size   :", odp->ObjectString.VSBufSize )                ;
     DumpLongDec ( "     ObjectString.Len    :", odp->ObjectString.VSLength )                 ;
     DumpLongDec ( "     ObjectString.CCSID  :", odp->ObjectString.VSCCSID )                  ;
     DumpPointer ( "     SelectionString.Ptr :", odp->SelectionString.VSPtr )                 ;
     DumpLongDec ( "     SelectionString.Off :", odp->SelectionString.VSOffset )              ;
     DumpLongDec ( "     SelectionString.Size:", odp->SelectionString.VSBufSize )             ;
     DumpLongDec ( "     SelectionString.Len :", odp->SelectionString.VSLength )              ;
     DumpLongDec ( "     SelectionString.CCSI:", odp->SelectionString.VSCCSID )               ;
     DumpPointer ( "     ResObjectString.Ptr :", odp->ResObjectString.VSPtr )                 ;
     DumpLongDec ( "     ResObjectString.Off :", odp->ResObjectString.VSOffset )              ;
     DumpLongDec ( "     ResObjectString.Size:", odp->ResObjectString.VSBufSize )             ;
     DumpLongDec ( "     ResObjectString.Len :", odp->ResObjectString.VSLength )              ;
     DumpLongDec ( "     ResObjectString.CCSI:", odp->ResObjectString.VSCCSID )               ;
     DumpLongDec ( "     ResolvedType        :", odp->ResolvedType )                          ;
     printf( "\n") ;
    }
 
  if (memcmp(cbptr, MQMD_STRUC_ID, sizeof(MQCHAR4)) == 0)      // Format MQMD
    {
     mdp = (MQMD2 *) cbptr;
     printf( "\n") ;
     DumpString  ( "MQMD StrucId             :", mdp->StrucId, sizeof(MQCHAR4)  )          ;
     DumpLongDec ( "     Version             :", mdp->Version )                            ;
     DumpLongDec ( "     Report              :", mdp->Report )                             ;
     DumpLongDec ( "     MsgType             :", mdp->MsgType )                            ;
     DumpLongDec ( "     Expiry              :", mdp->Expiry )                             ;
     DumpLongDec ( "     Feedback            :", mdp->Feedback )                           ;
     DumpLongDec ( "     Encoding            :", mdp->Encoding )                           ;
     DumpLongDec ( "     CodedCharSetId      :", mdp->CodedCharSetId )                     ;
     DumpString  ( "     Format              :", mdp->Format, sizeof(MQCHAR8) )            ;
     DumpLongDec ( "     Priority            :", mdp->Priority )                           ;
     DumpLongDec ( "     Persistence         :", mdp->Persistence )                        ;
     DumpBytes   ( "     MsgId               :", mdp->MsgId, sizeof(MQBYTE24) )            ;
     DumpBytes   ( "     CorrelId            :", mdp->CorrelId, sizeof(MQBYTE24) )         ;
     DumpLongDec ( "     BackoutCount        :", mdp->BackoutCount )                       ;
     DumpString  ( "     ReplyToQ            :", mdp->ReplyToQ, sizeof(MQCHAR48) )         ;
     DumpString  ( "     ReplyToQMgr         :", mdp->ReplyToQMgr, sizeof(MQCHAR48) )      ;
     DumpString  ( "     UserIdentifier      :", mdp->UserIdentifier, sizeof(MQCHAR12) )   ;
     DumpBytes   ( "     AccountingToken     :", mdp->AccountingToken, sizeof(MQBYTE32) )  ;
     DumpString  ( "     ApplIdentityData    :", mdp->ApplIdentityData, sizeof(MQCHAR32) ) ;
     DumpLongDec ( "     PutApplType         :", mdp->PutApplType )                        ;
     DumpString  ( "     PutApplName         :", mdp->PutApplName, sizeof(MQCHAR28) )      ;
     DumpString  ( "     PutDate             :", mdp->PutDate, sizeof(MQCHAR8) )           ;
     DumpString  ( "     PutTime             :", mdp->PutTime, sizeof(MQCHAR8) )           ;
     DumpString  ( "     ApplOriginData      :", mdp->ApplOriginData, sizeof(MQCHAR4) )    ;
     DumpBytes   ( "     GroupId             :", mdp->GroupId, sizeof(MQBYTE24) )          ;
     DumpLongDec ( "     MsgSeqNumber        :", mdp->MsgSeqNumber )                       ;
     DumpLongDec ( "     Offset              :", mdp->Offset )                             ;
     DumpLongDec ( "     MsgFlags            :", mdp->MsgFlags )                           ;
     DumpLongDec ( "     OriginalLength      :", mdp->OriginalLength )                     ;
     printf( "\n") ;
    }
 
  if (memcmp(cbptr, MQGMO_STRUC_ID, sizeof(MQCHAR4)) == 0)       // Format MQGMO
    {
     gmop = (MQGMO *) cbptr;
     printf( "\n") ;
     DumpString  ( "MQGMO StrucId            :", gmop->StrucId, sizeof(MQCHAR4)  )       ;
     DumpLongDec ( "      Version            :", gmop->Version )                         ;
     DumpLongDec ( "      Options            :", gmop->Options )                         ;
     DumpLongDec ( "      WaitInterval       :", gmop->WaitInterval )                    ;
#ifdef __MVS__
     DumpPointer ( "      Signal1            :", gmop->Signal1 )                         ;
#else
     DumpLongDec ( "      Signal1            :", gmop->Signal1 )                         ;
#endif
     DumpLongDec ( "      Signal2            :", gmop->Signal2 )                         ;
     DumpString  ( "      ResolvedQName      :", gmop->ResolvedQName, sizeof(MQCHAR48) ) ;
     DumpLongDec ( "      MatchOptions       :", gmop->MatchOptions )                    ;
     DumpChar    ( "      GroupStatus        :", gmop->GroupStatus )                     ;
     DumpChar    ( "      SegmentStatus      :", gmop->SegmentStatus )                   ;
     DumpChar    ( "      Segmentation       :", gmop->Segmentation )                    ;
     DumpChar    ( "      Reserved1          :", gmop->Reserved1 )                       ;
     DumpBytes   ( "      MsgToken           :", gmop->MsgToken, sizeof(MQBYTE16) )      ;
     DumpLongDec ( "      ReturnedLength     :", gmop->ReturnedLength )                  ;
     DumpLongDec ( "      Reserved2          :", gmop->Reserved2 )                       ;
     DumpLoLoHex ( "      MsgHandle          :", gmop->MsgHandle )                       ;
     printf( "\n") ;
    }
 
  if (memcmp(cbptr, MQPMO_STRUC_ID, sizeof(MQCHAR4)) == 0)       // Format MQPMO
    {
     pmop = (MQPMO *) cbptr;
     printf( "\n") ;
     DumpString  ( "MQPMO StrucId            :", pmop->StrucId, sizeof(MQCHAR4)  )          ;
     DumpLongDec ( "      Version            :", pmop->Version )                            ;
     DumpLongDec ( "      Options            :", pmop->Options )                            ;
     DumpLongDec ( "      Timeout            :", pmop->Timeout )                            ;
     DumpLongHex ( "      Context            :", pmop->Context )                            ;
     DumpLongDec ( "      KnownDestCount     :", pmop->KnownDestCount )                     ;
     DumpLongDec ( "      UnknownDestCount   :", pmop->UnknownDestCount )                   ;
     DumpLongDec ( "      InvalidDestCount   :", pmop->InvalidDestCount )                   ;
     DumpString  ( "      ResolvedQName      :", pmop->ResolvedQName, sizeof(MQCHAR48) )    ;
     DumpString  ( "      ResolvedQMgrName   :", pmop->ResolvedQMgrName, sizeof(MQCHAR48) ) ;
     DumpLongDec ( "      RecsPresent        :", pmop->RecsPresent )                        ;
     DumpLongDec ( "      PutMsgRecFields    :", pmop->PutMsgRecFields )                    ;
     DumpLongDec ( "      PutMsgRecOffset    :", pmop->PutMsgRecOffset )                    ;
     DumpLongDec ( "      ResponseRecOffset  :", pmop->ResponseRecOffset )                  ;
     DumpPointer ( "      ResponseRecPtr     :", pmop->ResponseRecPtr )                     ;
     DumpLoLoHex ( "      OriginalMsgHandle  :", pmop->OriginalMsgHandle )                  ;
     DumpLoLoHex ( "      NewMsgHandle       :", pmop->NewMsgHandle )                       ;
     DumpLongDec ( "      Action             :", pmop->Action )                             ;
     DumpLongDec ( "      PubLevel           :", pmop->PubLevel )                           ;
     printf( "\n") ;
    }
 
  if (memcmp(cbptr, MQCMHO_STRUC_ID, sizeof(MQCHAR4)) == 0)    // Format MQCMHO
    {
     cmhop = (MQCMHO *) cbptr;
     printf( "\n") ;
     DumpString  ( "MQCMHO StrucId          :", cmhop->StrucId, sizeof(MQCHAR4) ) ;
     DumpLongDec ( "       Version          :", cmhop->Version )                  ;
     DumpLongDec ( "       Options          :", cmhop->Options )                  ;
     printf( "\n") ;
    }
  if (memcmp(cbptr, MQDMHO_STRUC_ID, sizeof(MQCHAR4)) == 0)    // Format MQDMHO
    {
     dmhop = (MQDMHO *) cbptr;
     printf( "\n") ;
     DumpString  ( "MQDMHO StrucId          :", dmhop->StrucId, sizeof(MQCHAR4) ) ;
     DumpLongDec ( "       Version          :", dmhop->Version )                  ;
     DumpLongDec ( "       Options          :", dmhop->Options )                  ;
     printf( "\n") ;
    }
  if (memcmp(cbptr, MQSMPO_STRUC_ID, sizeof(MQCHAR4)) == 0)       // Format MQSMPO
    {
     smpop = (MQSMPO *) cbptr;
     printf( "\n") ;
     DumpString  ( "MQSMPO StrucId          :", smpop->StrucId, sizeof(MQCHAR4) ) ;
     DumpLongDec ( "       Version          :", smpop->Version )                  ;
     DumpLongDec ( "       Options          :", smpop->Options )                  ;
     printf( "\n") ;
    }
if (memcmp(cbptr, MQIMPO_STRUC_ID, sizeof(MQCHAR4)) == 0)       // Format MQIMPO
  {
   impop = (MQIMPO *) cbptr;
   printf( "\n") ;
   DumpString  ( "MQIMPO StrucId          :", impop->StrucId, sizeof(MQCHAR4) ) ;
   DumpLongDec ( "       Version          :", impop->Version )                  ;
   DumpLongDec ( "       Options          :", impop->Options )                  ;
   DumpLongDec ( "       RequestedEncoding :", impop->RequestedEncoding )       ;
   DumpLongDec ( "       RequestedCCSID    :", impop->RequestedCCSID )          ;
   DumpLongDec ( "       ReturnedEncoding  :", impop->ReturnedEncoding )        ;
   DumpLongDec ( "       ReturnedCCSID     :", impop->ReturnedCCSID )           ;
   printf( "\n") ;
  }
if (memcmp(cbptr, MQDMPO_STRUC_ID, sizeof(MQCHAR4)) == 0)       // Format MQDMPO
  {
   dmpop = (MQDMPO *) cbptr;
   printf( "\n") ;
   DumpString  ( "MQDMPO StrucId          :", dmpop->StrucId, sizeof(MQCHAR4) ) ;
   DumpLongDec ( "       Version          :", dmpop->Version )                  ;
   DumpLongDec ( "       Options          :", dmpop->Options )                  ;
   printf( "\n") ;
  }
if (memcmp(cbptr, MQBMHO_STRUC_ID, sizeof(MQCHAR4)) == 0)       // Format MQBMHO
  {
   bmhop = (MQBMHO *) cbptr;
   printf( "\n") ;
   DumpString  ( "MQBMHO StrucId          :", bmhop->StrucId, sizeof(MQCHAR4) ) ;
   DumpLongDec ( "       Version          :", bmhop->Version )                  ;
   DumpLongDec ( "       Options          :", bmhop->Options )                  ;
   printf( "\n") ;
  }
if (memcmp(cbptr, MQMHBO_STRUC_ID, sizeof(MQCHAR4)) == 0)
  {
   mhbop = (MQMHBO *) cbptr;
   printf("\n");
   DumpString ("MQMHBO StrucId          :", mhbop->StrucId,
               sizeof(MQCHAR4));
   DumpLongDec("       Version          :", mhbop->Version);
   DumpLongDec("       Options          :", mhbop->Options);
   printf("\n");
  }
if (memcmp(cbptr, MQPD_STRUC_ID, sizeof(MQCHAR4)) == 0)         // Format MQPD
    {
     pdp = (MQPD *) cbptr;
     printf( "\n") ;
     DumpString  ( "MQPD StrucId            :", pdp->StrucId, sizeof(MQCHAR4) ) ;
     DumpLongDec ( "     Version            :", pdp->Version )                  ;
     DumpLongDec ( "     Options            :", pdp->Options )                  ;
     DumpLongDec ( "     Support            :", pdp->Support )                  ;
     DumpLongDec ( "     Context            :", pdp->Context )                  ;
     DumpLongDec ( "     CopyOptions        :", pdp->CopyOptions )              ;
     printf( "\n") ;
    }
  if (memcmp(cbptr, MQSD_STRUC_ID, sizeof(MQCHAR4)) == 0)     // Format MQSD
    {
     sdp = (MQSD *) cbptr;
     printf( "\n") ;
     DumpString  ( "MQSD StrucId             :", sdp->StrucId, sizeof(MQCHAR4)  )             ;
     DumpLongDec ( "     Version             :", sdp->Version )                               ;
     DumpLongDec ( "     Options             :", sdp->Options )                               ;
     DumpString  ( "     ObjectName          :", sdp->ObjectName, sizeof(MQCHAR48) )          ;
     DumpString  ( "     AlternateUserId     :", sdp->AlternateUserId, sizeof(MQCHAR12) )     ;
     DumpBytes   ( "     AlternateSecurityId :", sdp->AlternateSecurityId, sizeof(MQBYTE40) ) ;
     DumpLongDec ( "     SubExpiry           :", sdp->SubExpiry )                             ;
     DumpPointer ( "     ObjectString.Ptr    :", sdp->ObjectString.VSPtr )                    ;
     DumpLongDec ( "     ObjectString.Off    :", sdp->ObjectString.VSOffset )                 ;
     DumpLongDec ( "     ObjectString.Size   :", sdp->ObjectString.VSBufSize )                ;
     DumpLongDec ( "     ObjectString.Len    :", sdp->ObjectString.VSLength )                 ;
     DumpLongDec ( "     ObjectString.CCSID  :", sdp->ObjectString.VSCCSID )                  ;
     DumpPointer ( "     SubName.Ptr         :", sdp->SubName.VSPtr )                         ;
     DumpLongDec ( "     SubName.Off         :", sdp->SubName.VSOffset )                      ;
     DumpLongDec ( "     SubName.Size        :", sdp->SubName.VSBufSize )                     ;
     DumpLongDec ( "     SubName.Len         :", sdp->SubName.VSLength )                      ;
     DumpLongDec ( "     SubName.CCSID       :", sdp->SubName.VSCCSID )                       ;
     DumpPointer ( "     SubUserData.Ptr     :", sdp->SubUserData.VSPtr )                     ;
     DumpLongDec ( "     SubUserData.Off     :", sdp->SubUserData.VSOffset )                  ;
     DumpLongDec ( "     SubUserData.Size    :", sdp->SubUserData.VSBufSize )                 ;
     DumpLongDec ( "     SubUserData.Len     :", sdp->SubUserData.VSLength )                  ;
     DumpLongDec ( "     SubUserData.CCSID   :", sdp->SubUserData.VSCCSID )                   ;
     DumpBytes   ( "     SubCorrelId         :", sdp->SubCorrelId, sizeof(MQBYTE24) )         ;
     DumpLongDec ( "     PubPriority         :", sdp->PubPriority )                           ;
     DumpBytes   ( "     PubAccountingToken  :", sdp->PubAccountingToken, sizeof(MQBYTE32) )  ;
     DumpString  ( "     PubApplIdentityData :", sdp->PubApplIdentityData, sizeof(MQCHAR32) ) ;
     DumpPointer ( "     SelectionString.Ptr :", sdp->SelectionString.VSPtr )                 ;
     DumpLongDec ( "     SelectionString.Off :", sdp->SelectionString.VSOffset )              ;
     DumpLongDec ( "     SelectionString.Size:", sdp->SelectionString.VSBufSize )             ;
     DumpLongDec ( "     SelectionString.Len :", sdp->SelectionString.VSLength )              ;
     DumpLongDec ( "     SelectionString.CCSI:", sdp->SelectionString.VSCCSID )               ;
     DumpLongDec ( "     SubLevel            :", sdp->SubLevel )                              ;
     DumpPointer ( "     ResObjectString.Ptr :", sdp->ResObjectString.VSPtr )                 ;
     DumpLongDec ( "     ResObjectString.Off :", sdp->ResObjectString.VSOffset )              ;
     DumpLongDec ( "     ResObjectString.Size:", sdp->ResObjectString.VSBufSize )             ;
     DumpLongDec ( "     ResObjectString.Len :", sdp->ResObjectString.VSLength )              ;
     DumpLongDec ( "     ResObjectString.CCSI:", sdp->ResObjectString.VSCCSID )               ;
     printf( "\n") ;
    }
 }
 
 
//
// Internal Functions
//
 
//
// Functions that Fetch and Set REXX Variables in desired format
//
//
// Fetch integer value from REXX function parameter
//
int parm_to_ulong ( RXSTRING   parm    // parameter REXX string
                  , MQLONG *   number  // received value
                  )
{
 MQULONG i        ;
 MQLONG  digit    ;
 MQLONG  parsed = 0 ;

 if ( parm.strlength == 0 ) return -1 ;

 for(i=0; i<parm.strlength; i++)
  {
   if((parm.strptr[i] < '0') || (parm.strptr[i] > '9')) return -1 ;
   digit = parm.strptr[i] - '0' ;
   if ( parsed > (INT32_MAX - digit) / 10 ) return -1 ;
   parsed = parsed * 10 + digit ;
  }

 *number = parsed ;
 return 0 ;
}
 
//
// Fetch MQPTR value from REXX variable (no conversion required!)
//
int var_to_ptr ( MQULONG    traceid      // trace id of caller
               , char     * name         // variable name
               , MQPTR    * anchorptr    // received value
               )
{
 SHVBLOCK                sv1              ;  // REXX var interface CB
 int                     sv1rc            ;  // REXX var interface RC
 MQPTR                   tempptr = 0      ;  // Receive value here
 char                    rawvalue[250U + 1U] ;
 *anchorptr      = NULL                   ;
 memset(rawvalue, 0, sizeof(rawvalue))    ;
 
 sv1.shvnext     = 0                      ; // Fetch only one variable
 sv1.shvcode     = RXSHV_SYFET            ; // Fetch operation
 sv1.shvret      = 0                      ; // Zero out RC
 
 MAKERXSTRING(sv1.shvname,name,strlen(name))  ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength      ; // REXX variable name length
 
 sv1.shvvalue.strptr    = rawvalue                 ; // Set pointer to value buffer for REXX
 sv1.shvvalue.strlength = sizeof(rawvalue)         ; // Set max accepted value length
 sv1.shvvaluelen        = sv1.shvvalue.strlength   ; // Actual length will be here
 
 sv1rc           = RexxVariablePool(&sv1)          ; // Call REXX variable interface
 
 TRACES(("RXfetch rc = %d, shvret = %d, %s length = %"PRIu32"/%u\n",
         sv1rc,sv1.shvret,name,(uint32_t)sv1.shvvalue.strlength,
         (uint32_t)sizeof(rawvalue))
       ) ;
 
 if (    (sv1rc                  == RXSHV_OK)
      && (sv1.shvret             == RXSHV_OK) )
   {
    if ( sv1.shvvalue.strlength != sizeof(MQPTR) )
      return -1 ;
    memcpy(&tempptr, rawvalue, sizeof(tempptr)) ;
    if ( tempptr == NULL )
      return 0 ;
    *anchorptr = tempptr ; // pointer value
    return 1 ;
   }

 if (    (    (sv1rc      == RXSHV_OK)
           || (sv1rc      == RXSHV_NEWV) )
      && (    (sv1.shvret == RXSHV_OK)
           || (sv1.shvret == RXSHV_NEWV) )
      && (    (sv1rc      == RXSHV_NEWV)
           || (sv1.shvret == RXSHV_NEWV) ) )
   return 0 ;

 return -1 ;
} // End of var_to_ptr
 
//
//
// Safely construct a REXX variable name.
//
#define RXMQ_REXX_VARNAME_MAX  250U
#define RXMQ_REXX_VARNAME_SIZE (RXMQ_REXX_VARNAME_MAX + 1U)
//
static int make_rexx_varname ( MQULONG     traceid
                             , char        varnamc[]
                             , const char * prefix
                             , size_t       prefixlen
                             , const char * name
                             , const char * suffix
                             )
{
 size_t namelen   ;
 size_t suffixlen ;

 namelen   = strlen(name)   ;
 suffixlen = strlen(suffix) ;

 if (   (prefixlen > RXMQ_REXX_VARNAME_MAX)
     || (namelen > (RXMQ_REXX_VARNAME_MAX - prefixlen))
     || (suffixlen > (RXMQ_REXX_VARNAME_MAX - prefixlen - namelen)) )
   {
    TRACE(traceid, ("REXX variable name exceeds 250 bytes\n") ) ;
    varnamc[0] = 0 ;
    return -1 ;
   }

 if (prefixlen != 0)
   memcpy(varnamc, prefix, prefixlen) ;

 memcpy(varnamc + prefixlen, name, namelen) ;
 memcpy(varnamc + prefixlen + namelen, suffix, suffixlen + 1U) ;

 return 0 ;
} // End of make_rexx_varname
//
// Fetch MQLONG value from REXX stem variable
//
void stem_to_long ( MQULONG    traceid      // trace id of caller
                  , RXSTRING   stem         // stem variable name high
                  , char       name[]       // stem variable name low
                  , MQLONG   * number       // received value
                  )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;  // Char version of variable name
 char                    varvalc[100] ;  // Char version of variable value
 SHVBLOCK                sv1          ;  // REXX var interface CB
 int                     sv1rc        ;  // REXX var interface RC
 intmax_t                parsedNumber = 0 ;
 char                  * endptr       = NULL ;
 
 memset(&varvalc,0,sizeof(varvalc))   ; // Clear REXX variable value buffer
 sv1.shvnext     = 0                  ; // Fetch only one variable
 sv1.shvcode     = RXSHV_SYFET        ; // Fetch operation
 sv1.shvret      = 0                  ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))              ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                        ; // REXX variable name length
 
 sv1.shvvalue.strptr    = varvalc                   ; // Set pointer to value buffer for REXX
 sv1.shvvalue.strlength = sizeof(varvalc) - 1U           ; // Set max accepted value length
 sv1.shvvaluelen        = sv1.shvvalue.strlength    ; // Actual length will be here
 
 sv1rc           = RexxVariablePool(&sv1)           ; // Call REXX variable interface
 
 TRACE(traceid, ("RXfetch rc = %d, %s ->%s<-%"PRIu32"/%"PRIu32"\n",
                 sv1rc,varnamc,varvalc,(uint32_t)sv1.shvvaluelen,(uint32_t)sizeof(varvalc))
      ) ;
 
 if (    (sv1rc                       == RXSHV_OK)
      && (sv1.shvret                  == RXSHV_OK)
      && (strlen(varvalc)             != 0       )
      && (strlen(varvalc)             == sv1.shvvalue.strlength) )
   {
    errno  = 0 ;
    endptr = NULL ;
    parsedNumber = strtoimax(varvalc, &endptr, 10) ;
    while (    (endptr != NULL)
            && (*endptr != 0)
            && isspace((unsigned char)*endptr) )
      endptr++ ;
    if (    (endptr != NULL)
         && (endptr != varvalc)
         && (errno != ERANGE)
         && (*endptr == 0)
         && (parsedNumber >= INT32_MIN)
         && (parsedNumber <= INT32_MAX) )
      *number = (MQLONG)parsedNumber ;
   }
 
 return ;
} // End of stem_to_long
 
//
// Fetch MQINT64 value from REXX stem variable
//
void stem_to_int64 ( MQULONG    traceid      // trace id of caller
                   , RXSTRING   stem         // stem variable name high
                   , char       name[]       // stem variable name low
                   , MQINT64 *  number       // received value
                   )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;  // Char version of variable name
 char                    varvalc[100] ;  // Char version of variable value
 SHVBLOCK                sv1          ;  // REXX var interface CB
 int                     sv1rc        ;  // REXX var interface RC
 intmax_t                parsedNumber = 0 ;
 char                  * endptr       = NULL ;
 
 memset(&varvalc,0,sizeof(varvalc))   ; // Clear REXX variable value buffer
 sv1.shvnext     = 0                  ; // Fetch only one variable
 sv1.shvcode     = RXSHV_SYFET        ; // Fetch operation
 sv1.shvret      = 0                  ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))                 ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                           ; // REXX variable name length
 
 sv1.shvvalue.strptr    = varvalc                   ; // Set pointer to value buffer for REXX
 sv1.shvvalue.strlength = sizeof(varvalc) - 1U           ; // Set max accepted value length
 sv1.shvvaluelen        = sv1.shvvalue.strlength    ; // Actual length will be here
 
 sv1rc           = RexxVariablePool(&sv1)           ; // Call REXX variable interface
 
 TRACE(traceid, ("RXfetch rc = %d, %s ->%s<-%"PRIu32"/%"PRIu32"\n",
                 sv1rc,varnamc,varvalc,(uint32_t)sv1.shvvaluelen,(uint32_t)sizeof(varvalc))
                ) ;
 
if (    (sv1rc           == RXSHV_OK)
     && (sv1.shvret      == RXSHV_OK)
     && (strlen(varvalc) != 0)
     && (strlen(varvalc) == sv1.shvvalue.strlength) )
  {
   errno  = 0 ;
   endptr = NULL ;
   parsedNumber = strtoimax(varvalc, &endptr, 10) ;
   while (    (endptr != NULL)
           && (*endptr != 0)
           && isspace((unsigned char)*endptr) )
     endptr++ ;
   if (    (endptr != NULL)
        && (endptr != varvalc)
        && (errno != ERANGE)
        && (*endptr == 0)
        && (parsedNumber >= INT64_MIN)
        && (parsedNumber <= INT64_MAX) )
     *number = (MQINT64)parsedNumber ;
  }
 return ;
} // End of stem_to_int64
 
//
// Fetch single MQCHAR value from REXX variable
//
void stem_to_char ( MQULONG    traceid      // trace id of caller
                  , RXSTRING   stem         // stem variable name high
                  , char       name[]       // stem variable name low
                  , MQCHAR *   letter       // received value
                  )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;  // Char version of variable name
 char                    varvalc[100] ;  // Char version of variable value
 SHVBLOCK                sv1          ;  // REXX var interface CB
 int                     sv1rc        ;  // REXX var interface RC
 
 memset(&varvalc,0,sizeof(varvalc))   ; // Clear REXX variable value buffer
 sv1.shvnext     = 0                  ; // Fetch only one variable
 sv1.shvcode     = RXSHV_SYFET        ; // Fetch operation
 sv1.shvret      = 0                  ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))        ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                  ; // REXX variable name length
 
 sv1.shvvalue.strptr    = varvalc                         ; // Set pointer to value buffer for REXX
 sv1.shvvalue.strlength = sizeof(varvalc) - 1U                 ; // Set max accepted value length
 sv1.shvvaluelen        = sv1.shvvalue.strlength          ; // Actual length will be here
 
 sv1rc           = RexxVariablePool(&sv1)                 ; // Call REXX variable interface
 
 TRACE(traceid, ("RXfetch rc = %d, %s ->%s<-%"PRIu32"/%"PRIu32"\n",
                 sv1rc,varnamc,varvalc,(uint32_t)sv1.shvvaluelen,(uint32_t)sizeof(varvalc))
      ) ;
 
 if (    (sv1rc                          == RXSHV_OK)
         && (sv1.shvret                  == RXSHV_OK)
         && (strlen(varvalc)             != 0       ))
         *letter = varvalc[0]                         ; // char value
 
 return ;
} // End of stem_to_char
 
 
//
// Fetch string MQCHAR value from REXX variable
//
void stem_to_string ( MQULONG    traceid      // trace id of caller
                    , RXSTRING   stem         // stem variable name high
                    , char       name[]       // stem variable name low
                    , MQCHAR     string[]     // received value
                    , int        size         // max size of value
                    )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;  // Char version of variable name
 char                    varvalc[100] ;  // Char version of variable value
 SHVBLOCK                sv1          ;  // REXX var interface CB
 int                     sv1rc        ;  // REXX var interface RC
 size_t                  stringlen    ;
 
 if (    (size <= 0)
      || (size > (int) sizeof(varvalc)) )   // should never happen
   {
    TRACE(traceid, ("Size = %d more than buffer length %"PRIu32" !\n",
                    size,(uint32_t)sizeof(varvalc))
         ) ;
    return ;
   }
 memset(&varvalc,0,sizeof(varvalc))       ; // To ensure value is 0-terminated
 
 sv1.shvnext     = 0                      ; // Fetch only one variable
 sv1.shvcode     = RXSHV_SYFET            ; // Fetch operation
 sv1.shvret      = 0                      ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))              ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                        ; // REXX variable name length
 
 sv1.shvvalue.strptr    = varvalc                ; // Set pointer to value buffer for REXX
 if ( size == (int) sizeof(varvalc) )
   sv1.shvvalue.strlength = sizeof(varvalc) - 1U ;
 else
   sv1.shvvalue.strlength = size                   ; // Set max accepted value length
 sv1.shvvaluelen        = sv1.shvvalue.strlength ; // Actual length will be here
 
 sv1rc           = RexxVariablePool(&sv1)        ; // Call REXX variable interface
 
 TRACE(traceid, ("RXfetch rc = %d, %s ->%s<-%"PRIu32"/%d\n",
                  sv1rc,varnamc,varvalc,(uint32_t)sv1.shvvaluelen,size)
      ) ;
 
 if (    (sv1rc      == RXSHV_OK)
      && (sv1.shvret == RXSHV_OK) )
   {
    stringlen = strlen(varvalc) ;

    memcpy(string, varvalc, stringlen) ;

    if ( stringlen < (size_t) size )
      string[stringlen] = 0 ;
   }
 
 return ;
} // End of stem_to_string
 
//
// Fetch MQBYTE value from REXX variable
// Use this only for MQBYTExx variables
//
void stem_to_bytes  ( MQULONG    traceid      // trace id of caller
                    , RXSTRING   stem         // stem variable name high
                    , char       name[]   // stem variable name low
                    , MQBYTE     string[] // received value
                    , int        size         // max size of value
                    )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;  // Char version of variable name
 MQBYTE                  varvalc[100] ;  // Char version of variable value
 SHVBLOCK                sv1          ;  // REXX var interface CB
 int                     sv1rc        ;  // REXX var interface RC
 
 if (    (size <= 0)
      || (size > (int) sizeof(varvalc)) )   // should never happen
   {
   TRACE(traceid, ("Size = %d more than buffer length %"PRIu32" !\n",
                   size,(uint32_t)sizeof(varvalc))
        ) ;
    return ;
   }
 memset(&varvalc,0,sizeof(varvalc))   ; // For readability only
 
 sv1.shvnext     = 0                  ; // Fetch only one variable
 sv1.shvcode     = RXSHV_SYFET        ; // Fetch operation
 sv1.shvret      = 0                  ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))              ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                        ; // REXX variable name length
 
 sv1.shvvalue.strptr    = (char *) varvalc       ; // Set pointer to value buffer for REXX
 sv1.shvvalue.strlength = size                   ; // Set max accepted value length
 sv1.shvvaluelen        = sv1.shvvalue.strlength ; // Actual length will be here
 
 sv1rc           = RexxVariablePool(&sv1)        ; // Call REXX variable interface
 
 TRACE(traceid, ("RXfetch rc = %d, %s ->",sv1rc,varnamc) )               ;
 TRACX(traceid, (varvalc,sv1.shvvaluelen) )                              ;
 TRACE(traceid, ("<-%"PRIu32"/%d\n",(uint32_t)sv1.shvvaluelen,size) )    ;
 
 if (    (sv1rc      == RXSHV_OK)
      && (sv1.shvret == RXSHV_OK) )
   memcpy(string, &varvalc, sv1.shvvalue.strlength) ;
                                                   // Return value, only if OK!
 return ;
} // End of stem_to_bytes
 
//
// Fetch long MQBYTE value from REXX variable
// Buffer is provided by the caller
//
MQULONG stem_to_data ( MQULONG    traceid      // trace id of caller
                     , RXSTRING   stem         // stem variable name high
                     , char       name[]       // stem variable name low
                     , MQBYTE     string[]     // received value
                     , int        size         // max size of value
                     )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;  // Char version of variable name
 SHVBLOCK                sv1              ;  // REXX var interface CB
 int                     sv1rc            ;  // REXX var interface RC
 
 if ( size <= 0 )
   return 0 ;

 sv1.shvnext     = 0                      ; // Fetch only one variable
 sv1.shvcode     = RXSHV_SYFET            ; // Fetch operation
 sv1.shvret      = 0                      ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return 0 ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))              ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                        ; // REXX variable name length
 
 sv1.shvvalue.strptr    = (char *) string        ; // Set pointer to value buffer for REXX
 sv1.shvvalue.strlength = size                   ; // Set max accepted value length
 sv1.shvvaluelen        = sv1.shvvalue.strlength ; // Actual length will be here
 
 sv1rc           = RexxVariablePool(&sv1)        ; // Call REXX variable interface
 
 TRACE(traceid, ("RXfetch rc = %d, %s ->",sv1rc,varnamc) )    ;
 TRACX(traceid, (string,sv1.shvvaluelen) )                     ;
 TRACE(traceid, ("<-%"PRIu32"/%d\n",(uint32_t)sv1.shvvaluelen,size) ) ;
 
 if (    (sv1rc      == RXSHV_OK)
      && (sv1.shvret == RXSHV_OK) )
   return (sv1.shvvalue.strlength)    ;
 else
   return 0                           ;
} // End of stem_to_data
 
//
// Fetch MQCHARV value from REXX variable
//
static void free_mqcharv ( MQCHARV * value )
{
 if ( value->VSPtr != NULL )
   free(value->VSPtr) ;

 value->VSPtr    = NULL ;
 value->VSOffset = 0    ;
 value->VSLength = 0    ;
 value->VSBufSize = 0   ;

 return ;
}

static void free_od_mqcharv ( MQOD * od )
{
 free_mqcharv(&od->ObjectString)    ;
 free_mqcharv(&od->SelectionString) ;
 free_mqcharv(&od->ResObjectString) ;

 return ;
}

static void free_sd_mqcharv ( MQSD * sd )
{
 free_mqcharv(&sd->ObjectString)    ;
 free_mqcharv(&sd->SubName)         ;
 free_mqcharv(&sd->SubUserData)     ;
 free_mqcharv(&sd->SelectionString) ;
 free_mqcharv(&sd->ResObjectString) ;

 return ;
}

typedef enum _RXMQ_MQCHARV_MODE {
    RXMQ_MQCHARV_INPUT_ONLY,
    RXMQ_MQCHARV_CAPACITY_REQUIRED
} RXMQ_MQCHARV_MODE ;

static int fetch_mqcharv_rvp ( MQULONG    traceid
                             , RXSTRING   stem
                             , char       name[]
                             , char     * value
                             , size_t     capacity
                             , size_t   * valuelen
                             )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;
 SHVBLOCK                sv1                              ;
 int                     sv1rc                            ;
 unsigned int            fetchflags                       ;

 *valuelen = 0 ;

 sv1.shvnext     = 0           ;
 sv1.shvcode     = RXSHV_SYFET ;
 sv1.shvret      = 0           ;

 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return -1 ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc)) ;
 sv1.shvnamelen          = sv1.shvname.strlength  ;
 sv1.shvvalue.strptr     = value                  ;
 sv1.shvvalue.strlength  = capacity               ;
 sv1.shvvaluelen         = sv1.shvvalue.strlength ;

 sv1rc = RexxVariablePool(&sv1) ;

 TRACE(traceid, ("RXfetch rc = %d, shvret = %d, %s length = %"PRIu32"/%"PRIu32"\n",
                  sv1rc,sv1.shvret,varnamc,
                  (uint32_t)sv1.shvvalue.strlength,(uint32_t)capacity) ) ;

 fetchflags = (unsigned int)sv1rc | (unsigned int)sv1.shvret ;

 if ( fetchflags == (unsigned int)RXSHV_OK )
   {
    if ( sv1.shvvalue.strlength > capacity )
      return -1 ;
    *valuelen = sv1.shvvalue.strlength ;
    TRACX(traceid, ((MQBYTE *)value,*valuelen) ) ;
    return 0 ;
   }

 if (    (fetchflags & (unsigned int)RXSHV_NEWV)
      && ((fetchflags & ~((unsigned int)RXSHV_NEWV |
                          (unsigned int)RXSHV_TRUNC)) == 0U) )
   return 1 ;

 if (    (fetchflags & (unsigned int)RXSHV_TRUNC)
      && ((fetchflags & ~(unsigned int)RXSHV_TRUNC) == 0U) )
   return 2 ;

 return -1 ;
}

typedef enum _RXMQ_EXACT_FETCH_RESULT {
 RXMQ_EXACT_FETCH_SUCCESS, RXMQ_EXACT_FETCH_INVALID,
 RXMQ_EXACT_FETCH_NOMEM } RXMQ_EXACT_FETCH_RESULT ;

static RXMQ_EXACT_FETCH_RESULT fetch_exact_rexx_bytes ( MQULONG    traceid
                                                      , RXSTRING   stem
                                                      , char       name[]
                                                      , MQLONG     expected
                                                      , MQBYTE  ** result )
{
 char                    probe[RXMQ_REXX_VARNAME_SIZE] ;
 int                     fetchrc, allocrc               ;
 size_t                  valuelen = 0, maximum          ;
 size_t                  capacity, newcapacity          ;
 MQBYTE                * buffer = NULL, * newbuffer = NULL ;

 if ( result == NULL ) return RXMQ_EXACT_FETCH_INVALID ;
 *result = NULL ;
 if ( expected <= 0 ) return RXMQ_EXACT_FETCH_INVALID ;
 maximum = (size_t)expected ;
 fetchrc = fetch_mqcharv_rvp(traceid, stem, name, probe,
                             sizeof(probe), &valuelen) ;
 if ( fetchrc == 0 )
   {
    if ( valuelen != maximum ) return RXMQ_EXACT_FETCH_INVALID ;
    buffer = (MQBYTE *)malloc(maximum) ;
    if ( buffer == NULL ) return RXMQ_EXACT_FETCH_NOMEM ;
    memcpy(buffer, probe, valuelen) ;
    *result = buffer                ;
    return RXMQ_EXACT_FETCH_SUCCESS ;
   }
 if ( (fetchrc != 2) || (maximum <= sizeof(probe)) )
   return RXMQ_EXACT_FETCH_INVALID ;
 capacity = maximum < (2U * sizeof(probe))
          ? maximum : 2U * sizeof(probe) ;
 buffer = (MQBYTE *)malloc(capacity) ;
 if ( buffer == NULL ) return RXMQ_EXACT_FETCH_NOMEM ;
 for ( ; ; )
   {
    fetchrc = fetch_mqcharv_rvp(traceid, stem, name,
                                (char *)buffer, capacity, &valuelen) ;
    if ( fetchrc == 0 )
      {
       if ( valuelen == maximum )
         {
          *result = buffer                ;
          return RXMQ_EXACT_FETCH_SUCCESS ;
         }
       free(buffer) ;
       return RXMQ_EXACT_FETCH_INVALID ;
      }
    if ( fetchrc != 2 )
      {
       free(buffer) ;
       return RXMQ_EXACT_FETCH_INVALID ;
      }
    if ( capacity == maximum )
      {
       free(buffer) ;
       return RXMQ_EXACT_FETCH_INVALID ;
      }
    newcapacity = capacity > (maximum / 2U)
                ? maximum : capacity * 2U ;
    if (    (newcapacity <= capacity)
         || (newcapacity > maximum) )
      {
       free(buffer) ;
       return RXMQ_EXACT_FETCH_INVALID ;
      }
    newbuffer = (MQBYTE *)realloc(buffer, newcapacity) ;
    if ( newbuffer == NULL )
      {
       allocrc = errno ;
       free(buffer) ;
       errno = allocrc ;
       return RXMQ_EXACT_FETCH_NOMEM ;
      }
    buffer   = newbuffer   ;
    capacity = newcapacity ;
   }
}

int stem_to_strinv ( MQULONG    traceid     // trace id of caller
                    , RXSTRING   stem        // stem variable name high
                    , char       name[]  // stem variable name low
                    , MQCHARV *  string      // received value
                    , RXMQ_MQCHARV_MODE mode // input or output-capable value
                    )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;  // Char version of variable name
 char                    varvalc[100] ;
 char                    probe[RXMQ_REXX_VARNAME_SIZE] ;
 RXSTRING                varname          ;  // REXX variable name
 int                     fetchrc          ;
 int                     sizepresent = 0  ;
 size_t                  valuelen = 0     ;
 size_t                  maximum = 0     ;
 size_t                  capacity = 0     ;
 size_t                  newcapacity = 0  ;
 void                  * newptr = NULL    ;
 intmax_t                parsedNumber = 0 ;
 char                  * endptr = NULL    ;

 if (    (mode != RXMQ_MQCHARV_INPUT_ONLY)
      && (mode != RXMQ_MQCHARV_CAPACITY_REQUIRED) )
   return -1 ;

 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return -1 ;
 MAKERXSTRING(varname,varnamc,strlen(varnamc))                  ; // Construct REXX variable name structure

 memset(&varvalc,0,sizeof(varvalc)) ;
 fetchrc = fetch_mqcharv_rvp(traceid, varname, ".0", varvalc,
                             sizeof(varvalc) - 1U, &valuelen) ;
 if (    (fetchrc < 0)
      || (fetchrc == 2) )
   return -1 ;
 if ( fetchrc == 0 )
   {
    if (    (valuelen == 0)
         || (strlen(varvalc) != valuelen) )
      return -1 ;

    errno  = 0 ;
    endptr = NULL ;
    parsedNumber = strtoimax(varvalc, &endptr, 10) ;
    while (    (endptr != NULL)
            && (*endptr != 0)
            && isspace((unsigned char)*endptr) )
      endptr++ ;
    if (    (endptr == NULL)
         || (endptr == varvalc)
         || (errno == ERANGE)
         || (*endptr != 0)
         || (parsedNumber < 0)
         || (parsedNumber > INT32_MAX) )
      return -1 ;

    string->VSPtr     = NULL                 ;
    string->VSOffset  = 0                    ;
    string->VSLength  = 0                    ;
    string->VSBufSize = (MQLONG)parsedNumber ;
    sizepresent = 1 ;
   }

 memset(&varvalc,0,sizeof(varvalc)) ;
 fetchrc = fetch_mqcharv_rvp(traceid, varname, ".CCSI", varvalc,
                             sizeof(varvalc) - 1U, &valuelen) ;
 if (    (fetchrc < 0)
      || (fetchrc == 2) )
   return -1 ;
 if ( fetchrc == 0 )
   {
    if (    (valuelen == 0)
         || (strlen(varvalc) != valuelen) )
      return -1 ;

    errno  = 0 ;
    endptr = NULL ;
    parsedNumber = strtoimax(varvalc, &endptr, 10) ;
    while (    (endptr != NULL)
            && (*endptr != 0)
            && isspace((unsigned char)*endptr) )
      endptr++ ;
    if (    (endptr == NULL)
         || (endptr == varvalc)
         || (errno == ERANGE)
         || (*endptr != 0)
         || (parsedNumber < INT32_MIN)
         || (parsedNumber > INT32_MAX) )
      return -1 ;

    string->VSCCSID = (MQLONG)parsedNumber ;
   }

 if (    (sizepresent == 0)
      || (string->VSBufSize == 0) )
   return 0 ;

 maximum = (size_t)string->VSBufSize ;

 if ( mode == RXMQ_MQCHARV_INPUT_ONLY )
   {
    fetchrc = fetch_mqcharv_rvp(traceid, varname, ".1", probe,
                                sizeof(probe), &valuelen) ;
    if ( fetchrc < 0 )
      return -1 ;
    if ( fetchrc == 1 )
      {
       free_mqcharv(string) ;
       return 0 ;
      }
    if ( fetchrc == 0 )
      {
       if ( valuelen > maximum )
         return -1 ;
       if ( valuelen == 0 )
         {
          free_mqcharv(string) ;
          return 0 ;
         }
       string->VSPtr = malloc(valuelen) ;
       if ( string->VSPtr == NULL )
         {
          TRACE(traceid, ("malloc rc %d\n",errno) ) ;
          free_mqcharv(string) ;
          return -1 ;
         }
       string->VSBufSize = (MQLONG)valuelen ;
       memcpy(string->VSPtr, probe, valuelen) ;
       string->VSLength = (MQLONG)valuelen ;
       return 0 ;
      }

    if ( maximum <= sizeof(probe) )
      return -1 ;

    if ( maximum < (2U * sizeof(probe)) )
      capacity = maximum ;
    else
      capacity = 2U * sizeof(probe) ;

    string->VSPtr = malloc(capacity) ;
    if ( string->VSPtr == NULL )
      {
       TRACE(traceid, ("malloc rc %d\n",errno) ) ;
       free_mqcharv(string) ;
       return -1 ;
      }
    string->VSBufSize = (MQLONG)capacity ;

    for ( ; ; )
      {
       fetchrc = fetch_mqcharv_rvp(traceid, varname, ".1",
                                   (char *)string->VSPtr,
                                   capacity, &valuelen) ;
       if ( fetchrc < 0 )
         {
          free_mqcharv(string) ;
          return -1 ;
         }
       if ( fetchrc == 1 )
         {
          free_mqcharv(string) ;
          return 0 ;
         }
       if ( fetchrc == 0 )
         {
          if (    (valuelen > capacity)
               || (valuelen > maximum) )
            {
             free_mqcharv(string) ;
             return -1 ;
            }
          if ( valuelen == 0 )
            {
             free_mqcharv(string) ;
             return 0 ;
            }
          string->VSLength = (MQLONG)valuelen ;
          return 0 ;
         }

       if ( capacity == maximum )
         {
          free_mqcharv(string) ;
          return -1 ;
         }
       if ( capacity > (maximum / 2U) )
         newcapacity = maximum ;
       else
         newcapacity = capacity * 2U ;
       if ( newcapacity <= capacity )
         {
          free_mqcharv(string) ;
          return -1 ;
         }
       newptr = realloc(string->VSPtr, newcapacity) ;
       if ( newptr == NULL )
         {
          TRACE(traceid, ("malloc rc %d\n",errno) ) ;
          free_mqcharv(string) ;
          return -1 ;
         }
       string->VSPtr = newptr ;
       capacity = newcapacity ;
       string->VSBufSize = (MQLONG)capacity ;
      }
   }

 TRACE(traceid, ("Doing malloc for %"PRId32" bytes",(int32_t)string->VSBufSize))  ;
 string->VSPtr = malloc((size_t)string->VSBufSize)                    ;
 if ( string->VSPtr == NULL )
   {
    TRACE(traceid, ("malloc rc %d\n",errno) )                         ;
    free_mqcharv(string)                                               ;
    return -1                                                          ;
   }

 fetchrc = fetch_mqcharv_rvp(traceid, varname, ".1",
                             (char *)string->VSPtr,
                             (size_t)string->VSBufSize, &valuelen) ;
 if (    (fetchrc < 0)
      || (fetchrc == 2)
      || (valuelen > (size_t)string->VSBufSize) )
   {
    TRACE(traceid, ("Unable to fetch MQCHARV data rc = %d\n",fetchrc) ) ;
    free_mqcharv(string) ;
    return -1 ;
   }

 string->VSLength = (MQLONG)valuelen ;

 return 0 ;
} // End of stem_to_strinv
 
//
// Set REXX variable to MQPTR value (no conversion required!)
//
int var_from_ptr ( MQULONG    traceid      // trace id of caller
                   , char    * name         // variable name
                   , MQPTR     anchor       // value to set
                   )
{
 SHVBLOCK                sv1              ;  // REXX var interface CB
 int                     sv1rc            ;  // REXX var interface RC
 
 sv1.shvnext     = 0                      ; // Set only one variable
 sv1.shvcode     = RXSHV_SYSET            ; // Set operation
 sv1.shvret      = 0                      ; // Zero out RC
 
 MAKERXSTRING(sv1.shvname,name,strlen(name))        ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength            ; // REXX variable name length
 
 MAKERXSTRING(sv1.shvvalue,(char*)&anchor,sizeof(anchor))  ; // Construct REXX variable value structure
 sv1.shvvaluelen = sv1.shvvalue.strlength           ; // Set actual value length for REXX
 
 sv1rc           = RexxVariablePool(&sv1)           ; // Call REXX variable interface
 
 TRACES(("RXset rc = %d, %s ->%p<-%ld/%"PRId32"\n",
         sv1rc,name,anchor,sv1.shvvaluelen,(int32_t)sizeof(anchor))
      ) ;
 
 if (    (sv1rc != RXSHV_OK)
      && (sv1rc != RXSHV_NEWV) )
   return sv1rc ;

 if (    (sv1.shvret != RXSHV_OK)
      && (sv1.shvret != RXSHV_NEWV) )
   return sv1.shvret ;

 if ( sv1rc == RXSHV_NEWV )
   return sv1rc ;

 return sv1.shvret ;
} // End of var_from_ptr
 
//
// Set REXX variable to MQLONG value
//
int stem_from_long ( MQULONG    traceid  // trace id of caller
                    , char       zlist[]  // .ZLIST string for accumulation
                    , RXSTRING   stem     // stem variable name high
                    , char       name[]   // stem variable name low
                    , int32_t    number   // value to set
                    )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ; // Char version of variable name
 char                    varvalc[100] ; // Char version of variable value
 SHVBLOCK                sv1          ; // REXX var interface CB
 int                     sv1rc        ; // REXX var interface RC
 
 sv1.shvnext     = 0                  ; // Set only one variable
 sv1.shvcode     = RXSHV_SYSET        ; // Set operation
 sv1.shvret      = 0                  ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return -1 ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))              ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                        ; // REXX variable name length
 
 sprintf(varvalc,"%"PRId32,number)                   ; // Convert long int to char
 MAKERXSTRING(sv1.shvvalue,varvalc,strlen(varvalc))  ; // Construct REXX variable value structure
 sv1.shvvaluelen = sv1.shvvalue.strlength            ; // Set actual value length for REXX
 
 sv1rc           = RexxVariablePool(&sv1)            ; // Call REXX variable interface
 
 TRACE(traceid, ("RXset rc = %d, %s ->%s<-%"PRIu32"/%"PRIu32"\n",
                  sv1rc,varnamc,varvalc,(uint32_t)sv1.shvvaluelen,(uint32_t)strlen(varvalc))
      ) ;
 
 if (   ( (sv1rc == RXSHV_OK) || (sv1rc == RXSHV_NEWV) )
     && (zlist != NULL))
   {
     strcat(zlist," ")        ;
     strcat(zlist,name)       ;
   }
 return sv1rc ;
} // End of stem_from_long
 
//
// Set REXX variable to MQINT64 value
//
int stem_from_int64 ( MQULONG    traceid  // trace id of caller
                    , char       zlist[]  // .ZLIST string for accumulation
                    , RXSTRING   stem     // stem variable name high
                    , char       name[]   // stem variable name low
                    , MQINT64    number   // value to set
                    )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;  // Char version of variable name
 char                    varvalc[100] ;  // Char version of variable value
 SHVBLOCK                sv1          ;  // REXX var interface CB
 int                     sv1rc        ;  // REXX var interface RC
 
 sv1.shvnext     = 0                  ; // Set only one variable
 sv1.shvcode     = RXSHV_SYSET        ; // Set operation
 sv1.shvret      = 0                  ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return -1 ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))              ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                        ; // REXX variable name length
 
 sprintf((char *)varvalc,"%"PRId64,(int64_t)number) ;
 MAKERXSTRING(sv1.shvvalue,varvalc,strlen(varvalc))  ; // Construct REXX variable value structure
 sv1.shvvaluelen = sv1.shvvalue.strlength            ; // Set actual value length for REXX
 
 sv1rc           = RexxVariablePool(&sv1)            ; // Call REXX variable interface
 
 TRACE(traceid, ("RXset rc = %d, shvret = %d, %s ->%s<-%"PRIu32"/%"PRIu32"\n",
                 sv1rc,sv1.shvret,varnamc,varvalc,
                 (uint32_t)sv1.shvvaluelen,(uint32_t)strlen(varvalc))
      ) ;

 if (   ((sv1rc      == RXSHV_OK) || (sv1rc      == RXSHV_NEWV))
     && ((sv1.shvret == RXSHV_OK) || (sv1.shvret == RXSHV_NEWV))
     && (zlist != NULL))
 {
     strcat(zlist," ")        ;
     strcat(zlist,name)       ;
 }
 return sv1rc ;
} // End of stem_from_int64
 
//
// Set REXX variable to single MQCHAR value
//
int stem_from_char ( MQULONG    traceid  // trace id of caller
                    , char       zlist[]  // .ZLIST string for accumulation
                    , RXSTRING   stem     // stem variable name high
                    , char       name[]   // stem variable name low
                    , MQCHAR     letter   // value to set
                    )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;  // Char version of variable name
 SHVBLOCK                sv1          ;  // REXX var interface CB
 int                     sv1rc        ;  // REXX var interface RC
 
 sv1.shvnext     = 0           ; // Set only one variable
 sv1.shvcode     = RXSHV_SYSET ; // Set operation
 sv1.shvret      = 0           ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return -1 ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))              ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                        ; // REXX variable name length
 
 MAKERXSTRING(sv1.shvvalue,&letter,sizeof(MQCHAR))  ; // Construct REXX variable value structure
 sv1.shvvaluelen = sv1.shvvalue.strlength           ; // Set actual value length for REXX
 
 sv1rc           = RexxVariablePool(&sv1)           ; // Call REXX variable interface
 
 TRACE(traceid, ("RXset rc = %d, %s ->%c<-%"PRIu32"/%"PRIu32"\n",
                 sv1rc,varnamc,letter,(uint32_t)sv1.shvvaluelen,(uint32_t)sizeof(MQCHAR))
      ) ;
 
 if (   ((sv1rc == RXSHV_OK) || (sv1rc == RXSHV_NEWV))
     && (zlist != NULL))
 {
     strcat(zlist," ")        ;
     strcat(zlist,name)       ;
 }
 
 return sv1rc ;
} // End of stem_from_char
 
//
// Set REXX variable to MQCHAR string value
//
int stem_from_string ( MQULONG    traceid  // trace id of caller
                      , char       zlist[]  // .ZLIST string for accumulation
                      , RXSTRING   stem     // stem variable name high
                      , char       name[]   // stem variable name low
                      , MQCHAR     string[] // value to set
                      , int        size     // full size of value
                      )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE] ;  // Char version of variable name
 SHVBLOCK                sv1          ;  // REXX var interface CB
 int                     sv1rc        ;  // REXX var interface RC
 
 sv1.shvnext     = 0                  ; // Set only one variable
 sv1.shvcode     = RXSHV_SYSET        ; // Set operation
 sv1.shvret      = 0                  ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return -1 ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))              ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                        ; // REXX variable name length
 
 if ( memchr(string,0,size) != NULL ) sv1.shvvaluelen = strlen(string) ; // If null-terminated
 else sv1.shvvaluelen = size                                           ; // else take full string
 MAKERXSTRING(sv1.shvvalue,string,sv1.shvvaluelen)   ; // Construct REXX variable value structure
 
 sv1rc           = RexxVariablePool(&sv1)            ; // Call REXX variable interface
 
 TRACE(traceid, ("RXset rc = %d, %s ->%.*s<-%"PRIu32"/%d\n",
                 sv1rc,varnamc,size,string,(uint32_t)sv1.shvvaluelen,size)
      ) ;
 
 if (   ((sv1rc == RXSHV_OK) || (sv1rc == RXSHV_NEWV))
     && (zlist != NULL))
 {
     strcat(zlist," ")        ;
     strcat(zlist,name)       ;
 }
 
 return sv1rc ;
} // End of stem_from_string
 
//
// Set REXX variable to MQBYTE string value
//
int stem_from_bytes  ( MQULONG    traceid   // trace id of caller
                      , char       zlist[]   // .ZLIST string for accumulation
                      , RXSTRING   stem      // stem variable name high
                      , char     * name      // stem variable name low
                      , MQBYTE   * string    // value to set
                      , int        size      // full size of value
                      )
{
 char                    varnamc[RXMQ_REXX_VARNAME_SIZE]     ;  // Char version of variable name
 SHVBLOCK                sv1              ;  // REXX var interface CB
 int                     sv1rc            ;  // REXX var interface RC
 
 sv1.shvnext     = 0                      ; // Set only one variable
 sv1.shvcode     = RXSHV_SYSET            ; // Set operation
 sv1.shvret      = 0                      ; // Zero out RC
 
 if (make_rexx_varname(traceid, varnamc, stem.strptr,
                       (size_t)stem.strlength, name, "") != 0)
   return -1 ;
 MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))              ; // Construct REXX variable name structure
 sv1.shvnamelen  = sv1.shvname.strlength                        ; // REXX variable name length
 
 MAKERXSTRING(sv1.shvvalue,(char*)string,size)                  ; // Construct REXX variable value structure
 sv1.shvvaluelen = sv1.shvvalue.strlength                       ; // Set actual value length for REXX
 
 sv1rc           = RexxVariablePool(&sv1)                       ; // Call REXX variable interface
 
 TRACE(traceid, ("RXset rc = %d, %s ->",sv1rc,varnamc) )       ;
 TRACX(traceid, (string,sv1.shvvaluelen) )                      ;
 TRACE(traceid, ("<-%"PRIu32"/%d\n",(uint32_t)sv1.shvvaluelen,size) ) ;
 
 if (   ((sv1rc == RXSHV_OK) || (sv1rc == RXSHV_NEWV))
     && (zlist != NULL))
 {
     strcat(zlist," ")                      ;
     strcat(zlist,(const char *)name)       ;
 }
 
 return sv1rc ;
} // End of stem_from_bytes
 
//
// Set REXX variable to MQCHARV value
//
int stem_from_strinv ( MQULONG    traceid      // trace id of caller
                      , char       zlist[]      // .ZLIST string for accumulation
                      , RXSTRING   stem         // stem variable name high
                      , char       name[]       // stem variable name low
                      , MQCHARV  * string       // value to set
                      )
{
 int                    rc = RXSHV_OK ;
 int                    rexxrc = RXSHV_OK ;
#ifdef __MVS__
 size_t                 publishLength = 0 ;
#endif
 char                   varnamc[RXMQ_REXX_VARNAME_SIZE] ;      // Char version of variable name

 if (make_rexx_varname(traceid, varnamc, NULL, 0U, name, ".CCSI") != 0)
   {
    rc = -1 ;
    goto cleanup ;
   }
 if (make_rexx_varname(traceid, varnamc, NULL, 0U, name, ".0") != 0)
   {
    rc = -1 ;
    goto cleanup ;
   }
 rexxrc =
   stem_from_long(traceid,
                  zlist,
                  stem,
                  varnamc,
                  string->VSLength) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 if (make_rexx_varname(traceid, varnamc, NULL, 0U, name, ".CCSI") != 0)
   {
    rc = -1 ;
    goto cleanup ;
   }
 rexxrc =
   stem_from_long(traceid,
                  zlist,
                  stem,
                  varnamc,
                  string->VSCCSID) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 if ( string->VSPtr != 0 )
    {
#ifdef __MVS__
     if (    (string->VSLength > 0)
          && (string->VSBufSize > 0) )
       {
        publishLength = (size_t)string->VSLength ;
        if (publishLength > (size_t)string->VSBufSize)
          publishLength = (size_t)string->VSBufSize ;
       }
#endif
     if (make_rexx_varname(traceid, varnamc, NULL, 0U, name, ".1") != 0)
       {
        rc = -1 ;
        goto cleanup ;
       }
     rexxrc =
       stem_from_bytes(traceid,
                       zlist,
                       stem,
                       varnamc,
                       (MQBYTE *)string->VSPtr,
#ifdef __MVS__
                       (int)publishLength) ;
#else
                       string->VSLength) ;
#endif
     if (    (rexxrc != RXSHV_OK)
          && (rexxrc != RXSHV_NEWV)
          && (    (rc == RXSHV_OK)
               || (rc == RXSHV_NEWV) ) )
       rc = rexxrc ;
    }

cleanup:
 //
 // Free data buffer for stem.name.1 variable data, if allocated.
 //
 if ( string->VSPtr != 0 )
   TRACE(traceid, ("Freemaining VS area\n"))                  ;
 free_mqcharv(string)                                          ;
 
 return rc ;
} // End of stem_from_strinv
 
 
// set_entry is called at the beginning of each external function execution
//
// Tracing is controlled by the TRACE macro.
//
//         If the dynamic usage of TRACE is set, then the
//            set_entry routine determines whether or not data
//            is printed (based on the RXMQTRACE Rexx var).
//
 
//
// set_entry will obtain the trace setting currently in effect,
//             and set tracebits variable for later checking in DEBUG macros
//
//         Known settings are :
//
//                              CONN  -> mqconn
//                              DISC  -> mqdisc
//                              OPEN  -> mqopen
//                              CLOSE -> mqclose
//                              GET   -> mqget
//                              PUT   -> mqput
//                              PUT1  -> mqput1
//                              INQ   -> mqinq
//                              SET   -> mqset
//                              CMIT  -> mqcmit
//                              BACK  -> mqback
//                              SUB   -> mqsub
//                              MH    -> mqcrtmh
//                              DMH   -> mqdltmh
//                              SMP   -> mqsetmp
//                              IMP   -> mqinqmp
//                              DMP   -> mqdltmp
//                              BMH   -> mqbufmh
//                              MBF   -> mqmhbuf
//
//                              BRO   -> Browse extension
//                              HXT   -> Header extraction extension
//                              EVENT -> Event determination extension
//                              TM    -> Trigger message extension
//                              COM   -> Command interface
//                              MQV   -> Debug a RXMQV
//
//                              INIT  -> initialization processing
//                              TERM  -> Deregistration processing
//
//                              *     -> Trace everything!!!
//
//
//  These settings are extracted from the RXMQTRACE Rexx Variable
//    (so RXMQTRACE = 'INQ OPEN' will trace Open and Inq operations)
//
//
//
 
MQLONG  set_envir ( char     * func        // Current function executed
                  , MQULONG  * traceidptr  // Trace bitmask
                  , RXMQCB  ** anchorptr   // pointer to RXMQCB pointer
                  )
{
  RXSTRING              varname      ;  // Variable name
  char                  varvalc[100] ;  // Char version of variable
  MQLONG                rc = 0       ;
   int                   rexxrc = RXSHV_OK ;
  int                   anchorFetchRc     ;
  RXMQCB              * candidate = NULL  ;
  RXMQCB              * trusted   = NULL  ;
  RXMQREGENTRY        * entry      = NULL  ;
 
// First of all let's try to access our anchor control block for the current thread.
// It may or may not exist when calling RXMQ function.
// If exists, use it; otherwise create it.
// This control block is a RXMQCB structure, which address is assigned to REXX RXMQANCHOR variable.
 
 *anchorptr = NULL ;
 anchorFetchRc = var_to_ptr (ZERO, RXMQANCHOR, (MQPTR *)&candidate) ;

 if ( anchorFetchRc < 0 )
   rc = -79 ;
 else
 if ( anchorFetchRc == 1 )
   {
    trusted = anchor_registry_lookup(candidate) ;
    if ( trusted == NULL )
      {
       *anchorptr = NULL ;
       rc = -79 ;
      }
    else
      *anchorptr = trusted ;
   }
 else
   {
    TRACES(("Doing malloc for RXMQREGENTRY for %u bytes\n",
            (uint32_t)sizeof(RXMQREGENTRY))) ;
    entry = (RXMQREGENTRY *) malloc(sizeof(RXMQREGENTRY)) ;

    if ( entry == NULL )
      {
       TRACES(("malloc rc = %d\n",errno)) ;
       rc = -77                           ;
      }
    else
      {
       memset (entry, 0, sizeof(*entry)) ;
       memcpy (entry->anchor.StrucId, RXMQeyecatcher, sizeof(MQCHAR4)) ;
       anchor_registry_add(entry) ;
       *anchorptr = &entry->anchor ;
       rexxrc = var_from_ptr (ZERO, RXMQANCHOR, *anchorptr) ;
       if ( (rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV) )
         {
          TRACES(("RexxVariablePool failed to publish RXMQANCHOR rc = %d\n",
                  rexxrc)) ;
          *anchorptr = NULL ;
          rc = -78 ;
         }
      }
   }
 
 if (rc == 0 )
   {
    (*anchorptr)->tracebits = ZERO                                 ;
 
    memset(&varvalc,0,sizeof(varvalc))                             ; // Clear REXX variable
    MAKERXSTRING(varname,TRACEVAR,strlen(TRACEVAR))                ; // Make name like RXMQNTRACE
    stem_to_string(ZERO, varname, "", varvalc, (int)sizeof(varvalc) - 2)    ; // Get old-style trace variable
    strcat(varvalc," ")                                            ; // Add a blank to tail
 
    if ( strlen(varvalc) > 1)
      {
       if ( strstr(varvalc,"* "    ) != NULL) (*anchorptr)->tracebits |= ALL   ;
       if ( strstr(varvalc,"CONN " ) != NULL) (*anchorptr)->tracebits |= CONN  ;
       if ( strstr(varvalc,"DISC " ) != NULL) (*anchorptr)->tracebits |= DISC  ;
       if ( strstr(varvalc,"OPEN " ) != NULL) (*anchorptr)->tracebits |= OPEN  ;
       if ( strstr(varvalc,"CLOSE ") != NULL) (*anchorptr)->tracebits |= CLOSE ;
       if ( strstr(varvalc,"GET "  ) != NULL) (*anchorptr)->tracebits |= GET   ;
       if ( strstr(varvalc,"PUT "  ) != NULL) (*anchorptr)->tracebits |= PUT   ;
       if ( strstr(varvalc,"PUT1 " ) != NULL) (*anchorptr)->tracebits |= PUT1  ;
       if ( strstr(varvalc,"INQ "  ) != NULL) (*anchorptr)->tracebits |= INQ   ;
       if ( strstr(varvalc,"SET "  ) != NULL) (*anchorptr)->tracebits |= SET   ;
       if ( strstr(varvalc,"CMIT " ) != NULL) (*anchorptr)->tracebits |= CMIT  ;
       if ( strstr(varvalc,"BACK " ) != NULL) (*anchorptr)->tracebits |= BACK  ;
       if ( strstr(varvalc,"SUB "  ) != NULL) (*anchorptr)->tracebits |= SUB   ;
       if ( strstr(varvalc,"MH  "  ) != NULL) (*anchorptr)->tracebits |= MH    ;
       if ( strstr(varvalc,"DMH "  ) != NULL) (*anchorptr)->tracebits |= DMH   ;
       if ( strstr(varvalc,"SMP "  ) != NULL) (*anchorptr)->tracebits |= SMP   ;
       if ( strstr(varvalc,"IMP "  ) != NULL) (*anchorptr)->tracebits |= IMP   ;
       if ( strstr(varvalc,"DMP "  ) != NULL) (*anchorptr)->tracebits |= DMP   ;
       if ( strstr(varvalc,"BMH "  ) != NULL) (*anchorptr)->tracebits |= BMH   ;
       if ( strstr(varvalc,"MBF "  ) != NULL) (*anchorptr)->tracebits |= MBF   ;
       if ( strstr(varvalc,"BRO "  ) != NULL) (*anchorptr)->tracebits |= BRO   ;
       if ( strstr(varvalc,"HXT "  ) != NULL) (*anchorptr)->tracebits |= HXT   ;
       if ( strstr(varvalc,"EVENT ") != NULL) (*anchorptr)->tracebits |= EVENT ;
       if ( strstr(varvalc,"TM "   ) != NULL) (*anchorptr)->tracebits |= TM    ;
       if ( strstr(varvalc,"COM "  ) != NULL) (*anchorptr)->tracebits |= COM   ;
       if ( strstr(varvalc,"MQV "  ) != NULL) (*anchorptr)->tracebits |= MQV   ;
       if ( strstr(varvalc,"INIT " ) != NULL) (*anchorptr)->tracebits |= INIT  ;
       if ( strstr(varvalc,"TERM " ) != NULL) (*anchorptr)->tracebits |= TERM  ;
      }
 
    memset(&varvalc,0,sizeof(varvalc))                             ; // Clear REXX variable
    MAKERXSTRING(varname,"RXMQTRACE",strlen("RXMQTRACE"))          ; // Make name like RXMQTRACE
    stem_to_string(ZERO, varname, "", varvalc, (int)sizeof(varvalc) - 2)    ; // Get new-style trace variable
    strcat(varvalc," ")                                            ; // Add a blank to tail
 
    if ( strlen(varvalc) > 1)
      {
       if ( strstr(varvalc,"* "    ) != NULL) (*anchorptr)->tracebits |= ALL   ;
       if ( strstr(varvalc,"CONN " ) != NULL) (*anchorptr)->tracebits |= CONN  ;
       if ( strstr(varvalc,"DISC " ) != NULL) (*anchorptr)->tracebits |= DISC  ;
       if ( strstr(varvalc,"OPEN " ) != NULL) (*anchorptr)->tracebits |= OPEN  ;
       if ( strstr(varvalc,"CLOSE ") != NULL) (*anchorptr)->tracebits |= CLOSE ;
       if ( strstr(varvalc,"GET "  ) != NULL) (*anchorptr)->tracebits |= GET   ;
       if ( strstr(varvalc,"PUT "  ) != NULL) (*anchorptr)->tracebits |= PUT   ;
       if ( strstr(varvalc,"PUT1 " ) != NULL) (*anchorptr)->tracebits |= PUT1  ;
       if ( strstr(varvalc,"INQ "  ) != NULL) (*anchorptr)->tracebits |= INQ   ;
       if ( strstr(varvalc,"SET "  ) != NULL) (*anchorptr)->tracebits |= SET   ;
       if ( strstr(varvalc,"CMIT " ) != NULL) (*anchorptr)->tracebits |= CMIT  ;
       if ( strstr(varvalc,"BACK " ) != NULL) (*anchorptr)->tracebits |= BACK  ;
       if ( strstr(varvalc,"SUB "  ) != NULL) (*anchorptr)->tracebits |= SUB   ;
       if ( strstr(varvalc,"MH  "  ) != NULL) (*anchorptr)->tracebits |= MH    ;
       if ( strstr(varvalc,"DMH "  ) != NULL) (*anchorptr)->tracebits |= DMH   ;
       if ( strstr(varvalc,"SMP "  ) != NULL) (*anchorptr)->tracebits |= SMP   ;
       if ( strstr(varvalc,"IMP "  ) != NULL) (*anchorptr)->tracebits |= IMP   ;
       if ( strstr(varvalc,"DMP "  ) != NULL) (*anchorptr)->tracebits |= DMP   ;
       if ( strstr(varvalc,"BMH "  ) != NULL) (*anchorptr)->tracebits |= BMH   ;
       if ( strstr(varvalc,"MBF "  ) != NULL) (*anchorptr)->tracebits |= MBF   ;
       if ( strstr(varvalc,"BRO "  ) != NULL) (*anchorptr)->tracebits |= BRO   ;
       if ( strstr(varvalc,"HXT "  ) != NULL) (*anchorptr)->tracebits |= HXT   ;
       if ( strstr(varvalc,"EVENT ") != NULL) (*anchorptr)->tracebits |= EVENT ;
       if ( strstr(varvalc,"TM "   ) != NULL) (*anchorptr)->tracebits |= TM    ;
       if ( strstr(varvalc,"COM "  ) != NULL) (*anchorptr)->tracebits |= COM   ;
       if ( strstr(varvalc,"MQV "  ) != NULL) (*anchorptr)->tracebits |= MQV   ;
       if ( strstr(varvalc,"INIT " ) != NULL) (*anchorptr)->tracebits |= INIT  ;
       if ( strstr(varvalc,"TERM " ) != NULL) (*anchorptr)->tracebits |= TERM  ;
      }
   }
 
 if (rc == 0 )
   {
    *traceidptr &= (*anchorptr)->tracebits          ;
    TRACE(*traceidptr, ("Function is <%s>\n",func)) ;
    TRACE(*traceidptr, ("traceid = %"PRIX32"\n",(uint32_t)*traceidptr));
    DUMPCB(*traceidptr, *anchorptr)                 ;
   }
 
 return rc;
} // End of set_envir function
 
 
//
// Return processing functions
//
//      set_return   : sets up the return variables
//
//          LASTRC    -> current operation Return Code
//          LASTCC    -> current operation MQ Completion Code
//          LASTAC    -> current operation MQ Reason     Code
//          LASTOP    -> current operation RXMQ Function
//          LASTMSG   -> current operation text message
//
//          The Return Code for the operation can be negative to
//              show that this interface has detected the error,
//              otherwise it will be the MQ Completion Code
//
//          The Message is in text format as follows:
//
//            Arg 1 : Return Code
//            Arg 2 : MQ Completion Code (or 0 if MQ not done)
//            Arg 2 : MQ Reason     Code (or 0 if MQ not done)
//            Arg 4 : RXMQ... function being run
//            Arg 5 : OK or an helpful error message
//            Arg 6 : Trace id of caller
//
//
void set_return ( const MQLONG   rc       //Function return Code
                , const MQLONG   cc       //MQ Completion Code
                , const MQLONG   ac       //MQ Reason Code
                , char         * op       //Function name
                , PRETMSG        pRetMsg  //Function message table
                , PRXSTRING      aretstr  //REXX Return String
                , MQULONG        traceid  //trace id of caller
                , char         * moremsg  //additional message
                )
{
 int                     i                ;
 int                     rexxrc       = RXSHV_OK ;
 int                     rexxrcOutput = RXSHV_OK ;
 RXSTRING                varname_new      ;  // Variable name
 RXSTRING                varname_old      ;  // Variable name
 
 TRACE(traceid,("Entering set_return\n")) ;
 
 MAKERXSTRING(varname_new, "RXMQ.", sizeof("RXMQ.")-1)  ;
 MAKERXSTRING(varname_old, PREFIX,  sizeof(PREFIX)-1)   ;
 
 rexxrc = stem_from_long  (traceid, NULL, varname_new, "LASTRC", rc);
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (   (rexxrcOutput == RXSHV_OK)
          || (rexxrcOutput == RXSHV_NEWV)) )
   rexxrcOutput = rexxrc ;
 rexxrc = stem_from_long  (traceid, NULL, varname_old, "LASTRC", rc);
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (   (rexxrcOutput == RXSHV_OK)
          || (rexxrcOutput == RXSHV_NEWV)) )
   rexxrcOutput = rexxrc ;
 
 rexxrc = stem_from_long  (traceid, NULL, varname_new, "LASTCC", cc);
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (   (rexxrcOutput == RXSHV_OK)
          || (rexxrcOutput == RXSHV_NEWV)) )
   rexxrcOutput = rexxrc ;
 rexxrc = stem_from_long  (traceid, NULL, varname_old, "LASTCC", cc);
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (   (rexxrcOutput == RXSHV_OK)
          || (rexxrcOutput == RXSHV_NEWV)) )
   rexxrcOutput = rexxrc ;
 
 rexxrc = stem_from_long  (traceid, NULL, varname_new, "LASTAC", ac);
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (   (rexxrcOutput == RXSHV_OK)
          || (rexxrcOutput == RXSHV_NEWV)) )
   rexxrcOutput = rexxrc ;
 rexxrc = stem_from_long  (traceid, NULL, varname_old, "LASTAC", ac);
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (   (rexxrcOutput == RXSHV_OK)
          || (rexxrcOutput == RXSHV_NEWV)) )
   rexxrcOutput = rexxrc ;
 
 rexxrc = stem_from_string(traceid, NULL, varname_new, "LASTOP", op, strlen(op)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (   (rexxrcOutput == RXSHV_OK)
          || (rexxrcOutput == RXSHV_NEWV)) )
   rexxrcOutput = rexxrc ;
 rexxrc = stem_from_string(traceid, NULL, varname_old, "LASTOP", op, strlen(op)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (   (rexxrcOutput == RXSHV_OK)
          || (rexxrcOutput == RXSHV_NEWV)) )
   rexxrcOutput = rexxrc ;
 
 if (rc < 0)
   {
    if (rc == -78)
      {
       sprintf(aretstr->strptr, "%"PRId32" %"PRId32" %"PRId32" %-s %s",
               (uint32_t)rc, (uint32_t)cc, (uint32_t)ac, op,
               "RexxVariablePool failed to publish RXMQANCHOR");
      }
    else
    if (rc == -79)
      {
       sprintf(aretstr->strptr, "%"PRId32" %"PRId32" %"PRId32" %-s %s",
               (uint32_t)rc, (uint32_t)cc, (uint32_t)ac, op,
               "Invalid or stale RXMQANCHOR");
      }
    else
      {
       for (i = 0; ; i++)
         {
          if (pRetMsg[i].retcode == rc ) break ;
          if (pRetMsg[i].retcode == -99) break ;
         }
       sprintf(aretstr->strptr, "%"PRId32" %"PRId32" %"PRId32" %-s %s",
               (uint32_t)rc, (uint32_t)cc, (uint32_t)ac, op,
               pRetMsg[i].retmsgc);
      }
   }
 else sprintf(aretstr->strptr,"%"PRId32" %"PRIu32" %"PRIu32" %-s %s %s",
              (uint32_t)rc, (uint32_t)cc, (uint32_t)ac, op,
              ( (cc == MQCC_OK      ) ? "OK"      :
                (cc == MQCC_WARNING ) ? "WARNING" :
                (cc == MQCC_FAILED  ) ? "FAILED"  :
                                        "UNKNOWN" ),
              moremsg        ) ;
 
 aretstr->strlength  = strlen(aretstr->strptr)     ;
 rexxrc = stem_from_string(traceid, NULL, varname_new, "LASTMSG",
                  aretstr->strptr, aretstr->strlength);
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (   (rexxrcOutput == RXSHV_OK)
          || (rexxrcOutput == RXSHV_NEWV)) )
   rexxrcOutput = rexxrc ;
 rexxrc = stem_from_string(traceid, NULL, varname_old, "LASTMSG",
                  aretstr->strptr, aretstr->strlength);
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (   (rexxrcOutput == RXSHV_OK)
          || (rexxrcOutput == RXSHV_NEWV)) )
   rexxrcOutput = rexxrc ;
 
 if (    (rexxrcOutput != RXSHV_OK)
      && (rexxrcOutput != RXSHV_NEWV) )
   {
    TRACE(traceid,
          ("RexxVariablePool failed while publishing LAST variables rc = %d\n",
           rexxrcOutput) ) ;
   }
 
 TRACE(traceid,("Leaving set_return\n"))  ;
 TRACE(traceid,("Leaving %s\n",op))       ; // Leaving function
 fflush(NULL)                             ; // Required to flush trace buffer
 
 return ;
} // End of set_return function
 
//
// Functions that manipulate MQ objects and Rexx Variables
//
//           The make_??_from_stem  functions build the ??
//                                  MQ object by trying to reference
//                                  a stem variable (whose name ending
//                                  with a dot) whose name is supplied.
//                                  If the .xx part is present, then
//                                  the contents are copied into the
//                                  MQ Object.
//
//
//            The make_stem_from_?? functions work the other way around.
//                                  they are supplied with the name of
//                                  a stem variable (including the dot)
//                                  and build a Rexx representatation of
//                                  the MQ object by setting xx.yy
//                                  variables, where the yy is a
//                                  component of the MQ object. In addition,
//                                  (.)ZLIST is set to a list of the components
//                                  in the stem (without the leading dot).
//
//
//   The names of the .extensions are described in the functions.
//
//   Note that the make_stem_from_?? functions will ALWAYS build
//        all of the .extensions, whereas the make_??_from_stem
//        functions can cope with the omission of extensions.
//
//
// make_od_from_stem will return an Object Descriptor from the
//                   contents of a Stem Variable:
//
//                     .VER  -> Version
//                     .OT   -> ObjectType
//                     .ON   -> ObjectName
//                     .OQM  -> ObjectQMgrName
//                     .DQN  -> DynamicQueue
//                     .AUID -> AlternateUserid
//                     .RP   -> RecsPresent
//                     .KDC  -> KnownDestCount (output only)
//                     .UDC  -> UnknownDestCount (output only)
//                     .IDC  -> InvalidDestCount (output only)
//                     .ORO  -> ObjectRecOffset (not implemented)
//                     .RRO  -> ResponseRecOffset (not implemented)
//                     .ORP  -> ObjectRecPtr (not implemented)
//                     .RRP  -> ResponseRecPtr (not implemented)
//                     .ASID -> AlternateSecurityId (Windows only)
//                     .RQN  -> ResolvedQName (output only)
//                     .RQMN -> ResolvedQMgrName (output only)
//                     .OS   -> ObjectString
//                     .SS   -> SelectionString
//                     .ROS  -> ResObjectString
//                     .RT   -> ResolvedType
//
 
int make_od_from_stem ( MQULONG    traceid      // trace id of caller
                       , MQOD     * od           // target object descriptor
                       , RXSTRING   stem         // name of stem variable
                       )
{
 TRACE(traceid, ("Entering make_od_from_stem\n") ) ;
 
 memcpy(od, &od_default, sizeof(MQOD))         ;
 
//If the given variable is not a stem. variable, then take the
//   quicker option of assuming it's a Queue to open
 
 if ( stem.strptr[stem.strlength-1] != '.' )
   {
     if ( stem.strlength > sizeof(od->ObjectName) )
       return -1 ;
     memcpy(od->ObjectName,
            stem.strptr,
            stem.strlength ) ;
     DUMPCB(traceid,  od )            ;
     TRACE(traceid, ("Leaving make_od_from_stem\n") ) ;
     return 0                     ;
    }
 
 //The given variable is a stem. variable, so get its contents
 
 // Version 1 of MQOD
 stem_to_long  (traceid, stem, "VER" , &od->Version)                               ;
 stem_to_long  (traceid, stem, "OT"  , &od->ObjectType)                            ;
 stem_to_string(traceid, stem, "ON"  ,  od->ObjectName,          sizeof(MQCHAR48)) ;
 stem_to_string(traceid, stem, "OQM" ,  od->ObjectQMgrName,      sizeof(MQCHAR48)) ;
 stem_to_string(traceid, stem, "DQN" ,  od->DynamicQName,        sizeof(MQCHAR48)) ;
 stem_to_string(traceid, stem, "AUID",  od->AlternateUserId,     sizeof(MQCHAR12)) ;
 // Version 2 of MQOD
 stem_to_long  (traceid, stem, "RP"  , &od->RecsPresent)                           ;
 stem_to_long  (traceid, stem, "KDC" , &od->KnownDestCount)                        ;
 stem_to_long  (traceid, stem, "UDC" , &od->UnknownDestCount)                      ;
 stem_to_long  (traceid, stem, "IDC" , &od->InvalidDestCount)                      ;
 // Version 3 of MQOD
 stem_to_bytes (traceid, stem, "ASID",  od->AlternateSecurityId, sizeof(MQBYTE40)) ;
 stem_to_string(traceid, stem, "RQN" ,  od->ResolvedQName,       sizeof(MQCHAR48)) ;
 stem_to_string(traceid, stem, "RQMN",  od->ResolvedQMgrName,    sizeof(MQCHAR48)) ;
 // Version 4 of MQOD
 if ( stem_to_strinv(traceid, stem, "OS"  , &od->ObjectString,
                     RXMQ_MQCHARV_INPUT_ONLY) != 0 )
   {
    free_od_mqcharv(od) ;
    return -2 ;
   }
 if ( stem_to_strinv(traceid, stem, "SS"  , &od->SelectionString,
                     RXMQ_MQCHARV_INPUT_ONLY) != 0 )
   {
    free_od_mqcharv(od) ;
    return -2 ;
   }
 if ( stem_to_strinv(traceid, stem, "ROS" , &od->ResObjectString,
                     RXMQ_MQCHARV_CAPACITY_REQUIRED) != 0 )
   {
    free_od_mqcharv(od) ;
    return -2 ;
   }
 stem_to_long  (traceid, stem, "RT"  , &od->ResolvedType)                          ;
 
 DUMPCB(traceid, od )                             ;
 TRACE(traceid, ("Leaving make_od_from_stem\n") ) ;
 
 return 0 ;
} // End of make_od_from_stem function
 
//
// make_stem_from_od will return an Object Descriptor as the
//                   contents of a Stem Variable:
//
//                     .VER  -> Version
//                     .OT   -> ObjectType
//                     .ON   -> ObjectName
//                     .OQM  -> ObjectQMgrName
//                     .DQN  -> DynamicQueue
//                     .AUID -> AlternateUserId
//                     .RP   -> RecsPresent
//                     .KDC  -> KnownDestCount
//                     .UDC  -> UnknownDestCount
//                     .IDC  -> InvalidDestCount
//                     .ORO  -> ObjectRecOffset (not implemented)
//                     .RRO  -> ResponseRecOffset (not implemented)
//                     .ORP  -> ObjectRecPtr (not implemented)
//                     .RRP  -> ResponseRecPtr (not implemented)
//                     .ASID -> AlternateSecurityId (Windows only)
//                     .RQN  -> ResolvedQName
//                     .RQMN -> ResolvedQMgrName
//                     .OS   -> ObjectString
//                     .SS   -> SelectionString
//                     .ROS  -> ResObjectString
//                     .RT   -> ResolvedType
//
//                     .ZLIST -> 'VER OT ON OQM DQN AUID
//                                RP
//                                KDC UDC IDC
//                                ASID
//                                RQN RQMN
//                                OS. SS. ROS. RT
//
 
int make_stem_from_od ( MQULONG    traceid      // trace id of caller
                       , MQOD     * od           // source object descriptor
                       , RXSTRING   stem         // name of stem variable
                       )
{
 int                         rc = RXSHV_OK ;
 int                         rexxrc = RXSHV_OK ;
 char                        zlist[200]   ;  // Char version of .ZLIST
 zlist[0] = '\0'                          ;
 
 TRACE(traceid, ("Entering make_stem_from_od\n") ) ;
 DUMPCB(traceid, od )                              ;
 
 
 // Version 1 of MQOD
 rexxrc = stem_from_long  (traceid, zlist, stem, "VER" , od->Version)                               ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "OT"  , od->ObjectType)                            ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "ON"  , od->ObjectName,          sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "OQM" , od->ObjectQMgrName,      sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "DQN" , od->DynamicQName,        sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "AUID", od->AlternateUserId,     sizeof(MQCHAR12)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 // Version 2 of MQOD
 rexxrc = stem_from_long  (traceid, zlist, stem, "RP"  , od->RecsPresent)                           ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "KDC" , od->KnownDestCount)                        ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "UDC" , od->UnknownDestCount)                      ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "IDC" , od->InvalidDestCount)                      ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 // Version 3 of MQOD
 rexxrc = stem_from_bytes (traceid, zlist, stem, "ASID", od->AlternateSecurityId, sizeof(MQBYTE40)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "RQN" , od->ResolvedQName,       sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "RQMN", od->ResolvedQMgrName,    sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 // Version 4 of MQOD
 rexxrc = stem_from_strinv(traceid, zlist, stem, "OS"  , &od->ObjectString)                          ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_strinv(traceid, zlist, stem, "SS"  , &od->SelectionString)                       ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_strinv(traceid, zlist, stem, "ROS" , &od->ResObjectString)                       ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "RT"  , od->ResolvedType)                          ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 
 rexxrc = stem_from_string(traceid, zlist, stem, "ZLIST", zlist, strlen(zlist))                         ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 
 TRACE(traceid, ("Leaving make_stem_from_od\n") ) ;
 
 return rc ;
} // End of make_stem_from_od function
 
//
// make_po_from_stem will return a Put Message Options Desc from the
//                   contents of a Stem Variable:
//
//                     .VER  -> Version
//                     .OPT  -> Options
//                     .TIME -> Timeout
//                     .CON  -> Context
//                     .KDC  -> KnownDestCount (output only)
//                     .UDC  -> UnKnownDestCount (output only)
//                     .IDC  -> InvalidDestCount (output only)
//                     .RQN  -> ResolvedQName (output only)
//                     .RQMN -> ResolvedQMgrName (output only)
//                     .RP   -> RecsPresent
//                     .PMRF -> PutMsgRecFields (not implemented)
//                     .PMRO -> PutMsgRecOffset (not implemented)
//                     .RRO  -> ResponseRecOffset (not implemented)
//                     .PMRP -> PutMsgRecPtr (not implemented)
//                     .RRP  -> ResponseRecPtr (not implemented)
//                     .OMH  -> OriginalMsgHandle
//                     .NMH  -> NewMsgHandle
//                     .ACT  -> Action
//                     .PL   -> PubLevel
//
 
void make_po_from_stem ( MQULONG    traceid      // trace id of caller
                       , MQPMO    * pmo          // target PMO
                       , RXSTRING   stem         // name of stem variable
                       )
{
 TRACE(traceid, ("Entering make_po_from_stem\n") ) ;
 
 memcpy(pmo, &pmo_default, sizeof(MQPMO))      ;
 
 // Version 1 of MQPMO
 stem_to_long  (traceid, stem, "VER" , &pmo->Version)                            ;
 stem_to_long  (traceid, stem, "OPT" , &pmo->Options)                            ;
 stem_to_long  (traceid, stem, "TIME", &pmo->Timeout)                            ;
 stem_to_long  (traceid, stem, "CON" , &pmo->Context)                            ;
 stem_to_long  (traceid, stem, "KDC" , &pmo->KnownDestCount)                     ;
 stem_to_long  (traceid, stem, "UDC" , &pmo->UnknownDestCount)                   ;
 stem_to_long  (traceid, stem, "IDC" , &pmo->InvalidDestCount)                   ;
 stem_to_string(traceid, stem, "RQN" ,  pmo->ResolvedQName,    sizeof(MQCHAR48)) ;
 stem_to_string(traceid, stem, "RQMN",  pmo->ResolvedQMgrName, sizeof(MQCHAR48)) ;
 // Version 2 of MQPMO
 stem_to_long  (traceid, stem, "RP"  , &pmo->RecsPresent)                        ;
 // Version 3 of MQPMO
 stem_to_int64 (traceid, stem, "OMH" , &pmo->OriginalMsgHandle)                  ;
 stem_to_int64 (traceid, stem, "NMH" , &pmo->NewMsgHandle)                       ;
 stem_to_long  (traceid, stem, "ACT" , &pmo->Action)                             ;
 stem_to_long  (traceid, stem, "PL"  , &pmo->PubLevel)                           ;
 
 DUMPCB(traceid, pmo ) ;
 TRACE(traceid, ("Leaving make_po_from_stem\n") ) ;
 
 return ;
} // End of make_po_from_stem function
 
//
// make_stem_from_po will return a Put Message Options Desc as the
//                   contents of a Stem Variable:
//
//                     .VER  -> Version
//                     .OPT  -> Options
//                     .TIME -> Timeout
//                     .CON  -> Context
//                     .KDC  -> KnownDestCount
//                     .UDC  -> UnKnownDestCount
//                     .IDC  -> InvalidDestCount
//                     .RQN  -> ResolvedQName
//                     .RQMN -> ResolvedQMgrName
//                     .RP   -> RecsPresent
//                     .PMRF -> PutMsgRecFields (not implemented)
//                     .PMRO -> PutMsgRecOffset (not implemented)
//                     .RRO  -> ResponseRecOffset (not implemented)
//                     .PMRP -> PutMsgRecPtr (not implemented)
//                     .RRP  -> ResponseRecPtr (not implemented)
//                     .OMH  -> OriginalMsgHandle
//                     .NMH  -> NewMsgHandle
//                     .ACT  -> Action
//                     .PL   -> PubLevel
//
//                     .ZLIST -> 'VER OPT CON CON TIME KDC UDC IDC RQN RQMN
//                                RP OMH NMH ACT PL'
//
 
int make_stem_from_po ( MQULONG    traceid      // trace id of caller
                       , MQPMO    * pmo          // source PMO
                       , RXSTRING   stem         // name of stem variable
                       )
{
 int                     rc = RXSHV_OK ;
 int                     rexxrc = RXSHV_OK ;
 char                    zlist[200]   ;  // Char version of .ZLIST
 zlist[0] = '\0'                      ;
 
 TRACE(traceid, ("Entering make_stem_from_po\n") ) ;
 DUMPCB(traceid,  pmo )                            ;
 
 // Version 1 of MQPMO
 rexxrc = stem_from_long  (traceid, zlist, stem, "VER" , pmo->Version)                               ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "OPT" , pmo->Options)                               ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "TIME", pmo->Timeout)                               ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "CON" , pmo->Context)                               ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "KDC" , pmo->KnownDestCount)                        ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "UDC" , pmo->UnknownDestCount)                      ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "IDC" , pmo->InvalidDestCount)                      ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "RQN" , pmo->ResolvedQName,       sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "RQMN", pmo->ResolvedQMgrName,    sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 // Version 2 of MQPMO
 rexxrc = stem_from_long  (traceid, zlist, stem, "RP"  , pmo->RecsPresent)                           ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 // Version 3 of MQPMO
 rexxrc = stem_from_int64 (traceid, zlist, stem, "OMH" , pmo->OriginalMsgHandle)                     ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_int64 (traceid, zlist, stem, "NMH" , pmo->NewMsgHandle)                          ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "ACT" , pmo->Action)                                ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "PL"  , pmo->PubLevel)                              ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 
 rexxrc = stem_from_string(traceid, zlist, stem, "ZLIST", zlist, strlen(zlist))                      ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 
 TRACE(traceid,  ("Leaving make_stem_from_po\n") ) ;
 
 return rc ;
} // End of make_stem_from_po function
 
//
// make_go_from_stem will return a Get Message Options Desc from the
//                   contents of a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//                     .WAIT  -> WaitInterval
//                     .RQN   -> ResolvedQueueName (output only)
//                     .MOPT  -> MatchOptions
//                     .GS    -> GroupStatus (output only)
//                     .SS    -> SegmentStatus (output only)
//                     .SEG   -> Segmentation (output only)
//                     .MT    -> MsgToken
//                     .RL    -> ReturnedLength (output only)
//                     .MH    -> MsgHandle
//
 
void make_go_from_stem ( MQULONG    traceid      // trace id of caller
                       , MQGMO    * gmo          // target GMO
                       , RXSTRING   stem         // name of stem variable
                       )
{
 TRACE(traceid, ("Entering make_go_from_stem\n") ) ;
 
 memcpy(gmo, &gmo_default, sizeof(MQGMO))      ;
 
 // Version 1 of MQGMO
 stem_to_long  (traceid, stem, "VER" , &gmo->Version)                         ;
 stem_to_long  (traceid, stem, "OPT" , &gmo->Options)                         ;
 stem_to_long  (traceid, stem, "WAIT", &gmo->WaitInterval)                    ;
 stem_to_string(traceid, stem, "RQN" ,  gmo->ResolvedQName, sizeof(MQCHAR48)) ;
 // Version 2 of MQGMO
 stem_to_long  (traceid, stem, "MOPT", &gmo->MatchOptions)                    ;
 stem_to_char  (traceid, stem, "GS"  , &gmo->GroupStatus)                     ;
 stem_to_char  (traceid, stem, "SS"  , &gmo->SegmentStatus)                   ;
 stem_to_char  (traceid, stem, "SEG" , &gmo->Segmentation)                    ;
 // Version 3 of MQGMO
 stem_to_bytes (traceid, stem, "MT"  ,  gmo->MsgToken,      sizeof(MQBYTE16)) ;
 stem_to_long  (traceid, stem, "RL"  , &gmo->ReturnedLength)                  ;
 // Version 4 of MQGMO
 stem_to_int64 (traceid, stem, "MH"  , &gmo->MsgHandle)                       ;
 
 DUMPCB(traceid,  gmo ) ;
 TRACE(traceid, ("Leaving make_go_from_stem\n") ) ;
 
 return ;
} // End of make_go_from_stem function
 
//
// make_stem_from_go will return a Get Message Options Desc as the
//                   contents of a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//                     .WAIT  -> WaitInterval
//                     .RQN   -> ResolvedQueueName
//                     .MOPT  -> MatchOptions
//                     .GS    -> GroupStatus
//                     .SS    -> SegmentStatus
//                     .SEG   -> Segmentation
//                     .MT    -> sgToken
//                     .RL    -> ReturnedLength
//                     .MH    -> MsgHandle
//                     .ZLIST -> 'VER OPT WAIT RQN
//                                MOPT GS SS SEG
//                                MT RL MH'
//
 
int make_stem_from_go ( MQULONG    traceid      // trace id of caller
                       , MQGMO    * gmo          // source GMO
                       , RXSTRING   stem         // name of stem variable
                       )
{
 int                     rc = RXSHV_OK ;
 int                     rexxrc = RXSHV_OK ;
 char                    zlist[200]   ;  // Char version of .ZLIST
 zlist[0] = '\0'                      ;
 
 TRACE(traceid, ("Entering make_stem_from_go\n") ) ;
 DUMPCB(traceid,  gmo )                            ;
 
// Version 1 of MQGMO
 rexxrc = stem_from_long  (traceid, zlist, stem, "VER" , gmo->Version)                         ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "OPT" , gmo->Options)                         ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "WAIT", gmo->WaitInterval)                    ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "RQN" , gmo->ResolvedQName, sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
// Version 2 of MQGMO
 rexxrc = stem_from_long  (traceid, zlist, stem, "MOPT", gmo->MatchOptions)                    ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_char  (traceid, zlist, stem, "GS"  , gmo->GroupStatus)                     ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_char  (traceid, zlist, stem, "SS"  , gmo->SegmentStatus)                   ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_char  (traceid, zlist, stem, "SEG" , gmo->Segmentation)                    ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
// Version 3 of MQGMO
 rexxrc = stem_from_bytes (traceid, zlist, stem, "MT"  , gmo->MsgToken,      sizeof(MQBYTE16)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "RL"  , gmo->ReturnedLength)                  ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
// Version 4 of MQGMO
 rexxrc = stem_from_int64 (traceid, zlist, stem, "MH"  , gmo->MsgHandle)                       ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 
 rexxrc = stem_from_string(traceid, zlist, stem, "ZLIST", zlist, strlen(zlist))                ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 
 TRACE(traceid, ("Leaving make_stem_from_go\n") ) ;
 
 return rc ;
} // End of make_stem_from_go function
 
//
//
//
//
// make_cmho_from_stem will return a Create Message Handle Options
//                 structure from the contents of a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//
void make_cmho_from_stem ( MQULONG    traceid
                         , MQCMHO   * cmho
                         , RXSTRING   stem
                         )
{
 TRACE(traceid, ("Entering make_cmho_from_stem\n") ) ;
 memcpy(cmho, &cmho_default, sizeof(MQCMHO)) ;
 stem_to_long(traceid, stem, "VER", &cmho->Version) ;
 stem_to_long(traceid, stem, "OPT", &cmho->Options) ;
 DUMPCB(traceid, cmho) ;
  TRACE(traceid, ("Leaving make_cmho_from_stem\n") ) ;
  return ;
 } // End of make_cmho_from_stem function
 //
 // make_stem_from_cmho will return a Create Message Handle Options
 //                 structure into a Stem Variable:
 //
 //                     .VER   -> Version
 //                     .OPT   -> Options
 //                     .ZLIST -> VER OPT
 //
 int make_stem_from_cmho ( MQULONG    traceid
                          , MQCMHO   * cmho
                          , RXSTRING   stem
                          )
 {
  int                    rc = RXSHV_OK ;
  int                    rexxrc = RXSHV_OK ;
  char                   zlist[100] ;  // Char version of .ZLIST
 zlist[0] = '\0' ;
 TRACE(traceid, ("Entering make_stem_from_cmho\n") ) ;
 rexxrc =
   stem_from_long(traceid,
                  zlist,
                  stem,
                  "VER",
                  cmho->Version) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc =
   stem_from_long(traceid,
                  zlist,
                  stem,
                  "OPT",
                  cmho->Options) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc =
   stem_from_string(traceid,
                    zlist,
                    stem,
                    "ZLIST",
                    zlist,
                    strlen(zlist)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 TRACE(traceid, ("Leaving make_stem_from_cmho\n") ) ;
 return rc ;
} // End of make_stem_from_cmho function
//
// make_dmho_from_stem will return a Delete Message Handle Options
//                 structure from the contents of a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//
void make_dmho_from_stem ( MQULONG    traceid
                         , MQDMHO   * dmho
                         , RXSTRING   stem
                         )
{
 TRACE(traceid, ("Entering make_dmho_from_stem\n") ) ;
 memcpy(dmho, &dmho_default, sizeof(MQDMHO)) ;
 stem_to_long(traceid, stem, "VER", &dmho->Version) ;
 stem_to_long(traceid, stem, "OPT", &dmho->Options) ;
 DUMPCB(traceid, dmho) ;
 TRACE(traceid, ("Leaving make_dmho_from_stem\n") ) ;
 return ;
} // End of make_dmho_from_stem function
//
// make_stem_from_dmho will return a Delete Message Handle Options
//                 structure into a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//                     .ZLIST -> VER OPT
//
int make_stem_from_dmho ( MQULONG    traceid
                         , MQDMHO   * dmho
                         , RXSTRING   stem
                         )
{
 int                    rc = RXSHV_OK ;
 int                    rexxrc = RXSHV_OK ;
 char                   zlist[100] ;  // Char version of .ZLIST
 zlist[0] = '\0'   ;
 TRACE(traceid, ("Entering make_stem_from_dmho\n") ) ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "VER", dmho->Version) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "OPT", dmho->Options) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "ZLIST", zlist, strlen(zlist)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 TRACE(traceid, ("Leaving make_stem_from_dmho\n") ) ;
 return rc ;
} // End of make_stem_from_dmho function
 //
 //
//
//
// make_smpo_from_stem will return a Set Message Property Options
//                 structure from the contents of a Stem Variable:
//
//                     .VER           -> Version
//                     .OPT           -> Options
//                     .VENC          -> ValueEncoding
//                     .VCCSI         -> ValueCCSID
//
void make_smpo_from_stem ( MQULONG    traceid
                         , MQSMPO   * smpo
                         , RXSTRING   stem
                         )
{
  TRACE(traceid, ("Entering make_smpo_from_stem\n") ) ;
  memcpy(smpo, &smpo_default, sizeof(MQSMPO)) ;
  stem_to_long(traceid,
               stem,
               "VER",
               &smpo->Version) ;
  stem_to_long(traceid,
               stem,
               "OPT",
               &smpo->Options) ;
  stem_to_long(traceid,
               stem,
               "VENC",
               &smpo->ValueEncoding) ;
  stem_to_long(traceid,
               stem,
               "VCCSI",
               &smpo->ValueCCSID) ;
  DUMPCB(traceid, smpo) ;
  TRACE(traceid, ("Leaving make_smpo_from_stem\n") ) ;
  return ;
}
//
// make_stem_from_smpo will return a Set Message Property Options
//                 structure into a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//                     .ZLIST -> VER OPT VENC VCCSI
//
int make_stem_from_smpo ( MQULONG    traceid
                         , MQSMPO   * smpo
                         , RXSTRING   stem
                         )
{
 int                    rc = RXSHV_OK ;
 int                    rexxrc = RXSHV_OK ;
 char                   zlist[100] ;  // Char version of .ZLIST
 zlist[0] = '\0'   ;
  TRACE(traceid, ("Entering make_stem_from_smpo\n") ) ;
  rexxrc = stem_from_long(traceid,
                 zlist,
                 stem,
                 "VER",
                 smpo->Version) ;
  if (    (rexxrc != RXSHV_OK)
       && (rexxrc != RXSHV_NEWV)
       && (    (rc == RXSHV_OK)
            || (rc == RXSHV_NEWV) ) )
    rc = rexxrc ;
  rexxrc = stem_from_long(traceid,
                 zlist,
                 stem,
                 "OPT",
                 smpo->Options) ;
  if (    (rexxrc != RXSHV_OK)
       && (rexxrc != RXSHV_NEWV)
       && (    (rc == RXSHV_OK)
            || (rc == RXSHV_NEWV) ) )
    rc = rexxrc ;
  rexxrc = stem_from_long(traceid,
                 zlist,
                 stem,
                 "VENC",
                 smpo->ValueEncoding) ;
  if (    (rexxrc != RXSHV_OK)
       && (rexxrc != RXSHV_NEWV)
       && (    (rc == RXSHV_OK)
            || (rc == RXSHV_NEWV) ) )
    rc = rexxrc ;
  rexxrc = stem_from_long(traceid,
                 zlist,
                 stem,
                 "VCCSI",
                 smpo->ValueCCSID) ;
  if (    (rexxrc != RXSHV_OK)
       && (rexxrc != RXSHV_NEWV)
       && (    (rc == RXSHV_OK)
            || (rc == RXSHV_NEWV) ) )
    rc = rexxrc ;
  rexxrc = stem_from_long(traceid,
                 NULL,
                 stem,
                 "VALUEENCODING",
                 smpo->ValueEncoding) ;
  if (    (rexxrc != RXSHV_OK)
       && (rexxrc != RXSHV_NEWV)
       && (    (rc == RXSHV_OK)
            || (rc == RXSHV_NEWV) ) )
    rc = rexxrc ;
  rexxrc = stem_from_long(traceid,
                 NULL,
                 stem,
                 "VALUECCSID",
                 smpo->ValueCCSID) ;
  if (    (rexxrc != RXSHV_OK)
       && (rexxrc != RXSHV_NEWV)
       && (    (rc == RXSHV_OK)
            || (rc == RXSHV_NEWV) ) )
    rc = rexxrc ;
  rexxrc = stem_from_string(traceid,
                   zlist,
                   stem,
                   "ZLIST",
                   zlist,
                   strlen(zlist)) ;
  if (    (rexxrc != RXSHV_OK)
       && (rexxrc != RXSHV_NEWV)
       && (    (rc == RXSHV_OK)
            || (rc == RXSHV_NEWV) ) )
    rc = rexxrc ;
  TRACE(traceid, ("Leaving make_stem_from_smpo\n") ) ;
  return rc ;
}
//
//
//
//
// make_impo_from_stem will return an Inquire Message Property Options
//                 structure from the contents of a Stem Variable:
//
//                     .VER               -> Version
//                     .OPT               -> Options
//                     .RQENC             -> RequestedEncoding
//                     .RQCCSI            -> RequestedCCSID
//
void make_impo_from_stem ( MQULONG    traceid
                         , MQIMPO   * impo
                         , RXSTRING   stem
                         )
{
 TRACE(traceid, ("Entering make_impo_from_stem\n") ) ;
  memcpy(impo, &impo_default, sizeof(MQIMPO)) ;
  stem_to_long(traceid, stem, "VER"              , &impo->Version)          ;
  stem_to_long(traceid, stem, "OPT"              , &impo->Options)          ;
  stem_to_long(traceid, stem, "RQENC"            , &impo->RequestedEncoding);
  stem_to_long(traceid, stem, "RQCCSI"           , &impo->RequestedCCSID)   ;
  DUMPCB(traceid, impo) ;
  TRACE(traceid, ("Leaving make_impo_from_stem\n") ) ;
  return ;
 } // End of make_impo_from_stem function
 //
 // make_stem_from_impo will return an Inquire Message Property Options
 //                 structure into a Stem Variable:
 //
 //                     .VER               -> Version
 //                     .OPT               -> Options
 //                     .RQENC             -> RequestedEncoding
 //                     .RTENC             -> ReturnedEncoding
 //                     .RQCCSI            -> RequestedCCSID
 //                     .RTCCSI            -> ReturnedCCSID
//                      .TYPESTRING        -> TypeString
//                      .RNAMELEN         -> ReturnedName.VSLength
 //                     .ZLIST            -> VER OPT RQENC RQCCSI RTENC RTCCSI RNAMELEN TYPESTRING
 //
 int make_stem_from_impo ( MQULONG    traceid
                          , MQIMPO   * impo
                          , RXSTRING   stem
                         )
{
 int                    rc = RXSHV_OK ;
 int                    rexxrc = RXSHV_OK ;
 char                   zlist[100]  ;  // Char version of .ZLIST
 zlist[0] = '\0' ;
 TRACE(traceid, ("Entering make_stem_from_impo\n") ) ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "VER"   , impo->Version)          ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "OPT"   , impo->Options)          ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "RQENC" , impo->RequestedEncoding);
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "RQCCSI", impo->RequestedCCSID)   ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "RTENC" , impo->ReturnedEncoding) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "RTCCSI", impo->ReturnedCCSID)    ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long(traceid,
                NULL,
                stem,
                "REQENC",
                impo->RequestedEncoding) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long(traceid,
                NULL,
                stem,
                "REQUESTEDENCODING",
                impo->RequestedEncoding) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long(traceid,
                NULL,
                stem,
                "REQCCSI",
                impo->RequestedCCSID) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long(traceid,
                NULL,
                stem,
               "REQUESTEDCCSID",
               impo->RequestedCCSID) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, NULL, stem, "RETENC"           , impo->ReturnedEncoding) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, NULL, stem, "RETURNEDENCODING" , impo->ReturnedEncoding) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, NULL, stem, "RETCCSI"          , impo->ReturnedCCSID)    ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, NULL, stem, "RETURNEDCCSID"    , impo->ReturnedCCSID)    ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long(traceid,zlist,stem,"RNAMELEN",impo->ReturnedName.VSLength) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid,zlist,stem,"TYPESTRING",impo->TypeString,sizeof(impo->TypeString));
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "ZLIST", zlist, strlen(zlist)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 TRACE(traceid, ("Leaving make_stem_from_impo\n") ) ;
 return rc ;
} // End of make_stem_from_impo function
//
//
//
//
// make_dmpo_from_stem will return a Delete Message Property Options
//                 structure from the contents of a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//
void make_dmpo_from_stem ( MQULONG    traceid
                         , MQDMPO   * dmpo
                         , RXSTRING   stem
                         )
{
 TRACE(traceid, ("Entering make_dmpo_from_stem\n") ) ;
 memcpy(dmpo, &dmpo_default, sizeof(MQDMPO)) ;
 stem_to_long(traceid, stem, "VER", &dmpo->Version) ;
 stem_to_long(traceid, stem, "OPT", &dmpo->Options) ;
  DUMPCB(traceid, dmpo) ;
  TRACE(traceid, ("Leaving make_dmpo_from_stem\n") ) ;
  return ;
 } // End of make_dmpo_from_stem function
 //
 // make_stem_from_dmpo will return a Delete Message Property Options
 //                 structure into a Stem Variable:
 //
 //                     .VER   -> Version
 //                     .OPT   -> Options
 //                     .ZLIST -> VER OPT
 //
 int make_stem_from_dmpo ( MQULONG    traceid
                          , MQDMPO   * dmpo
                          , RXSTRING   stem
                          )
 {
 int                    rc = RXSHV_OK ;
 int                    rexxrc = RXSHV_OK ;
 char                   zlist[100]  ;  // Char version of .ZLIST
  zlist[0] = '\0'   ;
  TRACE(traceid, ("Entering make_stem_from_dmpo\n") ) ;
  rexxrc = stem_from_long  (traceid, zlist, stem, "VER", dmpo->Version) ;
  if (    (rexxrc != RXSHV_OK)
       && (rexxrc != RXSHV_NEWV)
       && (    (rc == RXSHV_OK)
            || (rc == RXSHV_NEWV) ) )
    rc = rexxrc ;
  rexxrc = stem_from_long  (traceid, zlist, stem, "OPT", dmpo->Options) ;
  if (    (rexxrc != RXSHV_OK)
       && (rexxrc != RXSHV_NEWV)
       && (    (rc == RXSHV_OK)
            || (rc == RXSHV_NEWV) ) )
    rc = rexxrc ;
  rexxrc = stem_from_string(traceid, zlist, stem, "ZLIST", zlist, strlen(zlist)) ;
  if (    (rexxrc != RXSHV_OK)
       && (rexxrc != RXSHV_NEWV)
       && (    (rc == RXSHV_OK)
            || (rc == RXSHV_NEWV) ) )
    rc = rexxrc ;
  TRACE(traceid, ("Leaving make_stem_from_dmpo\n") ) ;
  return rc ;
 } // End of make_stem_from_dmpo function
//
//
//
//
// make_bmho_from_stem will return a Buffer To Message Handle Options
//                 structure from the contents of a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//
void make_bmho_from_stem ( MQULONG    traceid
                         , MQBMHO   * bmho
                         , RXSTRING   stem
                         )
{
 TRACE(traceid, ("Entering make_bmho_from_stem\n") ) ;
 memcpy(bmho, &bmho_default, sizeof(MQBMHO)) ;
 stem_to_long(traceid, stem, "VER", &bmho->Version) ;
 stem_to_long(traceid, stem, "OPT", &bmho->Options) ;
 DUMPCB(traceid, bmho) ;
 TRACE(traceid, ("Leaving make_bmho_from_stem\n") ) ;
 return ;
} // End of make_bmho_from_stem function
//
// make_stem_from_bmho will return a Buffer To Message Handle Options
//                 structure into a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//                     .ZLIST -> VER OPT
//
int make_stem_from_bmho ( MQULONG    traceid
                         , MQBMHO   * bmho
                         , RXSTRING   stem
                         )
{
 int                   rc = RXSHV_OK ;
 int                   rexxrc = RXSHV_OK ;
 char                  zlist[100]  ;  // Char version of .ZLIST
 zlist[0] = '\0' ;
 TRACE(traceid, ("Entering make_stem_from_bmho\n") ) ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "VER", bmho->Version) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "OPT", bmho->Options) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "ZLIST", zlist, strlen(zlist)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 TRACE(traceid, ("Leaving make_stem_from_bmho\n") ) ;
 return rc ;
} // End of make_stem_from_bmho function
//
//
//
// Message handle to buffer options     MQMHBO
//
// make_mhbo_from_stem will return a Message Handle To Buffer Options
//                 structure from the contents of a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//
void make_mhbo_from_stem ( MQULONG    traceid
                         , MQMHBO   * mhbo
                         , RXSTRING   stem
                         )
{
 TRACE(traceid, ("Entering make_mhbo_from_stem\n") ) ;
 memcpy(mhbo, &mhbo_default, sizeof(MQMHBO)) ;
 stem_to_long(traceid, stem, "VER", &mhbo->Version) ;
 stem_to_long(traceid, stem, "OPT", &mhbo->Options) ;
 DUMPCB(traceid, mhbo) ;
 TRACE(traceid, ("Leaving make_mhbo_from_stem\n") ) ;
 return ;
} // End of make_mhbo_from_stem function
//
// make_stem_from_mhbo will return a Message Handle To Buffer Options
//                 structure into a Stem Variable:
//
//                     .VER   -> Version
//                     .OPT   -> Options
//                     .ZLIST -> VER OPT
//
int make_stem_from_mhbo ( MQULONG    traceid
                         , MQMHBO   * mhbo
                         , RXSTRING   stem
                         )
{
 int                  rc = RXSHV_OK ;
 int                  rexxrc = RXSHV_OK ;
 char                 zlist[100]  ;  // Char version of .ZLIST
 zlist[0] = '\0' ;
 TRACE(traceid, ("Entering make_stem_from_mhbo\n") ) ;
 rexxrc =
   stem_from_long(traceid,
                  zlist,
                  stem,
                  "VER",
                  mhbo->Version) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc =
   stem_from_long(traceid,
                  zlist,
                  stem,
                  "OPT",
                  mhbo->Options) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc =
   stem_from_string(traceid,
                    zlist,
                    stem,
                    "ZLIST",
                    zlist,
                    strlen(zlist)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 TRACE(traceid, ("Leaving make_stem_from_mhbo\n") ) ;
 return rc ;
} // End of make_stem_from_mhbo function
//
// make_pd_from_stem will return a Property Descriptor
//                 structure from the contents of a Stem Variable:
//
//                     .VER         -> Version
//                     .OPT         -> Options
//                     .SUP         -> Support
//                     .SUPPORT     -> Support alias
//                     .CTX         -> Context
//                     .CONTEXT     -> Context alias
//                     .CO          -> CopyOptions
//                     .COPYOPTIONS -> CopyOptions alias
//
void make_pd_from_stem ( MQULONG    traceid
                       , MQPD     * pd
                       , RXSTRING   stem
                       )
{
 TRACE(traceid, ("Entering make_pd_from_stem\n") ) ;
 memcpy(pd, &pd_default, sizeof(MQPD)) ;
 stem_to_long(traceid, stem, "VER"        , &pd->Version)     ;
 stem_to_long(traceid, stem, "OPT"        , &pd->Options)     ;
 stem_to_long(traceid, stem, "SUP"        , &pd->Support)     ;
 stem_to_long(traceid, stem, "SUPPORT"    , &pd->Support)     ;
 stem_to_long(traceid, stem, "CTX"        , &pd->Context)     ;
 stem_to_long(traceid, stem, "CONTEXT"    , &pd->Context)     ;
 stem_to_long(traceid, stem, "CO"         , &pd->CopyOptions) ;
 stem_to_long(traceid, stem, "COPYOPTIONS", &pd->CopyOptions) ;
 DUMPCB(traceid, pd) ;
 TRACE(traceid, ("Leaving make_pd_from_stem\n") ) ;
 return ;
} // End of make_pd_from_stem function
//
// make_stem_from_pd will return a Property Descriptor
//                 structure into a Stem Variable:
//
//                     .VER         -> Version
//                     .OPT         -> Options
//                     .SUP         -> Support
//                     .SUPPORT     -> Support alias
//                     .CTX         -> Context
//                     .CONTEXT     -> Context alias
//                     .CO          -> CopyOptions
//                     .COPYOPTIONS -> CopyOptions alias
//                     .ZLIST       -> VER OPT SUP CTX CO
//
int make_stem_from_pd ( MQULONG    traceid
                       , MQPD     * pd
                       , RXSTRING   stem
                       )
{
 int                    rc = RXSHV_OK ;
 int                    rexxrc = RXSHV_OK ;
 char                   zlist[100] ;  // Char version of .ZLIST
 zlist[0] = '\0';
 TRACE(traceid, ("Entering make_stem_from_pd\n") ) ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "VER", pd->Version)     ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "OPT", pd->Options)     ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "SUP", pd->Support)     ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "CTX", pd->Context)     ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "CO" , pd->CopyOptions) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, NULL, stem, "SUPPORT"    , pd->Support)     ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, NULL, stem, "CONTEXT"    , pd->Context)     ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, NULL, stem, "COPYOPTIONS", pd->CopyOptions) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "ZLIST", zlist, strlen(zlist)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 TRACE(traceid, ("Leaving make_stem_from_pd\n") ) ;
 return rc ;
} // End of make_stem_from_pd function
//
// make_md_from_stem will return a Put Message Options Desc from the
//                   contents of a Stem Variable:
//
//                     .VER   -> Version
//                     .REP   -> Report
//                     .MSG   -> MsgType
//                     .EXP   -> Expiry
//                     .FBK   -> Feedback
//                     .ENC   -> Encoding
//                     .CCSI  -> CodedCharSetId
//                     .FORM  -> Format
//                     .PRI   -> Priority
//                     .PER   -> Persistence
//                     .MSGID -> MsgId
//                     .CID   -> CorrelId
//                     .BC    -> BackoutCount
//                     .RTOQ  -> ReplyToQ
//                     .RTOQM -> ReplyToQMgr
//                     .UID   -> UserIdentifier
//                     .AT    -> AccountingToken
//                     .AID   -> ApplyIdentityData
//                     .PAT   -> PutApplType
//                     .PAN   -> PutApplName
//                     .PD    -> PutDate
//                     .PT    -> PutTime
//                     .AOD   -> ApplOriginData
//                     .GID   -> GroupId
//                     .MSN   -> MsgSeqNumber
//                     .OFF   -> Offset
//                     .MF    -> MsgFlags
//                     .OL    -> OriginalLength
//
//
 
void make_md_from_stem ( MQULONG    traceid      // trace id of caller
                       , MQMD2    * md           // target message descriptor
                       , RXSTRING   stem         // name of stem variable
                       )
{
 TRACE(traceid, ("Entering make_md_from_stem\n") ) ;
 
 memcpy(md, &md_default, sizeof(MQMD2))        ;
 md->Version = MQMD_VERSION_1                  ;
 
 // Version 1 of MQMD
 stem_to_long  (traceid, stem, "VER"  , &md->Version)                           ;
 stem_to_long  (traceid, stem, "REP"  , &md->Report)                            ;
 stem_to_long  (traceid, stem, "MSG"  , &md->MsgType)                           ;
 stem_to_long  (traceid, stem, "EXP"  , &md->Expiry)                            ;
 stem_to_long  (traceid, stem, "FBK"  , &md->Feedback)                          ;
 stem_to_long  (traceid, stem, "ENC"  , &md->Encoding)                          ;
 stem_to_long  (traceid, stem, "CCSI" , &md->CodedCharSetId)                    ;
 stem_to_string(traceid, stem, "FORM" ,  md->Format,           sizeof(MQCHAR8)) ;
 stem_to_long  (traceid, stem, "PRI"  , &md->Priority)                          ;
 stem_to_long  (traceid, stem, "PER"  , &md->Persistence)                       ;
 stem_to_bytes (traceid, stem, "MSGID",  md->MsgId,            sizeof(MQBYTE24));
 stem_to_bytes (traceid, stem, "CID"  ,  md->CorrelId,         sizeof(MQBYTE24));
 stem_to_long  (traceid, stem, "BC"   , &md->BackoutCount)                      ;
 stem_to_string(traceid, stem, "RTOQ" ,  md->ReplyToQ,         sizeof(MQCHAR48));
 stem_to_string(traceid, stem, "RTOQM",  md->ReplyToQMgr,      sizeof(MQCHAR48));
 stem_to_string(traceid, stem, "UID"  ,  md->UserIdentifier,   sizeof(MQCHAR12));
 stem_to_bytes (traceid, stem, "AT"   ,  md->AccountingToken,  sizeof(MQBYTE32));
 stem_to_string(traceid, stem, "AID"  ,  md->ApplIdentityData, sizeof(MQCHAR32));
 stem_to_long  (traceid, stem, "PAT"  , &md->PutApplType)                       ;
 stem_to_string(traceid, stem, "PAN"  ,  md->PutApplName,      sizeof(MQCHAR28));
 stem_to_string(traceid, stem, "PD"   ,  md->PutDate,          sizeof(MQCHAR8)) ;
 stem_to_string(traceid, stem, "PT"   ,  md->PutTime,          sizeof(MQCHAR8)) ;
 stem_to_string(traceid, stem, "AOD"  ,  md->ApplOriginData,   sizeof(MQCHAR4)) ;
 // Version 2 of MQMD
 stem_to_bytes (traceid, stem, "GID"  ,  md->GroupId,          sizeof(MQBYTE24));
 stem_to_long  (traceid, stem, "MSN"  , &md->MsgSeqNumber)                      ;
 stem_to_long  (traceid, stem, "OFF"  , &md->Offset)                            ;
 stem_to_long  (traceid, stem, "MF"   , &md->MsgFlags)                          ;
 stem_to_long  (traceid, stem, "OL"   , &md->OriginalLength)                    ;
 
 DUMPCB(traceid,  md ) ;
 TRACE(traceid, ("Leaving make_md_from_stem\n") ) ;
 
 return ;
} // End of make_md_from_stem function
 
//
// make_stem_from_md will return a Message Descriptor as the
//                   contents of a Stem Variable:
//
//                     .VER   -> Version
//                     .REP   -> Report
//                     .MSG   -> MsgType
//                     .EXP   -> Expiry
//                     .FBK   -> Feedback
//                     .ENC   -> Encoding
//                     .CCSI  -> CodedCharSetId
//                     .FORM  -> Format
//                     .PRI   -> Priority
//                     .PER   -> Persistence
//                     .MSGID -> MsgId
//                     .CID   -> CorrelId
//                     .BC    -> BackoutCount
//                     .RTOQ  -> ReplyToQ
//                     .RTOQM -> ReplyToQMgr
//                     .UID   -> UserIdentifier
//                     .AT    -> AccountingToken
//                     .AID   -> ApplyIdentityData
//                     .PAT   -> PutApplType
//                     .PAN   -> PutApplName
//                     .PD    -> PutDate
//                     .PT    -> PutTime
//                     .AOD   -> ApplOriginData
//                     .GID   -> GroupId
//                     .MSN   -> MsgSeqNumber
//                     .OFF   -> Offset
//                     .MF    -> MsgFlags
//                     .OL    -> OriginalLength
//
//
//                     .ZLIST -> 'VER REP MSG EXP FBK ENC CCSI FORM PRI PER MSGID CID BC
//                                RTOQ RTOQM UID AT AID PAT PAN PD PT AOD
//                                GID MSN OFF MF OL
//
//
 
int make_stem_from_md ( MQULONG    traceid      // trace id of caller
                       , MQMD2    * md           // source message descriptor
                       , RXSTRING   stem         // name of stem variable
                       )
{
 int                          rc = RXSHV_OK ;
 int                          rexxrc = RXSHV_OK ;
 char                         zlist[200]   ;  // Char version of .ZLIST
 zlist[0] = '\0'                           ;
 
 TRACE(traceid, ("Entering make_stem_from_md\n") ) ;
 DUMPCB(traceid,  md )                             ;
 
// Version 1 of MQMD
 rexxrc = stem_from_long  (traceid, zlist, stem, "VER"  , md->Version)                               ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "REP"  , md->Report)                                ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "MSG"  , md->MsgType)                               ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "EXP"  , md->Expiry)                                ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "FBK"  , md->Feedback)                              ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "ENC"  , md->Encoding)                              ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "CCSI" , md->CodedCharSetId)                        ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "FORM" , md->Format,              sizeof(MQCHAR8))  ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "PRI"  , md->Priority)                              ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "PER"  , md->Persistence)                           ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_bytes (traceid, zlist, stem, "MSGID", md->MsgId,               sizeof(MQBYTE24)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_bytes (traceid, zlist, stem, "CID"  , md->CorrelId,            sizeof(MQBYTE24)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "BC"   , md->BackoutCount)                          ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "RTOQ" , md->ReplyToQ,            sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "RTOQM", md->ReplyToQMgr,         sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "UID"  , md->UserIdentifier,      sizeof(MQCHAR12)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_bytes (traceid, zlist, stem, "AT"   , md->AccountingToken,     sizeof(MQBYTE32)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "AID"  , md->ApplIdentityData,    sizeof(MQCHAR32)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "PAT"  , md->PutApplType)                           ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "PAN"  , md->PutApplName,         sizeof(MQCHAR28)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "PD"   , md->PutDate,             sizeof(MQCHAR8))  ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "PT"   , md->PutTime,             sizeof(MQCHAR8))  ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "AOD"  , md->ApplOriginData,      sizeof(MQCHAR4))  ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
// Version 2 of MQMD
 rexxrc = stem_from_bytes (traceid, zlist, stem, "GID"  , md->GroupId,             sizeof(MQBYTE24)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "MSN"  , md->MsgSeqNumber)                          ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "OFF"  , md->Offset)                                ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "MF"   , md->MsgFlags)                              ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "OL"   , md->OriginalLength)                        ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 
 rexxrc = stem_from_string(traceid, zlist, stem, "ZLIST", zlist, strlen(zlist))                      ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 
 TRACE(traceid, ("Leaving make_stem_from_md\n") ) ;
 
 return rc ;
} // End of make_stem_from_md function
 
//
// make_sd_from_stem will return a Subscription Descriptor from the
//                   contents of a Stem Variable:
//
//                     .VER  -> Version
//                     .OPT  -> Options
//                     .ON   -> ObjectName
//                     .AUID -> AlternateUserId
//                     .ASID -> AlternateSecurityId
//                     .SE   -> SubExpiry
//                     .OS   -> ObjectString
//                     .SN   -> SubName
//                     .SUD  -> SubUserData
//                     .SCID -> SubCorrelId
//                     .PP   -> PubPriority
//                     .PAT  -> PubAccountingToken
//                     .PAID -> PubAppIdentityData
//                     .SS   -> SelectionString
//                     .SL   -> SubLevel
//                     .ROS  -> ResObjectString
//
 
int make_sd_from_stem ( MQULONG    traceid      // trace id of caller
                       , MQSD     * subdesc      // target MQSD
                       , RXSTRING   stem         // name of stem variable
                       )
{
 TRACE(traceid, ("Entering make_sd_from_stem") ) ;
 
 memcpy(subdesc, &sd_default, sizeof(MQSD));
 
 //The given variable is a stem. variable, so get its contents
 
 // Version 1 of MQSD
 stem_to_long  (traceid, stem, "VER" , &subdesc->Version)                               ;
 stem_to_long  (traceid, stem, "OPT" , &subdesc->Options)                               ;
 stem_to_string(traceid, stem, "ON"  ,  subdesc->ObjectName,          sizeof(MQCHAR48)) ;
 stem_to_string(traceid, stem, "AUID",  subdesc->AlternateUserId,     sizeof(MQCHAR12)) ;
 stem_to_bytes (traceid, stem, "ASID",  subdesc->AlternateSecurityId, sizeof(MQBYTE40)) ;
 stem_to_long  (traceid, stem, "SE"  , &subdesc->SubExpiry)                             ;
 if ( stem_to_strinv(traceid, stem, "OS"  , &subdesc->ObjectString,
                     RXMQ_MQCHARV_INPUT_ONLY) != 0 )
   {
    free_sd_mqcharv(subdesc) ;
    return -1 ;
   }
 if ( stem_to_strinv(traceid, stem, "SN"  , &subdesc->SubName,
                     RXMQ_MQCHARV_INPUT_ONLY) != 0 )
   {
    free_sd_mqcharv(subdesc) ;
    return -1 ;
   }
 if ( stem_to_strinv(traceid, stem, "SUD" , &subdesc->SubUserData,
                     RXMQ_MQCHARV_CAPACITY_REQUIRED) != 0 )
   {
    free_sd_mqcharv(subdesc) ;
    return -1 ;
   }
 stem_to_bytes (traceid, stem, "SCID",  subdesc->SubCorrelId,         sizeof(MQBYTE24)) ;
 stem_to_long  (traceid, stem, "PP"  , &subdesc->PubPriority)                           ;
 stem_to_bytes (traceid, stem, "PAT" ,  subdesc->PubAccountingToken,  sizeof(MQBYTE32)) ;
 stem_to_string(traceid, stem, "PAID",  subdesc->PubApplIdentityData, sizeof(MQCHAR32)) ;
 if ( stem_to_strinv(traceid, stem, "SS"  , &subdesc->SelectionString,
                     RXMQ_MQCHARV_CAPACITY_REQUIRED) != 0 )
   {
    free_sd_mqcharv(subdesc) ;
    return -1 ;
   }
 stem_to_long  (traceid, stem, "SL"  , &subdesc->SubLevel)                              ;
 if ( stem_to_strinv(traceid, stem, "ROS" , &subdesc->ResObjectString,
                     RXMQ_MQCHARV_CAPACITY_REQUIRED) != 0 )
   {
    free_sd_mqcharv(subdesc) ;
    return -1 ;
   }
 
 DUMPCB(traceid, subdesc) ;
 TRACE(traceid, ("Leaving make_sd_from_stem\n") ) ;
 
 return 0 ;
} // End of make_sd_from_stem function
 
//
// make_stem_from_sd will return a Subscription Descriptor as the
//                   contents of a Stem Variable:
//
//                     .VER  -> Version
//                     .OPT  -> Options
//                     .ON   -> ObjectName
//                     .AUID -> AlternateUserId
//                     .ASID -> AlternateSecurityId
//                     .SE   -> SubExpiry
//                     .OS   -> ObjectString
//                     .SN   -> SubName
//                     .SUD  -> SubUserData
//                     .SCID -> SubCorrelId
//                     .PP   -> PubPriority
//                     .PAT  -> PubAccountingToken
//                     .PAID -> PubAppIdentityData
//                     .SS   -> SelectionString
//                     .SL   -> SubLevel
//                     .ROS  -> ResObjectString
//
//                     .ZLIST -> 'VER OPT ON AUID ASID
//                                SE OS. SN. SUD. SCID
//                                PP PAT PAID
//                                SS. SL ROS.
//
 
int make_stem_from_sd ( MQULONG    traceid      // trace id of caller
                       , MQSD     * subdesc      // source MQSD
                       , RXSTRING   stem         // name of stem variable
                       )
{
 int                         rc = RXSHV_OK ;
 int                         rexxrc = RXSHV_OK ;
 char                        zlist[200]   ;  // Char version of .ZLIST
 zlist[0] = '\0'                          ;
 
 TRACE(traceid, ("Entering make_stem_from_sd\n") ) ;
 DUMPCB(traceid, subdesc)                          ;
 
 // Version 1 of MQSD
 rexxrc = stem_from_long  (traceid, zlist, stem, "VER" , subdesc->Version)                               ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "OPT" , subdesc->Options)                               ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "ON"  , subdesc->ObjectName,          sizeof(MQCHAR48)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "AUID", subdesc->AlternateUserId,     sizeof(MQCHAR12)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_bytes (traceid, zlist, stem, "ASID", subdesc->AlternateSecurityId, sizeof(MQBYTE40)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "SE"  , subdesc->SubExpiry)                             ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_strinv(traceid, zlist, stem, "OS"  , &subdesc->ObjectString)                          ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_strinv(traceid, zlist, stem, "SN"  , &subdesc->SubName)                               ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_strinv(traceid, zlist, stem, "SUD" , &subdesc->SubUserData)                           ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_bytes (traceid, zlist, stem, "SCID", subdesc->SubCorrelId,         sizeof(MQBYTE24)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "PP"  , subdesc->PubPriority)                           ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_bytes (traceid, zlist, stem, "PAT" , subdesc->PubAccountingToken,  sizeof(MQBYTE32)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_string(traceid, zlist, stem, "PAID", subdesc->PubApplIdentityData, sizeof(MQCHAR32)) ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_strinv(traceid, zlist, stem, "SS"  , &subdesc->SelectionString)                       ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_long  (traceid, zlist, stem, "SL"  , subdesc->SubLevel)                              ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 rexxrc = stem_from_strinv(traceid, zlist, stem, "ROS" , &subdesc->ResObjectString)                       ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 
 rexxrc = stem_from_string(traceid, zlist, stem, "ZLIST", zlist, strlen(zlist))                         ;
 if (    (rexxrc != RXSHV_OK)
      && (rexxrc != RXSHV_NEWV)
      && (    (rc == RXSHV_OK)
           || (rc == RXSHV_NEWV) ) )
   rc = rexxrc ;
 
 TRACE(traceid, ("Leaving make_stem_from_sd\n") ) ;
 
 return rc ;
} // End of make_stem_from_sd function
 
 
//
// Genuine Internal functions, not concerned with specific things
//
 
//
// setcons - sets up the MQ constants in the Rexx Variable Space
//           Traced by INIT setting
//
 
int setcons (MQULONG traceid)
 {
 
//
// Structure to define MQ literals, to be setup in Variable Space
//
 
 struct s_define_mq_ints
        {
          char           s_define_mq_ints_name[255] ; // Constant name
          int32_t        s_define_mq_ints_value     ; // Constant value
          uint32_t       s_define_mq_ints_type      ; // 0 -> number
                                                      // 1 -> MQRC_
                                                      // 2 -> not used
                                                      // 3 -> MQCC_
                                                      // 4 -> MQCA_
        } ;
 
 typedef struct s_define_mq_ints   t_define_mq_ints ;
 typedef        t_define_mq_ints * p_define_mq_ints ;
 
 struct s_define_mq_str
        {
          char           s_define_mq_str_name[32]   ;  // 32 seems to be max
          char           s_define_mq_str_value[9]   ;  // 8 is biggest so far
          MQULONG        s_define_mq_str_type       ;  // 0 -> char
        } ;
 
 typedef struct s_define_mq_str   t_define_mq_str  ;
 typedef        t_define_mq_str * p_define_mq_str  ;
 
 struct s_define_mq_byte
        {
          char           s_define_mq_byte_name[32]  ;  // 32 seems to be max
          MQBYTE         s_define_mq_byte_value[33] ;  // 33 is biggest so far
          MQULONG        s_define_mq_byte_size      ;
        } ;
 
 typedef struct s_define_mq_byte   t_define_mq_byte ;
 typedef        t_define_mq_byte * p_define_mq_byte ;
 
 struct s_define_mq_char
        {
          char           s_define_mq_char_name[32]  ;  // 32 seems to be max
          char           s_define_mq_char_value     ;  // 1-byte char
          MQULONG        s_define_mq_char_type      ;  // 0 -> char
        } ;
 
 typedef struct s_define_mq_char   t_define_mq_char ;
 typedef        t_define_mq_char * p_define_mq_char ;
 
 //
 //  Usual local variables
 //
 
 MQULONG                  i                ;  // Looper
 p_define_mq_ints         pi               ;  // Table pointer
 p_define_mq_str          ps               ;  // Table pointer
 p_define_mq_byte         pb               ;  // Table pointer
 p_define_mq_char         pc               ;  // Table pointer
 RXSTRING                 varname          ;  // Variable name
 RXSTRING                 varname_new      ;  // Variable name
 RXSTRING                 varname_old      ;  // Variable name
 char                     varvalc[100]     ;  // Char version of variable
 int                      rc = RXSHV_OK    ;
 int                      rexxrc = RXSHV_OK ;
 
//
// Array and Structure to define MQ literals, to be setup in Variable Space
// Constants are placed in separate header file for ease of use
//
 
#include <rxmqcons.h>
 
 TRACE(traceid, ("Entering setcons\n") ) ;
 
 //
 // initialize all the constants required.
 //
 // This is done by looping through the table of names and
 //      settings, invoking the RexxVariable interface to set up
 //      each literal.
 //
 // Additionally, if the constant represents Completion Code (MQCC_)
 //               then create an number->name entry in the
 //               RXMQ.CCMAP. stem variable
 //
 // Additionally, if the constant represents Reason Code (MQRC_)
 //               then create an number->name entry in the
 //               RXMQ.RCMAP. stem variable
 //
 // Additionally, if the constant represents Event (MQCA_,...)
 //               then create an number->name entry in the
 //               RXMQ.CAMAP. stem variable
 //
 
 MAKERXSTRING(varname_new, "RXMQ.", sizeof("RXMQ.")-1)  ;
 MAKERXSTRING(varname_old, PREFIX,  sizeof(PREFIX)-1)   ;
 
 for (i=0; ; i++)
   {
    pi = &define_mq_ints[i]                           ;
    if ( pi->s_define_mq_ints_name[0] == '?' )  break ;
 
    MAKERXSTRING(varname,
                 &(pi->s_define_mq_ints_name[0]),
                 strlen(pi->s_define_mq_ints_name))         ; // REXX variable name
    rexxrc =
      stem_from_long  (traceid, NULL, varname, "", pi->s_define_mq_ints_value);
    if (    (rexxrc != RXSHV_OK)
         && (rexxrc != RXSHV_NEWV)
         && (    (rc == RXSHV_OK)
              || (rc == RXSHV_NEWV) ) )
      rc = rexxrc ;
 
    if ( pi->s_define_mq_ints_type == 1 )    //Only for MQRC_ stuff
     {
      sprintf(varvalc, "RCMAP.%"PRId32, pi->s_define_mq_ints_value)   ;
      rexxrc =
        stem_from_string(traceid, NULL, varname_new, varvalc,
                         &(pi->s_define_mq_ints_name[0]),
                         strlen(pi->s_define_mq_ints_name))   ; // like RXMQ.RCMAP.value
      if (    (rexxrc != RXSHV_OK)
           && (rexxrc != RXSHV_NEWV)
           && (    (rc == RXSHV_OK)
                || (rc == RXSHV_NEWV) ) )
        rc = rexxrc ;
      rexxrc =
        stem_from_string(traceid, NULL, varname_old, varvalc,
                         &(pi->s_define_mq_ints_name[0]),
                         strlen(pi->s_define_mq_ints_name))   ; // like RXMQV.RCMAP.value
      if (    (rexxrc != RXSHV_OK)
           && (rexxrc != RXSHV_NEWV)
           && (    (rc == RXSHV_OK)
                || (rc == RXSHV_NEWV) ) )
        rc = rexxrc ;
      }
 
    if (    (pi->s_define_mq_ints_type == 3)
         && (pi->s_define_mq_ints_value >= 0) )    //Only for MQCC_ stuff
     {
      sprintf(varvalc, "CCMAP.%"PRId32, pi->s_define_mq_ints_value)   ;
      rexxrc =
        stem_from_string(traceid, NULL, varname_new, varvalc,
                         &(pi->s_define_mq_ints_name[0]),
                         strlen(pi->s_define_mq_ints_name))   ; // like RXMQ.CCMAP.value
      if (    (rexxrc != RXSHV_OK)
           && (rexxrc != RXSHV_NEWV)
           && (    (rc == RXSHV_OK)
                || (rc == RXSHV_NEWV) ) )
        rc = rexxrc ;
     }
 
    if ( pi->s_define_mq_ints_type == 4 )    //Only for Selector/Event Attributes
     {
      sprintf(varvalc, "CAMAP.%"PRId32, pi->s_define_mq_ints_value)   ;
 
      varname.strptr    = strstr(&(pi->s_define_mq_ints_name[0]), "_") + 1 ;
      varname.strlength = strlen(varname.strptr)            ;
 
      rexxrc =
        stem_from_string(traceid, NULL, varname_new, varvalc,
                         varname.strptr, varname.strlength)   ; // like RXMQ.CAMAP.value
      if (    (rexxrc != RXSHV_OK)
           && (rexxrc != RXSHV_NEWV)
           && (    (rc == RXSHV_OK)
                || (rc == RXSHV_NEWV) ) )
        rc = rexxrc ;
     }
   } // End of Integer initializations Loop
 
 for (i=0; ; i++)
   {
    ps = &define_mq_str[i]                           ;
    if ( ps->s_define_mq_str_name[0] == '?' ) break  ;
 
    MAKERXSTRING(varname,
                 &(ps->s_define_mq_str_name[0]),
                 strlen(ps->s_define_mq_str_name))         ; // REXX variable name
    rexxrc =
      stem_from_string(traceid, NULL, varname, "",
                       &(ps->s_define_mq_str_value[0]),
                       strlen(ps->s_define_mq_str_value))    ;
    if (    (rexxrc != RXSHV_OK)
         && (rexxrc != RXSHV_NEWV)
         && (    (rc == RXSHV_OK)
              || (rc == RXSHV_NEWV) ) )
      rc = rexxrc ;
   } // End of String initializations Loop
 
 for (i=0; ; i++)
   {
    pb = &define_mq_byte[i]                           ;
    if ( pb->s_define_mq_byte_name[0] == '?' ) break  ;
 
    MAKERXSTRING(varname,
                 &(pb->s_define_mq_byte_name[0]),
                 strlen(pb->s_define_mq_byte_name))       ; // REXX variable name
    rexxrc =
      stem_from_bytes(traceid, NULL, varname, "",
                      &(pb->s_define_mq_byte_value[0]),
                      pb->s_define_mq_byte_size)            ;
    if (    (rexxrc != RXSHV_OK)
         && (rexxrc != RXSHV_NEWV)
         && (    (rc == RXSHV_OK)
              || (rc == RXSHV_NEWV) ) )
      rc = rexxrc ;
   } // End of Byte initializations Loop
 
 for (i=0; ; i++)
   {
    pc = &define_mq_char[i]                           ;
    if ( pc->s_define_mq_char_name[0] == '?' ) break  ;
 
    MAKERXSTRING(varname,
                 &(pc->s_define_mq_char_name[0]),
                 strlen(pc->s_define_mq_char_name))   ; // REXX variable name
    rexxrc =
      stem_from_char(traceid, NULL, varname, "",
                     (pc->s_define_mq_char_value))      ;
    if (    (rexxrc != RXSHV_OK)
         && (rexxrc != RXSHV_NEWV)
         && (    (rc == RXSHV_OK)
              || (rc == RXSHV_NEWV) ) )
      rc = rexxrc ;
   } // End of Character initializations Loop
 
 TRACE(traceid, ("Leaving setcons\n") ) ;
 
 return rc ;
 
 } // End of setcons function
 
//
// Routine geteventname - converts the NUMBER of an Event into
//                        a name (for use as a Component Variable),
//                        which is defined event name like MQCA_...
//                        without MQCA_ prefix itself
//
//         If the PCF field number is unknown, then the
//            component is set to the character version of the
//            PCF event number. This only works for EVENT PCFs!!!!
//
//         Tracing is via the EVENT setting
//
//
 
void geteventname(char * output, const MQLONG pcfnum )
 {
  char                    varnamc[100] ;  // Char version of var name
  char                    varvalc[31]  ;  // Char version of var value
  SHVBLOCK                sv1          ;  // Var interface CB
  MQULONG                 sv1rc        ;  // Var interface RC
 
  // TRACE(EVENT, ("Entering geteventname for %"PRId32"\n",(int32_t)pcfnum) ) ;
 
  sv1.shvnext     = 0           ; // Fetch only one variable
  sv1.shvcode     = RXSHV_SYFET ; // Fetch operation
  sv1.shvret      = 0           ; // Zero out RC
 
  sprintf(varnamc,"RXMQ.CAMAP.%"PRId32,(int32_t)pcfnum) ; // Construct REXX variable name
  MAKERXSTRING(sv1.shvname,varnamc,strlen(varnamc))     ; // Construct REXX variable name structure
  sv1.shvnamelen  = sv1.shvname.strlength               ; // REXX variable name length
 
  sv1.shvvalue.strptr   = varvalc                     ; // Set pointer to value buffer for REXX
  sv1.shvvalue.strlength= 30                          ; // Set max value length for REXX
  sv1.shvvaluelen       = 30                          ; // Set max accepted value length
 
  sv1rc           = RexxVariablePool(&sv1)            ; // Call REXX variable interface
 
  if (    (sv1rc                  == RXSHV_OK)
       && (sv1.shvret             == RXSHV_OK)
       && (sv1.shvvalue.strlength < sizeof(varvalc)) )
    {
     varvalc[sv1.shvvalue.strlength] = 0               ; // Ensure zero terminated
    }
  else
    {
     sprintf(varvalc,"%"PRId32,(int32_t)pcfnum)        ; // When something goes wrong
    }
 
  // TRACE(EVENT, ("Converted %"PRId32" into %s\n",(int32_t)pcfnum,varvalc) )  ;
 
  strcat(output, varvalc)                             ; // Concatenate
 
  return ;
 
 } // End of geteventname function
 
//
// External Functions, callable from Rexx
//
 
//
// initialize the interface - RXMQINIT
//
//    For Windows, AIX, Linux this function installs the other functions of the DLL.
//    It MUST be called before any of the other Rexx/MQ functions are
//    available for usage.
//
//    This is done by a RxFuncAdd('RXMQINIT','RXMQx',RXMQINIT')
//         call to let Rexx know about the DLL, and then a
//         rcc = RXMQINIT() to call the initialization function.
//
//    The following things are done:
//
//        * Create Rexx variables for ALL known MQ Constants
//        * Create number->name translation variables for MQ ACs
//        * initialize the DLL Global variables
//            (including obtaining the Mutex for Conn/Open/Close processing)
//        * Make the RXMQ... functions known to Rexx
//
//
//
FTYPE  RXMQINIT  RXMQPARM
 {
 
 RXMQCB                 * anchor = 0       ;  // RXMQ Control Block
 int                      i                ;  // Looper
 int                      rc = 0           ;  // Function Return Code
 int                      rexxrcCons = RXSHV_OK ;
 MQLONG                   mqrc = 0         ;  // MQ RC
 MQLONG                   mqac = 0         ;  // MQ AC
 MQULONG                  traceid = INIT   ;  // This function trace id
 
 RETMSG ReturnMsg[] = {
        {  -1, "Unable to publish MQ constants to REXX"},
        { -99, "UNKNOWN FAILURE"}} ;
 //
 // Prepare Copyright message
 //
 char * moremsg = "WMQ Support Pac MA95. Version 2.0 "
                  "(C) Copyright IBM Corporation. 1997, 2012.\n";
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
 //
 // Now, initialize all the constants required by calling setcons.
 //
 // Additionally, if the constant represents Reason Code (MQRC_)
 //               then create an number->name entry in the
 //               RXMQ.RCMAP. stem variable
 //
 // Additionally, if the constant represents Completion Code (MQCC_)
 //               then create an number->name entry in the
 //               RXMQ.CCMAP. stem variable
 //
 // Additionally, if the constant represents Event Name (MQCA_,...)
 //               then create an number->name entry in the
 //               RXMQ.CAMAP. stem variable
 //
 if ( rc == 0 )
   {
    rexxrcCons = setcons(traceid) ;
    if (    (rexxrcCons != RXSHV_OK)
         && (rexxrcCons != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("setcons failed to publish constants rc = %d\n",
              rexxrcCons) ) ;
      }
   }
 
#ifndef __MVS__
 //
 // Register the DLL functions in Windows/Linux/AIX
 //
 if ( rc == 0 )
   {
    for (i= 0; i< numfuncs ; i++ )
      {
       rc = RexxRegisterFunctionDll(        funcs[i],
                                      (PSZ) THISDLL,
                                            funcs[i]) ;
       TRACE(traceid, ("Registration of %s, rc = %d\n",funcs[i],rc) ) ;
      }
   }
#endif

 if (    (rc == 0)
      && (rexxrcCons != RXSHV_OK)
      && (rexxrcCons != RXSHV_NEWV) )
   {
    rc = -1 ;
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,moremsg) ;
 
return 0 ;
 } // End of RXMQINIT function
 
//
// RXMQCONS will simply setup the MQ mappings in the Rexx Workspace.
//
//           This is useful in a thread based environment where RXMQINIT
//                has been done elsewhere (so setting the mappings elsewhere)
//                to act, essentially, as the initialization for the thread
//
FTYPE  RXMQCONS  RXMQPARM
 {
 RXMQCB                 * anchor = 0       ;  // RXMQ Control Block
 MQLONG                   rc = 0           ;  // Function Return Code
 MQLONG                   mqrc = 0         ;  // MQ RC
 MQLONG                   mqac = 0         ;  // MQ AC
 MQULONG                  traceid = INIT   ;  // This function trace id
 int                      rexxrcCons = RXSHV_OK ;
 
 RETMSG ReturnMsg[] = {
        {  -1, "Unable to publish MQ constants to REXX"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
 //
 // Just initialize all the constants etc. required by calling setcons.
 //
 if ( rc == 0 )
   {
    rexxrcCons = setcons(traceid) ;

    if (    (rexxrcCons != RXSHV_OK)
         && (rexxrcCons != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("setcons failed to publish constants rc = %d\n",
              rexxrcCons) ) ;

       if (rc == 0)
         rc = -1 ;
      }
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQCONS function
 
//
// Terminate the interface - RXMQTERM
//
//    For Windows, AIX, Linux only this function removes access to the functions of the DLL.
//         After calling, no other Rexx/MQ functions are available.
//
//    However, all the REXX name bindings are left in the
//         Workspace; this is done to provide a method of assembling
//         stem variable with the correct settings.
//
//    NO end of process actions are taken. If a prior Syncpoint is
//         not taken, or the Queue Closed, or the QM Disconnected
//         before this routine is invoked, then the usual MQ
//         End of Process action will occur at that time.
//
//
FTYPE  RXMQTERM  RXMQPARM
 {
 
 RXMQCB                 * anchor = 0       ;  // RXMQ Control Block
 int                      i                ;  // Looper
 int                      rc = 0           ;  // Function Return Code
 MQLONG                   mqrc = 0         ;  // MQ RC
 MQLONG                   mqac = 0         ;  // MQ AC
 MQULONG                  traceid = TERM   ;  // This function trace id
 
 RETMSG ReturnMsg[] = {
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
#ifndef __MVS__
//
// Deregister the DLL functions on distributed
//
 
 for (i= 0; i< numfuncs ; i++ )
   {
    rc = RexxDeregisterFunction(funcs[i]) ;
    TRACE(traceid, ("Deregistration of %s, rc = %d\n",funcs[i],rc) ) ;
   }
#endif
 
 
// Return a good return Code
 
// strcpy((char *)returnc,"0 0 0 RXMQTERM OK Rexx MQ Interface terminated." );
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
#ifdef __MVS__
 return 4;
#else
 return 0;
#endif
 } // End of RXMQTERM function
 
//
// Do a Connect MQCONN
//
//   Call:   rc = RXMQconn(qmname)
//
FTYPE  RXMQCONN  RXMQPARM
 {
 
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = CONN   ;  // This function trace id
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null QM name"},
        {  -3, "Zero length QM name"},
        {  -4, "QM name too long"},
        { -98, "Already Connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 1 ) )              rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )     rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) )  rc =  -3 ;
 if ( (rc == 0) && ( aargv[0].strlength >
                     MQ_Q_MGR_NAME_LENGTH ) )   rc =  -4 ;
//
// Is this thread already connected ?
//
 if ( (rc == 0) && ( anchor->QMh != 0) )        rc = -98 ;
 
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(anchor->QMname,aargv[0].strptr,aargv[0].strlength)        ;
    TRACE(traceid, ("Requested connection with %.*s\n",
                    (int)sizeof(anchor->QMname),anchor->QMname) ) ;
   }
 
//
// Do the MQCONN
//
 if (rc == 0)
   {
    MQCONN ( anchor->QMname, &anchor->QMh, &mqrc, &mqac ) ;
    rc = mqrc                                             ;
    TRACE(traceid, ("MQCONN handle is %"PRIX32", QM is %.*s\n",
          (uint32_t)anchor->QMh,(int)sizeof(anchor->QMname),anchor->QMname) ) ;
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQCONN function
 
//
// Do a Disconnect   MQDISC
//
//   Call:   rc = RXMQdisc()
//
FTYPE  RXMQDISC  RXMQPARM
 {
 
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = DISC   ;  // This function trace id
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        { -98, "Not Connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && ( aargc != 0 ) )        rc =  -1 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )  rc = -98 ;
 
//
// Do the MQDISC
//
 if (rc == 0)
   {
    TRACE(traceid, ("Disconnecting from QM %.*s\n",
                    (int)sizeof(anchor->QMname),anchor->QMname) ) ;
    MQDISC ( &anchor->QMh, &mqrc, &mqac ) ;
    rc = mqrc                             ;
    memset (anchor, 0, sizeof(RXMQCB))   ;
    memcpy (&anchor->StrucId, RXMQeyecatcher, sizeof(MQCHAR4)) ;
   }
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQDISC function
 
//
//
// Create a message handle     MQCRTMH
//
//   Call:   rc = RXMQmh(handle)
//           rc = RXMQmh(handle, cmho)
//
//           handle : Rexx variable receiving MQHMSG, e.g. 'mh1'
//           cmho   : optional MQCMHO stem, e.g. 'cmho1.'
//
FTYPE  RXMQMH  RXMQPARM
{
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQLONG          cleanup_mqrc = 0         ;  // Cleanup MQ completion code
 MQLONG          cleanup_mqac = 0         ;  // Cleanup MQ reason code
 MQULONG                 traceid = MH     ;  // This function trace id
 int                     rexxrc = RXSHV_OK;  // REXX var interface RC
 int                     rexxrcCmho = RXSHV_OK ;
 int                     resetrc          ;  // Best-effort reset RC
 RXSTRING                RX_handle        ;  // Rexx variable receiving HMSG
 RXSTRING                RX_cmho          ;  // MQCMHO input/output stem
  RXSTRING                RXMQ_new         ;  // RXMQ. stem
  RXSTRING                RXMQ_old         ;  // PREFIX stem
  MQHMSG                  hmsg = MQHM_NONE ;  // Message handle
  MQCMHO                  cmho             ;  // Create message handle options
  MQDMHO                  dmho             ;  // Delete message handle options
  RETMSG ReturnMsg[] = {
         {  -1, "Bad number of parameters"},
         {  -2, "Null handle name"},
         {  -3, "Zero length handle name"},
         {  -4, "Null CMHO"},
         {  -5, "Zero length CMHO"},
         {  -6, "RexxVariablePool failed to publish message handle"},
         {  -7, "Unable to publish output CMHO to REXX"},
         { -77, "Unable to allocate RXMQ control block"},
         { -99, "UNKNOWN FAILURE"}};
  rc = set_envir (afuncname, &traceid, &anchor) ;
 //
 // Check the parms
 //
  if ( (rc == 0) && ( (aargc < 1) || (aargc > 2) ) ) rc = -1 ;
  if ( (rc == 0) && RXNULLSTRING(aargv[0]) )         rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) )      rc = -3 ;
 if ( (rc == 0) && (aargc == 2) && RXNULLSTRING(aargv[1]) )    rc = -4 ;
 if ( (rc == 0) && (aargc == 2) && RXZEROLENSTRING(aargv[1]) ) rc = -5 ;
//
// No connection: return a real MQRC so existing REXX rcmap logic works
//
 if ( (rc == 0) && ( anchor->QMh == 0 ) )
   {
    mqrc = MQCC_FAILED ;
    mqac = MQRC_HCONN_ERROR ;
    rc   = mqrc ;
   }
//
// Now the parms are correct, get them
//
  if (rc == 0)
   {
    memcpy(&RX_handle, &aargv[0], sizeof(RX_handle)) ;
    rexxrc = stem_from_int64(traceid, NULL, RX_handle, "", MQHM_NONE) ;
    if ( (rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV) )
      {
       TRACE(traceid, ("RexxVariablePool failed to initialize HMSG rc = %d\n",
                       rexxrc) ) ;
       rc = -6 ;
      }
    TRACE(traceid, ("RX_handle = %.*s\n",
          (int)RX_handle.strlength, RX_handle.strptr) ) ;
    if ( (rc == 0) && (aargc == 1) )
      memcpy(&cmho, &cmho_default, sizeof(MQCMHO)) ;
    if ( (rc == 0) && (aargc == 2) )
      {
       memcpy(&RX_cmho, &aargv[1], sizeof(RX_cmho)) ;
       TRACE(traceid, ("RX_cmho = %.*s\n",
             (int)RX_cmho.strlength, RX_cmho.strptr) ) ;
       make_cmho_from_stem(traceid, &cmho, RX_cmho) ;
      }
   }
//
// Do the MQCRTMH
//
 if (rc == 0)
   {
    TRACE(traceid, ("Calling MQCRTMH\n") ) ;
    MQCRTMH ( anchor->QMh,
              &cmho,
              &hmsg,
              &mqrc,
              &mqac ) ;
    rc = mqrc ;
   }
//
// If it worked, return the message handle value
// created by MQCRTMH.
//
  if (rc == 0)
   {
    MAKERXSTRING(RXMQ_new, "RXMQ.", sizeof("RXMQ.")-1) ;
    MAKERXSTRING(RXMQ_old, PREFIX,  sizeof(PREFIX)-1)  ;
    rexxrc = stem_from_int64(traceid, NULL, RXMQ_new, "HMSG", (MQINT64)hmsg) ;
    if ( (rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV) ) rc = -6 ;

    if (rc == 0)
      {
       rexxrc = stem_from_int64(traceid, NULL, RXMQ_old, "HMSG", (MQINT64)hmsg) ;
       if ( (rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV) ) rc = -6 ;
      }

    if (rc == 0)
      {
       rexxrc = stem_from_int64(traceid, NULL, RX_handle, "", (MQINT64)hmsg) ;
       if ( (rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV) ) rc = -6 ;
      }

    if (rc == -6)
      {
       TRACE(traceid, ("RexxVariablePool failed to publish HMSG rc = %d\n",
                       rexxrc) ) ;
       memcpy(&dmho, &dmho_default, sizeof(MQDMHO)) ;
       TRACE(traceid, ("Calling MQDLTMH to clean up unpublished HMSG\n") ) ;
       MQDLTMH ( anchor->QMh,
                 &hmsg,
                 &dmho,
                 &cleanup_mqrc,
                 &cleanup_mqac ) ;
       if (cleanup_mqrc != MQCC_OK)
         TRACE(traceid, ("MQDLTMH cleanup failed cc = %"PRId32", reason = %"PRId32"\n",
                         (int32_t)cleanup_mqrc,(int32_t)cleanup_mqac) ) ;

       resetrc = stem_from_int64(traceid, NULL, RXMQ_new, "HMSG", MQHM_NONE) ;
       if ( (resetrc != RXSHV_OK) && (resetrc != RXSHV_NEWV) )
         TRACE(traceid, ("Failed to reset RXMQ.HMSG rc = %d\n",resetrc) ) ;
       resetrc = stem_from_int64(traceid, NULL, RXMQ_old, "HMSG", MQHM_NONE) ;
       if ( (resetrc != RXSHV_OK) && (resetrc != RXSHV_NEWV) )
         TRACE(traceid, ("Failed to reset PREFIX.HMSG rc = %d\n",resetrc) ) ;
       resetrc = stem_from_int64(traceid, NULL, RX_handle, "", MQHM_NONE) ;
       if ( (resetrc != RXSHV_OK) && (resetrc != RXSHV_NEWV) )
         TRACE(traceid, ("Failed to reset user HMSG rc = %d\n",resetrc) ) ;
      }

    if ( (rc == 0) && (aargc == 2) )
      {
       rexxrcCmho =
         make_stem_from_cmho(traceid,
                             &cmho,
                             RX_cmho) ;
       if ( (rexxrcCmho != RXSHV_OK) &&
            (rexxrcCmho != RXSHV_NEWV) )
         {
          TRACE(traceid,
                ("RexxVariablePool failed to publish RX_cmho rc = %d\n",
                 rexxrcCmho) ) ;
          if (rc == 0)
            rc = -7 ;
         }
      }
   }
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
return 0;
} // End of RXMQMH function
//
//
// Delete a message handle     MQDLTMH
//
//   Call:   rc = RXMQdmh(handle)
//           rc = RXMQdmh(handle, dmho)
//
//           handle : Rexx variable containing MQHMSG, e.g. 'mh1'
//           dmho   : optional MQDMHO stem, e.g. 'dmho1.'
//
FTYPE  RXMQDMH  RXMQPARM
{
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = DMH    ;  // This function trace id
 int                     rexxrcDmhoInit = RXSHV_OK ;
 int                     rexxrcHandle   = RXSHV_OK ;
 int                     rexxrcNew      = RXSHV_OK ;
 int                     rexxrcOld      = RXSHV_OK ;
 int                     rexxrcDmho     = RXSHV_OK ;
 RXSTRING                RX_handle        ;  // Rexx variable containing HMSG
 RXSTRING                RX_dmho          ;  // MQDMHO input/output stem
 RXSTRING                RXMQ_new         ;  // RXMQ. stem
 RXSTRING                RXMQ_old         ;  // PREFIX stem
 MQHMSG                  hmsg = MQHM_NONE ;  // Message handle
 MQINT64                 hmsg64 = 0       ;  // Intermediate value
 MQDMHO                  dmho             ;  // Delete message handle options
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters"},
        {  -2, "Null handle name"},
        {  -3, "Zero length handle name"},
        {  -4, "Null DMHO"},
        {  -5, "Zero length DMHO"},
        {  -6, "Unable to publish output to REXX"},
        { -99, "UNKNOWN FAILURE"}} ;
 rc = set_envir (afuncname, &traceid, &anchor) ;
//
// Check the parms
//
 if ( (rc == 0) && ( (aargc < 1) || (aargc > 2) ) ) rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )        rc = -2 ;
  if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) )      rc = -3 ;
  if ( (rc == 0) && (aargc == 2) && RXNULLSTRING(aargv[1]) )    rc = -4 ;
  if ( (rc == 0) && (aargc == 2) && RXZEROLENSTRING(aargv[1]) ) rc = -5 ;
 //
 // No connection: return a real MQRC so existing REXX rcmap logic works
 //
  if ( (rc == 0) && ( anchor->QMh == 0 ) )
    {
     mqrc = MQCC_FAILED ;
     mqac = MQRC_HCONN_ERROR ;
     rc   = mqrc ;
    }
 //
 // Now the parms are correct, get them
 //
  if (rc == 0)
    {
     memcpy(&RX_handle, &aargv[0], sizeof(RX_handle)) ;
    TRACE(traceid, ("RX_handle = %.*s\n",
          (int)RX_handle.strlength, RX_handle.strptr) ) ;
    stem_to_int64(traceid, RX_handle, "", &hmsg64) ;
    hmsg = (MQHMSG)hmsg64 ;
   memcpy(&dmho, &dmho_default, sizeof(MQDMHO)) ;
 if (aargc == 2)
  {
   memcpy(&RX_dmho, &aargv[1], sizeof(RX_dmho)) ;
   TRACE(traceid, ("RX_dmho = %.*s\n",
         (int)RX_dmho.strlength, RX_dmho.strptr) ) ;
   make_dmho_from_stem(traceid, &dmho, RX_dmho) ;
   rexxrcDmhoInit =
     make_stem_from_dmho(traceid,
                         &dmho,
                         RX_dmho) ;
   if ( (rexxrcDmhoInit != RXSHV_OK) &&
        (rexxrcDmhoInit != RXSHV_NEWV) )
     {
      TRACE(traceid,
            ("RexxVariablePool failed to publish initial RX_dmho rc = %d\n",
             rexxrcDmhoInit) ) ;
      if (rc == 0)
        rc = -6 ;
     }
  }
 
   }
//
// Do the MQDLTMH
//
 if (rc == 0)
   {
    TRACE(traceid, ("Calling MQDLTMH\n") ) ;
    MQDLTMH ( anchor->QMh,
              &hmsg,
              &dmho,
              &mqrc,
              &mqac ) ;
    rc = mqrc ;
   }
//
// If it worked, return the message handle value
// actually supplied by MQDLTMH.
//
 if (rc == 0)
   {
    MAKERXSTRING(RXMQ_new, "RXMQ.", sizeof("RXMQ.")-1) ;
    MAKERXSTRING(RXMQ_old, PREFIX,  sizeof(PREFIX)-1)  ;
    rexxrcHandle =
      stem_from_int64(traceid,
                      NULL,
                      RX_handle,
                      "",
                      (MQINT64)hmsg) ;
    if ( (rexxrcHandle != RXSHV_OK) &&
         (rexxrcHandle != RXSHV_NEWV) )
      TRACE(traceid,
            ("RexxVariablePool failed to publish RX_handle rc = %d\n",
             rexxrcHandle) ) ;
    rexxrcNew =
      stem_from_int64(traceid,
                      NULL,
                      RXMQ_new,
                      "HMSG",
                      (MQINT64)hmsg) ;
    if ( (rexxrcNew != RXSHV_OK) &&
         (rexxrcNew != RXSHV_NEWV) )
      TRACE(traceid,
            ("RexxVariablePool failed to publish RXMQ.HMSG rc = %d\n",
             rexxrcNew) ) ;
    rexxrcOld =
      stem_from_int64(traceid,
                      NULL,
                      RXMQ_old,
                      "HMSG",
                      (MQINT64)hmsg) ;
    if ( (rexxrcOld != RXSHV_OK) &&
         (rexxrcOld != RXSHV_NEWV) )
      TRACE(traceid,
            ("RexxVariablePool failed to publish PREFIX.HMSG rc = %d\n",
             rexxrcOld) ) ;
    if (aargc == 2)
    {
      rexxrcDmho =
        make_stem_from_dmho(traceid,
                            &dmho,
                            RX_dmho) ;
      if ( (rexxrcDmho != RXSHV_OK) &&
           (rexxrcDmho != RXSHV_NEWV) )
        TRACE(traceid,
              ("RexxVariablePool failed to publish RX_dmho rc = %d\n",
               rexxrcDmho) ) ;
    }
    if (    (rc == 0)
         && (    ((rexxrcHandle != RXSHV_OK) &&
                  (rexxrcHandle != RXSHV_NEWV))
              || ((rexxrcNew != RXSHV_OK) &&
                  (rexxrcNew != RXSHV_NEWV))
              || ((rexxrcOld != RXSHV_OK) &&
                  (rexxrcOld != RXSHV_NEWV))
              || (    (aargc == 2)
                   && (rexxrcDmho != RXSHV_OK)
                   && (rexxrcDmho != RXSHV_NEWV)) ) )
      {
       rc = -6 ;
      }
   }
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
return 0;
} // End of RXMQDMH function
//
//
//
// Set a message property     MQSETMP
//
//   Call:   rc = RXMQsmp(handle, smpo, name, pd, type, value)
//
//           handle : Rexx variable containing MQHMSG, e.g. 'mh1'
//           smpo   : MQSMPO input/output stem, e.g. 'smpo1.'
//           name   : property name, e.g. 'usr.test'
//           pd     : MQPD input/output stem, e.g. 'pd1.'
//           type   : MQTYPE_* value
//           value  : property value
//
FTYPE  RXMQSMP  RXMQPARM
{
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = SMP    ;  // This function trace id
 int                     rexxrcSmpoInit = RXSHV_OK ;
 int                     rexxrcPdInit   = RXSHV_OK ;
 int                     rexxrcSmpo     = RXSHV_OK ;
 int                     rexxrcPd       = RXSHV_OK ;
 RXSTRING                RX_handle        ;  // Rexx variable containing HMSG
 RXSTRING                RX_smpo          ;  // MQSMPO input/output stem
 RXSTRING                RX_name          ;  // Property name
 RXSTRING                RX_pd            ;  // MQPD input/output stem
 RXSTRING                RX_type          ;  // MQ property type
 RXSTRING                RX_value         ;  // MQ property value
 MQHMSG                  hmsg = MQHM_NONE ;  // Message handle
 MQINT64                 hmsg64 = 0       ;  // Intermediate handle value
 MQSMPO                  smpo             ;  // Set message property options
 MQPD                    pd               ;  // Property descriptor
 MQLONG                  type = 0         ;  // Property type
 MQCHARV                 name             ;  // Property name
 MQLONG                  valueLength = 0  ;  // Property value length
 MQPTR                   value = NULL     ;  // Property value pointer
 MQLONG                  int32Value = 0   ;  // Binary MQTYPE_INT32 value
 MQINT64                 int64Value = 0   ;
 MQINT8                  int8Value  = 0   ;
 MQINT16                 int16Value = 0   ;
 char                    valueText[64]    ;  // Null-terminated REXX value
 char                  * endptr = NULL    ;  // Numeric conversion end
 intmax_t                 parsedValue = 0 ;  // Converted REXX integer
 MQLONG                  booleanValue = 0 ;
 MQFLOAT64               float64Value = 0 ;
 MQFLOAT32               float32Value = 0 ;
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters"},
        {  -2, "Null handle name"},
        {  -3, "Zero length handle name"},
        {  -4, "Null SMPO"},
        {  -5, "Zero length SMPO"},
        {  -6, "Null property name"},
        {  -7, "Zero length property name"},
        {  -8, "Null PD"},
        {  -9, "Zero length PD"},
        { -10, "Null property type"},
        { -11, "Zero length property type"},
        { -12, "Null property value"},
        { -13, "Invalid property value for requested MQTYPE"},
        { -14, "Unable to publish output to REXX"},
        { -15, "Invalid property type"},
        { -99, "UNKNOWN FAILURE"} } ;
 rc = set_envir (afuncname, &traceid, &anchor) ;
//
// Check the parms
//
 if ( (rc == 0) && ( aargc != 6 ) )              rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )      rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) )   rc = -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )      rc = -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) )   rc = -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )      rc = -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) )   rc = -7 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[3]) )      rc = -8 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[3]) )   rc = -9 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[4]) )      rc = -10 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[4]) )   rc = -11 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[5]) )      rc = -12 ;
//
// No connection: return a real MQRC so existing REXX rcmap logic works
//
 if ( (rc == 0) && ( anchor->QMh == 0 ) )
   {
    mqrc = MQCC_FAILED ;
    mqac = MQRC_HCONN_ERROR ;
    rc   = mqrc ;
   }
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(&RX_handle, &aargv[0], sizeof(RX_handle)) ;
    memcpy(&RX_smpo,   &aargv[1], sizeof(RX_smpo))  ;
    memcpy(&RX_name,   &aargv[2], sizeof(RX_name))  ;
    memcpy(&RX_pd,     &aargv[3], sizeof(RX_pd))    ;
    memcpy(&RX_type,   &aargv[4], sizeof(RX_type))  ;
    memcpy(&RX_value,  &aargv[5], sizeof(RX_value)) ;
    TRACE(traceid, ("RX_handle = %.*s\n",
          (int)RX_handle.strlength, RX_handle.strptr) ) ;
    TRACE(traceid, ("RX_smpo = %.*s\n",
          (int)RX_smpo.strlength, RX_smpo.strptr) ) ;
    TRACE(traceid, ("RX_name = %.*s\n",
          (int)RX_name.strlength, RX_name.strptr) ) ;
    TRACE(traceid, ("RX_pd = %.*s\n",
          (int)RX_pd.strlength, RX_pd.strptr) ) ;
    TRACE(traceid, ("RX_type = %.*s\n",
          (int)RX_type.strlength, RX_type.strptr) ) ;
    stem_to_int64(traceid, RX_handle, "", &hmsg64) ;
    hmsg = (MQHMSG)hmsg64 ;
    memcpy(&smpo, &smpo_default, sizeof(MQSMPO)) ;
    make_smpo_from_stem(traceid, &smpo, RX_smpo) ;
    rexxrcSmpoInit =
      make_stem_from_smpo(traceid,
                          &smpo,
                          RX_smpo) ;
    if ( (rexxrcSmpoInit != RXSHV_OK) &&
         (rexxrcSmpoInit != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish initial RX_smpo rc = %d\n",
              rexxrcSmpoInit) ) ;
      }
    memcpy(&pd, &pd_default, sizeof(MQPD)) ;
    make_pd_from_stem(traceid, &pd, RX_pd) ;
    rexxrcPdInit =
      make_stem_from_pd(traceid,
                        &pd,
                        RX_pd) ;
    if ( (rexxrcPdInit != RXSHV_OK) &&
         (rexxrcPdInit != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish initial RX_pd rc = %d\n",
              rexxrcPdInit) ) ;
      }
    if ( parm_to_ulong(RX_type, &type) != 0 ) rc = -15 ;
    memset(&name, 0, sizeof(MQCHARV)) ;
    name.VSPtr     = RX_name.strptr ;
    name.VSLength  = RX_name.strlength ;
    name.VSCCSID   = MQCCSI_APPL ;
    name.VSBufSize = RX_name.strlength ;
    //
    // By default, preserve the original REXX byte string.
    // This remains the existing behaviour for MQTYPE_STRING
    // and for all types not yet explicitly converted.
    //
         valueLength = RX_value.strlength ;
         value       = RX_value.strptr ;
 
     if (type == MQTYPE_FLOAT64)
       {
        if (RX_value.strlength == 0)
          {
           rc = -13 ;
          }
        else
        if (RX_value.strlength >= sizeof(valueText))
          {
           rc = -13 ;
          }
        else
          {
           memcpy(valueText,
                  RX_value.strptr,
                  RX_value.strlength) ;
           valueText[RX_value.strlength] = 0 ;
           errno       = 0 ;
           endptr      = NULL ;
           float64Value = strtod(valueText,
                                 &endptr) ;
           if (errno == ERANGE)
             {
              rc = -13 ;
             }
           else
           if (endptr == valueText)
             {
              rc = -13 ;
             }
           else
           if (*endptr != 0)
             {
              rc = -13 ;
             }
           else
             {
              valueLength = sizeof(float64Value) ;
              value       = (MQPTR)&float64Value ;
             }
          }
       }
     //
     // MQTYPE_FLOAT32:
     //
     if (type == MQTYPE_FLOAT32)
       {
        if (RX_value.strlength == 0)
          {
           rc = -13 ;
          }
        else
        if (RX_value.strlength >= sizeof(valueText))
          {
           rc = -13 ;
          }
        else
          {
           memcpy(valueText,
                  RX_value.strptr,
                  RX_value.strlength) ;
           valueText[RX_value.strlength] = 0 ;
           errno  = 0 ;
           endptr = NULL ;
           float32Value = (MQFLOAT32)strtod(valueText,
                                            &endptr) ;
           if (errno == ERANGE)
             {
              rc = -13 ;
             }
           else
           if (endptr == valueText)
             {
              rc = -13 ;
             }
           else
           if (*endptr != 0)
             {
              rc = -13 ;
             }
           else
             {
              valueLength = sizeof(float32Value) ;
              value       = (MQPTR)&float32Value ;
             }
          }
       }
     if (type == MQTYPE_BOOLEAN)
       {
        if (RX_value.strlength == 1)
          {
           if (*RX_value.strptr == '0')
             {
              booleanValue = 0 ;
              valueLength  = sizeof(booleanValue) ;
              value        = (MQPTR)&booleanValue ;
             }
           else
           if (*RX_value.strptr == '1')
             {
              booleanValue = 1 ;
              valueLength  = sizeof(booleanValue) ;
              value        = (MQPTR)&booleanValue ;
             }
           else
             {
              rc = -13 ;
             }
          }
        else
          {
           rc = -13 ;
          }
       }
    //
//
// MQTYPE_INT8:
// Convert the printable REXX value into the
// one-byte signed integer required by MQSETMP.
//
if (type == MQTYPE_INT8)
  {
   if (RX_value.strlength == 0)
     {
      rc = -13 ;
     }
   else
   if (RX_value.strlength >= sizeof(valueText))
     {
      rc = -13 ;
     }
   else
     {
      memcpy(valueText,
             RX_value.strptr,
             RX_value.strlength) ;
      valueText[RX_value.strlength] = 0 ;
      errno       = 0 ;
      endptr      = NULL ;
      parsedValue = strtoimax(valueText,
                              &endptr,
                              10) ;
      if ((errno == ERANGE)       ||
          (endptr == valueText)   ||
          (*endptr != 0)          ||
          (parsedValue < INT8_MIN) ||
          (parsedValue > INT8_MAX))
        {
         rc = -13 ;
        }
      else
        {
         int8Value   = (MQINT8)parsedValue ;
         valueLength = sizeof(int8Value)   ;
         value       = (MQPTR)&int8Value   ;
        }
     }
  }
    //
//
// MQTYPE_INT16:
// Convert the printable REXX value into the
// two-byte signed integer required by MQSETMP.
//
if (type == MQTYPE_INT16)
  {
   if (RX_value.strlength == 0)
     {
      rc = -13 ;
     }
   else
   if (RX_value.strlength >= sizeof(valueText))
     {
      rc = -13 ;
     }
   else
     {
      memcpy(valueText,
             RX_value.strptr,
             RX_value.strlength) ;
      valueText[RX_value.strlength] = 0 ;
      errno       = 0 ;
      endptr      = NULL ;
      parsedValue = strtoimax(valueText,
                              &endptr,
                              10) ;
      if ((errno == ERANGE)         ||
          (endptr == valueText)     ||
          (*endptr != 0)          ||
          (parsedValue < INT16_MIN) ||
          (parsedValue > INT16_MAX))
        {
         rc = -13 ;
        }
      else
        {
         int16Value  = (MQINT16)parsedValue ;
         valueLength = sizeof(int16Value)   ;
         value       = (MQPTR)&int16Value   ;
        }
     }
  }
    // MQTYPE_INT32:
    // Convert the printable REXX value, for example "25",
    // into the four-byte binary MQLONG required by MQSETMP.
    //
         if (type == MQTYPE_INT32)
           {
           if (RX_value.strlength == 0)
             {
              rc = -13 ;
             }
           else
           if (RX_value.strlength >= sizeof(valueText))
             {
              rc = -13 ;
             }
           else
             {
              memcpy(valueText,
                     RX_value.strptr,
                     RX_value.strlength) ;
              valueText[RX_value.strlength] = 0 ;
           errno       = 0 ;
           endptr      = NULL ;
           parsedValue = strtoimax(valueText,
                                   &endptr,
                                   10) ;
           if (errno == ERANGE)
             {
              rc = -13 ;
             }
           else
           if (endptr == valueText)
             {
              rc = -13 ;
             }
           else
           if (*endptr != 0)
             {
              rc = -13 ;
             }
           else
           if (parsedValue < INT32_MIN)
             {
              rc = -13 ;
             }
           else
           if (parsedValue > INT32_MAX)
             {
              rc = -13 ;
             }
           else
             {
              int32Value  = (MQLONG)parsedValue ;
              valueLength = sizeof(int32Value) ;
              value       = (MQPTR)&int32Value ;
             }
          }
       }
     //
     // MQTYPE_INT64:
     // Convert the printable REXX value into the
     // eight-byte binary MQINT64 required by MQSETMP.
     //
     if (type == MQTYPE_INT64)
       {
        if (RX_value.strlength == 0)
          {
           rc = -13 ;
          }
        else
        if (RX_value.strlength >= sizeof(valueText))
          {
           rc = -13 ;
          }
        else
          {
           memcpy(valueText,
                  RX_value.strptr,
                  RX_value.strlength) ;
           valueText[RX_value.strlength] = 0 ;
           errno       = 0 ;
           endptr      = NULL ;
           parsedValue = strtoimax(valueText,
                                   &endptr,
                                   10) ;
           if (errno == ERANGE)
             {
              rc = -13 ;
             }
           else
           if (endptr == valueText)
             {
              rc = -13 ;
             }
           else
           if (*endptr != 0)
             {
              rc = -13 ;
             }
           else
             {
              int64Value  = (MQINT64)parsedValue ;
              valueLength = sizeof(int64Value) ;
              value       = (MQPTR)&int64Value ;
             }
          }
       }
   }
//
//
// RXMQSMP converts printable REXX numeric values into binary C
// values. ValueEncoding must describe the bytes built here,
// rather than an arbitrary encoding left in the input stem.
//
if (    (type == MQTYPE_FLOAT32)
     || (type == MQTYPE_FLOAT64) )
  {
   smpo.ValueEncoding =
       (smpo.ValueEncoding & ~MQENC_FLOAT_MASK)
       | RXMQ_FLOAT_ENCODING ;
  }
if (    (rc == 0)
     && (    ((rexxrcSmpoInit != RXSHV_OK) &&
              (rexxrcSmpoInit != RXSHV_NEWV))
          || ((rexxrcPdInit != RXSHV_OK) &&
              (rexxrcPdInit != RXSHV_NEWV)) ) )
  {
   rc = -14 ;
  }
// Do the MQSETMP
//
 if (rc == 0)
   {
    TRACE(traceid, ("Calling MQSETMP\n") ) ;
    MQSETMP ( anchor->QMh,
              hmsg,
              &smpo,
              &name,
              &pd,
              type,
              valueLength,
              value,
              &mqrc,
              &mqac ) ;
    rc = mqrc ;
   }
//
// If it worked, return updated structures
//
 if (rc == 0)
   {
    rexxrcSmpo =
      make_stem_from_smpo(traceid,
                          &smpo,
                          RX_smpo) ;
    if ( (rexxrcSmpo != RXSHV_OK) &&
         (rexxrcSmpo != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RX_smpo rc = %d\n",
              rexxrcSmpo) ) ;
      }
    rexxrcPd =
      make_stem_from_pd  (traceid,
                          &pd,
                          RX_pd)   ;
    if ( (rexxrcPd != RXSHV_OK) &&
         (rexxrcPd != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RX_pd rc = %d\n",
              rexxrcPd) ) ;
      }
    if (    (rc == 0)
         && (    ((rexxrcSmpo != RXSHV_OK) &&
                  (rexxrcSmpo != RXSHV_NEWV))
              || ((rexxrcPd != RXSHV_OK) &&
                  (rexxrcPd != RXSHV_NEWV)) ) )
      {
       rc = -14 ;
      }
   }
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
return 0;
} // End of RXMQSMP function
//
//
// Inquire a message property     MQINQMP
//
//   Call:   rc = RXMQimp(handle,
//                         impo,
//                         name,
//                         pd,
//                         type,
//                         value,
//                         nameout)
//
//           handle  : Rexx variable containing MQHMSG,
//                     e.g. 'mh1'
//
//           impo    : MQIMPO input/output stem,
//                     e.g. 'impo1.'
//
//           name    : property name or property pattern,
//                     e.g. 'usr.test' or 'usr.%'
//
//           pd      : MQPD output stem,
//                     e.g. 'pd1.'
//
//           type    : Rexx input/output variable containing
//                     or receiving an MQTYPE_* value,
//                     e.g. 'ptype'
//
//           value   : Rexx input/output stem:
//                       value.0 = input buffer length / returned data length
//                       value.1 = returned property value
//
//           nameout : Rexx variable receiving the returned
//                     property name, e.g. 'pnameout'
//
FTYPE  RXMQIMP  RXMQPARM
{
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = IMP    ;  // This function trace id
 int                     rexxrcType    = RXSHV_OK ;
 int                     rexxrcValue0  = RXSHV_OK ;
 int                     rexxrcValue1  = RXSHV_OK ;
 int                     rexxrcNameout = RXSHV_OK ;
 int                     rexxrcPdInit  = RXSHV_OK ;
 int                     rexxrcImpo    = RXSHV_OK ;
 int                     rexxrcPd      = RXSHV_OK ;
 RXSTRING                RX_handle        ;  // Rexx variable containing HMSG
 RXSTRING                RX_impo          ;  // MQIMPO input/output stem
 RXSTRING                RX_name          ;  // Property name
 RXSTRING                RX_pd            ;  // MQPD output stem
 RXSTRING                RX_type          ;  // Rexx variable containing/receiving type
 RXSTRING                RX_value         ;  // Rexx value stem: .0 length, .1 data
 RXSTRING                RX_nameout       ;  // Rexx variable receiving returned property name
 MQHMSG                  hmsg = MQHM_NONE ;  // Message handle
 MQINT64                 hmsg64 = 0       ;  // Intermediate handle value
 MQIMPO                  impo             ;  // Inquire message property options
 MQPD                    pd               ;  // Property descriptor
 MQLONG                  type = MQTYPE_AS_SET ;
 MQCHARV                 name             ;  // Property name
 MQCHAR                * returnedNameBuffer = NULL ; // Returned property name buffer
 MQLONG                  returnedNameLength = MQ_MAX_PROPERTY_NAME_LENGTH ;
 MQLONG                  returnedNameCopyLength = 0 ;
 MQLONG                  valueLength = 0  ;  // Input buffer length from value.0
 MQLONG                  dataLength = 0   ;  // Actual property length returned by MQ
 MQLONG                  returnedValueLength = 0 ; // Bytes copied to value.1
 MQBYTE                * value = NULL     ;
 char                    valueText[100]        ;
 MQINT8                  int8Value  = 0   ;
 MQINT16                 int16Value = 0   ;
 MQLONG                  int32Value = 0   ;
 MQINT64                 int64Value = 0   ;
 MQLONG                  booleanValue = 0      ;
 MQFLOAT64               float64Value = 0      ;
 MQFLOAT32               float32Value = 0 ;
 MQLONG                  valueTextLength = 0   ;
 int                     integerEncodingMatches = 0 ;
 int                     floatEncodingMatches   = 0 ;
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters"},
        {  -2, "Null handle name"},
        {  -3, "Zero length handle name"},
        {  -4, "Null IMPO"},
        {  -5, "Zero length IMPO"},
        {  -6, "Null property name"},
        {  -7, "Zero length property name"},
        {  -8, "Null PD"},
        {  -9, "Zero length PD"},
        { -10, "Null type variable"},
        { -11, "Zero length type variable"},
        { -12, "Null value stem"},
        { -13, "Zero length value stem"},
        { -14, "Null returned property name variable"},
        { -15, "Zero length returned property name variable"},
        { -16, "Negative value buffer length in value.0"},
        { -17, "Value buffer allocation failed"},
        { -18, "Returned property name buffer allocation failed"},
        { -19, "Unable to publish output to REXX"},
        { -99, "UNKNOWN FAILURE"}} ;
rc = set_envir (afuncname, &traceid, &anchor) ;
//
// Check the parms
//
if ( (rc == 0) && (aargc != 7) ) rc = -1  ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )   rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc = -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )   rc = -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc = -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )   rc = -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) ) rc = -7 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[3]) )    rc = -8 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[3]) ) rc = -9 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[4]) )    rc = -10 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[4]) ) rc = -11 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[5]) )    rc = -12 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[5]) ) rc = -13 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[6]) )    rc = -14 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[6]) ) rc = -15 ;
//
// No connection: return a real MQRC so existing REXX rcmap logic works
//
 if ( (rc == 0) && ( anchor->QMh == 0 ) )
   {
    mqrc = MQCC_FAILED ;
    mqac = MQRC_HCONN_ERROR ;
    rc   = mqrc ;
   }
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(&RX_handle,  &aargv[0], sizeof(RX_handle))  ;
    memcpy(&RX_impo,    &aargv[1], sizeof(RX_impo))    ;
    memcpy(&RX_name,    &aargv[2], sizeof(RX_name))    ;
    memcpy(&RX_pd,      &aargv[3], sizeof(RX_pd))      ;
    memcpy(&RX_type,    &aargv[4], sizeof(RX_type))    ;
    memcpy(&RX_value,   &aargv[5], sizeof(RX_value))   ;
    memcpy(&RX_nameout, &aargv[6], sizeof(RX_nameout)) ;
stem_to_long(traceid, RX_value, "0", &valueLength) ;
make_impo_from_stem(traceid,
                    &impo,
                    RX_impo) ;
if ((impo.Options & MQIMPO_QUERY_LENGTH) != 0)
  {
   valueLength = 0 ;
  }
else
if (valueLength < 0)
  {
   rc = -16 ;
  }
type = MQTYPE_AS_SET ;
if ((impo.Options & MQIMPO_CONVERT_TYPE) != 0)
  {
   stem_to_long(traceid,
                RX_type,
                "",
                &type) ;
  }
 rexxrcType =
   stem_from_long(traceid,
                  NULL,
                  RX_type,
                  "",
                  0) ;
 rexxrcValue0 =
   stem_from_long(traceid,
                  NULL,
                  RX_value,
                  "0",
                  0) ;
 rexxrcValue1 =
   stem_from_bytes(traceid,
                   NULL,
                   RX_value,
                   "1",
                   (MQBYTE *)"",
                   0) ;
 rexxrcNameout =
   stem_from_string(traceid,
                    NULL,
                    RX_nameout,
                    "",
                    "",
                    0) ;
 if ( (rexxrcType != RXSHV_OK) && (rexxrcType != RXSHV_NEWV) )
   TRACE(traceid, ("RexxVariablePool failed to initialize RX_type rc = %d\n",
                   rexxrcType) ) ;
 if ( (rexxrcValue0 != RXSHV_OK) && (rexxrcValue0 != RXSHV_NEWV) )
   TRACE(traceid, ("RexxVariablePool failed to initialize RX_value.0 rc = %d\n",
                   rexxrcValue0) ) ;
 if ( (rexxrcValue1 != RXSHV_OK) && (rexxrcValue1 != RXSHV_NEWV) )
   TRACE(traceid, ("RexxVariablePool failed to initialize RX_value.1 rc = %d\n",
                   rexxrcValue1) ) ;
 if ( (rexxrcNameout != RXSHV_OK) && (rexxrcNameout != RXSHV_NEWV) )
   TRACE(traceid, ("RexxVariablePool failed to initialize RX_nameout rc = %d\n",
                   rexxrcNameout) ) ;
 if (   ((rexxrcType != RXSHV_OK) && (rexxrcType != RXSHV_NEWV))
     || ((rexxrcValue0 != RXSHV_OK) && (rexxrcValue0 != RXSHV_NEWV))
     || ((rexxrcValue1 != RXSHV_OK) && (rexxrcValue1 != RXSHV_NEWV))
     || ((rexxrcNameout != RXSHV_OK) && (rexxrcNameout != RXSHV_NEWV)) )
   if (rc == 0) rc = -19 ;
 rexxrcPdInit =
   make_stem_from_pd(traceid,
                     &pd_default,
                     RX_pd) ;
 if ( (rexxrcPdInit != RXSHV_OK) &&
      (rexxrcPdInit != RXSHV_NEWV) )
   {
    TRACE(traceid,
          ("RexxVariablePool failed to initialize RX_pd rc = %d\n",
           rexxrcPdInit) ) ;
    if (rc == 0) rc = -19 ;
   }
stem_to_int64(traceid,
              RX_handle,
              "",
              &hmsg64) ;
hmsg = (MQHMSG)hmsg64 ;
    if (rc == 0)
      {
       returnedNameBuffer = malloc(returnedNameLength + 1) ;
       if (returnedNameBuffer == NULL)
         rc = -18 ;
       else
         {
          memset(returnedNameBuffer, 0, returnedNameLength + 1) ;
          impo.ReturnedName.VSPtr     = returnedNameBuffer ;
          impo.ReturnedName.VSLength  = 0 ;
          impo.ReturnedName.VSCCSID   = MQCCSI_APPL ;
          impo.ReturnedName.VSBufSize = returnedNameLength ;
         }
      }
    memcpy(&pd, &pd_default, sizeof(MQPD)) ;
    memset(&name, 0, sizeof(MQCHARV)) ;
    if (RX_name.strlength > 0)
      {
       name.VSPtr     = RX_name.strptr ;
       name.VSLength  = RX_name.strlength ;
       name.VSCCSID   = MQCCSI_APPL ;
       name.VSBufSize = RX_name.strlength ;
      }
    else
      {
       name.VSPtr     = NULL ;
       name.VSLength  = 0 ;
       name.VSCCSID   = MQCCSI_APPL ;
       name.VSBufSize = 0 ;
      }
if ((rc == 0) && (valueLength > 0))
  {
   value = malloc(valueLength) ;
   if (value == NULL)
     {
      rc = -17 ;
     }
   else
     {
      memset(value, 0, valueLength) ;
     }
  }
   }
//
// Do the MQINQMP
//
 if (rc == 0)
   {
    TRACE(traceid, ("Calling MQINQMP\n") ) ;
    MQINQMP ( anchor->QMh,
              hmsg,
              &impo,
              &name,
              &pd,
              &type,
              valueLength,
              value,
              &dataLength,
              &mqrc,
              &mqac ) ;
    rc = mqrc ;
//
// Check whether the returned bytes use the same numeric
// representation as the C variables used by this wrapper.
//
// Check integer and floating-point components separately.
// INT8 does not require an encoding check because it is one byte.
//
integerEncodingMatches =
  ((impo.ReturnedEncoding & MQENC_INTEGER_MASK) ==
   (RXMQ_NUMERIC_ENCODING & MQENC_INTEGER_MASK)) ;
floatEncodingMatches =
  ((impo.ReturnedEncoding & MQENC_FLOAT_MASK) ==
   (RXMQ_NUMERIC_ENCODING & MQENC_FLOAT_MASK)) ;
    rexxrcImpo =
      make_stem_from_impo(traceid, &impo, RX_impo) ;
    if ( (rexxrcImpo != RXSHV_OK) &&
         (rexxrcImpo != RXSHV_NEWV) )
      TRACE(traceid,
            ("RexxVariablePool failed to publish RX_impo rc = %d\n",
             rexxrcImpo) ) ;
    rexxrcPd =
      make_stem_from_pd  (traceid, &pd,   RX_pd)   ;
    if ( (rexxrcPd != RXSHV_OK) &&
         (rexxrcPd != RXSHV_NEWV) )
      TRACE(traceid,
            ("RexxVariablePool failed to publish RX_pd rc = %d\n",
             rexxrcPd) ) ;
    rexxrcType = stem_from_long(traceid, NULL, RX_type,  "", type) ;
    rexxrcValue0 = stem_from_long(traceid, NULL, RX_value, "0", dataLength) ;
returnedNameCopyLength = impo.ReturnedName.VSLength ;
if (returnedNameCopyLength < 0)
  {
   returnedNameCopyLength = 0 ;
  }
if (returnedNameCopyLength > impo.ReturnedName.VSBufSize)
  {
   returnedNameCopyLength = impo.ReturnedName.VSBufSize ;
  }
if ((impo.Options & MQIMPO_QUERY_LENGTH) != 0)
  {
    rexxrcNameout = stem_from_string(traceid,
                    NULL,
                    RX_nameout,
                    "",
                    "",
                    0) ;
  }
else
  {
 rexxrcNameout = stem_from_string(traceid,
                 NULL,
                 RX_nameout,
                 "",
                 (MQCHAR *)impo.ReturnedName.VSPtr,
                 returnedNameCopyLength) ;
  }
     returnedValueLength = dataLength ;
     if (returnedValueLength > valueLength)
       returnedValueLength = valueLength ;
     if (returnedValueLength < 0)
       returnedValueLength = 0 ;
     memset(valueText, 0, sizeof(valueText)) ;
if ((returnedValueLength == 0) || (value == NULL))
  {
    rexxrcValue1 = stem_from_bytes(traceid,
                   NULL,
                   RX_value,
                   "1",
                   (MQBYTE *)"",
                   0) ;
  }
else
if (type == MQTYPE_INT8)
  {
   if (returnedValueLength == sizeof(int8Value))
     {
      memcpy(&int8Value,
             value,
             sizeof(int8Value)) ;
      sprintf(valueText,
              "%" PRId8,
              (int8_t)int8Value) ;
      valueTextLength = strlen(valueText) ;
      rexxrcValue1 = stem_from_string(traceid,
                       NULL,
                       RX_value,
                       "1",
                       valueText,
                       valueTextLength) ;
     }
   else
     {
      rexxrcValue1 = stem_from_bytes(traceid,
                      NULL,
                      RX_value,
                      "1",
                      value,
                      returnedValueLength) ;
     }
  }
else
if (type == MQTYPE_INT16)
  {
  if (    (returnedValueLength == sizeof(int16Value))
       && integerEncodingMatches )
     {
      memcpy(&int16Value,
             value,
             sizeof(int16Value)) ;
      sprintf(valueText,
              "%" PRId16,
              (int16_t)int16Value) ;
      valueTextLength = strlen(valueText) ;
      rexxrcValue1 = stem_from_string(traceid,
                       NULL,
                       RX_value,
                       "1",
                       valueText,
                       valueTextLength) ;
     }
   else
     {
      rexxrcValue1 = stem_from_bytes(traceid,
                      NULL,
                      RX_value,
                      "1",
                      value,
                      returnedValueLength) ;
     }
  }
else
     if (type == MQTYPE_INT32)
       {
        if (    (returnedValueLength == sizeof(int32Value))
             && integerEncodingMatches )
          {
           memcpy(&int32Value,
                  value,
                  sizeof(int32Value)) ;
           sprintf(valueText,
                   "%"PRId32,
                   (int32_t)int32Value) ;
           valueTextLength = strlen(valueText) ;
           rexxrcValue1 = stem_from_string(traceid,
                            NULL,
                            RX_value,
                            "1",
                            valueText,
                            valueTextLength) ;
          }
        else
          {
           rexxrcValue1 = stem_from_bytes(traceid,
                           NULL,
                           RX_value,
                           "1",
                           value,
                           returnedValueLength) ;
          }
       }
     else
     if (type == MQTYPE_INT64)
       {
        if (    (returnedValueLength == sizeof(int64Value))
             && integerEncodingMatches )
          {
           memcpy(&int64Value,
                  value,
                  sizeof(int64Value)) ;
           sprintf(valueText,
                   "%"PRId64,
                   (int64_t)int64Value) ;
           valueTextLength = strlen(valueText) ;
           rexxrcValue1 = stem_from_string(traceid,
                            NULL,
                            RX_value,
                            "1",
                            valueText,
                            valueTextLength) ;
          }
        else
          {
           rexxrcValue1 = stem_from_bytes(traceid,
                           NULL,
                           RX_value,
                           "1",
                           value,
                           returnedValueLength) ;
          }
       }
     else
     if (type == MQTYPE_BOOLEAN)
       {
        if (    (returnedValueLength == sizeof(booleanValue))
             && integerEncodingMatches )
          {
           memcpy(&booleanValue,
                  value,
                  sizeof(booleanValue)) ;
           if (booleanValue == 0)
             sprintf(valueText, "0") ;
           else
             sprintf(valueText, "1") ;
           valueTextLength = strlen(valueText) ;
           rexxrcValue1 = stem_from_string(traceid,
                            NULL,
                            RX_value,
                            "1",
                            valueText,
                            valueTextLength) ;
          }
        else
          {
           rexxrcValue1 = stem_from_bytes(traceid,
                           NULL,
                           RX_value,
                           "1",
                           value,
                           returnedValueLength) ;
          }
       }
     else
     if (type == MQTYPE_FLOAT32)
       {
        if (    (returnedValueLength == sizeof(float32Value))
             && floatEncodingMatches )
          {
           memcpy(&float32Value,
                  value,
                  sizeof(float32Value)) ;
           sprintf(valueText,
                   "%.7g",
                   (double)float32Value) ;
           valueTextLength = strlen(valueText) ;
           rexxrcValue1 = stem_from_string(traceid,
                            NULL,
                            RX_value,
                            "1",
                            valueText,
                            valueTextLength) ;
          }
        else
          {
           rexxrcValue1 = stem_from_bytes(traceid,
                           NULL,
                           RX_value,
                           "1",
                           value,
                           returnedValueLength) ;
          }
       }
     else
     if (type == MQTYPE_FLOAT64)
       {
        if (    (returnedValueLength == sizeof(float64Value))
             && floatEncodingMatches )
          {
           memcpy(&float64Value,
                  value,
                  sizeof(float64Value)) ;
           sprintf(valueText,
                   "%.15g",
                   float64Value) ;
           valueTextLength = strlen(valueText) ;
           rexxrcValue1 = stem_from_string(traceid,
                            NULL,
                            RX_value,
                            "1",
                            valueText,
                            valueTextLength) ;
          }
        else
          {
           rexxrcValue1 = stem_from_bytes(traceid,
                           NULL,
                           RX_value,
                           "1",
                           value,
                           returnedValueLength) ;
          }
       }
     else
       {
        rexxrcValue1 = stem_from_bytes(traceid,
                        NULL,
                        RX_value,
                        "1",
                        value,
                        returnedValueLength) ;
       }

     if ( (rexxrcType != RXSHV_OK) && (rexxrcType != RXSHV_NEWV) )
       TRACE(traceid, ("RexxVariablePool failed to publish RX_type rc = %d\n",
                       rexxrcType) ) ;
     if ( (rexxrcValue0 != RXSHV_OK) && (rexxrcValue0 != RXSHV_NEWV) )
       TRACE(traceid, ("RexxVariablePool failed to publish RX_value.0 rc = %d\n",
                       rexxrcValue0) ) ;
     if ( (rexxrcValue1 != RXSHV_OK) && (rexxrcValue1 != RXSHV_NEWV) )
       TRACE(traceid, ("RexxVariablePool failed to publish RX_value.1 rc = %d\n",
                       rexxrcValue1) ) ;
     if ( (rexxrcNameout != RXSHV_OK) && (rexxrcNameout != RXSHV_NEWV) )
       TRACE(traceid, ("RexxVariablePool failed to publish RX_nameout rc = %d\n",
                       rexxrcNameout) ) ;
     if (   ((rexxrcImpo != RXSHV_OK) && (rexxrcImpo != RXSHV_NEWV))
         || ((rexxrcPd != RXSHV_OK) && (rexxrcPd != RXSHV_NEWV))
         || ((rexxrcType != RXSHV_OK) && (rexxrcType != RXSHV_NEWV))
         || ((rexxrcValue0 != RXSHV_OK) && (rexxrcValue0 != RXSHV_NEWV))
         || ((rexxrcValue1 != RXSHV_OK) && (rexxrcValue1 != RXSHV_NEWV))
         || ((rexxrcNameout != RXSHV_OK) && (rexxrcNameout != RXSHV_NEWV)) )
       if (rc == 0) rc = -19 ;
   }
//
// Free output buffers
//
 if (returnedNameBuffer)
   {
    free(returnedNameBuffer) ;
    returnedNameBuffer = NULL ;
   }
 if (value)
   {
    free(value) ;
    value = NULL ;
   }
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
return 0;
} // End of RXMQIMP function
//
//
// Delete a message property     MQDLTMP
//
//   Call:   rc = RXMQdmp(handle, dmpo, name)
//
//           handle : Rexx variable containing MQHMSG, e.g. 'mh1'
//           dmpo   : MQDMPO input/output stem, e.g. 'dmpo1.'
//           name   : property name, e.g. 'usr.test'
//
FTYPE  RXMQDMP  RXMQPARM
{
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = DMP    ;  // This function trace id
 int                     rexxrcDmpoInit = RXSHV_OK ;
 int                     rexxrcDmpo     = RXSHV_OK ;
 RXSTRING                RX_handle        ;  // Rexx variable containing HMSG
 RXSTRING                RX_dmpo          ;  // MQDMPO input/output stem
 RXSTRING                RX_name          ;  // Property name
 MQHMSG                  hmsg = MQHM_NONE ;  // Message handle
 MQINT64                 hmsg64 = 0       ;  // Intermediate handle value
 MQDMPO                  dmpo             ;  // Delete message property options
 MQCHARV                 name             ;  // Property name
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters"},
        {  -2, "Null handle name"},
        {  -3, "Zero length handle name"},
        {  -4, "Null DMPO"},
        {  -5, "Zero length DMPO"},
        {  -6, "Null property name"},
        {  -7, "Zero length property name"},
        {  -8, "Unable to publish output to REXX"},
        { -99, "UNKNOWN FAILURE"}} ;
 rc = set_envir (afuncname, &traceid, &anchor) ;
//
// Check the parms
//
 if ( (rc == 0) && ( aargc != 3 ) )              rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )      rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) )   rc = -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )      rc = -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) )   rc = -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )      rc = -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) )   rc = -7 ;
//
// No connection: return a real MQRC so existing REXX rcmap logic works
//
 if ( (rc == 0) && ( anchor->QMh == 0 ) )
   {
    mqrc = MQCC_FAILED ;
    mqac = MQRC_HCONN_ERROR ;
    rc   = mqrc ;
   }
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(&RX_handle, &aargv[0], sizeof(RX_handle)) ;
    memcpy(&RX_dmpo,   &aargv[1], sizeof(RX_dmpo))   ;
    memcpy(&RX_name,   &aargv[2], sizeof(RX_name))   ;
    TRACE(traceid, ("RX_handle = %.*s\n",
          (int)RX_handle.strlength, RX_handle.strptr) ) ;
    TRACE(traceid, ("RX_dmpo = %.*s\n",
          (int)RX_dmpo.strlength, RX_dmpo.strptr) ) ;
    TRACE(traceid, ("RX_name = %.*s\n",
          (int)RX_name.strlength, RX_name.strptr) ) ;
    stem_to_int64(traceid, RX_handle, "", &hmsg64) ;
    hmsg = (MQHMSG)hmsg64 ;
    memcpy(&dmpo, &dmpo_default, sizeof(MQDMPO)) ;
    make_dmpo_from_stem(traceid, &dmpo, RX_dmpo) ;
    rexxrcDmpoInit =
      make_stem_from_dmpo(traceid,
                          &dmpo,
                          RX_dmpo) ;
    if ( (rexxrcDmpoInit != RXSHV_OK) &&
         (rexxrcDmpoInit != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish initial RX_dmpo rc = %d\n",
              rexxrcDmpoInit) ) ;
       if (rc == 0)
         rc = -8 ;
      }
    memset(&name, 0, sizeof(MQCHARV)) ;
    name.VSPtr     = RX_name.strptr ;
    name.VSLength  = RX_name.strlength ;
    name.VSCCSID   = MQCCSI_APPL ;
    name.VSBufSize = RX_name.strlength ;
   }
//
// Do the MQDLTMP
//
 if (rc == 0)
   {
    TRACE(traceid, ("Calling MQDLTMP\n") ) ;
    MQDLTMP ( anchor->QMh,
              hmsg,
              &dmpo,
              &name,
              &mqrc,
              &mqac ) ;
    rc = mqrc ;
   }
//
// Always return a clean DMPO stem once it is known
//
 if (rc == 0)
   {
    rexxrcDmpo =
      make_stem_from_dmpo(traceid,
                          &dmpo,
                          RX_dmpo) ;
    if ( (rexxrcDmpo != RXSHV_OK) &&
         (rexxrcDmpo != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RX_dmpo rc = %d\n",
              rexxrcDmpo) ) ;
       if (rc == 0)
         rc = -8 ;
      }
   }
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
return 0;
} // End of RXMQDMP function
//
//
//
//
//
// Buffer to message handle     MQBUFMH
//
//   Call:   rc = RXMQbmh(handle, bmho, md, buffer)
//
//           handle : Rexx variable containing MQHMSG, e.g. 'mh1'
//           bmho   : MQBMHO input/output stem, e.g. 'bmho1.'
//           md     : MQMD input/output stem, e.g. 'md1.'
//           buffer : Rexx input/output stem:
//                      buffer.0 = input buffer length / returned DataLength
//                      buffer.1 = input/output message buffer
//
FTYPE  RXMQBMH  RXMQPARM
{
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = BMH    ;  // This function trace id
 int                     rexxrcBmho    = RXSHV_OK ;
 int                     rexxrcMd      = RXSHV_OK ;
 int                     rexxrcBuffer0 = RXSHV_OK ;
 int                     rexxrcBuffer1 = RXSHV_OK ;
 RXSTRING                RX_handle        ;  // Rexx variable containing HMSG
 RXSTRING                RX_bmho          ;  // MQBMHO input/output stem
 RXSTRING                RX_md            ;  // MQMD input/output stem
 RXSTRING                RX_buffer         ;  // Rexx I/O stem: .0 length, .1 buffer
 MQLONG                  bufferLength = 0 ;
 MQLONG                  inputDataLength = 0 ;
 MQLONG                  returnedBufferLength = 0 ;
 MQBYTE                  * buffer = NULL ;
 RXMQ_EXACT_FETCH_RESULT   fetchResult = RXMQ_EXACT_FETCH_INVALID ;
 int                     mqbufmhCalled = 0 ;
 MQHMSG                  hmsg = MQHM_NONE ;  // Message handle
 MQINT64                 hmsg64 = 0       ;  // Intermediate handle value
 MQBMHO                  bmho             ;  // Buffer to message handle options
 MQMD2                   md               ;  // Message descriptor
 MQLONG                  dataLength = 0   ;  // Data length returned by MQBUFMH
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters"},
        {  -2, "Null handle name"},
        {  -3, "Zero length handle name"},
        {  -4, "Null BMHO"},
        {  -5, "Zero length BMHO"},
        {  -6, "Null MD"},
        {  -7, "Zero length MD"},
        {  -8, "Null input buffer"},
        {  -9, "Zero length input buffer"},
        { -10, "Zero or negative buffer length in buffer.0" },
        { -11, "Buffer allocation failed" },
        { -12, "buffer.1 length does not match buffer.0" },
        { -13, "Unable to publish output to REXX"},
        { -99, "UNKNOWN FAILURE"}} ;
 rc = set_envir (afuncname, &traceid, &anchor) ;
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 4) ) rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )  rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) )rc = -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )  rc = -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) )rc = -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )  rc = -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) )rc = -7 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[3]) )  rc = -8 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[3]) )rc = -9 ;
//
// No connection: return a real MQRC so existing REXX rcmap logic works
//
 if ( (rc == 0) && ( anchor->QMh == 0 ) )
   {
    mqrc = MQCC_FAILED ;
    mqac = MQRC_HCONN_ERROR ;
    rc   = mqrc ;
   }
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(&RX_handle, &aargv[0], sizeof(RX_handle)) ;
    memcpy(&RX_bmho,   &aargv[1], sizeof(RX_bmho))  ;
    memcpy(&RX_md,     &aargv[2], sizeof(RX_md))    ;
    memcpy(&RX_buffer, &aargv[3], sizeof(RX_buffer)) ;
    stem_to_long(traceid,
                 RX_buffer,
                 "0",
                 &bufferLength) ;
    if (bufferLength <= 0)
      {
       rc = -10 ;
      }
    if ((rc == 0) && (bufferLength > 0))
      {
       fetchResult = fetch_exact_rexx_bytes(traceid, RX_buffer, "1",
                                            bufferLength, &buffer) ;
       if (fetchResult == RXMQ_EXACT_FETCH_NOMEM)
         {
          rc = -11 ;
         }
       else if (fetchResult != RXMQ_EXACT_FETCH_SUCCESS)
         {
          rc = -12 ;
         }
       else
         {
          inputDataLength = bufferLength ;
      if (inputDataLength != bufferLength)
        {
         rc = -12 ;
        }
     }
  }
    TRACE(traceid, ("RX_handle = %.*s\n",
          (int)RX_handle.strlength, RX_handle.strptr) ) ;
    TRACE(traceid, ("RX_bmho = %.*s\n",
          (int)RX_bmho.strlength, RX_bmho.strptr) ) ;
    TRACE(traceid, ("RX_md = %.*s\n",
          (int)RX_md.strlength, RX_md.strptr) ) ;
    TRACE(traceid,
          ("RX_buffer.0 = %"PRId32"\n",
           bufferLength)) ;
    stem_to_int64(traceid, RX_handle, "", &hmsg64) ;
    hmsg = (MQHMSG)hmsg64 ;
    memcpy(&bmho, &bmho_default, sizeof(MQBMHO)) ;
    make_bmho_from_stem(traceid, &bmho, RX_bmho) ;
    memcpy(&md, &md_default, sizeof(MQMD2)) ;
    make_md_from_stem(traceid, &md, RX_md) ;
   }
//
// Do the MQBUFMH
//
 if (rc == 0)
   {
    TRACE(traceid, ("Calling MQBUFMH\n") ) ;
    MQBUFMH(anchor->QMh,
            hmsg,
            &bmho,
            &md,
            bufferLength,
            buffer,
            &dataLength,
            &mqrc,
            &mqac) ;
    mqbufmhCalled = 1 ;
    rc = mqrc ;
 if (mqbufmhCalled)
   {
    rexxrcBmho =
      make_stem_from_bmho(traceid,
                          &bmho,
                          RX_bmho) ;
    if ( (rexxrcBmho != RXSHV_OK) &&
         (rexxrcBmho != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RX_bmho rc = %d\n",
              rexxrcBmho) ) ;
      }
    rexxrcMd =
      make_stem_from_md(traceid,
                        &md,
                        RX_md) ;
    if ( (rexxrcMd != RXSHV_OK) &&
         (rexxrcMd != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RX_md rc = %d\n",
              rexxrcMd) ) ;
      }
    rexxrcBuffer0 =
      stem_from_long(traceid,
                     NULL,
                     RX_buffer,
                     "0",
                     dataLength) ;
    if ( (rexxrcBuffer0 != RXSHV_OK) &&
         (rexxrcBuffer0 != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RX_buffer.0 rc = %d\n",
              rexxrcBuffer0) ) ;
      }
    returnedBufferLength = dataLength ;
    if (returnedBufferLength < 0)
      {
       returnedBufferLength = 0 ;
     }
   if (returnedBufferLength > bufferLength)
     {
      returnedBufferLength = bufferLength ;
     }
   if (    (buffer != NULL)
        && (returnedBufferLength > 0) )
     {
      rexxrcBuffer1 =
        stem_from_bytes(traceid,
                        NULL,
                        RX_buffer,
                        "1",
                        buffer,
                        returnedBufferLength) ;
     }
   else
     {
      rexxrcBuffer1 =
        stem_from_bytes(traceid,
                        NULL,
                        RX_buffer,
                        "1",
                        (MQBYTE *)"",
                        0) ;
     }
    if ( (rexxrcBuffer1 != RXSHV_OK) &&
         (rexxrcBuffer1 != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RX_buffer.1 rc = %d\n",
              rexxrcBuffer1) ) ;
      }
    if (    (rc == 0)
         && (    (    (rexxrcBmho != RXSHV_OK)
                   && (rexxrcBmho != RXSHV_NEWV) )
              || (    (rexxrcMd != RXSHV_OK)
                   && (rexxrcMd != RXSHV_NEWV) )
              || (    (rexxrcBuffer0 != RXSHV_OK)
                   && (rexxrcBuffer0 != RXSHV_NEWV) )
              || (    (rexxrcBuffer1 != RXSHV_OK)
                   && (rexxrcBuffer1 != RXSHV_NEWV) ) ) )
      {
       rc = -13 ;
      }
  }
 TRACE(traceid,
       ("MQBUFMH returned mqrc=%"PRId32
        " mqac=%"PRId32
        " dataLength=%"PRId32
        " bufferLength=%"PRId32"\n",
        mqrc,
        mqac,
        dataLength,
        bufferLength)) ;
   }
//
//
if (buffer != NULL)
  {
   free(buffer) ;
   buffer = NULL ;
  }
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
return 0;
} // End of RXMQBMH function
//
// Message handle to buffer     MQMHBUF
//
//   Call:   rc = RXMQmbf(handle, mhbo, name, md, buffer, buflen, datalen)
//
//           handle  : Rexx variable containing MQHMSG, e.g. 'mh1'
//           mhbo    : MQMHBO input/output stem, e.g. 'mhbo1.'
//           name    : property name filter, e.g. 'usr.%'
//           md      : MQMD input/output stem, e.g. 'md1.'
//           buffer  : Rexx variable receiving message buffer
//           buflen  : buffer length
//           datalen : Rexx variable receiving DataLength
//
FTYPE  RXMQMBF  RXMQPARM
{
 RXMQCB                * anchor = 0       ;
 MQLONG                  rc = 0           ;
 MQLONG                  mqrc = 0         ;
 MQLONG                  mqac = 0         ;
 MQULONG                 traceid = MBF    ;
 int                     rexxrcBuffer  = RXSHV_OK ;
 int                     rexxrcDatalen = RXSHV_OK ;
 int                     rexxrcMhbo    = RXSHV_OK ;
 int                     rexxrcMd      = RXSHV_OK ;
 RXSTRING                RX_handle        ;
 RXSTRING                RX_mhbo          ;
 RXSTRING                RX_name          ;
 RXSTRING                RX_md            ;
 RXSTRING                RX_buffer        ;
 RXSTRING                RX_buflen        ;
 RXSTRING                RX_datalen       ;
 MQHMSG                  hmsg = MQHM_NONE ;
 MQINT64                 hmsg64 = 0       ;
 MQMHBO                  mhbo             ;
 MQMD2                   md               ;
 MQCHARV                 name             ;
 MQLONG                  bufferLength = 0 ;
 MQLONG                  dataLength = 0 ;
 MQLONG                  returnedBufferLength = 0 ;
 MQBYTE                  * buffer = NULL ;
 int                     mqmhbufCalled = 0 ;
 char                    bufferLengthText[32] ;
 char                    * bufferLengthEnd = NULL ;
 intmax_t                parsedBufferLength = 0 ;
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters"},
        {  -2, "Null handle name"},
        {  -3, "Zero length handle name"},
        {  -4, "Null MHBO"},
        {  -5, "Zero length MHBO"},
        {  -6, "Null name"},
        {  -7, "Zero length name"},
        {  -8, "Null MD"},
        {  -9, "Zero length MD"},
        { -10, "Null output buffer variable"},
        { -11, "Zero length output buffer variable"},
        { -12, "Null buffer length"},
        { -13, "Zero length buffer length"},
        { -14, "Null data length variable"},
        { -15, "Zero length data length variable"},
        { -16, "Invalid buffer length"},
        { -17, "Unable to allocate output buffer"},
        { -18, "Unable to publish output to REXX"},
        { -99, "UNKNOWN FAILURE"}} ;
 rc = set_envir (afuncname, &traceid, &anchor) ;
 if ( (rc == 0) && (aargc != 7) )             rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )  rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) )rc = -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )  rc = -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) )rc = -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )  rc = -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) )rc = -7 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[3]) )  rc = -8 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[3]) )rc = -9 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[4]) )  rc = -10 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[4]) )rc = -11 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[5]) )  rc = -12 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[5]) )rc = -13 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[6]) )  rc = -14 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[6]) )rc = -15 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )
   {
    mqrc = MQCC_FAILED ;
    mqac = MQRC_HCONN_ERROR ;
    rc   = mqrc ;
   }
 if (rc == 0)
   {
    memcpy(&RX_handle,  &aargv[0], sizeof(RX_handle)) ;
    memcpy(&RX_mhbo,    &aargv[1], sizeof(RX_mhbo))   ;
    memcpy(&RX_name,    &aargv[2], sizeof(RX_name))   ;
    memcpy(&RX_md,      &aargv[3], sizeof(RX_md))     ;
    memcpy(&RX_buffer,  &aargv[4], sizeof(RX_buffer)) ;
    memcpy(&RX_buflen,  &aargv[5], sizeof(RX_buflen)) ;
    memcpy(&RX_datalen, &aargv[6], sizeof(RX_datalen)) ;
    TRACE(traceid, ("RX_handle = %.*s\n",
          (int)RX_handle.strlength, RX_handle.strptr) ) ;
    TRACE(traceid, ("RX_mhbo = %.*s\n",
          (int)RX_mhbo.strlength, RX_mhbo.strptr) ) ;
    TRACE(traceid, ("RX_name = %.*s\n",
          (int)RX_name.strlength, RX_name.strptr) ) ;
    TRACE(traceid, ("RX_md = %.*s\n",
          (int)RX_md.strlength, RX_md.strptr) ) ;
    TRACE(traceid, ("RX_buffer = %.*s\n",
          (int)RX_buffer.strlength, RX_buffer.strptr) ) ;
    TRACE(traceid, ("RX_buflen = %.*s\n",
          (int)RX_buflen.strlength, RX_buflen.strptr) ) ;
    TRACE(traceid, ("RX_datalen = %.*s\n",
          (int)RX_datalen.strlength, RX_datalen.strptr) ) ;
    memset(bufferLengthText,
           0,
           sizeof(bufferLengthText)) ;
    if (RX_buflen.strlength >= sizeof(bufferLengthText))
      {
       rc = -16 ;
      }
    else
      {
       memcpy(bufferLengthText,
              RX_buflen.strptr,
              RX_buflen.strlength) ;
       errno = 0 ;
       bufferLengthEnd = NULL ;
       parsedBufferLength = strtoimax(bufferLengthText,
                                      &bufferLengthEnd,
                                      10) ;
   if (    (errno == ERANGE)
        || (bufferLengthEnd == bufferLengthText)
        || (*bufferLengthEnd != 0)
        || (parsedBufferLength < 0)
        || (parsedBufferLength > INT32_MAX) )
     {
      rc = -16 ;
     }
   else
     {
      bufferLength = (MQLONG)parsedBufferLength ;
     }
  }
    stem_to_int64(traceid, RX_handle, "", &hmsg64) ;
    hmsg = (MQHMSG)hmsg64 ;
    memcpy(&mhbo, &mhbo_default, sizeof(MQMHBO)) ;
    make_mhbo_from_stem(traceid, &mhbo, RX_mhbo) ;
    memcpy(&md, &md_default, sizeof(MQMD2)) ;
    make_md_from_stem(traceid, &md, RX_md) ;
    memset(&name, 0, sizeof(MQCHARV)) ;
    name.VSPtr     = RX_name.strptr ;
    name.VSLength  = RX_name.strlength ;
    name.VSCCSID   = MQCCSI_APPL ;
    name.VSBufSize = RX_name.strlength ;
   }
 if (rc == 0)
   {
    rexxrcBuffer = stem_from_bytes(traceid, NULL, RX_buffer, "", (MQBYTE *)"", 0) ;
    rexxrcDatalen = stem_from_long (traceid, NULL, RX_datalen, "", 0) ;
    if ( (rexxrcBuffer != RXSHV_OK) && (rexxrcBuffer != RXSHV_NEWV) )
      {
       TRACE(traceid, ("RexxVariablePool failed to initialize RX_buffer rc = %d\n",
             rexxrcBuffer) ) ;
      }
    if ( (rexxrcDatalen != RXSHV_OK) && (rexxrcDatalen != RXSHV_NEWV) )
      {
       TRACE(traceid, ("RexxVariablePool failed to initialize RX_datalen rc = %d\n",
             rexxrcDatalen) ) ;
      }
    if (    (rc == 0)
         && (    (    (rexxrcBuffer != RXSHV_OK)
                   && (rexxrcBuffer != RXSHV_NEWV) )
              || (    (rexxrcDatalen != RXSHV_OK)
                   && (rexxrcDatalen != RXSHV_NEWV) ) ) )
      {
       rc = -18 ;
      }
   }
if ((rc == 0) && (bufferLength > 0))
  {
   buffer = malloc(bufferLength) ;
   if (buffer == NULL)
     {
      rc = -17 ;
     }
   else
     {
      memset(buffer, 0, bufferLength) ;
     }
  }
 if (rc == 0)
   {
    TRACE(traceid, ("Calling MQMHBUF\n") ) ;
    MQMHBUF ( anchor->QMh,
              hmsg,
              &mhbo,
              &name,
              &md,
              bufferLength,
              buffer,
              &dataLength,
              &mqrc,
              &mqac ) ;
    mqmhbufCalled = 1 ;
    rc = mqrc ;
    TRACE(traceid, ("MQMHBUF returned mqrc=%"PRId32" mqac=%"PRId32" dataLength=%"PRId32" bufferLength=%"PRId32"\n",
          mqrc, mqac, dataLength, bufferLength) ) ;
   }
if (mqmhbufCalled)
  {
   rexxrcMhbo =
     make_stem_from_mhbo(traceid,
                         &mhbo,
                         RX_mhbo) ;
   if ( (rexxrcMhbo != RXSHV_OK) &&
        (rexxrcMhbo != RXSHV_NEWV) )
     {
      TRACE(traceid,
            ("RexxVariablePool failed to publish RX_mhbo rc = %d\n",
             rexxrcMhbo) ) ;
     }
   rexxrcMd =
     make_stem_from_md(traceid,
                       &md,
                       RX_md) ;
   if ( (rexxrcMd != RXSHV_OK) &&
        (rexxrcMd != RXSHV_NEWV) )
     {
      TRACE(traceid,
            ("RexxVariablePool failed to publish RX_md rc = %d\n",
             rexxrcMd) ) ;
     }
   rexxrcDatalen = stem_from_long(traceid,
                  NULL,
                  RX_datalen,
                  "",
                  dataLength) ;
   returnedBufferLength = dataLength ;
   if (returnedBufferLength < 0)
     {
      returnedBufferLength = 0 ;
     }
   if (returnedBufferLength > bufferLength)
     {
      returnedBufferLength = bufferLength ;
     }
   if (    (rc == 0)
        && (buffer != NULL)
        && (returnedBufferLength > 0) )
     {
      rexxrcBuffer = stem_from_bytes(traceid,
                      NULL,
                      RX_buffer,
                      "",
                      buffer,
                      returnedBufferLength) ;
     }
   else
     {
      rexxrcBuffer = stem_from_bytes(traceid,
                      NULL,
                      RX_buffer,
                      "",
                      (MQBYTE *)"",
                      0) ;
      }
   if ( (rexxrcDatalen != RXSHV_OK) && (rexxrcDatalen != RXSHV_NEWV) )
     {
      TRACE(traceid, ("RexxVariablePool failed to publish RX_datalen rc = %d\n",
            rexxrcDatalen) ) ;
     }
   if ( (rexxrcBuffer != RXSHV_OK) && (rexxrcBuffer != RXSHV_NEWV) )
     {
      TRACE(traceid, ("RexxVariablePool failed to publish RX_buffer rc = %d\n",
            rexxrcBuffer) ) ;
     }
   if (    (rc == 0)
        && (    (    (rexxrcMhbo != RXSHV_OK)
                  && (rexxrcMhbo != RXSHV_NEWV) )
             || (    (rexxrcMd != RXSHV_OK)
                  && (rexxrcMd != RXSHV_NEWV) )
             || (    (rexxrcDatalen != RXSHV_OK)
                  && (rexxrcDatalen != RXSHV_NEWV) )
             || (    (rexxrcBuffer != RXSHV_OK)
                  && (rexxrcBuffer != RXSHV_NEWV) ) ) )
     {
      rc = -18 ;
     }
  }
 if (buffer)
   {
    free(buffer) ;
    buffer = NULL ;
   }
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 return 0;
} // End of RXMQMBF function
//
// Do an Open   MQOPEN
//
//   Call:   rc = RXMQopen(iMQOD,opts,handle,oMQOD)
//
FTYPE  RXMQOPEN  RXMQPARM
 {
 
 RXMQCB                 * anchor = 0      ;  // RXMQ Control Block
 int                     i                ;  // Looper
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQLONG                  closecc = 0      ;
 MQLONG                  closerc = 0      ;
 MQULONG                 traceid = OPEN   ;  // This function trace id
 int                     odrc = 0          ;
 int                     odBuilt = 0       ;
 int                     rexxrc = RXSHV_OK ;
 int                     rexxrcOd = RXSHV_OK ;
 int                     resetrc = RXSHV_OK;
 
 RXSTRING                RXi_od           ;  // Stem Var Obj Desc Input
 RXSTRING                RXo_od           ;  // Stem Var Obj Desc Output
 RXSTRING                RX_opt           ;  // Open     Options
 RXSTRING                RX_handle        ;  //      Var Obj Handle
 
 MQOD                    od               ;  //MQ object desc
 MQLONG                  options    =  0  ;  //MQ open options
 int                     theobj     = -1  ;  //gmqo object to use
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null Input OD/Qname"},
        {  -3, "Zero length input OD/Qname"},
        {  -4, "Null options"},
        {  -5, "Zero length options"},
        {  -6, "Null handle name"},
        {  -7, "Zero length handle name"},
        {  -8, "Null Output OD"},
        {  -9, "Zero length output OD"},
        { -10, "No available Q objects"},
        { -11, "Unable to publish output handle to REXX"},
        { -12, "Unable to publish output OD to REXX"},
        { -13, "Object name too long"},
        { -14, "Invalid options"},
        { -15, "Unable to build MQCHARV input"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 4 ) )             rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc =  -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc =  -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc =  -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )    rc =  -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) ) rc =  -7 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[3]) )    rc =  -8 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[3]) ) rc =  -9 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// Now the parms are correct, get them
//
 if (rc == 0)
    {
      memcpy(&RXi_od    ,&aargv[0],sizeof(RXi_od))    ;
      memcpy(&RX_opt    ,&aargv[1],sizeof(RX_opt  ))  ;
      memcpy(&RX_handle ,&aargv[2],sizeof(RX_handle)) ;
      memcpy(&RXo_od    ,&aargv[3],sizeof(RXo_od))    ;
 
      TRACE(traceid, ("RXi_od = %.*s\n",   (int)RXi_od.strlength,   RXi_od.strptr) )     ;
      TRACE(traceid, ("RX_opt = %.*s\n",   (int)RX_opt.strlength,   RX_opt.strptr) )     ;
      TRACE(traceid, ("RX_handle = %.*s\n",(int)RX_handle.strlength,RX_handle.strptr) )  ;
      TRACE(traceid, ("RXo_od = %.*s\n",   (int)RXo_od.strlength,   RXo_od.strptr) )     ;
 
      odrc = make_od_from_stem(traceid,&od,RXi_od) ;
      odBuilt = 1                                  ;
      if ( odrc == -1 ) rc = -13                   ;
      if ( odrc == -2 ) rc = -15                   ;
      if ( (rc == 0) && (parm_to_ulong(RX_opt, &options) != 0) ) rc = -14 ;
     }
 
 
//
// Initialize the REXX output handle
//
 if (rc == 0)
   {
    rexxrc = stem_from_long(traceid, NULL, RX_handle, "", 0) ;
    if ( (rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV) )
      {
       TRACE(traceid, ("RexxVariablePool failed to initialize output handle rc = %d\n",
                       rexxrc) ) ;
       rc = -11 ;
      }
   }
 
//
// Select the handle slot
//
 if (rc == 0)
   {
    for ( i=MINQS ; i <= MAXQS ; i++ )
      {
       if ( anchor->Qh[i] == 0 )
         {
          theobj = i ;
          TRACE(traceid, ("Selected Qh Object [%d]\n",theobj) ) ;
          break      ;
         }
      }
 
    if ( (theobj == -1) ) rc = -10 ;
   }
 
//
// Do the MQOPEN on the obtained Queue object
//
 if (rc == 0)
   {
    TRACE(traceid, ("Calling MQOPEN with options = %"PRId32"\n",(int32_t)options) ) ;
    MQOPEN ( anchor->QMh, &od, options, &anchor->Qh[theobj], &mqrc, &mqac ) ;
    rc = mqrc ;
 
    if ( anchor->Qh[theobj] != 0 )   //If the Open worked,
      {                              //then .....
       rexxrc = stem_from_long(traceid, NULL, RX_handle, "", theobj) ;
       if ( (rexxrc == RXSHV_OK) || (rexxrc == RXSHV_NEWV) )
         {
          rexxrcOd = make_stem_from_od(traceid,&od,RXo_od) ; //and update the OD
          if ( (rexxrcOd != RXSHV_OK) && (rexxrcOd != RXSHV_NEWV) )
            {
             TRACE(traceid, ("RexxVariablePool failed to publish RXo_od rc = %d\n",
                             rexxrcOd) ) ;
             if (rc == 0) rc = -12 ;
            }
         }
       else
         {
          TRACE(traceid, ("RexxVariablePool failed to publish output handle rc = %d\n",
                          rexxrc) ) ;
          TRACE(traceid, ("Calling MQCLOSE to clean up unpublished output handle\n") ) ;
          MQCLOSE ( anchor->QMh, &anchor->Qh[theobj], MQCO_NONE,
                    &closecc, &closerc ) ;
          TRACE(traceid, ("MQCLOSE cleanup cc = %"PRId32", reason = %"PRId32"\n",
                          (int32_t)closecc,(int32_t)closerc) ) ;
          resetrc = stem_from_long(traceid, NULL, RX_handle, "", 0) ;
          if ( (resetrc != RXSHV_OK) && (resetrc != RXSHV_NEWV) )
            TRACE(traceid, ("Failed to reset output handle rc = %d\n",resetrc) ) ;
          rc = -11 ;
         }
      }
   }
 
//
// Set the LAST variables, and the function return string
//
 if (odBuilt)
   free_od_mqcharv(&od) ;

 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQOPEN function
 
//
// Do a Close   MQCLOSE
//
//   Call:   rc = RXMQclos(handle,opts)
//
FTYPE  RXMQCLOS  RXMQPARM
 {
 
 RXMQCB                 * anchor = 0      ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = CLOSE  ;  // This function trace id
 
 RXSTRING                RX_opts          ;  // Data     Options
 RXSTRING                RX_handle        ;  // Data     Obj Handle
 
 MQLONG                  options  = 0     ;  //MQ close options
 MQLONG                  handle   = 0     ;  //MQ object number
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null handle"},
        {  -3, "Zero length handle"},
        {  -4, "Null options"},
        {  -5, "Zero length options"},
        {  -6, "Handle out of range"},
        {  -7, "Invalid handle"},
        {  -8, "Invalid options"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 2 ) )             rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc =  -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc =  -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc =  -5 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(&RX_handle,&aargv[0],sizeof(RX_handle))   ;
    memcpy(&RX_opts  ,&aargv[1],sizeof(RX_opts  ))   ;
 
    TRACE(traceid, ("RX_opts = %.*s\n",  (int)RX_opts.strlength,  RX_opts.strptr) )   ;
    TRACE(traceid, ("RX_handle = %.*s\n",(int)RX_handle.strlength,RX_handle.strptr) ) ;
 
    if ( parm_to_ulong(RX_opts, &options) != 0 ) rc = -8 ;
    if ( (rc == 0) && (parm_to_ulong(RX_handle, &handle) != 0) ) rc = -6 ;
   }
 
//
// See if the handle is valid (ie: the gmqo to use)
//
 if ( (rc == 0) && ( ( handle > MAXQS ) || ( handle <= 0 ) ) ) rc = -6 ;
 if ( (rc == 0) && ( anchor->Qh[handle] == 0 ) )               rc = -7 ;
 
//
// Now close the object
//
 if (rc == 0)
   {
    TRACE(traceid, ("Closing object handle [%"PRIu32"]\n",(uint32_t)handle) ) ;
    MQCLOSE ( anchor->QMh, &anchor->Qh[handle], options, &mqrc, &mqac )       ;
    rc = mqrc ;
    if ( mqac == 0 )                  //If the Close worked,
      {                               //then .....
       anchor->Qh[handle] = 0 ;       //loose the MQ object
      }
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQCLOS function
 
//
// Do a Syncpoint    MQCMIT
//
//   Call:   rc = RXMQcmit()
//
FTYPE  RXMQCMIT  RXMQPARM
 {
 
 RXMQCB                 * anchor = 0      ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = CMIT   ;  // This function trace id
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 0 ) )             rc =  -1 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// Do the Actual Syncpoint on the required Queue Manager
//        This also forces it for all others on the thread
//
 if (rc == 0)
   {
    MQCMIT ( anchor->QMh, &mqrc, &mqac ) ;
    rc   = mqrc                          ;
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQCMIT function
 
//
// Do a Rollback     MQBACK
//
//   Call:   rc = RXMQback()
//
FTYPE  RXMQBACK  RXMQPARM
 {
 
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = BACK   ;  // This function trace id
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 0 ) )             rc =  -1 ;
 if ( (rc == 0) && (anchor->QMh == 0 ) )       rc = -98 ;
 
//
// Do the Actual Backout on the required Queue Manager
//        This also forces it for all others on the thread
//
 if (rc == 0)
   {
    MQBACK ( anchor->QMh, &mqrc, &mqac ) ;
    rc   = mqrc                          ;
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
 return 0;
 } // End of RXMQBACK function
 
//
//
//
//
//
// Do a Put     MQPUT
//
//   Call:   rc = RXMQput(handle, data,
//                        input_msgdesc, output_msgdesc,
//                        input_pmo, output_pmo)
//
//   MQ properties:
//        input_pmo.NMH maps to MQPMO.NewMsgHandle.
//        A message handle created by RXMQMH and populated by RXMQSMP
//        can therefore be passed directly to MQPUT.
//        RXMQMBF is not required for this standard IBM MQ scenario.
//
FTYPE  RXMQPUT  RXMQPARM
 {
 
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc   = 0         ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = PUT    ;  // This function trace id
 int                     rexxrcMd = RXSHV_OK ;
 int                     rexxrcPo = RXSHV_OK ;
 
 RXSTRING                RX_handle        ;  // Obj Handle
 RXSTRING                RX_data          ;  // Variable Data
 RXSTRING                RXi_md           ;  // Variable Input  Msg Desc
 RXSTRING                RXo_md           ;  // Variable Output Msg Desc
 RXSTRING                RXi_pmo          ;  // Variable Input  PMO
 RXSTRING                RXo_pmo          ;  // Variable Output PMO
 
 MQLONG                  handle           ;  //MQ object number
 MQMD2                   od               ;  //MQ Message descriptor
 MQPMO                   pmo              ;  //MQ Put Message options
 MQLONG                  data0 = 0        ;  // Variable Data len
 MQBYTE               *  data  = 0        ;  //-> Data buffer
 int                     datalen          ;  //   Data length
 RXMQ_EXACT_FETCH_RESULT   fetchResult = RXMQ_EXACT_FETCH_INVALID ;
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null handle"},
        {  -3, "Zero data handle"},
        {  -4, "Null data stem var"},
        {  -5, "Zero data stem var"},
        {  -6, "Null input MsgDesc"},
        {  -7, "Zero length input MsgDesc"},
        {  -8, "Null output MsgDesc"},
        {  -9, "Zero length output MsgDesc"},
        { -10, "Null input PMO"},
        { -11, "Zero length input PMO"},
        { -12, "Null output PMO"},
        { -13, "Zero length output PMO"},
        { -14, "Handle out of range"},
        { -15, "Invalid handle"},
        { -16, "malloc failure, check reason code"},
        { -17, "Zero length input data buffer"},
        { -18, "Data length is not equal to specified value"},
        { -19, "Context handle out of range"},
        { -20, "Invalid Context handle"},
        { -21, "Unable to publish output to REXX"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 6 ) )             rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc =  -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc =  -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc =  -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )    rc =  -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) ) rc =  -7 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[3]) )    rc =  -8 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[3]) ) rc =  -9 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[4]) )    rc = -10 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[4]) ) rc = -11 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[5]) )    rc = -12 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[5]) ) rc = -13 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(&RX_handle,&aargv[0],sizeof(RX_handle)) ;
    memcpy(&RX_data,  &aargv[1],sizeof(RX_data))   ;
    memcpy(&RXi_md,   &aargv[2],sizeof(RXi_md))    ;
    memcpy(&RXo_md,   &aargv[3],sizeof(RXo_md))    ;
    memcpy(&RXi_pmo,  &aargv[4],sizeof(RXi_pmo))   ;
    memcpy(&RXo_pmo,  &aargv[5],sizeof(RXo_pmo))   ;
 
    TRACE(traceid, ("RX_handle = %.*s\n",(int)RX_handle.strlength,RX_handle.strptr) ) ;
    TRACE(traceid, ("RX_data = %.*s\n",  (int)RX_data.strlength,  RX_data.strptr)   ) ;
    TRACE(traceid, ("RXi_md = %.*s\n",   (int)RXi_md.strlength,   RXi_md.strptr)    ) ;
    TRACE(traceid, ("RXo_md = %.*s\n",   (int)RXo_md.strlength,   RXo_md.strptr)    ) ;
    TRACE(traceid, ("RXi_pmo = %.*s\n",  (int)RXi_pmo.strlength,  RXi_pmo.strptr)   ) ;
    TRACE(traceid, ("RXo_pmo = %.*s\n",  (int)RXo_pmo.strlength,  RXo_pmo.strptr)   ) ;
 
    make_md_from_stem(traceid,&od, RXi_md )          ;
    make_po_from_stem(traceid,&pmo , RXi_pmo )       ;
 
    if ( parm_to_ulong(RX_handle, &handle) != 0 ) rc = -14 ;
    stem_to_long(traceid, RX_data, "0" , &data0)     ;
   }
 
//
// Now check the input Stem variable to see that there is
//     some valid data to obtain
//
 if ( (rc == 0) && ( data0 <= 0 ) ) rc = -17    ;
 
//
// Allocate data buffer to store stem.1 variable data.
//
 if ( rc == 0 )
   {
    fetchResult = fetch_exact_rexx_bytes(traceid, RX_data, "1",
                                         data0, &data) ;
    if ( fetchResult == RXMQ_EXACT_FETCH_NOMEM )
      {
       mqac = errno                                                        ;
       TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) )          ;
       rc = -16                                                            ;
      }
    else if ( fetchResult != RXMQ_EXACT_FETCH_SUCCESS )
      rc = -18 ;
    else
      {
       datalen = data0 ;
       TRACE(traceid, ("Length of data received = %d\n",datalen) ) ;
      }
   }
 
//
// Now check if stem.0 specifies the same value as stem.1 length.
// If not, don't know what to do.
//
 if ( (rc == 0) && ( datalen != data0 ) ) rc = -18;
 
//
// See if the output queue handle is valid
//
 if ( (rc == 0) && ( ( handle > MAXQS ) || ( handle <= 0 ) ) )   rc = -14 ;
 if ( (rc == 0) && ( anchor->Qh[handle] == 0 ) )                 rc = -15 ;
 
 
//
// If Context is specified (for MQPMO_PASS_ALL_CONTEXT)
// see if the input queue handle is valid
//
 if ( (rc == 0) && ( pmo.Context != 0 ) )
   {
    if ( ( pmo.Context > MAXQS ) || ( pmo.Context < 0 ) )  rc = -19 ;
    if ( (rc == 0) && (anchor->Qh[pmo.Context] == 0 ) )    rc = -20 ;
    if   (rc == 0) pmo.Context = anchor->Qh[pmo.Context];
    }
 
//
// Now put the data to the queue
//
 if (rc == 0)
   {
    TRACE(traceid, ("PUT Maxdatalen = %"PRId32"\n",(int32_t)data0) )                ;
    TRACE(traceid, ("PUT PMO.NewMsgHandle = %"PRIu64"\n",(uint64_t)pmo.NewMsgHandle) ) ;
    MQPUT ( anchor->QMh, anchor->Qh[handle], &od, &pmo, data0, data, &mqrc, &mqac ) ;
    rc = mqrc ;
    TRACE(traceid, ("PUT rc = %"PRId32", ac = %"PRId32"\n",(int32_t)mqrc, (int32_t)mqac) ) ;
 
    rexxrcMd =
      make_stem_from_md(traceid,
                        &od,
                        RXo_md) ;   //Set the return Variables
    if ( (rexxrcMd != RXSHV_OK) &&
         (rexxrcMd != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RXo_md rc = %d\n",
              rexxrcMd) ) ;
      }
    rexxrcPo =
      make_stem_from_po(traceid,
                        &pmo,
                        RXo_pmo) ;
    if ( (rexxrcPo != RXSHV_OK) &&
         (rexxrcPo != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RXo_pmo rc = %d\n",
              rexxrcPo) ) ;
      }
    if (    (rc == 0)
         && (    ((rexxrcMd != RXSHV_OK) &&
                  (rexxrcMd != RXSHV_NEWV))
              || ((rexxrcPo != RXSHV_OK) &&
                  (rexxrcPo != RXSHV_NEWV)) ) )
      {
       rc = -21 ;
      }
   }
 
//
// Free data buffer for stem.1 variable data, if allocated.
//
 if ( data != 0 )
   {
    TRACE(traceid, ("Free area\n") ) ;
    free(data) ;
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
 return 0;
 } // End of RXMQPUT function
 
//
// Do a put    MQPUT1
//
//   Call:   rc = RXMQPUT1(input1_objdesc, output1_objdesc, data,
//                         input1_msgdesc,output1_msgdesc,
//                         input1_pmo,output1_pmo)
//        or
//           rc = RXMQPUT1(queue_name, output1_objdesc, data,
//                         input1_msgdesc,output1_msgdesc,
//                         input1_pmo,output1_pmo)
//
FTYPE RXMQPUT1  RXMQPARM
 {
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = PUT1   ;  // This function trace id
 int                     odrc = 0          ;
 int                     odBuilt = 0       ;
 int                     rexxrcMd = RXSHV_OK ;
 int                     rexxrcPo = RXSHV_OK ;
 int                     rexxrcOd = RXSHV_OK ;
 
 RXSTRING                RXi_od           ;  // Stem Var Obj Desc Input
 RXSTRING                RXo_od           ;  // Stem Var Obj Desc Output
 RXSTRING                RX_data          ;  // Variable Data
 RXSTRING                RXi_md           ;  // Variable Input1  Msg Desc
 RXSTRING                RXo_md           ;  // Variable Output1 Msg Desc
 RXSTRING                RXi_pmo          ;  // Variable Input1  PMO
 RXSTRING                RXo_pmo          ;  // Variable Output1 PMO
 
 MQOD                    od               ;  // MQ object descriptor
 MQMD2                   md               ;  // MQ Message descriptor
 MQPMO                   pmo              ;  // MQ put1 Message options
 MQBYTE               *  data  = 0        ;  //-> Data buffer
 MQLONG                  data0 = 0        ;  // Variable Data len
 int                     datalen          ;  //   Data length
 RXMQ_EXACT_FETCH_RESULT   fetchResult = RXMQ_EXACT_FETCH_INVALID ;
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null input OD/Qname"},
        {  -3, "Zero length input OD/Qname"},
        {  -4, "Null output OD"},
        {  -5, "Zero length output OD"},
        {  -6, "Null data stem var"},
        {  -7, "Zero data stem var"},
        {  -8, "Null input MsgDesc"},
        {  -9, "Zero length input MsgDesc"},
        { -10, "Null output MsgDesc"},
        { -11, "Zero length output MsgDesc"},
        { -12, "Null input PMO"},
        { -13, "Zero length input PMO"},
        { -14, "Null output PMO"},
        { -15, "Zero length output PMO"},
        { -16, "Unable to publish output to REXX"},
        { -17, "malloc failure, check reason code"},
        { -18, "Zero length input data buffer"},
        { -19, "Data length is not equal to specified value"},
        { -20, "Context handle out of range"},
        { -21, "Invalid context handle"},
        { -22, "Object name too long"},
        { -23, "Unable to build MQCHARV input"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 7 ) )             rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc =  -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc =  -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc =  -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )    rc =  -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) ) rc =  -7 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[3]) )    rc =  -8 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[3]) ) rc =  -9 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[4]) )    rc = -10 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[4]) ) rc = -11 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[5]) )    rc = -12 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[5]) ) rc = -13 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[6]) )    rc = -14 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[6]) ) rc = -15 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(&RXi_od, &aargv[0],sizeof(RXi_od))  ;
    memcpy(&RXo_od, &aargv[1],sizeof(RXo_od))  ;
    memcpy(&RX_data,&aargv[2],sizeof(RX_data)) ;
    memcpy(&RXi_md, &aargv[3],sizeof(RXi_md))  ;
    memcpy(&RXo_md, &aargv[4],sizeof(RXo_md))  ;
    memcpy(&RXi_pmo,&aargv[5],sizeof(RXi_pmo)) ;
    memcpy(&RXo_pmo,&aargv[6],sizeof(RXo_pmo)) ;
 
    TRACE(traceid, ("RXi_od = %.*s\n", (int)RXi_od.strlength, RXi_od.strptr)  ) ;
    TRACE(traceid, ("RXo_od = %.*s\n", (int)RXo_od.strlength, RXo_od.strptr)  ) ;
    TRACE(traceid, ("RX_data = %.*s\n",(int)RX_data.strlength,RX_data.strptr) ) ;
    TRACE(traceid, ("RXi_md = %.*s\n", (int)RXi_md.strlength, RXi_md.strptr)  ) ;
    TRACE(traceid, ("RXo_md = %.*s\n", (int)RXo_md.strlength, RXo_md.strptr)  ) ;
    TRACE(traceid, ("RXi_pmo = %.*s\n",(int)RXi_pmo.strlength,RXi_pmo.strptr) ) ;
    TRACE(traceid, ("RXo_pmo = %.*s\n",(int)RXo_pmo.strlength,RXo_pmo.strptr) ) ;
 
    odrc = make_od_from_stem(traceid,&od, RXi_od       ) ;
    odBuilt = 1                                         ;
    if ( odrc == -1 ) rc = -22                          ;
    if ( odrc == -2 ) rc = -23                          ;
    make_md_from_stem(traceid,&md, RXi_md            ) ;
    make_po_from_stem(traceid,&pmo,RXi_pmo           ) ;
    stem_to_long     (traceid,RX_data, "0" , &data0  ) ;
   }
 
//
// Now check the input Stem variable to see that there is
//     some valid data to obtain
//
 if ( (rc == 0) && ( data0 <= 0 ) )  rc = -18 ;
 
//
// Allocate data buffer to store stem.1 variable data.
//
 if ( rc == 0 )
   {
    fetchResult = fetch_exact_rexx_bytes(traceid, RX_data, "1",
                                         data0, &data) ;
    if ( fetchResult == RXMQ_EXACT_FETCH_NOMEM )
      {
       mqac = errno                                                         ;
       TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) )           ;
       rc = -17                                                             ;
      }
    else if ( fetchResult != RXMQ_EXACT_FETCH_SUCCESS )
      rc = -19 ;
    else
      {
       datalen = data0 ;
       TRACE(traceid, ("Length of data received = %d\n",datalen) ) ;
      }
   }
 
//
// Now check if stem.0 specifies the same value as stem.1 length.
// If not, don't know what to do.
//
 if ( (rc == 0) && ( datalen != data0 ) ) rc = -19 ;
 
 //
 // If Context is specified (for MQPMO_PASS_ALL_CONTEXT)
 // see if the input queue handle is valid (ie: the gmqo to use)
 //
  if ( (rc == 0) && ( pmo.Context != 0 ) )
    {
     if ( ( pmo.Context > MAXQS ) || ( pmo.Context < 0 ) ) rc = -20 ;
     if ( (rc == 0) && (anchor->Qh[pmo.Context] == 0 ) )   rc = -21 ;
     if (rc == 0) pmo.Context = anchor->Qh[pmo.Context]             ;
    }
 
//
// Now execute PUT1
//
 if (rc == 0)
   {
    TRACE(traceid, ("PUT1 Maxdatalen is %"PRId32"\n",(int32_t)data0) ) ;
    MQPUT1 ( anchor->QMh, &od, &md, &pmo, data0, data, &mqrc, &mqac )  ;
    rc   = mqrc ;
    TRACE(traceid, ("PUT1 rc = %"PRId32", ac = %"PRId32"\n",
          (int32_t)mqrc,(int32_t)mqac) )                               ;
 
    rexxrcMd =
      make_stem_from_md(traceid,
                        &md,
                        RXo_md) ;
    if ( (rexxrcMd != RXSHV_OK) &&
         (rexxrcMd != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RXo_md rc = %d\n",
              rexxrcMd) ) ;
      }
    rexxrcPo =
      make_stem_from_po(traceid,
                        &pmo,
                        RXo_pmo) ;
    if ( (rexxrcPo != RXSHV_OK) &&
         (rexxrcPo != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RXo_pmo rc = %d\n",
              rexxrcPo) ) ;
      }
    rexxrcOd =
      make_stem_from_od(traceid,
                        &od,
                        RXo_od) ;
    if ( (rexxrcOd != RXSHV_OK) &&
         (rexxrcOd != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RXo_od rc = %d\n",
              rexxrcOd) ) ;
      }
    if (    (rc == 0)
         && (    ((rexxrcMd != RXSHV_OK) &&
                  (rexxrcMd != RXSHV_NEWV))
              || ((rexxrcPo != RXSHV_OK) &&
                  (rexxrcPo != RXSHV_NEWV))
              || ((rexxrcOd != RXSHV_OK) &&
                  (rexxrcOd != RXSHV_NEWV)) ) )
      {
       rc = -16 ;
      }
   }
 
 
//
// Free data buffer for stem.1 variable data, if allocated.
//
 if ( data != 0 )
   {
    TRACE(traceid, ("Free area\n") ) ;
    free(data) ;
   }

 if (odBuilt)
   free_od_mqcharv(&od) ;
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
} // End of RXMQPUT1 function
 
//
// Do a Get     MQGET
//
//   Call:   rc = RXMQget(handle, data,
//                        input_msgdesc,output_msgdesc,
//                        input_gmo,output_gmo)
//
//   MQ properties:
//        input_gmo.MH maps to MQGMO.MsgHandle.
//        A message handle created by RXMQMH can therefore receive
//        properties directly from MQGET and be read by RXMQIMP.
//        RXMQBMH is not required for this standard IBM MQ scenario.
//
FTYPE  RXMQGET  RXMQPARM
 {
 
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = GET    ;  // This function trace id
 int                     rexxrc0 = RXSHV_OK ;
 int                     rexxrc1 = RXSHV_OK ;
 int                     rexxrcMd = RXSHV_OK ;
 int                     rexxrcGo = RXSHV_OK ;
 
 RXSTRING                RX_handle        ;  // Obj Handle
 RXSTRING                RX_data          ;  // Variable Data
 RXSTRING                RXi_md           ;  // Variable Input  Msg Desc
 RXSTRING                RXo_md           ;  // Variable Output Msg Desc
 RXSTRING                RXi_gmo          ;  // Variable Input  GMO
 RXSTRING                RXo_gmo          ;  // Variable Output GMO
 
 MQLONG                  handle    = 0    ;  // MQ object number
 MQMD2                   md               ;  // MQ Message descriptor
 MQGMO                   gmo              ;  // MQ Get Message options
 void                 *  data      = 0    ;  //-> Data buffer
 MQLONG                  data0     = 0    ;  //   Data length max
 MQLONG                  datalen   = 0    ;  //   Data length actual
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null handle"},
        {  -3, "Zero data handle"},
        {  -4, "Null data stem var"},
        {  -5, "Zero data stem var"},
        {  -6, "Null input MsgDesc"},
        {  -7, "Zero length input MsgDesc"},
        {  -8, "Null output MsgDesc"},
        {  -9, "Zero length output MsgDesc"},
        { -10, "Null input GMO"},
        { -11, "Zero length input GMO"},
        { -12, "Null output GMO"},
        { -13, "Zero length output GMO"},
        { -14, "Handle out of range"},
        { -15, "Invalid handle"},
        { -16, "malloc failure, check reason code"},
        { -17, "Zero length input data buffer"},
        { -18, "Unable to publish output data to REXX"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 
 if ( (rc == 0) && (aargc != 6 ) )             rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc =  -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc =  -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc =  -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )    rc =  -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) ) rc =  -7 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[3]) )    rc =  -8 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[3]) ) rc =  -9 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[4]) )    rc = -10 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[4]) ) rc = -11 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[5]) )    rc = -12 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[5]) ) rc = -13 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// Now the parms are correct, get them
//
 
 if (rc == 0)
   {
    memcpy(&RX_handle,&aargv[0],sizeof(RX_handle)) ;
    memcpy(&RX_data,  &aargv[1],sizeof(RX_data))   ;
    memcpy(&RXi_md,   &aargv[2],sizeof(RXi_md))    ;
    memcpy(&RXo_md,   &aargv[3],sizeof(RXo_md))    ;
    memcpy(&RXi_gmo,  &aargv[4],sizeof(RXi_gmo))   ;
    memcpy(&RXo_gmo,  &aargv[5],sizeof(RXo_gmo))   ;
 
    TRACE(traceid, ("RX_handle = %.*s\n",(int)RX_handle.strlength,RX_handle.strptr) ) ;
    TRACE(traceid, ("RX_data = %.*s\n",  (int)RX_data.strlength,  RX_data.strptr)   ) ;
    TRACE(traceid, ("RXi_md = %.*s\n",   (int)RXi_md.strlength,   RXi_md.strptr)    ) ;
    TRACE(traceid, ("RXo_md = %.*s\n",   (int)RXo_md.strlength,   RXo_md.strptr)    ) ;
    TRACE(traceid, ("RXi_gmo = %.*s\n",  (int)RXi_gmo.strlength,  RXi_gmo.strptr)   ) ;
    TRACE(traceid, ("RXo_gmo = %.*s\n",  (int)RXo_gmo.strlength,  RXo_gmo.strptr)   ) ;
 
 
    make_md_from_stem(traceid,&md, RXi_md )      ;
    make_go_from_stem(traceid,&gmo , RXi_gmo )   ;
    if ( parm_to_ulong(RX_handle, &handle) != 0 ) rc = -14 ;
    stem_to_long(traceid, RX_data, "0" , &data0) ;
   }
 
//
// Now check the input Stem variable to see that there is
//     some valid data to obtain
//
  if ( (rc == 0) && ( data0 <= 0 ) ) rc = -17 ;
 
//
// See if the handle is valid
//
 if ( (rc == 0) && ( ( handle > MAXQS ) || ( handle <= 0 ) ) ) rc = -14 ;
 if ( (rc == 0) && ( anchor->Qh[handle] == 0 ) )               rc = -15 ;
 
//
// Now GETMAIN the buffer to receive the data records
//
 if (( rc == 0) && ( data0 != 0 ) )
   {
    TRACE(traceid, ("Doing malloc for %"PRId32" bytes\n",(int32_t)data0) ) ;
    data = malloc(data0)                                                   ;
    if ( data == NULL )
    {
     mqac = errno                                                          ;
     TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) )            ;
     rc = -16                                                              ;
    }
   }
 
//
// Initialize the REXX output data
//
 if (rc == 0)
   {
    rexxrc0 = stem_from_long(traceid, NULL, RX_data, "0", 0) ;
    if ( (rexxrc0 != RXSHV_OK) && (rexxrc0 != RXSHV_NEWV) )
      TRACE(traceid, ("RexxVariablePool failed to initialize RX_data.0 rc = %d\n",
                      rexxrc0) ) ;

    rexxrc1 = stem_from_bytes(traceid, NULL, RX_data, "1",
                              (MQBYTE *)"", 0) ;
    if ( (rexxrc1 != RXSHV_OK) && (rexxrc1 != RXSHV_NEWV) )
      TRACE(traceid, ("RexxVariablePool failed to initialize RX_data.1 rc = %d\n",
                      rexxrc1) ) ;

    if (   ((rexxrc0 != RXSHV_OK) && (rexxrc0 != RXSHV_NEWV))
        || ((rexxrc1 != RXSHV_OK) && (rexxrc1 != RXSHV_NEWV)) )
      rc = -18 ;
   }
 
//
// Now get the data from the queue
//
 if (rc == 0)
   {
    TRACE(traceid, ("GET Maxdatalen = %"PRId32"\n",(int32_t)data0) )                          ;
    TRACE(traceid, ("GET GMO.MsgHandle = %"PRIu64"\n",(uint64_t)gmo.MsgHandle) )              ;
    MQGET ( anchor->QMh, anchor->Qh[handle], &md, &gmo, data0, data, &datalen, &mqrc, &mqac ) ;
    rc = mqrc                                                                                 ;
    TRACE(traceid, ("GET rc = %"PRId32", ac = %"PRId32", datalen = %"PRId32"\n",
          (int32_t)mqrc,(int32_t)mqac,(int32_t)datalen) )                                     ;
 
    rexxrcMd = make_stem_from_md(traceid,&md,  RXo_md  ) ;  //Set the return Variables
    if ( (rexxrcMd != RXSHV_OK) && (rexxrcMd != RXSHV_NEWV) )
      TRACE(traceid, ("RexxVariablePool failed to publish RXo_md rc = %d\n",
                      rexxrcMd) ) ;
    rexxrcGo = make_stem_from_go(traceid,&gmo, RXo_gmo ) ;
    if ( (rexxrcGo != RXSHV_OK) && (rexxrcGo != RXSHV_NEWV) )
      TRACE(traceid, ("RexxVariablePool failed to publish RXo_gmo rc = %d\n",
                      rexxrcGo) ) ;
 
    rexxrc0 = stem_from_long (traceid, NULL, RX_data, "0" , datalen)       ;
    if (datalen > data0) datalen = data0                                   ;
    rexxrc1 = stem_from_bytes(traceid, NULL, RX_data, "1" , (MQBYTE *)data, datalen) ;

    if ( (rexxrc0 != RXSHV_OK) && (rexxrc0 != RXSHV_NEWV) )
      TRACE(traceid, ("RexxVariablePool failed to publish RX_data.0 rc = %d\n",
                      rexxrc0) ) ;
    if ( (rexxrc1 != RXSHV_OK) && (rexxrc1 != RXSHV_NEWV) )
      TRACE(traceid, ("RexxVariablePool failed to publish RX_data.1 rc = %d\n",
                      rexxrc1) ) ;

    if (   ((rexxrcMd != RXSHV_OK) && (rexxrcMd != RXSHV_NEWV))
        || ((rexxrcGo != RXSHV_OK) && (rexxrcGo != RXSHV_NEWV))
        || ((rexxrc0 != RXSHV_OK) && (rexxrc0 != RXSHV_NEWV))
        || ((rexxrc1 != RXSHV_OK) && (rexxrc1 != RXSHV_NEWV)) )
      if (rc == 0) rc = -18 ;
   }
 
//
// Free data buffer for stem.1 variable data, if allocated.
//
 if ( data != 0 )
   {
    TRACE(traceid, ("Free area\n") ) ;
    free(data) ;
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQGET function
 
//
// Do an inquire MQINQ
//
//   Call:   rc = RXMQinq(handle, attribute, attrsetting )
//
//           NB: only 1 attribute at a time!
//
//
FTYPE  RXMQINQ  RXMQPARM
 {
 
 RXMQCB         * anchor = 0           ;  // RXMQ Control Block
 MQLONG           rc = 0               ;  // Function Return Code
 MQLONG           mqrc = 0             ;  // MQ RC
 MQLONG           mqac = 0             ;  // MQ AC
 MQULONG          traceid = INQ        ;  // This function trace id
 int              rexxrc = RXSHV_OK   ;
 
 RXSTRING         RX_handle            ;  // Data     Obj Handle
 RXSTRING         RX_attr              ;  // Variable Input  Attr
 RXSTRING         RX_value             ;  // Variable Output Attr
 
 MQLONG           handle       = 0     ;  // MQ object number
 MQLONG           attrib       = 0     ;  // object attribute
 
 MQLONG           inqselcount  = 1     ;  //INQ - number of sels
 MQLONG           inqselicount = 1     ;  //INQ - number of int sels
 MQLONG           inqints      = 0     ;  //INQ - integer return
 MQLONG           inqcharlen   = 600   ;  //INQ - char return length
 char             inqchars[601]        ;  //INQ - char return
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null handle"},
        {  -3, "Zero data handle"},
        {  -4, "Null data input attr"},
        {  -5, "Zero data input attr"},
        {  -6, "Null output attr"},
        {  -7, "Zero length output attr"},
        {  -8, "No attribute supplied"},
        {  -9, "Attribute out of valid range"},
        { -10, "Handle out of range"},
        { -11, "Invalid handle"},
        { -12, "Unable to publish output attribute to REXX"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 3 ) )             rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc =  -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc =  -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc =  -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )    rc =  -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) ) rc =  -7 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(&RX_handle,&aargv[0],sizeof(RX_handle)) ;
    memcpy(&RX_attr,  &aargv[1],sizeof(RX_attr))   ;
    memcpy(&RX_value, &aargv[2],sizeof(RX_value))  ;
 
    TRACE(traceid, ("RX_handle = %.*s\n",(int)RX_handle.strlength,RX_handle.strptr) ) ;
    TRACE(traceid, ("RX_attr = %.*s\n",  (int)RX_attr.strlength,  RX_attr.strptr)   ) ;
    TRACE(traceid, ("RX_value = %.*s\n", (int)RX_value.strlength, RX_value.strptr)  ) ;
 
    if ( parm_to_ulong(RX_handle, &handle) != 0 ) rc = -10 ;
    if ( (rc == 0) && (parm_to_ulong(RX_attr, &attrib) != 0) ) rc = -9 ;
 
    if ( (rc == 0) && (attrib == 0) ) rc = -8          ;
 
    if (    (rc == 0)
         && !( ( (attrib >= MQIA_FIRST) && (attrib <= MQIA_LAST  ) )
            || ( (attrib >= MQCA_FIRST) && (attrib <= MQCA_LAST  ) ) ) ) rc = -9;
   }
//
// See if the handle is valid
//
 if ( (rc == 0) && ( ( handle > MAXQS ) || ( handle <= 0 ) ) ) rc = -10 ;
 if ( (rc == 0) && ( anchor->Qh[handle] == 0 ) )               rc = -11 ;
 
//
// Format up either a character or an integer area
//
 if (rc == 0)
   {
    memset(inqchars,0,sizeof(inqchars)) ;
    if ( (attrib >= MQCA_FIRST  ) && (attrib <= MQCA_LAST  ) )
      inqselicount = 0 ;
    else
      inqcharlen   = 0 ;
   } //End of Attribute setup
 
//
// Initialize the REXX output attribute
//
 if (rc == 0)
   {
    if ( (attrib >= MQCA_FIRST  ) && (attrib <= MQCA_LAST  ) )
      rexxrc = stem_from_string(traceid, NULL, RX_value, "", "", 0) ;
    else
      rexxrc = stem_from_long(traceid, NULL, RX_value, "", 0) ;

    if ( (rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV) )
      {
       TRACE(traceid, ("RexxVariablePool failed to initialize output attribute rc = %d\n",
                       rexxrc) ) ;
       rc = -12 ;
      }
   }
 
//
// Now do the Inquiry and return the setting
//
 if (rc == 0)
   {
    TRACE(traceid, ("Attr = %"PRId32", IntSelNum = %"PRId32", CharAttrLen = %"PRId32"\n",
                    (int32_t)attrib,(int32_t)inqselicount,(int32_t)inqcharlen) );
    MQINQ ( anchor->QMh , anchor->Qh[handle],
            inqselcount , &attrib  ,
            inqselicount, &inqints ,
            inqcharlen  , inqchars ,
            &mqrc, &mqac ) ;
    rc   = mqrc ;
    TRACE(traceid, ("INQ rc = %"PRId32", ac = %"PRId32", Intval = %"PRId32", Charval = %s\n",
          (int32_t)mqrc,(int32_t)mqac,(int32_t)inqints,inqchars) ) ;
 
    if ( (attrib >= MQCA_FIRST  ) && (attrib <= MQCA_LAST) )
      rexxrc = stem_from_string(traceid, NULL, RX_value, "", inqchars, inqcharlen) ;
    else
      rexxrc = stem_from_long  (traceid, NULL, RX_value, "", inqints) ;

    if ( (rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV) )
      {
       TRACE(traceid, ("RexxVariablePool failed to publish output attribute rc = %d\n",
                       rexxrc) ) ;
       if (rc == 0) rc = -12 ;
      }
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQINQ function
 
//
// Do a set      MQSET
//
//   Call:   rc = RXMQset(handle, attribute, atttrsetting )
//
//           NB: only 1 attribute at a time!
//
//
FTYPE  RXMQSET  RXMQPARM
 {
 
 RXMQCB         * anchor = 0           ;  // RXMQ Control Block
 MQLONG           rc = 0               ;  // Function Return Code
 MQLONG           mqrc = 0             ;  // MQ RC
 MQLONG           mqac = 0             ;  // MQ AC
 MQULONG          traceid = SET        ;  // This function trace id
 
 RXSTRING         RX_handle            ;  // Data     Obj Handle
 RXSTRING         RX_attr              ;  // Variable Input  Attr
 RXSTRING         RX_value             ;  // Variable Set    Attr
 
 MQLONG           handle       = 0     ;  //MQ object number
 MQLONG           attrib       = 0     ;  // object attribute
 
 MQLONG           setselcount  = 1     ;  //SET - number of sels
 MQLONG           setselicount = 1     ;  //SET - number of int sels
 MQLONG           setints      = 0     ;  //SET - integer return
 MQLONG           setcharlen   = 600   ;  //SET - char return length
 char             setchars[601]        ;  //SET - char return
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null handle"},
        {  -3, "Zero data handle"},
        {  -4, "Null data attribute"},
        {  -5, "Zero data attribute"},
        {  -6, "Null setting"},
        {  -7, "Zero length setting"},
        {  -8, "No attribute supplied"},
        {  -9, "Attribute out of valid range"},
        { -10, "Handle out of range"},
        { -11, "Invalid handle"},
        { -12, "Character attribute value too long"},
        { -13, "Invalid integer attribute value"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 
 if ( (rc == 0) && (aargc != 3 ) )             rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc =  -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc =  -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc =  -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )    rc =  -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) ) rc =  -7 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(&RX_handle,&aargv[0],sizeof(RX_handle)) ;
    memcpy(&RX_attr,  &aargv[1],sizeof(RX_attr))   ;
    memcpy(&RX_value, &aargv[2],sizeof(RX_value))  ;
 
    TRACE(traceid, ("RX_handle = %.*s\n",(int)RX_handle.strlength,RX_handle.strptr) ) ;
    TRACE(traceid, ("RX_attr = %.*s\n",  (int)RX_attr.strlength,  RX_attr.strptr)   ) ;
    TRACE(traceid, ("RX_value = %.*s\n", (int)RX_value.strlength, RX_value.strptr)  ) ;
 
    if ( parm_to_ulong(RX_handle, &handle) != 0 ) rc = -10 ;
    if ( (rc == 0) && (parm_to_ulong(RX_attr, &attrib) != 0) ) rc = -9 ;
 
    if ( (rc == 0) && (attrib == 0) ) rc = -8 ;
 
    if (    (rc == 0)
         && !( ( (attrib >= MQIA_FIRST) && (attrib <= MQIA_LAST  ) )
            || ( (attrib >= MQCA_FIRST) && (attrib <= MQCA_LAST  ) ) ) ) rc = -9;
   }
 
//
// See if the handle is valid (ie: the gmqo to use)
//
 if ( (rc == 0) && ( ( handle > MAXQS ) || ( handle <= 0 ) ) ) rc = -10 ;
 if ( (rc == 0) && ( anchor->Qh[handle] == 0 ) )               rc = -11 ;
 if (    (rc == 0)
      && (attrib >= MQCA_FIRST)
      && (attrib <= MQCA_LAST)
      && (RX_value.strlength > (sizeof(setchars) - 1)) )        rc = -12 ;
 
//
//
// Format up either a character or an integer area
//
 if (rc == 0)
   {
    memset(setchars,' ',sizeof(setchars))                     ;
    if ( (attrib >= MQCA_FIRST  ) && (attrib <= MQCA_LAST  ) )
      {
       setselicount = 0                                       ;
       memcpy(setchars,RX_value.strptr,RX_value.strlength)    ;
 
      }      //End of Char Attr processing
    else      //Int attr processing
      {
       setcharlen   = 0                                       ;
       if ( parm_to_ulong(RX_value, &setints) != 0 ) rc = -13 ;
      }      //End of Int Attr processing
 
   } //End of Attribute building
 
//
// Now do the Setting
//
 if (rc == 0)
   {
    TRACE(traceid, ("Attr = %"PRId32", IntSelNum = %"PRId32", IntSelVal = %"PRId32", CharAtrLen = %"PRId32", CharSetVal = %.*s\n",
          (int32_t)attrib,(int32_t)setselicount,(int32_t)setints,(int32_t)setcharlen,(int)setcharlen,setchars) ) ;
    MQSET ( anchor->QMh , anchor->Qh[handle],
            setselcount , &attrib   ,
            setselicount, &setints  ,
            setcharlen  , setchars  ,
            &mqrc, &mqac ) ;
    rc   = mqrc ;
    TRACE(traceid, ("MQSET rc = %"PRId32", ac = %"PRId32"\n",(int32_t)mqrc,(int32_t)mqac) ) ;
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQSET function
 
//
// Do a Subscribe MQSUB
//
//   Call:   rc = RXMQsub(isdesc,handle,osdesc)
//
//
//           Optional:
//                   rc = RXMQsub(isdesc,handle,osdesc,subhandle)
//
//
FTYPE  RXMQSUB  RXMQPARM
 {
 
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQULONG                 i                ;  // Looper
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQLONG                  closeobjcc = 0   ;
 MQLONG                  closeobjrc = 0   ;
 MQLONG                  closesubcc = 0   ;
 MQLONG                  closesubrc = 0   ;
 MQULONG                 traceid = SUB    ;  // This function trace id
 int                     rexxrc = RXSHV_OK ;
 int                     subrexxrc = RXSHV_OK ;
 int                     rexxrcSd = RXSHV_OK ;
 int                     resetrc = RXSHV_OK ;
 int                     sdrc = 0           ;
 int                     sdBuilt = 0        ;
 
 RXSTRING                RX_handle        ;  //      Var Obj Handle
 RXSTRING                RXi_sd           ;  // Stem Var Sub Desc Input
 RXSTRING                RXo_sd           ;  // Stem Var Sub Desc Output
 RXSTRING                RX_subhandle     ;  // Optional Var Sub Handle
 
 MQSD                    sd               ;  // MQ subscription desc
 int                     theobj     = -1  ;  // gmqo object to use
 int                     thesub     = -1  ;  // subscription object to use
 MQHOBJ                  sh         = 0   ;  // Subscription handle
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null input SD"},
        {  -3, "Zero length input SD"},
        {  -6, "Null handle name"},
        {  -7, "Zero length handle name"},
        {  -8, "Null Output SD"},
        {  -9, "Zero length Output SD"},
        { -10, "No available objects"},
        { -11, "Unable to publish output handle to REXX"},
        { -12, "Unable to publish output SD to REXX"},
        { -13, "Unable to build MQCHARV input"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
  if ( (rc == 0) && ( (aargc < 3) || (aargc > 4) ) ) rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc =  -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc =  -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc =  -7 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )    rc =  -8 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) ) rc =  -9 ;
 if ( (rc == 0) && (aargc == 4) && RXNULLSTRING(aargv[3]) )    rc =  -6 ;
 if ( (rc == 0) && (aargc == 4) && RXZEROLENSTRING(aargv[3]) ) rc =  -7 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// Now the parms are correct, get them
//
 if (rc == 0)
    {
      memcpy(&RXi_sd ,   &aargv[0],sizeof(RXi_sd))    ;
      memcpy(&RX_handle ,&aargv[1],sizeof(RX_handle)) ;
      memcpy(&RXo_sd ,   &aargv[2],sizeof(RXo_sd))    ;
       if (aargc == 4)
         memcpy(&RX_subhandle,&aargv[3],sizeof(RX_subhandle)) ;
 
      TRACE(traceid, ("RXi_sd = %.*s\n",   (int)RXi_sd.strlength,   RXi_sd.strptr)    )  ;
      TRACE(traceid, ("RX_handle = %.*s\n",(int)RX_handle.strlength,RX_handle.strptr) )  ;
      TRACE(traceid, ("RXo_sd = %.*s\n",   (int)RXo_sd.strlength,   RXo_sd.strptr)    )  ;
      if (aargc == 4)
        TRACE(traceid, ("RX_subhandle = %.*s\n",
                        (int)RX_subhandle.strlength,RX_subhandle.strptr) ) ;
 
      sdrc = make_sd_from_stem(traceid,&sd,RXi_sd)    ;
      sdBuilt = 1                                     ;
      if ( sdrc == -1 ) rc = -13                      ;
    }
 
//
// Initialize the REXX output handles
//
 if (rc == 0)
   {
    rexxrc = stem_from_long(traceid, NULL, RX_handle, "", 0) ;
    if ( (rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV) )
      {
       TRACE(traceid, ("RexxVariablePool failed to initialize RX_handle rc = %d\n",
                       rexxrc) ) ;
       rc = -11 ;
      }
   }

 if ( (rc == 0) && (aargc == 4) )
   {
    subrexxrc = stem_from_long(traceid, NULL, RX_subhandle, "", 0) ;
    if ( (subrexxrc != RXSHV_OK) && (subrexxrc != RXSHV_NEWV) )
      {
       TRACE(traceid, ("RexxVariablePool failed to initialize RX_subhandle rc = %d\n",
                       subrexxrc) ) ;
       rc = -11 ;
      }
   }
 
//
// Select the handle
//
//
// Select the handle
//
 if (rc == 0)
   {
    for ( i=MINQS ; i <= MAXQS ; i++ )
      {
       if ( anchor->Qh[i]  == 0 )
         {
          if (theobj == -1)
            {
             theobj = i ;
             TRACE(traceid, ("Selected Object %d]\n",theobj) ) ;
             if (aargc == 3) break ;
            }
          else
            {
             thesub = i ;
             TRACE(traceid, ("Selected Subscription Object %d]\n",thesub) ) ;
             break ;
            }
         }
      }
    if (theobj == -1) rc = -10 ;
    if ((rc == 0) && (aargc == 4) && (thesub == -1)) rc = -10 ;
   }
//
 
//
// Do the MQSUB on the obtained Subscription object
//
// Do the MQSUB on the obtained Subscription object
//
 if (rc == 0)
   {
    TRACE(traceid, ("Calling MQSUB\n") )                               ;
    MQSUB ( anchor->QMh, &sd, &anchor->Qh[theobj], &sh, &mqrc, &mqac ) ;
    rc   = mqrc                                                        ;
    if ( anchor->Qh[theobj] != 0 )   //If the Subscribe worked,
      {                              //then .
       rexxrc = stem_from_long(traceid, NULL, RX_handle, "", theobj) ;
       if ( (rexxrc == RXSHV_OK) || (rexxrc == RXSHV_NEWV) )
         {
       rexxrcSd = make_stem_from_sd(traceid,&sd,RXo_sd)       ; //and update the SD
       if ( (rexxrcSd != RXSHV_OK) && (rexxrcSd != RXSHV_NEWV) )
         {
          TRACE(traceid, ("RexxVariablePool failed to publish RXo_sd rc = %d\n",
                          rexxrcSd) ) ;
          if (rc == 0) rc = -12 ;
         }
       if (aargc == 4)
         {
          anchor->Qh[thesub] = sh ;
          subrexxrc =
            stem_from_long(traceid, NULL, RX_subhandle, "", thesub) ;
         }
         }

       if ( (rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV) )
         TRACE(traceid, ("RexxVariablePool failed to publish RX_handle rc = %d\n",
                         rexxrc) ) ;
       if (   (aargc == 4)
           && (subrexxrc != RXSHV_OK) && (subrexxrc != RXSHV_NEWV) )
         TRACE(traceid, ("RexxVariablePool failed to publish RX_subhandle rc = %d\n",
                         subrexxrc) ) ;

       if (   ((rexxrc != RXSHV_OK) && (rexxrc != RXSHV_NEWV))
           || (   (aargc == 4)
               && (subrexxrc != RXSHV_OK) && (subrexxrc != RXSHV_NEWV)) )
         {
          if (sh != 0)
            {
             TRACE(traceid, ("Calling MQCLOSE to clean up subscription handle\n") ) ;
             if (   (aargc == 4) && (thesub != -1)
                 && (anchor->Qh[thesub] == sh) )
               MQCLOSE ( anchor->QMh, &anchor->Qh[thesub], MQCO_NONE,
                         &closesubcc, &closesubrc ) ;
             else
               MQCLOSE ( anchor->QMh, &sh, MQCO_NONE,
                         &closesubcc, &closesubrc ) ;
             TRACE(traceid, ("MQCLOSE subscription cleanup cc = %"PRId32", reason = %"PRId32"\n",
                             (int32_t)closesubcc,(int32_t)closesubrc) ) ;
            }

          if (anchor->Qh[theobj] != 0)
            {
             TRACE(traceid, ("Calling MQCLOSE to clean up object handle\n") ) ;
             MQCLOSE ( anchor->QMh, &anchor->Qh[theobj], MQCO_NONE,
                       &closeobjcc, &closeobjrc ) ;
             TRACE(traceid, ("MQCLOSE object cleanup cc = %"PRId32", reason = %"PRId32"\n",
                             (int32_t)closeobjcc,(int32_t)closeobjrc) ) ;
            }

          resetrc = stem_from_long(traceid, NULL, RX_handle, "", 0) ;
          if ( (resetrc != RXSHV_OK) && (resetrc != RXSHV_NEWV) )
            TRACE(traceid, ("Failed to reset RX_handle rc = %d\n",resetrc) ) ;
          if (aargc == 4)
            {
             resetrc = stem_from_long(traceid, NULL, RX_subhandle, "", 0) ;
             if ( (resetrc != RXSHV_OK) && (resetrc != RXSHV_NEWV) )
               TRACE(traceid, ("Failed to reset RX_subhandle rc = %d\n",resetrc) ) ;
            }
          rc = -11 ;
         }
      }
   }
//
// Set the LAST variables, and the function return string
//
 if (sdBuilt)
   free_sd_mqcharv(&sd) ;

 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQSUB function
 
//
// Extension functions
//
 
//
// Do a Browse
//
//   Call:   rc = RXMQbrws(handle, data)
//
//           This function issues a browse on the Queue, and
//                returns the next message (if available). If you
//                need the Message descriptor as well, then use the
//                native Get interface.
//
//
FTYPE  RXMQBRWS  RXMQPARM
 {
 
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = BRO    ;  // This function trace id
 int                     rexxrc0 = RXSHV_OK ;
 int                     rexxrc1 = RXSHV_OK ;
 
 RXSTRING                RX_handle        ;  // Data     Obj Handle
 RXSTRING                RX_data          ;  // Variable Data
 
 MQMD2                   md               ;  // Browse MD
 MQGMO                   gmo              ;  // Browase GMO
 
 MQLONG                  handle    = 0    ;  //MQ object number
 void                 *  data      = 0    ;  //-> Data buffer
 MQLONG                  data0     = 0    ;  //   Data length max
 MQLONG                  datalen   = 0    ;  //   Data length actual
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null handle"},
        {  -3, "Zero data handle"},
        {  -4, "Null data stem var"},
        {  -5, "Zero data stem var"},
        {  -6, "Handle out of range"},
        {  -7, "Invalid handle"},
        {  -8, "malloc failure, check reason code"},
        {  -9, "Zero length input data buffer"},
        { -10, "Unable to publish output data to REXX"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 2 ) )             rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc =  -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc =  -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc =  -5 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// Now the parms are correct, get them
//
 if (rc == 0)
   {
    memcpy(&RX_handle,&aargv[0],sizeof(RX_handle)) ;
    memcpy(&RX_data,  &aargv[1],sizeof(RX_data))   ;
 
    TRACE(traceid, ("RX_handle = %.*s\n",(int)RX_handle.strlength,RX_handle.strptr) ) ;
    TRACE(traceid, ("RX_data   = %.*s\n",(int)RX_data.strlength,  RX_data.strptr)   ) ;
 
    if ( parm_to_ulong(RX_handle, &handle) != 0 ) rc = -6 ;
    stem_to_long(traceid, RX_data, "0" , &data0) ;
   }
 
//
// Now check the input Stem variable to see that there is
//     some valid data to obtain
//
 if ( (rc == 0) && ( data0 <= 0 ) ) rc = -9 ;
 
//
// See if the handle is valid
//
 if ( (rc == 0) && ( ( handle > MAXQS ) || ( handle <= 0 ) ) ) rc = -6 ;
 if ( (rc == 0) && ( anchor->Qh[handle] == 0 ) )               rc = -7 ;

 if (rc == 0)
   {
    rexxrc0 = stem_from_long(traceid,
                             NULL,
                             RX_data,
                             "0",
                             0) ;

    rexxrc1 = stem_from_bytes(traceid,
                              NULL,
                              RX_data,
                              "1",
                              (MQBYTE *)"",
                              0) ;

    if ( (rexxrc0 != RXSHV_OK) && (rexxrc0 != RXSHV_NEWV) )
      TRACE(traceid,
            ("RexxVariablePool failed to initialize RX_data.0 rc = %d\n",
             rexxrc0) ) ;

    if ( (rexxrc1 != RXSHV_OK) && (rexxrc1 != RXSHV_NEWV) )
      TRACE(traceid,
            ("RexxVariablePool failed to initialize RX_data.1 rc = %d\n",
             rexxrc1) ) ;

    if (   ((rexxrc0 != RXSHV_OK) && (rexxrc0 != RXSHV_NEWV))
        || ((rexxrc1 != RXSHV_OK) && (rexxrc1 != RXSHV_NEWV)) )
      rc = -10 ;
   }

//
// Now GETMAIN the buffer to receive the data records
//
 if (( rc == 0) && ( data0 != 0 ) )
   {
    TRACE(traceid, ("Doing malloc for %"PRId32" bytes\n",(int32_t)data0) ) ;
    data = malloc(data0)                                                   ;
    if ( data == NULL )
      {
       mqac = errno                                                        ;
       TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) )          ;
       rc = -8                                                             ;
      }
   }
 
//
// Now get the data from the queue
//
 if (rc == 0)
   {
    TRACE(traceid, ("BROWSE Maxdatalen = %"PRId32"\n",(int32_t)data0) )       ;
    memcpy(&md , &md_default , sizeof(MQMD2))                  ;
    memcpy(&gmo, &gmo_default, sizeof(MQGMO))                  ;
    gmo.Options        = MQGMO_NO_WAIT+MQGMO_BROWSE_NEXT
                       + MQGMO_ACCEPT_TRUNCATED_MSG
                       + MQGMO_FAIL_IF_QUIESCING               ;
    gmo.WaitInterval   = MQWI_UNLIMITED                        ;
    MQGET ( anchor->QMh, anchor->Qh[handle], &md, &gmo, data0, data, &datalen, &mqrc, &mqac ) ;
    rc   = mqrc ;
    TRACE(traceid, ("MQGET rc = %"PRId32", ac = %"PRId32", Datalen = %"PRId32"\n",
                    (int32_t)mqrc,(int32_t)mqac,(int32_t)datalen) ) ;
 
    rexxrc0 =
      stem_from_long(traceid,
                     NULL,
                     RX_data,
                     "0",
                     datalen) ;
    if (datalen > data0) datalen = data0                                    ;
    rexxrc1 =
      stem_from_bytes(traceid,
                      NULL,
                      RX_data,
                      "1",
                      (MQBYTE *)data,
                      datalen) ;

    if ( (rexxrc0 != RXSHV_OK) && (rexxrc0 != RXSHV_NEWV) )
      TRACE(traceid,
            ("RexxVariablePool failed to publish RX_data.0 rc = %d\n",
             rexxrc0) ) ;

    if ( (rexxrc1 != RXSHV_OK) && (rexxrc1 != RXSHV_NEWV) )
      TRACE(traceid,
            ("RexxVariablePool failed to publish RX_data.1 rc = %d\n",
             rexxrc1) ) ;

    if (   ((rexxrc0 != RXSHV_OK) && (rexxrc0 != RXSHV_NEWV))
        || ((rexxrc1 != RXSHV_OK) && (rexxrc1 != RXSHV_NEWV)) )
      {
       if (rc == 0)
         rc = -10 ;
      }
   }
 
//
// Free data buffer for stem.1 variable data, if allocated.
//
  if ( data != 0 )
    {
     TRACE(traceid, ("Free area\n") ) ;
     free(data) ;
    }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQBRWS function
 
//
// Do a Header extract  MQHXT
//
//   Call:   rc = RXMQhxt( input_Stem, output_stem )
//
//
//
//   This function takes an input Stem variable, and builds an
//        output stem variable, extracting the Headers from the
//        'message' data.
//
//        The actual message & its length are placed in .1 and .0 , with
//            all the other info being expanded into the .item variables.
//            In addition, for compatability with the EVENT splitting up
//            capability .NAME and .TYPE are also set (both to either
//            DLH or XQH). .ZLIST is also set to contain the exploded
//            components (including TYPE NAME 0 1).
//
//
//
//   The headers processed are:
//
//       XQH - Transmission Header
//
//             .0      -> Data Length
//             .1      -> Message Data
//
//             .RQM    -> Remote Queue Manager Name
//             .RQN    -> Remote Queue Name
//
//             .TYPE   -> XQH     for compatability with
//             .NAME   -> XQH    Event splitting up
//
//             + all the MD options
//
//             .ZLIST  -> 0 1 AID AOD AT BC CID CCSI ENC EXP FBK
//                        FORM MSG MSGID NAME PAN PAT PD PER PRI
//                        PT REP RQM RQN RTOQ RTOQM TYPE UID
//
//
//       DLH - Dead Letter Header
//
//             .0      -> Data Length
//             .1      -> Message Data
//
//             .TYPE   -> DLH     for compatability with
//             .NAME   -> DLH     Event splitting up
//
//             .REA    -> Reason
//             .DQM    -> Destination Queue Manager Name
//             .DQN    -> Destination Queue Name
//             .ENC    -> Encoding
//             .CCSI   -> Coded Character Set ID
//             .FORM   -> Format
//             .PAT    -> Put Appl Type
//             .PAN    -> Put Appl Name
//             .PD     -> Put Date
//             .PT     -> Put Time
//
//             .ZLIST  -> 0 1 CCSI DQM DQN ENC FORM NAME PAN
//                        PAT PD PT REA TYPE
//
//
FTYPE  RXMQHXT  RXMQPARM
 {
 
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = HXT    ;  // This function trace id
 int                     rexxrc = RXSHV_OK ;
 int                     rexxrcOutput = RXSHV_OK ;
 
 RXSTRING                RX_input         ;  // Variable Data - Input
 RXSTRING                RX_output        ;  // Variable Data - Output
 
 MQBYTE               *  data  = 0        ;  //-> Data buffer
 MQLONG                  data0   = 0      ;  // Input Data - len
 MQULONG                 datalen          ;  //   Data length
 MQLONG                  out0             ;  // Output Data - len
 RXMQ_EXACT_FETCH_RESULT   fetchResult = RXMQ_EXACT_FETCH_INVALID ;
 
 MQXQH                 * thexqh           ;  // -> XQH
 MQDLH                 * thedlh           ;  // -> DLH
 
 char                    zlist[200]       ;  // Char version of .ZLIST
 
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null input stem var"},
        {  -3, "Zero input stem var"},
        {  -4, "Null output stem var"},
        {  -5, "Zero output stem var"},
        {  -6, "No input data"},
        {  -7, "Unable to publish output to REXX"},
        {  -8, "Cannot verify Header"},
        { -10, "Unknown Header"},
        { -11, "Too short for a DLH"},
        { -12, "Too short for a XQH"},
        { -13, "malloc failure, check reason code"},
        { -14, "Data length is not equal to specified value"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
 sprintf((char *)zlist,"0 1") ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 2 ) )             rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc = -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc = -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc = -5 ;
 
//
// The input parms are OK, so obtain them
//
 if (rc == 0)
   {
    memcpy(&RX_input, &aargv[0],sizeof(RX_input))    ;
    memcpy(&RX_output,&aargv[1],sizeof(RX_output))   ;
    stem_to_long  (traceid, RX_input, "0" , &data0)  ;
   }
 
//
// Now check the input Stem variable to see that there is
//     some valid data to obtain
//
 if ( (rc == 0) && ( data0 <= 0 ) ) rc = -6  ; // No input data
 
//
// Allocate data buffer to store stem.1 variable data
//
 if ( rc == 0 )
   {
    fetchResult = fetch_exact_rexx_bytes(traceid, RX_input, "1",
                                         data0, &data) ;
    if ( fetchResult == RXMQ_EXACT_FETCH_NOMEM )
      {
       mqac = errno                                               ;
       TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) ) ;
       rc = -13                                                   ;
      }
    else if ( fetchResult != RXMQ_EXACT_FETCH_SUCCESS )
      rc = -14 ;
    else
      {
       datalen = (MQULONG)data0 ;
       TRACE(traceid, ("Length of data received = %"PRIu32"\n",(uint32_t)datalen) ) ;
      }
   }
 
//
// Now check if stem.0 specifies the same value as stem.1 length.
// If not, don't know what to do.
//
 if ((rc == 0) && ( datalen != (MQULONG) data0 ) ) rc = -14 ;
 
//
// Now check the input Stem variable to see that there is
//     some valid data to obtain
//
 if (    (rc == 0)
      && (datalen < sizeof(MQCHAR4)) ) rc = -8 ; // Cannot verify header
 
//
// There is the possibility of a Header, so see if it is a known
//       one, and ignore it, if it is not one to process.
//
 if ( rc == 0 )
   {
    if (    ( memcmp(data, MQDLH_STRUC_ID, sizeof(MQCHAR4)) != 0 )
         && ( memcmp(data, MQXQH_STRUC_ID, sizeof(MQCHAR4)) != 0 ) ) rc = -10   ;
   }
 
//
// Lets's try DLH
//
 if ( ( rc == 0 ) && ( memcmp(data, MQDLH_STRUC_ID, sizeof(MQCHAR4)) == 0 ) )
   {
    if ( datalen < sizeof(MQDLH) ) rc = -11 ; // Too short for DLH
    else
      {
       TRACE(traceid, ("Unravelling a DLH\n")  ) ;
       thedlh = (MQDLH *)data                    ; // Set Header pointer
       out0 = data0 - sizeof(MQDLH)              ; // Calc actual Data length
 
       rexxrc =
         stem_from_long  (traceid, NULL, RX_output, "0" , out0)                                 ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_bytes (traceid, NULL, RX_output, "1" , (MQBYTE *)data + sizeof(MQDLH), out0) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
 
       TRACE(traceid, (" Msg  length = %"PRId32", DLH length = %"PRId32", Datalen = %"PRId32"\n",
             (int32_t)data0,(int32_t)sizeof(MQDLH),(int32_t)out0) )                 ;
       TRACE(traceid, ("Data ptr = %p, DLH ptr = %p, Start of data ",data,thedlh) ) ;
       TRACX(traceid, ((MQBYTE *)data + sizeof(MQDLH),out0) )                       ;
       TRACE(traceid, ("\n") )                                                      ;
 
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "TYPE" , "DLH",                   3               ) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "NAME" , "DLH",                   3               ) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "REA"  , thedlh->Reason)                            ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "DQM"  , thedlh->DestQMgrName,    sizeof(MQCHAR48)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "DQN"  , thedlh->DestQName,       sizeof(MQCHAR48)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "ENC"  , thedlh->Encoding)                          ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "CCSI" , thedlh->CodedCharSetId)                    ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "FORM" , thedlh->Format,          sizeof(MQCHAR8) ) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "PAT"  , thedlh->PutApplType)                       ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "PAN"  , thedlh->PutApplName,     sizeof(MQCHAR28)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "PD"   , thedlh->PutDate,         sizeof(MQCHAR8) ) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "PT"   , thedlh->PutTime,         sizeof(MQCHAR8) ) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "ZLIST", zlist, strlen(zlist))                      ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;

       if (    (rc == 0)
            && (rexxrcOutput != RXSHV_OK)
            && (rexxrcOutput != RXSHV_NEWV) )
         {
          TRACE(traceid,
                ("RexxVariablePool failed to publish RXMQHXT DLH output rc = %d\n",
                 rexxrcOutput) ) ;
          rc = -7 ;
         }
 
       TRACE(traceid, ("Unravelled the DLH\n") ) ;
      }
   }
 
 //
 // Lets's try XQH
 //
 if ( ( rc == 0 ) && ( memcmp(data, MQXQH_STRUC_ID, sizeof(MQCHAR4)) == 0 ) )
   {
    if ( datalen < sizeof(MQXQH) ) rc = -12 ; // Too short for XQH
    else
      {
       TRACE(traceid, ("Unravelling a XQH\n") ) ;
       thexqh = (MQXQH *)data                   ; // Set Header pointer
       out0 = data0 - sizeof(MQXQH)             ; // Calc actual Data length
 
       rexxrc =
         stem_from_long  (traceid, NULL, RX_output, "0" , out0)                               ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_bytes (traceid, NULL, RX_output, "1" , (MQBYTE *)data + sizeof(MQXQH), out0) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
 
       TRACE(traceid, (" Msg  length = %"PRId32", XQH length = %"PRId32", Datalen = %"PRId32"\n",
             (int32_t)data0,(int32_t)sizeof(MQXQH),(int32_t)out0) )                 ;
       TRACE(traceid, ("Data ptr = %p, XQH ptr = %p, Start of data ",data,thexqh) ) ;
       TRACX(traceid, ((MQBYTE *)data + sizeof(MQXQH),out0) )                       ;
       TRACE(traceid, ("\n") )                                                      ;
 
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "TYPE" , "XQH",                   3               ) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "NAME" , "XQH",                   3               ) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "RQN"  , thexqh->RemoteQName,     sizeof(MQCHAR48)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "RQM"  , thexqh->RemoteQMgrName,  sizeof(MQCHAR48)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
 
       // Only Version 1 of MQMD is created here
       TRACE(traceid, ("Generating the XQH.MD\n") )                                                    ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "VER"  , thexqh->MsgDesc.Version)                               ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "REP"  , thexqh->MsgDesc.Report)                                ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "MSG"  , thexqh->MsgDesc.MsgType)                               ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "EXP"  , thexqh->MsgDesc.Expiry)                                ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "FBK"  , thexqh->MsgDesc.Feedback)                              ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "ENC"  , thexqh->MsgDesc.Encoding)                              ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "CCSI" , thexqh->MsgDesc.CodedCharSetId)                        ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "FORM" , thexqh->MsgDesc.Format,              sizeof(MQCHAR8))  ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "PRI"  , thexqh->MsgDesc.Priority)                              ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "PER"  , thexqh->MsgDesc.Persistence)                           ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_bytes (traceid, zlist, RX_output, "MSGID", thexqh->MsgDesc.MsgId,               sizeof(MQBYTE24)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_bytes (traceid, zlist, RX_output, "CID"  , thexqh->MsgDesc.CorrelId,            sizeof(MQBYTE24)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "BC"   , thexqh->MsgDesc.BackoutCount)                          ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "RTOQ" , thexqh->MsgDesc.ReplyToQ,            sizeof(MQCHAR48)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "RTOQM", thexqh->MsgDesc.ReplyToQMgr,         sizeof(MQCHAR48)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "UID"  , thexqh->MsgDesc.UserIdentifier,      sizeof(MQCHAR12)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_bytes (traceid, zlist, RX_output, "AT"   , thexqh->MsgDesc.AccountingToken,     sizeof(MQBYTE32)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "AID"  , thexqh->MsgDesc.ApplIdentityData,    sizeof(MQCHAR32)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_long  (traceid, zlist, RX_output, "PAT"  , thexqh->MsgDesc.PutApplType)                           ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "PAN"  , thexqh->MsgDesc.PutApplName,         sizeof(MQCHAR28)) ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "PD"   , thexqh->MsgDesc.PutDate,             sizeof(MQCHAR8))  ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "PT"   , thexqh->MsgDesc.PutTime,             sizeof(MQCHAR8))  ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "AOD"  , thexqh->MsgDesc.ApplOriginData,      sizeof(MQCHAR4))  ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       TRACE(traceid, ("End of XQH.MD generation\n") )                                 ;
       rexxrc =
         stem_from_string(traceid, zlist, RX_output, "ZLIST", zlist, strlen(zlist))      ;
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
       if (    (rc == 0)
            && (rexxrcOutput != RXSHV_OK)
            && (rexxrcOutput != RXSHV_NEWV) )
         {
          TRACE(traceid,
                ("RexxVariablePool failed to publish RXMQHXT XQH output rc = %d\n",
                 rexxrcOutput) ) ;
          rc = -7 ;
         }
       TRACE(traceid, ("Unravelled the XQH\n") )                                       ;
      }
   }
 
//
// Free data buffer for stem.1 variable data, if allocated.
//
 if ( data != 0 )
   {
    TRACE(traceid, ("Free area\n") ) ;
    free(data) ;
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQHXT function
 
//
// Do an Event extract  RXMQEVNT
//
//   Call:   rc = RXMQevnt( input_Stem, output_stem )
//
//
//
//   This function takes an input Stem variable, and builds an
//        output stem variable, extracting the Event Data from the
//        'message' data.
//
//        Stem.TYPE variable is set to 'EVENT'
//        Stem.REA variable is set to the event numerical value
//        Stem.NAME variable is set to the name of the event
//        corresponding to the numerical value expressed as a
//        MQRC_... constant name with MQRC_ prefix stripped.
//        All other info is expanded into Stem.item variables
//        where item is parameter attribute name like MQCA_...
//        with MQCA_prefix stripped.
//
//        Additionally (.)ZLIST is set to contain a list of all the
//        components (without the leading dot).
//
//     The events are splitup into the normal stem.component name, but
//                if the component is not a SINGLE string or Integer (as
//                defined by the IN & ST PCF types for the given field), then
//                the explosion is stem.component.0 for the number of items
//                                 stem.component.n for the individual items
//
//     The naming of the fields within the event is controlled by the
//                geteventname routine, which converts a known set of
//                fields (as defined by the Event part of the PCF book)
//                into character component names. Note that wherever the
//                field occurs generates its expansion - no matter whether
//                or not the PCF book say it should or should not occur!
//
FTYPE  RXMQEVNT  RXMQPARM
 {
 
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 int                     i                ;  // Looper for parms
 int                     j                ;  // Looper for list
 int                     g                ;  // Looper for group
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = EVENT  ;  // This function trace id
 int                     rexxrc = RXSHV_OK ;
 int                     rexxrcOutput = RXSHV_OK ;
 
 RXSTRING                RX_input         ;  // Variable Data - Input
 RXSTRING                RX_output        ;  // Variable Data - Output
 
 MQBYTE               *  data  = 0        ;  //-> Data buffer
 MQLONG                  data0   = 0      ;  // Input Data length
 MQLONG                  datalen = 0      ;  //   Data length
 MQLONG                  remaining = 0    ;  //   Remaining PCF data length
 RXMQ_EXACT_FETCH_RESULT   fetchResult = RXMQ_EXACT_FETCH_INVALID ;
 
 MQCFH                 * bufpcf           ;  // Pointer to PCF header
 MQCFIN                * bufpcfi          ;  // Pointer to PCF sub-structure
 
 MQLONG                  parms            ;  //Number of elements
 MQLONG                  grpparms         ;  //Number of elements in a group
 MQLONG                  lstparms         ;  //Number of elements in a list
 MQLONG                  parmtype         ;  //Type of element
 MQLONG                  parmsize         ;  //Length of element
 MQLONG                  parmnumb         ;  //Key of element
 char                  * sp               ;  //-> String element
 int                     sl               ;  //String length
 
 char                    comp[62]         ;  //Component name
 char                 *  zvars = 0        ;  //List of components
 char                 *  newzvars = 0     ;  //New list of components
 MQULONG                 zvarlen = 4096U  ;  //Current length of list
 MQULONG                 newzvarlen       ;  //New length of list
 size_t                  zvarused         ;  //Used length of list
 size_t                  zvarname         ;  //Length of component name
 
 RXSTRING                varname          ;  // REXX string of varnamc
 RXSTRING                varvalu          ;  // REXX string of varvalc
 char                    varnamc[100]     ;  // Char version of variable name
 char                    varvalc[100]     ;  // Char version of variable value
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null input stem var"},
        {  -3, "Zero input stem var"},
        {  -4, "Null output stem var"},
        {  -5, "Zero output stem var"},
        {  -6, "No input data"},
        {  -7, "Unable to publish output to REXX"},
        {  -8, "Cannot verify Header"},
        { -10, "Not an Event Header"},
        { -11, "Too short for an Event"},
        { -12, "Unknown Event Category"},
        { -13, "Unknown Event Type"},
        { -14, "No elements in the Event"},
        { -15, "malloc failure, check reason code"},
        { -16, "Data length is not equal to specified value"},
        { -97, "Handle not owned by current thread"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 2 ) )             rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc = -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc = -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc = -5 ;
 
//
// The input parms are OK, so obtain them
//
 if (rc == 0)
   {
    memcpy(&RX_input, &aargv[0],sizeof(RX_input))  ;
    memcpy(&RX_output,&aargv[1],sizeof(RX_output)) ;
    stem_to_long(traceid, RX_input, "0" , &data0)  ;
   }
 
//
// Now check the input Stem variable to see that there is
//     some valid data to obtain
//
 if ( (rc == 0) && ( data0 <= 0 ) ) rc = -6 ;
 
//
// Allocate data buffer to store stem.1 variable data
//
 if ( rc == 0 )
   {
    fetchResult = fetch_exact_rexx_bytes(traceid, RX_input, "1",
                                         data0, &data) ;
    if ( fetchResult == RXMQ_EXACT_FETCH_NOMEM )
      {
       mqac = errno                                               ;
       TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) ) ;
       rc = -15                                                   ;
      }
    else if ( fetchResult != RXMQ_EXACT_FETCH_SUCCESS )
      rc = -16 ;
    else
      {
       datalen = data0 ;
       TRACE(traceid, ("Length of data received = %"PRId32"\n",(int32_t)datalen) ) ;
      }
   }
 
//
// Now check if stem.0 specifies the same value as stem.1 length.
// If not, don't know what to do.
//
 if ((rc == 0) && ( datalen != data0 ) ) rc = -16 ;
 
//
// Now check the input Stem variable to see that there is
//     some valid data to obtain
//
 if ( (rc == 0) && ( datalen <= 3 ) ) rc = -8 ; // Cannot verify header
 
//
// There is the possibility of a Header, so see if it is an event
//       one, and ignore it it is not one to process.
//
 bufpcf    = (MQCFH  *)  data                      ;  //Point to PCF header
 
 if ( ( rc == 0 ) && ( bufpcf->Type != MQCFT_EVENT ) ) rc = -10 ;
 
//
// Although we have a valid header, just check the prefix length
//          to ensure the WHOLE header is present
//
 if ( ( rc == 0 ) && ( datalen < MQCFH_STRUC_LENGTH) ) rc = -11 ;
 if (    (rc == 0)
      && (bufpcf->StrucLength != MQCFH_STRUC_LENGTH) )
   rc = -11 ;
 if ( rc == 0 )
   {
    bufpcfi = (MQCFIN *) (data + MQCFH_STRUC_LENGTH);  //Point to Integer structure
    remaining = datalen - MQCFH_STRUC_LENGTH;
   }
 
//
// Now there is an event Header, check the general category
//
 if ( rc == 0 )
   {
    TRACE(traceid, ("Determining Event general Category\n") ) ;
 
    parms     = bufpcf->ParameterCount ;
 
    switch ( bufpcf->Command )  //See what the event category is
      {
       case MQCMD_Q_MGR_EVENT   : TRACE(traceid, ("It is a QMGR    Event\n") ) ; break ;
       case MQCMD_PERFM_EVENT   : TRACE(traceid, ("It is a PERF    Event\n") ) ; break ;
       case MQCMD_CHANNEL_EVENT : TRACE(traceid, ("It is a CHANNEL Event\n") ) ; break ;
       case MQCMD_CONFIG_EVENT  : TRACE(traceid, ("It is a CONFIG  Event\n") ) ; break ;
       case MQCMD_COMMAND_EVENT : TRACE(traceid, ("It is a COMMAND Event\n") ) ; break ;
       case MQCMD_LOGGER_EVENT  : TRACE(traceid, ("It is a LOGGER  Event\n") ) ; break ;
       default : rc = -12 ; break ;         //Other categories are not known yet
      } // End of Category determination select
   }
 
//
// Now there is a valid event Header, decide which one it is
//     and then create the NAME (and TYPE=EVENT) components
//
 if ( rc == 0 )
   {
    TRACE(traceid, ("Starting to examine the Event\n") ) ;
 
    sprintf(varnamc,"RXMQ.RCMAP.%"PRId32,(int32_t)bufpcf->Reason)  ; // Construct variable name
    MAKERXSTRING(varname,varnamc,strlen(varnamc))                  ; // Construct REXX variable name structure
    varvalc[0] = 0 ;
    stem_to_string(traceid, varname, "", varvalc, sizeof(varvalc)) ; // Get variable value
 
    if ( strlen(varvalc) == 0 )
      rc = -13 ;
    else
      {
       sp = strstr(varvalc, "_") ;
       if ( sp == NULL )
         rc = -13 ;
       else
         {
          varvalu.strptr = sp + 1                         ; // Bypass prefix
          varvalu.strlength = strlen(varvalu.strptr)      ; // New string length

          TRACE(traceid, ("Event %"PRId32" maps to %s %s\n",
                (int32_t)bufpcf->Reason,varvalu.strptr,(varvalu.strlength != 0) ? " " : " which is unknown ") ) ;
         }
      }
   }
 
//
// There is something that looks like a valid event, but just
//       ensure there are some elements in it
//
if ( rc == 0 )
  {
   TRACE(traceid, ("There are %"PRId32" elements in the Event\n",(int32_t)bufpcf->ParameterCount) ) ;
   if ( bufpcf->ParameterCount <= 0 ) rc = -14 ;
  }
 
//
// Now there is a valid event ,
//           allocate zvars buffer,
//           create the TYPE and NAME & REAson components from the Prefix Header.
//           initialize the zvars (to goto .ZLIST) component store.
//
 if ( rc == 0 )
   {
    TRACE(traceid, ("Doing malloc for zvars %"PRId32" bytes\n",(int32_t)zvarlen) ) ;
    zvars = (char *) malloc(zvarlen)                                               ;
    if ( zvars == NULL )
      {
      mqac = errno                                           ;
      TRACE(traceid, ("malloc rc = %"PRId32,(int32_t)mqac) ) ;
      rc = -15                                               ;
      }
   }
 
if ( rc == 0 )
  {
   zvars[0] = '\0'                                                                         ;
   rexxrc =
     stem_from_string(traceid, zvars, RX_output, "TYPE" , "EVENT", strlen("EVENT"))          ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zvars, RX_output, "NAME" , varvalu.strptr, varvalu.strlength) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_long  (traceid, zvars, RX_output, "REA"  , bufpcf->Reason)                    ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
  }
 
//
// Now parse the event and create the relevant components
//
//    Build the ZLIST results into zvars for each parm
//    List entries are setup as stem.component.0 = number of items
//                                            .n = the nth item
// Note. Event parameters potentially may contain the following structures:
//             MQCFST,MQCFIN  - all
//             MQCFBS         - CONFIG_EVENT, COMMAND_EVENT only
//             MQCFGR         - COMMAND_EVENT only
//             MQCFSL         - Q_MGR_EVENT only
//       MQCFGR structure may have only one layer
//       Hook is set to catch unsupported parameters
//
 if ( rc == 0 )                      // Scan and Extract
   {
    TRACE(traceid, ("Now starting to scan the Event\n") ) ;
 
    memset(&comp,0,sizeof(comp));
    for (i=0 ; i < parms ; i++ )
      {
       grpparms = 0 ;                    // Force pseudo group for 1 shot
       comp[0]  = 0 ;                    // Clear component name
 
       for ( g=0 ; g <= grpparms ; g++ )
         {
          if ( remaining < (MQLONG)(3 * sizeof(MQLONG)) )
            {
             rc = -11 ;
             break    ;
            }

          parmtype = bufpcfi->Type        ; //All types share a
          parmsize = bufpcfi->StrucLength ; // common prefix
          parmnumb = bufpcfi->Parameter   ;

          if (    (parmsize < (MQLONG)(3 * sizeof(MQLONG)))
               || (parmsize > remaining)
               || ((parmsize % (MQLONG)sizeof(MQLONG)) != 0) )
            {
             rc = -11 ;
             break    ;
            }
 
          sp = strstr(comp, ".")          ;   // If group component was used,
          if ( sp != NULL ) *(sp + 1) = 0 ;   // limit string to 1st qualifier
          geteventname(comp, parmnumb)    ;   // Translate parm to more readable
 
          switch ( parmtype )               // Select structure type
            {
             case MQCFT_GROUP  :            //Group of attributes
               if ( parmsize != MQCFGR_STRUC_LENGTH )
                 {
                  rc = -11 ;
                  break    ;
                 }
               if ( g != 0 )
                 {
                  rc = -11 ;
                  break    ;
                 }
               strcat      (comp, ".")                          ;
               grpparms = ((MQCFGR *) bufpcfi)->ParameterCount  ;
               if ( grpparms < 0 )
                 {
                  rc = -11 ;
                  break    ;
                 }
               TRACE(traceid, (" Group Parm %"PRId32" into %s. Count = %"PRId32"\n",
                     (int32_t)parmnumb,comp,(int32_t)grpparms) ) ;
               break ;
 
             case MQCFT_INTEGER  :          //Integer type of attribute
               if ( parmsize != MQCFIN_STRUC_LENGTH )
                 {
                  rc = -11 ;
                  break    ;
                 }
               switch ( parmnumb ) //Display in Hex or (mostly) decimal
                 {
                  case MQIACF_AUX_ERROR_DATA_INT_1      :
                  case MQIACF_AUX_ERROR_DATA_INT_2      :
                  case MQIACF_ERROR_IDENTIFIER          :
                  case MQIACH_SSL_RETURN_CODE           :
                    sprintf(varvalc,"%08"PRIX32,(uint32_t)bufpcfi->Value)                                 ; // Display in HEX
                    TRACE(traceid, (" Integer Parm %"PRId32". Value = %"PRId32" <%08"PRIX32">\n",
                          (int32_t)bufpcfi->Parameter,(int32_t)bufpcfi->Value,(uint32_t)bufpcfi->Value) ) ;
                    break ;
 
                  default  :
                    sprintf(varvalc,"%"PRIu32,(uint32_t)bufpcfi->Value)  ; // Display in decimal
                    break ;
                 }
               zvarused = strlen(zvars) ;
               zvarname = strlen(comp)  ;
               if (    ((size_t)zvarlen < 2U)
                    || (zvarname > ((size_t)zvarlen - 2U))
                    || (zvarused > ((size_t)zvarlen - (zvarname + 2U))) )
                 {
                  if ( zvarlen > (((MQULONG)-1) / 2U) )
                    {
                     mqac = 0 ;
                     rc = -15      ;
                     break         ;
                    }
                  newzvarlen = 2U * zvarlen ;
                  if (    ((size_t)newzvarlen < 2U)
                       || (zvarname > ((size_t)newzvarlen - 2U))
                       || (zvarused > ((size_t)newzvarlen - (zvarname + 2U))) )
                    {
                     mqac = 0 ;
                     rc = -15      ;
                     break         ;
                    }
                  TRACE(traceid, ("Doing realloc for zvars %"PRId32" bytes\n",(int32_t)newzvarlen) ) ;
                  newzvars = (char *) realloc(zvars, newzvarlen) ;
                  if ( newzvars == NULL )
                    {
                     mqac = errno ;
                     TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) ) ;
                     rc = -15 ;
                     break    ;
                    }
                  else
                    {
                     zvars = newzvars     ;
                     zvarlen = newzvarlen ;
                    }
                 }
               rexxrc =
                 stem_from_string (traceid, zvars, RX_output, comp ,
                                   varvalc, strlen(varvalc));
               if (    (rexxrc != RXSHV_OK)
                    && (rexxrc != RXSHV_NEWV)
                    && (    (rexxrcOutput == RXSHV_OK)
                         || (rexxrcOutput == RXSHV_NEWV) ) )
                 rexxrcOutput = rexxrc ;
               TRACE(traceid, (" Integer Parm %"PRId32" ->%"PRId32"<- into %s\n",
                               (int32_t)bufpcfi->Parameter,(int32_t)bufpcfi->Value,comp) ) ;
               break ;
 
             case MQCFT_BYTE_STRING :       //Byte-string attribute
               if ( parmsize < MQCFBS_STRUC_LENGTH_FIXED )
                 {
                  rc = -11 ;
                  break    ;
                 }
               sl = ((MQCFBS *)bufpcfi)->StringLength              ;
               if (    (sl < 0)
                    || (sl > (parmsize - MQCFBS_STRUC_LENGTH_FIXED)) )
                 {
                  rc = -11 ;
                  break    ;
                 }
               sp = (char *) &((MQCFBS *)bufpcfi)->String          ;
               zvarused = strlen(zvars) ;
               zvarname = strlen(comp)  ;
               if (    ((size_t)zvarlen < 2U)
                    || (zvarname > ((size_t)zvarlen - 2U))
                    || (zvarused > ((size_t)zvarlen - (zvarname + 2U))) )
                 {
                  if ( zvarlen > (((MQULONG)-1) / 2U) )
                    {
                     mqac = 0 ;
                     rc = -15      ;
                     break         ;
                    }
                  newzvarlen = 2U * zvarlen ;
                  if (    ((size_t)newzvarlen < 2U)
                       || (zvarname > ((size_t)newzvarlen - 2U))
                       || (zvarused > ((size_t)newzvarlen - (zvarname + 2U))) )
                    {
                     mqac = 0 ;
                     rc = -15      ;
                     break         ;
                    }
                  TRACE(traceid, ("Doing realloc for zvars %"PRId32" bytes\n",(int32_t)newzvarlen) ) ;
                  newzvars = (char *) realloc(zvars, newzvarlen) ;
                  if ( newzvars == NULL )
                    {
                     mqac = errno ;
                     TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) ) ;
                     rc = -15 ;
                     break    ;
                    }
                  else
                    {
                     zvars = newzvars     ;
                     zvarlen = newzvarlen ;
                    }
                 }
               rexxrc =
                 stem_from_bytes(traceid, zvars, RX_output, comp, (MQBYTE *)sp, sl) ;
               if (    (rexxrc != RXSHV_OK)
                    && (rexxrc != RXSHV_NEWV)
                    && (    (rexxrcOutput == RXSHV_OK)
                         || (rexxrcOutput == RXSHV_NEWV) ) )
                 rexxrcOutput = rexxrc ;
               TRACE(traceid, (" Byte String %"PRId32" ->",(int32_t)parmnumb) )   ;
               TRACX(traceid, ((MQBYTE *)sp,sl) )                  ;
               TRACE(traceid, ("<- into %s\n",comp) )              ;
               break                                               ;
 
             case MQCFT_STRING :            //String attribute
               if ( parmsize < MQCFST_STRUC_LENGTH_FIXED )
                 {
                  rc = -11 ;
                  break    ;
                 }
               sl = ((MQCFST *)bufpcfi)->StringLength              ;
               if (    (sl < 0)
                    || (sl > (parmsize - MQCFST_STRUC_LENGTH_FIXED)) )
                 {
                  rc = -11 ;
                  break    ;
                 }
               sp = ((MQCFST *)bufpcfi)->String                    ;
               zvarused = strlen(zvars) ;
               zvarname = strlen(comp)  ;
               if (    ((size_t)zvarlen < 2U)
                    || (zvarname > ((size_t)zvarlen - 2U))
                    || (zvarused > ((size_t)zvarlen - (zvarname + 2U))) )
                 {
                  if ( zvarlen > (((MQULONG)-1) / 2U) )
                    {
                     mqac = 0 ;
                     rc = -15      ;
                     break         ;
                    }
                  newzvarlen = 2U * zvarlen ;
                  if (    ((size_t)newzvarlen < 2U)
                       || (zvarname > ((size_t)newzvarlen - 2U))
                       || (zvarused > ((size_t)newzvarlen - (zvarname + 2U))) )
                    {
                     mqac = 0 ;
                     rc = -15      ;
                     break         ;
                    }
                  TRACE(traceid, ("Doing realloc for zvars %"PRId32" bytes\n",(int32_t)newzvarlen) ) ;
                  newzvars = (char *) realloc(zvars, newzvarlen) ;
                  if ( newzvars == NULL )
                    {
                     mqac = errno ;
                     TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) ) ;
                     rc = -15 ;
                     break    ;
                    }
                  else
                    {
                     zvars = newzvars     ;
                     zvarlen = newzvarlen ;
                    }
                 }
               rexxrc =
                 stem_from_string(traceid, zvars, RX_output, comp, sp, sl);
               if (    (rexxrc != RXSHV_OK)
                    && (rexxrc != RXSHV_NEWV)
                    && (    (rexxrcOutput == RXSHV_OK)
                         || (rexxrcOutput == RXSHV_NEWV) ) )
                 rexxrcOutput = rexxrc ;
               TRACE(traceid, (" String Parm %"PRId32" ->%.*s<- into %s\n",
                     (int32_t)parmnumb,sl,sp,comp) ) ;
               break                                               ;
 
             case MQCFT_STRING_LIST : //Print Strings in the Stem Variable
               if ( parmsize < MQCFSL_STRUC_LENGTH_FIXED )
                 {
                  rc = -11 ;
                  break    ;
                 }
               lstparms = ((MQCFSL *) bufpcfi)->Count     ;
               sl       = ((MQCFSL *) bufpcfi)->StringLength  ;
               if (    (lstparms < 0)
                    || (sl < 0) )
                 {
                  rc = -11 ;
                  break    ;
                 }
               if (    (sl != 0)
                    && (lstparms > (parmsize - MQCFSL_STRUC_LENGTH_FIXED) / sl) )
                 {
                  rc = -11 ;
                  break    ;
                 }
               if (    (sl == 0)
                    && (lstparms > (MQLONG)RXMQ_MAX_ZERO_LENGTH_STRING_LIST_COUNT) )
                 {
                  rc = -11 ;
                  break    ;
                 }
               TRACE(traceid, (" String List %"PRId32" values\n",(int32_t)lstparms) ) ;
               if ( lstparms != 0 )
                 {
                  sprintf(varnamc,"%s.0",comp)            ;
                  zvarused = strlen(zvars)   ;
                  zvarname = strlen(varnamc) ;
                  if (    ((size_t)zvarlen < 2U)
                       || (zvarname > ((size_t)zvarlen - 2U))
                       || (zvarused > ((size_t)zvarlen - (zvarname + 2U))) )
                    {
                     if ( zvarlen > (((MQULONG)-1) / 2U) )
                       {
                        mqac = 0 ;
                        rc = -15      ;
                        break         ;
                       }
                     newzvarlen = 2U * zvarlen ;
                     if (    ((size_t)newzvarlen < 2U)
                          || (zvarname > ((size_t)newzvarlen - 2U))
                          || (zvarused > ((size_t)newzvarlen - (zvarname + 2U))) )
                       {
                        mqac = 0 ;
                        rc = -15      ;
                        break         ;
                       }
                     TRACE(traceid, ("Doing realloc for zvars %"PRId32" bytes\n",(int32_t)newzvarlen) ) ;
                     newzvars = (char *) realloc(zvars, newzvarlen) ;
                     if ( newzvars == NULL )
                       {
                        mqac = errno ;
                        TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) ) ;
                        rc = -15 ;
                        break    ;
                       }
                     else
                       {
                        zvars = newzvars     ;
                        zvarlen = newzvarlen ;
                       }
                    }
                  rexxrc =
                    stem_from_long(traceid, zvars, RX_output,
                                   varnamc, lstparms)       ;
                  if (    (rexxrc != RXSHV_OK)
                       && (rexxrc != RXSHV_NEWV)
                       && (    (rexxrcOutput == RXSHV_OK)
                            || (rexxrcOutput == RXSHV_NEWV) ) )
                    rexxrcOutput = rexxrc ;
 
                  sp = ((MQCFSL *)bufpcfi)->Strings       ;
                  for ( j=0 ; j < lstparms ; j++ )
                    {
                     sprintf(varnamc,"%s.%u",comp,(j+1))  ;
                     zvarused = strlen(zvars)   ;
                     zvarname = strlen(varnamc) ;
                     if (    ((size_t)zvarlen < 2U)
                          || (zvarname > ((size_t)zvarlen - 2U))
                          || (zvarused > ((size_t)zvarlen - (zvarname + 2U))) )
                       {
                        if ( zvarlen > (((MQULONG)-1) / 2U) )
                          {
                           mqac = 0 ;
                           rc = -15      ;
                           break         ;
                          }
                        newzvarlen = 2U * zvarlen ;
                        if (    ((size_t)newzvarlen < 2U)
                             || (zvarname > ((size_t)newzvarlen - 2U))
                             || (zvarused > ((size_t)newzvarlen - (zvarname + 2U))) )
                          {
                           mqac = 0 ;
                           rc = -15      ;
                           break         ;
                          }
                        TRACE(traceid, ("Doing realloc for zvars %"PRId32" bytes\n",(int32_t)newzvarlen) ) ;
                        newzvars = (char *) realloc(zvars, newzvarlen) ;
                        if ( newzvars == NULL )
                          {
                           mqac = errno ;
                           TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) ) ;
                           rc = -15 ;
                           break    ;
                          }
                        else
                          {
                           zvars = newzvars     ;
                           zvarlen = newzvarlen ;
                          }
                       }
                     rexxrc =
                       stem_from_string(traceid, zvars, RX_output, varnamc, sp, sl);
                     if (    (rexxrc != RXSHV_OK)
                          && (rexxrc != RXSHV_NEWV)
                          && (    (rexxrcOutput == RXSHV_OK)
                               || (rexxrcOutput == RXSHV_NEWV) ) )
                       rexxrcOutput = rexxrc ;
                     TRACE(traceid, (" String Parm %"PRId32" ->*%.*s<- into %s\n",
                           (int32_t)parmnumb,sl,sp,comp) ) ;
                     sp = sp + sl ;
                    }
                  if ( rc != 0 ) break ;
                 }
               break        ;
             default :
               TRACE(traceid, (" Unsupported type %"PRId32", component %s\n",
                     (int32_t)parmtype,comp) ) ;
               break ;
            } ; //End of Switch group
 
          if ( rc != 0 ) break ;

          bufpcfi  = (MQCFIN *)( (char *)bufpcfi  + parmsize ) ; //Bump up pointer
          remaining = remaining - parmsize                     ;
 
         } ; //End of Group scanning loop
       if ( rc != 0 ) break ;
      } ; //End of Parameter scanning loop
 
    if ( (rc == 0) && (remaining != 0) )
      rc = -11 ;
 
 
// All Parms/Components extracted, so create ZLIST
 
    if ( rc == 0 )
      {
       rexxrc =
         stem_from_string(traceid, NULL, RX_output, "ZLIST", zvars , strlen(zvars));
       if (    (rexxrc != RXSHV_OK)
            && (rexxrc != RXSHV_NEWV)
            && (    (rexxrcOutput == RXSHV_OK)
                 || (rexxrcOutput == RXSHV_NEWV) ) )
         rexxrcOutput = rexxrc ;
      }

    if (    (rc == 0)
         && (rexxrcOutput != RXSHV_OK)
         && (rexxrcOutput != RXSHV_NEWV) )
      {
       TRACE(traceid,
             ("RexxVariablePool failed to publish RXMQEVNT output rc = %d\n",
              rexxrcOutput) ) ;
       rc = -7 ;
      }
 
    TRACE(traceid, ("All Event fields extracted\n") ) ;
   } // End of Event Processing Block
 
//
// Free ZLIST buffer, if allocated.
//
 if ( zvars != 0 )
   {
    TRACE(traceid, ("Free zlist\n") ) ;
    free(zvars) ;
   }
 
//
// Free data buffer for stem.1 variable data, if allocated.
//
 if ( data != 0 )
   {
    TRACE(traceid, ("Free area\n") )  ;
    free(data) ;
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQEVNT function
 
 
//
// Process a Trigger Message RXMQTM
//
//   Call:   rc = RXMQtm( input_Stem,   output_stem )
//
//           rc = RXMQtm( trig_message, output_stem )
//
//
//   This function works in two ways, depending upon whether or not the first
//        parameter ends with a dot (indicating a stem variable or some real
//        data).
//
//        If its a stem variable, then the usual .0 .1 convention applies and
//           and the message is deemed to be a Trigger Message derived from a
//           Queue. In this case, it is broken up into its components of
//           QN PN TD AT AID ED UD. These components are only generated if
//           the relevant fields are non-blank and non-zero. ZLIST processing
//           is provided for these components. Additionally, a component called
//           PL is created, which is a string (in TM2 format) suitable for
//           passing to an attached procedure (the QM field will be filled in
//           with the current Queue manager, if connected). PL is NOT part of
//           ZLIST (as it should not be fiddled about with in any way).
//
//        If its NOT a stem variable (not ending in a dot), then the input is
//           deamed to be the TM2 (PL) structure (which must be of the correct
//           length). In this case, the data (and its NOT a REXX variable) is
//           parsed for components QN PN TD AT ED UD QM with ZLIST processing;
//           if the item is all blanks (or all nulls!?!) then the component is
//           not built. Each component should be stripped of blanks before
//           usage in the Rexx environment.
//
//
FTYPE  RXMQTM  RXMQPARM
 {
 
 RXMQCB                * anchor  = 0      ;  // RXMQ Control Block
 MQLONG                  rc      = 0      ;  // Function Return Code
 MQLONG                  mqrc    = 0      ;  // MQ RC
 MQLONG                  mqac    = 0      ;  // MQ AC
 MQULONG                 traceid = TM     ;  // This function trace id
 int                     rexxrc = RXSHV_OK ;
 int                     rexxrcOutput = RXSHV_OK ;
 
 RXSTRING                RX_input         ;  // Variable Data - Input
 RXSTRING                RX_output        ;  // Variable Data - Output
 
 MQBYTE               *  data  = 0        ;  //-> Data buffer
 MQLONG                  data0   = 0      ;  // Input Data - len
 MQULONG                 datalen = 0      ;  //   Data length
 RXMQ_EXACT_FETCH_RESULT   fetchResult = RXMQ_EXACT_FETCH_INVALID ;
 
 MQTM                  * thetm            ;  // -> Trigger Message
 MQTMC2                * thetm2           ;  // -> Trigger Parm
 
 char                    header[5]        ;  // Header  Type
 char                    versionc[5]      ;  // Version Type in Char
 MQLONG                  version          ;  // Version Type
 int                     action = 1       ;  // TM or TM2 processing
 
 RXSTRING                oldtm2           ;  // Supplied Trigger Message
 MQTMC2                  newtm2           ;  // Build Trigger Parm
 
 char                    zlist[200]       ;  // Char version of .ZLIST
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null input stem var"},
        {  -3, "Zero input stem var"},
        {  -4, "Null output stem var"},
        {  -5, "Zero output stem var"},
        {  -6, "No input data"},
        {  -7, "Zero input data"},
        {  -8, "Cannot locate Header"},
        {  -9, "Cannot find Header"},
        { -10, "Not a TM Header"},
        { -11, "Cannot find Header"},
        { -12, "Unknown Header"},
        { -13, "Unknown Version"},
        { -14, "Header mismatch (1<>1)"},
        { -15, "Version mismatch (1<>1)"},
        { -16, "Header mismatch (2<>C)"},
        { -17, "Version mismatch (2<>C)"},
        { -18, "Too short for a TM"},
        { -19, "Too short for a TMC"},
        { -20, "malloc failure, check reason code"},
        { -21, "Data length is not equal to specified value"},
        { -22, "Unable to publish output to REXX"},
        { -98, "Not connected to a QM"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
 action = 1 ;
 zlist[0] = 0                             ;
 memset(&newtm2,  ' ', sizeof(newtm2)   ) ;
 
//
// Check the parms
//
 if ( (rc == 0) && (aargc != 2 ) )             rc =  -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc =  -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc =  -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc =  -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc =  -5 ;
 if ( (rc == 0) && ( anchor->QMh == 0 ) )      rc = -98 ;
 
//
// The input parms are OK, so obtain them, and obtain the resolutions
//
 if (rc == 0)
   {
    memcpy(&RX_input, &aargv[0],sizeof(RX_input))  ;
    memcpy(&RX_output,&aargv[1],sizeof(RX_output)) ;
 
    int dotat = RX_input.strlength - 1                       ;
    if ( RX_input.strptr[dotat] == '.' )
      {
       TRACE(traceid , ("Processing a Trigger Message\n") )  ;
       action = 1                                            ;
       stem_to_long  (traceid, RX_input, "0" , &data0)       ;
 
//
// Allocate data buffer to store stem.1 variable data
//
       if (( data0 > 0 ) )
         {
          fetchResult = fetch_exact_rexx_bytes(traceid, RX_input, "1",
                                               data0, &data) ;
          if ( fetchResult == RXMQ_EXACT_FETCH_NOMEM )
            {
             mqac = errno                                                        ;
             TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) )          ;
             rc = -20                                                            ;
            }
          else if ( fetchResult != RXMQ_EXACT_FETCH_SUCCESS )
            rc = -21 ;
          else
           {
            datalen = (MQULONG)data0 ;
            TRACE(traceid , ("Obtained the Trigger Message. Length = %"PRId32"\n",(int32_t)data0) ) ;
            if ( datalen != (MQULONG) data0 ) rc = -21                               ;
           }
         }
      }
    else
      {    // RX_input.strptr[dotat] != '.'
       TRACE(traceid , ("Processing Trigger Data\n") ) ;
       action = 2                                      ;
       oldtm2.strptr    = RX_input.strptr    ;
       oldtm2.strlength = RX_input.strlength ;
       TRACE(traceid , ("Obtained the Trigger Data. Length = %"PRIu32"\n",(uint32_t)RX_input.strlength) ) ;
      }
   }
 
//
// Now check the input Stem variable to see that there is
//     some valid data to obtain
//
 if ( (rc == 0) && (action == 1) && ( data0 <= 0 ) )   rc = -6 ;
 if ( (rc == 0) && (action == 1) && ( datalen == 0 ) ) rc = -7 ;
 if ( (rc == 0) && (action == 1) && ( data0 <= 3 ) )   rc = -8 ;
 if ( (rc == 0) && (action == 1) && ( datalen <= 3 ) ) rc = -9 ;
 
//
// Now check the input data to see that there is
//     some valid data to obtain
//
 if ( (rc == 0) && (action == 2) && ( oldtm2.strlength == 0 ) ) rc = -10 ;
 if ( (rc == 0) && (action == 2) && ( oldtm2.strlength <= 3 ) ) rc = -11 ;
 
//
// Although we have a valid item, just check the lengths
//          to ensure the Header and Version are present
//
 if ( ( rc == 0 ) && ( action == 1 ) &&
      ( datalen < (sizeof(MQCHAR4) + sizeof(MQLONG)) ) )
   rc = -18 ;

 if ( ( rc == 0 ) && ( action == 2 ) &&
      ( oldtm2.strlength < (sizeof(MQCHAR4) + sizeof(MQCHAR4)) ) )
   rc = -19 ;
 
//
// There is the possibility of an Trigger Message, so see if it is a known
//       one, and ignore it it is not one to process.
//
 if ( rc == 0 )
   {
    memset(header,    0, sizeof(header)   ) ;
    memset(&versionc, 0, sizeof(versionc) ) ;
    version = 0                             ;
 
    if ( action == 1 ) strncpy( header,   (const char *) data         , sizeof(MQCHAR4) ) ;
    else               strncpy( header,   (const char *)oldtm2.strptr , sizeof(MQCHAR4) ) ;
 
    if ( action == 1 )
      {
       memcpy(&version,
              (const char *)data + sizeof(MQCHAR4),
              sizeof(MQLONG)) ;

       memcpy(versionc,
              (const char *)data + sizeof(MQCHAR4),
              sizeof(MQCHAR4)) ;
      }
    else
      {
       memcpy(&version,
              (const char *)oldtm2.strptr + sizeof(MQCHAR4),
              sizeof(MQLONG)) ;

       memcpy(versionc,
              (const char *)oldtm2.strptr + sizeof(MQCHAR4),
              sizeof(MQCHAR4)) ;
      }
 
    TRACE(traceid, ("Action = %d, Header = /%s/ Versionc = /%s/ Version = /%"PRId32"/\n",
                    action,header,versionc,(int32_t)version) ) ;
 
    if (    ( rc == 0 )
         && ( strcmp(header, MQTM_STRUC_ID ) != 0 )
         && ( strcmp(header, MQTMC_STRUC_ID) != 0 ) ) rc = -12 ;
 
    if (    ( rc == 0 )
         && ( version  != MQTM_VERSION_1 )
         && ( strncmp(versionc, MQTMC_VERSION_2, sizeof(MQCHAR4) ) != 0 ) ) rc = -13 ;
   }
 
//
// Now there is a trigger area of some sort, just check to see its the right one
//
 if ( ( rc == 0 ) && (action == 1) && ( strcmp(header, MQTM_STRUC_ID ) != 0 ) )  rc = -14 ;
 if ( ( rc == 0 ) && (action == 1) && ( version != MQTM_VERSION_1 ) )            rc = -15 ;
 if ( ( rc == 0 ) && (action == 2) && ( strcmp(header, MQTMC_STRUC_ID ) != 0 ) ) rc = -16 ;
 if ( ( rc == 0 ) && (action == 2) && ( strncmp(versionc, MQTMC_VERSION_2, sizeof(MQCHAR4) ) != 0 ) ) rc = -17 ;
 
//
// Although we have a valid item, just check the lengths
//          to ensure the WHOLE thing is present
//
 if ( ( rc == 0 ) && ( action == 1 ) &&
      ( datalen < sizeof(MQTM) ) )
   rc = -18 ;

 if ( ( rc == 0 ) && ( action == 2 ) &&
      ( oldtm2.strlength < sizeof(MQTMC2) ) )
   rc = -19 ;

//
// Now we have got a valid Trigger Message, split it up
//
if ( ( rc == 0 ) && ( action == 1 ) )
  {
   TRACE(traceid, ("Unravelling a TM message\n")       ) ;
   TRACE(traceid, ("QM name is /%.*s/\n",
                   (int)sizeof(anchor->QMname),anchor->QMname) ) ;
   TRACE(traceid, ("Now starting the unpacking\n")     ) ;
   thetm = (MQTM *) data; // Set Header pointer
 
                         // Build the output TMC2 area Header
 
   memcpy(&newtm2.StrucId,     MQTMC_STRUC_ID,     sizeof(MQCHAR4)  ) ;
   memcpy(&newtm2.Version,     MQTMC_VERSION_2,    sizeof(MQCHAR4)  ) ;
   memcpy(&newtm2.QName,       thetm->QName,       sizeof(MQCHAR48) ) ;
   memcpy(&newtm2.ProcessName, thetm->ProcessName, sizeof(MQCHAR48) ) ;
   memcpy(&newtm2.TriggerData, thetm->TriggerData, sizeof(MQCHAR64) ) ;
   memcpy(&newtm2.ApplId,      thetm->ApplId,      sizeof(MQCHAR256)) ;
   memcpy(&newtm2.EnvData,     thetm->EnvData,     sizeof(MQCHAR128)) ;
   memcpy(&newtm2.UserData,    thetm->UserData,    sizeof(MQCHAR128)) ;
   memcpy(&newtm2.QMgrName,    anchor->QMname,     sizeof(MQCHAR48) ) ;
 
                         //Build the components
 
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "QN"  , thetm->QName,                sizeof(MQCHAR48)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "PN"  , thetm->ProcessName,          sizeof(MQCHAR48)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "TD"  , thetm->TriggerData,          sizeof(MQCHAR64)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_long  (traceid, zlist, RX_output, "AT"  , thetm->ApplType)                               ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "AID" , thetm->ApplId,              sizeof(MQCHAR256)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "ED"  , thetm->EnvData,             sizeof(MQCHAR128)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "UD"  , thetm->UserData,            sizeof(MQCHAR128)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "ZLIST", zlist, strlen(zlist))                         ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_bytes (traceid, zlist, RX_output, "PL"  , (MQBYTE *)&newtm2,sizeof(MQTMC2))              ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;

   if (    (rc == 0)
        && (rexxrcOutput != RXSHV_OK)
        && (rexxrcOutput != RXSHV_NEWV) )
     {
      TRACE(traceid,
            ("RexxVariablePool failed to publish RXMQTM TM output rc = %d\n",
             rexxrcOutput) ) ;
      rc = -22 ;
     }
 
   TRACE(traceid, ("Unravelled the TM\n") ) ;
  }
 
//
// Now we have got valid Trigger Data, split it up
//
if ( ( rc == 0 ) && ( action == 2 ) )
  {
   TRACE(traceid, ("Unravelling Trigger Data\n") ) ;
 
   thetm2 = (MQTMC2 *)oldtm2.strptr   ; // Set Header pointer
 
//Build the components
 
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "QN"  , thetm2->QName,          sizeof(MQCHAR48)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "PN"  , (char*)thetm2->ProcessName, sizeof(MQCHAR48)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "TD"  , thetm2->TriggerData,          sizeof(MQCHAR64)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "AID" , thetm2->ApplId,              sizeof(MQCHAR256)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "ED"  , thetm2->EnvData,             sizeof(MQCHAR128)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "UD"  , thetm2->UserData,            sizeof(MQCHAR128)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "QM"  , thetm2->QMgrName,             sizeof(MQCHAR48)) ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;
   rexxrc =
     stem_from_string(traceid, zlist, RX_output, "ZLIST", zlist, strlen(zlist))                         ;
   if (    (rexxrc != RXSHV_OK)
        && (rexxrc != RXSHV_NEWV)
        && (    (rexxrcOutput == RXSHV_OK)
             || (rexxrcOutput == RXSHV_NEWV) ) )
     rexxrcOutput = rexxrc ;

   if (    (rc == 0)
        && (rexxrcOutput != RXSHV_OK)
        && (rexxrcOutput != RXSHV_NEWV) )
     {
      TRACE(traceid,
            ("RexxVariablePool failed to publish RXMQTM TMC2 output rc = %d\n",
             rexxrcOutput) ) ;
      rc = -22 ;
     }
 
   TRACE(traceid, ("Unravelled the Trigger Data\n") ) ;
  }
 
//
// Free data buffer for stem.1 variable data, if allocated.
//
 if ( data != 0 )
   {
    TRACE(traceid, ("Free area\n") ) ;
    free(data) ;
   }
 
//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
 } // End of RXMQTM function
 
 
#ifdef __MVS__
//
// Execute a Command (use MQSC interface for MVS)
//
//   Call:   rc = RXMQC(parms, input_command, output_response )
//
FTYPE RXMQC  RXMQPARM
{
 RXMQCB                * anchor = 0       ;  // RXMQ Control Block
 MQLONG                  rc      = 0      ;  // Function Return Code
 MQLONG                  mqrc    = 0      ;  // MQ RC
 MQLONG                  mqac    = 0      ;  // MQ AC
 MQLONG                  mqac2   = 0      ;  // MQ AC
 MQULONG                 traceid = COM    ;  // This function trace id
 MQLONG                  dummy            ;  // No interest rc
 MQULONG                 count   = 0      ;  // Responses received
 MQULONG                 linecnt = 0      ;  // Line count in reply msg
 MQULONG                 lines   = 0      ;  // Lines processed in a group
 int                     rexxrc       = RXSHV_OK ;
 int                     rexxrcOutput = RXSHV_OK ;
 
 RXSTRING                RX_parm          ;  // Variable Parms
 RXSTRING                RX_command       ;  // Variable Command
 RXSTRING                RX_response      ;  // Variable Reply Stem Var
 
 char                 *  buffer   =     0 ;  //-> Data buffer
 char                 *  newbuffer =    0 ;  //-> Reallocated data buffer
 MQLONG                  bufflen  = 15000 ;  // Default buffer length
 MQLONG                  reclen   =     0 ;  // Received record length
 
 MQLONG        DisconnectFinally = 0      ;  // Disconnect after completion
 MQHCONN                 qmh              ;  // Queue Manager handle
 MQHOBJ                  cQh     = 0      ;  // Command queue handle
 MQOD                    cod              ;  // Command queue object descriptor
 MQHOBJ                  rQh     = 0      ;  // Reply queue handle
 MQOD                    rod              ;  // Reply queue object descriptor
 MQMD2                   md               ;  // Message descriptor for PUT & GET
 MQPMO                   pmo              ;  // PUT message options
 MQGMO                   gmo              ;  // GET message options
 
 
 char        qm   [MQ_Q_MGR_NAME_LENGTH+1] ; //QM name
 char        cq   [MQ_Q_NAME_LENGTH+1    ] ; //Q  name - command
 char        rq   [MQ_Q_NAME_LENGTH+1    ] ; //Q  name - replyToq
 MQBYTE24    CorrelMsg                     ; //PUT MsgId = GET CorrelId
 char        var  [16]                     ;
 MQLONG      to   = 5000                   ; //Timeout for MQ Get in msec
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null parms"},
        {  -3, "Zero parms"},
        {  -4, "Null command var"},
        {  -5, "Zero command var"},
        {  -6, "Null response stem var"},
        {  -7, "Zero length response stem var"},
        {  -8, "No command supplied"},
        {  -9, "Too big a Command supplied"},
        { -10, "malloc failure, check reason code"},
        { -11, "Connect to QMgr failed, check rc/rsn"},
        { -12, "Open command queue failed, check rc/rsn"},
        { -13, "Open response queue failed, check rc/rsn"},
        { -14, "Put command to queue failed, check rc/rsn"},
        { -15, "Get response from queue failed, check rc/rsn"},
        { -16, "Unable to publish command response to REXX"},
        { -17, "Queue manager name too long"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
 if ( (rc == 0) && (aargc != 3 ) )             rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc = -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc = -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc = -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )    rc = -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) ) rc = -7 ;
 
 if (rc == 0)
   {
    memcpy(&RX_parm,    &aargv[0],sizeof(RX_parm))     ; //QM Name (or stem.)
    memcpy(&RX_command, &aargv[1],sizeof(RX_command))  ; //Command to issue
    memcpy(&RX_response,&aargv[2],sizeof(RX_response)) ; //Return stem.
 
    TRACE(traceid, ("RX_parm     = %.*s\n",(int)RX_parm.strlength,    RX_parm.strptr)     ) ;
    TRACE(traceid, ("RX_command  = %.*s\n",(int)RX_command.strlength, RX_command.strptr)  ) ;
    TRACE(traceid, ("RX_response = %.*s\n",(int)RX_response.strlength,RX_response.strptr) ) ;
 
    memset(qm,0,MQ_Q_MGR_NAME_LENGTH+1     ) ; //Clear data areas
    memset(cq,0,MQ_Q_NAME_LENGTH+1         ) ;
    memset(rq,0,MQ_Q_NAME_LENGTH+1         ) ;
    strcpy(cq,"SYSTEM.COMMAND.INPUT" )       ; // and set defaults
    strcpy(rq,"SYSTEM.COMMAND.REPLY.MODEL" ) ;
 
 //If the given variable is not a stem. variable, then take the
 //   quicker option of assuming it's the Queue Manager name
 
    if ( RX_parm.strptr[RX_parm.strlength-1] != '.' )
      {
       if ( RX_parm.strlength > MQ_Q_MGR_NAME_LENGTH )
         rc = -17 ;
       else
         memcpy(qm, RX_parm.strptr, RX_parm.strlength ) ;
      }
 
 //The given variable is a stem. variable, so get its contents
    else
      {
       stem_to_string(traceid, RX_parm, "QM"  , qm, MQ_Q_MGR_NAME_LENGTH) ;
       stem_to_string(traceid, RX_parm, "CQ"  , cq, MQ_Q_NAME_LENGTH    ) ;
       stem_to_string(traceid, RX_parm, "RQ"  , rq, MQ_Q_NAME_LENGTH    ) ;
       stem_to_long  (traceid, RX_parm, "TO"  , &to                     ) ;
      }
 
    TRACE(traceid, ("QM = %s\n",qm) ) ;
    TRACE(traceid, ("CQ = %s\n",cq) ) ;
    TRACE(traceid, ("RQ = %s\n",rq) ) ;
    TRACE(traceid, ("Command = <%.*s>\n",(int)RX_command.strlength,RX_command.strptr) ) ;
 
   }
 
 if ( (rc == 0 ) && ( RX_command.strlength == 0 ) )                     rc = -8 ;
 if ( (rc == 0 ) && ( RX_command.strlength > MQ_COMMAND_MQSC_LENGTH ) ) rc = -9 ;
 
//
//The return stem variable is initially set to no info
//
 if ( rc == 0 )
   {
    rexxrc = stem_from_long (traceid, NULL, RX_response, "0" , 0)                 ;
    if (    (rexxrc != RXSHV_OK)
         && (rexxrc != RXSHV_NEWV) )
      {
       TRACE(traceid, ("Unable to initialize command response in REXX, rc = %d\n",rexxrc) ) ;
       rc = -16 ;
      }
   }
 
// 1) Connect to queue manager, if not yet connected
 
 if (rc == 0)
   if ( (anchor->QMh == 0) || strncmp(anchor->QMname, qm, sizeof(anchor->QMname)) ) // Is it connected to correct QM ?
     {                                                       // No
      TRACE(traceid, ("Connecting to QM %s\n",qm) ) ;
      MQCONN ( qm, &qmh, &mqrc, &mqac )             ;
      TRACE(traceid, ("MQCONN rc = %ld\n",mqrc ) )  ;
      if ( mqrc != 0 )  rc = -11                    ;
      else DisconnectFinally = 1                    ;
     }
   else qmh = anchor->QMh                           ;
 
 
// 2) Open the command queue for PUT access
 
 if ( rc == 0 )
   {
    memcpy ( &cod, &od_default, sizeof(MQOD))     ;
    strncpy( cod.ObjectQMgrName, qm, strlen(qm))  ;
    strncpy( cod.ObjectName, cq, strlen(cq))      ;
    TRACE(traceid, ("Opening Command Q %s for output\n",cq) ) ;
    MQOPEN ( qmh, &cod, MQOO_OUTPUT, &cQh, &mqrc, &mqac )     ;
    TRACE(traceid, ("MQOPEN rc = %ld\n",mqrc) )               ;
    if ( mqrc != 0 ) rc = -12                             ;
   }
 
// 3) Open/Create the ReplyToQ for Get access using model
 
 if ( rc == 0)
   {
    memcpy(&rod, &od_default, sizeof(MQOD));
    memcpy(rod.ObjectName, rq, strlen(rq)) ;
    strcpy(rod.DynamicQName,"RXMQ.*")      ;
    TRACE(traceid, ("Opening Response Q by model %s for Destructive access\n",rq) ) ;
    MQOPEN ( qmh, &rod, MQOO_INPUT_SHARED, &rQh, &mqrc, &mqac ) ;
    TRACE(traceid, ("MQOPEN rc = %ld\n",mqrc) )                 ;
    if ( mqrc != 0 ) rc = -13              ;
   }
 
// 4) Put command on command queue
 
 if ( rc == 0)
   {
    memcpy(rq, rod.ObjectName, MQ_Q_NAME_LENGTH)         ;
    TRACE(traceid, ("The ReplyToQ name is %.*s\n",MQ_Q_NAME_LENGTH,rq) ) ;
 
    memcpy(&md,  &md_default,  sizeof(MQMD2))            ;
    md.MsgType = MQMT_REQUEST                            ;
    memcpy(md.Format, MQFMT_STRING, sizeof(MQCHAR8))     ;
    memcpy(md.ReplyToQ, rq, MQ_Q_NAME_LENGTH)            ;
    memcpy(md.ReplyToQMgr, qm, MQ_Q_MGR_NAME_LENGTH)     ;
 
    memcpy(&pmo, &pmo_default, sizeof(MQPMO))            ;
    pmo.Options = MQPMO_NO_SYNCPOINT
                | MQPMO_DEFAULT_CONTEXT                  ;
 
    TRACE(traceid, ("Now issuing the MQPUT to the Command Queue\n") ) ;
    MQPUT ( qmh, cQh, &md, &pmo, RX_command.strlength, RX_command.strptr, &mqrc, &mqac ) ;
    TRACE(traceid, ("MQPUT rc = %ld\n",mqrc) )                   ;
    if ( mqrc != 0 ) rc = -14                            ;
   }
 
 if (cQh) MQCLOSE ( qmh, &cQh, MQCO_NONE, &dummy, &dummy ) ;
 
 
//
// Allocate data buffer to receive the ReplyToQ records
//
 if ( rc == 0 )
   {
    TRACE(traceid, ("Doing malloc for %ld bytes\n",bufflen) )  ;
    buffer = (char *) malloc(bufflen)                          ;
    if ( buffer == NULL )
      {
       mqac = errno                                ;
       TRACE(traceid, ("malloc rc = %ld\n",mqac) ) ;
       rc = -10                                    ;
      }
   }
 
// 5) Get results
 
 if ( rc == 0 )
   {
    TRACE(traceid, ("Now starting to obtain the ReplyToQ messages\n") ) ;
    memcpy(CorrelMsg, md.MsgId, sizeof(MQBYTE24))   ;
    memcpy(&gmo, &gmo_default, sizeof(MQGMO))       ;
    gmo.Options = MQGMO_NO_SYNCPOINT                |
                  MQGMO_WAIT                        ;
    gmo.WaitInterval =    to                        ;  // 5 sec
 
    mqac2 = 4                                        ;  // CSQN205I rsn to continue
    while ( ( (mqrc == 0) && (mqac2 == 4) ) || (linecnt < lines) )
      {
       TRACE(traceid, ("Issuing a MQGET to the ReplyToQ %s\n",rq) ) ;
       memcpy(&md,  &md_default,  sizeof(MQMD2))             ;
       memcpy(md.CorrelId, CorrelMsg, sizeof(MQBYTE24))      ;
       memset(buffer, 0, bufflen)                            ;
       MQGET ( qmh, rQh, &md, &gmo, bufflen, buffer, &reclen, &mqrc, &mqac ) ;
       TRACE(traceid, ("MQGET rc = %ld, ac = %ld, Datalen = %ld\n",mqrc,mqac,reclen) ) ;
 
       if (mqac == MQRC_TRUNCATED_MSG_FAILED)            // buffer is too small
         {
          if ( reclen <= bufflen )
            {
             rc = -15 ;
             break;
            }
          TRACE(traceid, ("Reallocating buffer to %"PRId32" bytes\n",(int32_t)reclen) ) ;
          newbuffer = (char *) realloc(buffer,(size_t)reclen) ;
          if ( newbuffer == NULL )
            {
             mqac = errno                                ;
             TRACE(traceid, ("malloc rc = %ld\n",mqac) ) ;
             rc = -10                                    ;
             break;
            }
          buffer  = newbuffer ;
          bufflen = reclen   ;
          mqrc = 0           ;
          mqac = 0           ;
          continue;
         }
 
    if (mqrc != 0)
      {
       rc = -15 ;
       break ;
      }
 
    if (!strncmp ("CSQN205I", (const char *)buffer, 8))
      {
       uint32_t parsedLines = 0 ;
       uint32_t parsedRc    = 0 ;
       uint32_t parsedAc    = 0 ;
       char     linesField[9] ;
       char     rcField[9]    ;
       char     acField[9]    ;
       linecnt = 1;
       if (    (reclen < 17)
            || (reclen - 17 < 8)
            || (reclen < 34)
            || (reclen - 34 < 8)
            || (reclen < 51)
            || (reclen - 51 < 8) )
         {
          rc = -15 ;
          break;
         }
       memcpy(linesField, buffer + 17, 8) ;
       linesField[8] = '\0' ;
       memcpy(rcField, buffer + 34, 8) ;
       rcField[8] = '\0' ;
       memcpy(acField, buffer + 51, 8) ;
       acField[8] = '\0' ;
       if (    (sscanf(linesField, "%8"SCNu32, &parsedLines) != 1)
            || (sscanf(rcField, "%8"SCNx32, &parsedRc   ) != 1)
            || (sscanf(acField, "%8"SCNx32, &parsedAc   ) != 1) )
         {
          rc = -15 ;
          break;
         }
       lines = (MQULONG)parsedLines ;
       mqrc  = (MQLONG) parsedRc    ;
       mqac2 = (MQLONG) parsedAc    ;
       TRACE(traceid, ("CSQN205I COUNT = %ld, RETURN = %ld, REASON = %ld\n",lines,mqrc,mqac2) ) ;
      }
    else
      if ( lines == 0 )              // CSQN205I must be 1st response
        {
        TRACE(traceid, ("No CSQN205I response received\n") ) ;
         break;
        }
      else
        {
         count++   ;
         linecnt++ ;
         sprintf(var,"%"PRIu32, (uint32_t)count );
         rexxrc = stem_from_long  (traceid, NULL, RX_response, "0" , count)                           ;
         if (    (rexxrc != RXSHV_OK)
              && (rexxrc != RXSHV_NEWV)
              && (   (rexxrcOutput == RXSHV_OK)
                  || (rexxrcOutput == RXSHV_NEWV)) )
           rexxrcOutput = rexxrc ;
         rexxrc = stem_from_bytes (traceid, NULL, RX_response, var , (unsigned char *)buffer, reclen) ;
         if (    (rexxrc != RXSHV_OK)
              && (rexxrc != RXSHV_NEWV)
              && (   (rexxrcOutput == RXSHV_OK)
                  || (rexxrcOutput == RXSHV_NEWV)) )
           rexxrcOutput = rexxrc ;
        }
      }
   }
 
//
// Free data buffer, if allocated.
//
 if ( buffer != 0 )
   {
   TRACE(traceid, ("Free buffer\n") ) ;
    free(buffer) ;
   }
 
 if (rQh) MQCLOSE ( qmh, &rQh, MQCO_DELETE_PURGE, &dummy, &dummy ) ;
 
 if( DisconnectFinally ) MQDISC ( &qmh, &dummy, &dummy ) ;
 
 if (    (rc == 0)
      && (rexxrcOutput != RXSHV_OK)
      && (rexxrcOutput != RXSHV_NEWV) )
   {
    rc = -16 ;
   }

//
// Set the LAST variables, and the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
return 0;
}
 
#else
//
// Execute a Command (use PCF interface for Windows, AIX, Linux)
//
//   Call:   rc = RXMQC(parms, input_command, output_response )
//
FTYPE RXMQC RXMQPARM
{
 
 RXMQCB                * anchor  = 0      ;  // RXMQ Control Block
 MQLONG                  rc      = 0      ;  // Function Return Code
 MQLONG                  mqrc    = 0      ;  // MQ RC
 MQLONG                  mqac    = 0      ;  // MQ AC
 MQLONG                  dummy            ;  // No interest rc
 MQULONG                 traceid = COM    ;  // This function trace id
 int                     rexxrc       = RXSHV_OK ;
 int                     rexxrcOutput = RXSHV_OK ;
 
 RXSTRING                RX_parm          ;  // Variable Parms
 RXSTRING                RX_command       ;  // Variable Command
 RXSTRING                RX_response      ;  // Variable Reply Stem Var
 
 char        qm   [MQ_Q_MGR_NAME_LENGTH+1] ; //QM name
 char        cq   [MQ_Q_NAME_LENGTH+1    ] ; //Q  name - command
 char        rq   [MQ_Q_NAME_LENGTH+1    ] ; //Q  name - replyToq
 MQLONG      to   = 10000                  ; //Timeout for MQ Get (10 sec)
 
 MQLONG        DisconnectFinally = 0      ;  // Disconnect after completion
 MQHCONN                 qmh              ;  // Queue Manager handle
 MQHOBJ                  cQh     = 0      ;  // Command queue handle
 MQOD                    cod              ;  // Command queue object descriptor
 MQHOBJ                  rQh     = 0      ;  // Reply queue handle
 MQOD                    rod              ;  // Reply queue object descriptor
 MQMD2                   md               ;  // Message descriptor for PUT & GET
 MQPMO                   pmo              ;  // PUT message options
 MQGMO                   gmo              ;  // GET message options
 
 char        pcfarea  [MQCFH_STRUC_LENGTH        +    //PCF area - header
                       MQCFIN_STRUC_LENGTH       +    //         - integer parm
                       MQCFST_STRUC_LENGTH_FIXED +    //         - string parm
                       MAXCOMMLEN+5              ] ;  //         - command
 
 MQCFH     * ptrpcf1 = (MQCFH  *) pcfarea                 ; //Set PCF area pointers
 MQCFIN    * ptrpcf2 = (MQCFIN *)&pcfarea[MQCFH_STRUC_LENGTH]    ;
 MQCFST    * ptrpcf3 = (MQCFST *)&pcfarea[MQCFH_STRUC_LENGTH+MQCFIN_STRUC_LENGTH] ;
 char      * ptrpcf4 = (char   *)&pcfarea[MQCFH_STRUC_LENGTH+MQCFIN_STRUC_LENGTH+MQCFST_STRUC_LENGTH_FIXED] ;
 
 MQCFH     * bufpcf1         ; //PCF pointers
 MQCFIN    * bufpcf2i        ; // for the returned
 MQCFST    * bufpcf2s        ; // info
 MQCFIL    * bufpcf2il       ;
 MQCFSL    * bufpcf2sl       ;
 
 MQLONG      pcflen  = 0    ; //Size of PCF to be sent
 MQLONG      recno   = 0    ; //Obtained record number
 MQLONG      outrec  = 0    ; //Published response number
 MQLONG      reclen  = 0    ;
 MQLONG      remaining = 0  ;
 
 void                 *  buffer       = 0     ;  //-> Data buffer
 void                 *  newbuffer    = 0     ;  //-> Reallocated data buffer
 int                     bufflen      = 10000 ;  //   Data length max
 
 char                    command[MAXCOMMLEN+5]  ; //Padded Command
 MQLONG                  commandlen             ; // to send to QM
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null parms"},
        {  -3, "Zero parms"},
        {  -4, "Null command var"},
        {  -5, "Zero command var"},
        {  -6, "Null response stem var"},
        {  -7, "Zero length response stem var"},
        {  -8, "No command supplied"},
        {  -9, "Too big a Command supplied"},
        { -10, "malloc failure, check reason code"},
        { -11, "Connect to QMgr failed, check rc/rsn"},
        { -12, "Open command queue failed, check rc/rsn"},
        { -13, "Open response queue failed, check rc/rsn"},
        { -14, "Put command to queue failed, check rc/rsn"},
        { -15, "Get response from queue failed, check rc/rsn"},
        { -16, "Unable to publish command response to REXX"},
        { -17, "Queue manager name too long"},
        { -18, "Invalid PCF response"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 rc = set_envir (afuncname, &traceid, &anchor)    ;
 
//
// Check the parms
//
 
 if ( (rc == 0) && (aargc != 3 ) )             rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc = -3 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[1]) )    rc = -4 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[1]) ) rc = -5 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[2]) )    rc = -6 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[2]) ) rc = -7 ;
 
//
// Now the parms are correct, get them
//
 
 if (rc == 0)
   {
    memcpy(&RX_parm,    &aargv[0],sizeof(RX_parm))     ; //QM Name (or stem.)
    memcpy(&RX_command, &aargv[1],sizeof(RX_command))  ; //Command to issue
    memcpy(&RX_response,&aargv[2],sizeof(RX_response)) ; //Return stem.
 
    TRACE(traceid, ("RX_parm     = %.*s\n",(int)RX_parm.strlength,    RX_parm.strptr)     ) ;
    TRACE(traceid, ("RX_command  = %.*s\n",(int)RX_command.strlength, RX_command.strptr)  ) ;
    TRACE(traceid, ("RX_response = %.*s\n",(int)RX_response.strlength,RX_response.strptr) ) ;
 
 
    memset(qm,0,MQ_Q_MGR_NAME_LENGTH+1     ) ; //Clear data areas
    memset(cq,0,MQ_Q_NAME_LENGTH+1         ) ;
    memset(rq,0,MQ_Q_NAME_LENGTH+1         ) ;
    strcpy(cq,"SYSTEM.ADMIN.COMMAND.QUEUE" ) ; // and set defaults
    strcpy(rq,"SYSTEM.MQSC.REPLY.QUEUE"    ) ;
 
 //If the given variable is not a stem. variable, then take the
 //   quicker option of assuming it's the Queue Manager name
 
    if ( RX_parm.strptr[RX_parm.strlength-1] != '.' )
      {
       if ( RX_parm.strlength > MQ_Q_MGR_NAME_LENGTH )
         rc = -17 ;
       else
         memcpy(qm, RX_parm.strptr, RX_parm.strlength ) ;
      }
 
 //The given variable is a stem. variable, so get its contents
    else
      {
       stem_to_string(traceid, RX_parm, "QM"  , qm, MQ_Q_MGR_NAME_LENGTH) ;
       stem_to_string(traceid, RX_parm, "CQ"  , cq, MQ_Q_NAME_LENGTH    ) ;
       stem_to_string(traceid, RX_parm, "RQ"  , rq, MQ_Q_NAME_LENGTH    ) ;
       stem_to_long  (traceid, RX_parm, "TO"  , &to                     ) ;
      }
 
    TRACE(traceid, ("QM = %s\n",qm) ) ;
    TRACE(traceid, ("CQ = %s\n",cq) ) ;
    TRACE(traceid, ("RQ = %s\n",rq) ) ;
    TRACE(traceid, ("Command = <%.*s>\n",(int)RX_command.strlength,RX_command.strptr) ) ;
   }
 
 //If no command, then nothing to do
 
 if ( (rc == 0 ) && ( RX_command.strlength == 0 ) ) rc = -8 ;
 
 //If command is too big, error
 
 if ( (rc == 0 ) && ( RX_command.strlength > MAXCOMMLEN ) ) rc = -9 ;
 
//Ensure Command is a multiple of 4 bytes long (PCF String requirement)
 
if ( rc == 0 )
  {
   memset(command,0,sizeof(command)) ;
   memcpy(command,RX_command.strptr,RX_command.strlength) ;
   switch ( RX_command.strlength % 4 )
     {
      case 1  : strcat(command,"   ") ; break ;
      case 2  : strcat(command,"  " ) ; break ;
      case 3  : strcat(command," "  ) ; break ;
      default : break                         ;
     }
   commandlen = strlen(command) ;
  }
 
//
// Allocate data buffer to receive the ReplyToQ records
//
 if ( rc == 0 )
   {
    TRACE(traceid, ("Doing malloc for %d bytes\n",bufflen) ) ;
    buffer = malloc(bufflen)                                 ;
    if ( buffer == NULL )
      {
       mqac = errno                                               ;
       TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) ) ;
       rc = -10                                                   ;
      }
   }
 
//The return stem variable is initially set to no info
 
 if ( rc == 0 )
   {
    rexxrc = stem_from_long  (traceid, NULL, RX_response, "0" , 0);
    if (    (rexxrc != RXSHV_OK)
         && (rexxrc != RXSHV_NEWV) )
      {
       TRACE(traceid, ("Unable to initialize command response in REXX, rc = %d\n",rexxrc) ) ;
       rc = -16 ;
      }
   }
 
//Connect to the QM, open the Command Queue, and create the ReplyToQ
 
 if (rc == 0)
   {
    if ( (anchor->QMh == 0) || strncmp(anchor->QMname, qm, sizeof(anchor->QMname)) ) // Is it connected to correct QM ?
      {                                                     // No
       TRACE(traceid, ("Connecting to QM %s\n",qm) )                ;
       MQCONN ( qm, &qmh, &mqrc, &mqac )                            ;
       TRACE(traceid, ("MQCONN rc = %"PRId32"\n",(int32_t)mqrc ) )  ;
       if ( mqrc != 0 )  rc = -11                                   ;
       else DisconnectFinally = 1                                   ;
      }
    else qmh = anchor->QMh                                          ;
   }
 
 if ( rc == 0 )                      // Open the command Q for Put access
   {
    memcpy ( &cod, &od_default, sizeof(MQOD))     ;
    strncpy( cod.ObjectQMgrName, qm, strlen(qm))  ;
    strncpy( cod.ObjectName, cq, strlen(cq))      ;
    TRACE(traceid, ("Opening Command Q %s for output\n",cq) )  ;
    MQOPEN ( qmh, &cod, MQOO_OUTPUT, &cQh, &mqrc, &mqac )      ;
    TRACE(traceid, ("MQOPEN rc = %"PRId32"\n",(int32_t)mqrc) ) ;
    if ( mqrc != 0 ) rc = -12                                  ;
   }
 
 if ( rc == 0 )                      // Open/Create the ReplyToQ for Get access
   {
    memcpy(&rod, &od_default, sizeof(MQOD));
    memcpy(rod.ObjectName, rq, strlen(rq)) ;
    strcpy(rod.DynamicQName,"RXMQ.*")      ;
    TRACE(traceid, ("Opening Response Q by model %s for Destructive access\n",rq) ) ;
    MQOPEN ( qmh, &rod, MQOO_INPUT_SHARED, &rQh, &mqrc, &mqac ) ;
    TRACE(traceid, ("MQOPEN rc = %"PRId32"\n",(int32_t)mqrc) )  ;
    if ( mqrc != 0 ) rc = -13                                   ;
  }
 
 //Build the PCF ESCAPE command
 
 if ( rc == 0 )                      // Now build the PCF command
   {
    TRACE(traceid, ("Now building the PCF area\n") ) ;
    memset(pcfarea,0,sizeof(pcfarea))             ;
    ptrpcf1->Type           = MQCFT_COMMAND       ; // Header - 2 parms
    ptrpcf1->StrucLength    = MQCFH_STRUC_LENGTH  ;
    ptrpcf1->Version        = MQCFH_VERSION_1     ;
    ptrpcf1->Command        = MQCMD_ESCAPE        ;
    ptrpcf1->MsgSeqNumber   = 1                   ;
    ptrpcf1->Control        = MQCFC_LAST          ;
    ptrpcf1->ParameterCount = 2                   ;
    ptrpcf2->Type           = MQCFT_INTEGER       ; // Parm1 is ESCAPE
    ptrpcf2->StrucLength    = MQCFIN_STRUC_LENGTH ;
    ptrpcf2->Parameter      = MQIACF_ESCAPE_TYPE  ;
    ptrpcf2->Value          = MQET_MQSC           ;
    ptrpcf3->Type           = MQCFT_STRING        ; // Parm2 is ESCAPE data
    ptrpcf3->StrucLength    = MQCFST_STRUC_LENGTH_FIXED + commandlen  ;
    ptrpcf3->Parameter      = MQCACF_ESCAPE_TEXT  ;
    ptrpcf3->CodedCharSetId = MQCCSI_DEFAULT      ;
    ptrpcf3->StringLength   = commandlen          ;
    memcpy(ptrpcf4,command,commandlen)            ;
 
    pcflen =   MQCFH_STRUC_LENGTH
             + MQCFIN_STRUC_LENGTH
             + MQCFST_STRUC_LENGTH_FIXED
             + commandlen                         ;
 
   }
 
// Now write the Command to the relevant Command Queue
 
 if ( rc == 0)                       //Now write to the Queue
   {
    memcpy(&md,  &md_default,  sizeof(MQMD2))            ;
    md.MsgType = MQMT_REQUEST                            ;
    memcpy(md.Format, MQFMT_ADMIN, sizeof(MQCHAR8))      ;
    memcpy(md.ReplyToQ, rod.ObjectName, MQ_Q_NAME_LENGTH)            ;
    memcpy(md.ReplyToQMgr, qm, MQ_Q_MGR_NAME_LENGTH)     ;
 
    memcpy(&pmo, &pmo_default, sizeof(MQPMO))            ;
    pmo.Options = MQPMO_NO_SYNCPOINT         +
                  MQPMO_DEFAULT_CONTEXT      + //Needed to bluff PCF Security!
                  MQPMO_FAIL_IF_QUIESCING    ;
 
    TRACE(traceid, ("Now issuing the MQPUT to the Command Queue\n") ) ;
    MQPUT ( qmh, cQh, &md, &pmo, pcflen, pcfarea, &mqrc, &mqac )      ;
    TRACE(traceid, ("MQPUT rc = %"PRId32"\n",(int32_t)mqrc) )         ;
    if ( mqrc != 0 ) rc = -14                                         ;
   }
 
 
 //Read the ReplyToQ, and display the contained messages
 
 if ( rc == 0 )                      // Read Q and Display
   {
    TRACE(traceid, ("Now starting to obtain the ReplyToQ messages\n"             ) ) ;
    TRACE(traceid, ("The ReplyToQ name is %.*s\n",MQ_Q_NAME_LENGTH,rod.ObjectName) ) ;
 
    for ( recno = 1 ; ; recno++ )
      {
       char     exiter = 'N' ;
 
       memcpy(&gmo, &gmo_default, sizeof(MQGMO))       ;
       gmo.Options = MQGMO_NO_SYNCPOINT         +
                     MQGMO_WAIT                 +
                     MQGMO_FAIL_IF_QUIESCING    ;
       gmo.WaitInterval =    to                        ;  // 5 sec
 
       while ( rc == 0 )
         {
          memcpy(&md,  &md_default,  sizeof(MQMD2))             ;
          memset(buffer, 0, bufflen)                            ;
          TRACE(traceid, ("Issuing a MQGET to the ReplyToQ\n") )                ;
          MQGET ( qmh, rQh, &md, &gmo, bufflen, buffer, &reclen, &mqrc, &mqac ) ;
          TRACE(traceid, ("MQGET rc = %"PRId32", ac = %"PRId32", Datalen = %"PRId32"\n",
                (int32_t)mqrc,(int32_t)mqac,(int32_t)reclen) )  ;

          if ( mqac == MQRC_TRUNCATED_MSG_FAILED )
            {
             if ( reclen <= bufflen )
               {
                rc = -15 ;
                break;
               }
             newbuffer = realloc(buffer,(size_t)reclen) ;
             if ( newbuffer == NULL )
               {
                mqac = errno                                               ;
                TRACE(traceid, ("malloc rc = %"PRId32"\n",(int32_t)mqac) ) ;
                rc = -10                                                   ;
                break;
               }
             buffer  = newbuffer   ;
             bufflen = (int)reclen ;
             mqrc = 0              ;
             mqac = 0              ;
             continue;
            }
          break;
         }

       if ( rc != 0 )
         exiter = 'G' ;
       else
       switch ( mqrc ) //Print obtained message
         {
          case MQCC_OK      :
            if (    (reclen < MQCFH_STRUC_LENGTH)
                 || (reclen > bufflen) )
              {
               rc = -18     ;
               exiter = 'G' ;
               break        ;
              }

            bufpcf1 = (MQCFH *)buffer ;

            if (    (bufpcf1->StrucLength != MQCFH_STRUC_LENGTH)
                 || (bufpcf1->ParameterCount < 0) )
              {
               rc = -18     ;
               exiter = 'G' ;
               break        ;
              }

            remaining = reclen - MQCFH_STRUC_LENGTH ;
            bufpcf2i  = (MQCFIN *)&((char *)buffer)[MQCFH_STRUC_LENGTH] ; //Point to
            bufpcf2s  = (MQCFST *)&((char *)buffer)[MQCFH_STRUC_LENGTH] ; //Buffer
            bufpcf2il = (MQCFIL *)&((char *)buffer)[MQCFH_STRUC_LENGTH] ; //Structures
            bufpcf2sl = (MQCFSL *)&((char *)buffer)[MQCFH_STRUC_LENGTH] ;

            TRACE(traceid, ("Message %"PRId32" = MQ Message %"PRId32" %s has MQ rc = %"PRId32", %"PRId32"\n",
                 (int32_t)recno,(int32_t)bufpcf1->MsgSeqNumber,
                 (bufpcf1->Control == MQCFC_LAST) ? " (last)" : " (more)",
                 (int32_t)bufpcf1->Reason,(int32_t)bufpcf1->ParameterCount) ) ;
 
            if ( bufpcf1->ParameterCount != 0 ) //Do not attempt to ignore null messages
              {                                // when printing the results
               MQLONG   parms           ;
               int      i               ;
               int      j               ;
               MQLONG   parmtype        ;
               MQLONG   parmsize        ;
               MQBYTE * sp              ;
               char     resvar[100] ;
               parms = bufpcf1->ParameterCount ;
 
               for (i=0 ; i < parms ; i++ )
                {
                 if ( remaining < (MQLONG)(3 * sizeof(MQLONG)) )
                   {
                    rc = -18 ;
                    break;
                   }
                 parmtype = bufpcf2i->Type        ; //All types share a
                 parmsize = bufpcf2i->StrucLength ; // common prefix
                 if (    (parmsize < (MQLONG)(3 * sizeof(MQLONG)))
                      || (parmsize > remaining)
                      || ((parmsize % (MQLONG)sizeof(MQLONG)) != 0) )
                   {
                    rc = -18 ;
                    break;
                   }
                 switch ( parmtype )
                   {
                    case MQCFT_INTEGER  : //Convert to attr=value
                      if ( parmsize < MQCFIN_STRUC_LENGTH )
                        rc = -18 ;
                      else
                        TRACE(traceid, (" Integer parm %"PRId32" ->%"PRId32"<-\n",
                              (int32_t)bufpcf2i->Parameter,(int32_t)bufpcf2i->Value) ) ;
                      break        ;
 
                    case MQCFT_INTEGER_LIST : //Convert to attr=value pairs
                      if ( parmsize < MQCFIL_STRUC_LENGTH_FIXED )
                        rc = -18 ;
                      else
                      if (    (bufpcf2il->Count < 0)
                           || (bufpcf2il->Count >
                              (parmsize - MQCFIL_STRUC_LENGTH_FIXED) /
                              (MQLONG)sizeof(MQLONG)) )
                        rc = -18 ;
                      else
                      if ( bufpcf2il->Count != 0 )
                        {
                         TRACE(traceid, (" Integer parm values = %"PRId32"\n",(int32_t)bufpcf2il->Parameter) ) ;
                         for ( j=0 ; j < bufpcf2il->Count ; j++ )
                           {
                            TRACE(traceid, ("%"PRId32,(int32_t)bufpcf2il->Values[j]) ) ;
                           }
                        }
                      break        ;
 
                    case MQCFT_STRING : //Print in the Stem variable
                      if (    (parmsize < MQCFST_STRUC_LENGTH_FIXED)
                           || (bufpcf2s->StringLength < 0)
                           || (bufpcf2s->StringLength >
                               parmsize - MQCFST_STRUC_LENGTH_FIXED) )
                        rc = -18 ;
                      else
                        {
                         outrec++ ;
                         rexxrc = stem_from_long  (traceid, NULL, RX_response, "0" , outrec) ;
                         if (    (rexxrc != RXSHV_OK)
                              && (rexxrc != RXSHV_NEWV)
                              && (   (rexxrcOutput == RXSHV_OK)
                                  || (rexxrcOutput == RXSHV_NEWV)) )
                           rexxrcOutput = rexxrc ;
                         sprintf(resvar,"%"PRId32, (int32_t)outrec)                ;
                         rexxrc = stem_from_bytes(traceid, NULL, RX_response, resvar,
                                        (MQBYTE *)&(bufpcf2s->String) , bufpcf2s->StringLength) ;
                         if (    (rexxrc != RXSHV_OK)
                              && (rexxrc != RXSHV_NEWV)
                              && (   (rexxrcOutput == RXSHV_OK)
                                  || (rexxrcOutput == RXSHV_NEWV)) )
                           rexxrcOutput = rexxrc ;
                        }
                      break        ;
 
                    case MQCFT_STRING_LIST : //Print Strings in the Stem Variable
                      if (    (parmsize < MQCFSL_STRUC_LENGTH_FIXED)
                           || (bufpcf2sl->Count < 0)
                           || (bufpcf2sl->StringLength < 0) )
                        rc = -18 ;
                      else
                      if (    (bufpcf2sl->StringLength != 0)
                           && (bufpcf2sl->Count >
                               (parmsize - MQCFSL_STRUC_LENGTH_FIXED) /
                               bufpcf2sl->StringLength) )
                        rc = -18 ;
                      else
                      if (    (bufpcf2sl->StringLength == 0)
                           && (bufpcf2sl->Count >
                               (MQLONG)RXMQ_MAX_ZERO_LENGTH_STRING_LIST_COUNT) )
                        rc = -18 ;
                      else
                      if ( bufpcf2sl->Count != 0 )
                        {
                         for ( j=0 ; j < bufpcf2sl->Count ; j++ )
                          {
                           sp = ((MQBYTE *)bufpcf2sl)
                              + MQCFSL_STRUC_LENGTH_FIXED
                              + (bufpcf2sl->StringLength * j) ;
                           outrec++ ;
                           rexxrc = stem_from_long  (traceid, NULL, RX_response, "0" , outrec)   ;
                           if (    (rexxrc != RXSHV_OK)
                                && (rexxrc != RXSHV_NEWV)
                                && (   (rexxrcOutput == RXSHV_OK)
                                    || (rexxrcOutput == RXSHV_NEWV)) )
                             rexxrcOutput = rexxrc ;
                           sprintf(resvar,"%"PRId32, (int32_t)outrec)                  ;
                           rexxrc = stem_from_bytes(traceid, NULL, RX_response, resvar ,
                                          sp, bufpcf2sl->StringLength);
                           if (    (rexxrc != RXSHV_OK)
                                && (rexxrc != RXSHV_NEWV)
                                && (   (rexxrcOutput == RXSHV_OK)
                                    || (rexxrcOutput == RXSHV_NEWV)) )
                             rexxrcOutput = rexxrc ;
                          }
                        }
                      break        ;
                    default : break ;
                   }
 
                 if ( rc != 0 ) break ;

                 remaining -= parmsize ;
                 bufpcf2i  = (MQCFIN *)( (char *)bufpcf2i  + parmsize ) ; //Bump
                 bufpcf2s  = (MQCFST *)( (char *)bufpcf2s  + parmsize ) ; // up
                 bufpcf2il = (MQCFIL *)( (char *)bufpcf2il + parmsize ) ; // structure
                 bufpcf2sl = (MQCFSL *)( (char *)bufpcf2sl + parmsize ) ; // pointers
 
                }  //End of Parameter Printing loop
              }  //End of Parameter Listing

            if ( (rc == 0) && (remaining != 0) )
              rc = -18 ;
 
            if ( rc != 0 )
              exiter = 'G' ;
            else
            if ( bufpcf1->Control == MQCFC_LAST) exiter = 'L' ; //Last means just that!
            break          ;
 
          default             :
            TRACE(traceid, ("MQGET on ReplyToQ error rc = %"PRId32", ac = %"PRId32" on message %"PRId32"\n",
                  (int32_t)mqrc,(int32_t)mqac,(int32_t)recno) ) ;
            rc = -15      ;
            exiter = 'G'  ;
            break         ;
           }
 
         if ( exiter != 'N' ) break ; //Get next message
                                      //unless error/EOQ occured
        } // End of Message Processing FOR loop
    } // End of Queue/Queue Processing Block
 
 TRACE(traceid, ("All ReplyToQ records obtained\n") ) ;
 
//End of processing
 
 TRACE(traceid, ("Closing the Command Q\n") ) ; //Close the Command Queue
 if (cQh) MQCLOSE ( qmh, &cQh, MQCO_NONE, &dummy, &dummy ) ;
 
 TRACE(traceid, ("Closing the ReplyToQ\n") ) ; //Close/Delete the ReplyToQ
 if (rQh) MQCLOSE ( qmh, &rQh, MQCO_DELETE_PURGE, &dummy, &dummy ) ;
 
 if ( DisconnectFinally )
   {
    TRACE(traceid, ("Disconnecting from QM\n") ) ; //Disconnect from QM
    MQDISC ( &qmh, &dummy, &dummy ) ;
   }
 
//
// Free data buffer, if allocated.
//
 if ( buffer != 0 )
   {
    TRACE(traceid, ("Free area\n") ) ;
    free(buffer) ;
   }

 if (    (rc == 0)
      && (rexxrcOutput != RXSHV_OK)
      && (rexxrcOutput != RXSHV_NEWV) )
   {
    rc = -16 ;
   }
//
// Set the function return string
//
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
//
// Return to caller
//
return 0;
 
 } // End of RXMQC function
#endif
 
//
// Perform one of RXMQ operations  RXMQV
//
//   Call:   rc = RXMQV(<RXMQ operation name>, <RXMQ operation's parameters>)
//                   Available operations:
//
//                     INIT     ->  RXMQINIT, performs REXX environment initialization
//                     TERM     ->  RXMQTERM, performs REXX environment termination
//                     CONS     ->  RXMQCONS, setup MQ constants for REXX
//                     CONN     ->  RXMQCONN, connect to QManager
//                     OPEN     ->  RXMQOPEN, open MQ queue
//                     CLOSE    ->  RXMQCLOS, close MQ queue
//                     DISC     ->  RXMQDISC, disconnect from QManager
//                     CMIT     ->  RXMQCMIT, do a syncpoint
//                     BACK     ->  RXMQBACK, do a rollback
//                     PUT      ->  RXMQPUT,  do a MQPUT operation
//                     PUT1     ->  RXMQPUT1, do a MQPUT1 operation
//                     GET      ->  RXMQGET,  do a MQGET operation
//                     INQ      ->  RXMQINQ,  do a MQINQ operation
//                     SET      ->  RXMQSET,  do a MQSET operation
//                     SUB      ->  RXMQSUB,  do a MQSUB operation
//                     BROWSE   ->  RXMQBRWS, do a MQBROWSE operation
//                     HXT      ->  RXMQHXT,  do a Header extract, MQHXT
//                     EVENT    ->  RXMQEVNT, extract the Event Data from the 'message' data
//                     TM       ->  RXMQTM,   process a Trigger Message
//                     MH       ->  RXMQMH,  create a message handle
//                     DMH      ->  RXMQDMH, delete a message handle
//                     SMP      ->  RXMQSMP, set a message property
//                     IMP      ->  RXMQIMP, inquire a message property
//                     DMP      ->  RXMQDMP, delete a message property
//                     BMH      ->  RXMQBMH, convert a buffer to a message handle
//                     MBF      ->  RXMQMBF, convert a message handle to a buffer
//
FTYPE RXMQV  RXMQPARM
{
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = MQV    ;  // This function trace id
 MQULONG                 i                ;  // Looper
 char                    name[9]          ;  // Uppercased function name
 MQULONG                 namelen          ;  // Name length
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null MQ function name"},
        {  -3, "Zero length MQ function name"},
        { -20, "Unknown MQ function name"},
        { -99, "UNKNOWN FAILURE"}} ;
 
 // Check the parms
 if ( (rc == 0) && (aargc == 0 ) )                 rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc = -3 ;
 
 typedef struct
         {
          char * func_name;
          int (APIENTRY *func_ptr) RXMQPARM;
 
 } mqftypes;
 
 mqftypes funclist[] = {
          {"INIT"  , RXMQINIT},
          {"TERM"  , RXMQTERM},
          {"CONS"  , RXMQCONS},
          {"CONN"  , RXMQCONN},
          {"OPEN"  , RXMQOPEN},
          {"CLOSE" , RXMQCLOS},
          {"DISC"  , RXMQDISC},
          {"CMIT"  , RXMQCMIT},
          {"BACK"  , RXMQBACK},
          {"PUT1"  , RXMQPUT1},
          {"PUT"   , RXMQPUT},
          {"GET"   , RXMQGET},
          {"INQ"   , RXMQINQ},
          {"SET"   , RXMQSET},
          {"SUB"   , RXMQSUB},
          {"BROWSE", RXMQBRWS},
          {"HXT"   , RXMQHXT},
          {"EVENT" , RXMQEVNT},
          {"TM"    , RXMQTM},
          {"MH"    , RXMQMH},
          {"DMH"   , RXMQDMH},
          {"SMP"   , RXMQSMP},
          {"IMP"   , RXMQIMP},
          {"DMP"   , RXMQDMP},
          {"BMH"   , RXMQBMH},
          {"MBF"   , RXMQMBF},
          {"?"     , NULL}  };
 
// Uppercase specified function name
 if (rc == 0)
   {
    namelen = (aargv[0].strlength < 8) ? aargv[0].strlength : 8 ;
    for (i = 0; i < namelen; i++)
      name[i] = toupper(aargv[0].strptr[i]);
    name[namelen] = '\0' ;
   }
 
// Find and call appropriate function
 if (rc == 0)  for (i = 0; ; i++)
   {
    if (funclist[i].func_name[0] == '?') break;
    if ( ( strlen(funclist[i].func_name) == namelen ) &&
         ( memcmp(funclist[i].func_name, name, namelen) == 0) )
      return funclist[i].func_ptr(name, aargc-1, &(aargv[1]), aqname, aretstr);
   }
 
 if (rc == 0) rc = -20 ;
 
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
 return 0;
}
 
//
// Perform one of RXMQ Command operations  RXMQVC
//
//   Call:   rc = RXMQVC(<RXMQ Command operation name>, <RXMQ Command operation's parameters>)
//                   Available operations:
//
//                     INIT     ->  RXMQINIT, performs REXX environment initialization
//                     TERM     ->  RXMQTERM, performs REXX environment termination
//                     COMMAND  ->  RXMQC,    implements MQ command line
//
FTYPE RXMQVC  RXMQPARM
{
 MQLONG                  rc = 0           ;  // Function Return Code
 MQLONG                  mqrc = 0         ;  // MQ RC
 MQLONG                  mqac = 0         ;  // MQ AC
 MQULONG                 traceid = MQV    ;  // This function trace id
 MQULONG                 i                ;  // Looper
 char                    name[9]          ;  // Uppercased function name
 MQULONG                 namelen          ;  // Name length
 
 RETMSG ReturnMsg[] = {
        {  -1, "Bad number of parameters" },
        {  -2, "Null MQ function name"},
        {  -3, "Zero length MQ function name"},
        { -20, "Unknown MQ function name"},
        { -99, "UNKNOWN FAILURE"}} ;
 
// Check the parms
 if ( (rc == 0) && (aargc == 0 ) )             rc = -1 ;
 if ( (rc == 0) && RXNULLSTRING(aargv[0]) )    rc = -2 ;
 if ( (rc == 0) && RXZEROLENSTRING(aargv[0]) ) rc = -3 ;
 
 typedef struct
         {
          char * func_name;
          int (APIENTRY *func_ptr) RXMQPARM;
         } mqftypes;
 
 mqftypes funclist[] = {
          {"INIT"   , RXMQINIT},
          {"TERM"   , RXMQTERM},
          {"COMMAND", RXMQC},
          {"?"      , NULL}  };
 
// Uppercase specified function name
 if (rc == 0)
   {
    namelen = (aargv[0].strlength < 8) ? aargv[0].strlength : 8 ;
    for (i = 0; i < namelen; i++)
      name[i] = toupper(aargv[0].strptr[i]);
    name[namelen] = '\0' ;
   }
 
// Find and call appropriate function
 if (rc == 0) for (i = 0; ; i++)
   {
    if (funclist[i].func_name[0] == '?') break;
    if ( ( strlen(funclist[i].func_name) == namelen ) &&
         ( memcmp(funclist[i].func_name, name, namelen) == 0) )
      return funclist[i].func_ptr(name, aargc-1, &(aargv[1]), aqname, aretstr);
    }
 
 if (rc == 0) rc = -20;
 
 set_return(rc,mqrc,mqac,afuncname,ReturnMsg,aretstr,traceid,"") ;
 
 return 0;
}
 
//
// Support for long Windows REXX function names compatible with MA78
//
#ifdef _RXMQN
FTYPE  RXMQNINIT  RXMQPARM
 {
  return RXMQINIT(afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNCONS  RXMQPARM
 {
  return RXMQCONS (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNTERM  RXMQPARM
 {
   return RXMQTERM (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNCONN  RXMQPARM
 {
  return RXMQCONN (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNOPEN  RXMQPARM
 {
  return RXMQOPEN (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNCLOSE  RXMQPARM
 {
  return RXMQCLOS (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNDISC  RXMQPARM
 {
  return RXMQDISC (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNCMIT  RXMQPARM
 {
  return RXMQCMIT (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNBACK  RXMQPARM
 {
  return RXMQBACK (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNPUT  RXMQPARM
 {
  return RXMQPUT (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNPUT1  RXMQPARM
 {
  return RXMQPUT1 (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNGET  RXMQPARM
 {
  return RXMQGET (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNINQ  RXMQPARM
 {
  return RXMQINQ (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNSET  RXMQPARM
 {
  return RXMQSET (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNSUB  RXMQPARM
 {
  return RXMQSUB (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQNMH  RXMQPARM
 {
  return RXMQMH (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQNDMH  RXMQPARM
 {
  return RXMQDMH (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQNSMP  RXMQPARM
 {
  return RXMQSMP (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQNIMP  RXMQPARM
 {
  return RXMQIMP (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQNDMP  RXMQPARM
 {
  return RXMQDMP (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQNBMH  RXMQPARM
 {
  return RXMQBMH (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQNMBF  RXMQPARM
 {
  return RXMQMBF (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQNBROWSE  RXMQPARM
 {
  return RXMQBRWS (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNHXT  RXMQPARM
 {
  return RXMQHXT (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNEVENT  RXMQPARM
 {
  return RXMQEVNT (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNTM  RXMQPARM
 {
  return RXMQTM (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQNC  RXMQPARM
 {
  return RXMQC (afuncname,aargc,aargv,aqname,aretstr);
 }
#endif
 
#ifdef _RXMQT
FTYPE  RXMQTINIT  RXMQPARM
 {
  return RXMQINIT(afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTCONS  RXMQPARM
 {
  return RXMQCONS (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTTERM  RXMQPARM
 {
  return RXMQTERM (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTCONN  RXMQPARM
 {
  return RXMQCONN (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTOPEN  RXMQPARM
 {
  return RXMQOPEN (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTCLOSE  RXMQPARM
 {
  return RXMQCLOS (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTDISC  RXMQPARM
 {
  return RXMQDISC (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTCMIT  RXMQPARM
 {
  return RXMQCMIT (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTBACK  RXMQPARM
 {
  return RXMQBACK (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTPUT  RXMQPARM
 {
  return RXMQPUT (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTPUT1  RXMQPARM
 {
  return RXMQPUT1 (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTGET  RXMQPARM
 {
  return RXMQGET (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTINQ  RXMQPARM
 {
  return RXMQINQ (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTSET  RXMQPARM
 {
  return RXMQSET (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTSUB  RXMQPARM
 {
  return RXMQSUB (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQTMH  RXMQPARM
 {
  return RXMQMH (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQTDMH  RXMQPARM
 {
  return RXMQDMH (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQTSMP  RXMQPARM
 {
  return RXMQSMP (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQTIMP  RXMQPARM
 {
  return RXMQIMP (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQTDMP  RXMQPARM
 {
  return RXMQDMP (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQTBMH  RXMQPARM
 {
  return RXMQBMH (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQTMBF  RXMQPARM
 {
  return RXMQMBF (afuncname,aargc,aargv,aqname,aretstr);
 }
FTYPE  RXMQTBROWSE  RXMQPARM
 {
  return RXMQBRWS (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTHXT  RXMQPARM
 {
  return RXMQHXT (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTEVENT  RXMQPARM
 {
  return RXMQEVNT (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTTM  RXMQPARM
 {
  return RXMQTM (afuncname,aargc,aargv,aqname,aretstr);
 }
 
FTYPE  RXMQTC  RXMQPARM
 {
  return RXMQC (afuncname,aargc,aargv,aqname,aretstr);
 }
#endif
