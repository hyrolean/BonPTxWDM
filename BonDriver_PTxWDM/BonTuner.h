//===========================================================================
#pragma once
#ifndef _BONTUNER_20210227170702286_H_INCLUDED_
#define _BONTUNER_20210227170702286_H_INCLUDED_
//---------------------------------------------------------------------------

#include <vector>
#include <queue>
#include <string>
#include <set>

#include "../Common/PTxWDMCmd.h"
#include "../Common/pryutil.h"
#include "IBonDriver3.h"

void InitializeBonTuners(HMODULE hModule);
void FinalizeBonTuners();

class CBonTuner : public IBonDriver3
{
protected:
	enum BAND {
		BAND_na, // [n/a]
		BAND_VU, // VHS, UHF or CATV
		BAND_BS, // BS
		BAND_ND  // CS110
	};
	struct CHANNEL {
		std::wstring Space ;
		BAND        Band ;
		int			Channel ;
		WORD		Stream ;
		WORD        TSID ;
		std::wstring	Name ;
		CHANNEL(std::wstring space, BAND band, int channel, std::wstring name,unsigned stream=0,unsigned tsid=0) {
			Space = space ;
			Band = band ;
			Name = name ;
			Channel = channel ;
			Stream = stream ;
			TSID = tsid ;
		}
		CHANNEL(const CHANNEL &src) {
			Space = src.Space ;
			Band = src.Band ;
			Name = src.Name ;
			Channel = src.Channel ;
			Stream = src.Stream ;
			TSID = src.TSID;
		}
		bool isISDBT() { return Band==BAND_VU; }
		bool isISDBS() { return Band==BAND_BS||Band==BAND_ND; }
		int PtDrvChannel() {
			if(!Channel)
				return -1;
			switch(Band) {
			case BAND_VU:
				if(Channel>=113) { // CATV
					return Channel-113;
				}else { // UHF
					return Channel-13 + 51;
				}
				break;
			case BAND_BS: // BS
				return (Channel-1)/2;
				break;
			case BAND_ND: // CS110
				return (Channel-1)/2 + 12;
				break;
			}
			return -1;
		}
	};
	typedef std::vector<CHANNEL> CHANNELS ;
public: // IBonDriver �p��

// IBonDriver3
	const DWORD GetTotalDeviceNum(void);
	const DWORD GetActiveDeviceNum(void);
	const BOOL SetLnbPower(const BOOL bEnable);

// IBonDriver2
	LPCTSTR GetTunerName(void);

	const BOOL IsTunerOpening(void);

	LPCTSTR EnumTuningSpace(const DWORD dwSpace);
	LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel);

	const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel);

	const DWORD GetCurSpace(void);
	const DWORD GetCurChannel(void);

// IBonDriver
	const BOOL OpenTuner(void);
	void CloseTuner(void);

	const BOOL SetChannel(const BYTE bCh);
	const float GetSignalLevel(void);

	const DWORD WaitTsStream(const DWORD dwTimeOut = 0);
	const DWORD GetReadyCount(void);

	const BOOL GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain);
	const BOOL GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain);

	void PurgeTsStream(void);

	void Release(void);

public: // constructor / destructor
	CBonTuner();
	~CBonTuner();

	static HMODULE HModule ;

protected: // settings

	//����̃`�����l�����
	//BOOL DEFSPACEVHF               ; // VHF���܂߂邩�ǂ���
	BOOL DEFSPACEUHF               ; // UHF���܂߂邩�ǂ���
	BOOL DEFSPACECATV              ; // CATV���܂߂邩�ǂ���
	BOOL DEFSPACEBS                ; // BS���܂߂邩�ǂ���
	int  DEFSPACEBSSTREAMS         ; // BS�̊e�X�g���[����(0-8)
	BOOL DEFSPACEBSSTREAMSTRIDE    ; // BS���X�g���[����ɔz�u���邩�ǂ���
	BOOL DEFSPACECS110             ; // CS110���܂߂邩�ǂ���
	BOOL DEFSPACECS110STREAMSTRIDE ; // CS110���X�g���[����ɔz�u���邩�ǂ���
	int  DEFSPACECS110STREAMS      ; // CS110�̊e�X�g���[����(0-8)

	// �t���O
	BOOL USELNB;
	BOOL BON3LNB;
	BOOL FASTSCAN;
	BOOL TRYSPARES;
	DWORD SETCHDELAY;
	DWORD CTRLPACKETS;
	DWORD MAXDUR_FREQ;
	DWORD MAXDUR_TMCC;
	DWORD MAXDUR_TSID;

	// �񓯊�TS
	DWORD ASYNCTSPACKETSIZE;
	DWORD ASYNCTSQUEUENUM;
	DWORD ASYNCTSQUEUEMAX;
	DWORD ASYNCTSEMPTYBORDER;
	DWORD ASYNCTSEMPTYLIMIT;
	DWORD ASYNCTSRECVTHREADWAIT;
	int   ASYNCTSRECVTHREADPRIORITY;
	BOOL  ASYNCTSFIFOALLOCWAITING;
	DWORD ASYNCTSFIFOTHREADWAIT;
	int   ASYNCTSFIFOTHREADPRIORITY;

protected: // internals

	std::string m_strPath;

	void InitTunerProperty() ;
	bool LoadIniFile(std::string strIniFileName);
	bool LoadChannelFile(std::string strChannelFileName);
	void InitChannelToDefault() ;
    const BOOL SetRealChannel(const DWORD dwCh) ;

	BOOL LoadTuner();
    void UnloadTuner();

    // �`���[�i�[�̃v���p�e�B
    std::wstring m_strTunerName ;
	std::wstring m_strTunerStaticName;
	int m_iTunerId, m_iTunerStaticId;
	bool m_isISDBS;

    // �`�����l�����
	CHANNELS m_Channels ;
    std::vector<DWORD> m_SpaceAnchors ;
    std::vector<DWORD> m_ChannelAnchors ;
    std::vector<std::wstring> m_InvisibleSpaces ;
    std::set<std::wstring> m_InvalidSpaces ;
    std::vector<std::wstring> m_SpaceArrangement ;
    DWORD space_index_of(DWORD sch) const ;
    DWORD channel_index_of(DWORD sch) const ;
    BOOL is_invalid_space(DWORD spc) const ;
    void RebuildChannels() ;

	DWORD			m_dwCurSpace;
	DWORD			m_dwCurChannel;
	BOOL			m_hasStream;
	HANDLE			m_hTunerMutex;

	//Lnb
	HANDLE m_hLnbMutex;
	const BOOL ChangeLnbPower(BOOL power);

	//�񓯊�TS
	CAsyncFifo		*m_AsyncTSFifo;
	HANDLE			m_hAsyncTsThread;
	BOOL			m_bAsyncTsTerm;
	event_object	m_evAsyncTsStream;
	int AsyncTsThreadProcMain();
	static unsigned int __stdcall AsyncTsThreadProc (PVOID pv);
	void StartAsyncTsThread();
	void StopAsyncTsThread();

	//�{��
	CPTxWDMCmdOperator	*m_CmdClient;

};

//===========================================================================
#endif // _BONTUNER_20210227170702286_H_INCLUDED_
