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

#include "stdafx.h"
#include "Profiler.h"


#pragma warning (disable: 4996) 

#define ARRAY_SIZE(s) (sizeof(s) / sizeof(s[0]))

WCHAR g_szClass[NAME_BUFFER_SIZE];
WCHAR g_szMethodName[NAME_BUFFER_SIZE];
WCHAR g_szName[NAME_BUFFER_SIZE];


// global reference to the profiler object (this) used by the static functions
CProfiler* g_pICorProfilerCallback = NULL;


// CProfiler
CProfiler::CProfiler() :
	m_MyClassID(0)
{
	m_hLogFile = INVALID_HANDLE_VALUE;
	m_callStackSize = 0;
}

HRESULT CProfiler::FinalConstruct()
{
	// create the log file
	CreateLogFile();

	// log that we have reached FinalConstruct
	LogString("Entering FinalConstruct\r\n\r\n");

	return S_OK;
}

void CProfiler::FinalRelease()
{
	// log that we have reached FinalRelease
	LogString("\r\n\r\nEntering FinalRelease\r\n\r\n");

	// close the log file
	CloseLogFile();
}

// ----  CALLBACK FUNCTIONS ------------------

// this function simply forwards the FunctionEnter call the global profiler object
void __stdcall FunctionEnterGlobal(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo, COR_PRF_FUNCTION_ARGUMENT_INFO *argInfo)
{
	// make sure the global reference to our profiler is valid
    if (g_pICorProfilerCallback != NULL)
        g_pICorProfilerCallback->Enter(functionID, clientData, frameInfo, argInfo);
}

// this function is called by the CLR when a function has been entered
void _declspec(naked) FunctionEnterNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo)
{
    __asm
    {
        push    ebp                 // Create a standard frame
        mov     ebp,esp
        pushad                      // Preserve all registers

        mov     eax,[ebp+0x14]      // argumentInfo
        push    eax
        mov     ecx,[ebp+0x10]      // func
        push    ecx
        mov     edx,[ebp+0x0C]      // clientData
        push    edx
        mov     eax,[ebp+0x08]      // functionID
        push    eax
        call    FunctionEnterGlobal

        popad                       // Restore all registers
        pop     ebp                 // Restore EBP
        ret     16
    }
}

// this function simply forwards the FunctionLeave call the global profiler object
void __stdcall FunctionLeaveGlobal(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo, COR_PRF_FUNCTION_ARGUMENT_RANGE *retvalRange)
{
	// make sure the global reference to our profiler is valid
    if (g_pICorProfilerCallback != NULL)
        g_pICorProfilerCallback->Leave(functionID,clientData,frameInfo,retvalRange);
}

// this function is called by the CLR when a function is exiting
void _declspec(naked) FunctionLeaveNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_RANGE *retvalRange)
{
    __asm
    {
        push    ebp                 // Create a standard frame
        mov     ebp,esp
        pushad                      // Preserve all registers

        mov     eax,[ebp+0x14]      // argumentInfo
        push    eax
        mov     ecx,[ebp+0x10]      // func
        push    ecx
        mov     edx,[ebp+0x0C]      // clientData
        push    edx
        mov     eax,[ebp+0x08]      // functionID
        push    eax
        call    FunctionLeaveGlobal

        popad                       // Restore all registers
        pop     ebp                 // Restore EBP
        ret     16
    }
}

// this function simply forwards the FunctionLeave call the global profiler object
void __stdcall FunctionTailcallGlobal(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo)
{
    if (g_pICorProfilerCallback != NULL)
        g_pICorProfilerCallback->Tailcall(functionID,clientData,frameInfo);
}

// this function is called by the CLR when a tailcall occurs.  A tailcall occurs when the 
// last action of a method is a call to another method.
void _declspec(naked) FunctionTailcallNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func)
{
    __asm
    {
        push    ebp                 // Create a standard frame
        mov     ebp,esp
        pushad                      // Preserve all registers

        mov     ecx,[ebp+0x10]      // func
        push    ecx
        mov     edx,[ebp+0x0C]      // clientData
        push    edx
        mov     eax,[ebp+0x08]      // functionID
        push    eax
        call    FunctionTailcallGlobal
 
        popad                       // Restore all registers
        pop     ebp                 // Restore EBP
        ret     12
    }
}

// ----  MAPPING FUNCTIONS ------------------

// this function is called by the CLR when a function has been mapped to an ID
UINT_PTR CProfiler::FunctionMapper(FunctionID functionID, BOOL *pbHookFunction)
{
	// make sure the global reference to our profiler is valid.  Forward this
	// call to our profiler object
    if (g_pICorProfilerCallback != NULL)
        g_pICorProfilerCallback->MapFunction(functionID);

	// we must return the function ID passed as a parameter
	return (UINT_PTR)functionID;
}

// the static function called by .Net when a function has been mapped to an ID
void CProfiler::MapFunction(FunctionID functionID)
{
	// see if this function is in the map
	CFunctionInfo* functionInfo = NULL;
	std::map<FunctionID, CFunctionInfo*>::iterator iter = m_functionMap.find(functionID);
	if (iter == m_functionMap.end())
	{
		// declared in this block so they are not created if the function is found
		WCHAR szMethodName[NAME_BUFFER_SIZE];
		const WCHAR* p = NULL;
		USES_CONVERSION;

		// get the method name
		HRESULT hr = GetFullMethodName(functionID, szMethodName, NAME_BUFFER_SIZE); 
		if (FAILED(hr))
		{
			// if we couldn't get the function name, then log it
			LogString("Unable to find the name for %i\r\n", functionID);
			return;
		}
		// add it to the map
		functionInfo = new CFunctionInfo(functionID, W2A(szMethodName));
		m_functionMap.insert(std::pair<FunctionID, CFunctionInfo*>(functionID, functionInfo));
	}
}

// ----  CALLBACK HANDLER FUNCTIONS ------------------

// our real handler for FunctionEnter notification
void CProfiler::Enter(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo, COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo)
{
	// see if this function is in the map.  It should be since we are using the funciton mapper
	CFunctionInfo* functionInfo = NULL;
	std::map<FunctionID, CFunctionInfo*>::iterator iter = m_functionMap.find(functionID);
	
	if (iter != m_functionMap.end())
	{
		// get it from the map and update it
		functionInfo = (iter->second);
		// increment the call count
		
		
		functionInfo->IncrementCallCount();
		// create the padding based on the call stack size
		int padCharCount = m_callStackSize * 2;
		if (padCharCount > 0)
		{
			char* padding = new char[(padCharCount) + 1];
			memset(padding, 0, padCharCount + 1);
			memset(padding, ' ', padCharCount);
			// log the function call
			//LogString("%s%s, id=%d, call count = %d\r\n", padding, functionInfo->GetName(), functionInfo->GetFunctionID(), functionInfo->GetCallCount());
			delete padding;
		}
		else
		{
			// log the function call
			//LogString("%s, id=%d, call count = %d\r\n", functionInfo->GetName(), functionInfo->GetFunctionID(), functionInfo->GetCallCount());
		}
	}
	else
	{
		// log an error (this shouldn't happen because we're caching the functions
		// in the function mapping callback
		LogString("Error finding function ID %d in the map.\r\n", (int)functionID);
	}
	// increment the call stack size (we must do this even if we don't find the 
	// function in the map
	m_callStackSize++;
}

