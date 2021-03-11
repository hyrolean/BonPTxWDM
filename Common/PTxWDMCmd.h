//===========================================================================
#pragma once
#ifndef _PTXWDMCMD_20210301140612558_H_INCLUDED_
#define _PTXWDMCMD_20210301140612558_H_INCLUDED_
//---------------------------------------------------------------------------

#include "SharedCmd.h"
//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------

#define	PTXWDMCMDMAXDATA	8
#define	PTXWDMCMDTIMEOUT	30000

enum PTXWDMCMD : DWORD {
	PTXWDMCMD_TERMINATE,
	PTXWDMCMD_OPEN_TUNER,
	PTXWDMCMD_CLOSE_TUNER,
	PTXWDMCMD_GET_TUNER_COUNT,
	PTXWDMCMD_SET_TUNER_SLEEP,
	PTXWDMCMD_SET_STREAM_ENABLE,
	PTXWDMCMD_IS_STREAM_ENABLED,
	PTXWDMCMD_SET_CHANNEL,
	PTXWDMCMD_SET_FREQ,
	PTXWDMCMD_GET_IDLIST_S,
	PTXWDMCMD_GET_ID_S,
	PTXWDMCMD_SET_ID_S,
	PTXWDMCMD_SET_LNB_POWER,
	PTXWDMCMD_GET_CN_AGC,
	PTXWDMCMD_PURGE_STREAM,
	PTXWDMCMD_SETUP_SERVER,
};

struct TSIDLIST {
	DWORD Id[8];
};

struct SERVER_SETTINGS {
  DWORD dwSize;
  DWORD CtrlPackets;
  int StreamerThreadPriority;
  DWORD MAXDUR_FREQ;
  DWORD MAXDUR_TMCC;
  DWORD MAXDUR_TSID;
  DWORD StreamerPacketSize;
};

  // CPTxWDMCmdOperator (PTxWDM Command Operator)

class CPTxWDMCmdOperator : public CSharedCmdOperator
{
protected:
	bool ServerMode;
	struct OP {
		DWORD id;
		union {
			PTXWDMCMD cmd;
			BOOL res;
		};
		DWORD data[PTXWDMCMDMAXDATA];
	};
	DWORD StaticId;
	HANDLE HXferMutex;
	bool XferLock(DWORD timeout=PTXWDMCMDTIMEOUT) const ;
	bool XferUnlock() const ;
	BOOL Xfer(OP &cmd, OP &res, DWORD timeout=PTXWDMCMDTIMEOUT);
	CPTxWDMCmdOperator *UniServer;
public:
	CPTxWDMCmdOperator(std::wstring name, bool server=false);
	virtual ~CPTxWDMCmdOperator();
public:
	// for Client Operations
	BOOL CmdTerminate(DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdOpenTuner(BOOL Sate, DWORD TunerID, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdCloseTuner(DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdGetTunerCount(DWORD &Count, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdSetTunerSleep(BOOL Sleep, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdSetStreamEnable(BOOL Enable, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdIsStreamEnabled(BOOL &Enable, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdSetChannel(BOOL &Tuned, DWORD Freq, DWORD TSID, DWORD Stream, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdSetFreq(DWORD Freq, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdGetIdListS(TSIDLIST &TSIDList, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdGetIdS(DWORD &TSID, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdSetIdS(DWORD TSID, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdSetLnbPower(BOOL Power, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdGetCnAgc(DWORD &Cn100, DWORD &CurAgc, DWORD &MaxAgc, DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdPurgeStream(DWORD timeout=PTXWDMCMDTIMEOUT);
	BOOL CmdSetupServer(const SERVER_SETTINGS *Options, DWORD timeout=PTXWDMCMDTIMEOUT);
protected:
	// for Server Operations
	virtual BOOL ResTerminate()	{ return FALSE ; }
	virtual BOOL ResOpenTuner(BOOL Sate, DWORD TunerID)	{ return FALSE ; }
	virtual BOOL ResCloseTuner()	{ return FALSE ; }
	virtual BOOL ResGetTunerCount(DWORD &Count)	{ return FALSE ; }
	virtual BOOL ResSetTunerSleep(BOOL Sleep)	{ return FALSE ; }
	virtual BOOL ResSetStreamEnable(BOOL Enable)	{ return FALSE ; }
	virtual BOOL ResIsStreamEnabled(BOOL &Enable)	{ return FALSE ; }
	virtual BOOL ResSetChannel(BOOL &Tuned, DWORD Freq, DWORD TSID, DWORD Stream)	{ return FALSE ; }
	virtual BOOL ResSetFreq(DWORD Freq)	{ return FALSE ; }
	virtual BOOL ResGetIdListS(TSIDLIST &TSIDList)	{ return FALSE ; }
	virtual BOOL ResGetIdS(DWORD &TSID)	{ return FALSE ; }
	virtual BOOL ResSetIdS(DWORD TSID)	{ return FALSE ; }
	virtual BOOL ResSetLnbPower(BOOL Power)	{ return FALSE ; }
	virtual BOOL ResGetCnAgc(DWORD &Cn100, DWORD &CurAgc, DWORD &MaxAgc)	{ return FALSE ; }
	virtual BOOL ResPurgeStream()	{ return FALSE ; }
	virtual BOOL ResSetupServer(const SERVER_SETTINGS *Options)	{ return FALSE ; }
public:
	// Service Reaction ( It will be called from the server after Listen() )
	BOOL ServiceReaction(DWORD timeout=PTXWDMCMDTIMEOUT);
	// Uniqulize ( Unify the server & client flows of command operations )
	BOOL Uniqulize(CPTxWDMCmdOperator *server);
	CPTxWDMCmdOperator *UniqueServer() { return UniServer; }
};


  // CPTxWDMStreamer

	#define PTXWDMSTREAMER_SUFFIX	    L"_Stream"
	#define PTXWDMSTREAMER_DEFPACKETNUM	2

	typedef BOOL (__stdcall *TXDIRECTFUNC)(LPVOID dst, DWORD &sz, PVOID arg);

class CPTxWDMStreamer : public CSharedPacketStreamer
{
private:
	CSharedPacketStreamer::Send ;
	CSharedPacketStreamer::Recv ;
	DWORD LastLockedRecvCur;
protected:
	DWORD PosSzPackets ; // Position of the begining packets' size lists
public:
	CPTxWDMStreamer(std::wstring name, BOOL receiver, DWORD packet_sz,
		DWORD packet_num);
	virtual ~CPTxWDMStreamer();
	BOOL Tx(const LPVOID data, DWORD size, DWORD timeout=INFINITE) ;
	BOOL TxDirect(TXDIRECTFUNC Func, PVOID arg, DWORD timeout=INFINITE);
	BOOL Rx(LPVOID data, DWORD &size, DWORD timeout=INFINITE) ;
};

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
using namespace PRY8EAlByw ;
//===========================================================================
#endif // _PTXWDMCMD_20210301140612558_H_INCLUDED_
