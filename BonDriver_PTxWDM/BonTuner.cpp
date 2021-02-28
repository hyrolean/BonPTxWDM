//===========================================================================
#include "stdafx.h"
#include <iterator>
#include <cstdio>

#include "BonTuner.h"
//---------------------------------------------------------------------------

using namespace std;

//===========================================================================
  // BonTuners
//---------------------------------------------------------------------------

static CRITICAL_SECTION secBonTuners;
static set<CBonTuner*> BonTuners ;

//---------------------------------------------------------------------------
void InitializeBonTuners(HMODULE hModule)
{
	::InitializeCriticalSection(&secBonTuners);
	CBonTuner::HModule = hModule;
}
//---------------------------------------------------------------------------
void FinalizeBonTuners()
{
	::EnterCriticalSection(&secBonTuners);
	vector<CBonTuner*> clone;
	copy(BonTuners.begin(),BonTuners.end(),back_inserter(clone));
	for(auto bon: clone) if(bon!=NULL) bon->Release();
	::LeaveCriticalSection(&secBonTuners);
	::DeleteCriticalSection(&secBonTuners);
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
	InitTunerProperty();
}
//---------------------------------------------------------------------------
CBonTuner::~CBonTuner()
{
	UnloadTuner();

	::EnterCriticalSection(&secBonTuners);
	BonTuners.erase(this);
	::LeaveCriticalSection(&secBonTuners);
}
//---------------------------------------------------------------------------
void CBonTuner::InitTunerProperty()
{

	// 値を初期化
	m_pcTuner = NULL;
	m_hLnbMutex = NULL;
	m_dwCurSpace = 0xFF;
	m_dwCurChannel = 0xFF;
	m_iTunerId = m_iTunerStaticId = -1;

	// フラグの初期化
	USELNB = FALSE;
	BON3LNB = FALSE;
	FASTSCAN = FALSE;
	TRYSPARES = FALSE;
	SETCHDELAY = 0;

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
	LoadIniFile(string(szMyDrive)+string(szMyDir)+"BonDriver_PTxWDM.ini") ;
	LoadIniFile(string(szMyDrive)+string(szMyDir)+string(szMyName)+".ini") ;


	// Channel ファイルをロード
	if(!LoadChannelFile(string(szMyDrive)+string(szMyDir)+string(szMyName)+".ch.txt")) {
	  if(!LoadChannelFile(string(szMyDrive)+string(szMyDir)+"BonDriver_PTxWDM-"+string(m_isISDBS?"S":"T")+".ch.txt")) {
		if(!LoadChannelFile(string(szMyDrive)+string(szMyDir)+"BonDriver_PTxWDM.ch.txt")) {
		  InitChannelToDefault() ;
		}
	  }
	}


	// チャンネル情報再構築
	RebuildChannels() ;


	LoadTuner() ;

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
  LOADINT(BON3LNB);
  LOADINT(TRYSPARES);
  LOADINT(FASTSCAN);
  LOADINT(SETCHDELAY);

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
  m_Channels.clear() ;
  //m_SpaceIndices.clear() ;

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
		m_Channels.push_back(
		  CHANNEL(space,freq,name,stream,tsid)) ;
	  else*/ if(band!=BAND_na&&channel>0) {
		CHANNEL channel(space,band,channel,name,stream,tsid);
		if(!channel.isISDBS()==!m_isISDBS) m_Channels.push_back(channel) ;
	  }else
		continue ;
	  if(space_name!=space) {
		//m_SpaceIndices.push_back(m_Channels.size()-1) ;
		space_name=space ;
	  }
	}
  }

  fclose(st) ;

  return true ;
}
//---------------------------------------------------------------------------
void CBonTuner::InitChannelToDefault()
{
	m_Channels.clear() ;

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
	const DWORD MAXDUR_FREQ = 1000; //周波数調整に費やす最大時間(msec)
	const DWORD MAXDUR_TMCC = 1500; //TMCC取得に費やす最大時間(msec)
	const DWORD MAXDUR_TSID = 3000; //TSID設定に費やす最大時間(msec)

	if(!m_pcTuner) return FALSE ;

	//ストリーム一時停止
	//is_channel_valid = FALSE;
	m_pcTuner->SetStreamEnable(false);

	if(dwCh >= m_Channels.size()) {
		return FALSE;
	}

	//チューニング
	bool tuned=false ;

	do {

		{
			bool done=false ;
			for (DWORD t=0,s=Elapsed(),n=0; t<MAXDUR_FREQ; t=Elapsed(s)) {
				if(m_pcTuner->SetFrequency(m_Channels[dwCh].PtDrvChannel()))
				{done=true;break;}
			}
			if(!done) break ;
		}

		Sleep(60);

		if(m_Channels[dwCh].isISDBS()) {
			WORD tsid = m_Channels[dwCh].TSID ;
			WORD stream = m_Channels[dwCh].Stream ;
			if(!tsid) {
				for (DWORD t=0,s=Elapsed(); t<MAXDUR_TMCC; t=Elapsed(s)) {
					TMCC_STATUS tmcc;
					ZeroMemory(&tmcc,sizeof(tmcc));
					//std::fill_n(tmcc.Id,8,0xffff) ;
					if(m_pcTuner->GetTmcc(&tmcc)) {
						for (int i=0; i<8; i++) {
							WORD id = tmcc.u.bs.tsId[i]&0xffff;
							if ((id&0xff00) && (id^0xffff)) {
								if( (id&7) == stream ) { //ストリームに一致した
									//一致したidに書き換える
									tsid = id ;
									break;
								}
							}
						}
						if(tsid&~7UL) break ;
					}
					Sleep(50);
				}
			}
			if(!tsid) break ;
			for (DWORD t=0,s=Elapsed(),n=0; t<MAXDUR_TSID ; t=Elapsed(s)) {
				if(m_pcTuner->SetIdS(tsid)) { if(++n>=2) {tuned=true;break;} }
				Sleep(50);
			}
		}else {
			TMCC_STATUS tmcc;
			for (DWORD t=0,s=Elapsed(); t<MAXDUR_TMCC; t=Elapsed(s)) {
				if(m_pcTuner->GetTmcc(&tmcc)) {tuned=true;break;}
				Sleep(50);
			}
		}

	}while(0);

	//ストリーム再開
	//is_channel_valid = tuned ? TRUE : FALSE ;
	m_pcTuner->SetStreamEnable(true);

	if (SETCHDELAY)
		Sleep(SETCHDELAY);

	// 撮り溜めたTSストリームの破棄
	PurgeTsStream();

	return tuned ? TRUE : FALSE ;
}
//---------------------------------------------------------------------------
BOOL CBonTuner::LoadTuner()
{

	int num = CPtDrvWrapper::TunerCount() ;
	if(!num||num<=m_iTunerId) return FALSE ;


	if(m_iTunerId>=0) {
		auto tuner = new CPtDrvWrapper(m_isISDBS,m_iTunerId) ;
		if(!tuner->HandleAllocated()) {
			delete tuner ;
			if(!TRYSPARES) return FALSE ;
		}
		else {
			m_pcTuner = tuner ;
			m_iTunerStaticId = m_iTunerId;
		}
	}
	if(!m_pcTuner) {
		for(int i=0;i<num;i++) {
			auto tuner = new CPtDrvWrapper(m_isISDBS,i) ;
			if(!tuner->HandleAllocated())
				delete tuner ;
			else {
				m_pcTuner = tuner ;
				m_iTunerStaticId = i;
				break;
			}
		}
	}

	return m_pcTuner!=NULL ? TRUE : FALSE ;
}
//---------------------------------------------------------------------------
void CBonTuner::UnloadTuner()
{
	CloseTuner();
	if(m_pcTuner) {
		delete m_pcTuner ;
		m_pcTuner = NULL ;
	}
	if(m_hLnbMutex) {
		ReleaseMutex(m_hLnbMutex);
		CloseHandle(m_hLnbMutex);
		m_hLnbMutex = NULL;
	}
}
//---------------------------------------------------------------------------
const BOOL CBonTuner::ChangeLnbPower(BOOL power)
{
	if(!m_isISDBS||!m_pcTuner||m_iTunerStaticId<0) return FALSE ;

	wstring mutexFormat = L"BonDriver_PTxWDM LNB Power %d" ;
	WCHAR wcsMutexName[100],wcsAnotherMutexName[100];
	swprintf_s(wcsMutexName,mutexFormat.c_str(),m_iTunerStaticId);
	int anotherId = (m_iTunerStaticId&~1) | (m_iTunerStaticId&1?0:1) ;
	swprintf_s(wcsAnotherMutexName,mutexFormat.c_str(),anotherId);

	if(power) {
		if(!m_hLnbMutex) {
			m_hLnbMutex = CreateMutex(NULL, TRUE, wcsMutexName);
			m_pcTuner->SetLnbPower(BS_LNB_POWER_15V);
		}
	}else {
		if(m_hLnbMutex) {
			//対のミューテックスを開いてLnb使用中か確認する
			HANDLE hAnother = OpenMutex(MUTEX_ALL_ACCESS, FALSE, wcsAnotherMutexName);
			if(!hAnother) {
				m_pcTuner->SetLnbPower(BS_LNB_POWER_OFF);
			}else {
				//対のチューナーがLnb電源供給中なので、電源OFFしない
				CloseHandle(hAnother);
			}
			ReleaseMutex(m_hLnbMutex);
			CloseHandle(m_hLnbMutex);
			m_hLnbMutex=NULL;
		}
	}

	return TRUE ;
}
//---------------------------------------------------------------------------
  // IBonDriver
