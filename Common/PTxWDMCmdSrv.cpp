//===========================================================================
#include "stdafx.h"
#include "PtDrvWrap.h"
#include "PTxWDMCmdSrv.h"
//---------------------------------------------------------------------------

using namespace std ;

//===========================================================================
namespace PRY8EAlByw {
//===========================================================================
// CPTxWDMCmdServiceOperator
//---------------------------------------------------------------------------
CPTxWDMCmdServiceOperator::CPTxWDMCmdServiceOperator(wstring name)
  : CPTxWDMCmdOperator(name,true)
{
	Tuner_ = NULL ;
	Terminated_ = FALSE ;
	StreamingEnabled_ = FALSE ;
	Settings_.dwSize = sizeof(Settings_);
	Settings_.CtrlPackets = PTXWDMSTREAMER_DEFPACKETNUM ;
	Settings_.StreamerThreadPriority = THREAD_PRIORITY_HIGHEST ;
	Settings_.MAXDUR_FREQ = 1000; //���g�������ɔ�₷�ő厞��(msec)
	Settings_.MAXDUR_TMCC = 1500; //TMCC�擾�ɔ�₷�ő厞��(msec)
	Settings_.MAXDUR_TSID = 3000; //TSID�ݒ�ɔ�₷�ő厞��(msec)
	Settings_.StreamerPacketSize = 64*1024 ;
	Settings_.LNB11V = FALSE;
	KeepAlive();
}
//---------------------------------------------------------------------------
CPTxWDMCmdServiceOperator::~CPTxWDMCmdServiceOperator()
{
	ResCloseTuner();
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResTerminate()
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	Terminated_=TRUE;
	return TRUE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResOpenTuner(BOOL Sate, DWORD TunerID)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	ResCloseTuner();

	LINEDEBUG;
	auto tuner = new CPtDrvWrapper(Sate,TunerID) ;
	if(!tuner->HandleAllocated()) {
		LINEDEBUG;
		delete tuner;
		return FALSE;
	}

	Tuner_ = tuner;
	Sate_ = Sate;

	LINEDEBUG;
	return TRUE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResCloseTuner()
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		delete Tuner_;
		Tuner_ = NULL;
	}
	return TRUE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResGetTunerCount(DWORD &Count)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	Count = CPtDrvWrapper::TunerCount() ;
	return TRUE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetTunerSleep(BOOL Sleep)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->SetTunerSleep(Sleep?true:false))
			return TRUE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetStreamEnable(BOOL Enable)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->SetStreamEnable(Enable?true:false)) {
			StreamingEnabled_ = Enable ;
			DBGOUT("service streaming: %s.\n",StreamingEnabled_?"Enabled":"Disabled");
			return TRUE;
		}
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResIsStreamEnabled(BOOL &Enable)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		Enable = Tuner_->IsStreamEnabled();
		return Tuner_->LastErrorCode()==NOERROR? TRUE: FALSE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetChannel(BOOL &Tuned, DWORD Freq,
	DWORD TSID, DWORD Stream)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(!Tuner_) return FALSE ;

	const DWORD MAXDUR_FREQ = Settings_.MAXDUR_FREQ; //���g�������ɔ�₷�ő厞��(msec)
	const DWORD MAXDUR_TMCC = Settings_.MAXDUR_TMCC; //TMCC�擾�ɔ�₷�ő厞��(msec)

	//�`���[�j���O
	bool tuned=false ;

	do {

		{
			bool done=false ;
			for (DWORD t=0,s=Elapsed(),n=0; t<MAXDUR_FREQ; t=Elapsed(s)) {
				if(Tuner_->SetFrequency(Freq))
				{done=true;break;}
			}
			if(!done) break ;
		}

		Sleep(60);

		if(Sate_) {
			if(!TSID) {
				for (DWORD t=0,s=Elapsed(); t<MAXDUR_TMCC; t=Elapsed(s)) {
					TMCC_STATUS tmcc;
					ZeroMemory(&tmcc,sizeof(tmcc));
					//std::fill_n(tmcc.Id,8,0xffff) ;
					if(Tuner_->GetTmcc(&tmcc)) {
						for (int i=0; i<8; i++) {
							WORD id = tmcc.u.bs.tsId[i]&0xffff;
							if ((id&0xff00) && (id^0xffff)) {
								if( (id&7) == Stream ) { //�X�g���[���Ɉ�v����
									//��v����id�ɏ���������
									TSID = id ;
									break;
								}
							}
						}
						if(TSID&~7UL) break ;
					}
					Sleep(50);
				}
			}
			if(!TSID) break ;
			if(ResSetIdS(TSID)) tuned = true ;
		}else {
			TMCC_STATUS tmcc;
			for (DWORD t=0,s=Elapsed(); t<MAXDUR_TMCC; t=Elapsed(s)) {
				if(Tuner_->GetTmcc(&tmcc)) {tuned=true;break;}
				Sleep(50);
			}
		}

	}while(0);

	Tuned = tuned ? TRUE : FALSE ;

	return TRUE ;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetFreq(DWORD Freq)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->SetFrequency(Freq))
			return TRUE ;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResGetIdListS(TSIDLIST &TSIDList)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		TMCC_STATUS tmcc;
		ZeroMemory(&tmcc,sizeof(tmcc));
		if(Tuner_->GetTmcc(&tmcc)) {
			for (int i=0 ; i<8 ; i++)
				TSIDList.Id[i] = tmcc.u.bs.tsId[i] ;
            return TRUE;
		}
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResGetIdS(DWORD &TSID)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		TSID = Tuner_->CurIdS();
		return Tuner_->LastErrorCode()==NOERROR? TRUE: FALSE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetIdS(DWORD TSID)
{
	const DWORD MAXDUR_TSID = Settings_.MAXDUR_TSID; //TSID�ݒ�ɔ�₷�ő厞��(msec)
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		BOOL bRes = FALSE ;
		for (DWORD t=0,s=Elapsed(),n=0; t<MAXDUR_TSID ; t=Elapsed(s)) {
			if(Tuner_->SetIdS(TSID)) { if(++n>=2) {bRes=TRUE;break;} }
			Sleep(50);
		}
		return bRes;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetLnbPower(BOOL Power)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->SetLnbPower(
			Power?
				(Settings_.LNB11V?BS_LNB_POWER_11V:BS_LNB_POWER_15V):
				BS_LNB_POWER_OFF	))
			return TRUE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResGetCnAgc(DWORD &Cn100, DWORD &CurAgc,
	DWORD &MaxAgc)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		UINT cn100, curAgc, maxAgc;
		if(Tuner_->GetCnAgc(&cn100,&curAgc,&maxAgc)) {
			Cn100 = cn100; CurAgc = curAgc; MaxAgc = maxAgc;
			return TRUE;
		}
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResPurgeStream()
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->PurgeStream())
			return TRUE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetupServer(const SERVER_SETTINGS *Options)
{
	exclusive_lock lock(&Critical_);
	KeepAlive();
	if(Options->dwSize<=sizeof(SERVER_SETTINGS)) {
		CopyMemory(&Settings_,Options,Options->dwSize);
		Settings_.dwSize = sizeof(Settings_);
		return TRUE ;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
DWORD CPTxWDMCmdServiceOperator::CurStreamSize()
{
	exclusive_lock lock(&Critical_);
	if(Tuner_) {
		return Tuner_->CurStreamSize();
	}
	return 0;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::GetStreamData(LPVOID data, DWORD &size)
{
	exclusive_lock lock(&Critical_);
	if(Tuner_) {
		UINT szOut = 0 ;
		BOOL res = Tuner_->GetStreamData(data, size, &szOut) ? TRUE : FALSE ;
		if(res) size = szOut ;
		return res;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