// our real handler for FunctionLeave notification
void CProfiler::Leave(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo, COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange)
{
	CFunctionInfo* functionInfo = NULL;
	std::map<FunctionID, CFunctionInfo*>::iterator iter = m_functionMap.find(functionID);
	
	if (iter != m_functionMap.end())
	{
		// get it from the map and update it
		functionInfo = (iter->second);

		/*size_t pRetValueLength =0;
		char pRetValue[100000];
		if(TryGetCommandText("get_CommandText",functionInfo, argumentRange, pRetValue, &pRetValueLength)){
			LogString("SqlCommand Length=%d; CommandText=%s",pRetValueLength, pRetValue);
			PrintFunctionArguments(functionID, frameInfo, argumentRange);
		}

		pRetValueLength = 0;
		memset(pRetValue,0, sizeof(pRetValue));
		if(TryGetParameter("get_ParameterName",functionInfo, argumentRange, pRetValue, &pRetValueLength)){
			LogString("ParameterName Length=%d; Name=%s",pRetValueLength, pRetValue);
			PrintFunctionArguments(functionID, frameInfo, argumentRange);
		}
		
		pRetValueLength = 0;
		memset(pRetValue,0, sizeof(pRetValue));
		if(TryGetParameter("System.Data.SqlClient.SqlParameter.get_Value",functionInfo, argumentRange, pRetValue, &pRetValueLength)){
			LogString("System.Data.SqlClient.SqlParameter.get_Value.ParameterName Length=%d; Name=%s",pRetValueLength, pRetValue);
			
		}*/

		std::string fNameRef = "TestString";
		std::string functionName = functionInfo->GetName();
		if(functionName.find(fNameRef)!=std::string::npos){
			INT32 returnTypeAsEnum;
			functionInfo->SetReturnTypeName(GetReturnTypeName(functionID, argumentRange, &returnTypeAsEnum));
			LogString("ReturnTypeName=%s, CoreElementType=%d",functionInfo->GetReturnTypeName().c_str(), returnTypeAsEnum);	
			switch(returnTypeAsEnum){
				case CorElementType::ELEMENT_TYPE_I4:{
						INT32 value = GetInt32ReturnValue(argumentRange);
						LogString("ReturnValue=%d",value);	
						break;
					}
					
				case CorElementType::ELEMENT_TYPE_R8:
					{
						DOUBLE value = GetDoubleReturnValue(argumentRange);
						LogString("ReturnValueDouble=%f",value);	
						break;
					}

				case CorElementType::ELEMENT_TYPE_R4:{
						FLOAT value = GetFloatReturnValue(argumentRange);
						LogString("ReturnValueFloat=%f",value);	
						break;
					}
				case CorElementType::ELEMENT_TYPE_STRING:
					{
						string value = GetStringReturnValue(argumentRange);
						LogString("ReturnValueString=%s",value.c_str());
						break;
					}
			}
			//GetDateTimeReturnValue(argumentRange);
		}
	}
	// decrement the call stack size
	if (m_callStackSize > 0)
		m_callStackSize--;
}

DOUBLE CProfiler::GetDoubleReturnValue(const COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange){
	DOUBLE value;
	memcpy(&value,(void*)argumentRange->startAddress,argumentRange->length);
	return value;
}

FLOAT CProfiler::GetFloatReturnValue(const COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange){
	FLOAT value;
	memcpy(&value,(void*)argumentRange->startAddress,argumentRange->length);
	return value;
}

INT32 CProfiler::GetInt32ReturnValue(const COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange){
	INT32 value;
	memcpy(&value,(void*)argumentRange->startAddress,argumentRange->length);
	return value;
}

std::string CProfiler::GetReturnTypeName(const FunctionID functionId, const COR_PRF_FUNCTION_ARGUMENT_RANGE *range, INT32 *pCoreElementType){
	
	std::string returnTypeName;
	IMetaDataImport * pIMetaDataImport = 0;
	mdToken funcToken = 0;
	HRESULT hr;
	hr = m_pICorProfilerInfo->GetTokenAndMetaDataFromFunction(functionId, IID_IMetaDataImport,(LPUNKNOWN *) &pIMetaDataImport,&funcToken);
	if(SUCCEEDED(hr)){
		mdTypeDef memberClass;
		DWORD dwAttr;
		PCCOR_SIGNATURE pMethodSigBlob;
		ULONG cbMethodSigBlob;
		ULONG ulCodeRVA = 0;
		DWORD dwImplFlags = 0;

		hr = pIMetaDataImport->GetMethodProps (funcToken, &memberClass, g_szMethodName, NAME_BUFFER_SIZE, NULL, &dwAttr,
												&pMethodSigBlob, &cbMethodSigBlob, &ulCodeRVA, &dwImplFlags);
	
		if(SUCCEEDED(hr)){

			WCHAR buffer[NAME_BUFFER_SIZE];
			ULONG callConvention;
			ULONG argumentCount = 0;
			LPWSTR returnTypeStr = new WCHAR[NAME_BUFFER_SIZE]; 
			returnTypeStr[0] = NULL;
			WCHAR functionParameters [NAME_BUFFER_SIZE];
	
			pMethodSigBlob += CorSigUncompressData( pMethodSigBlob, &callConvention);
			if ( callConvention != IMAGE_CEE_CS_CALLCONV_FIELD) {
					pMethodSigBlob += CorSigUncompressData(pMethodSigBlob,&argumentCount);
					pMethodSigBlob = ParseElementType( pIMetaDataImport, pMethodSigBlob, returnTypeStr, pCoreElementType);	
					if ( returnTypeStr[0] == '\0' ) swprintf( returnTypeStr,L"%s","void");
					char pRetValue[MAX_CLASSNAME_LENGTH];
					size_t pRetValueLength;
					wcstombs_s(&pRetValueLength,pRetValue,wcslen(returnTypeStr)+1,returnTypeStr,_TRUNCATE);
					returnTypeName =  string(pRetValue);
			}
	
			pIMetaDataImport->Release();
			return returnTypeName;
		}
		else{
			LogString("Error GetMethodProps");
		}
	}else LogString("Error GetTokenAndMetaDataFromFunction");
}


