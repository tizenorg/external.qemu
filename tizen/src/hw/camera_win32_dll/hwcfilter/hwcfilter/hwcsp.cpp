#include "hwcsp.h"

CCallback::CCallback() : m_pCallback(NULL), m_cRef(1)
{
}

CCallback::~CCallback()
{
	if (m_pCallback) {
		m_pCallback = NULL;
	}
}

STDMETHODIMP CCallback::QueryInterface(REFIID riid, void **ppv)
{
	if (riid == IID_IUnknown) {
		*ppv = static_cast<IUnknown*>(this);
	} else if (riid == IID_ICaptureCallBack) {
		*ppv = static_cast<ICaptureCallBack*>(this);
	} else {
		LPWSTR str;
		StringFromIID(riid, &str);
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	reinterpret_cast<IUnknown*>(this)->AddRef();
	return S_OK;
}

STDMETHODIMP CCallback::CaptureCallback(ULONG dwSize, BYTE *pBuffer)
{	
	if (m_pCallback(dwSize, pBuffer)) {
		return S_OK;
	}
	return E_FAIL;
}

STDMETHODIMP CCallback::SetCallback(CallbackFn pCallbackFn)
{
	m_pCallback = pCallbackFn;
	return S_OK;
}

STDMETHODIMP_(ULONG) CCallback::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CCallback::Release()
{
	if( InterlockedDecrement(&m_cRef) == 0)
	{
		delete this;
		return 0;
	}
	return m_cRef;
}

HWCSP::HWCSP()
{
	CoInitialize(NULL);
	m_pGB = NULL;
	m_pCGB = NULL;
	m_pMC = NULL;
	m_pME = NULL;
	m_pOutputPin = NULL;
	m_pInputPin = NULL;
	m_pDF = NULL;
	m_pSF = NULL;
	m_hDLL = NULL;
	m_pCallback = NULL;
	m_dwFourcc = MAKEFOURCC('Y','U','Y','2');
	m_dwWidth = 640;
	m_dwHeight = 480;
	m_dwAvgInterval = 333333;
}

HWCSP::~HWCSP()
{
	CloseInterfaces();
	CoUninitialize();
}

STDMETHODIMP HWCSP::Init()
{
	HRESULT hr;

	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC, IID_IGraphBuilder, (void**)&m_pGB);
	if (FAILED(hr))
		return hr;

	hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC, IID_ICaptureGraphBuilder2, (void**)&m_pCGB);
	if (FAILED(hr))
		return hr;

	hr = m_pCGB->SetFiltergraph(m_pGB);
	if (FAILED(hr))
		return hr;

	hr = m_pGB->QueryInterface(IID_IMediaControl, (void **)&m_pMC);
	if (FAILED(hr))
		return hr;

	hr = m_pGB->QueryInterface(IID_IMediaEventEx, (void **)&m_pME);
	if (FAILED(hr))
		return hr;

	m_pCallback = new CCallback;
	if (m_pCallback == NULL)
		hr = E_OUTOFMEMORY;

	return hr;
}

STDMETHODIMP HWCSP::BindSourceFilter()
{
	HRESULT hr;
	
	ICreateDevEnum *pCreateDevEnum = NULL;
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, IID_ICreateDevEnum, (void**)&pCreateDevEnum);
	if (FAILED(hr))
		return hr;

	IEnumMoniker *pEnumMK = NULL;
	hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumMK, 0);
	if (FAILED(hr))
	{
		pCreateDevEnum->Release();
		return hr;
	}
	
	if (!pEnumMK)
	{
		pCreateDevEnum->Release();
		return E_FAIL;
	}
	pEnumMK->Reset();

	IMoniker *pMoniKer;
	hr = pEnumMK->Next(1, &pMoniKer, NULL);
	if (hr == S_FALSE)
	{
		hr = E_FAIL;
	}
	if (SUCCEEDED(hr))
	{
		IPropertyBag *pBag = NULL;
		IBaseFilter *temp = NULL;
		hr = pMoniKer->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pBag);
		if(SUCCEEDED(hr)) 
		{
			VARIANT var;
			var.vt = VT_BSTR;
			hr = pBag->Read(L"FriendlyName", &var, NULL);
			if (hr == NOERROR)
			{
				hr = pMoniKer->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&m_pDF);
				if (FAILED(hr))
				{
					//Counldn't bind moniker to filter object!!
				}
				else
				{
					m_pDF->AddRef();
				}
				SysFreeString(var.bstrVal);
			}
			pBag->Release();
		}
		pMoniKer->Release();
	}

	if (SUCCEEDED(hr))
	{
		hr = m_pGB->AddFilter(m_pDF, L"Video Capture");
		if (hr != S_OK && hr != S_FALSE)
		{
			//Counldn't add Video Capture filter to our graph!
		}
	}

	return hr;
}

