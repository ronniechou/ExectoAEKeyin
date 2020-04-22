#include <stdio.h>
#include <stdlib.h>
#include "MBusListener.h"
using namespace std;
//------------------------------------------------------------------------------
MBusListener::MBusListener(UFC::AnsiString AppName, UFC::AnsiString MBusIP, UFC::Int32 MBusPort)
:PThread( NULL )
,FAppName(AppName)
,FMBusIP(MBusIP)
,FMBusPort(MBusPort)
{
    FMessageObj = new MessageObject(FAppName, "1.0",  "SystemListener", FMBusPort);
    FMessageObj->SetHost(FMBusIP);
    FMessageObj->SetMonitorListener( this );
}
//------------------------------------------------------------------------------
MBusListener::~MBusListener()
{  
    Terminate();    
    UFC::BufferedLog::Printf( " [%s][%s] delete FMessageObj...",  __FILE__,__FUNCTION__);   
    FMessageObj->Terminate();
    delete FMessageObj;
    FMessageObj = NULL;
    UFC::BufferedLog::Printf( " [%s][%s] delete FMessageObj...done",  __FILE__,__FUNCTION__); 
}
//------------------------------------------------------------------------------
void MBusListener::OnProcessConnected( BOOL IsTheFirstOne )
{
    if( IsTheFirstOne == FALSE )
    {
        UFC::BufferedLog::Printf( "Process [%s] already exists, EXIT.", FAppName.c_str()  );
        g_bRunning = FALSE;
        UFC::SleepMS( 2000 );
        exit( EXIT_SUCCESS );
    }
} 
//------------------------------------------------------------------------------
void MBusListener::OnProcessStartup( const UFC::AnsiString& Host, const UFC::AnsiString& AppName )  
{ 
}
//------------------------------------------------------------------------------
void MBusListener::OnProcessStopped( const UFC::AnsiString& Host, const UFC::AnsiString& AppName )  
{ 
}
//------------------------------------------------------------------------------
void MBusListener::OnProcessList( UFC::PStringList& Processs ) 
{
}
//------------------------------------------------------------------------------
void MBusListener::OnConnected(  )
{
    UFC::BufferedLog::Printf( " [%s][%s] %s is connected to MBus <IP:%s> <Port:%d> ", __FILE__,__FUNCTION__, FAppName.c_str(),FMBusIP.c_str(), FMBusPort);
}
//------------------------------------------------------------------------------
void MBusListener::OnDisconnected( )
{
    UFC::BufferedLog::Printf( " [%s][%s] %s is disconnected to MBus <IP:%s> <Port:%d>", __FILE__,__FUNCTION__, FAppName.c_str(),FMBusIP.c_str(), FMBusPort);
}
//------------------------------------------------------------------------------
void MBusListener::OnMigoMessage( const UFC::AnsiString& Subject, const UFC::AnsiString& Key,  MTree* pTree )
{
    UFC::BufferedLog::Printf( " [%s][%s] Subject=%s,Key=%s",  __FILE__,__FUNCTION__,Subject.c_str(),Key.c_str());
    FQueue.Inqueue( new MTree(*pTree) );
}
//------------------------------------------------------------------------------
void MBusListener::Execute( void )
{
    UFC::BufferedLog::Printf( " [%s][%s] star to DeQueue",  __FILE__,__FUNCTION__); 
    UFC::Int32 FlagTick = UFC::GetTickCountMS();
 
    while ( ! IsTerminated()  )
    {     
       //ProcessRequest(NULL);
       //UFC::SleepMS(5000);
        
        
        MTree* pMTree = FQueue.Dequeue( 1 );        
        if(pMTree == NULL)
            continue;        
        //----
        ProcessRequest(pMTree);
        
        delete pMTree;
        pMTree = NULL;   
        
    }
}
//------------------------------------------------------------------------------
void MBusListener::StartService()
{
    UFC::BufferedLog::Printf(" [%s][%s] %s is try to connect MBus %s:%d",__FILE__,__FUNCTION__, FAppName.c_str(), FMBusIP.c_str(), FMBusPort);
    FMessageObj->Start(); ///< Start Migo Message pump.
    FMessageObj->WaitForConnected();
    UFC::SleepMS(500);
    
    this->Start();  // trigger Execute())
}
//------------------------------------------------------------------------------
void MBusListener::ProcessRequest(MTree *pMTree)
{ 
    int iSeqNo = 0;
    UFC::FileStreamEx File(g_asOutputFileURL.c_str(),"a+");
    UFC::AnsiString asLine,asData;
    
    try
    {
        while(true)
        { 
            if( File.ReadLine(asLine) )
            {
UFC::BufferedLog::Printf(" [%s][%s] line=%s", __FILE__,__FUNCTION__,asLine.c_str()); 
                iSeqNo = asLine.ToInt() + 1;
            }
            else
            {
UFC::BufferedLog::Printf(" [%s][%s] line=%s,      eof", __FILE__,__FUNCTION__,asLine.c_str()); 
                break;
            }
        }
        asData.Printf("%d\n",iSeqNo);
        File.Write(asData.c_str(),asData.Length());
        File.Flush();        
    }
    catch( UFC::Exception &e )
    {
        UFC::BufferedLog::Printf(" [%s][%s] UFC exception occurred <Reason:%s>\n", __FILE__,__func__, e.what() ); 
    }
    catch(...)
    {
        UFC::BufferedLog::Printf(" [%s][%s] Unknown exception occurred\n", __FILE__,__func__ ); 
    }
}
//------------------------------------------------------------------------------



