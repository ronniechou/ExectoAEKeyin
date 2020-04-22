#ifndef MBUSLISTENER_H
#define MBUSLISTENER_H
//------------------------------------------------------------------------------
#include "Parameter.h"
#include "Sigo.h"
typedef UFC::PtrQueue<MTree>  TMemoryQueue;
//------------------------------------------------------------------------------
class MBusListener : public UFC::PThread,public MonitorListener, public MessageListener
{
private:
    UFC::AnsiString    FAppName;
    MessageObject     *FMessageObj;
    UFC::AnsiString    FMBusIP ;
    UFC::Int32         FMBusPort;
    TMemoryQueue       FQueue;
    
public:
    MBusListener(UFC::AnsiString AppName, UFC::AnsiString MBusIP, UFC::Int32 MBusPort);
    virtual ~MBusListener();
    
protected:/// implement interface MonitorListener
    virtual void OnProcessStartup( const UFC::AnsiString& Host, const UFC::AnsiString& AppName ) ;
    virtual void OnProcessStopped( const UFC::AnsiString& Host, const UFC::AnsiString& AppName ) ;
    virtual void OnProcessConnected( BOOL IsTheFirstOne );
    virtual void OnProcessList( UFC::PStringList& Processs ) ;
    virtual void OnConnected( void );
    virtual void OnDisconnected( void );

private:///implement interface MessageListener
    virtual void OnMigoMessage( const UFC::AnsiString& Subject, const UFC::AnsiString& Key, MTree* pMTree );
    
protected: /// Implement interface PThread
    void Execute( void );    

public:
    void StartService();
    void ProcessRequest(MTree *pMTree);
};
//------------------------------------------------------------------------------
#endif /* MBUSLISTENER_H */

