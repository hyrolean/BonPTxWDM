//===========================================================================
#pragma once
#ifndef _PRYUTIL_20141218222525309_H_INCLUDED_
#define _PRYUTIL_20141218222525309_H_INCLUDED_
//---------------------------------------------------------------------------

#include <Windows.h>
#include <assert.h>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <stdexcept>

//===========================================================================
namespace PRY8EAlByw {
//===========================================================================
// Functions
//---------------------------------------------------------------------------
DWORD Elapsed(DWORD start=0,DWORD end=GetTickCount()) ;
DWORD PastSleep(DWORD wait=0,DWORD start=GetTickCount()) ;

std::wstring mbcs2wcs(std::string src, UINT code_page=CP_ACP);
std::string wcs2mbcs(std::wstring src, UINT code_page=CP_ACP);

std::string itos(int val,int radix=10);
std::wstring itows(int val,int radix=10);

std::string upper_case(std::string str) ;
std::string lower_case(std::string str) ;

std::string file_drive_of(std::string filename);
std::string file_path_of(std::string filename);
std::string file_name_of(std::string filename);
std::string file_prefix_of(std::string filename);
std::string file_suffix_of(std::string filename);
int file_age_of(std::string filename);
bool file_is_existed(std::string filename);
bool folder_is_existed(std::string filename);

int acalci(const char *s, int defVal=0, const char **endPtr=NULL, bool allowIndigest=false);
__int64 acalci64(const char *s, __int64 defVal=0, const char **endPtr=NULL, bool allowIndigest=false);

void acalci_entry_const(const char *name=NULL, int val=0);
void acalci64_entry_const(const char *name=NULL, __int64 val=0);

//===========================================================================
// Inline Functions
//---------------------------------------------------------------------------
template<class String> String inline trim(const String &str)
{
  String str2 ; str2.clear() ;
  for(typename String::size_type i=0;i<str.size();i++) {
    if(unsigned(str[i])>0x20UL) {
      str2 = str.substr(i,str.size()-i) ;
      break ;
    }
  }
  if(str2.empty()) return str2 ;
  for(typename String::size_type i=str2.size();i>0;i--) {
    if(unsigned(str2[i-1])>0x20UL) {
      return str2.substr(0,i) ;
    }
  }
  str2.clear() ;
  return str2 ;
}
//---------------------------------------------------------------------------
#if 0
template<class Container> void inline split(
	Container &DivStrings, const typename Container::value_type &Text,
	typename Container::value_type::value_type Delimiter)
#else
template<class Container,class String> void inline split(
	Container &DivStrings/*string container*/, const String &Text,
	typename String::value_type Delimiter)
#endif
{
  #ifdef _DEBUG
  assert(typeid(typename Container::value_type)==typeid(Text));
  #endif
  typename Container::value_type temp; temp.clear() ;
  for(typename Container::value_type::size_type i=0;i<Text.size();i++) {
    if(Text[i]==Delimiter) {
      DivStrings.push_back(trim(temp));
      temp.clear();
      continue;
    }
    temp+=Text[i];
  }
  if(!trim(temp).empty()) {
     DivStrings.push_back(trim(temp));
  }
}
//---------------------------------------------------------------------------
//===========================================================================
// Classes
//---------------------------------------------------------------------------

  // 簡易イベントオブジェクト

class event_object
{
private:
  HANDLE event ;
  std::wstring name ;
public:
  event_object(BOOL _InitialState=TRUE/*default:signalized*/,std::wstring _name=L"") ;
  event_object(const event_object &clone_source) ; // 現スレッドでイベントを複製
  ~event_object() ;
  std::wstring event_name() const { return name ; }
  HANDLE handle() const { return event ; }
  BOOL is_valid() const { return (event&&event!=INVALID_HANDLE_VALUE)?TRUE:FALSE; }
  // 現スレッドの下で複製のイベントハンドルを開く
  HANDLE open() const ;
  // 現スレッドでシグナル化されるまでブロックし、解除された後、自動的に再び非シグナル化
  DWORD wait(DWORD timeout=INFINITE) ;
  // シグナル化（ブロックされているスレッドが解除されるのは複数のうちの一つ）
  BOOL set() ;
  // マニュアル的な非シグナル化
  BOOL reset() ;
  // シグナル化（ブロックされている全スレッドの一括解除）
  BOOL pulse() ;
  // lock(wait) / unlock(set)
  DWORD lock(DWORD timeout=INFINITE) { return wait(timeout) ; }
  BOOL unlock() { return set() ; }

};


