// PTxWDMCtrl.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include <process.h>
#include <string>

#include "PTxWDMCtrl.h"
#include "../Common/PTxWDMCmdSrv.h"

using namespace std;

#define DEFAULT_MAXALIVE	30000 // 30秒
#define DEFAULT_MAXWAIT		5000 // 5秒

DWORD MaxWait = DEFAULT_MAXWAIT ;
DWORD MaxAlive = DEFAULT_MAXALIVE ;

int MainLoop(wstring name);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	if (_tcslen(lpCmdLine)<=0) return -1 ;

	wstring name = lpCmdLine ;
    DBGOUT("service name: %s\n",wcs2mbcs(name).c_str());

	return MainLoop(name) ;
}

class CPTxWDMService
{
private:
	CPTxWDMCmdServiceOperator	Op;
	CPTxWDMStreamer				*St;
	CAsyncFifo					Fifo;
	DWORD Timeout;
	HANDLE Thread;
	BOOL ThTerm;
private:
	int StreamingThreadProcMain() {
		DWORD pos=0;
		BUFFER<BYTE> buf(PTXWDMSTREAMER_PACKETSIZE);
		while(!ThTerm) {
			DWORD sz = Op.CurStreamSize();
			if(!sz) {Sleep(10);continue;}
			if(sz>buf.size()) sz=(DWORD)buf.size();
			if(Op.GetStreamData(buf.data()+pos,sz)) {
				if(Fifo.Push(buf.data(),sz,false)) {
					BYTE *data; DWORD len,rem;
					if(Fifo.Pop(&data,&len,&rem))
						St->Tx(data,len,Timeout);
				}
			}
		}
		DBGOUT("-- Streaming Done --\n");
		return 0;
	}
	static unsigned int __stdcall StreamingThreadProc (PVOID pv) {
		auto this_ = static_cast<CPTxWDMService*>(pv) ;
		unsigned int result = this_->StreamingThreadProcMain() ;
		_endthreadex(result) ;
		return result;
	}
public:
	CPTxWDMService(wstring name, DWORD timeout=INFINITE)
	 :	Op( name ), Thread(INVALID_HANDLE_VALUE), ThTerm(TRUE),
		St( NULL ), Fifo(2,2,0,PTXWDMSTREAMER_PACKETSIZE)
	{ Timeout = timeout ; }
	~CPTxWDMService() { StopStreaming(); }
	CPTxWDMCmdServiceOperator	*Operator()	{ return &Op; }
	CPTxWDMStreamer				*Streamer()	{ return St; }
	void StartStreaming() {
		if(Thread != INVALID_HANDLE_VALUE) return /*active*/;
		Thread = (HANDLE)_beginthreadex(NULL, 0, StreamingThreadProc, this,
			CREATE_SUSPENDED, NULL) ;
		if(Thread != INVALID_HANDLE_VALUE) {
			if(St) delete St;
			St = new CPTxWDMStreamer( Op.Name()+PTXWDMSTREAMER_SUFFIX, FALSE,
				PTXWDMSTREAMER_PACKETSIZE, Op.CtrlPackets() );
			DBGOUT("-- Start Streaming --\n");
			::SetThreadPriority( Thread, Op.StreamerThreadPriority() );
			ThTerm=FALSE;
			::ResumeThread(Thread) ;
		}else {
			DBGOUT("*** Streaming thread creation failed. ***\n");
		}
	}
	void StopStreaming() {
		if(Thread == INVALID_HANDLE_VALUE) return /*inactive*/;
		ThTerm=TRUE;
		if(::WaitForSingleObject(Thread,Timeout*2) != WAIT_OBJECT_0) {
			::TerminateThread(Thread, 0);
		}
		Thread = INVALID_HANDLE_VALUE ;
		Fifo.Purge();
		if(St) { delete St; St=NULL; }
		DBGOUT("-- Stop Streaming --\n");
	}
};

int MainLoop(wstring name)
{
	CPTxWDMService service(name, MaxAlive);

	BOOL enable_streaming = FALSE ;

	while(!service.Operator()->Terminated()) {

		DWORD wait_res = service.Operator()->WaitForCmd(MaxWait);

		if(wait_res==WAIT_OBJECT_0) {

			if(!service.Operator()->ServiceReaction(MaxAlive)) {
				DBGOUT("service reaction failed.\n");
				return 1;
			}

			if(enable_streaming != service.Operator()->StreamingEnabled()) {
				enable_streaming = service.Operator()->StreamingEnabled() ;
				if(enable_streaming)	service.StartStreaming();
				else					service.StopStreaming();
			}

		}else if(wait_res==WAIT_TIMEOUT) {

			if(Elapsed(service.Operator()->LastAlive())>=MaxAlive) {
				if(HANDLE mutex = OpenMutex(MUTEX_ALL_ACCESS,FALSE,name.c_str())) {
					service.Operator()->KeepAlive();
					CloseHandle(mutex);
				}else {
					DBGOUT("service timeout.\n");
					return -1;
				}
			}

		}

	}

	DBGOUT("service terminated.\n");
	return 0;
}

