#include <signal.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "CheckSystemListener.h"
#include "MBusListener.h"
#include "iniFile.h"
#include "NameValueMessage.h"
using namespace std;
//------------------------------------------------------------------------------
UFC::AnsiString             g_asAppName                 = "";
UFC::AnsiString             g_asArgv                    = "";
UFC::AnsiString             g_asLogDate                 = "";
UFC::AnsiString             g_asChineseLogDate          = "";
UFC::AnsiString             g_asLogPath                 = ""; 
UFC::AnsiString             g_asLogPrefixName           = "";  
UFC::AnsiString             g_asLogURL                  = ""; 
UFC::AnsiString             g_asConfigURL               = "../cfg/"APPNAME".ini";
UFC::AnsiString             g_asExecutionFilePrefix     = "";
UFC::AnsiString             g_asExecutionFileExtension  = ".DATA";
UFC::AnsiString             g_asExecutionFileURL        = "";
UFC::AnsiString             g_asTargetSecurity          = SECURITY_FUT;
UFC::AnsiString             g_asTargetHost              = UFC::Hostname;
UFC::AnsiString             g_asServerIP                = "";                         
UFCType::Int32              g_iServerPort               = -1; 
UFC::AnsiString             g_asBackupServerIP          = "";                         
UFCType::Int32              g_iBackupServerPort         = -1; 
BOOL                        g_bIsCheckSeqNo             = TRUE;

UFC::AnsiString             g_asMBusServerIP            = "127.0.0.1";
UFCType::Int32              g_iMBusServerPort           = 12345;
UFC::AnsiString             g_asListenSecurity          = SECURITY_FUT;
UFC::AnsiString             g_asOutputFilePrefix        = "";
UFC::AnsiString             g_asOutputFileExtension     = ".DATA";
UFC::AnsiString             g_asOutputFileURL           = "";

