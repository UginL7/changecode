#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <windows.h>
#include "tinyxml2.h"

#pragma warning (disable:4996)

#define FILE_NAME_SIZE 32
#define BIG_BUFF_SIZE 1024
#define BUFF_SIZE 512

using namespace std;

typedef struct _tagChange
{
	string sName;
	string sOrigValue;
	string sNewValue;
}CHANGE_DATA, *PCHANGE_DATA;

HANDLE g_hLogFile = NULL;
string g_strFolderPath = "";
map<string, PCHANGE_DATA> g_mapChangeInfo;
vector<string> vFileName;


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

	if (g_hLogFile == NULL)
	{
		g_hLogFile = CreateFile("Report.log", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (g_hLogFile == INVALID_HANDLE_VALUE)
		{
			char szErrMessage[BIG_BUFF_SIZE] = { 0 };
			DWORD dwErr = GetLastError();
			GetLastErrorMessage(dwErr, szErrMessage);
			OutputDebugString(szErrMessage);
			return -1;
		}
		SetFilePointer(g_hLogFile, 0, NULL, FILE_END);
	}

	bResult = WriteFile(g_hLogFile, szMessage, strlen(szMessage), &dwReal, NULL);
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
	g_strFolderPath = szFolderPath;
	nPos = g_strFolderPath.find_last_of("\\");
	if (nPos != string::npos)
	{
		g_strFolderPath = g_strFolderPath.substr(0, nPos + 1);
	}

	strncpy_s(szFullXmlFilePath, BUFF_SIZE, g_strFolderPath.c_str(), g_strFolderPath.length());
	strncpy_s(szFullXmlFilePath + strlen(szFullXmlFilePath), BUFF_SIZE - strlen(szFullXmlFilePath), "path.xml", strlen("path.xml"));

	tinyxml2::XMLDocument pXmlDoc;
	tinyxml2::XMLError xmlErr = pXmlDoc.LoadFile(szFullXmlFilePath);
	if (xmlErr != tinyxml2::XML_SUCCESS)
	{
		WriteLog(" - Error >> path.xml not opened\r\n");
		return -1;
	}

	// Отримання даних з файлу
	tinyxml2::XMLElement *pXmlElement = pXmlDoc.FirstChildElement("change")->FirstChildElement("folder");
	if (pXmlElement == nullptr)
	{
		WriteLog(" - Error >> Folder FirstChild not opened\r\n");
		return -1;
	}

	if (pXmlElement != nullptr)
	{
		g_strFolderPath = "";
		g_strFolderPath = pXmlElement->GetText();
	}

	pXmlElement = pXmlDoc.FirstChildElement("change")->FirstChildElement("file");
	while (pXmlElement != nullptr)
	{
		PCHANGE_DATA pChange = (PCHANGE_DATA)malloc(sizeof(CHANGE_DATA));
		if (pChange == nullptr)
		{
			WriteLog(" - Error >> Not enough memory for pSwInfo");

			return -1;
		}
		memset(pChange, 0, sizeof(CHANGE_DATA));

		pChange->sName = pXmlElement->Attribute("name");
		pChange->sOrigValue = pXmlElement->Attribute("original");
		pChange->sNewValue = pXmlElement->Attribute("new");		
		g_mapChangeInfo[pChange->sName] = pChange;
		vFileName.push_back(pChange->sName);

		// Отримання наступного запису в файлі
		pXmlElement = pXmlElement->NextSiblingElement("file");
	}


	return 0;
}

// Creating full file path
int FormatFullFolderPath()
{
	char szFolder[16] = { 0 };
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
		
	sprintf_s(szFolder, 16, "%02d\\%02d%02d\\", nMonth, nDay, nMonth);

	g_strFolderPath.append(szFolder);

	return 0;
}

// Change symbol inside file
int ChangeSymbol()
{
	string strFilePath = "";
	string strFullPath = "";
	string strLog = "";
	string sLine = "";
	fstream parsingFile;
	size_t nPos;
	bool bIsChanged = false;
	int nLength = 0;
	int nChangeLength = 0;

	HANDLE hFindFile;
	WIN32_FIND_DATA findData;
	
	for each (string sFilename in vFileName)
	{
		CHANGE_DATA pData;
		strFullPath = g_strFolderPath;
		strFullPath.append(sFilename);
		nChangeLength = g_mapChangeInfo[sFilename]->sOrigValue.length();

		hFindFile = FindFirstFile(strFullPath.c_str(), &findData);
		do {
			if (hFindFile == INVALID_HANDLE_VALUE)
			{
				char szErrMessage[BIG_BUFF_SIZE] = { 0 };
				DWORD dwErr = GetLastError();
				if (dwErr == ERROR_FILE_NOT_FOUND)
				{
					continue;
				}
				GetLastErrorMessage(dwErr, szErrMessage);
				WriteLog(szErrMessage);
				return -1;
			}

			bIsChanged = false;
			nPos = 0;
			nLength = 0;
			strFilePath = "";
			strFilePath.append(g_strFolderPath);
			strFilePath.append(findData.cFileName);

			parsingFile.open(strFilePath, ifstream::in | ifstream::out);
			if (parsingFile.is_open())
			{
				strLog = " > Processing file : ";
				strLog.append(strFilePath);
				strLog.append(" - ");
				while (getline(parsingFile, sLine))
				{
					nPos = sLine.find(g_mapChangeInfo[sFilename]->sOrigValue);					
					if (nPos != string::npos)
					{
						sLine.replace(nPos, nChangeLength, g_mapChangeInfo[sFilename]->sNewValue);
						nLength += nPos;
						parsingFile.seekg(nLength, parsingFile.beg);
						parsingFile.write(g_mapChangeInfo[sFilename]->sNewValue.c_str(), nChangeLength);
						parsingFile.flush();
						strLog.append("OK!\n");
						bIsChanged = true;
					}
					nLength += sLine.length() + strlen("\r\n");
				};
				if (bIsChanged == false)
				{
					strLog.append("there is nothing to change\n");
				}				
				WriteLog((char *)strLog.c_str());
				parsingFile.close();
			}
			else
			{
				strLog.append(" - Error opening ");
				strLog.append(strFilePath.c_str());
				strLog.append("\n");
				WriteLog((char *)strLog.c_str());
				return -1;
			}
		} while (FindNextFile(hFindFile, &findData));
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

		CloseHandle(g_hLogFile);
		return nRet;
	}

	nRet = FormatFullFolderPath();
	if (nRet < 0)
	{
		strset(szStart, 0);
		sprintf_s(szStart, BIG_BUFF_SIZE, "End : %04d-%02d-%02d\n", Time.wYear, Time.wMonth, Time.wDay);
		WriteLog(szStart);

		CloseHandle(g_hLogFile);
		return nRet;
	}
	
	nRet = ChangeSymbol();
	if(nRet < 0)
	{
		strset(szStart, 0);
		sprintf_s(szStart, BIG_BUFF_SIZE, "End : %04d-%02d-%02d\n", Time.wYear, Time.wMonth, Time.wDay);
		WriteLog(szStart);

		CloseHandle(g_hLogFile);
		return nRet;
	}

	strset(szStart, 0);
	sprintf_s(szStart, BIG_BUFF_SIZE, "End : %04d-%02d-%02d\n", Time.wYear, Time.wMonth, Time.wDay);
	WriteLog(szStart);

/*
	for (map<char*, PCHANGE_DATA>::iterator it = g_mapChangeInfo.begin(); it != g_mapChangeInfo.end(); ++it)
	{
		if (it->second)
		{
			free(it->second);
		}
	}*/

	return nRet;
}