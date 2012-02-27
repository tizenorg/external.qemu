#include "stdafx.h"
#include "hwcfilter.h"
#include <VFWMSGS.H>
#include <stdio.h>

static HMODULE g_hModule = NULL;
static long g_cServerLocks = 0;
static long g_cComponents = 0;

CHWCPin::CHWCPin(CHWCFilter *pFilter) :
	m_pCFilter(pFilter), m_pConnectedPin(NULL), 
	m_pCallback(NULL), m_bReadOnly(FALSE), m_cRef(1)
{
	InterlockedIncrement(&g_cComponents);
}

CHWCPin::~CHWCPin(void)
{
	InterlockedDecrement(&g_cComponents);
}

STDMETHODIMP CHWCPin::QueryInterface(REFIID riid, void **ppv)
{
	if (riid == IID_IUnknown) {
		*ppv = (IUnknown*)((IHWCPin*)this);
	} else if (riid == IID_IPin) {
		*ppv = (IPin*)this;
	} else if (riid == IID_IMemInputPin) {
		*ppv = (IMemInputPin*)this;
	} else if (riid == IID_IHWCPin) {
		*ppv = (IHWCPin*)this;
	} else {
		LPWSTR str;
		StringFromIID(riid, &str);
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	reinterpret_cast<IHWCPin*>(this)->AddRef();
	return S_OK;
}

STDMETHODIMP_(ULONG) CHWCPin::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CHWCPin::Release()
{
	if( InterlockedDecrement(&m_cRef) == 0)
	{
		delete this;
		return 0;
	}
	return m_cRef;
}

STDMETHODIMP CHWCPin::Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt)
{
	if ( !pmt )
		return S_OK;
	return S_FALSE;
}

STDMETHODIMP CHWCPin::ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt)
{
	if (pConnector == NULL || pmt == NULL)
		return E_POINTER;

	if (m_pConnectedPin) {
		return VFW_E_ALREADY_CONNECTED;
	}
	FILTER_STATE fs;
	m_pCFilter->GetState(0, &fs);
	if (fs != State_Stopped) {
		return VFW_E_NOT_STOPPED;
	}
	PIN_DIRECTION pd;
	pConnector->QueryDirection(&pd);
	if (pd == PINDIR_INPUT) {
		return VFW_E_INVALID_DIRECTION;
	}

	m_pConnectedPin = pConnector;
	m_pConnectedPin->AddRef();
	return S_OK;
}

STDMETHODIMP CHWCPin::Disconnect(void)
{
	HRESULT hr;
	if (m_pConnectedPin == NULL) {
		hr = S_FALSE;
	} else {
		m_pConnectedPin->Release();
		m_pConnectedPin = NULL;
		hr = S_OK;
	}
	return hr;
}

STDMETHODIMP CHWCPin::ConnectedTo(IPin **pPin)
{
	if (pPin == NULL)
		return E_POINTER;

	HRESULT hr;
	if ( m_pConnectedPin == NULL ) {
		hr = VFW_E_NOT_CONNECTED;
	} else {
		m_pConnectedPin->AddRef();
		*pPin = m_pConnectedPin;
		hr = S_OK;
	}
	return hr;
}

STDMETHODIMP CHWCPin::ConnectionMediaType(AM_MEDIA_TYPE *pmt)
{	
	if (pmt == NULL) {
		return E_POINTER;
	}
	return VFW_E_NOT_CONNECTED;
}

STDMETHODIMP CHWCPin::QueryPinInfo(PIN_INFO *pInfo)
{
	if (pInfo == NULL)
		return E_POINTER;

	pInfo->pFilter = (IBaseFilter*)m_pCFilter;
	if (m_pCFilter)
		m_pCFilter->AddRef();
	memcpy((void*)pInfo->achName, (void*)HWCPinName, sizeof(HWCPinName));
	pInfo->dir = PINDIR_INPUT;
	return S_OK;
}

STDMETHODIMP CHWCPin::QueryDirection(PIN_DIRECTION *pPinDir)
{
	if (pPinDir == NULL)
		return E_POINTER;
	*pPinDir = PINDIR_INPUT;
	return S_OK;
}