STDMETHODIMP HWCSP::BindTargetFilter()
{
	HRESULT hr;
	IClassFactory *pClassFactory= NULL;
	hr = DllGetClassObject(CLSID_HWCFilterClass, IID_IClassFactory, (void**)&pClassFactory);
	if (FAILED(hr))
		return hr;

	IBaseFilter *pHWCFilter = NULL;
	pClassFactory->CreateInstance(NULL, IID_IBaseFilter, (void**)&pHWCFilter);
	if (FAILED(hr))
	{
		SAFE_RELEASE(pClassFactory);
		return hr;
	}
	m_pSF = pHWCFilter;

	if (SUCCEEDED(hr))
	{
		hr = m_pGB->AddFilter(m_pSF, L"HWCFilter");
		if (FAILED(hr))
		{
			//Counldn't add HWCFilterr to our graph!
		}
	}
	SAFE_RELEASE(pClassFactory);
	
	return hr;
}

STDMETHODIMP HWCSP::ConnectFilters()
{
	HRESULT hr;

	hr = GetPin(m_pDF, PINDIR_OUTPUT , &m_pOutputPin);
	if (FAILED(hr))
		return hr;

	hr = GetPin(m_pSF, PINDIR_INPUT , &m_pInputPin);
	if (FAILED(hr))
		return hr;

	hr = m_pGB->Connect(m_pOutputPin, m_pInputPin);
	return hr;
}

STDMETHODIMP_(void) HWCSP::CloseInterfaces() 
{
	if (m_pMC)
		m_pMC->Stop();
	
	if (m_pOutputPin)
		m_pOutputPin->Disconnect();

	SAFE_RELEASE(m_pGB);
	SAFE_RELEASE(m_pCGB);
	SAFE_RELEASE(m_pMC);
	SAFE_RELEASE(m_pME);
	SAFE_RELEASE(m_pOutputPin);
	SAFE_RELEASE(m_pInputPin);
	SAFE_RELEASE(m_pDF);
	SAFE_RELEASE(m_pSF);
	SAFE_RELEASE(m_pCallback);

	if (m_hDLL) {
		FreeLibrary(m_hDLL);
		m_hDLL = NULL;
	}
}

STDMETHODIMP_(void) HWCSP::DeleteMediaType(AM_MEDIA_TYPE *pmt)
{
	if (pmt == NULL) {
		return;
	}

	if (pmt->cbFormat != 0) {
		CoTaskMemFree((PVOID)pmt->pbFormat);
		pmt->cbFormat = 0;
		pmt->pbFormat = NULL;
	}
	if (pmt->pUnk != NULL) {
		pmt->pUnk->Release();
		pmt->pUnk = NULL;
	}

	CoTaskMemFree((PVOID)pmt);
}

STDMETHODIMP HWCSP::GetPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin)
{
	HRESULT	hr;
	IEnumPins *pEnum = NULL;
	IPin *pPin = NULL;

	if (ppPin == NULL)
	{
		return E_POINTER;
	}

	hr = pFilter->EnumPins(&pEnum);
	if (FAILED(hr))
		return hr;

	while(pEnum->Next(1, &pPin, 0) == S_OK)
	{
		PIN_DIRECTION PinDirThis;
		hr = pPin->QueryDirection(&PinDirThis);
		if (FAILED(hr))
		{
			SAFE_RELEASE(pPin);
			SAFE_RELEASE(pEnum);
			return hr;
		}
		if (PinDir == PinDirThis)
		{
			*ppPin = pPin;
			SAFE_RELEASE(pEnum);
			return S_OK;
		}
		SAFE_RELEASE(pPin);
	}

	SAFE_RELEASE(pEnum);
	return S_FALSE;
}