PCCOR_SIGNATURE CProfiler::ParseElementType( IMetaDataImport *metaDataImport, PCCOR_SIGNATURE signature, LPWSTR signatureText, INT32 *pElementType)
{	
	COR_SIGNATURE corSignature = *signature++;
	
	*pElementType = corSignature;
	switch (corSignature) 
	{	
	case ELEMENT_TYPE_VOID:
		wcscat(signatureText, L"void");
		break;							
	case ELEMENT_TYPE_BOOLEAN:	
		wcscat(signatureText, L"bool");	
		break;			
	case ELEMENT_TYPE_CHAR:
		wcscat(signatureText, L"wchar");	
		break;							
	case ELEMENT_TYPE_I1:
		wcscat(signatureText, L"int8");	
		break;		 		
	case ELEMENT_TYPE_U1:
		wcscat(signatureText, L"unsigned int8");	
		break;	 		
	case ELEMENT_TYPE_I2:
		wcscat(signatureText, L"int16");	
		break;		
	case ELEMENT_TYPE_U2:
		wcscat(signatureText, L"unsigned int16");	
		break;					
	case ELEMENT_TYPE_I4:
		wcscat(signatureText, L"int32");	
		break;        
	case ELEMENT_TYPE_U4:
		wcscat(signatureText, L"unsigned int32");	
		break;				
	case ELEMENT_TYPE_I8:
		wcscat(signatureText, L"int64");	
		break;				
	case ELEMENT_TYPE_U8:
		wcscat(signatureText, L"unsigned int64");	
		break;		
	case ELEMENT_TYPE_R4:
		wcscat(signatureText, L"float32");	
		break;					
	case ELEMENT_TYPE_R8:
		wcscat(signatureText, L"float64");	
		break;			 		
	case ELEMENT_TYPE_STRING:
		wcscat(signatureText, L"String");	
		break;	
	case ELEMENT_TYPE_VAR:
		wcscat(signatureText, L"class variable(unsigned int8)");	
		break;		
	case ELEMENT_TYPE_MVAR:
		wcscat(signatureText, L"method variable(unsigned int8)");	
		break;					         
	case ELEMENT_TYPE_TYPEDBYREF:
		wcscat(signatureText, L"refany");	
		break;		 		
	case ELEMENT_TYPE_I:
		wcscat(signatureText, L"int");	
		break;			
	case ELEMENT_TYPE_U:
		wcscat(signatureText, L"unsigned int");	
		break;			  		
	case ELEMENT_TYPE_OBJECT:
		wcscat(signatureText, L"Object");	
		break;		       
	case ELEMENT_TYPE_SZARRAY:	 
		signature = ParseElementType(metaDataImport, signature, signatureText, pElementType); 
		wcscat(signatureText, L"[]");
		break;				
	case ELEMENT_TYPE_PINNED:
		signature = ParseElementType(metaDataImport, signature, signatureText, pElementType); 
		wcscat(signatureText, L"pinned");	
		break;	        
	case ELEMENT_TYPE_PTR:   
		signature = ParseElementType(metaDataImport, signature, signatureText, pElementType); 
		wcscat(signatureText, L"*");	
		break;           
	case ELEMENT_TYPE_BYREF:   
		signature = ParseElementType(metaDataImport, signature, signatureText, pElementType); 
		wcscat(signatureText, L"&");	
		break;
	case ELEMENT_TYPE_VALUETYPE:   
	case ELEMENT_TYPE_CLASS:	
	case ELEMENT_TYPE_CMOD_REQD:
	case ELEMENT_TYPE_CMOD_OPT:
		{
			mdToken	token;	
			signature += CorSigUncompressToken( signature, &token ); 

			WCHAR className[ NAME_BUFFER_SIZE ];
			if ( TypeFromToken( token ) == mdtTypeRef )
			{
				metaDataImport->GetTypeRefProps(token, NULL, className, NAME_BUFFER_SIZE, NULL);
			}
			else
			{
				metaDataImport->GetTypeDefProps(token, className, NAME_BUFFER_SIZE, NULL, NULL, NULL );
			}

			wcscat(signatureText, className );
		}
		break;		
	case ELEMENT_TYPE_GENERICINST:
		{
			signature = ParseElementType(metaDataImport, signature, signatureText, pElementType); 

			wcscat(signatureText, L"<");	
			ULONG arguments = CorSigUncompressData(signature);
			for (ULONG i = 0; i < arguments; ++i)
			{
				if(i != 0)
				{
					wcscat(signatureText, L", ");	
				}

				signature = ParseElementType(metaDataImport, signature, signatureText, pElementType);
			}
			wcscat(signatureText, L">");	
		}
		break;		        
	case ELEMENT_TYPE_ARRAY:	
		{
			signature = ParseElementType(metaDataImport, signature, signatureText, pElementType);              
			ULONG rank = CorSigUncompressData(signature);													
			if ( rank == 0 ) 
			{
				wcscat(signatureText, L"[?]");
			}
			else 
			{		
				ULONG arraysize = (sizeof(ULONG) * 2 * rank);
				ULONG *lower = (ULONG *)_alloca(arraysize);
				memset(lower, 0, arraysize); 
				ULONG *sizes = &lower[rank];

				ULONG numsizes = CorSigUncompressData(signature);	
				for (ULONG i = 0; i < numsizes && i < rank; i++)
				{
					sizes[i] = CorSigUncompressData(signature);	
				}

				ULONG numlower = CorSigUncompressData(signature);	
				for (ULONG i = 0; i < numlower && i < rank; i++)	
				{
					lower[i] = CorSigUncompressData( signature ); 
				}

				wcscat(signatureText, L"[");	
				for (ULONG i = 0; i < rank; ++i)	
				{					
					if (i > 0) 
					{
						wcscat(signatureText, L",");
					}

					if (lower[i] == 0)
					{
						if(sizes[i] != 0)
						{
							WCHAR *size = new WCHAR[NAME_BUFFER_SIZE];
							size[0] = '\0';
							wsprintf(size, L"%d", sizes[i]);											
							wcscat(signatureText, size);
						}
					}	
					else	
					{
						WCHAR *low = new WCHAR[NAME_BUFFER_SIZE];
						low[0] = '\0';
						wsprintf(low, L"%d", lower[i]);
						wcscat(signatureText, low);
						wcscat(signatureText, L"...");	

						if (sizes[i] != 0)	
						{
							WCHAR *size = new WCHAR[NAME_BUFFER_SIZE];
							size[0] = '\0';
							wsprintf(size, L"%d", (lower[i] + sizes[i] + 1));
							wcscat(signatureText, size);
						}
					}
				}
				wcscat(signatureText, L"]");
			}
		} 
		break;   		    
	default:	
	case ELEMENT_TYPE_END:	
	case ELEMENT_TYPE_SENTINEL:	
		WCHAR *elementType = new WCHAR[NAME_BUFFER_SIZE];
		elementType[0] = '\0';
		wsprintf(elementType, L"<UNKNOWN:0x%X>", corSignature);
		wcscat(signatureText, elementType);
		break;				                      				            
	}

	return signature;
}
bool CProfiler::TryGetParameterValue(char *pFilterFunctionName, CFunctionInfo *pFunctionInfo, COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange,void * pRetValue, size_t *pRetValueLength){
	std::string fNameRef = pFilterFunctionName;
	std::string functionName = pFunctionInfo->GetName();
	if(functionName.find(fNameRef)!=std::string::npos){
		//GetStringReturnValue(argumentRange,pRetValue,pRetValueLength);
		//PrintParameterValue(1,ELEMENT_TYPE_GENERICINST,argumentRange->startAddress);
	}
	return *pRetValueLength>0;
}