STDMETHODIMP CHWCPin::QueryId(LPWSTR *Id)
{
	if (Id == NULL)
		return E_POINTER;
	PVOID pId = CoTaskMemAlloc(sizeof(HWCPinName));
	memcpy((void*)pId, (void*)HWCPinName, sizeof(HWCPinName));
	*Id = (LPWSTR)pId;
	return S_OK;
}

STDMETHODIMP CHWCPin::QueryAccept(const AM_MEDIA_TYPE *pmt)
{
	if (pmt == NULL)
		return E_POINTER;
	return S_OK;
}

STDMETHODIMP CHWCPin::EnumMediaTypes(IEnumMediaTypes **ppEnum)
{
	if (ppEnum == NULL)
			return E_POINTER;
	return E_NOTIMPL;
}

STDMETHODIMP CHWCPin::QueryInternalConnections(IPin **ppPin, ULONG *nPin)
{
	return E_NOTIMPL;
}

STDMETHODIMP CHWCPin::EndOfStream(void)
{
	return S_OK;
}

STDMETHODIMP CHWCPin::BeginFlush(void)
{
	return S_OK;
}

STDMETHODIMP CHWCPin::EndFlush(void)
{
	return S_OK;
}

STDMETHODIMP CHWCPin::NewSegment(REFERENCE_TIME tStart,
								REFERENCE_TIME tStop,
								double dRate)
{
	return S_OK;
}

STDMETHODIMP CHWCPin::GetAllocator(IMemAllocator **ppAllocator)
{
	if (ppAllocator == NULL)
		return E_POINTER;
	return VFW_E_NO_ALLOCATOR;
}

STDMETHODIMP CHWCPin::NotifyAllocator(IMemAllocator *pAllocator,
										BOOL bReadOnly)
{
	if (pAllocator == NULL)
		return E_POINTER;

	return NOERROR;
}

STDMETHODIMP CHWCPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps)
{
	return E_NOTIMPL;
}

STDMETHODIMP CHWCPin::Receive(IMediaSample *pSample)
{
	if (pSample == NULL)
		return E_POINTER;
	if (m_pCallback != NULL) {
		HRESULT hr;
		BYTE* pBuffer = NULL;
		DWORD dwSize = 0;
		dwSize = pSample->GetSize();
		hr = pSample->GetPointer(&pBuffer);
		if (FAILED(hr))
			return hr;
		m_pCallback->CaptureCallback(dwSize, pBuffer);
	}
	return S_OK;
}

STDMETHODIMP CHWCPin::ReceiveMultiple(IMediaSample **pSamples,
										long nSamples, long *nSamplesProcessed)
{
	if (pSamples == NULL)
		return E_POINTER;
	HRESULT hr = S_OK;
	nSamplesProcessed = 0;
	while (nSamples-- > 0) 
	{
		hr = Receive(pSamples[*nSamplesProcessed]);
		if (hr != S_OK)
			break;
		(*nSamplesProcessed)++;
	}
	return hr;
}

STDMETHODIMP CHWCPin::ReceiveCanBlock(void)
{
	return S_FALSE;
}

STDMETHODIMP CHWCPin::SetCallback(ICaptureCallBack *pCaptureCB)
{
	if (pCaptureCB == NULL) {
		m_pCallback->Release();
	} else {
		m_pCallback = pCaptureCB;
		m_pCallback->AddRef();
	}	
	return S_OK;
}

CHWCEnumPins::CHWCEnumPins(CHWCFilter *pCHWCFilter, int pos) :
  m_pFilter(pCHWCFilter), m_pos(pos), m_cRef(1)
{
	m_pFilter->AddRef();
	InterlockedIncrement(&g_cComponents);
}

CHWCEnumPins::~CHWCEnumPins(void)
{
	m_pFilter->Release();
	InterlockedDecrement(&g_cComponents);
}

