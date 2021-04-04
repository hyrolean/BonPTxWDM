//===========================================================================
#include "stdafx.h"

#include "SharedCmd.h"
//---------------------------------------------------------------------------

using namespace std;

//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------
//===========================================================================
// CSharedCmdOperator
//---------------------------------------------------------------------------
CSharedCmdOperator::CSharedCmdOperator(wstring name, BOOL server,
	DWORD cmd_size, DWORD extra_sz)
  : SzCmd(cmd_size), CSharedMemory(name, cmd_size*2 + extra_sz),
	EvSent(FALSE,L"Global\\" + name + L"_SharedCmdOperator_" + wstring(server ? L"SentSrv":L"SentCli"),TRUE),
	NamListen(L"Global\\" + name + L"_SharedCmdOperator_" + wstring(server ? L"SentCli":L"SentSrv"))
{
	#if _DEBUG
	if(EvSent.is_valid()) {
		DBGOUT("Event \"%s\" created.\n", wcs2mbcs(EvSent.event_name()).c_str());
	}else {
		DBGOUT("Event \"%s\" creation failed.\n", wcs2mbcs(EvSent.event_name()).c_str());
	}
	#endif

	LPVOID p1 = Memory(), p2 = &static_cast<BYTE*>(Memory())[cmd_size] ;

	PCmdRecv = server ? p1 : p2 ;
	PCmdSend = server ? p2 : p1 ;
}
//---------------------------------------------------------------------------
CSharedCmdOperator::~CSharedCmdOperator()
{
	;
}
//---------------------------------------------------------------------------
BOOL CSharedCmdOperator::Send(const LPVOID cmd, DWORD timeout)
{
	if(!IsValid()) return FALSE ;
	if(Lock(timeout)) {
		CopyMemory(PCmdSend, cmd, SzCmd);
		EvSent.set();
		//DBGOUT("%s sent.\n",wcs2mbcs(Name()).c_str());
		return Unlock();
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CSharedCmdOperator::Recv(LPVOID cmd, DWORD timeout)
{
	if(!IsValid()) return FALSE ;
	if(Lock(timeout)) {
		CopyMemory(cmd, PCmdRecv, SzCmd);
		//DBGOUT("%s recv.\n",wcs2mbcs(Name()).c_str());
		return Unlock();
	}
	return FALSE;
}
//---------------------------------------------------------------------------
DWORD CSharedCmdOperator::WaitForCmd(DWORD timeout)
{
	if(!IsValid()) return WAIT_FAILED ;
	DWORD res = WAIT_FAILED;
	DWORD e = Elapsed();
	HANDLE hListen = NULL ;
	for(DWORD s=e ; Elapsed(s,e)<=timeout ; e = Elapsed()) {
		HANDLE event = OpenEvent(EVENT_ALL_ACCESS,FALSE,NamListen.c_str());
		if(event&&event!=INVALID_HANDLE_VALUE) { hListen=event; break; }
		Sleep(10);
	}
	if(!hListen) {
		res = WAIT_TIMEOUT;
		DBGOUT("Failed to open Listen event \"%s\".\n",wcs2mbcs(NamListen).c_str());
	}
	if(hListen) {
		//DBGOUT("Succeeded to open Listen event \"%s\".\n",wcs2mbcs(NamListen).c_str());
		DWORD past = Elapsed(e);
		timeout = past>timeout ? 0 : timeout - past ;
		res = WaitForSingleObject(hListen, timeout) ;
		#if 0
		if(res==WAIT_OBJECT_0) {
			DBGOUT("WaitForCmd \"%s\" successful.\n",wcs2mbcs(NamListen).c_str());
			//SetEvent(hListen);
		}
		#endif
		CloseHandle(hListen);
	}
	return res ;
}
//===========================================================================
// CSharedPacketStreamer
//---------------------------------------------------------------------------
#define PACKETSTREAMER_CMDSIZE sizeof(DWORD)
//---------------------------------------------------------------------------
CSharedPacketStreamer::CSharedPacketStreamer(wstring name,
	BOOL receiver, DWORD packet_sz, DWORD packet_num, DWORD extra_sz)
  : SzPacket(packet_sz), NumPacket(packet_num), FReceiver(receiver),
    CSharedCmdOperator(name,receiver,PACKETSTREAMER_CMDSIZE,
		packet_sz*packet_num+extra_sz)
{
	PacketMutex = new HANDLE[NumPacket] ;
	if(PacketMutex) {
		for(DWORD i=0;i<NumPacket;i++) {
			wstring mutex_name = name + L"_SharedPacketStreamer_Mutex" + mbcs2wcs(itos(i)) ;
			PacketMutex[i] = CreateMutex(NULL, FALSE, mutex_name.c_str());
		}
	}
	CurPacketSend = CurPacketRecv = 0 ;
	PosPackets = Size() - extra_sz - ( NumPacket * SzPacket ) ;
}
//---------------------------------------------------------------------------
CSharedPacketStreamer::~CSharedPacketStreamer()
{
	if(PacketMutex) {
		for(DWORD i=0;i<NumPacket;i++) {
			if(PacketMutex[i])
				CloseHandle(PacketMutex[i]) ;
		}
		delete[] PacketMutex ;
	}
}
//---------------------------------------------------------------------------
bool CSharedPacketStreamer::LockPacket(DWORD index, DWORD timeout) const
{
	if(!PacketMutex||index>=NumPacket||!PacketMutex[index])
		return Lock(timeout);
	return WaitForSingleObject(PacketMutex[index], timeout) == WAIT_OBJECT_0 ;
}
//---------------------------------------------------------------------------
bool CSharedPacketStreamer::UnlockPacket(DWORD index) const
{
	if(!PacketMutex||index>=NumPacket||!PacketMutex[index])
		return Unlock();
	return ReleaseMutex(PacketMutex[index]) ? true : false ;
}
//---------------------------------------------------------------------------
BOOL CSharedPacketStreamer::Send(const LPVOID data, DWORD timeout)
{
	if(!IsValid()||FReceiver) return FALSE;
	DWORD cur = CurPacketSend ;
	if(LockPacket(cur,timeout)) {
		LPBYTE data_top = &static_cast<LPBYTE>(Memory())[PosPackets];
		if(data) CopyMemory(&data_top[CurPacketSend*SzPacket],data,SzPacket);
		if(++CurPacketSend>=NumPacket) CurPacketSend=0 ;
		BOOL res = CSharedCmdOperator::Send(&CurPacketSend,timeout);
		if(!res) CurPacketSend=cur;
		if(!UnlockPacket(cur)) res = FALSE;
		return res;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CSharedPacketStreamer::Recv(LPVOID data, DWORD timeout)
{
	if(!PacketRemain()) return FALSE ;
	DWORD cur = CurPacketRecv ;
	if(LockPacket(cur,timeout)) {
		LPBYTE data_top = &static_cast<LPBYTE>(Memory())[PosPackets];
		if(data) CopyMemory(data,&data_top[CurPacketRecv*SzPacket],SzPacket);
		if(++CurPacketRecv>=NumPacket) CurPacketRecv=0;
		return UnlockPacket(cur)?TRUE:FALSE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
DWORD CSharedPacketStreamer::PacketRemain(DWORD timeout)
{
	if(!IsValid()||!FReceiver) return 0;
	CSharedCmdOperator::Recv(&CurPacketSend,timeout);
	if(CurPacketSend>=CurPacketRecv) return CurPacketSend-CurPacketRecv;
	return CurPacketSend+(NumPacket-CurPacketRecv);
}
//===========================================================================
// CSharedTransportStreamer
//---------------------------------------------------------------------------
CSharedTransportStreamer::CSharedTransportStreamer(wstring name, BOOL receiver,
	DWORD packet_sz, DWORD packet_num, DWORD extra_sz)
  : CSharedPacketStreamer(name, receiver, packet_sz, packet_num,
		packet_num*sizeof(DWORD)+extra_sz)
{
	PosSzPackets = Size() - extra_sz - packet_num*sizeof(DWORD) ;
	LastLockedRecvCur = packet_num ;
}
//---------------------------------------------------------------------------
CSharedTransportStreamer::~CSharedTransportStreamer()
{
	if(LastLockedRecvCur<PacketCount())
		UnlockPacket(LastLockedRecvCur);
}
//---------------------------------------------------------------------------
BOOL CSharedTransportStreamer::Tx(const LPVOID data, DWORD size, DWORD timeout)
{
	if(!IsValid()||FReceiver) return FALSE;
	if(size>SzPacket) return FALSE;
	DWORD cur = CurPacketSend ;
	if(LockPacket(cur,timeout)) {
		LPVOID sizes_top = &static_cast<LPBYTE>(Memory())[PosSzPackets];
		static_cast<LPDWORD>(sizes_top)[CurPacketSend]=size;
		BOOL res = Send(data, timeout);
		if(!UnlockPacket(cur)) res = FALSE ;
		return res;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CSharedTransportStreamer::TxDirect(TXDIRECTFUNC func, PVOID arg, DWORD timeout)
{
	if(!IsValid()||FReceiver) return FALSE;
	if(func==NULL) { // for retry
		if(arg) *static_cast<BOOL*>(arg) = TRUE ;
		return Send(NULL, timeout);
	}
	DWORD cur = CurPacketSend ;
	if(LockPacket(cur,timeout)) {
		LPBYTE data_top = &static_cast<LPBYTE>(Memory())[PosPackets];
		DWORD sz = SzPacket;
		BOOL res = FALSE ;
		if(func(&data_top[CurPacketSend*SzPacket],sz,arg)) {
			LPVOID sizes_top = &static_cast<LPBYTE>(Memory())[PosSzPackets];
			static_cast<LPDWORD>(sizes_top)[CurPacketSend]=sz;
			res = Send(NULL, timeout);
		}
		if(!UnlockPacket(cur)) res = FALSE ;
		return res;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CSharedTransportStreamer::Rx(LPVOID data, DWORD &size, DWORD timeout)
{
	if(!PacketRemain()) return FALSE;
	DWORD cur = CurPacketRecv ;
	if(LockPacket(cur,timeout)) {
		LPVOID sizes_top = &static_cast<LPBYTE>(Memory())[PosSzPackets];
		DWORD n = static_cast<LPDWORD>(sizes_top)[CurPacketRecv];
		BOOL res = Recv(data, timeout);
		if(res) {
			if(LastLockedRecvCur<PacketCount()) {
				// Unlock the previous locking point.
				if(!UnlockPacket(LastLockedRecvCur))
					res = FALSE;
			}
		}
		if(res) {
			size = n ;
			// Store the current locking point to a LastLockedRecvCur variable.
			// This locking point will be unlocked on the next time calling Rx.
			// ( It's a purpose to prevent the behavior that the CurPacketSend
			//   cursor jump over the CurPacketRecv cursor. )
			LastLockedRecvCur = cur ;
		}else
			UnlockPacket(cur);
		return res;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