//-----
const BOOL CBonTuner::OpenTuner(void)
{
	if(m_pcTuner) {
		m_pcTuner->SetTunerSleep(false);
		if(USELNB&&m_isISDBS) {
			ChangeLnbPower(TRUE);
		}
	}

	return m_pcTuner!=NULL ? TRUE : FALSE ;
}
//-----
void CBonTuner::CloseTuner(void)
{
	if(m_pcTuner) {
		if(USELNB&&m_isISDBS) {
			ChangeLnbPower(FALSE);
		}
		m_pcTuner->SetTunerSleep(true);
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
	if(!m_pcTuner) return -1.f;

	UINT    cn100, curAgc, maxAgc;
	m_pcTuner->GetCnAgc(&cn100, &curAgc, &maxAgc);

	return cn100/100.f;
}
//-----
const DWORD CBonTuner::WaitTsStream(const DWORD dwTimeOut)
{
	if(!m_pcTuner) return WAIT_FAILED;

	if(!m_pcTuner->IsStreamEnabled()){
		m_pcTuner->SetStreamEnable(true);
	}

	DWORD timeout = dwTimeOut ;
	if(!timeout) timeout = 60000 * 5 ;

	for(DWORD s=Elapsed();Elapsed(s)<timeout;SleepEx(10,FALSE)) {
		if(m_pcTuner->CurStreamSize())
			return WAIT_OBJECT_0 ;
	}
	return WAIT_TIMEOUT;
}
//-----
const DWORD CBonTuner::GetReadyCount(void)
{
	return m_pcTuner ? m_pcTuner->CurStreamSize() : 0UL ;
}
//-----
const BOOL CBonTuner::GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	if(!m_pcTuner) return FALSE;

	if(!m_pcTuner->IsStreamEnabled()){
		m_pcTuner->SetStreamEnable(true);
	}

	if(!pdwSize) return FALSE ;
	UINT size = *pdwSize ;
	UINT outSz = 0 ;
	*pdwSize=0;

	if(!m_pcTuner->GetStreamData(pDst,size,&outSz))
		return FALSE ;

	if(pdwRemain) *pdwRemain = m_pcTuner->CurStreamSize();

	*pdwSize = outSz;
	return (outSz!=0)? TRUE: FALSE;
}
//-----
const BOOL CBonTuner::GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	if(!m_pcTuner) return FALSE;

	DWORD sz = m_pcTuner->CurStreamSize() ;

	if(!sz) return FALSE ;

	tmpBuf.resize(sz) ;

	if(pdwSize) *pdwSize = sz ;

	if(GetTsStream(tmpBuf.data(),pdwSize,pdwRemain)) {
		*ppDst = tmpBuf.data() ;
		return TRUE ;
	}
	return FALSE ;
}
//-----
void CBonTuner::PurgeTsStream(void)
{
	if(!m_pcTuner) return;

	m_pcTuner->PurgeStream();
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
	return m_pcTuner ? TRUE : FALSE ;
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

	if(dwChannel<end-start) {
	  BOOL tuned = SetRealChannel(start+dwChannel) ;
	  if( tuned ){
		m_dwCurSpace = dwSpace;
		m_dwCurChannel = dwChannel;
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
	if(m_iTunerId<0) return 1;
	return CPtDrvWrapper::TunerCount();
}
//-----
const DWORD CBonTuner::GetActiveDeviceNum(void)
{
	if(m_iTunerId<0) return 1;
	int num = CPtDrvWrapper::TunerCount() ;
	int act = 0 ;

	for(int i=0;i<num;i++) {
		auto tuner = new CPtDrvWrapper(m_isISDBS,i) ;
		if(!m_pcTuner->HandleAllocated())
			act++;
		delete tuner ;
	}

	return act ;
}
//-----
const BOOL CBonTuner::SetLnbPower(const BOOL bEnable)
{
	if(!m_isISDBS) return FALSE;
	if(!BON3LNB) return TRUE;
	return ChangeLnbPower(bEnable) ;
}
//===========================================================================