bool CProfiler::TryGetParameter(char *pFilterFunctionName, CFunctionInfo *pFunctionInfo, COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange,char * pRetValue, size_t *pRetValueLength){
	
	std::string fNameRef = pFilterFunctionName;
	std::string functionName = pFunctionInfo->GetName();
	if(functionName.find(fNameRef)!=std::string::npos){
		GetStringReturnValue(argumentRange);
	}
	return *pRetValueLength>0;
}

bool CProfiler::TryGetCommandText(char *pFilterFunctionName, CFunctionInfo *pFunctionInfo, COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange,char * pRetValue, size_t *pRetValueLength){
	std::string fNameRef = pFilterFunctionName;
	std::string functionName = pFunctionInfo->GetName();
	if(functionName.find(fNameRef)!=std::string::npos){
		GetStringReturnValue(argumentRange);
	}
	return *pRetValueLength>0;
}

string CProfiler::GetStringReturnValue(COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange){
	if(argumentRange== NULL) return NULL;
	string returnValue;
	ULONG pBufferLengthOffset;
	ULONG pStringLengthOffset;
	ULONG pBufferOffset;
	m_pICorProfilerInfo2->GetStringLayout(&pBufferLengthOffset, &pStringLengthOffset, &pBufferOffset);
	ObjectID stringOID;
	memcpy(&stringOID,((const void*)(argumentRange->startAddress)),argumentRange->length);
	DWORD stringLength;
	memcpy(&stringLength,((const void*)(stringOID+pStringLengthOffset)),sizeof(DWORD));

	WCHAR tempParameterValue[100000];
	memcpy(tempParameterValue,((const void*)(stringOID+pBufferOffset)),stringLength * sizeof(DWORD));
	tempParameterValue[stringLength*sizeof(DWORD)]= '\0';
	//convert WCHAR to char
	char narrowTempParameterValue[100000];
	UINT narrowTempParameterValueLength=0;
	wcstombs_s(&narrowTempParameterValueLength,narrowTempParameterValue,wcslen(tempParameterValue)+1,tempParameterValue,_TRUNCATE);
	returnValue = narrowTempParameterValue;
	return returnValue;
}


void CProfiler::GetDateTimeReturnValue(COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange){
	if(argumentRange== NULL) return;
	
	ClassID classID=0;
	HRESULT hr;
	INT64 objectID;

	objectID=	*(INT64*)(argumentRange->startAddress);
	objectID = objectID & 0x3FFFFFFFFFFFFFFF;
	LogString("Startaddress=%d, length=%d, Value=%I64d",argumentRange->startAddress,argumentRange->length, objectID);
	/*hr = m_pICorProfilerInfo2->GetClassFromObject(objectID, &classID);
	if(FAILED(hr)) goto _ERROR;
	LogString("GetClassFromObject");

	ModuleID moduleId;
	mdTypeDef mdTokenId;
	IMetaDataImport *pIMetaDataImport = 0;
	hr = m_pICorProfilerInfo->GetClassIDInfo(classID,&moduleId,&mdTokenId);
	LogString("GetClassIDInfo");

	if(FAILED(hr)) goto _ERROR;
	

	hr= m_pICorProfilerInfo->GetModuleMetaData(moduleId,ofRead, IID_IMetaDataImport, (LPUNKNOWN *) &pIMetaDataImport);
	if(FAILED(hr)) goto _ERROR;
	LogString("GetModuleMetaData");

	LPWSTR szClass = new WCHAR[MAX_CLASSNAME_LENGTH];
	ULONG cchClass;
	IfFailGoto(pIMetaDataImport->GetTypeDefProps(mdTokenId, szClass, MAX_CLASSNAME_LENGTH, &cchClass, 0, 0), _ERROR);
	LogString("GetTypeDefProps");
	
	size_t pRetValueLength;
	char pRetValue[MAX_CLASSNAME_LENGTH];
	wcstombs_s(&pRetValueLength,pRetValue,wcslen(szClass)+1,szClass,_TRUNCATE);
	LogString("Length=%d, Name=%s",cchClass,pRetValue);
	delete szClass;
	
	pIMetaDataImport->Release();
	goto _END;

	_ERROR:
		LogString("Error");
	_END:*/

	return;
}



WCHAR* CProfiler::DecodeString(PBYTE pObjectByte)
{
	HRESULT hr = E_FAIL;
	ULONG offsBufferLength = 0;
	ULONG offsStringLength = 0;
	ULONG offsStringBuffer = 0;
	WCHAR* pString = NULL;

	hr = m_pICorProfilerInfo2->GetStringLayout(&offsBufferLength,
											&offsStringLength,
											&offsStringBuffer);
	if(SUCCEEDED(hr))
	{
		// treating all parameters as objects for now...
		// obviously would need to rework for intrinsics
		if(pObjectByte != NULL)
		{
			// get the string information based on the offsets
			PBYTE pObjectString = pObjectByte+offsStringBuffer;
			pString = (WCHAR*)pObjectString;
			ULONG* buflen = (ULONG*)(pObjectByte+ offsBufferLength);
			ULONG* strlength = (ULONG*)(pObjectByte+ offsStringLength);
		}
	}
	return pString;
}


