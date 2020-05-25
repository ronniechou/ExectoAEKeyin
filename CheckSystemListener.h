#ifndef CHECKSYSTEMLISTENER_H
#define CHECKSYSTEMLISTENER_H
//------------------------------------------------------------------------------
#include "Parameter.h"
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class CheckSystemListener : public UFC::PThread, public UFC::SocketClientListener
{
protected:    
    static UFC::PClientSocket      *FSocketObject; 
    static BOOL                     FIsLogon;
    BOOL                            FIsCheckSeqNo;
    int                             FSeqNo;
    int                             FHaveSentSeqNo;
    int                             FConnectFailCount;
    static unsigned long            FSendTick;
    UFC::PEvent                     FEventLogon;
    static UFC::PEvent              FEventHeartBeat;
    static UFC::PEvent              FEventExecutionData;
    UFC::AnsiString                 FCurrentServerIP;
    UFCType::Int32                  FCurrentServerPort;
    pthread_t                       FThread_HeartBeatChecker;
    static BOOL                     FIsSendingHeartBeat;
    static BOOL                     FIsSendingExecutionData;
public:    
    CheckSystemListener();
    virtual ~CheckSystemListener();
protected:  ///Implement interface SocketClientListener
    void OnConnect( UFC::PClientSocket *Socket);
    void OnDisconnect( UFC::PClientSocket *Socket, BOOL NeedReconnect = FALSE );
    BOOL OnDataArrived( UFC::PClientSocket *Socket);
    void OnIdle( UFC::PClientSocket *Socket);   
protected: /// Implement interface PThread
    void Execute( void );
public:
    void Run( void );   
    void ConnectServer();
    void ConnectServer( UFC::AnsiString IP, int Port);
    BOOL Logon();
    static void* HeartBeatChecker(void *);
    static BOOL SendHeartBeat();
    BOOL ExecutionDataChecker();
    BOOL SendExecutionData(char*,int);
    BOOL UpdateKey(UFC::AnsiString *Key);
    void ProcessResponse(int,UFC::UInt8*);
};
//------------------------------------------------------------------------------
#endif /* CHECKSYSTEMLISTENER_H */