STDMETHODIMP HWCSP::QueryVideoProcAmp(long nProperty, long *pMin, long *pMax, long *pStep, long *pDefault)
{
	HRESULT hr;

	IAMVideoProcAmp *pProcAmp = NULL;
	hr = m_pDF->QueryInterface(IID_IAMVideoProcAmp, (void**)&pProcAmp);
	if (FAILED(hr))
	{
		return hr;
	}

	long Flags;
	hr = pProcAmp->GetRange(nProperty, pMin, pMax, pStep, pDefault, &Flags);

	SAFE_RELEASE(pProcAmp);
	return hr;
}

STDMETHODIMP HWCSP::GetVideoProcAmp(long nProperty, long *pValue)
{
	HRESULT hr;

	IAMVideoProcAmp *pProcAmp = NULL;
	hr = m_pDF->QueryInterface(IID_IAMVideoProcAmp, (void**)&pProcAmp);
	if (FAILED(hr))
		return hr;

	long Flags;
	hr = pProcAmp->Get(nProperty, pValue, &Flags);
	if (FAILED(hr))
	{
	}
	SAFE_RELEASE(pProcAmp);
	return hr;
}

STDMETHODIMP HWCSP::SetVideoProcAmp(long nProperty, long value)
{
	HRESULT hr;

	IAMVideoProcAmp *pProcAmp = NULL;
	hr = m_pDF->QueryInterface(IID_IAMVideoProcAmp, (void**)&pProcAmp);
	if (FAILED(hr))
		return hr;

	hr = pProcAmp->Set(nProperty, value, VideoProcAmp_Flags_Manual);
	SAFE_RELEASE(pProcAmp);
	return hr;
}

STDMETHODIMP HWCSP::StartPreview()
{
	HRESULT hr;

	hr = ((IHWCPin*)m_pInputPin)->SetCallback((ICaptureCallBack*)m_pCallback);
	if (FAILED(hr)) 
		return hr;

	hr = m_pMC->Run();
	return hr;
}

STDMETHODIMP HWCSP::StopPreview()
{
	HRESULT hr;
	hr = ((IHWCPin*)m_pInputPin)->SetCallback(NULL);

	if (SUCCEEDED(hr))
		hr = m_pMC->Stop();
	return hr;
}
STDMETHODIMP HWCSP::GetDeviceCaps(ULONG *fourcc, ULONG *width, ULONG *height, ULONG *fps)
{
	HRESULT hr;
	IAMStreamConfig* vsc = NULL;
	AM_MEDIA_TYPE* pmt = NULL;
	hr = m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pDF, IID_IAMStreamConfig, (void**)&vsc);
	if (FAILED(hr))
		return hr;

	hr = vsc->GetFormat(&pmt);
	if (FAILED(hr)) 
	{
		vsc->Release();
		return hr;
	}

	if (pmt != NULL)
	{
		if (pmt->formattype == FORMAT_VideoInfo)
		{
			VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmt->pbFormat;
			if (pvi->bmiHeader.biBitCount == 24 && pvi->bmiHeader.biCompression == BI_RGB) {
				*fourcc = MAKEFOURCC('R', 'G', 'B', '3');
			} else {
				*fourcc = (ULONG)pvi->bmiHeader.biCompression;
			}
			*width = (ULONG)pvi->bmiHeader.biWidth;
			*height = (ULONG)pvi->bmiHeader.biHeight;
			*fps = (ULONG)pvi->AvgTimePerFrame;
		}
		DeleteMediaType(pmt);
	}
	vsc->Release();

	return hr;
}

