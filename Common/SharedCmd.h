//===========================================================================
#pragma once
#ifndef _SHAREDCMD_20210302003230838_H_INCLUDED_
#define _SHAREDCMD_20210302003230838_H_INCLUDED_
//---------------------------------------------------------------------------

#include "pryutil.h"
//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------

  // CSharedCmdOperator ( Bidirectional command operator )

class CSharedCmdOperator : public CSharedMemory
{
private:
	DWORD SzCmd; // Maximum command size
	LPVOID PCmdRecv, PCmdSend; // Pointer position of command RX/TX
	event_object EvSent ;  // Command sent event
	std::wstring NamListen ; // Listening event name
public:
	CSharedCmdOperator(std::wstring name, BOOL server, DWORD cmd_size,  DWORD extra_sz=0);
	virtual ~CSharedCmdOperator();
	bool IsValid() const { return CSharedMemory::IsValid() && EvSent.is_valid() ; }
	virtual BOOL Send(const LPVOID cmd, DWORD timeout=INFINITE) ;
	virtual BOOL Recv(LPVOID cmd, DWORD timeout=INFINITE) ;
	virtual BOOL Listen(DWORD timeout=INFINITE) { return WaitForCmd(timeout)==WAIT_OBJECT_0 ? TRUE : FALSE ; }
	virtual DWORD WaitForCmd(DWORD timeout=INFINITE) ;
	DWORD CmdSize() { return SzCmd ; }
};

  // CSharedPacketStreamer ( Unidirectional packet streamer )

class CSharedPacketStreamer : public CSharedCmdOperator
{
protected:
	DWORD SzPacket;  // Packet size
	DWORD NumPacket; // Packet count (Packet size x Packet count = Whole size)
	DWORD CurPacketSend, CurPacketRecv; // FIFO streaming cursor
	BOOL FReceiver; // TRUE: Receiver, FALSE: Transimitter
	DWORD PosPackets;  // Position of begining packets
	HANDLE *PacketMutex;
	bool LockPacket(DWORD index, DWORD timeout = INFINITE) const ;
	bool UnlockPacket(DWORD index) const ;
public:
	CSharedPacketStreamer(std::wstring name, BOOL receiver, DWORD packet_sz,
		DWORD packet_num, DWORD extra_sz=0);
	virtual ~CSharedPacketStreamer();
	virtual BOOL Send(const LPVOID data, DWORD timeout=INFINITE) ;
	virtual BOOL Recv(LPVOID data, DWORD timeout=INFINITE) ;
	DWORD PacketSize() { return SzPacket ; }
	DWORD PacketCount() { return NumPacket ; }
	BOOL IsReceiver() { return FReceiver; }
	DWORD PacketRemain(DWORD timeout=INFINITE) ;
};

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
using namespace PRY8EAlByw ;
//===========================================================================
#endif // _SHAREDCMD_20210302003230838_H_INCLUDED_
