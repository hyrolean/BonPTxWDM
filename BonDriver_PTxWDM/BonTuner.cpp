//===========================================================================
#include "stdafx.h"
#include <process.h>
#include <iterator>
#include <cstdio>

#include "../Common/PTxWDMCmdSrv.h"
#include "BonTuner.h"
//---------------------------------------------------------------------------

using namespace std;

//===========================================================================
  // BonTuners
//---------------------------------------------------------------------------

static CRITICAL_SECTION secBonTuners;
static set<CBonTuner*> BonTuners ;
static HANDLE hBonTunersMutex = NULL ;
#define BONTUNER_PREFIX L"BonDriver_PTxWDM"
#define BONTUNERS_MUTEXNAME BONTUNER_PREFIX L" BonTuners Mutex"
#define BONTUNER_STATIC_FORMAT BONTUNER_PREFIX L"-%c%d"

#define PTXWDMCTRL_EXE "PTxWDMCtrl.exe"


//---------------------------------------------------------------------------
void InitializeBonTuners()
{
	if(!hBonTunersMutex) {
		hBonTunersMutex = MakeMutexDacl(BONTUNERS_MUTEXNAME,TRUE);
		::InitializeCriticalSection(&secBonTuners);
	}
}
//---------------------------------------------------------------------------
void FinalizeBonTuners()
{
	if(hBonTunersMutex) {
		::EnterCriticalSection(&secBonTuners);
		vector<CBonTuner*> clone;
		copy(BonTuners.begin(),BonTuners.end(),back_inserter(clone));
		for(auto bon: clone) if(bon!=NULL) bon->Release();
		::LeaveCriticalSection(&secBonTuners);
		::DeleteCriticalSection(&secBonTuners);
		CloseHandle(hBonTunersMutex) ;
		hBonTunersMutex=NULL;
	}
}
//---------------------------------------------------------------------------
#pragma warning( disable : 4273 )
extern "C" __declspec(dllexport) IBonDriver * CreateBonDriver()
{
	// 同一プロセスからの複数インスタンス取得可能(IBonDriver3対応により)
	::EnterCriticalSection(&secBonTuners);
	CBonTuner *p = new CBonTuner ;
	if(p!=NULL) BonTuners.insert(p);
	::LeaveCriticalSection(&secBonTuners);
	return p;
}
#pragma warning( default : 4273 )
//===========================================================================
  // CBonTuner
//---------------------------------------------------------------------------
HMODULE CBonTuner::HModule = NULL;

