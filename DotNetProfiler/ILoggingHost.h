#pragma once

class ILoggingHost
{
public:
    virtual int LogString(char* pszFmtString, ... ) = 0;
};
