//===========================================================================
#include "stdafx.h"

#include "PTxWDMCmd.h"
//---------------------------------------------------------------------------

using namespace std;

//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------
//===========================================================================
// CPTxWDMCmdOperator
//---------------------------------------------------------------------------
CPTxWDMCmdOperator::CPTxWDMCmdOperator(wstring name, bool server)
  : CSharedCmdOperator(name, server, sizeof OP)
{
	ServerMode = server;
	StaticId = 0 ;
	UniServer = NULL;
}
//---------------------------------------------------------------------------
CPTxWDMCmdOperator::~CPTxWDMCmdOperator()
{
	Uniqulize(nullptr);
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::Xfer(OP &cmd, OP &res, DWORD timeout)
{
	exclusive_lock lock(&XferCritical);
	if(ServerMode) return FALSE;
	cmd.id = StaticId ;
	if(!Send(&cmd,timeout)) return FALSE;
	res.id = StaticId-1;
	res.res = FALSE;
	if(UniServer) {
		if(!UniServer->ServiceReaction(timeout)) return FALSE;
	}else {
		if(!Listen(timeout)) return FALSE;
	}
	if(!Recv(&res,timeout)) return FALSE;
	if(res.id!=StaticId) return FALSE;
	StaticId++;
	return TRUE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdTerminate(DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_TERMINATE ;
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res ;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdOpenTuner(BOOL Sate, DWORD TunerID, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_OPEN_TUNER ;
	op.data[0] = Sate ;
	op.data[1] = TunerID ;
	LINEDEBUG;
	if(!Xfer(op,op,timeout)) return FALSE;
	LINEDEBUG;
	return op.res ;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdCloseTuner(DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_CLOSE_TUNER ;
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res ;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdGetTunerCount(DWORD &Count, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_GET_TUNER_COUNT ;
	if(!Xfer(op,op,timeout)) return FALSE;
	Count = op.data[0];
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdSetTunerSleep(BOOL Sleep, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_SET_TUNER_SLEEP ;
	op.data[0] = Sleep ;
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdSetStreamEnable(BOOL Enable, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_SET_STREAM_ENABLE ;
	op.data[0] = Enable ;
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdIsStreamEnabled(BOOL &Enable, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_IS_STREAM_ENABLED ;
	if(!Xfer(op,op,timeout)) return FALSE;
	Enable = op.data[0];
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdSetChannel(BOOL &Tuned, DWORD Freq, DWORD TSID,
	DWORD Stream, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_SET_CHANNEL ;
	op.data[0] = Freq ;
	op.data[1] = TSID ;
	op.data[2] = Stream ;
	if(!Xfer(op,op,timeout)) return FALSE;
	Tuned = op.data[0];
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdSetFreq(DWORD Freq, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_SET_FREQ ;
	op.data[0] = Freq ;
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdGetIdListS(TSIDLIST &TSIDList, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_GET_IDLIST_S ;
	if(!Xfer(op,op,timeout)) return FALSE;
	for(int i=0;i<8;i++) TSIDList.Id[i] = op.data[i];
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdGetIdS(DWORD &TSID, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_GET_ID_S ;
	if(!Xfer(op,op,timeout)) return FALSE;
	TSID = op.data[0];
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdSetIdS(DWORD TSID, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_SET_ID_S ;
	op.data[0] = TSID ;
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdSetLnbPower(BOOL Power, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_SET_LNB_POWER ;
	op.data[0] = Power ;
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdGetCnAgc(DWORD &Cn100, DWORD &CurAgc,
	DWORD &MaxAgc, DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_GET_CN_AGC ;
	if(!Xfer(op,op,timeout)) return FALSE;
	Cn100	= op.data[0];
	CurAgc	= op.data[1];
	MaxAgc	= op.data[2];
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdPurgeStream(DWORD timeout)
{
	OP op;
	op.cmd = PTXWDMCMD_PURGE_STREAM ;
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::CmdSetupServer(const SERVER_SETTINGS *Options, DWORD timeout)
{
	OP op;
	if(!Options||Options->dwSize>sizeof(op.data)) return FALSE ;
	op.cmd = PTXWDMCMD_SETUP_SERVER ;
	CopyMemory(&op.data[0],Options,Options->dwSize);
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::ServiceReaction(DWORD timeout)
{
	if(!ServerMode) return FALSE;
	OP cmd, res;
	if(!Recv(&cmd,timeout)) return FALSE ;
	BOOL result = TRUE ;
	DBGOUT("service reaction: cmd=%d\n",cmd.cmd);
	switch(cmd.cmd) {
	case PTXWDMCMD_TERMINATE:
		res.res = ResTerminate() ;
		break;
	case PTXWDMCMD_OPEN_TUNER:
		res.res = ResOpenTuner(cmd.data[0]?TRUE:FALSE,cmd.data[1]) ;
		break;
	case PTXWDMCMD_CLOSE_TUNER:
		res.res = ResCloseTuner() ;
		break;
    case PTXWDMCMD_GET_TUNER_COUNT:
		res.res = ResGetTunerCount(res.data[0]);
		break;
    case PTXWDMCMD_SET_TUNER_SLEEP:
		res.res = ResSetTunerSleep(cmd.data[0]?TRUE:FALSE);
		break;
    case PTXWDMCMD_SET_STREAM_ENABLE:
		res.res = ResSetStreamEnable(cmd.data[0]?TRUE:FALSE);
		break;
    case PTXWDMCMD_IS_STREAM_ENABLED: {
		BOOL enable = FALSE ;
		res.res = ResIsStreamEnabled(enable);
		res.data[0] = enable ;
		break;
	}
    case PTXWDMCMD_SET_CHANNEL: {
		BOOL tuned = FALSE ;
		res.res = ResSetChannel(tuned,cmd.data[0],cmd.data[1],cmd.data[2]);
		res.data[0] = tuned ;
		break;
	}
    case PTXWDMCMD_SET_FREQ:
		res.res = ResSetFreq(cmd.data[0]);
		break;
    case PTXWDMCMD_GET_IDLIST_S: {
		TSIDLIST tsid_list={0};
		res.res = ResGetIdListS(tsid_list);
		for(int i=0;i<8;i++) res.data[i] = tsid_list.Id[i];
		break;
	}
    case PTXWDMCMD_GET_ID_S:
		res.res = ResGetIdS(res.data[0]);
		break;
    case PTXWDMCMD_SET_ID_S:
		res.res = ResSetIdS(cmd.data[0]);
		break;
    case PTXWDMCMD_SET_LNB_POWER:
		res.res = ResSetLnbPower(cmd.data[0]);
		break;
    case PTXWDMCMD_GET_CN_AGC:
		res.res = ResGetCnAgc(res.data[0],res.data[1],res.data[2]);
		break;
    case PTXWDMCMD_PURGE_STREAM:
		res.res = ResPurgeStream();
		break;
	case PTXWDMCMD_SETUP_SERVER:
		res.res = ResSetupServer(reinterpret_cast<SERVER_SETTINGS*>(&cmd.data[0]));
		break;
	default:
		result = res.res = FALSE ;
	}
	res.id = cmd.id ;
	if(!Send(&res,timeout)) result = FALSE ;
	return result ;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdOperator::Uniqulize(CPTxWDMCmdOperator *server)
{
	if(!server) {
		if(UniServer)
			delete UniServer;
		UniServer=NULL;
		return TRUE ;
	}
	if( !ServerMode && server->ServerMode && Name()==server->Name() ) {
		UniServer=server;
		DBGOUT("--- Enter the UniServer mode ---\n");
		return TRUE ;
	}
	return FALSE ;
}
//===========================================================================
// CPTxWDMStreamer
//---------------------------------------------------------------------------
CPTxWDMStreamer::CPTxWDMStreamer(wstring name, BOOL receiver, DWORD packet_sz,
	DWORD packet_num)
  : CSharedPacketStreamer(name, receiver, packet_sz, packet_num,
		packet_num*sizeof(DWORD))
{
	PosSzPackets = Size() - packet_num*sizeof(DWORD) ;
}
//---------------------------------------------------------------------------
CPTxWDMStreamer::~CPTxWDMStreamer()
{
	;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMStreamer::Tx(const LPVOID data, DWORD size, DWORD timeout)
{
	if(!IsValid()||FReceiver) return FALSE;
	if(size>SzPacket) return FALSE;
	DWORD cur = CurPacketSend ;
	if(LockPacket(cur,timeout)) {
		LPVOID data_top = &static_cast<LPBYTE>(Memory())[PosSzPackets];
		static_cast<LPDWORD>(data_top)[CurPacketSend]=size;
		BOOL res = Send(data, timeout);
		if(!UnlockPacket(cur)) res = FALSE ;
		return res;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMStreamer::Rx(LPVOID data, DWORD &size, DWORD timeout)
{
	if(!PacketRemain()) return FALSE;
	DWORD cur = CurPacketRecv ;
	if(LockPacket(cur,timeout)) {
		LPVOID data_top = &static_cast<LPBYTE>(Memory())[PosSzPackets];
		DWORD n = static_cast<LPDWORD>(data_top)[CurPacketRecv];
		BOOL res = Recv(data, timeout);
		if(res) size = n ;
		if(!UnlockPacket(cur)) res = FALSE ;
		return res;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