UFC::Int32                  g_iDEBUG_LEVEL              = 0;  
BOOL                        g_bRunning                  = TRUE;
BOOL                        g_bFirstRun                 = FALSE;
CheckSystemListener*        FAPPListener                = NULL;   
MBusListener*               FMBusListener               = NULL;  
UFC::AnsiString             FExecutionFilePrefix_FUT    = "";
UFC::AnsiString             FExecutionFilePrefix_OPT    = "";
UFC::AnsiString             FExecutionFileExtension_FUT = ".DATA";
UFC::AnsiString             FExecutionFileExtension_OPT = ".DATA";
//------------------------------------------------------------------------------
BOOL                        CheckSystemListener::FIsLogon;
unsigned long               CheckSystemListener::FSendTick;
UFC::PClientSocket          *CheckSystemListener::FSocketObject;
UFC::PEvent                 CheckSystemListener::FEventHeartBeat;
UFC::PEvent                 CheckSystemListener::FEventExecutionData;
BOOL                        CheckSystemListener::FIsSendingHeartBeat;
BOOL                        CheckSystemListener::FIsSendingExecutionData;
//------------------------------------------------------------------------------
void AtStart( void );
void AtSignal( int signum );
void AtExit( void );
void StopObjects( void );
//------------------------------------------------------------------------------
///> set signal handlers
void AtStart( void )
{
    atexit( AtExit );
    signal( SIGINT,  AtSignal );
    signal( SIGQUIT, AtSignal );
    signal( SIGTERM, AtSignal );
    signal( SIGHUP,  AtSignal );
    signal( SIGKILL, AtSignal );
    signal( SIGQUIT, AtSignal );
    signal( SIGABRT, AtSignal );
    signal( SIGFPE, AtSignal );
    signal( SIGILL, AtSignal );
//    signal( SIGSEGV, AtSignal );  // capture core dump event
}
//------------------------------------------------------------------------------
void AtSignal( int signum )
{
    UFC::BufferedLog::Printf(" [%s][%s] Got signal=%d", __FILE__,__FUNCTION__,signum);
    g_bRunning = FALSE;     
    if(signum == 2) //crtl+c
        exit(0);
}
//------------------------------------------------------------------------------
void AtExit( void )
{
    g_bRunning = FALSE;
    UFC::BufferedLog::Printf(" [%s][%s]", __FILE__,__FUNCTION__);
    UFC::BufferedLog::FlushToFile();
    StopObjects();
}
//------------------------------------------------------------------------------
void StopObjects( void )
{
    try
    {
        /*
        if ( FAPPListener != NULL)
        {            
            delete FAPPListener; 
            FAPPListener = NULL;
        }
        */ 
    }catch(...){
        UFC::BufferedLog::Printf(" [%s][%s] exception happen", __FILE__,__FUNCTION__);
    }
}
//------------------------------------------------------------------------------
void GetLogDate()
{
    UFC::GetYYYYMMDD(g_asLogDate,FALSE); 
    UFC::GetTradeYYYMMDD(g_asChineseLogDate);    
}
//------------------------------------------------------------------------------
void ParseArg(int argc, char** argv)
{
    for(int i=1;i<argc;i++)
    {
        UFC::AnsiString asArgv = argv[i];
        g_asArgv.AppendPrintf("%s ",asArgv.c_str());
        
        if( asArgv.AnsiCompare("-F")  == 0 )
            g_bFirstRun = TRUE;
        else if( asArgv.SubString(0,5).LowerCase().AnsiCompare("-cfg=") == 0  )
        {
            UFC::NameValueMessage NV_ConfigLine("|");
            NV_ConfigLine.FromString( asArgv ); 
            if( NV_ConfigLine.IsExists("-cfg") )
                NV_ConfigLine.Get("-cfg", g_asConfigURL);
        }
        else if( asArgv.AnsiCompare("-d")  == 0 )
            g_iDEBUG_LEVEL = 1;
        else if( asArgv.SubString(0,6).LowerCase().AnsiCompare("-date=") == 0  )
        {        
            UFC::NameValueMessage NV_ConfigLine("|");
            NV_ConfigLine.FromString( asArgv ); 
            if( NV_ConfigLine.IsExists("-date") )
                NV_ConfigLine.Get("-date", g_asChineseLogDate);          
        }      
        else if( asArgv.AnsiCompare("-FUT")  == 0 )
            g_asTargetSecurity = SECURITY_FUT;
        else if( asArgv.AnsiCompare("-OPT")  == 0 )
            g_asTargetSecurity = SECURITY_OPT;
    }
}
//------------------------------------------------------------------------------
void ParseConfig()
{
    if( !UFC::FileExists(g_asConfigURL) )
    {
        UFC::BufferedLog::Printf(" [%s][%s] config=%s NOT exit\n", __FILE__,__func__,g_asConfigURL.c_str() );
        return;
    }
    UFC::UiniFile Config(g_asConfigURL);

/*    
printf(" [%s][%s] SectionCount=%d\n", __FILE__,__func__,Config.SectionCount());  
UFC::Section*  HostSection;
for( int i = 0; i < Config.SectionCount(); i++ )
{
    HostSection = Config.GetSection( i );
    ///< Find a HA Speedy Server, and create sync executions thread.
    printf(" [%s][%s] i=%d,sectionName=%s\n", __FILE__,__func__,i,HostSection->GetSectionName().c_str());
    
    if( HostSection->GetSectionName() == "CoverIP" )
    {
        UFC::AnsiString asValue;
        HostSection->GetValue("ip",asValue);
        printf("ip=%s,",asValue.c_str());
        HostSection->GetValue("host",asValue);
        printf("host=%s\n",asValue.c_str());
    }
}
*/
    UFC::AnsiString asValue;
    if( Config.GetValue( "Setting", "TargetHost",asValue ) == TRUE )    
        g_asTargetHost = asValue;
    if( Config.GetValue( "Setting", "ServerIP",asValue ) == TRUE )    
        g_asServerIP = asValue;
    if( Config.GetValue( "Setting", "ServerPort",asValue ) == TRUE )    
        g_iServerPort = asValue.ToInt();
    if( Config.GetValue( "Setting", "BackupServerIP",asValue ) == TRUE )    
        g_asBackupServerIP = asValue;
    if( Config.GetValue( "Setting", "BackupServerPort",asValue ) == TRUE )    
        g_iBackupServerPort = asValue.ToInt();    
    if( Config.GetValue( "Setting", "CheckSeqNo",asValue ) == TRUE )
    {
        if(asValue.UpperCase().AnsiCompare("N") == 0)
            g_bIsCheckSeqNo = FALSE;
        else
            g_bIsCheckSeqNo = TRUE;
    }   
    if( Config.GetValue( "Setting", "ExecutionFilePrefix_FUT",asValue ) == TRUE )    
        FExecutionFilePrefix_FUT = asValue;
    if( Config.GetValue( "Setting", "ExecutionFileExtension_FUT",asValue ) == TRUE )    
        FExecutionFileExtension_FUT = asValue;    
    if( Config.GetValue( "Setting", "ExecutionFilePrefix_OPT",asValue ) == TRUE )    
        FExecutionFilePrefix_OPT = asValue;
    if( Config.GetValue( "Setting", "ExecutionFileExtension_OPT",asValue ) == TRUE )    
        FExecutionFileExtension_OPT = asValue;  
    
    if( Config.GetValue( "MBus", "AppName",asValue ) == TRUE )    
        g_asAppName = asValue;
    if( Config.GetValue( "MBus", "MBusServerIP",asValue ) == TRUE )    
        g_asMBusServerIP = asValue;
    if( Config.GetValue( "MBus", "MBusServerPort",asValue ) == TRUE )    
        g_iMBusServerPort = asValue.ToInt();
    if( Config.GetValue( "MBus", "OutputFilePrefix",asValue ) == TRUE )    
        g_asOutputFilePrefix = asValue;
    if( Config.GetValue( "MBus", "OutputFileExtension",asValue ) == TRUE )    
        g_asOutputFileExtension = asValue;

/*
     if( Config.GetValue( "Setting", "ExecutionFileExtension",asValue ) == TRUE )    
        g_asExecutionFileExtension = asValue;
    if( Config.GetValue( "Setting", "ExecutionFilePrefix",asValue ) == TRUE )    
        g_asExecutionFilePrefix = asValue;
    if( Config.GetValue( "MBus", "ListenSecurity",asValue ) == TRUE )    
        g_asListenSecurity = asValue;
    if( g_asExecutionFileURL.AnsiPos(SECURITY_OPT) >=0 )
        g_asTargetSecurity = SECURITY_OPT; 
    else
        g_asTargetSecurity = SECURITY_FUT;
*/  
}
//------------------------------------------------------------------------------
void UpdateVariable()
{
    //---- AppName
    if(g_asAppName.Length() == 0)
        g_asAppName.Printf("%s.%s",APPNAME,g_asTargetSecurity.c_str());
    
    //---- server IP
    if(g_asBackupServerIP.Length() == 0)
        g_asBackupServerIP = g_asServerIP;
    if(g_iBackupServerPort < 0)
        g_iBackupServerPort = g_iServerPort;
    
    //---- ExecutionFile    /oms/Speedy/bin/SpeedyFUT.Execution.    
    if(FExecutionFilePrefix_FUT.Length() == 0)
        FExecutionFilePrefix_FUT.Printf("/oms/Speedy/bin/Speedy%s.Execution.",SECURITY_FUT);
    if(FExecutionFilePrefix_OPT.Length() == 0)
        FExecutionFilePrefix_OPT.Printf("/oms/Speedy/bin/Speedy%s.Execution.",SECURITY_OPT);
    if(g_asTargetSecurity == SECURITY_FUT)
    {
        g_asExecutionFilePrefix = FExecutionFilePrefix_FUT;
        g_asExecutionFileExtension = FExecutionFileExtension_FUT;
    }
    else
    {
        g_asExecutionFilePrefix = FExecutionFilePrefix_OPT;
        g_asExecutionFileExtension = FExecutionFileExtension_OPT;
    } 
    g_asExecutionFileURL.Printf("%s%s%s",g_asExecutionFilePrefix.c_str(),g_asChineseLogDate.c_str(),g_asExecutionFileExtension.c_str());
    
    //---- Log Path
    g_asLogPath.Printf("%s/../log/",UFC::GetCurrentDir().c_str());
    g_asLogPrefixName.Printf("%s.%s",APPNAME,g_asTargetSecurity.c_str());
    g_asLogURL.Printf("%s%s.%s.log",g_asLogPath.c_str(),g_asLogPrefixName.c_str(),g_asLogDate.c_str());
    
    //---- MBUS Output File  
    g_asListenSecurity = g_asTargetSecurity;
    if( g_asOutputFilePrefix.Length() == 0)
        g_asOutputFilePrefix.Printf("../log/%s.%s.Execution.",APPNAME,g_asListenSecurity.c_str());
    g_asOutputFileURL.Printf("%s%s%s",g_asOutputFilePrefix.c_str(),g_asChineseLogDate.c_str(),g_asOutputFileExtension.c_str());    
}
//------------------------------------------------------------------------------
void CreateLogObject()
{  
    try
    {
        if( UFC::FileExists(g_asLogPath) == false)           
            mkdir(g_asLogPath,S_IRWXU);        
        UFC::BufferedLog::SetLogObject( new UFC::BufferedLog( g_asLogURL.c_str() ,10240,true,true) ); 
        UFC::BufferedLog::SetDebugMode( g_iDEBUG_LEVEL );
        UFC::BufferedLog::SetPrintToStdout( TRUE );   
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
void PrintStartUp( void )
{ 
    // Print startup screen
    UFC::BufferedLog::Printf( " ____________________________________________" );
    UFC::BufferedLog::Printf( " " );
    UFC::BufferedLog::Printf( "    Yuanta %s", g_asAppName.c_str());
    UFC::BufferedLog::Printf( "    Startup on %s at %s ", UFC::Hostname, UFC::GetDateString().c_str() );
    UFC::BufferedLog::Printf( " ");
    UFC::BufferedLog::Printf( "    Ver : 1.1.0 Build Date:%s       ",  __DATE__ );
    UFC::BufferedLog::Printf( "    [%d bits version]               ",  sizeof(void*)*8 );
    UFC::BufferedLog::Printf( "    Argv               : %s         ",  g_asArgv.c_str()  ); 
    UFC::BufferedLog::Printf( "    FirstRun           : %s         ",  g_bFirstRun?"Y":"N"  );    
    UFC::BufferedLog::Printf( "    Debug Level        : %d         ",  g_iDEBUG_LEVEL  );   
    
    UFC::BufferedLog::Printf( "    cfg URL            : %s         ",  g_asConfigURL.c_str() );
    UFC::BufferedLog::Printf( "    Log URL            : %s         ",  g_asLogURL.c_str() ); 
    UFC::BufferedLog::Printf( "    ExecutionFileURL   : %s         ",  g_asExecutionFileURL.c_str() ); 
    UFC::BufferedLog::Printf( "    TargetSecurity     : %s         ",  g_asTargetSecurity.c_str() ); 
    UFC::BufferedLog::Printf( "    TargetHost         : %s         ",  g_asTargetHost.c_str() ); 
    UFC::BufferedLog::Printf( "    ServerIP           : %s         ",  g_asServerIP.c_str() ); 
    UFC::BufferedLog::Printf( "    ServerPort         : %d         ",  g_iServerPort ); 
    UFC::BufferedLog::Printf( "    Backup ServerIP    : %s         ",  g_asBackupServerIP.c_str() ); 
    UFC::BufferedLog::Printf( "    Backup ServerPort  : %d         ",  g_iBackupServerPort ); 
    UFC::BufferedLog::Printf( "    CheckSeqNo         : %s         ",  g_bIsCheckSeqNo?"Y":"N" ); 
    UFC::BufferedLog::Printf( " ");
    UFC::BufferedLog::Printf( "    MBusServerIP       : %s         ",  g_asMBusServerIP.c_str() ); 
    UFC::BufferedLog::Printf( "    MBusServerPort     : %d         ",  g_iMBusServerPort ); 
    UFC::BufferedLog::Printf( "    ListenSecurity     : %s         ",  g_asListenSecurity.c_str() ); 
    UFC::BufferedLog::Printf( "    OutputFileURL      : %s         ",  g_asOutputFileURL.c_str() );     
    UFC::BufferedLog::Printf( " ");
    UFC::BufferedLog::Printf( " ____________________________________________" );
}
//------------------------------------------------------------------------------
void Initialize( void )
{ 
    if( FAPPListener == NULL)
        FAPPListener = new CheckSystemListener();
    if( FMBusListener == NULL )
        FMBusListener = new MBusListener(g_asAppName,g_asMBusServerIP,g_iMBusServerPort);
}
//------------------------------------------------------------------------------
int main(int argc, char** argv) 
{  
    GetLogDate();
    ParseArg(argc,argv);
    ParseConfig(); 
    UpdateVariable();
    CreateLogObject();
    AtStart();
    PrintStartUp();
    Initialize();
    
    //---- 連線 MBus server
    FMBusListener->StartService(); 
    
    //---- 連線 socket server & 傳送資料
    FAPPListener->Run();  
    
    //int ronnie= 0;
    while ( g_bRunning )
    {  
        UFC::SleepMS( 5*1000 );
        /*
        ronnie++;        
        if(ronnie == 1)
        {
            delete FMBusListener;          
        }
        */
    }
    UFC::BufferedLog::Printf(" [%s][%s] Daemon job interrupted!", __FILE__, __func__ );
    return 0;
}
//------------------------------------------------------------------------------
