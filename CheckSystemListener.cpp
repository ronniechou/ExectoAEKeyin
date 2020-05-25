#include <stdio.h>
#include <stdlib.h>
#include "CheckSystemListener.h"
#include "NameValueMessage.h"
using namespace std;

//------------------------------------------------------------------------------
CheckSystemListener::CheckSystemListener()
: PThread( NULL )
, FSeqNo( 0 )
, FHaveSentSeqNo ( -1 )
, FConnectFailCount ( 0 )
, FThread_HeartBeatChecker()
{  
    FIsLogon = TRUE;
    FSendTick = 0;
    FIsSendingHeartBeat = FALSE;
    FIsSendingExecutionData = FALSE;
    FIsCheckSeqNo = g_bIsCheckSeqNo;
    FCurrentServerIP = g_asServerIP;
    FCurrentServerPort = g_iServerPort;    
    FSocketObject = new UFC::PClientSocket(FCurrentServerIP,FCurrentServerPort); 
    FSocketObject->SetListener(this);
    FSocketObject->Start();

    //---- 啟動 HeartBeat 執行緒
    int iResult = pthread_create(&FThread_HeartBeatChecker, NULL ,&CheckSystemListener::HeartBeatChecker , NULL);
    if(iResult!=0)	
        UFC::BufferedLog::Printf ("create thread for HeartBeatChecker()  error!\n");	
}
//------------------------------------------------------------------------------
CheckSystemListener::~CheckSystemListener()
{
    Terminate();   
    pthread_join(FThread_HeartBeatChecker, NULL); // 等待HeartBeat執行緒執行完成   
    UFC::SleepMS(1000); // to avoid core dump.
    
    if(FSocketObject != NULL)
    {
        UFC::BufferedLog::Printf(" [%s][%s]  delete FSocketObject...", __FILE__,__FUNCTION__);
        delete FSocketObject;
        FSocketObject = NULL;
        UFC::BufferedLog::Printf(" [%s][%s]  delete FSocketObject...done", __FILE__,__FUNCTION__);
    }
}
//------------------------------------------------------------------------------
void CheckSystemListener::OnConnect( UFC::PClientSocket *Socket)
{    
    UFC::BufferedLog::Printf( " [%s][%s] connect to %s:%d succeed.", __FILE__,__func__,FSocketObject->GetPeerIPAddress().c_str(),FSocketObject->GetPort());     
    return;
}
//------------------------------------------------------------------------------
void CheckSystemListener::OnDisconnect( UFC::PClientSocket *Socket, BOOL NeedReconnect )
{
    UFC::BufferedLog::Printf( " [%s][%s] OnDisconnect(%d)!", __FILE__,__func__, NeedReconnect);
    FIsLogon = FALSE;
    return;
}
//------------------------------------------------------------------------------
BOOL CheckSystemListener::OnDataArrived( UFC::PClientSocket *Socket)
{
    UFC::UInt8      rcvb[MAX_DATA_SIZE];
    int iRecvSize   = 0;
    memset(rcvb,0,sizeof(rcvb));
    iRecvSize = Socket->RecvBuffer( rcvb,sizeof(rcvb) );
    ProcessResponse(iRecvSize,rcvb);
    return true;
}
//------------------------------------------------------------------------------
void CheckSystemListener::OnIdle( UFC::PClientSocket *Socket)
{
    UFC::BufferedLog::Printf( " [%s][%s] OnIdle!", __FILE__,__func__);
    return;
}
//------------------------------------------------------------------------------
void CheckSystemListener::Execute( void )
{    
    while( !IsTerminated() && g_bRunning )
    { 
        try
        {
            if(FSocketObject->IsConnect() == FALSE)
            {  
                try
                {
                    //---- 連續斷線3次則 連線另一台server
                    if(FConnectFailCount > 0 && FConnectFailCount == 3)
                    {
                        if(FCurrentServerIP.AnsiCompare(g_asServerIP) == 0)
                        {
                            FCurrentServerIP = g_asBackupServerIP;
                            FCurrentServerPort = g_iBackupServerPort;                            
                        }
                        else
                        {
                            FCurrentServerIP = g_asServerIP;
                            FCurrentServerPort = g_iServerPort;               
                        }                        
                        FConnectFailCount = 0;
                    }
                    UFC::BufferedLog::Printf( " [%s][%s] start to connect server <%s:%d>", __FILE__,__func__,FCurrentServerIP.c_str(),FCurrentServerPort);
                    ConnectServer(FCurrentServerIP,FCurrentServerPort);                    
                }
                catch( UFC::Exception &e )
                {
                    FConnectFailCount ++;
                    UFC::BufferedLog::Printf(" [%s][%s] UFC exception occurred <FailLogonCount:%d> <Reason:%s> ", __FILE__,__func__,FConnectFailCount, e.what() );  
                    UFC::SleepMS(10*1000);
                    continue;
                } 
                catch(...)
                {
                    FConnectFailCount ++;
                    UFC::BufferedLog::Printf(" [%s][%s] Unknown exception occurred", __FILE__,__func__ );
                    UFC::SleepMS(10*1000);
                    continue;
                }
                FConnectFailCount = 0;
            }
            else
            {  
                if(FIsLogon == FALSE)
                {
                    UFC::BufferedLog::DebugPrintf( " [%s][%s] start to logon", __FILE__,__func__);
                    Logon();
                }
                else
                {                    
                    //if( ExecutionDataChecker() == FALSE)
                    ExecutionDataChecker();
                    UFC::SleepMS(10*1000);
                }
            }            
        }
        catch( UFC::Exception &e )
        {
            UFC::BufferedLog::Printf(" [%s][%s] UFC exception occurred <Reason:%s>", __FILE__,__func__, e.what() );            
        }
        catch(...)
        {
            UFC::BufferedLog::Printf(" [%s][%s] Unknown exception occurred", __FILE__,__func__ );
        }
        UFC::SleepMS(1*1000);
    }
}
//------------------------------------------------------------------------------
void CheckSystemListener::Run( void )
{
    Start();
}
//------------------------------------------------------------------------------
void CheckSystemListener::ConnectServer( void )
{
    FIsLogon = FALSE;
    FSeqNo = 0;
    FSocketObject->Connect(TIMEOUT);    
}
//------------------------------------------------------------------------------
void CheckSystemListener::ConnectServer( UFC::AnsiString IP, int Port)
{
    FIsLogon = FALSE;
    FSeqNo = 0;    
    FSocketObject->Connect(IP,Port,TIMEOUT);
}
//------------------------------------------------------------------------------
BOOL CheckSystemListener::Logon( void )
{
    UFC::AnsiString asData = "" ;
    int iDataLen = LEN_SOCK_ID + LEN_MSG_TYPE + LEN_HOST_NAME + LEN_TYPE + LEN_CHECK_SEQNO;
    char caBuffer[iDataLen + 1];
    memset(caBuffer,'\x0A',sizeof(caBuffer) );
    //asData.Printf("%s%s%s ",SOCK_ID,MSG_TYPE_LOGON,g_asTargetSecurity.c_str());
    asData.Printf("%s%s%-44s%-4s%d",SOCK_ID,MSG_TYPE_LOGON,g_asTargetHost.c_str(),g_asTargetSecurity.c_str(),FIsCheckSeqNo);
    memcpy(caBuffer,asData.c_str() ,iDataLen );
    
    //---- 傳送data
    FEventLogon.ResetEvent();       
    try
    {               
        UFC::BufferedLog::Printf(" [%s][%s] send data=%s", __FILE__,__func__, asData.c_str() );     
        FSocketObject->BlockSend(caBuffer,iDataLen);
    }
    catch( UFC::Exception &e )
    {
        UFC::BufferedLog::Printf(" [%s][%s] UFC exception occurred <Reason:%s>\n", __FILE__,__func__, e.what() );            
        throw e;
    }
    catch(...)
    {
        UFC::BufferedLog::Printf(" [%s][%s] Unknown exception occurred\n", __FILE__,__func__ );            
        throw FALSE;
    }
    //---- 等待 Server回傳 SeqNo
    if( FEventLogon.WaitFor( TIMEOUT ) == FALSE ) 
    {
        UFC::BufferedLog::Printf( " [%s][%s] %s <IP:%s><Port:%d>", __FILE__,__func__,ERRMSG_TIMEOUT,FSocketObject->GetPeerIPAddress().c_str(),FSocketObject->GetPort());
        FSocketObject->Disconnect();
        return FALSE;
    }
    FIsLogon = TRUE;
    FSendTick = UFC::GetTickCountMS();
    FEventLogon.ResetEvent();    
    return TRUE;
}
//------------------------------------------------------------------------------
void* CheckSystemListener::HeartBeatChecker( void *ptr )
{        
    while(g_bRunning)
    {       
        try
        {
            if(FSocketObject->IsConnect() == FALSE)
            {                
                UFC::SleepMS( 1000 );
                continue;
            }            
            if(FIsLogon == FALSE)
            {                
                UFC::SleepMS( 1000 );
                continue;
            }            
            unsigned long iCurrentTick = UFC::GetTickCountMS();
            if( iCurrentTick - FSendTick >= HEARTBEAT_INTERVAL )
            {
                //----等待 send Execution Data 完成
                while(FIsSendingExecutionData)
                    UFC::SleepMS( 1000 );
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                FIsSendingHeartBeat = TRUE;    
                UFC::BufferedLog::DebugPrintf( " [%s][%s]  start to send heartBeat", __FILE__,__func__);
                SendHeartBeat();
            }
        }
        catch( UFC::Exception &e )
        {
            UFC::BufferedLog::Printf(" [%s][%s] UFC exception occurred <Reason:%s>", __FILE__,__func__, e.what() );            
        }
        catch(...)
        {
            UFC::BufferedLog::Printf(" [%s][%s] Unknown exception occurred", __FILE__,__func__ );
        }
        FIsSendingHeartBeat = FALSE;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++        
        UFC::SleepMS( 1000 );
    } 
    pthread_exit(NULL);
    return NULL;
}
//------------------------------------------------------------------------------
BOOL CheckSystemListener::SendHeartBeat()
{
    UFC::AnsiString asData = "" ;
    int iDataLen = LEN_SOCK_ID + LEN_MSG_TYPE;
    char caBuffer[iDataLen + 1];
    memset(caBuffer,'\x0A',sizeof(caBuffer) );
    asData.Printf("%s%s",SOCK_ID,MSG_TYPE_HEARTBEAT);
    memcpy(caBuffer,asData.c_str() ,iDataLen );
    
    //---- 傳送data
    unsigned long iCurrentTick = UFC::GetTickCountMS();
    FEventHeartBeat.ResetEvent();               
    try
    {
        UFC::BufferedLog::DebugPrintf(" [%s][%s] send data=%s", __FILE__,__func__, caBuffer);               
        FSocketObject->BlockSend(caBuffer,iDataLen);
    }
    catch( UFC::Exception &e )
    {
        UFC::BufferedLog::Printf(" [%s][%s] UFC exception occurred <Reason:%s>\n", __FILE__,__func__, e.what() );            
        throw e;
    }
    catch(...)
    {
        UFC::BufferedLog::Printf(" [%s][%s] Unknown exception occurred\n", __FILE__,__func__ );            
        throw FALSE;
    }
    //----等待Server回應ack
    if( FEventHeartBeat.WaitFor( TIMEOUT ) == FALSE ) 
    {
        UFC::BufferedLog::Printf( " [%s][%s] %s <IP:%s><Port:%d>", __FILE__,__func__,ERRMSG_TIMEOUT,FSocketObject->GetPeerIPAddress().c_str(),FSocketObject->GetPort());  
        FSocketObject->Disconnect();
        return FALSE;
    }  
    FEventHeartBeat.ResetEvent();    
    FSendTick = iCurrentTick;
    return TRUE;
}
//------------------------------------------------------------------------------
BOOL CheckSystemListener::ExecutionDataChecker()
{
    if( !UFC::FileExists(g_asExecutionFileURL.c_str()) )
    {
        UFC::BufferedLog::DebugPrintf(" [%s][%s] Execution file NOT exist. <FileName:%s>", __FILE__,__FUNCTION__,g_asExecutionFileURL.c_str());
        return FALSE;
    }
    UFC::FileStream File(g_asExecutionFileURL.c_str(),0);
    UFC::BufferedLog::DebugPrintf(" [%s][%s] file size=%d", __FILE__,__FUNCTION__, (int) File.GetSize());
    UFC::AnsiString asLine,asHost,asKey;
    int iCurrentSeqNo = 0;
    BOOL bIsSendOK = TRUE;
    
    while(true)
    {
        asLine = File.ReadLine();
        if( asLine.c_str() == NULL)
            break;
        UFC::SleepNS(100);
        
        UFC::PStringList StrList;
        asLine.TrimRight();
        asLine.TrimLeft();
        StrList.SetStrings(asLine,"|");
       
        if( StrList.ItemCount() < 8 )
        {
            UFC::BufferedLog::Printf(" [%s][%s] %s,text format error of Execution <line:%s>", __FILE__,__FUNCTION__,ERRMSG_FORMAT_ERROR,asLine.c_str());
            continue;
        }
        iCurrentSeqNo = StrList[3].ToInt();
        asHost = StrList[7];
        asKey = StrList[4];

        //---- 過濾不符主機
        if( asHost != g_asTargetHost )
        {
            UFC::BufferedLog::DebugPrintf(" [%s][%s] Filter data! Host!=TargetHost  <Host=%s> <TargetHost=%s> <CurrentSeq=%d> <FSeq=%d>", __FILE__,__FUNCTION__,asHost.c_str(),g_asTargetHost.c_str(),iCurrentSeqNo,FSeqNo);      
            continue;
        }
        //---- 過濾已傳資料   FSeqNo:Server要的序號  CurrentSeqNo:目前讀到檔案的序號   
        if(FIsCheckSeqNo)
        {
            if( iCurrentSeqNo < FSeqNo)
            {
                UFC::BufferedLog::DebugPrintf(" [%s][%s] Filter data! CurrentSeqNo < FSeqNo  <Host=%s> <TargetHost=%s> <CurrentSeq=%d> <FSeq=%d>", __FILE__,__FUNCTION__,asHost.c_str(),g_asTargetHost.c_str(),iCurrentSeqNo,FSeqNo);  
                continue;
            }
        }
        else
        {
            if( iCurrentSeqNo < FSeqNo || iCurrentSeqNo <= FHaveSentSeqNo)
            {
                UFC::BufferedLog::DebugPrintf(" [%s][%s] Filter data! CurrentSeqNo < FSeqNo  <CurrentSeq=%d> <FSeq=%d> <HaveSentSeqNo=%d>", __FILE__,__FUNCTION__,iCurrentSeqNo,FSeqNo,FHaveSentSeqNo);  
                continue;
            }
        }            
        //---- 重組Key值 = "AE,GDD+UDD"
        if( UpdateKey(&asKey) )
        {
            int idx1 = StrList[0].Length() + 1 + StrList[1].Length() + 1 + StrList[2].Length() + 1 + StrList[3].Length() + 1;
            int idx2 = idx1 + StrList[4].Length();
            UFC::AnsiString asTemp = asLine;  
            asLine.Printf("%s%s%s",asTemp.SubString(0,idx1).c_str(),asKey.c_str(), asTemp.SubString(idx2,asLine.Length() - idx2).c_str() );
        }
        UFC::AnsiString asData = "" ;
        int iDataLen = LEN_SOCK_ID + LEN_MSG_TYPE + asLine.Length();
        char caBuffer[MAX_DATA_SIZE];

        if( iDataLen + 1 > MAX_DATA_SIZE )
        {                
            UFC::BufferedLog::Printf(" [%s][%s] %s,Execution data size over limit <line:%s> <size:%d> <limit:%d>", __FILE__,__FUNCTION__,ERRMSG_FORMAT_ERROR,asLine.c_str(),iDataLen,MAX_DATA_SIZE);
            iDataLen = MAX_DATA_SIZE -1 ;
        }
          
        asData.Printf("%s%s%s",SOCK_ID,MSG_TYPE_DATA,asLine.c_str());        
        memset(caBuffer,'\x0A',sizeof(caBuffer) );             
        memcpy(caBuffer, asData.c_str() ,iDataLen );        
        
        //----等待 HeartBeat 傳送完成
        while(FIsSendingHeartBeat)        
            UFC::SleepMS( 1000 );
        
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        //---- 傳送data
        FIsSendingExecutionData = TRUE;
        UFC::BufferedLog::Printf(" [%s][%s] CurrentSeqNo=%d,ServerSeqNo=%d,len=%d,Data=%s", __FILE__,__FUNCTION__,iCurrentSeqNo,FSeqNo,iDataLen,asData.c_str()); 
        try
        {            
            bIsSendOK = SendExecutionData(caBuffer,iDataLen + 1);
        }
        catch( UFC::Exception &e )
        {
            bIsSendOK = FALSE;
            UFC::BufferedLog::Printf(" [%s][%s] UFC exception occurred <Reason:%s>", __FILE__,__func__, e.what() );            
        }
        catch(...)
        {
            bIsSendOK = FALSE;
            UFC::BufferedLog::Printf(" [%s][%s] Unknown exception occurred", __FILE__,__func__ );
        }
        FIsSendingExecutionData = FALSE;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        
        //---- 若傳送失敗,離開迴圈
        if(!bIsSendOK)
            break;
        FHaveSentSeqNo = iCurrentSeqNo;
        
        //---- 若有漏資料,直接關檔,重新判斷 (下一筆資料SeqNo 不是 Server 要的SeqNo)
        if(FIsCheckSeqNo)
        {
            if(iCurrentSeqNo +1 != FSeqNo)
            {
                UFC::BufferedLog::Printf("[%s][%s] Server expect SeqNo not match Client next SeqNo <Client next SeqNo:%d><Server expect SeqNo:%d>\n", __FILE__,__func__, iCurrentSeqNo+1, FSeqNo);     
                bIsSendOK = FALSE;
                break;
            }
        }
    }
    return bIsSendOK;
}
//------------------------------------------------------------------------------
BOOL CheckSystemListener::SendExecutionData(char* ExecutionData,int DataLen)
{
    unsigned long iCurrentTick = UFC::GetTickCountMS();
    FEventExecutionData.ResetEvent(); 
    try
    {        
        FSocketObject->BlockSend(ExecutionData,DataLen); 
    }
    catch( UFC::Exception &e )
    {
        UFC::BufferedLog::Printf(" [%s][%s] UFC exception occurred <Reason:%s>\n", __FILE__,__func__, e.what() );            
        throw e;
    }
    catch(...)
    {
        UFC::BufferedLog::Printf(" [%s][%s] Unknown exception occurred\n", __FILE__,__func__ );            
        throw FALSE;
    }
    //----等待Server回應ack
    if( FEventExecutionData.WaitFor( TIMEOUT ) == FALSE ) 
    {
        UFC::BufferedLog::Printf( " [%s][%s] %s <IP:%s><Port:%d>", __FILE__,__func__,ERRMSG_TIMEOUT,FSocketObject->GetPeerIPAddress().c_str(),FSocketObject->GetPort());  
        FSocketObject->Disconnect();
        return FALSE;
    } 
    FEventExecutionData.ResetEvent();
    FSendTick = iCurrentTick;
    return TRUE;
}
//------------------------------------------------------------------------------
BOOL CheckSystemListener::UpdateKey(UFC::AnsiString *pKey)
{
    UFC::AnsiString asBuff;
    UFC::AnsiString asAE="",asGDD="",asUDD="",asTemp="";
    int idx1,idx2;
    bool bUpdateOK = false;
    asBuff = *pKey;
    
    //---- ,前面為AE
    idx1 = asBuff.AnsiPos(",");
    if( idx1 > 0 ) 
        asAE = asBuff.SubString(0,idx1);
    
    //----是否為標準 GDD+UDD 
    //----[<GDD=#0D0x788888660000P0092001545N83                       10.21.125.137                           ^UDD=TC4A41DDAEE344FE99C005574C0F833E7,0^(null)>]   
    //----[<GDD=#0D0x788888660000P0092001545N83                       10.21.125.137                           ^(null)>]
    idx1 = asBuff.AnsiPos("[<GDD=");
    idx2 = asBuff.AnsiPos(">]");
    if(idx1 >=0 && idx2 >7)
    { 
        UFC::NameValueMessage NameValues( "^" );
        NameValues.FromString( asBuff.SubString(idx1+2, idx2-idx1-2) );        
        NameValues.Get( "GDD", asGDD );
        NameValues.Get( "UDD", asUDD );
        asUDD.PadThis(32,' ',true);
        bUpdateOK = true;
    }    
    //----若非為標準 => 找第一個出現 '#'  當作GDD的開始
    if(!bUpdateOK)
    {  
        idx1 = asBuff.AnsiPos("#");
        //idx2 = asBuff.AnsiPos("0x");
        if(idx1 >= 0)
        {  
            asTemp = asBuff.SubString(idx1,asBuff.Length() - idx1); 
            asGDD = asTemp.SubString(0,94);
            asUDD = asTemp.SubString(94,32);
            asGDD.PadThis(94,' ',true);
            asUDD.PadThis(32,' ',true);
            bUpdateOK = true;
        }        
    }    
    if(bUpdateOK)
    {
        if(asAE.IsEmpty())            
            pKey->Printf(",%s%s",asGDD.c_str(),asUDD.c_str());
        else
            pKey->Printf("%s,%s%s",asAE.c_str(),asGDD.c_str(),asUDD.c_str());
    }
    return bUpdateOK;
}
//------------------------------------------------------------------------------
void CheckSystemListener::ProcessResponse(int iRecvSize,UFC::UInt8 *rcvb)
{
    UFC::AnsiString asData = (char*) rcvb;
    if( iRecvSize < LEN_SOCK_ID + LEN_MSG_TYPE )
    {
        UFC::BufferedLog::Printf(" [%s][%s] %s <rcvb:%s>", __FILE__,__FUNCTION__,ERRMSG_FORMAT_ERROR,asData.c_str());
        return;
    }
    UFC::AnsiString asMsgType = asData.SubString(LEN_SOCK_ID,LEN_MSG_TYPE);
    
    ///> triggle event to release next process for ProcessCARequest() 
    if( asMsgType == MSG_TYPE_LOGON )
    {
        //if( iRecvSize < LEN_SOCK_ID + LEN_MSG_TYPE + LEN_TYPE + LEN_SEQNO )
        if( iRecvSize < LEN_SOCK_ID + LEN_MSG_TYPE + LEN_HOST_NAME + LEN_TYPE + LEN_SEQNO )    
        {
            UFC::BufferedLog::Printf(" [%s][%s] %s <rcvb:%s>", __FILE__,__FUNCTION__,ERRMSG_FORMAT_ERROR,asData.c_str());
            return;
        }
        UFC::BufferedLog::Printf( " [%s][%s] receive logon reply=%s", __FILE__,__func__,rcvb);
        //UFC::AnsiString asSeqNo = asData.SubString(LEN_SOCK_ID + LEN_MSG_TYPE + LEN_TYPE ,LEN_SEQNO);
        UFC::AnsiString asSeqNo = asData.SubString(LEN_SOCK_ID + LEN_MSG_TYPE + LEN_HOST_NAME + LEN_TYPE ,LEN_SEQNO);
        FSeqNo = asSeqNo.ToInt();
        FEventLogon.SetEvent(); 
    }
    else if( asMsgType == MSG_TYPE_HEARTBEAT )
    {
        UFC::BufferedLog::DebugPrintf( " [%s][%s] receive data=%s", __FILE__,__func__,rcvb);
       
        //---- 送出資料,收到Server返回的ACK (含下一筆的 SeqNo)
        if(iRecvSize >= LEN_SOCK_ID + LEN_MSG_TYPE + LEN_SEQNO )
        {
            UFC::AnsiString asSeqNo = asData.SubString(LEN_SOCK_ID + LEN_MSG_TYPE,LEN_SEQNO);
            FSeqNo = asSeqNo.ToInt();
            UFC::BufferedLog::Printf( " [%s][%s] receive ack=%s <Next SeqNo:%d>", __FILE__,__func__,rcvb,FSeqNo);  
            FEventExecutionData.SetEvent();
        }
        else            
            FEventHeartBeat.SetEvent(); 
    }
}
//------------------------------------------------------------------------------