//---------------------------------------------------------------------------
CBonTuner::CBonTuner()
{
	#ifdef _DEBUG
	for(int i=0;i<77;i++)
		DBGOUT("-");
	DBGOUT("\n");
	#endif

	InitTunerProperty();
}
//---------------------------------------------------------------------------
CBonTuner::~CBonTuner()
{
	UnloadTuner();

	::EnterCriticalSection(&secBonTuners);
	BonTuners.erase(this);
	::LeaveCriticalSection(&secBonTuners);

	StopAsyncTsThread();
	if(m_AsyncTSFifo) {
		delete m_AsyncTSFifo;
	}
}
//---------------------------------------------------------------------------
void CBonTuner::InitTunerProperty()
{
	// 値を初期化
	m_CmdClient = NULL;
	m_AsyncTSFifo = NULL;
	m_bAsyncTsTerm = TRUE;
	m_hAsyncTsThread = INVALID_HANDLE_VALUE;
	m_hLnbMutex = NULL;
	m_dwCurSpace = 0xFF;
	m_dwCurChannel = 0xFF;
	m_chCur = CHANNEL();
	m_hasStream = FALSE;
	m_hTunerMutex = NULL;
	m_hCtrlProcess = NULL;
	m_iTunerId = m_iTunerStaticId = -1;
	m_isISDBS = m_bLoaded = false ;

	// 非同期TS
	ASYNCTSPACKETSIZE         = 48128UL                 ; // 非同期TSデータのパケットサイズ
	ASYNCTSQUEUENUM           = 66UL                    ; // 非同期TSデータの環状ストック数(初期値)
	ASYNCTSQUEUEMAX           = 660UL                   ; // 非同期TSデータの環状ストック最大数
	ASYNCTSEMPTYBORDER        = 22UL                    ; // 非同期TSデータの空きストック数底値閾値(アロケーション開始閾値)
	ASYNCTSEMPTYLIMIT         = 11UL                    ; // 非同期TSデータの最低限確保する空きストック数(オーバーラップからの保障)
	ASYNCTSRECVTHREADWAIT     = 250UL                   ; // 非同期TSスレッドキュー毎に待つ最大時間
	ASYNCTSRECVTHREADPRIORITY = THREAD_PRIORITY_HIGHEST ; // 非同期TSスレッドの優先度
	ASYNCTSFIFOALLOCWAITING   = FALSE                   ; // 非同期TSデータのアロケーションの完了を待つかどうか
	ASYNCTSFIFOTHREADWAIT     = 1000UL                  ; // 非同期TSデータのアロケーションの監視毎時間
	ASYNCTSFIFOTHREADPRIORITY = THREAD_PRIORITY_HIGHEST ; // 非同期TSアロケーションスレッドの優先度

	// フラグの初期化
	USELNB = FALSE;
	LNB11V = FALSE;
	BON3LNB = FALSE;
	PRELOAD = FALSE;
	RETRYDUR = 3000;
	FASTSCAN = FALSE;
	TRYSPARES = FALSE;
	PREVENTSUSPENDING = FALSE;
	SETCHDELAY = 0;
	CTRLPACKETS = PTXWDMSTREAMER_DEFPACKETNUM;
	MAXDUR_FREQ = 1000; //周波数調整に費やす最大時間(msec)
	MAXDUR_TMCC = 1500; //TMCC取得に費やす最大時間(msec)
	MAXDUR_TSID = 3000; //TSID設定に費やす最大時間(msec)

	// 既定のチャンネル情報を初期化
	//DEFSPACEVHF               = FALSE ; // VHFを含めるかどうか
	DEFSPACEUHF               = TRUE  ; // UHFを含めるかどうか
	DEFSPACECATV              = FALSE ; // CATVを含めるかどうか
	DEFSPACEBS                = TRUE  ; // BSを含めるかどうか
	DEFSPACEBSSTREAMS         = 8     ; // BSの各ストリーム数(0-8)
	DEFSPACEBSSTREAMSTRIDE    = FALSE ; // BSをストリーム基準に配置するかどうか
	DEFSPACECS110             = TRUE  ; // CS110を含めるかどうか
	DEFSPACECS110STREAMSTRIDE = FALSE ; // CS110をストリーム基準に配置するかどうか
	DEFSPACECS110STREAMS      = 8     ; // CS110の各ストリーム数(0-8)

	// 自分の名前を取得
	char szMyPath[_MAX_PATH] ;
	GetModuleFileNameA( HModule, szMyPath, _MAX_PATH ) ;
	char szMyDrive[_MAX_FNAME] ;
	char szMyDir[_MAX_FNAME] ;
	char szMyName[_MAX_FNAME] ;
	_splitpath_s( szMyPath, szMyDrive, _MAX_FNAME,szMyDir, _MAX_FNAME, szMyName, _MAX_FNAME, NULL, 0 ) ;
	_strupr_s( szMyName, sizeof(szMyName) ) ;
    m_strPath = string(szMyDrive)+string(szMyDir);

	// ID を決定
	int  id =-1 ;
	if(sscanf_s( szMyName, "BONDRIVER_PTXWDM-S%d", &id )==1)
	  m_isISDBS=true, m_iTunerId = id ;
	else if(sscanf_s( szMyName, "BONDRIVER_PTXWDM-T%d", &id )==1)
	  m_isISDBS=false, m_iTunerId = id ;
	else if(!strcmp( szMyName, "BONDRIVER_PTXWDM-S"))
	  m_isISDBS=true;
	else
	  m_isISDBS=false;

	// チューナー名を決定
	TCHAR szTunerName[100] ;
	if(m_iTunerId>=0)
		swprintf_s( szTunerName, L"PTxWDM ISDB-%c%d", (m_isISDBS?L'S':L'T') , m_iTunerId ) ;
	else
		swprintf_s( szTunerName, L"PTxWDM ISDB-%c", (m_isISDBS?L'S':L'T') ) ;
	m_strTunerName = szTunerName ;

	DBGOUT("TunerName: %s\n", wcs2mbcs(szTunerName).c_str()) ;

	// Ini ファイルをロード
	m_InvisibleSpaces.clear() ;
	m_InvalidSpaces.clear() ;
	m_SpaceArrangement.clear() ;
	LoadIniFile(m_strPath+"BonDriver_PTxWDM.ini") ;
	LoadIniFile(m_strPath+string(szMyName)+".ini") ;

	// Channel ファイルをロード
	if(!LoadChannelFile(m_strPath+string(szMyName)+".ch.txt")) {
	  if(!LoadChannelFile(m_strPath+"BonDriver_PTxWDM-"+string(m_isISDBS?"S":"T")+".ch.txt")) {
		if(!LoadChannelFile(m_strPath+"BonDriver_PTxWDM.ch.txt")) {
		  InitChannelToDefault() ;
		}
	  }
	}

	// チャンネル情報再構築
	RebuildChannels() ;

	// 非同期FIFOバッファオブジェクト作成
	m_AsyncTSFifo = new CAsyncFifo(
		ASYNCTSQUEUENUM,ASYNCTSQUEUEMAX,ASYNCTSEMPTYBORDER,
		ASYNCTSPACKETSIZE,ASYNCTSFIFOTHREADWAIT,ASYNCTSFIFOTHREADPRIORITY ) ;
	m_AsyncTSFifo->SetEmptyLimit(ASYNCTSEMPTYLIMIT) ;

	if(PRELOAD) for(auto t=Elapsed();Elapsed(t)<RETRYDUR;Sleep(10)) {
		if(LoadTuner())  { m_bLoaded = true ; break ; }
	}
}
//---------------------------------------------------------------------------
bool CBonTuner::LoadIniFile(string strIniFileName)
{
  if(GetFileAttributesA(strIniFileName.c_str())==-1) return false ;
  const DWORD BUFFER_SIZE = 1024;
  char buffer[BUFFER_SIZE];
  ZeroMemory(buffer, BUFFER_SIZE);
  string Section;

  #define LOADSTR2(val,key) do { \
	  GetPrivateProfileStringA(Section.c_str(),key,val.c_str(),buffer,BUFFER_SIZE,strIniFileName.c_str()) ; \
	  val = buffer ; \
	}while(0)
  #define LOADSTR(val) LOADSTR2(val,#val)
  #define LOADWSTR(val) do { \
	  string temp = wcs2mbcs(val) ; \
	  LOADSTR2(temp,#val) ; val = mbcs2wcs(temp) ; \
	}while(0)

  #define LOADINT2(val,key,a2i) do { \
	  GetPrivateProfileStringA(Section.c_str(),key,"",buffer,BUFFER_SIZE,strIniFileName.c_str()) ; \
	  val = a2i(buffer,val) ; \
	}while(0)
  #define LOADINT(val) LOADINT2(val,#val,acalci)
  #define LOADINT64(val) LOADINT2(val,#val,acalci64)

  #define LOADSTR_SEC(sec,val) do {\
	  Section = #sec ; \
	  LOADSTR2(sec##val,#val); \
	}while(0)
  #define LOADINT_SEC(sec,val) do {\
	  Section = #sec ; \
	  LOADINT2(sec##val,#val,acalci); \
	}while(0)
  #define LOADINT64_SEC(sec,val) do {\
	  Section = #sec ; \
	  LOADINT2(sec##val,#val,acalci64); \
	}while(0)

  Section = "SET" ;
  LOADINT(USELNB);
  LOADINT(LNB11V);
  LOADINT(BON3LNB);
  LOADINT(PRELOAD);
  LOADINT(RETRYDUR);
  LOADINT(FASTSCAN);
  LOADINT(TRYSPARES);
  LOADINT(PREVENTSUSPENDING);
  LOADINT(SETCHDELAY);
  LOADINT(CTRLPACKETS); if(CTRLPACKETS<1) CTRLPACKETS=1;
  LOADINT(MAXDUR_FREQ);
  LOADINT(MAXDUR_TMCC);
  LOADINT(MAXDUR_TSID);

  Section = "DEFSPACE" ;
  wstring InvisibleSpaces ;
  LOADWSTR(InvisibleSpaces) ;
  split(m_InvisibleSpaces,InvisibleSpaces,L',') ;
  vector<wstring> vInvalidSpaces ;
  wstring InvalidSpaces ;
  LOADWSTR(InvalidSpaces) ;
  split(vInvalidSpaces,InvalidSpaces,L',') ;
  if(!vInvalidSpaces.empty()) {
	copy(vInvalidSpaces.begin(),vInvalidSpaces.end(),
	  inserter(m_InvalidSpaces,m_InvalidSpaces.begin())) ;
  }
  wstring SpaceArrangement ;
  LOADWSTR(SpaceArrangement) ;
  split(m_SpaceArrangement,SpaceArrangement,L',') ;

  //LOADINT_SEC(DEFSPACE,VHF);
  LOADINT_SEC(DEFSPACE,UHF);
  LOADINT_SEC(DEFSPACE,CATV);
  LOADINT_SEC(DEFSPACE,BS);
  LOADINT_SEC(DEFSPACE,BSSTREAMS);
  LOADINT_SEC(DEFSPACE,BSSTREAMSTRIDE);
  LOADINT_SEC(DEFSPACE,CS110);
  LOADINT_SEC(DEFSPACE,CS110STREAMS);
  LOADINT_SEC(DEFSPACE,CS110STREAMSTRIDE);

  LOADINT_SEC(ASYNCTS,PACKETSIZE);
  LOADINT_SEC(ASYNCTS,QUEUENUM);
  LOADINT_SEC(ASYNCTS,QUEUEMAX);
  LOADINT_SEC(ASYNCTS,EMPTYBORDER);
  LOADINT_SEC(ASYNCTS,EMPTYLIMIT);
  LOADINT_SEC(ASYNCTS,RECVTHREADWAIT);
  LOADINT_SEC(ASYNCTS,RECVTHREADPRIORITY);
  LOADINT_SEC(ASYNCTS,FIFOALLOCWAITING);
  LOADINT_SEC(ASYNCTS,FIFOTHREADWAIT);
  LOADINT_SEC(ASYNCTS,FIFOTHREADPRIORITY);

  #undef LOADINT64_SEC
  #undef LOADINT_SEC
  #undef LOADSTR_SEC

  #undef LOADINT64
  #undef LOADINT
  #undef LOADINT2

  #undef LOADSTR
  #undef LOADWSTR
  #undef LOADSTR2

  return true ;
}
//---------------------------------------------------------------------------
bool CBonTuner::LoadChannelFile(string strChannelFileName)
{
  CHANNELS channels ;

  FILE *st=NULL ;
  fopen_s(&st,strChannelFileName.c_str(),"rt") ;
  if(!st) return false ;
  char s[512] ;

  std::wstring space_name=L"" ;
  while(!feof(st)) {
	s[0]='\0' ;
	fgets(s,512,st) ;
	string strLine = trim(string(s)) ;
	if(strLine.empty()) continue ;
	wstring wstrLine = mbcs2wcs(strLine) ;
	int t=0 ;
	vector<wstring> params ;
	split(params,wstrLine,L';') ;
	wstrLine = params[0] ; params.clear() ;
	split(params,wstrLine,L',') ;
	if(params.size()>=2&&!params[0].empty()) {
	  BAND band = BAND_na ;
	  int channel = 0 ;
	  DWORD freq = 0 ;
	  int stream = 0 ;
	  int tsid = 0 ;
	  wstring &space = params[0] ;
	  wstring name = params.size()>=3 ? params[2] : wstring(L"") ;
	  wstring subname = params[1] ;
	  vector<wstring> phyChDiv ;
	  split(phyChDiv,params[1],'/') ;
	  for(size_t i=0;i<phyChDiv.size();i++) {
		wstring phyCh = phyChDiv[i] ;
		/*if( phyCh.length()>3&&
			phyCh.substr(phyCh.length()-3)==L"MHz" ) {
		  float megaHz = 0.f ;
		  if(swscanf_s(phyCh.c_str(),L"%fMHz",&megaHz)==1) {
			freq=DWORD(megaHz*1000.f) ;
			channel = CHANNEL::BandFromFreq(freq)!=BAND_na ? -1 : 0 ;
		  }
		}else*/ {
		  if(swscanf_s(phyCh.c_str(),L"TS%d",&stream)==1)
			;
		  else if(swscanf_s(phyCh.c_str(),L"ID%i",&tsid)==1)
			;
		  else if(band==BAND_na) {
			if(swscanf_s(phyCh.c_str(),L"BS%d",&channel)==1)
			  band = BAND_BS ;
			else if(swscanf_s(phyCh.c_str(),L"ND%d",&channel)==1)
			  band = BAND_ND ;
			else if(swscanf_s(phyCh.c_str(),L"C%d",&channel)==1)
			  band = BAND_VU, subname=L"C"+itows(channel)+L"ch", channel+=100 ;
			else if(swscanf_s(phyCh.c_str(),L"%d",&channel)==1)
			  band = BAND_VU, subname=itows(channel)+L"ch" ;
		  }
		}
	  }
	  if(name==L"")
		name=subname ;
	  /*if(freq>0&&channel<0)
		channels.push_back(
		  CHANNEL(space,freq,name,stream,tsid)) ;
	  else*/ if(band!=BAND_na&&channel>0) {
		CHANNEL channel(space,band,channel,name,stream,tsid);
		if(!channel.isISDBS()==!m_isISDBS) channels.push_back(channel) ;
	  }else
		continue ;
	  if(space_name!=space) {
		//m_SpaceIndices.push_back(channels.size()-1) ;
		space_name=space ;
	  }
	}
  }

  fclose(st) ;

	if (!channels.empty()) {
		m_Channels.swap(channels) ;
		m_Transponders.swap(CHANNELS());
		return true ;
	}

	return false ;
}
//---------------------------------------------------------------------------
void CBonTuner::InitChannelToDefault()
{
	m_Channels.clear() ;
	m_Transponders.clear() ;

	wstring space; BAND band;

	if(m_isISDBS) { // ISDB_S

		if(DEFSPACEBS) {
			space = L"BS" ;
			band = BAND_BS ;
			if(DEFSPACEBSSTREAMS>0) {
				if(DEFSPACEBSSTREAMSTRIDE) {
					for(int j=0;    j<DEFSPACEBSSTREAMS;    j++)
					for(int i=1;    i <= 23;    i+=2)
						m_Channels.push_back(
						  CHANNEL(space,band,i,L"BS"+itows(i)+L"/TS"+itows(j),j)) ;
				}else {
					for(int i=1;    i <= 23;    i+=2)
					for(int j=0;    j<DEFSPACEBSSTREAMS;    j++)
						m_Channels.push_back(
						  CHANNEL(space,band,i,L"BS"+itows(i)+L"/TS"+itows(j),j)) ;
				}
			}else {
				for(int i=1;    i <= 23;    i+=2)
					m_Channels.push_back(
					  CHANNEL(space,band,i,L"BS"+itows(i))) ;
			}
			for(int i=1;	i <= 23;	i+=2)
			for(int j=0;	j<max<int>(DEFSPACEBSSTREAMS,1);	j++)
				m_Transponders.push_back(
					CHANNEL(space,band,i,L"BS"+itows(i))) ;
		}

		if(DEFSPACECS110) {
			space = L"CS110" ;
			band = BAND_ND ;
			if(DEFSPACECS110STREAMS>0) {
				if(DEFSPACECS110STREAMSTRIDE) {
					for(int j=0;    j<DEFSPACECS110STREAMS; j++)
					for(int i=2;    i <= 24;    i+=2)
						m_Channels.push_back(
						  CHANNEL(space,band,i,L"ND"+itows(i)+L"/TS"+itows(j),j)) ;
				}else {
					for(int i=2;    i <= 24;    i+=2)
					for(int j=0;    j<DEFSPACECS110STREAMS; j++)
						m_Channels.push_back(
						  CHANNEL(space,band,i,L"ND"+itows(i)+L"/TS"+itows(j),j)) ;
				}
			}else {
				for(int i=2;    i <= 24;    i+=2)
					m_Channels.push_back(
					  CHANNEL(space,band,i,L"ND"+itows(i))) ;
			}
			for(int i=2;	i <= 24;	i+=2)
			for(int j=0;	j<max<int>(DEFSPACECS110STREAMS,1);	j++)
				m_Transponders.push_back(
					CHANNEL(space,band,i,L"ND"+itows(i))) ;
		}

	}else { // ISDB_T

		if(DEFSPACEUHF) {
			space = L"UHF" ;
			band = BAND_VU ;
			for(int i=13;   i<=62;  i++)
				m_Channels.push_back(
				  CHANNEL(space,band,i,itows(i)+L"ch")) ;
		}

		if(DEFSPACECATV) {
			space = L"CATV" ;
			band = BAND_VU ;
			for(int i=13;   i<=63;  i++)
				m_Channels.push_back(
				  CHANNEL(space,band,i+100,L"C"+itows(i)+L"ch")) ;
		}

		/*
		if(DEFSPACEVHF) {
			space = L"VHF" ;
			band = BAND_VU ;
			for(int i=1;    i<=12;  i++)
				m_Channels.push_back(
				  CHANNEL(space,band,i,itows(i)+L"ch")) ;
		}
		*/

		copy(m_Channels.begin(), m_Channels.end(), back_inserter(m_Transponders)) ;
	}
}
//---------------------------------------------------------------------------
// 通しチャンネル番号から空間番号取得
DWORD CBonTuner::space_index_of(DWORD sch) const
{
  for(size_t i=m_SpaceAnchors.size();i>0;i--)
	if(m_SpaceAnchors[i-1]<sch)
	  return DWORD(i-1) ;
  return 0 ;
}
//-----
// 通しチャンネル番号から空間チャンネル番号取得
DWORD CBonTuner::channel_index_of(DWORD sch) const
{
  if(sch>=m_ChannelAnchors.size()) return 0 ;
  return m_ChannelAnchors[sch] - m_SpaceAnchors[space_index_of(sch)] ;
}
//-----
// 有効でない空間番号かどうか
BOOL CBonTuner::is_invalid_space(DWORD spc) const
{
  if(spc>=m_SpaceAnchors.size()) return TRUE ;
  std::wstring space_name = m_Channels[m_SpaceAnchors[spc]].Space ;
  return m_InvalidSpaces.find(space_name)==m_InvalidSpaces.end() ? FALSE:TRUE ;
}
//-----
// トランスポンダのインデックスを返す
int CBonTuner::transponder_index_of(DWORD dwSpace, DWORD dwTransponder) const
{
  if(m_Transponders.empty())
    return -1 ; // transponder is not entried

  if(is_invalid_space(dwSpace))
    return -1 ; // space is over

  size_t si = m_SpaceAnchors[dwSpace] ;

  if(m_Transponders[si].Band!=BAND_BS && m_Transponders[si].Band!=BAND_ND)
    return -1 ; // transponder is not supported

  size_t tp = 0 , i = si ;
  for(int ch=m_Transponders[si].Channel; tp<dwTransponder&&i<m_Transponders.size(); i++) {
    if(m_Transponders[i].Space != m_Transponders[si].Space) break ;
    if(m_Transponders[i].Channel != ch) {
      ch = m_Transponders[i].Channel ; tp++;
      if(tp==dwTransponder) break;
    }
  }

  if(tp!=dwTransponder)
    return -1 ; // transponder is not found

  return (int) i ;
}
//-----
// チャンネルを選択する
BOOL CBonTuner::select_ch(const CHANNEL &ch, BOOL doSetFreq, BOOL doSetTSID)
{
	BOOL tuned = FALSE;
	if(doSetFreq&&doSetTSID) {
		m_CmdClient->CmdSetChannel(
			tuned, ch.PtDrvChannel(), ch.TSID, ch.Stream ) ;
	}else do {
		if(doSetFreq) {
			tuned = m_CmdClient->CmdSetFreq(ch.PtDrvChannel());
			if(!tuned) break;
		}
		if(doSetTSID) {
			if(!ch.TSID) {
				m_CmdClient->CmdSetChannel(
					tuned, ch.PtDrvChannel(), ch.TSID, ch.Stream ) ;
			}else {
				tuned = m_CmdClient->CmdSetIdS(ch.TSID);
			}
			if(!tuned) break;
		}
	}while(0);

	return tuned ;
}
//---------------------------------------------------------------------------
void CBonTuner::RebuildChannels()
{
  // チャンネル並べ替え
  struct space_finder : public std::unary_function<CHANNEL, bool> {
	std::wstring space ;
	space_finder(std::wstring space_) {
	  space = space_ ;
	}
	bool operator ()(const CHANNEL &ch) const {
	  return space == ch.Space;
	}
  };
  if (!m_InvisibleSpaces.empty() || !m_SpaceArrangement.empty()) {
	CHANNELS newChannels ;
	//CHANNELS oldChannels(m_Channels) ;
	CHANNELS &oldChannels = m_Channels ;
	CHANNELS::iterator beg = oldChannels.begin() ;
	CHANNELS::iterator end = oldChannels.end() ;
	for (CHANNELS::size_type i = 0; i < m_InvisibleSpaces.size(); i++) {
	  end = remove_if(beg, end, space_finder(m_InvisibleSpaces[i]));
	}
	for (CHANNELS::size_type i = 0; i < m_SpaceArrangement.size(); i++) {
	  space_finder finder(m_SpaceArrangement[i]) ;
	  remove_copy_if(beg, end, back_inserter(newChannels), not1(finder)) ;
	  end = remove_if(beg, end, finder) ;
	}
	copy(beg, end, back_inserter(newChannels)) ;
	m_Channels.swap(newChannels) ;
  }
  // チャンネルアンカー構築
  m_SpaceAnchors.clear() ;
  m_ChannelAnchors.clear() ;
  wstring space = L"" ;
  for (CHANNELS::size_type i = 0;i < m_Channels.size();i++) {
	if (m_Channels[i].Space != space) {
	  space = m_Channels[i].Space ;
	  m_SpaceAnchors.push_back(DWORD(i)) ;
	}
	if (m_InvalidSpaces.find(space) == m_InvalidSpaces.end())
	  m_ChannelAnchors.push_back(DWORD(i)) ;
  }
}
//---------------------------------------------------------------------------
const BOOL CBonTuner::SetRealChannel(const DWORD dwCh)
{
	if(!m_CmdClient) return FALSE ;

	//ストリーム一時停止
	m_CmdClient->CmdSetStreamEnable(FALSE);
	StopAsyncTsThread();

	if(dwCh >= m_Channels.size()) {
		return FALSE;
	}

	//チューニング
	BOOL tuned = select_ch(m_Channels[dwCh]) ;

	//ストリーム再開
	if(tuned) {
		m_chCur = m_Channels[dwCh] ;
		if(m_CmdClient->CmdSetStreamEnable(TRUE))
			StartAsyncTsThread();
	}

	if (SETCHDELAY)
		Sleep(SETCHDELAY);

	// 撮り溜めたTSストリームの破棄
	PurgeTsStream();

	return tuned ;
}
//---------------------------------------------------------------------------
BOOL CBonTuner::LoadTuner()
{
	if(hBonTunersMutex)
		WaitForSingleObject(hBonTunersMutex , INFINITE);

	WCHAR bonName[100];

	string ctrl_exe = m_strPath + PTXWDMCTRL_EXE ;
	bool ctrl_exists = file_is_existed(ctrl_exe) ;

	auto launch_ctrl = [&]() -> HANDLE {
		if(!file_is_existed(ctrl_exe)) return NULL;
		PROCESS_INFORMATION pi;
		STARTUPINFOA si;
		ZeroMemory(&si,sizeof(si));
		si.cb=sizeof(si);
		string cmdline = "\""+ctrl_exe+"\" "+wcs2mbcs(bonName);
		BOOL bRet = CreateProcessA( NULL, (LPSTR)cmdline.c_str(), NULL
			, NULL, FALSE, GetPriorityClass(GetCurrentProcess()), NULL, NULL, &si, &pi );
		if(bRet) {
			//SetThreadPriority(pi.hThread,ASYNCTSRECVTHREADPRIORITY);
			DBGOUT("AUX Control program {%s} launched.\n", cmdline.c_str());
		}
		if(pi.hThread&&pi.hThread!=INVALID_HANDLE_VALUE) CloseHandle(pi.hThread);
		if(!bRet&&pi.hProcess&&pi.hProcess!=INVALID_HANDLE_VALUE)
		{ CloseHandle(pi.hProcess); pi.hProcess=NULL; }
		if(pi.hProcess==INVALID_HANDLE_VALUE) return NULL;
		return pi.hProcess ;
	};

	do {

		int num = CPtDrvWrapper::TunerCount() ;
		if(!num||num<=m_iTunerId) break ;

		if(m_iTunerId>=0) { // ID固定
			swprintf_s(bonName,BONTUNER_STATIC_FORMAT,(m_isISDBS?L'S':L'T'),m_iTunerId) ;
			HANDLE mutex = MakeMutexDacl(bonName);
			if(mutex==NULL) {
				if(!TRYSPARES) break ;
			}else {
				auto client = new CPTxWDMCmdOperator(bonName);
				m_hCtrlProcess=launch_ctrl();
				if(!m_hCtrlProcess) client->Uniqulize(new CPTxWDMCmdServiceOperator(bonName));
				if(!client->CmdOpenTuner(m_isISDBS,m_iTunerId)) {
					delete client;
					CloseHandle(mutex);
					if(!TRYSPARES) break ;
				}else {
					m_CmdClient = client;
					m_iTunerStaticId = m_iTunerId;
					m_strTunerStaticName = bonName;
					m_hTunerMutex = mutex;
				}
			}
		}

		if(!m_CmdClient) { // ID自動割当
			for(int i=0;i<num;i++) {
				swprintf_s(bonName,BONTUNER_STATIC_FORMAT,(m_isISDBS?L'S':L'T'),i) ;
				HANDLE mutex = MakeMutexDacl(bonName);
				if(mutex==NULL) {
					CloseHandle(mutex);
					continue ;
				}
				auto client = new CPTxWDMCmdOperator(bonName);
				m_hCtrlProcess=launch_ctrl();
				if(!m_hCtrlProcess) client->Uniqulize(new CPTxWDMCmdServiceOperator(bonName));
				if(!client->CmdOpenTuner(m_isISDBS,i)) {
					delete client;
					CloseHandle(mutex);
					continue;
				}
				m_CmdClient = client;
				m_iTunerStaticId = i;
				m_strTunerStaticName = bonName;
				m_hTunerMutex = mutex;
				break;
			}
		}

	}while(0);

	if( m_CmdClient ) {
		SERVER_SETTINGS SrvOpt;
		SrvOpt.dwSize = sizeof(SERVER_SETTINGS);
		SrvOpt.CtrlPackets = CTRLPACKETS ;
		SrvOpt.StreamerThreadPriority = ASYNCTSRECVTHREADPRIORITY ;
		SrvOpt.MAXDUR_FREQ = MAXDUR_FREQ ;
		SrvOpt.MAXDUR_TMCC = MAXDUR_TMCC ;
		SrvOpt.MAXDUR_TSID = MAXDUR_TSID ;
		SrvOpt.StreamerPacketSize = ASYNCTSPACKETSIZE ;
		SrvOpt.LNB11V = LNB11V ;
		if(!m_CmdClient->CmdSetupServer(&SrvOpt)) {
			MessageBeep(MB_ICONEXCLAMATION);
			delete m_CmdClient ;
			m_CmdClient=NULL;
		}
	}

	if(hBonTunersMutex)
		ReleaseMutex(hBonTunersMutex);

	return m_CmdClient !=NULL ? TRUE : FALSE ;
}
//---------------------------------------------------------------------------
void CBonTuner::UnloadTuner()
{
	if(hBonTunersMutex)
		WaitForSingleObject(hBonTunersMutex , INFINITE);

	if(m_CmdClient) {
		if(USELNB) {
			ChangeLnbPower(FALSE);
		}
		m_CmdClient->CmdSetStreamEnable(FALSE);
		m_CmdClient->CmdSetTunerSleep(TRUE);
		m_CmdClient->CmdTerminate();
		if(m_hCtrlProcess) {
			WaitForSingleObject(m_hCtrlProcess,PTXWDMCMDTIMEOUT);
			CloseHandle(m_hCtrlProcess);
			m_hCtrlProcess=NULL;
		}
		if(m_hTunerMutex) {
			CloseHandle(m_hTunerMutex);
			m_hTunerMutex = NULL;
		}
		delete m_CmdClient ;
		m_CmdClient = NULL ;
	}
	if(m_hLnbMutex) {
		CloseHandle(m_hLnbMutex);
		m_hLnbMutex = NULL;
	}
	m_iTunerStaticId = -1 ;

	if(hBonTunersMutex)
		ReleaseMutex(hBonTunersMutex);

}
//---------------------------------------------------------------------------
const BOOL CBonTuner::ChangeLnbPower(BOOL power)
{
	if(!m_CmdClient||m_iTunerStaticId<0) return FALSE ;

	if(hBonTunersMutex)
		WaitForSingleObject(hBonTunersMutex , INFINITE);

	const char mutexFormat[] = "BonDriver_PTxWDM LNB Power %d" ;
	string mutexName = str_printf(mutexFormat, m_iTunerStaticId);
	int anotherId = (m_iTunerStaticId&~1) | (m_iTunerStaticId&1?0:1) ;
	string anotherMutexName = str_printf(mutexFormat, anotherId);

	if(m_isISDBS) {
		if(power) {
			if(!m_hLnbMutex) {
				m_hLnbMutex = MakeMutexDaclA(mutexName,TRUE);
			}
		}else {
			//対のミューテックスを開いてLnb使用中か確認する
			HANDLE hAnother = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, anotherMutexName.c_str());
			if(hAnother) {
				//対のチューナーがLnb電源供給中なので、電源OFFしない
				power=TRUE;
				CloseHandle(hAnother);
			}
			if(m_hLnbMutex) {
				CloseHandle(m_hLnbMutex);
				m_hLnbMutex=NULL;
			}
		}
	}else {
		power=FALSE;
		if(HANDLE hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, mutexName.c_str())) {
			power=TRUE;
			CloseHandle(hMutex);
		}
		if(HANDLE hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, anotherMutexName.c_str())) {
			power=TRUE;
			CloseHandle(hMutex);
		}
	}

	m_CmdClient->CmdSetLnbPower(power);

	if(hBonTunersMutex)
		ReleaseMutex(hBonTunersMutex);

	return TRUE ;
}
//---------------------------------------------------------------------------
int CBonTuner::AsyncTsThreadProcMain()
{
	CPTxWDMCmdServiceOperator *uniserver
		= static_cast<CPTxWDMCmdServiceOperator*>(m_CmdClient->UniqueServer());

	DBGOUT("--- Start Client Streaming ---\n");

	BOOL &terminated = m_bAsyncTsTerm ;

	suspend_preventer sp(this);

	if(uniserver) {

		while(!terminated) {
			if(uniserver->CurStreamSize()<ASYNCTSPACKETSIZE) {Sleep(10);continue;}
			auto cache = m_AsyncTSFifo->BeginWriteBack(ASYNCTSFIFOALLOCWAITING?true:false) ;
			if(cache) {
				DWORD sz=ASYNCTSPACKETSIZE;
				if(uniserver->GetStreamData(cache->data(),sz))
					cache->resize(sz);
				else
					cache->resize(0);
				if(m_AsyncTSFifo->FinishWriteBack(cache))
					m_evAsyncTsStream.set();
			}
		}

		DBGOUT("--- Client UniServer Streaming Finished ---\n");

	}else {

		CSharedTransportStreamer streamer(m_strTunerStaticName+PTXWDMSTREAMER_SUFFIX,
			TRUE,ASYNCTSPACKETSIZE,CTRLPACKETS);

		DWORD rem=0;
		while(!terminated) {
			DWORD wait_res = rem ? WAIT_OBJECT_0 : streamer.WaitForCmd(ASYNCTSRECVTHREADWAIT);
			if(wait_res==WAIT_TIMEOUT) continue;
			if(wait_res==WAIT_OBJECT_0) {
				DWORD sz=0;
				auto cache = m_AsyncTSFifo->BeginWriteBack(ASYNCTSFIFOALLOCWAITING?true:false) ;
				if(cache) {
					if(streamer.Rx(cache->data(),sz,ASYNCTSRECVTHREADWAIT))
						cache->resize(sz);
					else
						cache->resize(0);
					if(m_AsyncTSFifo->FinishWriteBack(cache))
						m_evAsyncTsStream.set();
				}
				if(!rem)
					rem=streamer.PacketRemain(ASYNCTSRECVTHREADWAIT);
				else
					rem--;
			}else break;
		}

		DBGOUT("--- Client Streamer Finished ---\n");

	}

	return 0;
}
//---------------------------------------------------------------------------
unsigned int __stdcall CBonTuner::AsyncTsThreadProc (PVOID pv)
{
	auto this_ = static_cast<CBonTuner*>(pv) ;
	unsigned int result = this_->AsyncTsThreadProcMain() ;
	_endthreadex(result) ;
	return result;
}
//---------------------------------------------------------------------------
void CBonTuner::StartAsyncTsThread()
{
	if(!m_AsyncTSFifo||!m_CmdClient) return;
	auto &Thread = m_hAsyncTsThread;
	if(Thread != INVALID_HANDLE_VALUE) return /*active*/;
	Thread = (HANDLE)_beginthreadex(NULL, 0, AsyncTsThreadProc, this,
		CREATE_SUSPENDED, NULL) ;
	if(Thread != INVALID_HANDLE_VALUE) {
		SetThreadPriority(Thread,ASYNCTSRECVTHREADPRIORITY);
		m_bAsyncTsTerm=FALSE;
		::ResumeThread(Thread) ;
	}
}
//---------------------------------------------------------------------------
void CBonTuner::StopAsyncTsThread()
{
	if(!m_AsyncTSFifo||!m_CmdClient) return;
	auto &Thread = m_hAsyncTsThread;
	if(Thread == INVALID_HANDLE_VALUE) return /*inactive*/;
	m_bAsyncTsTerm=TRUE;
	if(::WaitForSingleObject(Thread,30000) != WAIT_OBJECT_0) {
		::TerminateThread(Thread, 0);
	}
	CloseHandle(Thread);
	Thread = INVALID_HANDLE_VALUE ;
}
//---------------------------------------------------------------------------
void CBonTuner::PreventSuspending(BOOL bInner)
{
	if(!PREVENTSUSPENDING) return ;
	if(bInner) {
		SetThreadExecutionState(
			ES_CONTINUOUS|ES_SYSTEM_REQUIRED|ES_AWAYMODE_REQUIRED);
	}else {
		SetThreadExecutionState(ES_CONTINUOUS);
	}
}
//---------------------------------------------------------------------------
  // IBonDriver
