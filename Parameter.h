#ifndef PARAMETER_H
#define PARAMETER_H

#define TIMEOUT                 20
#define HEARTBEAT_INTERVAL      10 * 1000
#define MAX_DATA_SIZE           4096
#define SECURITY_FUT            "FUT" 
#define SECURITY_OPT            "OPT"
#define LEN_SOCK_ID             4
#define LEN_MSG_TYPE            1
#define LEN_TYPE                4
#define LEN_HOST_NAME           44
#define LEN_SEQNO               8
#define LEN_CHECK_SEQNO         1
#define SOCK_ID                 "sock"
#define MSG_TYPE_LOGON          "L"       
#define MSG_TYPE_HEARTBEAT      "H" 
#define MSG_TYPE_DATA           "D" 
#define ERRMSG_FORMAT_ERROR     "Format Error"
#define ERRMSG_TIMEOUT          "Timeout! no response from server"

#include "UFC.h"

extern UFC::AnsiString              g_asServerIP;
extern UFCType::Int32               g_iServerPort;   
extern UFC::AnsiString              g_asBackupServerIP;
extern UFCType::Int32               g_iBackupServerPort;
extern UFC::AnsiString              g_asExecutionFileURL;
extern UFC::AnsiString              g_asTargetHost;
extern UFC::AnsiString              g_asTargetSecurity;
extern UFC::AnsiString              g_asMBusServerIP;
extern UFCType::Int32               g_iMBusServerPort;
extern UFC::AnsiString              g_asOutputFileURL;
extern BOOL                         g_bRunning;
extern BOOL                         g_bIsCheckSeqNo;

#endif /* PARAMETER_H */

