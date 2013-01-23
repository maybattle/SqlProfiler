/*****************************************************************************
 * DotNetProfiler
 * 
 * Copyright (c) 2006 Scott Hackett
 * 
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the author be held liable for any damages arising from the
 * use of this software. Permission to use, copy, modify, distribute and sell
 * this software for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 * 
 * Scott Hackett (code@scotthackett.com)
 *****************************************************************************/

#pragma once
#include "resource.h"
#include "DotNetProfiler.h"
#include "CorProfilerCallbackImpl.h"
#include "resource.h"
#include "FunctionInfo.h"
#include <map>

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

#define ASSERT_HR(x) _ASSERT(SUCCEEDED(x))
#define NAME_BUFFER_SIZE 1024
#define MAX_FUNCTION_LENGTH NAME_BUFFER_SIZE

// CProfiler
class ATL_NO_VTABLE CProfiler :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CProfiler, &CLSID_Profiler>,
	public CCorProfilerCallbackImpl
{
public:
	CProfiler();

	DECLARE_REGISTRY_RESOURCEID(IDR_PROFILER)
	BEGIN_COM_MAP(CProfiler)
		COM_INTERFACE_ENTRY(ICorProfilerCallback)
		COM_INTERFACE_ENTRY(ICorProfilerCallback2)
	END_COM_MAP()
	DECLARE_PROTECT_FINAL_CONSTRUCT()

	// overridden implementations of FinalConstruct and FinalRelease
	HRESULT FinalConstruct();
	void FinalRelease();

    // STARTUP/SHUTDOWN EVENTS
    STDMETHOD(Initialize)(IUnknown *pICorProfilerInfoUnk);
    STDMETHOD(Shutdown)();

	// callback functions
	void Enter(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo, COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo);
	void Leave(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo, COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange);
	void Tailcall(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo);

	// mapping functions
	static UINT_PTR _stdcall FunctionMapper(FunctionID functionId, BOOL *pbHookFunction);
	void MapFunction(FunctionID);

	// logging function
    void LogString(char* pszFmtString, ... );

private:
    // container for ICorProfilerInfo reference
	CComQIPtr<ICorProfilerInfo> m_pICorProfilerInfo;
    // container for ICorProfilerInfo2 reference
	CComQIPtr<ICorProfilerInfo2> m_pICorProfilerInfo2;

	// the number of levels deep we are in the call stack
	int m_callStackSize;
	// handle and filename of log file
	HANDLE m_hLogFile;
	TCHAR m_logFileName[_MAX_PATH]; 
	CRITICAL_SECTION m_prf_crit_sec; // SYNCHRONIZATION PRIMITIVE
	ClassID m_MyClassID;	


	// STL map for our hashed functions
	std::map<FunctionID, CFunctionInfo*> m_functionMap;
	
	bool TryGetParameterValue(char *pFilterFunctionName, CFunctionInfo *pFunctionInfo, COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange,void * pRetValue, size_t *pRetValueLength);
	bool TryGetCommandText(char *pFilterFunctionName, CFunctionInfo *pFunctionInfo, COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange,string &result);
	bool TryGetParameter(char *pFilterFunctionName, CFunctionInfo *pFunctionInfo, COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange,string &result);

	//get the return value of a function
	std::string CProfiler::GetReturnTypeName(const FunctionID functionId, const COR_PRF_FUNCTION_ARGUMENT_RANGE *range, INT32 *pCoreElementType);
	void CProfiler::GetDateTimeReturnValue(COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange);
	void GetUnspecifiedReturnValue(COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange, void *pRetValue, size_t* pRetValueLength);
	PCCOR_SIGNATURE CProfiler::ParseElementType( IMetaDataImport *pMDImport, PCCOR_SIGNATURE signature, LPWSTR signatureText, INT32 *pElementType);

	DOUBLE CProfiler::GetDoubleReturnValue(const COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange);
	INT32 CProfiler::GetInt32ReturnValue(const COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange);
	FLOAT CProfiler::GetFloatReturnValue(const COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange);
	string CProfiler::GetStringReturnValue(COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange);

	// gets the full method name given a function ID
	HRESULT GetFullMethodName(FunctionID functionId, LPWSTR wszMethod, int cMethod );
	string CProfiler::GetClassName(ClassID classId);

	HRESULT GetMethodParameters(FunctionID functionId, COR_PRF_FRAME_INFO frameInfo, LPWSTR wszMethod, int cMethod);
	// function to set up our event mask
	HRESULT SetEventMask();
	// creates the log file
	void CreateLogFile();
	// closes the log file ;)
	void CloseLogFile();
	bool IsReferenceType(CorElementType type);
	WCHAR* DecodeString(PBYTE pObjectByte);
	// get the name of the class for this classID
    //bool ClassName( ClassID classID, LPWSTR wszClass);
	//void CProfiler::PrintParameterValue(int index,CorElementType type,PBYTE pValues);
	//void PrintFunctionArguments (FunctionID funcId,COR_PRF_FRAME_INFO func,COR_PRF_FUNCTION_ARGUMENT_RANGE* range);

	

};

OBJECT_ENTRY_AUTO(__uuidof(Profiler), CProfiler)
