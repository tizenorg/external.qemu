

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 7.00.0555 */
/* at Wed Nov 23 21:52:11 2011
 */
/* Compiler settings for HWCCallBack.idl:
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

#ifndef __HWCCallBack_h_h__
#define __HWCCallBack_h_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __ICaptureCallBack_FWD_DEFINED__
#define __ICaptureCallBack_FWD_DEFINED__
typedef interface ICaptureCallBack ICaptureCallBack;
#endif 	/* __ICaptureCallBack_FWD_DEFINED__ */


/* header files for imported files */
#include "Unknwn.h"
#include <InitGuid.h>
#include <DShow.h>

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __ICaptureCallBack_INTERFACE_DEFINED__
#define __ICaptureCallBack_INTERFACE_DEFINED__

/* interface ICaptureCallBack */
/* [full][helpstring][uuid][object] */ 


EXTERN_C const IID IID_ICaptureCallBack;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("4C337035-C89E-4B42-9B0C-367444DD70DD")
    ICaptureCallBack : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CaptureCallback( 
            /* [in] */ ULONG dwSize,
            /* [size_is][in] */ BYTE *pBuffer) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ICaptureCallBackVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ICaptureCallBack * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ICaptureCallBack * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ICaptureCallBack * This);
        
        HRESULT ( STDMETHODCALLTYPE *CaptureCallback )( 
            ICaptureCallBack * This,
            /* [in] */ ULONG dwSize,
            /* [size_is][in] */ BYTE *pBuffer);
        
        END_INTERFACE
    } ICaptureCallBackVtbl;

    interface ICaptureCallBack
    {
        CONST_VTBL struct ICaptureCallBackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICaptureCallBack_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ICaptureCallBack_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ICaptureCallBack_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ICaptureCallBack_CaptureCallback(This,dwSize,pBuffer)	\
    ( (This)->lpVtbl -> CaptureCallback(This,dwSize,pBuffer) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICaptureCallBack_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