  // 簡易クリティカルオブジェクト

class critical_object
{
private:
  struct critical_ref_t {
    CRITICAL_SECTION critical ;
    int ref_count ;
    critical_ref_t(): ref_count(1) {}
  };
  critical_ref_t *ref ;
public:
  critical_object() ;
  critical_object(const critical_object &clone_source) ;
  ~critical_object() ;
  CRITICAL_SECTION *handle() const { return &ref->critical ; }
  // wrapper
  void enter() ;
  BOOL try_enter() ;
  void leave() ;
  // lock / unlock
  void lock() { enter() ; }
  void unlock() { leave() ; }
};


  // スコープレベルの自動ロック／アンロック(オブジェクト参照レベル)

template <typename locker_t>
class basic_lock
{
private:
  bool unlocked ;
  locker_t *locker_ref ;
public:
  basic_lock(locker_t *source_ref,bool initial_locking=true)
    : locker_ref(source_ref) {
	unlocked = !initial_locking;
    // 非シグナル状態が解除されるまで現スレッドをブロック
    if(initial_locking) locker_ref->lock() ;
  }
  ~basic_lock() {
    // スコープ終了時点でシグナル状態を自動解除する
    unlock() ;
  }
  void unlock() {
    // スコープの途中でこのメソッド呼び出すとその時点で非シグナル状態を解除する
    if(!unlocked) {
      // シグナル状態にセット
      locker_ref->unlock() ;
      unlocked = true ;
    }
  }
  void lock() {
    // スコープの途中でこのメソッド呼び出すとその時点で非シグナル状態にする
    if(unlocked) {
      locker_ref->lock() ;
      unlocked = false ;
	}
  }
};
typedef basic_lock<event_object> event_lock ;
typedef basic_lock<critical_object> critical_lock ;


  // スコープレベルの自動ロック／アンロック(オブジェクト複製レベル)

template <typename locker_t>
class basic_lock_object
{
private:
  locker_t locker ;
  bool unlocked ;
public:
  basic_lock_object(const locker_t &source_object, bool initial_locking = true)
    : locker(source_object) {
    // 非シグナル状態が解除されるまで現スレッドをブロック
    unlocked = !initial_locking;
    // 非シグナル状態が解除されるまで現スレッドをブロック
    if (initial_locking) locker.lock();
  }
  ~basic_lock_object() {
    // スコープ終了時点で非シグナル状態を自動解除する
    unlock() ;
  }
  void unlock() {
    // スコープの途中でこのメソッド呼び出すとその時点で非シグナル状態を解除する
    if(!unlocked) {
      // シグナル状態にセット
      locker.unlock() ;
      unlocked = true ;
    }
  }
  void lock() {
    // スコープの途中でこのメソッド呼び出すとその時点で非シグナル状態にする
    if (unlocked) {
      locker.lock();
      unlocked = false;
    }
  }
};
typedef basic_lock_object<event_object> event_lock_object ;
typedef basic_lock_object<critical_object> critical_lock_object ;


  // exclusive

  #if 0  // ｲﾍﾞﾝﾄ
  typedef event_object exclusive_object ;
  #else  // ｸﾘﾃｨｶﾙ ｾｸｼｮﾝ
  typedef critical_object exclusive_object ;
  #endif
  typedef basic_lock<exclusive_object> exclusive_lock ;
  typedef basic_lock_object<exclusive_object> exclusive_lock_object ;


