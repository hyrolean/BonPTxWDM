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

class CPTxWDMCtrlService
{
protected:
	CPTxWDMCmdServiceOperator	Op;
	CSharedTransportStreamer	*St;
	DWORD CmdWait, Timeout;
	HANDLE Thread;
	BOOL ThTerm;

private:
	BOOL TxWriteDone;
	static BOOL __stdcall TxDirectWriteProc(LPVOID dst, DWORD &sz, PVOID arg) {
		auto this_ = static_cast<CPTxWDMCtrlService*>(arg) ;
		return (this_->TxWriteDone = this_->Op.GetStreamData(dst, sz)) ;
	}

	int StreamingThreadProcMain() {
		const DWORD szp = Op.StreamerPacketSize() ;
		bool retry=false;
		while(!ThTerm) {
			if(!retry) {
				if(Op.CurStreamSize()<szp) {Sleep(10);continue;}
			}
			TxWriteDone = FALSE;
			BOOL r = retry ?
				St->TxDirect(NULL, &TxWriteDone, CmdWait):
				St->TxDirect(TxDirectWriteProc, this, CmdWait);
			retry = !r && TxWriteDone ;
		}
		DBGOUT("-- Streaming Done --\n");
		return 0;
	}

	static unsigned int __stdcall StreamingThreadProc (PVOID pv) {
		auto this_ = static_cast<CPTxWDMCtrlService*>(pv) ;
		unsigned int result = this_->StreamingThreadProcMain() ;
		_endthreadex(result) ;
		return result;
	}

protected:
	void StartStreaming() {
		if(Thread != INVALID_HANDLE_VALUE) return /*already activated*/;
		Thread = (HANDLE)_beginthreadex(NULL, 0, StreamingThreadProc, this,
			CREATE_SUSPENDED, NULL) ;
		if(Thread != INVALID_HANDLE_VALUE) {
			if(St) delete St;
			St = new CSharedTransportStreamer( Op.Name()+PTXWDMSTREAMER_SUFFIX, FALSE,
				Op.StreamerPacketSize(), Op.CtrlPackets() );
			DBGOUT("-- Start Streaming --\n");
			::SetThreadPriority( Thread, Op.StreamerThreadPriority() );
			ThTerm=FALSE;
			::ResumeThread(Thread) ;
		}else {
			DBGOUT("*** Streaming thread creation failed. ***\n");
		}
	}

	void StopStreaming() {
		if(Thread == INVALID_HANDLE_VALUE) return /*already inactivated*/;
		ThTerm=TRUE;
		if(::WaitForSingleObject(Thread,Timeout*2) != WAIT_OBJECT_0) {
			::TerminateThread(Thread, 0);
		}
		Thread = INVALID_HANDLE_VALUE ;
		if(St) { delete St; St=NULL; }
		DBGOUT("-- Stop Streaming --\n");
	}

public:
	CPTxWDMCtrlService(wstring name, DWORD cmdwait=INFINITE, DWORD timeout=INFINITE)
	 :	Op( name ), St( NULL ), Thread(INVALID_HANDLE_VALUE), ThTerm(TRUE)
	{ CmdWait = cmdwait ; Timeout = timeout ; }
	~CPTxWDMCtrlService() { StopStreaming(); }

	int MainLoop() {
		BOOL enable_streaming = FALSE ;
		while(!Op.Terminated()) {
			DWORD wait_res = Op.WaitForCmd(CmdWait);
			if(wait_res==WAIT_OBJECT_0) {
				if(!Op.ServiceReaction(Timeout)) {
					DBGOUT("service reaction failed.\n");
					return 1;
				}
				BOOL streaming_enabled = Op.StreamingEnabled();
				if(enable_streaming != streaming_enabled) {
					enable_streaming = streaming_enabled;
					if(enable_streaming)	StartStreaming();
					else					StopStreaming();
				}
			}else if(wait_res==WAIT_TIMEOUT) {
				if(Elapsed(Op.LastAlive())>=Timeout) {
					if(HANDLE mutex = OpenMutex(MUTEX_ALL_ACCESS,FALSE,Op.Name().c_str())) {
						Op.KeepAlive();
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

};

int APIENTRY _tWinMain(HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPTSTR    lpCmdLine,
					 int       nCmdShow)
{
	if (_tcslen(lpCmdLine)<=0) return -1 ;

	wstring name = lpCmdLine ;
	DBGOUT("service name: %s\n",wcs2mbcs(name).c_str());

	return CPTxWDMCtrlService(name, MaxWait, MaxAlive).MainLoop() ;
}