STDMETHODIMP HWCSP::SetFPS(REFERENCE_TIME inFps)
{
	HRESULT hr;
	IAMStreamConfig* vsc = NULL;
	AM_MEDIA_TYPE* pmt = NULL;
	hr = m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pDF, IID_IAMStreamConfig, (void**)&vsc);
	if (FAILED(hr))
		return hr;

	hr = vsc->GetFormat(&pmt);
	if (FAILED(hr))
	{
		vsc->Release();
		return hr;
	}

	if (pmt != NULL)
	{
		if (pmt->formattype == FORMAT_VideoInfo)
		{
			VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmt->pbFormat;
			pvi->AvgTimePerFrame = inFps;
			hr = vsc->SetFormat(pmt);
		}
		DeleteMediaType(pmt);
	}
	vsc->Release();
	return hr;
}

STDMETHODIMP HWCSP::GetFPS(REFERENCE_TIME *outFps)
{
	HRESULT hr;
	IAMStreamConfig* vsc = NULL;
	AM_MEDIA_TYPE* pmt = NULL;
	hr = m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pDF, IID_IAMStreamConfig, (void**)&vsc);
	if (FAILED(hr))
		return hr;

	hr = vsc->GetFormat(&pmt);
	if (FAILED(hr))
	{
		vsc->Release();
		return hr;
	}

	if (pmt != NULL)
	{
		if (pmt->formattype == FORMAT_VideoInfo)
		{
			VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmt->pbFormat;
			*outFps = pvi->AvgTimePerFrame;
		}
		DeleteMediaType(pmt);
	}
	vsc->Release();
	return hr;
}


STDMETHODIMP HWCSP::SetResolution(LONG width, LONG height)
{
	HRESULT hr;
	IAMStreamConfig* vsc = NULL;
	AM_MEDIA_TYPE* pmt = NULL;
	hr = m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pDF, IID_IAMStreamConfig, (void**)&vsc);
	if (FAILED(hr))
		return hr;

	hr = vsc->GetFormat(&pmt);
	if (FAILED(hr))
	{
		vsc->Release();
		return hr;
	}

	if (pmt != NULL)
	{
		if (pmt->formattype == FORMAT_VideoInfo)
		{
			VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmt->pbFormat;
			pvi->bmiHeader.biWidth = width;
			pvi->bmiHeader.biHeight = height;
			pvi->AvgTimePerFrame = 333333;
			pvi->bmiHeader.biSizeImage = ((width * pvi->bmiHeader.biBitCount) >> 3 ) * height;
			hr = vsc->SetFormat(pmt);
		}
		DeleteMediaType(pmt);
	}
	vsc->Release();
	return hr;
}

STDMETHODIMP HWCSP::GetResolution(LONG *width, LONG *height)
{
	HRESULT hr;
	IAMStreamConfig* vsc = NULL;
	AM_MEDIA_TYPE* pmt = NULL;
	hr = m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pDF, IID_IAMStreamConfig, (void**)&vsc);
	if (FAILED(hr))
		return hr;

	hr = vsc->GetFormat(&pmt);
	if (FAILED(hr))
	{
		vsc->Release();
		return hr;
	}

	if (pmt != NULL)
	{
		if (pmt->formattype == FORMAT_VideoInfo)
		{
			VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmt->pbFormat;
			*width = pvi->bmiHeader.biWidth;
			*height = pvi->bmiHeader.biHeight;
		}
		DeleteMediaType(pmt);
	}
	vsc->Release();
	return hr;
}

STDMETHODIMP HWCSP::GetFormats(LPVOID pData)
{
	HRESULT hr;
	IAMStreamConfig *pSC;

	if (pData == NULL)
		return E_POINTER;

	HWCParam *param = (HWCParam*)pData;

	hr = m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, 0, m_pDF, IID_IAMStreamConfig, (void**)&pSC);
	if (FAILED(hr))
		return hr;

	int iCount = 0, iSize = 0, nIndex = 0;
	hr = pSC->GetNumberOfCapabilities(&iCount, &iSize);
	if (FAILED(hr))
	{
		pSC->Release();
		return hr;
	}

	if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS))
	{
		for (int iFormat = 0; iFormat < iCount; iFormat++)
		{
			VIDEO_STREAM_CONFIG_CAPS scc;
			AM_MEDIA_TYPE *pmtConfig;
			hr = pSC->GetStreamCaps(iFormat, &pmtConfig, (BYTE*)&scc);
			if (SUCCEEDED(hr))
			{
				if (pmtConfig != NULL && pmtConfig->formattype == FORMAT_VideoInfo)
				{
					VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmtConfig->pbFormat;
					// processing!!!!
				}
			}
			DeleteMediaType(pmtConfig);
		}
	}
	pSC->Release();
	return hr;
}

