//===========================================================================

#ifndef _PTXWDMCMDSRV_20210304223243864_H_INCLUDED_
#define _PTXWDMCMDSRV_20210304223243864_H_INCLUDED_
//---------------------------------------------------------------------------

#include "PTxWDMCmd.h"
#include "PtDrvWrap.h"
//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------

  // CPTxWDMCmdServiceOperator (PTxWDM Command Operator for Server)

class CPTxWDMCmdServiceOperator : public CPTxWDMCmdOperator
{
protected:
	critical_object Critical_;
	DWORD LastAlive_;
	BOOL Sate_;
	CPtDrvWrapper *Tuner_;
	BOOL Terminated_;
	BOOL StreamingEnabled_;
public:
	CPTxWDMCmdServiceOperator(std::wstring name);
	virtual ~CPTxWDMCmdServiceOperator();
protected:
	// for Server Operations (override)
	BOOL ResTerminate();
	BOOL ResOpenTuner(BOOL Sate, DWORD TunerID);
	BOOL ResCloseTuner();
	BOOL ResGetTunerCount(DWORD &Count);
	BOOL ResSetTunerSleep(BOOL Sleep);
	BOOL ResSetStreamEnable(BOOL Enable);
	BOOL ResIsStreamEnabled(BOOL &Enable);
	BOOL ResSetChannel(BOOL &Tuned, DWORD Freq, DWORD TSID, DWORD Stream);
	BOOL ResSetFreq(DWORD Freq);
	BOOL ResGetIdListS(TSIDLIST &TSIDList);
	BOOL ResGetIdS(DWORD &TSID);
	BOOL ResSetIdS(DWORD TSID);
	BOOL ResSetLnbPower(BOOL Power);
	BOOL ResGetCnAgc(DWORD &Cn100, DWORD &CurAgc, DWORD &MaxAgc);
	BOOL ResPurgeStream();
public:
	CPtDrvWrapper *Tuner() { return Tuner_; }
	VOID KeepAlive() { LastAlive_ = Elapsed(); }
	DWORD LastAlive() { return LastAlive_; }
	BOOL Terminated() { return Terminated_; }
	BOOL StreamingEnabled() { return StreamingEnabled_; }
	DWORD CurStreamSize();
	BOOL GetStreamData(LPVOID data, DWORD &size);
};

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
using namespace PRY8EAlByw ;
//===========================================================================
#endif // _PTXWDMCMDSRV_20210304223243864_H_INCLUDED_