//-----
const BOOL CBonTuner::OpenTuner(void)
{
	for(bool retry=false;;) {

		if(!m_bLoaded) {
			for(auto t=Elapsed();Elapsed(t)<RETRYDUR;Sleep(10)) {
				if(LoadTuner())  { m_bLoaded = true ; break ; }
			}
			if(!m_bLoaded)
				return FALSE;
		}

		if(m_CmdClient) {
			if(!m_CmdClient->CmdSetTunerSleep(FALSE,5000)) {
				UnloadTuner();
				m_bLoaded=false;
				if(!retry) {
					retry=true;
					continue;
				}
				return FALSE;
			}
			if(USELNB) {
				ChangeLnbPower(TRUE);
			}
		}

		break;
	}

	return m_CmdClient!=NULL ? TRUE : FALSE ;
}
//-----
void CBonTuner::CloseTuner(void)
{
	if(m_CmdClient) {
		if(USELNB) {
			ChangeLnbPower(FALSE);
		}
		m_CmdClient->CmdSetStreamEnable(FALSE);
		StopAsyncTsThread();
		m_CmdClient->CmdSetTunerSleep(TRUE);
		m_chCur = CHANNEL();
	}

	if(!PRELOAD) {
		UnloadTuner();
		m_bLoaded=false;
	}
}
//-----
const BOOL CBonTuner::SetChannel(const BYTE bCh)
{
	return TRUE;
}
//-----
const float CBonTuner::GetSignalLevel(void)
{
	if(!m_CmdClient) return -1.f;
	if(!m_hasStream) return 0.f;

	DWORD    cn100, curAgc, maxAgc;
	m_CmdClient->CmdGetCnAgc(cn100, curAgc, maxAgc);

	return cn100/100.f;
}
//-----
const DWORD CBonTuner::WaitTsStream(const DWORD dwTimeOut)
{
	if(!m_AsyncTSFifo) return WAIT_ABANDONED;

	if(!m_AsyncTSFifo->Empty()) return WAIT_OBJECT_0;
	if(!m_CmdClient) return WAIT_ABANDONED;

	const DWORD dwRet = m_evAsyncTsStream.wait(dwTimeOut);

	switch(dwRet){
	case WAIT_ABANDONED:
		return WAIT_ABANDONED;

	case WAIT_OBJECT_0:
	case WAIT_TIMEOUT:
		return dwRet;
	}

	return WAIT_FAILED;
}
//-----
const DWORD CBonTuner::GetReadyCount(void)
{
	if(!m_AsyncTSFifo) return 0;

	return (DWORD)m_AsyncTSFifo->Size();
}
//-----
const BOOL CBonTuner::GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	if(!m_CmdClient||!m_AsyncTSFifo) return FALSE;

	BYTE *pSrc = NULL;

	if(GetTsStream(&pSrc, pdwSize, pdwRemain)){
		if(*pdwSize&&pSrc)
			CopyMemory(pDst, pSrc, *pdwSize);
		return TRUE;
	}

	return FALSE;
}
//-----
const BOOL CBonTuner::GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	if(!m_CmdClient||!m_AsyncTSFifo) return FALSE;

	m_AsyncTSFifo->Pop(ppDst,pdwSize,pdwRemain) ;
    return TRUE;
}
//-----
void CBonTuner::PurgeTsStream(void)
{
	if(m_CmdClient) m_CmdClient->CmdPurgeStream();
	if(m_AsyncTSFifo) m_AsyncTSFifo->Purge() ;
}
//-----
void CBonTuner::Release(void)
{
	delete this ;
}
//---------------------------------------------------------------------------
  // IBonDriver2