STDMETHODIMP HWCSP::GetFrameSizes(LPVOID pData)
{
	HRESULT hr;
	IAMStreamConfig *pSC;

	if (pData == NULL)
		return E_POINTER;

	HWCParam *param = (HWCParam*)pData;
	hr = m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, 0, m_pDF, IID_IAMStreamConfig, (void**)&pSC);
	if (FAILED(hr))
		return hr;

	int iCount = 0, iSize = 0, nIndex = 0;
	hr = pSC->GetNumberOfCapabilities(&iCount, &iSize);
	if (FAILED(hr))
	{
		pSC->Release();
		return hr;
	}

	if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS))
	{
		for (int iFormat = 0; iFormat < iCount; iFormat++)
		{
			VIDEO_STREAM_CONFIG_CAPS scc;
			AM_MEDIA_TYPE *pmtConfig;
			hr = pSC->GetStreamCaps(iFormat, &pmtConfig, (BYTE*)&scc);
			if (SUCCEEDED(hr))
			{
				if (pmtConfig != NULL && pmtConfig->formattype == FORMAT_VideoInfo)
				{
					VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmtConfig->pbFormat;
					// processing!!!!
				}
			}
			DeleteMediaType(pmtConfig);
		}
	}
	pSC->Release();
	return hr;
}

STDMETHODIMP HWCSP::SetDefaultValues()
{
	HRESULT hr;
	IAMStreamConfig *pSConfig;

	hr = m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, 0, m_pDF, IID_IAMStreamConfig, (void**)&pSConfig);
	if (FAILED(hr))
		return hr;

	int iCount = 0, iSize = 0;
	hr = pSConfig->GetNumberOfCapabilities(&iCount, &iSize);
	if (FAILED(hr))
	{
		pSConfig->Release();
		return hr;
	}

	if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS))
	{
		for (int iFormat = 0; iFormat < iCount; iFormat++)
		{
			VIDEO_STREAM_CONFIG_CAPS scc;
			AM_MEDIA_TYPE *pmtConfig;

			hr = pSConfig->GetStreamCaps(iFormat, &pmtConfig, (BYTE*)&scc);
			if (SUCCEEDED(hr))
			{
				if (pmtConfig->formattype == FORMAT_VideoInfo)
				{
					VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmtConfig->pbFormat;
					if ((pvi->bmiHeader.biWidth == m_dwWidth) && 
						(pvi->bmiHeader.biHeight == m_dwHeight) && 
						(pvi->bmiHeader.biCompression == m_dwFourcc)) 
					{
						pvi->AvgTimePerFrame = (REFERENCE_TIME)m_dwAvgInterval;
						pSConfig->SetFormat(pmtConfig);
					}
				}
			}
			DeleteMediaType(pmtConfig);
		}
	}
	pSConfig->Release();
	return hr;
}

STDMETHODIMP HWCSP::GetFrameIntervals(LPVOID pData)
{
}

STDMETHODIMP HWCSP::SetCallback(CallbackFn pCallback)
{
	if (m_pCallback == NULL) {
		return E_FAIL;
	}
	return m_pCallback->SetCallback(pCallback);
}

static HWCSP *g_pObject = NULL;

STDMETHODIMP HWCOpen()
{
	HRESULT hr;

	g_pObject = new HWCSP();
	hr = g_pObject->Init();
	if (FAILED(hr))
		goto error_failed;

	hr = g_pObject->BindSourceFilter();
	if (hr != S_OK)
		goto error_failed;

	hr = g_pObject->BindTargetFilter();
	if (FAILED(hr))
		goto error_failed;

	hr = g_pObject->ConnectFilters();
	if (FAILED(hr))
		goto error_failed;

	hr = g_pObject->SetDefaultValues();
	if (FAILED(hr))
		goto error_failed;

	return NOERROR;

error_failed:
	delete g_pObject;
	return hr;
}

