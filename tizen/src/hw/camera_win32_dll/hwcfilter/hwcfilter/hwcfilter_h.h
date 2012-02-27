

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 7.00.0555 */
/* at Thu Nov 24 00:56:10 2011
 */
/* Compiler settings for HWCFilter.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 7.00.0555 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __HWCFILTER_H_H__
#define __HWCFILTER_H_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IHWCPin_FWD_DEFINED__
#define __IHWCPin_FWD_DEFINED__
typedef interface IHWCPin IHWCPin;
#endif 	/* __IHWCPin_FWD_DEFINED__ */


/* header files for imported files */
#include <oaidl.h>
#include <ocidl.h>
#include "hwccallback_h.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_HWCFilter_0000_0000 */
/* [local] */ 

// {320D90F3-3150-4D22-8437-1E5C0507CC39}
DEFINE_GUID(CLSID_HWCFilterClass, 0x320d90f3, 0x3150, 0x4d22, 0x84, 0x37, 0x1e, 0x5c, 0x5, 0x7, 0xcc, 0x39);



extern RPC_IF_HANDLE __MIDL_itf_HWCFilter_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_HWCFilter_0000_0000_v0_0_s_ifspec;

#ifndef __IHWCPin_INTERFACE_DEFINED__
#define __IHWCPin_INTERFACE_DEFINED__

/* interface IHWCPin */
/* [full][helpstring][uuid][object] */ 


EXTERN_C const IID IID_IHWCPin;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("33AFDC07-C2AB-4FC4-BA54-65FADF4B474D")
    IHWCPin : public IPin
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE SetCallback( 
            ICaptureCallBack *pCaptureCB) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IHWCPinVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IHWCPin * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IHWCPin * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IHWCPin * This);
        
        HRESULT ( STDMETHODCALLTYPE *Connect )( 
            IHWCPin * This,
            /* [in] */ IPin *pReceivePin,
            /* [annotation][in] */ 
            __in_opt  const AM_MEDIA_TYPE *pmt);
        
        HRESULT ( STDMETHODCALLTYPE *ReceiveConnection )( 
            IHWCPin * This,
            /* [in] */ IPin *pConnector,
            /* [in] */ const AM_MEDIA_TYPE *pmt);
        
        HRESULT ( STDMETHODCALLTYPE *Disconnect )( 
            IHWCPin * This);
        
        HRESULT ( STDMETHODCALLTYPE *ConnectedTo )( 
            IHWCPin * This,
            /* [annotation][out] */ 
            __out  IPin **pPin);
        
        HRESULT ( STDMETHODCALLTYPE *ConnectionMediaType )( 
            IHWCPin * This,
            /* [annotation][out] */ 
            __out  AM_MEDIA_TYPE *pmt);
        
        HRESULT ( STDMETHODCALLTYPE *QueryPinInfo )( 
            IHWCPin * This,
            /* [annotation][out] */ 
            __out  PIN_INFO *pInfo);
        
        HRESULT ( STDMETHODCALLTYPE *QueryDirection )( 
            IHWCPin * This,
            /* [annotation][out] */ 
            __out  PIN_DIRECTION *pPinDir);
        
        HRESULT ( STDMETHODCALLTYPE *QueryId )( 
            IHWCPin * This,
            /* [annotation][out] */ 
            __out  LPWSTR *Id);
        
        HRESULT ( STDMETHODCALLTYPE *QueryAccept )( 
            IHWCPin * This,
            /* [in] */ const AM_MEDIA_TYPE *pmt);
        
        HRESULT ( STDMETHODCALLTYPE *EnumMediaTypes )( 
            IHWCPin * This,
            /* [annotation][out] */ 
            __out  IEnumMediaTypes **ppEnum);
        
        HRESULT ( STDMETHODCALLTYPE *QueryInternalConnections )( 
            IHWCPin * This,
            /* [annotation][out] */ 
            __out_ecount_part_opt(*nPin, *nPin)  IPin **apPin,
            /* [out][in] */ ULONG *nPin);
        
        HRESULT ( STDMETHODCALLTYPE *EndOfStream )( 
            IHWCPin * This);
        
        HRESULT ( STDMETHODCALLTYPE *BeginFlush )( 
            IHWCPin * This);
        
        HRESULT ( STDMETHODCALLTYPE *EndFlush )( 
            IHWCPin * This);
        
        HRESULT ( STDMETHODCALLTYPE *NewSegment )( 
            IHWCPin * This,
            /* [in] */ REFERENCE_TIME tStart,
            /* [in] */ REFERENCE_TIME tStop,
            /* [in] */ double dRate);
        
        HRESULT ( STDMETHODCALLTYPE *SetCallback )( 
            IHWCPin * This,
            ICaptureCallBack *pCaptureCB);
        
        END_INTERFACE
    } IHWCPinVtbl;

    interface IHWCPin
    {
        CONST_VTBL struct IHWCPinVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IHWCPin_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IHWCPin_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IHWCPin_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IHWCPin_Connect(This,pReceivePin,pmt)	\
    ( (This)->lpVtbl -> Connect(This,pReceivePin,pmt) ) 

#define IHWCPin_ReceiveConnection(This,pConnector,pmt)	\
    ( (This)->lpVtbl -> ReceiveConnection(This,pConnector,pmt) ) 

#define IHWCPin_Disconnect(This)	\
    ( (This)->lpVtbl -> Disconnect(This) ) 

#define IHWCPin_ConnectedTo(This,pPin)	\
    ( (This)->lpVtbl -> ConnectedTo(This,pPin) ) 

#define IHWCPin_ConnectionMediaType(This,pmt)	\
    ( (This)->lpVtbl -> ConnectionMediaType(This,pmt) ) 

#define IHWCPin_QueryPinInfo(This,pInfo)	\
    ( (This)->lpVtbl -> QueryPinInfo(This,pInfo) ) 

#define IHWCPin_QueryDirection(This,pPinDir)	\
    ( (This)->lpVtbl -> QueryDirection(This,pPinDir) ) 

#define IHWCPin_QueryId(This,Id)	\
    ( (This)->lpVtbl -> QueryId(This,Id) ) 

#define IHWCPin_QueryAccept(This,pmt)	\
    ( (This)->lpVtbl -> QueryAccept(This,pmt) ) 

#define IHWCPin_EnumMediaTypes(This,ppEnum)	\
    ( (This)->lpVtbl -> EnumMediaTypes(This,ppEnum) ) 

#define IHWCPin_QueryInternalConnections(This,apPin,nPin)	\
    ( (This)->lpVtbl -> QueryInternalConnections(This,apPin,nPin) ) 

#define IHWCPin_EndOfStream(This)	\
    ( (This)->lpVtbl -> EndOfStream(This) ) 

#define IHWCPin_BeginFlush(This)	\
    ( (This)->lpVtbl -> BeginFlush(This) ) 

#define IHWCPin_EndFlush(This)	\
    ( (This)->lpVtbl -> EndFlush(This) ) 

#define IHWCPin_NewSegment(This,tStart,tStop,dRate)	\
    ( (This)->lpVtbl -> NewSegment(This,tStart,tStop,dRate) ) 


#define IHWCPin_SetCallback(This,pCaptureCB)	\
    ( (This)->lpVtbl -> SetCallback(This,pCaptureCB) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IHWCPin_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif // __HWCFILTER_H_H__