//-----
LPCTSTR CBonTuner::GetTunerName(void)
{
	return m_strTunerName.c_str();
}
//-----
const BOOL CBonTuner::IsTunerOpening(void)
{
	return m_CmdClient ? TRUE : FALSE ;
}
//-----
LPCTSTR CBonTuner::EnumTuningSpace(const DWORD dwSpace)
{
	if(m_SpaceAnchors.size()<=dwSpace)
		return NULL ;
	return m_Channels[m_SpaceAnchors[dwSpace]].Space.c_str() ;
}
//-----
LPCTSTR CBonTuner::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel)
{
	if (is_invalid_space(dwSpace))
		return NULL;

	DWORD start = m_SpaceAnchors[dwSpace] ;
	DWORD end = dwSpace + 1 >= m_SpaceAnchors.size() ? (DWORD)m_Channels.size() : m_SpaceAnchors[dwSpace + 1];

	if(dwChannel<end-start)
		return m_Channels[start+dwChannel].Name.c_str() ;

	return NULL ;
}
//-----
const BOOL CBonTuner::SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	if (is_invalid_space(dwSpace))
		return FALSE;

	DWORD start = m_SpaceAnchors[dwSpace] ;
	DWORD end = dwSpace + 1 >= m_SpaceAnchors.size() ? (DWORD)m_Channels.size() : m_SpaceAnchors[dwSpace + 1];

	m_hasStream = FALSE;
	if(dwChannel<end-start) {
	  BOOL tuned = SetRealChannel(start+dwChannel) ;
	  if( tuned ){
		m_dwCurSpace = dwSpace;
		m_dwCurChannel = dwChannel;
		m_hasStream = TRUE;
	  }
	  if(FASTSCAN) return tuned ? TRUE : FALSE ;
	  return TRUE ;
	}
	return FALSE ;
}
//-----
const DWORD CBonTuner::GetCurSpace(void)
{
	return m_dwCurSpace;
}
//-----
const DWORD CBonTuner::GetCurChannel(void)
{
	return m_dwCurChannel;
}
//---------------------------------------------------------------------------
  // IBonDriver3
