#include <fstream>
#include <string>
#include <windows.h>
#include "tinyxml2.h"

#define FILE_NAME_SIZE 32
#define BIG_BUFF_SIZE 1024
#define BUFF_SIZE 512

using namespace std;
HANDLE hLogFile = NULL;
string strFolderPath = "";
string strFileName = "";

// Translte code error to message
void GetLastErrorMessage(DWORD dwErrCode, char *szErrText)
{
	LPSTR szBuff = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dwErrCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&szBuff, 0, NULL);
	sprintf(szErrText, "\t==>Error\r\n\tCode = %d(0x%x)   Message:%s\n", dwErrCode, dwErrCode, szBuff);

	LocalFree(szBuff);
}

// Writing log-file
int WriteLog(char *szMessage)
{
	BOOL bResult = FALSE;
	DWORD dwReal;

	if (hLogFile == NULL)
	{
		hLogFile = CreateFile("Report.log", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hLogFile == INVALID_HANDLE_VALUE)
		{
			char szErrMessage[BIG_BUFF_SIZE] = { 0 };
			DWORD dwErr = GetLastError();
			GetLastErrorMessage(dwErr, szErrMessage);
			OutputDebugString(szErrMessage);
			return -1;
		}
		SetFilePointer(hLogFile, 0, NULL, FILE_END);
	}


	bResult = WriteFile(hLogFile, szMessage, strlen(szMessage), &dwReal, NULL);
	if (bResult == FALSE)
	{
		char szErrMessage[BIG_BUFF_SIZE] = { 0 };
		DWORD dwErr = GetLastError();
		GetLastErrorMessage(dwErr, szErrMessage);
		WriteLog(szErrMessage);
		return -1;
	}
	return 0;
}

// Parsing a xml-file that contains the path to the folder
int ReadPath()
{
	size_t nPos;
	int nFolderPathLength = 0;
	char szFolderPath[BUFF_SIZE] = { 0 };
	char szFullXmlFilePath[BUFF_SIZE] = { 0 };

	nFolderPathLength = GetModuleFileName(NULL, szFolderPath, BUFF_SIZE);
	if (nFolderPathLength == 0)
	{
		char szErrMessage[BIG_BUFF_SIZE] = { 0 };
		DWORD dwErr = GetLastError();
		GetLastErrorMessage(dwErr, szErrMessage);
		WriteLog(szErrMessage);
		return -1;
	}
	strFolderPath = szFolderPath;
	nPos = strFolderPath.find_last_of("\\");
	if (nPos != string::npos)
	{
		strFolderPath = strFolderPath.substr(0, nPos + 1);
	}

	strncpy_s(szFullXmlFilePath, BUFF_SIZE, strFolderPath.c_str(), strFolderPath.length());
	strncpy_s(szFullXmlFilePath + strlen(szFullXmlFilePath), BUFF_SIZE - strlen(szFullXmlFilePath), "path.xml", strlen("path.xml"));

	tinyxml2::XMLDocument pXmlDoc;
	tinyxml2::XMLError xmlErr = pXmlDoc.LoadFile(szFullXmlFilePath);
	if (xmlErr != tinyxml2::XML_SUCCESS)
	{
		WriteLog(" - Error >> path.xml not opened\r\n");
		return -1;
	}

	tinyxml2::XMLElement *pXmlElement = pXmlDoc.FirstChildElement("Folder");
	if (pXmlElement == nullptr)
	{
		WriteLog(" - Error >> FirstChild not opened\r\n");
		return -1;
	}

	if (pXmlElement != nullptr)
	{
		strFolderPath = "";
		strFolderPath = pXmlElement->GetText();
	}

	return 0;
}

