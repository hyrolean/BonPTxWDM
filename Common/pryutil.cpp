//===========================================================================
#include "stdafx.h"
#include <cctype>
#include <cstdarg>
#include <process.h>
#include <locale.h>
#include <Rpc.h>
#pragma comment(lib, "Rpcrt4.lib")
#ifdef PRY8EAlByw_SHLWAPI
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#endif

#include "pryutil.h"
//---------------------------------------------------------------------------
using namespace std ;

//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------

#ifdef PRY8EAlByw_HRTIMER
  #define wait_object HRWaitForSingleObject
  #define delay HRSleep
#else
  #define wait_object WaitForSingleObject
  #define delay Sleep
#endif

//===========================================================================
// Statics
//---------------------------------------------------------------------------
static errno_t splitPath(
   const string &path,
   string *pDrv=NULL,
   string *pDir=NULL,
   string *pFname=NULL,
   string *pExt=NULL,
   UINT code_page=CP_ACP
) {

  wstring wpath = mbcs2wcs(path,code_page) ;

  #define BUFALLOC(Nam) BUFFER<wchar_t> w##Nam(p##Nam?_MAX_FNAME:0)
  BUFALLOC(Drv);
  BUFALLOC(Dir);
  BUFALLOC(Fname);
  BUFALLOC(Ext);
  #undef BUFALLOC

  #define SPLPARAM(Nam) \
     (p##Nam?w##Nam.data():NULL), \
     (p##Nam?w##Nam.size():0)
  errno_t r = _wsplitpath_s(
     wpath.c_str(),
     SPLPARAM(Drv),
     SPLPARAM(Dir),
     SPLPARAM(Fname),
     SPLPARAM(Ext)
  );
  #undef SPLPARAM

  if(!r) {
    #define STOREVAL(Nam) if(p##Nam) *p##Nam = wcs2mbcs(w##Nam.data(),code_page)
    STOREVAL(Drv) ;
    STOREVAL(Dir) ;
    STOREVAL(Fname) ;
    STOREVAL(Ext) ;
    #undef STOREVAL
  }

  return r ;
}
//===========================================================================
// Functions
//---------------------------------------------------------------------------
DWORD Elapsed(DWORD start, DWORD end)
{
  DWORD result = (end>=start) ? end-start : MAXDWORD-end+1+start ;
  return result ;
}
//---------------------------------------------------------------------------
DWORD PastSleep(DWORD wait,DWORD start)
{
  if(!wait) return start ;
  DWORD past = Elapsed(start,GetTickCount()) ;
  if(wait>past) delay(wait-past);
  return start+wait ;
}
//---------------------------------------------------------------------------
wstring mbcs2wcs(string src, UINT code_page)
{
  int wLen = MultiByteToWideChar(code_page, 0, src.c_str(), (int)src.length(), NULL, 0);
  if(wLen>0) {
    BUFFER<wchar_t> wcs(wLen) ;
    MultiByteToWideChar(code_page, 0, src.c_str(), (int)src.length(), wcs.data(), wLen);
    return wstring(wcs.data(),wLen);
  }
  return wstring(L"");
}
//---------------------------------------------------------------------------
string wcs2mbcs(wstring src, UINT code_page)
{
  int mbLen = WideCharToMultiByte(code_page, 0, src.c_str(), (int)src.length(), NULL, 0, NULL, NULL) ;
  if(mbLen>0) {
    BUFFER<char> mbcs(mbLen) ;
    WideCharToMultiByte(code_page, 0, src.c_str(), (int)src.length(), mbcs.data(), mbLen, NULL, NULL);
    return string(mbcs.data(),mbLen);
  }
  return string("");
}
//---------------------------------------------------------------------------
string itos(int val,int radix)
{
  BUFFER<char> str(72) ;
  if(!_itoa_s(val,str.data(),70,radix))
    return static_cast<string>(str.data()) ;
  return "NAN" ;
}
//---------------------------------------------------------------------------
wstring itows(int val,int radix)
{
  BUFFER<wchar_t> str(72) ;
  if(!_itow_s(val,str.data(),70,radix))
    return static_cast<wstring>(str.data()) ;
  return L"NAN" ;
}
//---------------------------------------------------------------------------
string upper_case(string str)
{
  BUFFER<char> temp(str.length()+1) ;
  CopyMemory(temp.data(),str.c_str(),(str.length()+1)*sizeof(char)) ;
  _strupr_s(temp.data(),str.length()+1) ;
  return static_cast<string>(temp.data()) ;
}
//---------------------------------------------------------------------------
string lower_case(string str)
{
  BUFFER<char> temp(str.length()+1) ;
  CopyMemory(temp.data(),str.c_str(),(str.length()+1)*sizeof(char)) ;
  _strlwr_s(temp.data(),str.length()+1) ;
  return static_cast<string>(temp.data()) ;
}
//---------------------------------------------------------------------------
string uuid_string()
{
  UUID uuid_ ;
  UuidCreate(&uuid_) ;
  unsigned char * result ;
  UuidToStringA(&uuid_,&result) ;
  string str_result = (char*)result ;
  RpcStringFreeA(&result);
  return lower_case(str_result) ;
}
//---------------------------------------------------------------------------
string str_printf(const char *format, ...)
{
	va_list marker ;
	va_start( marker, format ) ;
	int edit_ln = _vscprintf(format, marker);
	if(edit_ln++>0) {
		BUFFER<char> edit_str(edit_ln);
		vsprintf_s( edit_str.data(), edit_ln, format, marker ) ;
		va_end( marker ) ;
		return string(edit_str.data()) ;
	}
	va_end( marker ) ;
	return string() ;
}
//---------------------------------------------------------------------------
string file_drive_of(string filename)
{
  string drv ;
  splitPath(filename,&drv) ;
  return drv ;
}
//---------------------------------------------------------------------------
string file_path_of(string filename)
{
  string drv, dir ;
  splitPath(filename,&drv,&dir) ;
  return drv+dir ;
}
//---------------------------------------------------------------------------
string file_name_of(string filename)
{
  string name, ext ;
  splitPath(filename,NULL,NULL,&name,&ext) ;
  return name+ext ;
}
//---------------------------------------------------------------------------
string file_prefix_of(string filename)
{
  string name;
  splitPath(filename,NULL,NULL,&name) ;
  return name ;
}
//---------------------------------------------------------------------------
string file_suffix_of(string filename)
{
  string ext ;
  splitPath(filename,NULL,NULL,NULL,&ext) ;
  return ext ;
}
//---------------------------------------------------------------------------
int file_age_of(string filename)
{
  WIN32_FIND_DATAA data ;
  HANDLE h = FindFirstFileA(filename.c_str(),&data);
  if(h == INVALID_HANDLE_VALUE) return -1 ;
  FindClose(h) ;
  if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) return -1 ;
  FILETIME local ;
  FileTimeToLocalFileTime(&data.ftLastWriteTime, &local);
  WORD d, t ;
  if(!FileTimeToDosDateTime(&local, &d, &t)) return -1 ;
  return int(DWORD(d)<<16|DWORD(t)) ;
}
//---------------------------------------------------------------------------
bool file_is_existed(string filename)
{
#ifndef PRY8EAlByw_SHLWAPI
  return file_age_of(filename.c_str()) != -1 ;
#else
  return PathFileExistsA(filename.c_str()) && !folder_is_existed(filename) ;
#endif
}
//---------------------------------------------------------------------------
bool folder_is_existed(string filename)
{
#ifndef PRY8EAlByw_SHLWAPI
  DWORD attr = GetFileAttributesA(filename.c_str()) ;
  return attr!=MAXDWORD && (attr&FILE_ATTRIBUTE_DIRECTORY) ? true : false ;
#else
  return PathIsDirectoryA(filename.c_str()) ? true : false ;
#endif
}
//===========================================================================
// acalci
//---------------------------------------------------------------------------

#define ACALCI_CONST_MATCHCASE  false

    template<typename T>
    class integer_c_expression_const_table
    {
    protected:
      class str_comp_f : public binary_function<string,string,bool> {
        bool match_case;
      public:
        str_comp_f(bool match_case_) : match_case(match_case_) {}
        str_comp_f(const str_comp_f &src) : match_case(src.match_case) {}
        bool operator()(const string lhs, const string rhs) const {
          return match_case ? lhs < rhs : lower_case(lhs) < lower_case(rhs) ;
        }
      };
      typedef map<string,T,str_comp_f> const_map_t;
      const_map_t const_map ;
      exclusive_object excl ;
    public:
      integer_c_expression_const_table(bool match_case)
        : const_map(str_comp_f(match_case)) {}
      void clear() {
        exclusive_lock lock(&excl);
        const_map.clear();
      }
      void entry(const string name, const T val) {
        exclusive_lock lock(&excl);
        const_map[name]=val;
      }
      bool find(const string name, T &val) const {
        exclusive_lock lock(const_cast<exclusive_object*>(&excl));
        const_map_t::const_iterator pos = const_map.find(name);
        if(pos==const_map.end()) return false;
        val=pos->second ;
        return true;
      }
    };

    template<typename T>
    class integer_c_expression_string_calculator
    {
    protected:
      enum token_t {
        tEND    = 0,
        tSTART  = 1,
        tVAL    = 10,
        tPLUS   = 20,  /* prec ADD */
        tMINUS  = 21,  /* prec SUB */
        tNOT    = 22,
        tFACT   = 30,
        tMUL    = 40,
        tDIV    = 41,
        tMOD    = 42,
        tADD    = 50,
        tSUB    = 51,
        tLSHIFT = 60,
        tRSHIFT = 61,
        tAND    = 70,
        tXOR    = 80,
        tOR     = 90,
        tLP     = 100,
        tRP     = 101,
      };
      struct node_t {
        token_t token ;
        T val ;
        struct node_t *next ;
        node_t(): token(tEND),val(0),next(NULL) {}
      };
      node_t *top ;
      T def_val ;
      integer_c_expression_const_table<T> *const_table ;
      bool halfway ;
      const char *es ;
    protected:
      static T strimmval(const char *s,char **endptr,int radix) {
        #if 1
        T val=0;
        while(*s) {
          int c = toupper(*s) ; T v ;
          if(c>='0'&&c<='9'&&c-'0'<radix)
            v = c-'0' ;
          else if(radix>10&&c>='A'&&c<='Z'&&c-'A'<radix-10)
            v = c-'A'+10 ;
          else break ;
          switch(radix) {
            case 2:  val<<=1; break;
            case 8:  val<<=3; break;
            case 16: val<<=4; break;
            default: val*=radix;
          }
          val += v ;
          s++;
        }
        if(endptr) *endptr=const_cast<char*>(s) ;
        return val ;
        #else
        if(sizeof(T)>sizeof(long))
          return static_cast<T>(strtoull(s,endptr,radix)); // NG: @VS2008
        return static_cast<T>(strtoul(s,endptr,radix));
        #endif
      }
      static void skip_separator(const char *&s) {
        while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++;
      }
      static void skip_val_literal(const char *&s) {
        while(*s&&(isalnum(*s)||*s=='_')) s++;
      }
      T __fastcall parse(node_t *backTK) {
        node_t nextTK;
        backTK->next = &nextTK;
        skip_separator(es);
        if(*es>='0'&&*es<='9') {
          char *e=NULL;
          nextTK.token=tVAL;
          if(es[0]=='0') {
            if(es[1]=='b'||es[1]=='B') // bin
              nextTK.val=strimmval(es+=2,&e,2);
            else if(es[1]=='x'||es[1]=='X') // hex
              nextTK.val=strimmval(es+=2,&e,16);
            else // oct
              nextTK.val=strimmval(es++,&e,8);
          }else // dec
            nextTK.val=strimmval(es++,&e,10);
          if(e) es=e;
          switch(*es) { // binary units
            case 'K': nextTK.val<<=10, es++; break ; // Ki
            case 'M': nextTK.val<<=20, es++; break ; // Mi
            case 'G': nextTK.val<<=30, es++; break ; // Gi
            case 'T': nextTK.val<<=40, es++; break ; // Ti (over int)
            case 'P': nextTK.val<<=50, es++; break ; // Pi
            case 'E':
            case 'X': nextTK.val<<=60, es++; break ; // Ei
            case 'Z': nextTK.val<<=70, es++; break ; // Zi (over __int64)
            case 'Y': nextTK.val<<=80, es++; break ; // Yi
          }
          skip_val_literal(es) ;
        }
        else if(!strncmp("**",es,2)) nextTK.token=tFACT,   es+=2;
        else if(!strncmp("<<",es,2)) nextTK.token=tLSHIFT, es+=2;
        else if(!strncmp(">>",es,2)) nextTK.token=tRSHIFT, es+=2;
        else {
          switch(*es) {
            case '+': nextTK.token=tADD; break;
            case '-': nextTK.token=tSUB; break;
            case '~': nextTK.token=tNOT; break;
            case '*': nextTK.token=tMUL; break;
            case '/': nextTK.token=tDIV; break;
            case '%': nextTK.token=tMOD; break;
            case '&': nextTK.token=tAND; break;
            case '^': nextTK.token=tXOR; break;
            case '|': nextTK.token=tOR;  break;
            case '(': nextTK.token=tLP;  break;
            case ')': nextTK.token=tRP;  break;
            case ';': es++;
            case '\0':
              return calculate(); // terminal
            default:
              if(const_table&&(isalpha(*es)||*es=='_')) { // constant
                const char *e = es ;
                skip_val_literal(e) ;
                if(const_table->find(string(es,e-es),nextTK.val)) {
                  nextTK.token=tVAL;
                  es=e-1;
                  break;
                }
              }
              return calculate(); // outside of c-expression literals
          }
          es++;
        }
        return parse(&nextTK); // parse the next token
      }
      T calculate() {
        int num=0;
        node_t *p=top->next;
        while(p&&p->token!=tEND) {
          num++;
          p=p->next;
        }
        return do_calculate(num,top->next);
      }
      static node_t* nest_node(node_t *p,int num) {
        while(num>0) p=p->next, num--;
        return p;
      }
      T __fastcall do_calculate(int &num,node_t *pos)
      {
      #define D1(a)  (p->token==(a))
      #define D2(a,b)  (D1(a)&&p->next->token==(b))
      #define D3(a,b,c) (D2(a,b)&&p->next->next->token==(c))
        if(!pos) {num=0; return def_val;}
        //PRIOR100: ( )  <- digest parenthesses at first
        node_t *p=pos;
        for(int i=0;i<num-1;i++,p=p->next) {
          if(D1(tRP)) break ;
          if(D1(tLP)) {
            num -= i+1;
            do_calculate(num,p->next);
            num += i+1;
            if(num-i>=3&&D3(tLP,tVAL,tRP)) {
              p->val= ( p->next->val );
              p->token=tVAL;
              p->next=nest_node(p,3);
              num-=2;
            }
          }
        }
        //PRIOR(20-90): digest operators ( parenthesses excluded )
        node_t *q;
        int n;
        //PRIOR20: + - ~ (single)
        do {
          p=pos, q=NULL, n=0 ;
          for (int i = 0; i < num - 1; q = p, p = p->next, i++) {
            if(D1(tRP)) break ;
            if(q&&q->token==tVAL) // NG: the LHS value is existed
              continue;
            if(D2(tADD,tVAL))       p->val= + p->next->val;
            else if(D2(tSUB,tVAL))  p->val= - p->next->val;
            else if(D2(tNOT,tVAL))  p->val= ~ p->next->val;
            else continue ;
            p->token=tVAL;
            p->next=nest_node(p,2);
            num--;
            if(q&&(q->token==tADD||q->token==tSUB||q->token==tNOT))
              n++;
          }
        }while(n>0);
        //PRIOR30: **
        p=pos;
        for(int i=0;i<num-2;) {
          if(D1(tRP)) break ;
          if(D3(tVAL,tFACT,tVAL)) {
            T val=p->val;
            q=p->next->next;
            if(q->val==0)
              p->val=1;
            else if(q->val<0)
              p->val=0;
            else while(q->val-1) {
              p->val*=val;
              q->val--;
            }
            p->next=q->next;
            num-=2;
          }
          else i++, p=p->next;
        }
        //PRIOR40: * / %
        p=pos;
        for(int i=0;i<num-2;) {
          if(D1(tRP)) break ;
          if(D3(tVAL,tMUL,tVAL))        p->val=p->val * p->next->next->val;
          else if(D3(tVAL,tDIV,tVAL))   p->val=p->val / p->next->next->val;
          else if(D3(tVAL,tMOD,tVAL))   p->val=p->val % p->next->next->val;
          else { i++;p=p->next; continue; }
          p->next=nest_node(p,3);
          num-=2;
        }
        //PRIOR50: + -
        p=pos;
        for(int i=0;i<num-2;) {
          if(D1(tRP)) break ;
          if(D3(tVAL,tADD,tVAL))        p->val=p->val + p->next->next->val;
          else if(D3(tVAL,tSUB,tVAL))   p->val=p->val - p->next->next->val;
          else { i++;p=p->next; continue; }
          p->next=nest_node(p,3);
          num-=2;
        }
        //PRIOR60: << >>
        p=pos;
        for(int i=0;i<num-2;) {
          if(D1(tRP)) break ;
          if(D3(tVAL,tLSHIFT,tVAL))      p->val=p->val << p->next->next->val;
          else if(D3(tVAL,tRSHIFT,tVAL)) p->val=p->val >> p->next->next->val;
          else { i++;p=p->next; continue; }
          p->next=nest_node(p,3);
          num-=2;
        }
        //PRIOR70: &
        p=pos;
        for(int i=0;i<num-2;) {
          if(D1(tRP)) break ;
          if(D3(tVAL,tAND,tVAL)) {
            p->val=p->val & p->next->next->val;
            p->next=nest_node(p,3);
            num-=2;
          }
          else i++, p=p->next;
        }
        //PRIOR80: ^
        p=pos;
        for(int i=0;i<num-2;) {
          if(D1(tRP)) break ;
          if(D3(tVAL,tXOR,tVAL)) {
            p->val=p->val ^ p->next->next->val;
            p->next=nest_node(p,3);
            num-=2;
          }
          else i++, p=p->next;
        }
        //PRIOR90: |
        p=pos;
        for(int i=0;i<num-2;) {
          if(D1(tRP)) break ;
          if(D3(tVAL,tOR,tVAL)) {
            p->val=p->val | p->next->next->val;
            p->next=nest_node(p,3);
            num-=2;
          }
          else i++, p=p->next;
        }
        //PRIOR10: a VAL token is finally existed at the head or not
        return pos&&pos->token==tVAL&&(halfway||num==1)? pos->val: def_val;
      #undef D1
      #undef D2
      #undef D3
      }
    public:
      T execute(
            const char *expression_string, T default_value,
            const char **out_end_pointer=NULL,
            integer_c_expression_const_table<T> *constant_var_table=NULL,
            bool allow_indigestion=false ) {
        node_t sTK ;
        sTK.token = tSTART ;
        top = &sTK ;
        es = expression_string ;
        def_val = default_value ;
        halfway = allow_indigestion ;
        const_table = constant_var_table ;
        T ret = es ? parse(top) : def_val ;
        if(out_end_pointer) *out_end_pointer = es ;
        return ret ;
      }
    };


//----- acalci entity -----------------------------------------------
/* Calculate the c-expression string and convert it to an integer. */
/********************************************************************

  int acalci(const char *s, int defVal, const char **endPtr, bool allowIndigest)

    s: A c-expression string consisted of operators and terms.
    defVal: A returned value as default on being failed to calculate.
    endPtr: An output value wherence end of c-expression literals on s.
    allowIndigest: Allow the indigestion of operators or not.
    [result]: A value as converted result of calculating s.

    Operators and Associativity::
    (The order of priority is from top to bottom.)
    ==============================================
        ( )                    -> left to right
        + - ~ (single)         <- right to left
        ** (factorial)         -> left to right
        * / %                  -> left to right
        + -                    -> left to right
        << >>                  -> left to right
        &                      -> left to right
        ^                      -> left to right
        |                      -> left to right
    ==============================================
    Operator-meanings are almost same as C-Lang.

    Only integers can be terms as the imm value.
    Integer formats:: (described below as regexp.)
    ==============================================
      0[bB][01]+                  Binary digits
      [1-9][0-9].                 Decimal digits
      0[0-7].                     Octal digits
      0[xX][0-9a-fA-F]+           Hexical digits
    ==============================================

    Separators:: [SPACE][TAB(\t)][CR(\r)][LF(\n)]

********************************************************************/

  static integer_c_expression_const_table<int> acalci_const_table(ACALCI_CONST_MATCHCASE) ;
  static integer_c_expression_const_table<__int64> acalci64_const_table(ACALCI_CONST_MATCHCASE) ;

//-----
int acalci(const char *s, int defVal, const char **endPtr, bool allowIndigest)
{
  integer_c_expression_string_calculator<int> calculator ;
  return calculator.execute(s, defVal, endPtr, &acalci_const_table, allowIndigest) ;
}
//-----
__int64 acalci64(const char *s, __int64 defVal, const char **endPtr, bool allowIndigest)
{
  integer_c_expression_string_calculator<__int64> calculator ;
  return calculator.execute(s, defVal, endPtr, &acalci64_const_table, allowIndigest) ;
}
//-----
void acalci_entry_const(const char *name, int val)
{
  if(!name) acalci_const_table.clear();
  else acalci_const_table.entry(name, val);
}
//-----
void acalci64_entry_const(const char *name, __int64 val)
{
  if(!name) acalci64_const_table.clear();
  else acalci64_const_table.entry(name, val);
}
//===========================================================================
// event_object
//---------------------------------------------------------------------------
static int event_create_count = 0 ;
//---------------------------------------------------------------------------
event_object::event_object(BOOL initialState_,wstring name_,BOOL security_)
{
#ifdef _DEBUG
  id = event_create_count;
#endif
  if(name_.empty()) name_ = L"Local\\event"+itows(event_create_count)+L"_"+mbcs2wcs(uuid_string()) ;

  name = name_ ;
  event_create_count++;

  SECURITY_ATTRIBUTES sa,*psa=NULL;
  SECURITY_DESCRIPTOR sd;
  if(security_) {
    ZeroMemory(&sd,sizeof sd);
    if(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
      if(SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE)) {
        ZeroMemory(&sa,sizeof sa);
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = FALSE;
        sa.lpSecurityDescriptor = &sd;
        psa = &sa;
      }
    }
  }

  event = CreateEvent(psa,FALSE,initialState_,name.empty()?NULL:name.c_str()) ;
#ifdef _DEBUG
  if(is_valid()) {
    TRACE(L"event_object(%d) created. [name=%s]\r\n",id,name.empty()?L"<EMPTY>":name.c_str()) ;
  }else {
    TRACE(L"event_object(%d) failed to create. [name=%s]\r\n",id,name.empty()?L"<EMPTY>":name.c_str()) ;
  }
#endif
}
//---------------------------------------------------------------------------
event_object::event_object(const event_object &clone_source)
{
#ifdef _DEBUG
  id = event_create_count++;
#endif
  name = clone_source.name ;
  event = clone_source.open() ;
#ifdef _DEBUG
  if(is_valid()) {
    TRACE(L"event_object(%d) cloned. [name=%s]\r\n",id,name.empty()?L"<EMPTY>":name.c_str()) ;
  }else {
    TRACE(L"event_object(%d) failed to clone. [name=%s]\r\n",id,name.empty()?L"<EMPTY>":name.c_str()) ;
  }
#endif
}
//---------------------------------------------------------------------------
event_object::~event_object()
{
#ifdef _DEBUG
  if(is_valid()) {
    TRACE(L"event_object(%d) disposed. [name=%s]\r\n",id,name.c_str()) ;
  }else {
    TRACE(L"event_object(%d) disposed. (failure) [name=%s]\r\n",id,name.c_str()) ;
  }
#endif
  if(is_valid()) CloseHandle(event) ;
}
//---------------------------------------------------------------------------
HANDLE event_object::open() const
{
  if(!is_valid()) return NULL ;
  HANDLE open_event = OpenEvent(EVENT_ALL_ACCESS, FALSE, name.c_str());
  return open_event ;
}
//---------------------------------------------------------------------------
DWORD event_object::wait(DWORD timeout)
{
  return is_valid() ? wait_object(event,timeout) : WAIT_FAILED ;
}
//---------------------------------------------------------------------------
BOOL event_object::set()
{
  return is_valid() ? SetEvent(event) : FALSE ;
}
//---------------------------------------------------------------------------
BOOL event_object::reset()
{
  return is_valid() ? ResetEvent(event) : FALSE ;
}
//---------------------------------------------------------------------------
BOOL event_object::pulse()
{
  return is_valid() ? PulseEvent(event) : FALSE ;
}
//===========================================================================
// critical_object
//---------------------------------------------------------------------------
critical_object::critical_object()
{
  ref = new critical_ref_t ;
  InitializeCriticalSection(&ref->critical) ;
}
//---------------------------------------------------------------------------
critical_object::critical_object(const critical_object &clone_source)
{
  ref = clone_source.ref ;
  assert(ref!=NULL);
  enter();
  ref->ref_count++ ;
  leave();
}
//---------------------------------------------------------------------------
critical_object::~critical_object()
{
  enter();
  bool empty = !--ref->ref_count ;
  leave();
  if(empty) {
    DeleteCriticalSection(&ref->critical) ;
    delete ref ;
  }
}
//---------------------------------------------------------------------------
void critical_object::enter()
{
  EnterCriticalSection(&ref->critical) ;
}
//---------------------------------------------------------------------------
BOOL critical_object::try_enter()
{
  return TryEnterCriticalSection(&ref->critical) ;
}
//---------------------------------------------------------------------------
void critical_object::leave()
{
  LeaveCriticalSection(&ref->critical) ;
}
//===========================================================================
// CAsyncFifo
//---------------------------------------------------------------------------
CAsyncFifo::CAsyncFifo(
  size_t initialPool, size_t maximumPool, size_t emptyBorder,
  size_t packetSize, DWORD threadWait,int threadPriority)
  : MaximumPool(max(1,max(initialPool,maximumPool))),
    TotalPool(min(max(1,initialPool),MaximumPool)),
    Writing(WRITING_NONE),
    Indices(MaximumPool),
    EmptyIndices(MaximumPool),
    EmptyBorder(emptyBorder),
    EmptyLimit(0),
    PacketSize(packetSize),
    THREADWAIT(threadWait),
    AllocThread(INVALID_HANDLE_VALUE),
    AllocOrderEvent(FALSE),
    AllocatedEvent(FALSE),
    ModerateAllocating(true),
    Terminated(false)
{
    DWORD flag = HEAP_NO_SERIALIZE ;
    // バッファ初期化
    PacketSize = packetSize ;
    BufferPool.resize(MaximumPool);
#ifdef ASYNCFIFO_HEAPBUFFERPOOL
    // ヒープ作成
    Heap = HeapCreate(flag, PacketSize*TotalPool, PacketSize*MaximumPool);
    BufferPool.set_heap(Heap) ;
    BufferPool.set_heap_flag(flag) ;
#endif
    for(size_t i = 0UL ; i < TotalPool ; i++){
        BufferPool[i].resize(PacketSize);
        EmptyIndices.push(i) ;
    }
#ifdef ASYNCFIFO_HEAPBUFFERPOOL
    //HeapCompact(Heap, flag) ;
#endif
    // アロケーションスレッド作成
    if(MaximumPool>TotalPool) {
      AllocThread = (HANDLE)_beginthreadex(NULL, 0, AllocThreadProc, this, CREATE_SUSPENDED, NULL) ;
      if(AllocThread != INVALID_HANDLE_VALUE) {
          SetThreadPriority(AllocThread,threadPriority);
          ::ResumeThread(AllocThread) ;
      }
    }
}
//---------------------------------------------------------------------------
CAsyncFifo::~CAsyncFifo()
{
    // アロケーションスレッド破棄
    Terminated=true ;
    bool abnormal=false ;
    if(AllocThread!=INVALID_HANDLE_VALUE) {
      AllocOrderEvent.set() ;
      if(wait_object(AllocThread,30000) != WAIT_OBJECT_0) {
        ::TerminateThread(AllocThread, 0);
        abnormal=true ;
      }
      ::CloseHandle(AllocThread) ;
    }

#ifdef ASYNCFIFO_HEAPBUFFERPOOL
    // バッファ放棄（直後にヒープ自体を破棄するのでメモリリークは発生しない）
    BufferPool.abandon_erase(Heap) ;
    // ヒープ破棄
    if(!abnormal) HeapDestroy(Heap) ;
#endif
}
//---------------------------------------------------------------------------
CAsyncFifo::CACHE *CAsyncFifo::BeginWriteBack(bool allocWaiting)
{
  exclusive_lock plock(&PurgeExclusive,false) ;
  exclusive_lock elock(&Exclusive,false) ;

  if(!allocWaiting&&EmptyIndices.size()<=EmptyLimit) {
    AllocOrderEvent.set() ;
    return NULL ;
  }

  plock.lock();

  if(EmptyIndices.size()<=EmptyBorder) {
    if(allocWaiting) {
      if(!WaitForAllocation()&&EmptyIndices.empty())
        return NULL ;
    }else {
      // Allocation ordering...
      AllocOrderEvent.set() ;
    }
  }

  elock.lock();

  if(EmptyIndices.empty()) return NULL ;
  size_t index = EmptyIndices.front() ;
  EmptyIndices.pop() ;
  CACHE *cache = &BufferPool[index] ;
  WriteBackMap[cache] = index ;
  return cache;
}
//---------------------------------------------------------------------------
bool CAsyncFifo::FinishWriteBack(CAsyncFifo::CACHE *cache,bool fragment)
{
  exclusive_lock lock(&Exclusive) ;

  WRITEBACKMAP::iterator pos = WriteBackMap.find(cache) ;
  if(pos==WriteBackMap.end()) return false ;

  bool result = true ;

  size_t index = pos->second ;
  if(cache->size()>0) {
    if(fragment) {
      exclusive_lock plock(&Exclusive) ;
      if(Writing==WRITING_FRAGMENT) {
        Indices.push(WritingIndex) ;
        Writing=WRITING_NONE ;
      }
    }
    Indices.push(index) ;
  }else {
    EmptyIndices.push_front(index) ;
    result = false ;
  }
  WriteBackMap.erase(pos) ;

  return result ;
}
//---------------------------------------------------------------------------
size_t CAsyncFifo::Push(const BYTE *data, DWORD len, bool ignoreFragment,bool allocWaiting)
{
  if(!data||!len)
    return 0 ;

  exclusive_lock plock(&PurgeExclusive,false) ;
  exclusive_lock elock(&Exclusive,false) ;


  if(!allocWaiting&&EmptyIndices.size()<=EmptyLimit) {
    AllocOrderEvent.set() ;
    return 0 ;
  }

  plock.lock();

  size_t sz, n=0 ;
  for(BYTE *p = const_cast<BYTE*>(data) ; len ; len-=(DWORD)sz, p+=sz) {

    sz=min(len,PacketSize) ;

    if(Writing!=WRITING_FRAGMENT) {

      if(EmptyIndices.size()<=EmptyBorder) {
        if(allocWaiting) {
          if(!WaitForAllocation()&&EmptyIndices.empty())
            return n ;
        }else {
          // allocation ordering...
          AllocOrderEvent.set() ;
          if(EmptyIndices.size()<=EmptyLimit)
            return n ;
        }
      }

      elock.lock();

      if(EmptyIndices.empty()) return n ;
      // get the empty index
      WritingIndex = EmptyIndices.front() ;
      EmptyIndices.pop() ;
      Writing = WRITING_PACKET ;

      elock.unlock();
    }

    // resize and data writing (no lock)
    switch(Writing) {
      case WRITING_FRAGMENT: { // fragmentation occurred
        size_t buf_sz = BufferPool[WritingIndex].size();
        sz = min(sz, PacketSize - buf_sz);
        BufferPool[WritingIndex].resize(buf_sz + sz);
        CopyMemory(BufferPool[WritingIndex].data() + buf_sz, p, sz);
        break;
      }
      case WRITING_PACKET:
        BufferPool[WritingIndex].resize(sz) ;
        CopyMemory(BufferPool[WritingIndex].data(), p, sz );
        break;
    }

        if (ignoreFragment || BufferPool[WritingIndex].size() == PacketSize) {
            // push to FIFO buffer
            elock.lock();
            Writing = WRITING_NONE;
            Indices.push(WritingIndex);
            elock.unlock();
            n++;
        }
        else
            Writing = WRITING_FRAGMENT;

  }
  return n ;
}
//---------------------------------------------------------------------------
bool CAsyncFifo::Pop(BYTE **data, DWORD *len,DWORD *remain)
{
    exclusive_lock lock(&Exclusive);
    if(Empty()) {
        if(len) *len = 0 ;
        if(data) *data = 0 ;
        if(remain) *remain = 0 ;
        return false ;
    }
    size_t index = Indices.front() ;
    if(len) *len = (DWORD)BufferPool[index].size() ;
    if(data) *data = (BYTE*)BufferPool[index].data() ;
    EmptyIndices.push(index) ;
    Indices.pop();
    if(remain) *remain = (DWORD)Size() ;
    return true;
}
//---------------------------------------------------------------------------
void CAsyncFifo::Purge(bool purgeWriteBack)
{
    // バッファから取り出し可能データをパージする
    exclusive_lock plock(&PurgeExclusive) ;
    exclusive_lock lock(&Exclusive);

    // 未処理のデータをパージする
    while(!Indices.empty()) {
        EmptyIndices.push(Indices.front()) ;
        Indices.pop() ;
    }
    if(Writing!=WRITING_NONE) {
        EmptyIndices.push(WritingIndex) ;
        Writing = WRITING_NONE ;
    }

    if(purgeWriteBack) {
        for(WRITEBACKMAP::iterator pos = WriteBackMap.begin() ;
         pos!= WriteBackMap.end() ; ++pos) {
            EmptyIndices.push(pos->second) ;
        }
        WriteBackMap.clear() ;
    }
}
//---------------------------------------------------------------------------
unsigned int CAsyncFifo::AllocThreadProcMain ()
{
    exclusive_lock elock(&Exclusive,false) ;
    for(;;) {
        DWORD dwRet = AllocOrderEvent.wait(THREADWAIT);
        if (Terminated) break;
        bool doAllocate = false;
        switch(dwRet) {
            case WAIT_OBJECT_0: // Allocation ordered
                elock.lock() ;
                doAllocate = Growable() ;
                if(EmptyIndices.size() > EmptyBorder)
                  AllocatedEvent.set() ;
                elock.unlock() ;
                break ;
            case WAIT_TIMEOUT:
                if(!ModerateAllocating) {
                  elock.lock() ;
                  doAllocate = Growable() && EmptyIndices.size() <= EmptyBorder ;
                  elock.unlock() ;
                }
                break ;
            case WAIT_FAILED:
                return 1;
        }
        if(doAllocate) {
            bool failed=false ;
            do {
                elock.unlock() ;
                BufferPool[TotalPool].resize(PacketSize); // Allocating...
                if(BufferPool[TotalPool].size()!=PacketSize) {
                  failed = true ; break ;
                }
                elock.lock() ;
                EmptyIndices.push_front(TotalPool++) ;
                elock.unlock() ;
                AllocatedEvent.set();
                if (Terminated) break;
                if (ModerateAllocating) break;
                elock.lock() ;
            } while (Growable() && EmptyIndices.size() <= EmptyBorder );
            elock.unlock() ;
            if(failed)
              DBGOUT("Async FIFO allocation: allocation failed!\r\n") ;
            else
              DBGOUT("Async FIFO allocation: total %d bytes grown.\r\n",
                  int((TotalPool)*PacketSize)) ;
        }
        if (Terminated) break;
    }
    return 0 ;
}
//---------------------------------------------------------------------------
unsigned int __stdcall CAsyncFifo::AllocThreadProc (PVOID pv)
{
    register CAsyncFifo *this_ = static_cast<CAsyncFifo*>(pv) ;
    unsigned int result = this_->AllocThreadProcMain() ;
    _endthreadex(result) ;
    return result;
}
//---------------------------------------------------------------------------
bool CAsyncFifo::WaitForAllocation()
{
    exclusive_lock elock(&Exclusive) ;

    size_t n=EmptyIndices.size() ;
    if(n>EmptyBorder) {
      return true ;
    }

    DBGOUT("Async FIFO allocation: allocation waiting...\r\n") ;

    bool result = false ;

      do {
        if (!Growable()) break ;
        elock.unlock() ;
        AllocOrderEvent.set();
        DWORD res = AllocatedEvent.wait(THREADWAIT) ;
        if(res==WAIT_FAILED) break ;
        elock.lock() ;
        size_t m = EmptyIndices.size();
        if (n < m) {
            if (ModerateAllocating&&m>EmptyLimit)
                result = true;
            n = m;
        }
        if (n > EmptyBorder) result = true ;
        if(result) break ;
      }while(!Terminated) ;

    DBGOUT("Async FIFO allocation: allocation waiting %s.\r\n",result?"completed":"failed") ;

    return result ;
}
//===========================================================================
// CSharedMemory
//---------------------------------------------------------------------------
CSharedMemory::CSharedMemory(wstring name, DWORD size)
  : BaseName(name), HMutex(NULL), HMapping(NULL), PMapView(NULL)
{
    wstring mutex_name = BaseName + L"_SharedMemory_Mutex";
    wstring mapping_name = BaseName + L"_SharedMemory_Mapping";

    HMutex = CreateMutex(NULL, FALSE, mutex_name.c_str());
    if(Lock()) {
        HMapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
            PAGE_READWRITE, 0, size, mapping_name.c_str());
        BOOL map_existed = (GetLastError() == ERROR_ALREADY_EXISTS);
        SzMapView=0;
        if (HMapping) {
            PMapView = MapViewOfFile(HMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            if (PMapView) {
                SzMapView = size;
                if (!map_existed) {
                    //共有領域初期化
                    ZeroMemory(PMapView, SzMapView);
                }
            }
        }
        Unlock();
    }
}
//---------------------------------------------------------------------------
CSharedMemory::~CSharedMemory()
{
    if(PMapView) UnmapViewOfFile(PMapView);
    if(HMapping) CloseHandle(HMapping);
    if(HMutex)   CloseHandle(HMutex);
}
//---------------------------------------------------------------------------
bool CSharedMemory::IsValid() const
{
    return HMutex && HMapping && PMapView ;
}
//---------------------------------------------------------------------------
bool CSharedMemory::Lock(DWORD timeout) const
{
    if(!HMutex) return false ;
    return wait_object(HMutex, timeout) == WAIT_OBJECT_0 ;
}
//---------------------------------------------------------------------------
bool CSharedMemory::Unlock() const
{
    if(!HMutex) return false ;
    return ReleaseMutex(HMutex) ? true : false ;
}
//---------------------------------------------------------------------------
DWORD CSharedMemory::Read(LPVOID *dst, DWORD sz, DWORD pos
    , DWORD timeout) const
{
    if(!IsValid()||Size()<=pos) return 0;
    if(sz+pos>Size()) sz = Size()-pos ;
    if(Lock(timeout)) {
        CopyMemory(dst,&static_cast<BYTE*>(Memory())[pos],sz);
        Unlock();
    }
    return sz;
}
//---------------------------------------------------------------------------
DWORD CSharedMemory::Write(const LPVOID *src, DWORD sz, DWORD pos
    , DWORD timeout)
{
    if(!IsValid()||Size()<=pos) return 0;
    if(sz+pos>Size()) sz = Size()-pos ;
    if(Lock(timeout)) {
        CopyMemory(&static_cast<BYTE*>(Memory())[pos],src,sz);
        Unlock();
    }
    return sz;
}
//===========================================================================
// Initializer
//---------------------------------------------------------------------------

  class pryutil_initializer
  {
  protected:
    void init_acalci_constants(bool first) {
    #define ACALCI_ENTRY_CONST(name) do { \
        acalci_entry_const(#name,(int)name); \
        acalci64_entry_const(#name,(__int64)name); \
        }while(0)
      if(first) {
        acalci_entry_const();
        acalci64_entry_const();
      }
      const int n=FALSE,y=TRUE;
      ACALCI_ENTRY_CONST(NULL);
      ACALCI_ENTRY_CONST(n);
      ACALCI_ENTRY_CONST(y);
      ACALCI_ENTRY_CONST(INT_MIN);
      ACALCI_ENTRY_CONST(INT_MAX);
      ACALCI_ENTRY_CONST(INFINITE);
      ACALCI_ENTRY_CONST(IDLE_PRIORITY_CLASS);
      ACALCI_ENTRY_CONST(BELOW_NORMAL_PRIORITY_CLASS);
      ACALCI_ENTRY_CONST(NORMAL_PRIORITY_CLASS);
      ACALCI_ENTRY_CONST(ABOVE_NORMAL_PRIORITY_CLASS);
      ACALCI_ENTRY_CONST(HIGH_PRIORITY_CLASS);
      ACALCI_ENTRY_CONST(REALTIME_PRIORITY_CLASS);
      ACALCI_ENTRY_CONST(THREAD_PRIORITY_IDLE);
      ACALCI_ENTRY_CONST(THREAD_PRIORITY_LOWEST);
      ACALCI_ENTRY_CONST(THREAD_PRIORITY_BELOW_NORMAL);
      ACALCI_ENTRY_CONST(THREAD_PRIORITY_NORMAL);
      ACALCI_ENTRY_CONST(THREAD_PRIORITY_ABOVE_NORMAL);
      ACALCI_ENTRY_CONST(THREAD_PRIORITY_HIGHEST);
      ACALCI_ENTRY_CONST(THREAD_PRIORITY_TIME_CRITICAL);
    #undef ACALCI_ENTRY_CONST
    }
  public:
    pryutil_initializer(bool first) {
      // initialize locale to japanese for the string converters
      if(first) setlocale(LC_ALL, "japanese") ;
      // initialize constants
      init_acalci_constants(first) ;
    }
  };

  static pryutil_initializer static_initializer(true);

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