STDMETHODIMP HWCClose()
{
	if (g_pObject)
		delete g_pObject;
	return NOERROR;
}

STDMETHODIMP HWCStart()
{
	if (g_pObject)
		return g_pObject->StartPreview();

	return E_FAIL;
}

STDMETHODIMP HWCStop()
{
	if (g_pObject)
		return g_pObject->StopPreview();

	return E_FAIL;
}

STDMETHODIMP HWCSetFPS(long num, long denom)
{
	HRESULT hr;
	REFERENCE_TIME inFps = (REFERENCE_TIME)(10000000 * num / denom);
	hr = g_pObject->SetFPS(inFps);
	return hr;
}

STDMETHODIMP HWCGetFPS(long *num, long *denom)
{
	HRESULT hr;
	REFERENCE_TIME outFps = 0;
	hr = g_pObject->GetFPS(&outFps);
	if (SUCCEEDED(hr)) {
		*num = 1;
		*denom = (long)(10000000 / outFps);
	} else {
		*num = 0;
		*denom = 0;
	}
	return hr;
}

STDMETHODIMP HWCSetFormat(long width, long height)
{
	HRESULT hr;
	hr = g_pObject->SetResolution(width, height);
	return hr;
}
STDMETHODIMP HWCGetFormat()
{
	return NOERROR;
}
STDMETHODIMP HWCTryFormat()
{
	return NOERROR;
}
STDMETHODIMP HWCEnumFormat()
{
	return NOERROR;
}

STDMETHODIMP HWCQueryControl(long nProperty, long *pMin, long *pMax, long *pStep, long *pDefault)
{
	if (g_pObject)
		return g_pObject->QueryVideoProcAmp(nProperty, pMin, pMax, pStep, pDefault);

	return E_FAIL;
}

STDMETHODIMP HWCSetControlValue(long nProperty, long value)
{
	if (g_pObject)
		return g_pObject->SetVideoProcAmp(nProperty, value);

	return E_FAIL;
}

STDMETHODIMP HWCGetControlValue(long nProperty, long *pVal)
{
	if (g_pObject)
		return g_pObject->GetVideoProcAmp(nProperty, pVal);

	return E_FAIL;
}

STDMETHODIMP HWCEnumFrameSizes()
{
	return NOERROR;
}
STDMETHODIMP HWCEnumFrameIntervals()
{
	return NOERROR;
}

HWCFILTER_API HWCSetCallback(CallbackFn pCallback)
{
	HRESULT hr = g_pObject->SetCallback(pCallback);
	return hr;
}

HWCFILTER_API HWCCtrl(UINT nCmd, UINT nSize, LPVOID pBuf)
{
	HRESULT hr;
	HWCParam *param = NULL;
	
	if (nSize  && pBuf)
		param = (HWCParam *)pBuf;

	switch (nCmd)
	{
	case HWC_OPEN:
		hr = HWCOpen();
		break;
	case HWC_CLOSE:
		hr = HWCClose();
		break;
	case HWC_START:
		hr = HWCStart();
		break;
	case HWC_STOP:
		hr = HWCStop();
		break;
	case HWC_S_FPS:
		hr = HWCSetFPS(param->val1, param->val2);
		break;
	case HWC_G_FPS:
		hr = HWCGetFPS(&param->val1, &param->val2);
		break;
	case HWC_S_FMT:
		hr = HWCSetFormat(param->val1, param->val2);
		break;
	case HWC_G_FMT:
		break;
	case HWC_TRY_FMT:
		break;
	case HWC_ENUM_FMT:
		break;
	case HWC_QCTRL:
		hr = HWCQueryControl(param->val1, &param->val2, &param->val3,
								&param->val4, &param->val5);
		break;
	case HWC_S_CTRL:
		hr = HWCSetControlValue(param->val1, param->val2);
		break;
	case HWC_G_CTRL:
		hr = HWCGetControlValue(param->val1, &param->val2);
		break;
	case HWC_ENUM_FSIZES:
		break;
	case HWC_ENUM_INTERVALS:
		break;
	default:
		hr = E_INVALIDARG;
		break;
	}
	return hr;
}