  //BUFFER/BUFFERPOOL

template<typename T>
struct BUFFER {
    typedef size_t size_type ;
    typedef T  value_type ;
    typedef T& reference_type ;
    typedef T* pointer_type ;
    BUFFER(size_type size=0) : buff_(NULL), size_(0UL), grow_(0UL) {
      if(size>0) resize(size) ;
    }
    BUFFER(const BUFFER<T> &src)
     : buff_(NULL), size_(0UL), grow_(0UL) {
      *this = src ;
    }
    BUFFER(const void *buffer, size_type size)
     : buff_(NULL), size_(0UL), grow_(0UL) {
      if(size>0) resize(size) ;
      if(buff_&&size_==size)
        CopyMemory(buff_,buffer,size*sizeof(value_type)) ;
    }
    ~BUFFER() {
      clear() ;
    }
    BUFFER &operator =(const BUFFER<T> &src) {
      resize(src.size_) ;
      if(buff_&&size_==src.size_)
        CopyMemory(buff_,src.buff_,src.size_*sizeof(value_type)) ;
      return *this ;
    }
    void clear() {
      if(buff_) {
        std::free(buff_) ;
        buff_=NULL ; grow_=0UL ; size_=0UL ;
      }
    }
    void resize(size_type size) {
      if(size_!=size) {
        if(buff_) {
          if(size>grow_) {
            buff_ = (pointer_type)std::realloc(buff_,size*sizeof(value_type)) ;
            if(buff_) grow_ = size ;
            else grow_ = 0UL ;
          }
        }else {
          buff_ = (pointer_type)std::malloc(size*sizeof(value_type)) ;
          if(buff_) grow_ = size ;
          else grow_ = 0UL ;
        }
        if(buff_) size_ = size ;
        else size_ = 0UL ;
      }
    }
    reference_type operator[](size_type index) { return buff_[index] ; }
    pointer_type data() { return buff_ ; }
    size_type size() const { return size_ ; }
private:
    pointer_type buff_ ;
    size_type size_ ;
    size_type grow_ ;
};

template<typename T,class Container=std::vector< BUFFER<T> > >
struct BUFFERPOOL {
  typedef BUFFER<T> value_type ;
  typedef BUFFER<T>* pointer_type ;
  typedef BUFFER<T>& reference_type ;
  typedef size_t size_type ;
  void resize(size_type size) {
    cont_.resize(size) ;
  }
  void clear() {
    cont_.clear() ;
  }
  size_type size() {
    return cont_.size() ;
  }
  reference_type operator[](size_t index) {
    return cont_[index] ;
  }
  Container &container() { return cont_ ; }
private:
  Container cont_ ;
};


