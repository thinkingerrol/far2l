#include <all_far.h>

#include "Int.h"

//------------------------------------------------------------------------
FTPCopyInfo::FTPCopyInfo(void)
{
	asciiMode       = FALSE;
	ShowProcessList = FALSE;
	AddToQueque     = FALSE;
	MsgCode         = ocNone;
	Download        = FALSE;
	UploadLowCase   = FALSE;
	FTPRename       = FALSE;
}
//------------------------------------------------------------------------
void AddWait(time_t tm)
{
	for(int n = 0; n < 3; n++)
		if(FTPPanels[n] && FTPPanels[n]->hConnect &&
		        FTPPanels[n]->hConnect->IOCallback)
			FTPPanels[n]->hConnect->TrafficInfo->Waiting(tm);
}

//------------------------------------------------------------------------
BOOL WINAPI IsAbsolutePath(LPCSTR path)
{
	return *path=='/';
}
//------------------------------------------------------------------------
const char quotes[] = " \"%,;[]";

void WINAPI QuoteStr(char *str)
{
	char buff[ 1024 ],*m,*src;

	if(strpbrk(str,quotes) == NULL)
		return;

	m = buff;
	src = str;
	*m++ = '\"';

	for(size_t n = 0; n < ARRAYSIZE(buff)-3 && *src; n++)
		if(*src == '\"')
		{
			*m++ = '\"';
			*m++ = '\"';
			n++;
			src++;
		}
		else
			*m++ = *src++;

	*m++ = '\"';
	*m = 0;
	strcpy(str,buff);
}

void WINAPI QuoteStr(String& str)
{
	String  buff;

	if(str.Chr(quotes) == -1)
		return;

	buff.Add('\"');

	for(int n = 0; n < str.Length(); n++)
		if(str[n] == '\"')
		{
			buff.Add('\"');
			buff.Add('\"');
		}
		else
			buff.Add(str[n]);

	buff.Add('\"');
	str = buff;
}

//------------------------------------------------------------------------
#define SIZE_M 1024*1024
#define SIZE_K 1024

void WINAPI Size2Str(char *buff,DWORD sz)
{
	char   letter = 0;
	double size = (double)sz;
	int    rc;

	if(size >= SIZE_M)
	{
		size /= SIZE_M;
		letter = 'M';
	}
	else if(size >= SIZE_K)
	{
		size /= SIZE_K;
		letter = 'K';
	}

	if(!letter)
	{
		sprintf(buff,"%d",(int)sz);
		return;
	}

	sprintf(buff,"%f",size);
	rc = (int)strlen(buff);

	if(!rc || strchr(buff,'.') == NULL)
		return;

	for(rc--; rc && buff[rc] == '0'; rc--);

	if(buff[rc] != '.')
		rc++;

	buff[rc]   = letter;
	buff[rc+1] = 0;
}