//-----
const DWORD CBonTuner::GetTotalDeviceNum(void)
{
	if(m_iTunerId>=0) return 1;
	if(m_CmdClient) {
		CPTxWDMCmdServiceOperator *uniserver
			= static_cast<CPTxWDMCmdServiceOperator*>(m_CmdClient->UniqueServer());
		if(uniserver) return 1;
	}else if(!file_is_existed(m_strPath+PTXWDMCTRL_EXE)) return 1;
	return CPtDrvWrapper::TunerCount();
}
//-----
const DWORD CBonTuner::GetActiveDeviceNum(void)
{
	if(m_iTunerId>=0) return m_CmdClient?1:0;
	if(m_CmdClient) {
		CPTxWDMCmdServiceOperator *uniserver
			= static_cast<CPTxWDMCmdServiceOperator*>(m_CmdClient->UniqueServer());
		if(uniserver) return 1;
	}else if(!file_is_existed(m_strPath+PTXWDMCTRL_EXE)) return 0;

	int num = CPtDrvWrapper::TunerCount() ;
	int act = 0 ;

	for(int i=0;i<num;i++) {
		WCHAR mutexName[100];
		swprintf_s(mutexName,BONTUNER_STATIC_FORMAT,(m_isISDBS?L'S':L'T'),i) ;
		if(HANDLE mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, mutexName)) {
			act++; CloseHandle(mutex);
		}
	}

	return act ;
}
//-----
const BOOL CBonTuner::SetLnbPower(const BOOL bEnable)
{
	if(!BON3LNB) return TRUE;
	return ChangeLnbPower(bEnable) ;
}
//---------------------------------------------------------------------------
  // IBonTransponder
