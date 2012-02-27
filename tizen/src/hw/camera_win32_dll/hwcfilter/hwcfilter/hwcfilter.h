#ifndef __HWCFILTER_H__
#define __HWCFILTER_H__

#include "hwcfilter_h.h"

const IID __declspec(selectany) IID_IHWCPin			= {0x33AFDC07,0xC2AB,0x4FC4,{0xBA,0x54,0x65,0xFA,0xDF,0x4B,0x47,0x4D}};
const IID __declspec(selectany) IID_ICaptureCallBack	= {0x4C337035,0xC89E,0x4B42,{0x9B,0x0C,0x36,0x74,0x44,0xDD,0x70,0xDD}};

static const WCHAR HWCPinName[] = L"HWCInputPin\0";
static const WCHAR HWCFilterName[] = L"HWCFilter\0";

class CHWCFilter;

class CHWCPin: public IHWCPin, public IMemInputPin
{
public:
	CHWCPin(CHWCFilter *pFilter);
	virtual ~CHWCPin();
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	STDMETHODIMP Connect(IPin * pReceivePin, const AM_MEDIA_TYPE *pmt);
	STDMETHODIMP ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt);
	STDMETHODIMP Disconnect();
	STDMETHODIMP ConnectedTo(IPin **pPin);
	STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE *pmt);
	STDMETHODIMP QueryPinInfo(PIN_INFO *pInfo);
	STDMETHODIMP QueryDirection(PIN_DIRECTION *pPinDir);
	STDMETHODIMP QueryId(LPWSTR * Id);
	STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE *pmt);
	STDMETHODIMP EnumMediaTypes(IEnumMediaTypes **ppEnum);
	STDMETHODIMP QueryInternalConnections(IPin **apPin, ULONG *nPin);
	STDMETHODIMP EndOfStream();
	STDMETHODIMP BeginFlush();
	STDMETHODIMP EndFlush();
	STDMETHODIMP NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate );
	STDMETHODIMP SetCallback(ICaptureCallBack *pCallback);

	STDMETHODIMP GetAllocator(IMemAllocator **ppAllocator);
	STDMETHODIMP NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly);
	STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps);
	STDMETHODIMP Receive(IMediaSample *pSample);        
	STDMETHODIMP ReceiveMultiple(IMediaSample **pSamples, long nSamples, long *nSamplesProcessed);
	STDMETHODIMP ReceiveCanBlock();

protected:
	CHWCFilter *m_pCFilter;
	IPin *m_pConnectedPin;
	ICaptureCallBack *m_pCallback;
	BOOL m_bReadOnly;
	long m_cRef;
};

class CHWCEnumPins: public IEnumPins
{
public:
	CHWCEnumPins(CHWCFilter *pFilter, int pos);
	virtual ~CHWCEnumPins();
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP Next(ULONG cPins, IPin **ppPins, ULONG *pcFetched);
	STDMETHODIMP Skip(ULONG cPins);
	STDMETHODIMP Reset();
	STDMETHODIMP Clone(IEnumPins **ppEnum);

protected:
	CHWCFilter *m_pFilter;
	int m_pos;
	long m_cRef;
};

class CHWCFilter: public IBaseFilter
{
public:
	CHWCFilter();
	virtual ~CHWCFilter();
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP GetClassID(CLSID *pClsID);
	STDMETHODIMP Stop();
	STDMETHODIMP Pause();
	STDMETHODIMP Run(REFERENCE_TIME tStart);
	STDMETHODIMP GetState(DWORD dwMSecs, FILTER_STATE *State);
	STDMETHODIMP SetSyncSource(IReferenceClock *pClock);
	STDMETHODIMP GetSyncSource(IReferenceClock **pClock);
	STDMETHODIMP EnumPins(IEnumPins **ppEnum);
	STDMETHODIMP FindPin(LPCWSTR Id, IPin **ppPin);
	STDMETHODIMP QueryFilterInfo(FILTER_INFO *pInfo);
	STDMETHODIMP JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName);
	STDMETHODIMP QueryVendorInfo(LPWSTR *pVendorInfo);

protected:
	CHWCPin *m_pPin;
	IFilterGraph *m_pFilterGraph;
	FILTER_STATE m_state;
	long m_cRef;
};

class HWCClassFactory : public IClassFactory
{
public:
	HWCClassFactory();
	virtual ~HWCClassFactory();

	STDMETHODIMP QueryInterface(REFIID riid, LPVOID* ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, LPVOID* ppv);
	STDMETHODIMP LockServer(BOOL bLock);

private:
	long m_cRef;
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv);

#endif // __HWCFILTER_H__