// Creating full file path
int FormatFullFolderPath()
{
	char szFolder[16] = { 0 };
	char szFileName[FILE_NAME_SIZE] = { 0 };
	int nDay = 0;
	int nMonth = 0;
	SYSTEMTIME Time;
	GetSystemTime(&Time);

	// Getting yesterday date
	nMonth = Time.wMonth;
	if (Time.wDay - 1 == 0)
	{
		if (Time.wMonth - 1 == 0)
		{
			nMonth = 12;
		}
		else
		{
			nMonth = Time.wMonth - 1;
		}

		if (nMonth == 1 || nMonth == 3 || nMonth == 5 || nMonth == 7 || nMonth == 8 || nMonth == 10 || nMonth == 12)
		{
			nDay = 31;
		}
		if (nMonth == 4 || nMonth == 6 || nMonth == 9 || nMonth == 11)
		{
			nDay = 30;
		}
		if (nMonth == 2)
		{
			if (Time.wYear % 4 == 0 && Time.wYear % 100 != 0 || Time.wYear % 400 == 0)
			{
				nDay = 29;
			}
			else
			{
				nDay = 28;
			}
		}
	}
	else
	{
		nDay = Time.wDay - 1;
	}
		
	if (nDay > 9)
	{
		// Convert date to letter if needed
		sprintf_s(szFileName, FILE_NAME_SIZE, "@b201%x%c1.002", nMonth, 97 + (nDay - 10));
	}
	else
	{
		sprintf_s(szFileName, FILE_NAME_SIZE, "@b201%x%x1.002", nMonth, nDay);
	}

	sprintf_s(szFolder, 16, "%02d\\%02d%02d\\", nMonth, nDay, nMonth);

	strFileName = "";
	strFileName.append(szFileName);
	strFolderPath.append(szFolder);
	strFolderPath.append(szFileName);
	return 0;
}

// 
int ChangeLetter()
{
	string strLog = "";
	fstream parsingFile;
	size_t nPos;
	string sLine;
	int nLength = 0;

	parsingFile.open(strFolderPath, ifstream::in | ifstream::out);
	if(parsingFile.is_open())
	{
		strLog = " > Processing file : ";
		strLog.append(strFolderPath);
		strLog.append(" - ");
		while (getline(parsingFile, sLine))
		{
			nPos = sLine.find_first_of(".");
			if (nPos != string::npos)
			{
				if (sLine.at(nPos + 3) == '2')
				{
					nLength += (nPos + 5);
					parsingFile.seekg(nLength, parsingFile.beg);
					parsingFile.write("1", 1);
					strLog.append("OK!\n");
					break;
				}
				else
				{
					strLog.append("BAD!\n");
					break;
				}
			}
			nLength += sLine.length();
		};

		WriteLog((char *)strLog.c_str());
		parsingFile.close();
	}
	else
	{
		strLog.append(" - Error opening ");
		strLog.append(strFolderPath.c_str());
		strLog.append("\n");
		WriteLog((char *)strLog.c_str());
		return -1;
	}



	return 0;
}

int APIENTRY
WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	int nRet = 0;
	char szStart[BIG_BUFF_SIZE] = { 0 };
	SYSTEMTIME Time;
	GetSystemTime(&Time);
	sprintf_s(szStart, BIG_BUFF_SIZE, "Begin : %04d-%02d-%02d\n", Time.wYear, Time.wMonth, Time.wDay);
	WriteLog(szStart);
	
	nRet = ReadPath();
	if (nRet < 0)
	{
		strset(szStart, 0);
		sprintf_s(szStart, BIG_BUFF_SIZE, "End : %04d-%02d-%02d\n", Time.wYear, Time.wMonth, Time.wDay);
		WriteLog(szStart);

		CloseHandle(hLogFile);
		return nRet;
	}

	nRet = FormatFullFolderPath();
	if (nRet < 0)
	{
		strset(szStart, 0);
		sprintf_s(szStart, BIG_BUFF_SIZE, "End : %04d-%02d-%02d\n", Time.wYear, Time.wMonth, Time.wDay);
		WriteLog(szStart);

		CloseHandle(hLogFile);
		return nRet;
	}
	
	nRet = ChangeLetter();
	if(nRet < 0)
	{
		strset(szStart, 0);
		sprintf_s(szStart, BIG_BUFF_SIZE, "End : %04d-%02d-%02d\n", Time.wYear, Time.wMonth, Time.wDay);
		WriteLog(szStart);

		CloseHandle(hLogFile);
		return nRet;
	}

	strset(szStart, 0);
	sprintf_s(szStart, BIG_BUFF_SIZE, "End : %04d-%02d-%02d\n", Time.wYear, Time.wMonth, Time.wDay);
	WriteLog(szStart);

	return nRet;
}