//void CProfiler::PrintParameterValue(int index,CorElementType type,PBYTE pValues)
//{
//	HRESULT hr = 0;
//
//	// get our value pointer
//	PVOID* pValueVoid = (PVOID*)(pValues + (index*4));
//	PVOID pObjectVoid = NULL;
//	PBYTE pObjectByte = NULL;
//	ObjectID objectId = 0;
//	ClassID classId = 0;
//	if(pValueVoid == NULL)
//	{
//		LogString("\t\t\tParameter[%d] had a NULL value\r\n",index);
//		return;
//	}
//	if(type == ELEMENT_TYPE_VAR)
//	{
//		LogString("\t\t\tParameter[%d] was an ELEMENT_TYPE_VAR.  Value not retrieved.\r\n",index);
//		return;
//	}
//	if(type == ELEMENT_TYPE_VOID)
//	{
//		LogString("\t\t\tParameter[%d] was an ELEMENT_TYPE_VOID.  No value to retrieve.\r\n",index);
//		return;
//	}
//
//	// get the object value or pointer
//	pObjectVoid = *pValueVoid;
//	if(IsReferenceType(type))
//	{
//		// get the object ID / pointer by derefing since it's a ref type thing
//		pObjectByte = (PBYTE)pObjectVoid;
//		objectId = (ObjectID)pObjectVoid; 
//		hr = m_pICorProfilerInfo2->GetClassFromObject(objectId,&classId);
//		if(FAILED(hr))
//		{
//			LogString("!!!! Failed to get class from objectID (0x%08X), param[%d]\r\n",objectId,index); 
//			return;
//		}
//	}
	
	//WCHAR* pString = NULL;
	//CorElementType baseElementType = ELEMENT_TYPE_VOID;
	//ClassID baseClassId = 0;
	//ULONG cRank = 0;
	//ULONG numElements = 0;
	//ULONG32* pDimensionSizes = NULL;
	//int* pDimensionLowerBounds = NULL;
	//BYTE* pArrayData;
	//char typeString[128];
	//typeString[0]=0x00;

	//ULONG32 offsBoxBuffer = 0;

	//switch(type)
	//{
	//	case ELEMENT_TYPE_I4:
	//		if(pObjectVoid != NULL)
	//			LogString("\t\t\tParameter[%d] Value (ELEMENT_TYPE_I4) == %d\r\n",index,pObjectVoid); 
	//		else
	//			LogString("\t\t\tParameter[%d] Value (ELEMENT_TYPE_I4) == No value accessible\r\n",index); 
	//		break;
	//	case ELEMENT_TYPE_STRING:
	//		pString = DecodeString(pObjectByte);
	//		// if we got the string value print it out
	//		if(pString != NULL)
	//			LogString("\t\t\tParameter[%d] Value (ELEMENT_TYPE_STRING) == %S\r\n",index,pString); 
	//		else
	//			LogString("\t\t\tParameter[%d] Value (ELEMENT_TYPE_STRING) == No value accessible\r\n",index); 
	//		break;

	//	case ELEMENT_TYPE_SZARRAY:
	//		hr = m_pICorProfilerInfo2->IsArrayClass(classId,&baseElementType,&baseClassId,&cRank);
	//		if((SUCCEEDED(hr))&&(S_FALSE != hr))
	//		{

	//			GetElementTypeString(baseElementType,typeString);
	//			ClassName(baseClassId,g_szName);
	//			if((baseElementType == ELEMENT_TYPE_CLASS)&&
	//				(wcscmp(g_szName,L"System.String")==0))
	//			{
	//				baseElementType = ELEMENT_TYPE_STRING;
	//			}
	//			LogString("\t\t\tArray Element Type: %s; Array Element Class Name: %S; Rank %ld\r\n",typeString,g_szName,cRank);
	//			// found an array
	//			pDimensionSizes = new ULONG32[cRank];
	//			pDimensionLowerBounds = new int[cRank];
	//			hr = m_pICorProfilerInfo2->GetArrayObjectInfo(objectId,
	//														cRank,
	//														pDimensionSizes,
	//														pDimensionLowerBounds,
	//														&pArrayData);
	//			if(SUCCEEDED(hr))
	//			{
	//				if((pDimensionSizes != NULL) && (pDimensionSizes[0]>0))
	//				{
	//					LogString("\t\t\tParameter[%d] (ELEMENT_TYPE_SZARRAY)\r\n",index); 
	//					if(baseElementType == ELEMENT_TYPE_STRING)
	//					{
	//						pString = DecodeString((pArrayData + (index * 4)));
	//						// if we got the string value print it out
	//						if(pString != NULL)
	//							LogString("\t\t\tParameter[%d] Value (ELEMENT_TYPE_STRING) == %S\r\n",index,pString); 
	//						else
	//							LogString("\t\t\tParameter[%d] Value (ELEMENT_TYPE_STRING) == No value accessible\r\n",index); 
	//						break;
	//					}
	//					else if (baseClassId == m_MyClassID)
	//					{
	//						DumpArrayInfo (baseClassId,
	//									   cRank,
	//									   pDimensionSizes,
	//									   pDimensionLowerBounds,
	//									   pArrayData);

	//					}
	//				}
	//			}

	//			delete [] pDimensionSizes;
	//			delete [] pDimensionLowerBounds;
	//		}
	//		break;
	//	case ELEMENT_TYPE_OBJECT:
	//		// See if any boxing is going on
	//		hr = m_pICorProfilerInfo2->GetBoxClassLayout(classId,&offsBoxBuffer);
	//		if(SUCCEEDED(hr))
	//		{
	//			pObjectByte += offsBoxBuffer;
	//			char value[48];
	//			value[0]=0x00;
	//			sprintf_s(value,48,"%d",*pObjectByte);
	//			// Assuming 4 byte item here...
	//			ClassName(classId,g_szName);
	//			LogString("\t\t\tParameter[%d] Value (Boxed Value was %S) == %s\r\n",index,g_szName,value); 
	//		}
	//		else
	//		{
	//			LogString("\t\t\tParameter[%d] Value (ELEMENT_TYPE_OBJECT)== \r\n",index); 
	//		}
	//		break;
	//	default:
	//		LogString("\t\t\tParameter[%d] Value == NOTIMPL\r\n",index); 
	//		break;
	//}
//}


//
// Print out the CorElementTypes for the arguments to this function
//
//



void CProfiler::GetUnspecifiedReturnValue(COR_PRF_FUNCTION_ARGUMENT_RANGE *argumentRange, void *pRetValue, size_t* pRetValueLength){
	//if(argumentRange== NULL) return;
	//ObjectID stringOID;
	//memcpy(&stringOID,((const void*)(argumentRange->startAddress)),argumentRange->length);
	//
	//DWORD stringLength;
	//memcpy(&stringLength,((const void*)(stringOID+pStringLengthOffset)),sizeof(DWORD));

	//WCHAR tempParameterValue[100000];
	//memcpy(tempParameterValue,((const void*)(stringOID+pBufferOffset)),stringLength * sizeof(DWORD));
	////convert WCHAR to char
	return;
}

