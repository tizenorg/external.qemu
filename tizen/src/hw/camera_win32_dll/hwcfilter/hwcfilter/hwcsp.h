#ifndef __HWCPS_H__
#define __HWCPS_H__

#ifdef HWCFILTER_EXPORTS
#define HWCFILTER_API extern "C" __declspec(dllexport) HRESULT __stdcall
#else
#define HWCFILTER_API __declspec(dllimport)
#endif

#include <DShow.h>
#include "hwcfilter.h"

#define SAFE_RELEASE(x)		if (x) { (x)->Release(); (x) = NULL; }

typedef int (STDAPICALLTYPE *CallbackFn)(ULONG dwSize, BYTE *pBuffer);

class CCallback : ICaptureCallBack
{
public:
	CCallback();
	virtual ~CCallback();
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP CaptureCallback(ULONG dwSize, BYTE *pBuffer);
	STDMETHODIMP SetCallback(CallbackFn pCallbackFn);

private:
	long m_cRef;
	CallbackFn m_pCallback;
};

class HWCSP
{
public:
	HWCSP();
	~HWCSP();

	STDMETHODIMP Init();
	
	STDMETHODIMP BindSourceFilter();
	STDMETHODIMP BindTargetFilter();
	STDMETHODIMP ConnectFilters();
	STDMETHODIMP GetDeviceCaps(ULONG *fourcc, ULONG *width, ULONG *height, ULONG *fps);

	STDMETHODIMP_(void) CloseInterfaces();
	STDMETHODIMP_(void) DeleteMediaType(AM_MEDIA_TYPE *pmt);

	STDMETHODIMP QueryVideoProcAmp(long nProperty, long *pMin, long *pMax, long *pStep, long *pDefault);
	STDMETHODIMP GetVideoProcAmp(long nProperty, long *pValue);
	STDMETHODIMP SetVideoProcAmp(long nProperty, long value);

	STDMETHODIMP StartPreview();
	STDMETHODIMP StopPreview();

	STDMETHODIMP SetFPS(REFERENCE_TIME inFps);
	STDMETHODIMP GetFPS(REFERENCE_TIME *outFps);

	STDMETHODIMP SetResolution(LONG width, LONG height);
	STDMETHODIMP GetResolution(LONG *width, LONG *height);

	STDMETHODIMP GetFormats(LPVOID pData);
	STDMETHODIMP GetFrameSizes(LPVOID pData);
	STDMETHODIMP GetFrameIntervals(LPVOID pData);
	STDMETHODIMP SetDefaultValues();

	STDMETHODIMP SetCallback(CallbackFn pCallback);

private:
	STDMETHODIMP GetPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin);

	IGraphBuilder *m_pGB ;
	ICaptureGraphBuilder2 *m_pCGB;
	IMediaControl *m_pMC;
	IMediaEventEx *m_pME;
	
	IPin *m_pOutputPin;
	IPin *m_pInputPin;
	IBaseFilter *m_pDF;
	IBaseFilter *m_pSF;

	CCallback *m_pCallback;

	HINSTANCE m_hDLL;
	DWORD m_dwFourcc;
	DWORD m_dwWidth;
	DWORD m_dwHeight;
	DWORD m_dwAvgInterval;
};

enum {
	HWC_OPEN,
	HWC_CLOSE,
	HWC_START,
	HWC_STOP,
	HWC_S_FPS,
	HWC_G_FPS,
	HWC_S_FMT,
	HWC_G_FMT,
	HWC_TRY_FMT,
	HWC_ENUM_FMT,
	HWC_QCTRL,
	HWC_S_CTRL,
	HWC_G_CTRL,
	HWC_ENUM_FSIZES,
	HWC_ENUM_INTERVALS
};

typedef struct tagHWCParam {
	long val1;
	long val2;
	long val3;
	long val4;
	long val5;
} HWCParam;

HWCFILTER_API HWCCtrl(UINT nCmd, UINT nSize, LPVOID pBuf);

HWCFILTER_API HWCSetCallback(CallbackFn pCallback);

STDMETHODIMP HWCOpen();
STDMETHODIMP HWCClose();
STDMETHODIMP HWCStart();
STDMETHODIMP HWCStop();

STDMETHODIMP HWCSetFPS(long num, long denom);
STDMETHODIMP HWCGetFPS(long *num, long *denom);

STDMETHODIMP HWCSetFormat(long width, long height);
STDMETHODIMP HWCGetFormat();
STDMETHODIMP HWCTryFormat();
STDMETHODIMP HWCEnumFormat();

STDMETHODIMP HWCQueryControl(long nProperty, long *pMin, long *pMax, long *pStep, long *pDefault);
STDMETHODIMP HWCSetControlValue(long nProperty, long value);
STDMETHODIMP HWCGetControlValue(long nProperty, long *pVal);

STDMETHODIMP HWCEnumFrameSizes();
STDMETHODIMP HWCEnumFrameIntervals();

#endif	// __HWCPS_H__