//-----
LPCTSTR CBonTuner::TransponderEnumerate(const DWORD dwSpace, const DWORD dwTransponder)
{
	int idx = transponder_index_of(dwSpace, dwTransponder) ;
	if(idx<0) return NULL ;

	return m_Transponders[idx].Name.c_str();
}
//-----
const BOOL CBonTuner::TransponderSelect(const DWORD dwSpace, const DWORD dwTransponder)
{
  if(!IsTunerOpening()) return FALSE;

  int idx = transponder_index_of(dwSpace, dwTransponder) ;
  if(idx<0) return FALSE ;

  if(m_hasStream) {
    //ストリーム一時停止
    m_CmdClient->CmdSetStreamEnable(FALSE);
    StopAsyncTsThread();
    m_hasStream = FALSE ;
  }

  //チューニング
  CHANNEL ch = m_Transponders[idx] ;
  BOOL res = select_ch(ch, TRUE, FALSE) ;

  if(res) {
    m_dwCurSpace = dwSpace;
    m_dwCurChannel = dwTransponder | TRANSPONDER_CHMASK ;
    m_chCur = ch ;
    m_hasStream = FALSE; // TransponderSetCurID はまだ行っていないので
  }

  return res ;
}
//-----
const BOOL CBonTuner::TransponderGetIDList(LPDWORD lpIDList, LPDWORD lpdwNumID)
{
  if(!IsTunerOpening()) return FALSE;
  if(m_chCur.Band!=BAND_BS&&m_chCur.Band!=BAND_ND) return FALSE;

  const DWORD numId = 8 ;

  if(lpdwNumID==NULL) {
    return FALSE ;
  }else if(lpIDList==NULL) {
    *lpdwNumID = numId ;
    return TRUE ;
  }

  TSIDLIST TSIDList ;

  BOOL res = m_CmdClient->CmdGetIdListS(TSIDList);
  if(res) {
	  DWORD num = min(8,*lpdwNumID) ;
	  for(DWORD i=0;i<num;i++) {
	    DWORD id = TSIDList.Id[i] ;
		if(id==0||id==0xffff) id = 0xFFFFFFFF ;
		lpIDList[i] = id ;
	  }
	  *lpdwNumID = num ;
  }

  return res ;
}
//-----
const BOOL CBonTuner::TransponderSetCurID(const DWORD dwID)
{
  if(!IsTunerOpening()) return FALSE;
  if(m_chCur.Band!=BAND_BS&&m_chCur.Band!=BAND_ND) return FALSE;

  if(m_hasStream) {
    //ストリーム一時停止
    m_CmdClient->CmdSetStreamEnable(FALSE);
    StopAsyncTsThread();
    m_hasStream=FALSE ;
  }

  CHANNEL ch = m_chCur ;
  ch.TSID = (WORD) (dwID&0xFFFF) ;
  BOOL res = select_ch(ch, FALSE, TRUE) ;

  if(res) {
    m_chCur = ch ;
    m_hasStream = TRUE ;
	//ストリーム再開
	if(m_CmdClient->CmdSetStreamEnable(TRUE))
		StartAsyncTsThread();
  }

  PurgeTsStream();

  return res ;
}
//-----
const BOOL CBonTuner::TransponderGetCurID(LPDWORD lpdwID)
{
  if(!IsTunerOpening()||!lpdwID) return FALSE;
  if(m_chCur.Band!=BAND_BS&&m_chCur.Band!=BAND_ND) return FALSE;

  if(!m_hasStream) {
    *lpdwID=0xFFFFFFFF;
    return TRUE;
  }

  DWORD id=0;
  BOOL res = m_CmdClient->CmdGetIdS(id) ;
  if(res) {
    if(id==0||id==0xffff) id = 0xFFFFFFFF ;
	*lpdwID=id;
  }
  return res ;
}
//===========================================================================
// static initializer

class bon_static_initializer_t {
public:
	bon_static_initializer_t() {
		InitializeBonTuners();
	}
	~bon_static_initializer_t() {
		FinalizeBonTuners();
	}
};

static bon_static_initializer_t static_initializer;

//===========================================================================