// our real handler for the FunctionTailcall notification
void CProfiler::Tailcall(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo)
{
	// decrement the call stack size
	if (m_callStackSize > 0)
		m_callStackSize--;
}

// ----  ICorProfilerCallback IMPLEMENTATION ------------------

// called when the profiling object is created by the CLR
STDMETHODIMP CProfiler::Initialize(IUnknown *pICorProfilerInfoUnk)
{
	// set up our global access pointer
	g_pICorProfilerCallback = this;

	// log that we are initializing
	LogString("Initializing...\r\n\r\n");

	// get the ICorProfilerInfo interface
    HRESULT hr = pICorProfilerInfoUnk->QueryInterface(IID_ICorProfilerInfo, (LPVOID*)&m_pICorProfilerInfo);
    if (FAILED(hr))
        return E_FAIL;
	// determine if this object implements ICorProfilerInfo2
    hr = pICorProfilerInfoUnk->QueryInterface(IID_ICorProfilerInfo2, (LPVOID*)&m_pICorProfilerInfo2);
    if (FAILED(hr))
	{
		// we still want to work if this call fails, might be an older .NET version
		m_pICorProfilerInfo2.p = NULL;
	}

	// Indicate which events we're interested in.
	hr = SetEventMask();
    if (FAILED(hr))
        LogString("Error setting the event mask\r\n\r\n");

	// set the enter, leave and tailcall hooks
	if (m_pICorProfilerInfo2.p == NULL)
	{
		// note that we are casting our functions to the definitions for the callbacks
		
		hr = m_pICorProfilerInfo->SetEnterLeaveFunctionHooks((FunctionEnter*)&FunctionEnterNaked, (FunctionLeave*)&FunctionLeaveNaked, (FunctionTailcall*)&FunctionTailcallNaked);
		if (SUCCEEDED(hr))
			hr = m_pICorProfilerInfo->SetFunctionIDMapper((FunctionIDMapper*)&FunctionMapper);
	}
	else
	{
		hr = m_pICorProfilerInfo2->SetEnterLeaveFunctionHooks2((FunctionEnter2*)&FunctionEnterNaked, (FunctionLeave2*)&FunctionLeaveNaked, (FunctionTailcall2*)&FunctionTailcallNaked);
		
		if (SUCCEEDED(hr))
			hr = m_pICorProfilerInfo2->SetFunctionIDMapper(FunctionMapper);
		
	}
	// report our success or failure to the log file
    if (FAILED(hr))
        LogString("Error setting the enter, leave and tailcall hooks\r\n\r\n");
	else
		LogString("Successfully initialized profiling\r\n\r\n" );

    return S_OK;
}

// called when the profiler is being terminated by the CLR
STDMETHODIMP CProfiler::Shutdown()
{
	// log the we're shutting down
	LogString("\r\n\r\nShutdown... writing function list\r\n\r\n" );

	// write the function names and call counts to the output file
	std::map<FunctionID, CFunctionInfo*>::iterator iter;
	for (iter = m_functionMap.begin(); iter != m_functionMap.end(); iter++)
	{
		// log the function's info
		CFunctionInfo* functionInfo = iter->second;
		LogString("%s : call count = %d\r\n", functionInfo->GetName(), functionInfo->GetCallCount());
		// free the memory for the object
		delete iter->second;
	}
	// clear the map
	m_functionMap.clear();

	// tear down our global access pointers
	g_pICorProfilerCallback = NULL;

    return S_OK;
}