  // HEAPBUFFER/HEAPBUFFERPOOL

template<typename T>
struct HEAPBUFFER {
    typedef size_t size_type ;
    typedef T  value_type ;
    typedef T& reference_type ;
    typedef T* pointer_type ;
    HEAPBUFFER(size_type size=0,HANDLE heap=NULL,DWORD heap_flag=0) : buff_(NULL), size_(0UL), grow_(0UL) {
      heap_ = heap ? heap : GetProcessHeap() ;
      heap_flag_ = heap_flag ;
      if(size>0) resize(size) ;
    }
    HEAPBUFFER(const HEAPBUFFER<T> &src)
     : buff_(NULL), size_(0UL), grow_(0UL), heap_(src.heap_), heap_flag_(src.heap_flag_) {
      *this = src ;
    }
    HEAPBUFFER(const void *buffer, size_type size,HANDLE heap=NULL)
     : buff_(NULL), size_(0UL), grow_(0UL) {
      heap_ = heap ? heap : GetProcessHeap() ;
      if(size>0) resize(size) ;
      if(buff_&&size_==size)
        CopyMemory(buff_,buffer,size*sizeof(value_type)) ;
    }
    ~HEAPBUFFER() {
      clear() ;
    }
    HEAPBUFFER &operator =(const HEAPBUFFER<T> &src) {
      if(heap_!=src.heap_) {
        clear() ;
        heap_ = src.heap_ ;
      }
      heap_flag_ = src.heap_flag_ ;
      resize(src.size_) ;
      if(buff_&&size_==src.size_)
        CopyMemory(buff_,src.buff_,src.size_*sizeof(value_type)) ;
      return *this ;
    }
    void clear() {
      if(buff_) {
        HeapFree(heap_,heap_flag_&HEAP_NO_SERIALIZE,buff_) ;
        buff_=NULL ; grow_=0UL ; size_=0UL ;
      }
    }
    bool abandon(HANDLE heap) {
      if(heap_==heap) {
        if(buff_) { buff_=NULL ; grow_=0UL ; size_=0UL ; }
        return true ;
      }
      return false ;
    }
    void resize(size_type size) {
      if(size_!=size) {
        if(buff_) {
          if(size>grow_) {
            buff_ = (pointer_type)HeapReAlloc(heap_,
              heap_flag_&(HEAP_NO_SERIALIZE|HEAP_ZERO_MEMORY|HEAP_REALLOC_IN_PLACE_ONLY),
              buff_,size*sizeof(value_type)) ;
            if(buff_) grow_ = size ;
            else grow_ = 0UL ;
          }
        }else {
          buff_ = (pointer_type)HeapAlloc(heap_,
            heap_flag_&(HEAP_NO_SERIALIZE|HEAP_ZERO_MEMORY),size*sizeof(value_type)) ;
          if(buff_) grow_ = size ;
          else grow_ = 0UL ;
        }
        if(buff_) size_ = size ;
        else size_ = 0UL ;
      }
    }
    reference_type operator[](size_type index) { return buff_[index] ; }
    pointer_type data() { return buff_ ; }
    size_type size() const { return size_ ; }
    void set_heap_flag(DWORD flag) { heap_flag_ = flag ; }
    void set_heap(HANDLE heap) {
      if(!heap) heap=GetProcessHeap() ;
      if(heap_!=heap) {
        size_type sz = size() ;
        if(sz) {
          pointer_type buffer = (pointer_type)HeapAlloc(heap,
            heap_flag_&(HEAP_NO_SERIALIZE|HEAP_ZERO_MEMORY),sz*sizeof(value_type)) ;
          if(buffer) {
            CopyMemory(buffer,buff_,sz*sizeof(value_type)) ;
            clear() ;
            buff_ = buffer ;
            size_ = sz ; grow_ = sz ;
          }else {
            clear() ;
          }
        }
        heap_ = heap ;
      }
    }
private:
    HANDLE heap_ ;
    DWORD heap_flag_ ;
    pointer_type buff_ ;
    size_type size_ ;
    size_type grow_ ;
};

template<typename T,class Container=std::vector< HEAPBUFFER<T> > >
struct HEAPBUFFERPOOL {
  typedef HEAPBUFFER<T> value_type ;
  typedef HEAPBUFFER<T>* pointer_type ;
  typedef HEAPBUFFER<T>& reference_type ;
  typedef size_t size_type ;
  void set_heap(HANDLE heap) {
    std::for_each(cont_.begin(),cont_.end(),
      std::bind2nd(std::mem_fun_ref(&value_type::set_heap),heap));
  }
  void set_heap_flag(DWORD flag) {
    std::for_each(cont_.begin(),cont_.end(),
      std::bind2nd(std::mem_fun_ref(&value_type::set_heap_flag),flag));
  }
  void abandon_erase(HANDLE heap) {
    cont_.erase(std::remove_if(cont_.begin(),cont_.end(),
      std::bind2nd(std::mem_fun_ref(&value_type::abandon),heap)),cont_.end());
  }
  void resize(size_type size) {
    cont_.resize(size) ;
  }
  void clear() {
    cont_.clear() ;
  }
  size_type size() {
    return cont_.size() ;
  }
  reference_type operator[](size_t index) {
    return cont_[index] ;
  }
  Container &container() { return cont_ ; }
private:
  Container cont_ ;
};