STDMETHODIMP CHWCEnumPins::QueryInterface(REFIID riid, void **ppv)
{
	if (ppv == NULL)
		return E_POINTER;

	if (riid == IID_IUnknown) {
		*ppv = (IUnknown*)this;
	} else if (riid == IID_IEnumPins) {
		*ppv = (IEnumPins*)this;
	} else {
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	reinterpret_cast<IUnknown*>(this)->AddRef();
	return S_OK;
}

STDMETHODIMP_(ULONG) CHWCEnumPins::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CHWCEnumPins::Release()
{
	if (InterlockedDecrement(&m_cRef) == 0) {
		delete this;
		return 0;
	}
	return m_cRef;
}

STDMETHODIMP CHWCEnumPins::Next(ULONG cPins, IPin **ppPins,
								ULONG *pcFetched)
{
	if (ppPins == NULL)
			return E_POINTER;

	ULONG fetched;
	if (m_pos < 1 && cPins > 0) {
		IPin *pPin;
		m_pFilter->FindPin(HWCPinName, &pPin);
		*ppPins = pPin;
		pPin->AddRef();
		fetched = 1;
		m_pos++;
	} else {
		fetched = 0;
	}
	if (pcFetched != NULL ) {
		*pcFetched = fetched;
	}

	return ( fetched == cPins ) ? S_OK : S_FALSE;
}

STDMETHODIMP CHWCEnumPins::Skip(ULONG cPins)
{
	m_pos += cPins;
	return ( m_pos >= 1 ) ? S_FALSE : S_OK;
}

STDMETHODIMP CHWCEnumPins::Reset(void)
{
	m_pos = 0;
	return S_OK;
}

STDMETHODIMP CHWCEnumPins::Clone(IEnumPins **ppEnum)
{
	if (ppEnum == NULL)
		return E_POINTER;
	*ppEnum = new CHWCEnumPins(m_pFilter, m_pos);
	if (*ppEnum == NULL) {
		return E_OUTOFMEMORY;
	}
	return NOERROR;
}

CHWCFilter::CHWCFilter() : m_pFilterGraph(NULL), m_state(State_Stopped), m_cRef(1)
{
	m_pPin = new CHWCPin(this);
	InterlockedIncrement(&g_cComponents);
}

CHWCFilter::~CHWCFilter()
{
	InterlockedDecrement(&g_cComponents);

	if (m_pPin) {
		m_pPin->Release();
		m_pPin = NULL;
	}
	if (m_pFilterGraph) {
		m_pFilterGraph->Release();
		m_pFilterGraph = NULL;
	}
}

STDMETHODIMP CHWCFilter::QueryInterface(REFIID riid, void **ppv)
{
	if( riid == IID_IUnknown ) {
		*ppv = (IUnknown*)this;
	} else if( riid == IID_IPersist ) {
		*ppv = (IPersist*)this;
	} else if( riid == IID_IMediaFilter ) {
		*ppv = (IMediaFilter*)this;
	} else if( riid == IID_IBaseFilter ) {
		*ppv = (IBaseFilter*)this;
	} else {
		LPWSTR str;
		StringFromIID(riid, &str);
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	reinterpret_cast<IUnknown*>(this)->AddRef();

	return S_OK;
}

STDMETHODIMP_(ULONG) CHWCFilter::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CHWCFilter::Release()
{
	if( InterlockedDecrement(&m_cRef) == 0) {
		delete this;
		return 0;
	}
	return m_cRef;
}

STDMETHODIMP CHWCFilter::GetClassID(CLSID *pClsID)
{
	if (pClsID == NULL)
		return E_POINTER;
	return E_NOTIMPL;
}

STDMETHODIMP CHWCFilter::GetState(DWORD dwMSecs, FILTER_STATE *State)
{
	*State = m_state;
	return S_OK;
}

STDMETHODIMP CHWCFilter::SetSyncSource(IReferenceClock *pClock)
{
	return S_OK;
}

STDMETHODIMP CHWCFilter::GetSyncSource(IReferenceClock **pClock)
{
	*pClock = NULL;
	return NOERROR;
}

STDMETHODIMP CHWCFilter::Stop()
{
	m_pPin->EndFlush();
	m_state = State_Stopped;
	return S_OK;
}

STDMETHODIMP CHWCFilter::Pause()
{
	m_state = State_Paused;
	return S_OK;
}

STDMETHODIMP CHWCFilter::Run(REFERENCE_TIME tStart)
{
	if (m_state == State_Stopped){
		HRESULT hr = Pause();
		if (FAILED(hr)) {
			return hr;
		}
	}
	m_state = State_Running;
	return S_OK;
}

STDMETHODIMP CHWCFilter::EnumPins(IEnumPins **ppEnum)
{
	if (ppEnum == NULL)
		return E_POINTER;
	*ppEnum = (IEnumPins*)(new CHWCEnumPins(this, 0));
	return *ppEnum == NULL ? E_OUTOFMEMORY : S_OK;
}

STDMETHODIMP CHWCFilter::FindPin(LPCWSTR Id, IPin **ppPin)
{
	if (ppPin == NULL)
		return E_POINTER;

	if (memcmp((void*)Id, (void*)HWCPinName, sizeof(HWCPinName))) {
		return VFW_E_NOT_FOUND;
	}

	if (!m_pPin) {
		m_pPin = new CHWCPin(this);
	}
	*ppPin = (IPin*)m_pPin;
	m_pPin->AddRef();
	return S_OK;
}

STDMETHODIMP CHWCFilter::QueryFilterInfo(FILTER_INFO *pInfo)
{
	if (pInfo == NULL)
		return E_POINTER;

	memcpy((void*)pInfo->achName, (void*)HWCFilterName, sizeof(HWCFilterName));
	pInfo->pGraph = m_pFilterGraph;
	if(m_pFilterGraph) {
		m_pFilterGraph->AddRef();
	}
	return S_OK;
}

STDMETHODIMP CHWCFilter::JoinFilterGraph(IFilterGraph *pGraph,
										LPCWSTR pName)
{
	m_pFilterGraph = pGraph;
	pGraph->AddRef();
	return S_OK;
}

STDMETHODIMP CHWCFilter::QueryVendorInfo(LPWSTR* pVendorInfo)
{
	return E_NOTIMPL;
}

HWCClassFactory::HWCClassFactory(void) : m_cRef(1)
{
}

HWCClassFactory::~HWCClassFactory(void)
{
}

STDMETHODIMP HWCClassFactory::QueryInterface(REFIID riid, LPVOID* ppv)
{
	if (riid == IID_IUnknown || riid == IID_IClassFactory) {
		*ppv = (IClassFactory*)this;
	} else {
		*ppv = NULL;
		return E_NOINTERFACE;
	}

	reinterpret_cast<IUnknown*>(*ppv)->AddRef();
	return S_OK;
}

STDMETHODIMP_(ULONG) HWCClassFactory::AddRef(void)
{
	return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) HWCClassFactory::Release(void)
{
	if (InterlockedDecrement(&m_cRef) == 0) {
		delete this;
		return 0;
	}
	return m_cRef;
}

STDMETHODIMP HWCClassFactory::CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, LPVOID* ppv)
{
	HRESULT hr;
	CHWCFilter* pFilter = NULL;
	*ppv = NULL;

	if (pUnkOuter != NULL) {
		hr = CLASS_E_NOAGGREGATION;
	} else {
		pFilter = new CHWCFilter;
		if (pFilter != NULL) {
			hr = pFilter->QueryInterface(riid, ppv);
			pFilter->Release();
		} else {
			return E_OUTOFMEMORY;
		}
	}

	return hr;
}

STDMETHODIMP HWCClassFactory::LockServer(BOOL bLock)
{
	if (bLock) {
		InterlockedIncrement(&g_cServerLocks);
	} else {
		InterlockedDecrement(&g_cServerLocks);
	}
	return S_OK;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;
	HWCClassFactory *pFactory = NULL;

	if (rclsid != CLSID_HWCFilterClass) {
		return hr;
	}

	pFactory = new HWCClassFactory;
	if (pFactory == NULL) {
		hr = E_OUTOFMEMORY;
	} else {
		hr = pFactory->QueryInterface(riid, ppv);
		pFactory->Release();
	}

	return hr;
}

STDAPI DllCanUnloadNow(void)
{
	if ((g_cComponents == 0) && (g_cServerLocks == 0)) {
		return S_OK;
	}
	return S_FALSE;
}

STDAPI DllRegisterServer(void)
{
	return S_OK;
}

STDAPI DllUnregisterServer(void)
{
	return S_OK;
}