// Creates the log file.  It uses the LOG_FILENAME environment variable if it 
// exists, otherwise it creates the file "ICorProfilerCallback Log.log" in the 
// executing directory.  This function doesn't report success or not because 
// LogString always checks for a valid file handle whenever the file is written
// to.
void CProfiler::CreateLogFile()
{
	// get the log filename
	memset(m_logFileName, 0, sizeof(m_logFileName));
	// get the log file name (stored in an environment var)
	if (GetEnvironmentVariable(_T("LOG_FILENAME"), m_logFileName, _MAX_PATH) == 0)
	{
		// just write to "ICorProfilerCallback Log.log"
		_tcscpy(m_logFileName, _T("ICorProfilerCallback Log.log"));
	}
	// delete any existing log file
	::DeleteFile(m_logFileName);
	// set up log file in the current working directory
	m_hLogFile = CreateFile(m_logFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

// Closes the log file
void CProfiler::CloseLogFile()
{
	// close the log file
	if (m_hLogFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hLogFile);
		m_hLogFile = INVALID_HANDLE_VALUE;
	}
}

// Writes a string to the log file.  Uses the same calling convention as printf.
void CProfiler::LogString(char *pszFmtString, ...)
{
	CHAR szBuffer[4096]; 
	DWORD dwWritten = 0;

	if(m_hLogFile != INVALID_HANDLE_VALUE)
	{
		va_list args;
		va_start( args, pszFmtString );
		vsprintf(szBuffer, pszFmtString, args );
		va_end( args );

		// write out to the file if the file is open
		WriteFile(m_hLogFile, szBuffer, (DWORD)strlen(szBuffer), &dwWritten, NULL);
	}
}




///<summary>
// We are monitoring events that are interesting for determining
// the hot spots of a managed CLR program (profilee). This includes
// thread related events, function enter/leave events, exception 
// related events, and unmanaged/managed transition events. Note 
// that we disable inlining. Although this does indeed affect the 
// execution time, it provides better accuracy for determining
// hot spots.
//
// If the system does not support high precision counters, then
// do not profile anything. This is determined in the constructor.
///</summary>
HRESULT CProfiler::SetEventMask()
{
	//COR_PRF_MONITOR_NONE	= 0,
	//COR_PRF_MONITOR_FUNCTION_UNLOADS	= 0x1,
	//COR_PRF_MONITOR_CLASS_LOADS	= 0x2,
	//COR_PRF_MONITOR_MODULE_LOADS	= 0x4,
	//COR_PRF_MONITOR_ASSEMBLY_LOADS	= 0x8,
	//COR_PRF_MONITOR_APPDOMAIN_LOADS	= 0x10,
	//COR_PRF_MONITOR_JIT_COMPILATION	= 0x20,
	//COR_PRF_MONITOR_EXCEPTIONS	= 0x40,
	//COR_PRF_MONITOR_GC	= 0x80,
	//COR_PRF_MONITOR_OBJECT_ALLOCATED	= 0x100,
	//COR_PRF_MONITOR_THREADS	= 0x200,
	//COR_PRF_MONITOR_REMOTING	= 0x400,
	//COR_PRF_MONITOR_CODE_TRANSITIONS	= 0x800,
	//COR_PRF_MONITOR_ENTERLEAVE	= 0x1000,
	//COR_PRF_MONITOR_CCW	= 0x2000,
	//COR_PRF_MONITOR_REMOTING_COOKIE	= 0x4000 | COR_PRF_MONITOR_REMOTING,
	//COR_PRF_MONITOR_REMOTING_ASYNC	= 0x8000 | COR_PRF_MONITOR_REMOTING,
	//COR_PRF_MONITOR_SUSPENDS	= 0x10000,
	//COR_PRF_MONITOR_CACHE_SEARCHES	= 0x20000,
	//COR_PRF_MONITOR_CLR_EXCEPTIONS	= 0x1000000,
	//COR_PRF_MONITOR_ALL	= 0x107ffff,
	//COR_PRF_ENABLE_REJIT	= 0x40000,
	//COR_PRF_ENABLE_INPROC_DEBUGGING	= 0x80000,
	//COR_PRF_ENABLE_JIT_MAPS	= 0x100000,
	//COR_PRF_DISABLE_INLINING	= 0x200000,
	//COR_PRF_DISABLE_OPTIMIZATIONS	= 0x400000,
	//COR_PRF_ENABLE_OBJECT_ALLOCATED	= 0x800000,
	// New in VS2005
	//	COR_PRF_ENABLE_FUNCTION_ARGS	= 0x2000000,
	//	COR_PRF_ENABLE_FUNCTION_RETVAL	= 0x4000000,
	//  COR_PRF_ENABLE_FRAME_INFO	= 0x8000000,
	//  COR_PRF_ENABLE_STACK_SNAPSHOT	= 0x10000000,
	//  COR_PRF_USE_PROFILE_IMAGES	= 0x20000000,
	// End New in VS2005
	//COR_PRF_ALL	= 0x3fffffff,
	//COR_PRF_MONITOR_IMMUTABLE	= COR_PRF_MONITOR_CODE_TRANSITIONS | COR_PRF_MONITOR_REMOTING | COR_PRF_MONITOR_REMOTING_COOKIE | COR_PRF_MONITOR_REMOTING_ASYNC | COR_PRF_MONITOR_GC | COR_PRF_ENABLE_REJIT | COR_PRF_ENABLE_INPROC_DEBUGGING | COR_PRF_ENABLE_JIT_MAPS | COR_PRF_DISABLE_OPTIMIZATIONS | COR_PRF_DISABLE_INLINING | COR_PRF_ENABLE_OBJECT_ALLOCATED | COR_PRF_ENABLE_FUNCTION_ARGS | COR_PRF_ENABLE_FUNCTION_RETVAL | COR_PRF_ENABLE_FRAME_INFO | COR_PRF_ENABLE_STACK_SNAPSHOT | COR_PRF_USE_PROFILE_IMAGES

	// set the event mask 
	DWORD eventMask = (DWORD) (COR_PRF_MONITOR_ENTERLEAVE|COR_PRF_ENABLE_FUNCTION_ARGS|COR_PRF_ENABLE_FUNCTION_RETVAL);
	return m_pICorProfilerInfo->SetEventMask(eventMask);
}

// creates the fully scoped name of the method in the provided buffer
HRESULT CProfiler::GetFullMethodName(FunctionID functionID, LPWSTR wszMethod, int cMethod)
{
	IMetaDataImport* pIMetaDataImport = 0;
	HRESULT hr = S_OK;
	mdToken funcToken = 0;
	WCHAR szFunction[NAME_BUFFER_SIZE];
	WCHAR szClass[NAME_BUFFER_SIZE];

	// get the token for the function which we will use to get its name
	
	hr = m_pICorProfilerInfo->GetTokenAndMetaDataFromFunction(functionID, IID_IMetaDataImport, (LPUNKNOWN *) &pIMetaDataImport, &funcToken);
	
	if(SUCCEEDED(hr))
	{
		mdTypeDef classTypeDef;
		ULONG cchFunction;
		ULONG cchClass;
		
		// retrieve the function properties based on the token
		hr = pIMetaDataImport->GetMethodProps(funcToken, &classTypeDef, szFunction, NAME_BUFFER_SIZE, &cchFunction, 0, 0, 0, 0, 0);
		if (SUCCEEDED(hr))
		{
			// get the function name
			hr = pIMetaDataImport->GetTypeDefProps(classTypeDef, szClass, NAME_BUFFER_SIZE, &cchClass, 0, 0);
			if (SUCCEEDED(hr))
			{
				// create the fully qualified name
				_snwprintf_s(wszMethod,cMethod,cMethod,L"%s.%s",szClass,szFunction);
			}
		}
		// release our reference to the metadata
		pIMetaDataImport->Release();
	}

	return hr;
}



bool CProfiler::IsReferenceType(CorElementType type)
{
	switch(type)
	{
		case ELEMENT_TYPE_STRING://         = 0xe,
		case ELEMENT_TYPE_PTR://           = 0xf,      // PTR <type>
		case ELEMENT_TYPE_BYREF://          = 0x10,     // BYREF <type>
		case ELEMENT_TYPE_CLASS://         = 0x12,     // CLASS <class Token>
		case ELEMENT_TYPE_VAR://           = 0x13,     // a class type variable VAR <U1>
		case ELEMENT_TYPE_ARRAY://          = 0x14,     // MDARRAY <type> <rank> <bcount> <bound1> ... <lbcount> <lb1> ...
		case ELEMENT_TYPE_GENERICINST://    = 0x15,     // instantiated type
		case ELEMENT_TYPE_TYPEDBYREF://    = 0x16,     // This is a simple type.
		case ELEMENT_TYPE_FNPTR://          = 0x1B,     // FNPTR <complete sig for the function including calling convention>
		case ELEMENT_TYPE_OBJECT://         = 0x1C,     // Shortcut for System.Object
		case ELEMENT_TYPE_SZARRAY://        = 0x1D,     // Shortcut for single dimension zero lower bound array SZARRAY <type>
		case ELEMENT_TYPE_MVAR://           = 0x1e,     // a method type variable MVAR <U1>
			{
				return true;
				break;
			}
	}
	return false;
}


//
// ClassName
//
//bool CProfiler::ClassName(ClassID classId, LPWSTR wszClass )
//{
//	ClassID parentClassId;
//	ModuleID moduleId;
//	mdTypeDef typeDef;
//	bool bIsArray = false;
//	ClassID* typeArgs = NULL;
//	ULONG32 cNumTypeArgs = 16;
//
//	// init the string to blank
//	wszClass[0] = 0;
//
//	// Get the information about the class
//	HRESULT hr = E_FAIL;
//	// occasionally this call seems to fail when the classID looks ok from inside of movedreferences....
//	// Since it is possible for the __COMObject types classes (interop) to receive different classId values for the same class
//	// perhaps we are falling afoul of that since interop requires a different EE class for each application domain so the
//	// user can potentially create a different v-table for each one of them.  The profiler doesn't provide a way to get this appdomain however...
//	try
//	{
//		if(m_pICorProfilerInfo2.p == NULL)
//		{
//			hr = m_pICorProfilerInfo->GetClassIDInfo( classId, &moduleId, &typeDef );
//		}
//		else
//		{
//			typeArgs = new ClassID[cNumTypeArgs];
//			hr = m_pICorProfilerInfo2->GetClassIDInfo2( classId, 
//														&moduleId, 
//														&typeDef,
//														&parentClassId,
//														cNumTypeArgs,
//														&cNumTypeArgs,
//														typeArgs);
//		}
//	}
//	catch(...)
//	{
//		LogString("**** ICorProfilerInfo::GetClassIDInfo failed with classId 0x%08X\r\n",classId);
//			return false;
//	}
//
//	if ( FAILED(hr) || typeDef == 0)
//	{
//		ClassID baseClassId = 0;
//		CorElementType type;
//		ULONG cRank = 0;
//		// might be an array in which case we need to translate the classID to the base classID of the array
//		hr = m_pICorProfilerInfo->IsArrayClass(classId,&type,&baseClassId,&cRank);
//		if(FAILED (hr) || hr == S_FALSE)
//			return false;
//		else
//		{
//			// use the base class of the array
//			hr = m_pICorProfilerInfo->GetClassIDInfo( baseClassId, &moduleId,
//														&typeDef );
//			if(FAILED (hr))
//				return false;
//			else
//			{
//				// update the classId
//				classId = baseClassId;
//				bIsArray = true;
//			}
//		}
//	}
//	    
//
//	IMetaDataImport * pIMetaDataImport = 0;
//	// go get the information about the module this class belongs to
//	hr = m_pICorProfilerInfo->GetModuleMetaData( moduleId, ofRead,
//										IID_IMetaDataImport,
//										(LPUNKNOWN *)&pIMetaDataImport );
//	if ( FAILED(hr) )
//		return false;
//
//	if ( !pIMetaDataImport )
//		return false;
//
//	wchar_t wszTypeDef[NAME_BUFFER_SIZE];
//	DWORD cchTypeDef = sizeof(wszTypeDef)/sizeof(wszTypeDef[0]);
//	// get the properties for the typedef for the class
//	hr = pIMetaDataImport->GetTypeDefProps( typeDef, wszTypeDef, cchTypeDef,
//											&cchTypeDef, 0, 0 );
//	if ( FAILED(hr) )
//		return false;
//
//	wcscpy_s( wszClass,1024, wszTypeDef );
//
//	// get generic type param names
//	if(typeArgs != NULL)
//	{
//		if(cNumTypeArgs>0)
//			wcscat_s(wszClass,1024,L"<");
//		for(ULONG32 i=0;i<cNumTypeArgs;i++)
//		{
//			ClassName(typeArgs[i],wszTypeDef);
//			wcscat_s(wszClass,1024,wszTypeDef);
//			wcscat_s(wszClass,1024,L",");
//		}
//		if(cNumTypeArgs>0)
//		{
//			int index = wcslen(wszClass);
//			// remove last comma
//			wszClass[index-1]=0x00;
//			wcscat_s(wszClass,1024,L">");
//		}
//	}
//	// add on array identifier if was an array instance
//	if(bIsArray)
//		wcscat_s(wszClass,1024,L"[]");
//
//	delete [] typeArgs;
//	 
//	// make sure we give back the metadata import instance
//	pIMetaDataImport->Release();
//    return true;
//}


//void CProfiler::PrintFunctionArguments (FunctionID funcId,
//											    COR_PRF_FRAME_INFO func,
//											    COR_PRF_FUNCTION_ARGUMENT_RANGE* range)
//{
//	
//	IMetaDataImport * pIMetaDataImport = 0;
//	HRESULT hr = S_OK;
//	mdToken funcToken = 0;
//
//	hr = m_pICorProfilerInfo->GetTokenAndMetaDataFromFunction (funcId,
//    															IID_IMetaDataImport,
//																(LPUNKNOWN *) &pIMetaDataImport,
//																&funcToken);
//
//	if(SUCCEEDED(hr))
//	{
//		mdTypeDef memberClass;
//		DWORD dwAttr;
//		PCCOR_SIGNATURE pMethodSigBlob;
//		ULONG cbMethodSigBlob;
//		ULONG ulCodeRVA = 0;
//		DWORD dwImplFlags = 0;
//	    
//		hr = pIMetaDataImport->GetMethodProps (funcToken,
//												&memberClass,
//												g_szMethodName,
//												NAME_BUFFER_SIZE,
//												NULL,
//												&dwAttr,
//												&pMethodSigBlob, 
//												&cbMethodSigBlob,
//												&ulCodeRVA, 
//												&dwImplFlags);
//
//		// Now crack open the sig
//
//        // Get the number of parameters,skip past callconv
//        ULONG numberParams = ( *((PBYTE)pMethodSigBlob + 1) );
//		PBYTE pParamStart = (PBYTE)pMethodSigBlob + 2;
//		PBYTE pCurrent = pParamStart;
//
//		// NOTE: THIS ASSUMES ONE RANGE FOR SIMPLICITY
//		PBYTE pValues = (PBYTE)range->startAddress;
//
//        // Now loop through all of the params
//        for (ULONG i=0; i<numberParams; i++)
//        {
//			// skip over custommod
//			pCurrent++;
//
//			PCCOR_SIGNATURE pWorkingSig = (PCCOR_SIGNATURE)pCurrent;
//			// get the token type of this signature
//			CorElementType type = (CorElementType)CorSigUncompressData( pWorkingSig );
//
//			// hack for signature cracking arrays
//			if((*pWorkingSig) == 0x1d) //ELEMENT_TYPE_SZARRAY
//				type = ELEMENT_TYPE_SZARRAY;
//
//			char typeString[128];
//			typeString[0]=0x00;
//			GetElementTypeString(type,typeString);
//			if(typeString[0] != 0x00)
//			{
//				LogString("\t\t\tNumberParams[%d]\tParameter[%d] is of type (0x%08X) %s\r\n",numberParams,i,type,typeString);
//			}
//			//PrintParameterValue(i,type,pValues);
//        }
//
//
//		pIMetaDataImport->Release();
//	}
//}