  // fixed_queue (アロケーションが発生しない順列)

template<typename T>
class fixed_queue
{
public:
  typedef T value_type ;
  typedef T& reference_type ;
  typedef T* pointer_type ;
  typedef size_t size_type ;
protected:
  pointer_type buff_ ;
  size_type cue_ ;
  size_type size_ ;
  size_type grew_ ;
public:
  fixed_queue(size_type fixed_size)
    : grew_(fixed_size), cue_(0), size_(0) {
    buff_ = new value_type[grew_] ;
  }
  ~fixed_queue() {
    delete [] buff_ ;
  }
  size_type capacity() const { return grew_ ; }
  size_type size() const { return size_ ; }
  bool empty() const { return size_==0 ; }
  bool full() const { return size_>=grew_ ; }
  bool push(const value_type &val) {
    if(full()) return false ;
    buff_[cue_+size_-(cue_+size_<grew_?0:grew_)] = val ;
    size_++ ;
    return true ;
  }
  bool push_front(const value_type &val) {
    if(full()) return false ;
	buff_[cue_?--cue_:cue_=grew_-1] = val ;
	size_++ ;
	return true ;
  }
  bool pop() {
    if(empty()) return false ;
    if(++cue_>=grew_) cue_=0 ;
    size_-- ;
    return true ;
  }
  reference_type front() {
  #ifdef _DEBUG
    if(empty()) throw std::range_error("fixed_queue: front() range error.") ;
  #endif
    return buff_[cue_] ;
  }
  void clear() { cue_ = 0 ; size_ = 0 ; }
};

  // CAsyncFifo

    #define ASYNCFIFO_HEAPBUFFERPOOL

class CAsyncFifo
{
public:
  #ifdef ASYNCFIFO_HEAPBUFFERPOOL
  typedef HEAPBUFFER<BYTE> CACHE ;
  #else
  typedef BUFFER<BYTE> CACHE ;
  #endif
  typedef std::map<CACHE*,size_t/*index*/> WRITEBACKMAP ;
  enum WRITING {
	  WRITING_NONE,
	  WRITING_PACKET,
	  WRITING_FRAGMENT
  };
private:
  size_t MaximumPool ;
  size_t TotalPool ;
  size_t EmptyBorder ;
  size_t EmptyLimit ;
  size_t PacketSize ;
  DWORD THREADWAIT ;
  exclusive_object Exclusive, PurgeExclusive ;
  WRITING Writing ;
  size_t WritingIndex ;
  #ifdef ASYNCFIFO_HEAPBUFFERPOOL
  HANDLE Heap ;
  HEAPBUFFERPOOL<BYTE> BufferPool ;
  #else
  BUFFERPOOL<BYTE> BufferPool ;
  #endif
  fixed_queue<size_t> Indices;
  fixed_queue<size_t> EmptyIndices;
  WRITEBACKMAP WriteBackMap ;
  HANDLE AllocThread ;
  event_object AllocOrderEvent, AllocatedEvent ;
  unsigned int AllocThreadProcMain () ;
  static unsigned int __stdcall AllocThreadProc (PVOID pv) ;
  bool ModerateAllocating ;
  bool Terminated ;
public:
  CAsyncFifo(
    size_t initialPool, size_t maximumPool, size_t emptyBorder,
    size_t packetSize, DWORD threadWait=1000,
	int threadPriority=THREAD_PRIORITY_HIGHEST ) ;
  virtual ~CAsyncFifo() ;
  size_t Size() const { return Indices.size() ; }
  bool Empty() const { return Indices.empty() ; }
  bool Full() const { return EmptyIndices.empty() ; }
  bool Pushable() const { return EmptyIndices.size()>EmptyLimit ; }
  bool Growable() const { return TotalPool<MaximumPool ; }
  CACHE *BeginWriteBack(bool allocWaiting=false) ;
  bool FinishWriteBack(CACHE *cache,bool fragment=false);
  size_t Push(const BYTE *data, DWORD len, bool ignoreFragment=false, bool allocWaiting=false) ;
  bool Pop(BYTE **data, DWORD *len, DWORD *remain) ;
  void Purge(bool purgeWriteBack=false) ;
  void SetModerateAllocating(bool Moderate) { ModerateAllocating=Moderate ; }
  void SetEmptyLimit(size_t emptyLimit) { EmptyLimit = min(emptyLimit,EmptyBorder) ; }
  bool WaitForAllocation() ;
};

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
using namespace PRY8EAlByw ;
//===========================================================================
#endif // _PRYUTIL_20141218222525309_H_INCLUDED_