DWORD WINAPI Str2Size(char *str)
{
	int    rc = (int)strlen(str);
	double sz;
	char   letter;

	if(!rc)
		return 0;

	rc--;

	if(str[rc] == 'k' || str[rc] == 'K')
		letter = 'K';
	else if(str[rc] == 'm' || str[rc] == 'M')
		letter = 'M';
	else
		letter = 0;

	if(letter)
		str[rc] = 0;

	sz = atof(str);

	if(letter == 'K') sz *= SIZE_K;
	else if(letter == 'M') sz *= SIZE_M;

	return (DWORD)sz;
}
//------------------------------------------------------------------------
int WINAPI StrSlashCount(LPCSTR m)
{
	int cn = 0;

	if(m)
		for(; *m; m++)
			if(*m == '/')
				cn++;

	return cn;
}
//------------------------------------------------------------------------
BOOL WINAPI FTestOpen(LPCSTR nm)
{
	HANDLE f;
	BOOL   rc;
	f = WINPORT(CreateFile)(MB2Wide(nm).c_str(), 0, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	rc = f &&
	     f != INVALID_HANDLE_VALUE &&
	     WINPORT(GetFileType)(f) == FILE_TYPE_DISK;
	WINPORT(CloseHandle)(f);
	return rc;
}

BOOL WINAPI FRealFile(LPCSTR nm,FAR_FIND_DATA* fd)
{
	HANDLE f;
	BOOL   rc;
	f = WINPORT(CreateFile)(MB2Wide(nm).c_str(), 0, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	rc = f &&
	     f != INVALID_HANDLE_VALUE &&
	     WINPORT(GetFileType)(f) == FILE_TYPE_DISK;

	if(rc && fd)
	{
		strcpy(fd->cFileName, nm);
		fd->dwFileAttributes = WINPORT(GetFileAttributes)(MB2Wide(nm).c_str());
		fd->nFileSizeLow     = WINPORT(GetFileSize)(f, &fd->nFileSizeHigh);
		WINPORT(GetFileTime)(f, &fd->ftCreationTime, &fd->ftLastAccessTime, &fd->ftLastWriteTime);
	}

	WINPORT(CloseHandle)(f);
	return rc;
}

//------------------------------------------------------------------------
BOOL WINAPI DoCreateDirectory(char *directoryPath)
{
	PROC(("DoCreateDirectory","[%s]", directoryPath));

	// Check directory
	if(!directoryPath || !*directoryPath)
	{
		Log(("Directory path is empty"));
		return TRUE;
	}

	if(sdc_mkdir(directoryPath, 0777) == 0 || errno==EEXIST)
		return TRUE;

	for (char *p = directoryPath + 1; *p; ++p) {
		if (*p=='/') {
			std::string tmp(directoryPath, p - directoryPath);
			sdc_mkdir(tmp.c_str(), 0777) ;
			//WINPORT(CreateDirectory)(tmp.c_str(), NULL);
		}
	}
	if(sdc_mkdir(directoryPath, 0777) == 0 || errno==EEXIST)
		return TRUE;

	Log(("CreateDirectory error: %d", WINPORT(GetLastError)()));
	return FALSE;

#if 0
	// Get full directory path
	char directoryPathFull[MAX_PATH], *lpFilePart;

	if(!GetFullPathName(directoryPath, MAX_PATH, directoryPathFull, &lpFilePart))
	{
		Log(("GetFullPathName error: %d", WINPORT(GetLastError)()));
		return FALSE;
	}

	// UNC network path flag
	bool isUncPath = false;
	// Folders only path
	char *directoriesPath = directoryPathFull;

	// Build folders only path
	if(StrCmp(directoriesPath, "\\\\", 2) == 0)
	{
		directoriesPath += 2;

		if(StrCmp(directoriesPath, "?\\", 2) == 0)
		{
			directoriesPath += 2;

			if(StrCmp(directoriesPath, "UNC\\", 4) == 0)
			{
				directoriesPath += 4;
				isUncPath = true;
			}
			else if(StrCmp(directoriesPath, "Volume{", 7) == 0)
			{
				directoriesPath += 7;
				directoriesPath = *directoriesPath ? strchr(++directoriesPath, '}') : NULL;

				if(!directoriesPath)
				{
					Log(("Volume name is not valid"));
					return FALSE;
				}

				directoriesPath++;
			}
			else if(*directoriesPath && StrCmp(directoriesPath+1, ":\\", 2) == 0)
			{
				directoriesPath += 3;
			}
		}
		else
		{
			isUncPath = true;
		}
	}
	else if(StrCmp(directoriesPath+1, ":\\", 2) == 0)
	{
		directoriesPath += 3;
	}

	if(isUncPath)
	{
		directoriesPath = strchr(directoriesPath, '/');

		if(!directoriesPath)
		{
			Log(("UNC path does not contains resource name"));
			return FALSE;
		}

		directoriesPath = strchr(++directoriesPath, '/');

		if(!directoriesPath)
		{
			return TRUE;
		}

		directoriesPath++;
	}

	// Root folder, no need to create
	if(!*directoriesPath)
		return TRUE;

	// Step by step create all directoriesPath structure (maybe SHCreateDirectoryEx?)
	char ch=0;

	do
	{
		directoriesPath = strchr(++directoriesPath, '/');

		if(directoriesPath)
		{
			ch = *directoriesPath;
			*directoriesPath = 0;
		}

		Log(("CreateDirectory: [%s]", directoryPathFull));

		if(!CreateDirectory(directoryPathFull, NULL))
		{
			if(WINPORT(GetLastError)() != ERROR_ALREADY_EXISTS)
			{
				Log(("CreateDirectory error: %d", WINPORT(GetLastError)()));
				return FALSE;
			}
		}

		if(directoriesPath)
		{
			*directoriesPath = ch;
		}
	}
	while(directoriesPath);
#endif
	return TRUE;
}

int64_t WINAPI Fsize(LPCSTR nm)
{
	struct stat s = {0};
	if (sdc_stat(nm, &s)<0)
		return 0;
		
	return s.st_size;
}

int64_t WINAPI Fsize(HANDLE File)
{
	DWORD low,hi;
	low = WINPORT(GetFileSize)(File,&hi);

	if(low == MAX_DWORD)
		return 0;
	else
		return ((int64_t)hi) << 32 | low;
}

BOOL WINAPI Fmove(HANDLE file,int64_t restart)
{
	LONG lo = (DWORD)(restart & MAX_DWORD),
	     hi = (DWORD)((restart >> 32) & MAX_DWORD);

	if(WINPORT(SetFilePointer)(file,lo,&hi,FILE_BEGIN) == 0xFFFFFFFF &&
	        WINPORT(GetLastError)() != NO_ERROR)
		return FALSE;

	return TRUE;
}

void WINAPI Fclose(HANDLE file)
{
	if(file)
	{
		WINPORT(SetEndOfFile)(file);
		WINPORT(CloseHandle)(file);
	}
}

BOOL WINAPI Ftrunc(HANDLE h,DWORD move)
{
	if(move != FILE_CURRENT)
		if(WINPORT(SetFilePointer)(h,0,NULL,move) == 0xFFFFFFFF)
			return FALSE;

	return WINPORT(SetEndOfFile)(h);
}

HANDLE WINAPI Fopen(LPCSTR nm,LPCSTR mode /*R|W|A[+]*/, DWORD attr)
{
	BOOL   rd  = toupper(mode[0]) == 'R';
	HANDLE h;

	if(rd)
		h = WINPORT(CreateFile)(MB2Wide(nm).c_str(), GENERIC_READ,
		               FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, attr, NULL);
	else
		h = WINPORT(CreateFile)(MB2Wide(nm).c_str(), GENERIC_WRITE,
		               FILE_SHARE_READ, NULL, OPEN_ALWAYS, attr, NULL);

	if(!h ||
	        h == INVALID_HANDLE_VALUE)
		return NULL;

	do
	{
		if(toupper(mode[0]) == 'A' || mode[1] == '+')
		{
			LONG nHighPart = 0;

			if((WINPORT(SetFilePointer)(h,0,&nHighPart,FILE_END) == INVALID_SET_FILE_POINTER) && (WINPORT(GetLastError)() != NO_ERROR))
				break;
		}

		if(!rd)
			WINPORT(SetEndOfFile)(h);  //Ignore SetEndOfFile result in case of use with CON, NUL and others

		return h;
	}
	while(0);

	WINPORT(CloseHandle)(h);
	return NULL;
}

int WINAPI Fwrite(HANDLE File,LPCVOID Buff,int Size)
{
	DWORD res;
	return WINPORT(WriteFile)(File,Buff,(DWORD)Size,&res,NULL) ? ((int)res) : (-1);
}

int WINAPI Fread(HANDLE File,LPVOID Buff,int Size)
{
	DWORD res;
	return WINPORT(ReadFile)(File,Buff,(DWORD)Size,&res,NULL) ? ((int)res) : (-1);
}

void DMessage(LPCSTR str,BOOL full,int color,int y)
{
	char err[MAX_PATH];

	if(full)
	{
		StrCpy(err, str, ARRAYSIZE(err));
		int len = (int)strlen(err),
		    w   = Min((int)ARRAYSIZE(err)-1, FP_ConWidth()-4);

		while(len < w) err[len++] = ' ';

		err[len] = 0;
		FP_Info->Text(2,y,color,err);
	}
	else
		FP_Info->Text(2,y,color,str);
}

void WINAPI IdleMessage(LPCSTR str,int color)
{
	static HANDLE hScreen;

//Clear
	if(!str)
	{
		if(hScreen)
		{
			FP_Info->RestoreScreen(hScreen);
			hScreen = NULL;
		}

		return;
	}

//Draw
	LPCSTR msg = FP_GetMsg(str);

	if(IS_FLAG(Opt.IdleMode,IDLE_CAPTION))
		SaveConsoleTitle::Text(msg);

	if(IS_FLAG(Opt.IdleMode,IDLE_CONSOLE))
	{
		DWORD    er  = WINPORT(GetLastError)();
		BOOL     err = er != ERROR_CANCELLED &&
		               er != ERROR_SUCCESS &&
		               er != ERROR_NO_MORE_FILES;

		if(!hScreen)
			hScreen = FP_Info->SaveScreen(0,0,FP_ConWidth(),2);

		DMessage(msg, err, color, 0);

		if(err)
			DMessage(__WINError(), err, color, 1);

		FP_Info->Text(0,0,0,NULL);
	}
}

int WINAPI FMessage(unsigned int Flags,LPCSTR HelpTopic,LPCSTR *Items,int ItemsNumber,int ButtonsNumber)
{
	time_t b = time(NULL);
	BOOL   delayed;
	int    rc;
	rc = FP_Message(Flags, HelpTopic, Items, ItemsNumber, ButtonsNumber, &delayed);

	if(delayed)
		AddWait(time(NULL)-b);

	return rc;
}

int WINAPI FDialog(int X2,int Y2,LPCSTR HelpTopic,struct FarDialogItem *Item,int ItemsNumber)
{
	time_t b = time(NULL);
	int    rc;
	rc = FP_Info->Dialog(FP_Info->ModuleNumber,-1,-1,X2,Y2,HelpTopic,Item,ItemsNumber);
	AddWait(time(NULL)-b);
	return rc;
}

int WINAPI FDialogEx(int X2,int Y2,LPCSTR HelpTopic,struct FarDialogItem *Item,int ItemsNumber,DWORD Flags,FARWINDOWPROC DlgProc,LONG_PTR Param)
{
	time_t b = time(NULL);
	int    rc;

	if(DlgProc == (FARWINDOWPROC)(size_t)-1)
		DlgProc = FP_Info->DefDlgProc;

	rc = FP_Info->DialogEx(FP_Info->ModuleNumber,-1,-1,X2,Y2,HelpTopic,Item,ItemsNumber,0,Flags,DlgProc,Param);
	AddWait(time(NULL)-b);
	return rc;
}

void WINAPI AddEndSlash(String& p, char slash)
{
	if(!p.Length()) return;

	if(!slash)
		slash = '/';

	if(p[p.Length()-1] != slash)
		p.Add(slash);
}

void WINAPI AddEndSlash(char *Path,char slash, size_t ssz)
{
	size_t Length;

	if(!Path || !Path[0]) return;

	Length = strlen(Path)-1;

	if(Length <= 0 || Length >= ssz) return;

	if(!slash)
		slash = '/';

	if(Path[Length] != slash)
	{
		Path[Length+1] = slash;
		Path[Length+2] = 0;
	}
}

void WINAPI DelEndSlash(String& p,char shash)
{
	int len;

	if((len=p.Length()-1) >= 0 &&
	        p[len] == shash)
		p.SetLength(len);
}

void WINAPI DelEndSlash(char *Path,char shash)
{

	if(Path && *Path)
	{
		size_t len=strlen(Path);
		if(Path[len-1] == shash)
		{
			Path[len-1] = 0;
		}
	}
}

char* WINAPI TruncStr(char *Str,int MaxLength)
{
	int Length;

	if((Length=static_cast<int>(strlen(Str)))>MaxLength)
      {
		if(MaxLength>3)
		{
			char *TmpStr=new char[MaxLength+5];
			sprintf(TmpStr,"...%s",Str+Length-MaxLength+3);
			strcpy(Str,TmpStr);
			delete[] TmpStr;
		}
		else
			Str[MaxLength]=0;
      }

	return(Str);
}

char *WINAPI PointToName(char *Path)
{
	char *NamePtr = Path;

	while(*Path)
	{
		if(*Path=='/')
			NamePtr=Path+1;

		Path++;
	}

	return NamePtr;
}

BOOL WINAPI CheckForEsc(BOOL isConnection,BOOL IgnoreSilent)
{
	WORD  ESCCode = VK_ESCAPE;
	BOOL  rc;

	if(!IgnoreSilent && IS_FLAG(FP_LastOpMode,OPM_FIND))
		return FALSE;

	rc = CheckForKeyPressed(&ESCCode,1);

	if(!rc)
		return FALSE;

	rc = !Opt.AskAbort ||
	     AskYesNo(FMSG(isConnection ? MTerminateConnection : MTerminateOp));

	if(rc)
	{
		Log(("ESC: cancel detected"));
	}

	return rc;
}

BOOL WINAPI IsDirExist(LPCSTR nm)
{
	struct stat s = {0};
	if (sdc_stat(nm, &s) < 0)
		return FALSE;
	
	if ((s.st_mode & S_IFMT)!=S_IFDIR)
		return FALSE;
		
	return TRUE;
}

void WINAPI FixFTPSlash(String& s)
{
	FixFTPSlash((char*)s.c_str());
}
void WINAPI FixFTPSlash(char *s)
{
	if(!s) return;
}

void WINAPI FixLocalSlash(String& s)
{
	FixLocalSlash((char*)s.c_str());
}
void WINAPI FixLocalSlash(char *s)
{
}
