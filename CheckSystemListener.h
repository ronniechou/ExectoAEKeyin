#ifndef CHECKSYSTEMLISTENER_H
#define CHECKSYSTEMLISTENER_H
//------------------------------------------------------------------------------
#include "Parameter.h"
//------------------------------------------------------------------------------
class CheckSystemListener : public UFC::PThread, public UFC::SocketClientListener
{
protected:    
    UFC::PClientSocket      *FSocketObject; 
    BOOL                    FIsLogon;
    BOOL                    FIsCheckSeqNo;
    int                     FSeqNo;
    int                     FHaveSentSeqNo;
    int                     FConnectFailCount;
    unsigned long           FSendTick;
    UFC::PEvent             FEventLogon;
    UFC::PEvent             FEventHeartBeat;
    UFC::AnsiString         FCurrentServerIP;
    UFCType::Int32          FCurrentServerPort;
public:    
    CheckSystemListener();
    virtual ~CheckSystemListener();
protected:  ///Implement interface SocketClientListener
    void OnConnect( UFC::PClientSocket *Socket);
    void OnDisconnect( UFC::PClientSocket *Socket);
    BOOL OnDataArrived( UFC::PClientSocket *Socket);
    void OnIdle( UFC::PClientSocket *Socket);   
protected: /// Implement interface PThread
    void Execute( void );     
public:
    void Run( void );   
    void ConnectServer();
    void ConnectServer( UFC::AnsiString IP, int Port);
    BOOL Logon();
    BOOL SendHeartBeat();
    BOOL SendExecutionData();
    BOOL UpdateKey(UFC::AnsiString *Key);
    void ProcessResponse(int,UFC::UInt8*);
};
//------------------------------------------------------------------------------
#endif /* CHECKSYSTEMLISTENER_H */

