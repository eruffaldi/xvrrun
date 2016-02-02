/**
 * XVRRUN 1.0 by Emanuele Ruffaldi 2009
 *
 * Initial Version 2009/05/18
 * TODO:
 * - complete network renderer support
 * - set resolution and stereomode for xvrglut
 * - read HTM file for embed mode and modify: version,userparam,engineparam
 * - timeout or key for overriding selection + shortcut => faster
 * - make selection window resizable
 * - detach mode for IE, in the sense that we run close process and run another process
 * - remember position
 * - remember last configuration
 * - keyboard shortcut for selectors
 * - s3d.bin for embed 
 *
 * TODO: opengl32 createfile (why?)
 *
 * Updated 2009/05/27 - not working opengl32.dll removed
 * Updated 2009/07/09 - absolute path
 * Updated 2010/12/07 - fixed 
 */
#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <process.h>  
#include <algorithm>
#include <cctype>
#include <iostream>
#include <vector>
#include "resource.h" 
#include "patcher.h"
#include "axembed.h"	/* Declarations of the functions in DLL.c */
#include <map>
#include <string>
#define _T(x) x

typedef struct _LSA_UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
} LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;

#define WINDOW_STYLE                                    WS_OVERLAPPEDWINDOW
#define WINDOW_STYLE_EX                                 0

// debugger window styles
#define DEBUG_WINDOW_STYLE                              WS_OVERLAPPED
#define DEBUG_WINDOW_STYLE_EX                   0

// full screen window styles
#define FULLSCREEN_STYLE                                WS_POPUP
#define FULLSCREEN_STYLE_EX                             WS_EX_TOPMOST
char exepath[256];

std::string pathonly(const std::string & path)
{
	int n = path.rfind('\\');
	if(n < 0)
		n = path.rfind('/');
	if(n > 0)
	{
		return path.substr(0,n);
	}
	else
		return path;
}

//---------------------------------------------------------------------------------------
// Utility
//---------------------------------------------------------------------------------------
HMODULE mylookup(const char * name,HMODULE r = 0)
{
	static const char * lastxLoadLibrary = 0;
	static HMODULE hlastxLoadLibrary = 0;
	typedef std::map<std::string,HMODULE> map_t;
	static map_t mappy;
	if(r != 0)
	{
		mappy[name] = r;
		//lastxLoadLibrary = name;
		//hlastxLoadLibrary = r;
		return r;
	}
	else
	{
		if(lastxLoadLibrary == name)
			return hlastxLoadLibrary;
		map_t::const_iterator it = mappy.find(name);
		if(it != mappy.end())
			return it->second;
		return 0;
	}
}

struct autobool
{
	autobool(): value(false) {}
	operator bool () const { return value; }
	autobool&operator = (const bool & v) { value = v; return *this;}
	bool value;
};

template <int x>
struct autoint
{
	autoint(): value(x) {}
	operator int () const { return value; }
	autoint&operator = (const int & v) { value = v; return *this;}
	int  value;
};

//---------------------------------------------------------------------------------------
// String Utilities for Path
//---------------------------------------------------------------------------------------
bool startswith(const char * a,const char * b)
{
	int na = strlen(a);
	int nb = strlen(b);
	if(na < nb)
		return false;
	return strncmp(a,b,nb) == 0;
}

bool endswith(const char * a,const char * b)
{
	int na = strlen(a);
	int nb = strlen(b);
	if(na < nb)
		return false;
	return strcmp(a+na-nb,b) == 0;
}

void splitch(const char * path, std::string & p1,std::string & p2,char c)
{	
	const char * pp = strrchr(path,c);
	if(pp != 0)
	{
		p1 = std::string(path,pp-path);
		p2 = std::string(pp+1);
	}
	else
	{
		p2= "";		
		p1 = path;
	}
}

void splitpath(const char * path, std::string & p1,std::string & p2)
{	
	const char * pp = strrchr(path,'\\');
	if(pp != 0)
	{
		p2 = std::string(pp);
		p1 = std::string(path,pp-path);
	}
	else
	{
		p1= "";		
		p2 = path;
	}
}

std::string makeabs(std::string x)
{
	if(x[0] == '\\' || x[0] == '/' || x[1] == ':')
		return x;
	char tmp[256];
	GetCurrentDirectory(sizeof(tmp),tmp);
	std::string r = tmp;
	if(r[r.size()-1] != '\\' && r[r.size()-1] != '/')
		r+='\\';
	return r+x;
}

// TODO: check if p2 is absolute
std::string joinpath(std::string p1,std::string p2)
{
	if(p1.empty())
		return p2;
	std::string r = p1;
	if(r[r.size()-1] != '\\' && r[r.size()-1] != '/')
		r+='\\';
	return r + p2;
}

std::string splitpath1(std::string x)
{
	std::string p1,p2;
	splitpath(x.c_str(),p1,p2);
	return p1;
}

void skipws(const char * & szk)
{
	while(isspace(*szk))
		szk++;
}

void fixvalue(char * dst, const char * src)
{
	skipws(src);
	strcpy(dst,src);
}

struct mcbinfo
{
    HMONITOR hmi;
    int count;
    int index;
};

#ifndef __in
#define __in
#define __in_opt
#endif
BOOL CALLBACK MyInfoEnumProc(
  __in  HMONITOR hMonitor,
  __in  HDC hdcMonitor,
  __in  LPRECT lprcMonitor,
  __in  LPARAM dwData
)
{
    mcbinfo * info = (mcbinfo *)dwData;
    if(info->count++ == info->index)
    {
        info->hmi = hMonitor;
    }
    return TRUE;
}


/**
 * Makes a Window full screen
 */
void MakeFullScreen(HWND hwnd, RECT & rcold,bool fullscreen, int defmonitor = -1,int monitorptx = -1)
{
	ShowWindow(hwnd, SW_HIDE);
	if(fullscreen)
	{
		GetWindowRect(hwnd, &rcold);
        HMONITOR hm;
        if(defmonitor >= 0)
        {
            printf("selecting monitor by identifier:id %d\n",defmonitor);
            mcbinfo info;
            info.hmi = 0;
            info.index = defmonitor;
            info.count = 0;
            EnumDisplayMonitors(0,0,MyInfoEnumProc,(LPARAM)&info);
            hm = info.hmi;
        }
		else if(defmonitor == -1)
        {
			// from pointof viewport
            if(monitorptx == -1)
            {
                printf("nearestmonitor\n");
                hm = MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
            }
            else 
            {
                printf("monitor by horizontal point: %d\n",monitorptx);
                POINT pt;
                pt.x = monitorptx;
                pt.y = 0;
                hm = MonitorFromPoint(pt,MONITOR_DEFAULTTONEAREST);  
            }
        }
		if(defmonitor == -2)
		{
			RECT rc;
			HWND h =GetDesktopWindow();
			GetClientRect(h,&rc);
			printf("selected all monitors %d %d x %d %d\n",rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top);
			SetWindowLong(hwnd, GWL_STYLE, FULLSCREEN_STYLE);
			SetWindowLong(hwnd, GWL_EXSTYLE, FULLSCREEN_STYLE_EX);
			SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			SetWindowPos(hwnd, HWND_TOP, rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER);
		}
		else if(defmonitor == -3)
		{
			printf("selected manual shape\n");
			SetWindowLong(hwnd, GWL_STYLE, FULLSCREEN_STYLE);
			SetWindowLong(hwnd, GWL_EXSTYLE, FULLSCREEN_STYLE_EX);
			SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
		else
		{
			MONITORINFOEX mi;
			RECT & rc = mi.rcMonitor;
			ZeroMemory(&mi,sizeof(mi));
			mi.cbSize = sizeof(mi);
			GetMonitorInfo(hm,&mi);
			printf("selected monitor %s: %d %d x %d %d\n",mi.szDevice,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top);
			SetWindowLong(hwnd, GWL_STYLE, FULLSCREEN_STYLE);
			SetWindowLong(hwnd, GWL_EXSTYLE, FULLSCREEN_STYLE_EX);
			SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			SetWindowPos(hwnd, HWND_TOP, rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER);
		}
	}
	else
	{
		SetWindowLong(hwnd, GWL_STYLE, WINDOW_STYLE);
		SetWindowLong(hwnd, GWL_EXSTYLE, WINDOW_STYLE_EX);
		SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		SetWindowPos(hwnd, HWND_TOP,rcold.left, rcold.top,rcold.right-rcold.left,rcold.bottom-rcold.top,0);

	}
	ShowWindow(hwnd, SW_SHOW);
}

//---------------------------------------------------------------------------------------
// Configurations
//---------------------------------------------------------------------------------------

class Configurations;

class Configuration
{
public:	
	unsigned long long hwnd;
	std::string shape; // W x H [: bits][@rate]
	std::string name;
	std::string application;
	std::string userparam;
	std::string engparam;
	std::string version;
	std::string script;
	std::string netrender;
	std::string netconfig;
	autobool netrenderon;
	autobool fullscreen;
	autoint<-1> fullscreenmonitor;
	autobool stereo;
    autoint<-1>  monitorpt;

	std::string configpath; // configuration that specified the path (the last one)
	std::string configdir; // configuration that specified the path (the last one)
};

std::ostream & operator << (std::ostream & ons, const Configuration & c)
{
	ons << '[' << c.name << "]\napp=" << c.application << "\nuserparam="<<c.userparam<<"\nengineparam="<<c.engparam<<"\nversion="<<c.version<<"\nscript="<<c.script << "\nfullscreen="<<c.fullscreen<<"\nmonitorpt="<<c.monitorpt<<"\nnetrendergl="<<c.netrender<<"\nnetrender="<<c.netrenderon<<"\nnetconfig="<<c.netconfig;
	return ons;
}

class Configurations
{
public:
	void load(const std::string & filename);
	Configurations() :active(0) {}
	
	typedef std::map<std::string, Configuration*> container_t;
	container_t confs;
	Configuration * active;
	
};

void Configurations::load(const std::string & filename)
{
	{
		FILE * fp = fopen(filename.c_str(),"rb");
		if(fp == 0)
			return;
		fclose(fp);
	}
	char tmp[256];
	std::vector<char> sz(256);
	std::vector<char> r(2048);
	DWORD n1 = GetPrivateProfileSectionNames(&sz[0],sz.size(),filename.c_str());
	const char * szname = &sz[0];
	while(szname[0] != 0)
	{
		DWORD n = GetPrivateProfileSection(szname,&r[0],r.size(),filename.c_str());
		const char * szkey = &r[0];
 		Configuration * conf=0;
		// first lookup for the extend
		while(szkey[0] != 0)
		{
			if(strncmp(szkey,"extend=",7) == 0)
			{
				fixvalue(tmp,szkey+7);
				std::map<std::string, Configuration*>::const_iterator it = confs.find(tmp);
				if(it != confs.end())
				{
					conf = new Configuration(*it->second);
					break;
				}
			}
			szkey += strlen(szkey)+1;
		}
		if(conf == 0)
		{
			conf = new Configuration();
			conf->configdir = pathonly(filename);
			conf->configpath = filename;
		}
		conf->name = szname;
		
		// now process building a map
		szkey = &r[0];
		while(szkey[0] != 0)
		{
			if(strncmp(szkey,"app=",4) == 0)
			{
				fixvalue(tmp,szkey+4);
				conf->application = tmp;
			}
			else if(strncmp(szkey,"extend=",7) == 0)
			{
			}
			else if(strncmp(szkey,"userparam=",10) == 0)
			{
				fixvalue(tmp,szkey+10);
				conf->userparam = tmp;
			}
			else if(strncmp(szkey,"version=",8) == 0)
			{
				fixvalue(tmp,szkey+8);
				conf->version = tmp;
			}
			else if(strncmp(szkey,"engineparam=",12) == 0)
			{
				fixvalue(tmp,szkey+12);
				conf->engparam = tmp;
			}
			else if(strncmp(szkey,"script=",7) == 0)
			{
				fixvalue(tmp,szkey+7);
				conf->script = tmp;
			}
			else if(strncmp(szkey,"shape=",6) == 0)
			{
				fixvalue(tmp,szkey+6);
				conf->shape = tmp;
			}
			else if(strncmp(szkey,"stereo=",7) == 0)
			{
				fixvalue(tmp,szkey+7);
				conf->stereo = stricmp(tmp,"true") == 0 || tmp[0] != '0';
			}
			else if(strncmp(szkey,"fullscreen=",11) == 0)
			{
				fixvalue(tmp,szkey+11);
				conf->fullscreen = stricmp(tmp,"true") == 0 || tmp[0] != '0';
			}
			else if(strncmp(szkey,"fullscreenmonitor=",11+7) == 0)
			{
				fixvalue(tmp,szkey+11+7);
				conf->fullscreenmonitor = atoi(tmp);
			}
			else if(strncmp(szkey,"monitorpt=",10) == 0)
			{
				fixvalue(tmp,szkey+10);
				conf->monitorpt = atoi(tmp);
			}
			else if(strncmp(szkey,"netrendergl=",12) == 0)
			{
				fixvalue(tmp,szkey+12);
				conf->netrender = tmp;
			}
			else if(strncmp(szkey,"netrender=",10) == 0)
			{
				fixvalue(tmp,szkey+10);
				conf->netrenderon = stricmp(tmp,"true") == 0 || tmp[0] != '0';
			}
			else if(strncmp(szkey,"netconfig=",10) == 0)
			{
				fixvalue(tmp,szkey+10);
				conf->netconfig = tmp;
			}
			else
				printf("%s: unknown %s in section %s\n",filename.c_str(),szname,szkey);
			szkey += strlen(szkey)+1;
		}
		
		confs[szname] = conf;
		szname += strlen(szname)+1;
	}
}

//---------------------------------------------------------------------------------------
// Globals
//---------------------------------------------------------------------------------------

Configurations configurations;

//---------------------------------------------------------------------------------------
// Network renderer Redirection Mechanism
//---------------------------------------------------------------------------------------
FILE * fplog;
std::string redirectopengl32;

typedef ULONG (PASCAL FAR *tLdrLoadDll)(
		IN PWCHAR PathToFile OPTIONAL,
		IN ULONG Flags OPTIONAL,
		IN PUNICODE_STRING ModuleFileName,
		OUT PHANDLE ModuleHandle);

typedef HMODULE  (PASCAL FAR * tGetModuleHandle )(const char * name);
typedef HMODULE  (PASCAL FAR * tLoadLibrary)(const char * name);
typedef HMODULE  (PASCAL FAR * tLoadLibraryExW)(const wchar_t * name,HANDLE h,DWORD flags);
typedef HANDLE  (PASCAL FAR *tCreateFile)(
  __in      LPCTSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile
);

typedef HANDLE  (PASCAL FAR *tCreateFileW)(
  __in      const wchar_t*lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile
);

tGetModuleHandle pGetModuleHandle;
tLoadLibrary pLoadLibrary;
tLoadLibraryExW pLoadLibraryExW;
tCreateFile pCreateFile;
tCreateFileW pCreateFileW;
tLdrLoadDll pLdrLoadDll;

static ULONG PASCAL FAR xLdrLoadDll(
		IN PWCHAR PathToFile OPTIONAL,
		IN ULONG Flags OPTIONAL,
		IN PUNICODE_STRING ModuleFileName,
		OUT PHANDLE ModuleHandle)
{
	wchar_t buf[256];
	wcsncpy(buf,ModuleFileName->Buffer,ModuleFileName->Length);
	//printf("\t\t>LdrLoadDll %d %ws\n",Flags,buf);
	ULONG r = pLdrLoadDll(PathToFile,Flags,ModuleFileName,ModuleHandle);
	//printf("\t\t<LdrLoadDll %d %ws %p\n",Flags,buf,r);
	return r;
}

static HMODULE PASCAL FAR xGetModuleHandle(const char * name)
{
	return pGetModuleHandle(name);
}

static HMODULE PASCAL FAR xLoadLibraryExW(const wchar_t * name,HANDLE h,DWORD f)
{
	//printf("\t>LoadLibraryExW %ws %x %x\n",name,h,f);
	HMODULE hr = pLoadLibraryExW(name,h,f);
	//printf("\t<LoadLibraryExW %ws %p %p => %p\n",name,h,f,hr);
	return hr;
}

static HMODULE PASCAL FAR xLoadLibrary(const char * name)
{
	//static HMODULE hLibrary = 0;
	const bool redirected = (_stricmp(name,"opengl32.dll") == 0 || _stricmp(name,"OPENGL32") == 0) ;
	if(redirected)
	{
		if(!redirectopengl32.empty())
		{
//			printf(">LoadLibrary Ask Redirected %s %d To %s\n",name,redirected,redirectopengl32.c_str());
			HMODULE hLibrary = pLoadLibrary(redirectopengl32.c_str());
//			printf("<LoadLibrary Loaded Redirected %s %d To %p\n",name,redirected,hLibrary);
			return hLibrary;
		}
	}
//	printf(">LoadLibrary %s\n",name);
	HMODULE r = pLoadLibrary(name);
//	printf("<LoadLibrary %s as %p\n",name,r);
	return r;
}

bool endswithpak(const char * p)
{
	int n = strlen(p);
	if(n < 3+4+4)
		return false;
	return p[n-1] == 'k' && p[n-2] == 'a' && p[n-3] == 'p' && p[n-4] == '.';
}

static HANDLE  PASCAL FAR xCreateFile(
  __in      LPCTSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile
)
{
	//printf("CreateFile %s\n",lpFileName);
	if(!configurations.active->netconfig.empty() && endswith(lpFileName,"XVR Network Renderer\\masterconfig.ini")) 
	{
		printf("\tRedirect %s => %s\n",lpFileName,configurations.active->netconfig.c_str());
		HANDLE h = pCreateFile(configurations.active->netconfig.c_str(),dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
		if(h == INVALID_HANDLE_VALUE)
		{
			printf("not found ",configurations.active->netconfig.c_str());
			return pCreateFile(lpFileName,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
		}
	}
	else if(endswithpak(lpFileName))
	{
		// try load default
		HANDLE h = pCreateFile(lpFileName,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
		if(h == INVALID_HANDLE_VALUE)
		{
			// try in local folder
			const char * cp = strrchr(lpFileName,'\\');
			if(cp != 0)
			{
				printf("\tTryRedirect %s => %s\n",lpFileName,cp+1);
				h = pCreateFile(cp+1,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
				if(h == INVALID_HANDLE_VALUE)
				{
					char tmp[256];
					strcpy(tmp,exepath);
					strcat(tmp,"\\");
					strcat(tmp,cp+1);
					printf("\tTryRedirect %s => %s\n",lpFileName,tmp);
					h = pCreateFile(tmp,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
				}
			}
			
		}
		return h;
	}
	else
	{
		return pCreateFile(lpFileName,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
	}
}

static HANDLE  PASCAL FAR xCreateFileW(
  __in      const wchar_t*lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile
)
{
	//printf("CreateFileW %ws\n",lpFileName);
	return pCreateFileW(lpFileName,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
}

//---------------------------------------------------------------------------------------
// Web Embedder
//---------------------------------------------------------------------------------------

extern "C"
{
	// axembed.h
	void WINAPI ResizeBrowser(HWND, DWORD, DWORD);
	long WINAPI EmbedBrowserObject(HWND);
	void WINAPI UnEmbedBrowserObject(HWND);
	void WINAPI DoPageAction(HWND hwnd, DWORD action);
	long WINAPI DisplayHTMLPage(HWND, const char *);
	long WINAPI DisplayHTMLStr(HWND, const char *);
	HRESULT WINAPI GetWebPtrs(HWND hwnd, IWebBrowser2 **webBrowser2Result, IHTMLDocument2 **htmlDoc2Result);
	IDispatch * WINAPI CreateWebEvtHandler(HWND hwnd, IHTMLDocument2 * htmlDoc2, DWORD extraData, long id, IUnknown *obj, void *userdata);
}


const char g_szClassName[] = "myWindowClass";
LRESULT CALLBACK XWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_SIZE:
			ResizeBrowser(hwnd, LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_KEYDOWN:
			printf("window key %d %d\n",wParam,lParam);
			break;
		case WM_CREATE:
			if(EmbedBrowserObject(hwnd) == -1)
			{
				printf("Cannot create embedded IE\n");
				return TRUE;
			}
			break;
		case WM_NOTIFY:
		{
			break;
			NMHDR	*nmhdr;

			// Is it a message that was sent from one of our _IDispatchEx's (informing us
			// of an event that happened which we want to be informed of)? If so, then the
			// NMHDR->hwndFrom is the handle to our window, and NMHDR->idFrom is 0.
			nmhdr = (NMHDR *)lParam;
			if (((NMHDR *)lParam)->hwndFrom == hwnd && !((NMHDR *)lParam)->idFrom)
			{
				// This message is sent from one of our _IDispatchEx's. The NMHDR is really
				// a WEBPARAMS struct, so we can recast it as so. Also, the wParam is the
				// __IDispatchEx object we used to attach to the event that just happened.
				WEBPARAMS		*webParams;
				_IDispatchEx	*lpDispatch;

				webParams =		(WEBPARAMS *)lParam;
				lpDispatch = 	(_IDispatchEx *)wParam;

				// If NMHDR->code is not zero, then this is not the "beforeunload" event.
				if (((NMHDR *)lParam)->code)
				{
					LPCTSTR		eventType;

					// It is some other event type, such as "onmouseover".
					eventType = webParams->eventStr;
					printf("notify %s %d\n",eventType ,lpDispatch->id);

					// Remember that we assigned a unique ID to each element on the page (and
					// each event that is not associated with any particular element). So let's
					// see which element's _IDispatchEx this is.
					switch (lpDispatch->id)
					{
						case 3:
						{
							IHTMLWindow2	*htmlWin2;
							htmlWin2 = (IHTMLWindow2 *)lpDispatch->object;				
							break;
						}
						case 4:
						{
							IHTMLDocument2	*htmlDoc2;
							htmlDoc2 = (IHTMLDocument2 *)lpDispatch->object;				
							break;
						}
					}
				}
				else
				{
					// This _IDispatch is about to be freed, so we need to detach all
					// events that were attached to it.
					VARIANT			varNull;
					printf("end %d\n",lpDispatch->id);

					varNull.vt = VT_NULL;
					switch (lpDispatch->id)
					{
						case 3:
						{
							IHTMLWindow2 * htmlWin2;
							htmlWin2 = (IHTMLWindow2 *)lpDispatch->object;
							break;
						}
						case 4:
						{
							IHTMLDocument2 * htmlDoc2;
							htmlDoc2 = (IHTMLDocument2 *)lpDispatch->object;
							// Detach from the "put_ondblclick" event for the document
							htmlDoc2->put_onkeydown(varNull);
							htmlDoc2->Release();
							break;
						}
					}
				}
			}

			// Must be some other entity that sent me a WM_NOTIFY. It wasn't because of a
			// web page action.
			else
			{

			}

			break;
		}
		case WM_DESTROY:
			PostQuitMessage(0);
			return TRUE;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

WPARAM WINAPI XDisplayWeb(HWND parent, HINSTANCE hInstance, HINSTANCE hInstNULL, LPSTR lpszCmdLine, int nCmdShow, bool istext, const char * title,const char * html_or_url,bool fullscreen,int defmonitor,int monitorpt, int x, int y, int width,int height)
{
	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;
	OleInitialize(0);

	//Step 1: Registering the Window Class
	wc.cbSize		 = sizeof(WNDCLASSEX);
	wc.style		 = 0;
	wc.lpfnWndProc	 = XWndProc;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra	 = 0;
	wc.hInstance	 = hInstance;
	wc.hIcon		 = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor		 = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = g_szClassName;
	wc.hIconSm		 = LoadIcon(NULL, IDI_APPLICATION);

	if(!RegisterClassEx(&wc))
	{
		MessageBox(NULL, "Window Registration Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	if(parent)
	{
		x = 0;
		y = 0;
	}

	// Step 2: Creating the Window
	hwnd = CreateWindowEx(
		0,
		g_szClassName,
		title == 0 ? html_or_url:title,
		WS_OVERLAPPEDWINDOW,
		x,y,width,height,
		HWND_DESKTOP, NULL, hInstance, NULL);
	// SetWindowPos(hwnd, 0, 0, 0, ClientWidth, ClientHeight, SWP_ASYNCWINDOWPOS);

	// http://stackoverflow.com/questions/170800/embedding-hwnd-into-external-process-using-setparent

	//Remove WS_POPUP style and add WS_CHILD style
	DWORD style = GetWindowLong(hwnd,GWL_STYLE);
	style = style & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_POPUP | WS_MAXIMIZE | WS_SYSMENU);
	style = style | WS_CHILD;
	SetWindowLong(hwnd,GWL_STYLE,style);

	if(parent)
		SetParent(hwnd,parent);

	// TODO specify window size somewhere!

	if(hwnd == NULL)
	{
		MessageBox(NULL, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	HMENU h = GetSystemMenu (hwnd,0);
	///AppendMenu(h,0,IDC_MENU_REFRESH,"Refresh (F5)");
	///AppendMenu(h,0,IDC_MENU_FULLSCREEN,"Fullscreen (F11)");
	//AppendMenu(h,0,IDC_MENU_DETACH,"Detach");
	int r;
	if(istext)
		r = DisplayHTMLStr(hwnd, html_or_url);
	else
	{
		r = DisplayHTMLPage(hwnd, html_or_url);
	}
	if(r != 0)
	{
		printf("cannot display\n");
		return 0;
	}
#ifdef USEEVENTS
	IHTMLDocument2 *			htmlDoc2=0;
	IHTMLWindow2 *				htmlWin2=0;
	VARIANT						varDisp;	
	GetWebPtrs(hwnd, 0, &htmlDoc2);
	/*
	if (!htmlDoc2->get_parentWindow(&htmlWin2) && htmlWin2 &&
		(varDisp.pdispVal = CreateWebEvtHandler(hwnd, htmlDoc2, 0, 3, (IUnknown *)htmlWin2, 0)))
	{
		//htmlWin2->put_onkeydown(varDisp);
	}
	*/

	VariantInit(&varDisp);
	varDisp.vt = VT_DISPATCH;
	if (htmlDoc2 != 0 && (varDisp.pdispVal = CreateWebEvtHandler(hwnd, htmlDoc2, 0, 4, (IUnknown *)htmlDoc2, 0)))
	{
		//printf("connecting put_onkeydown\n");
		//htmlDoc2->put_ondblclick(varDisp);
		//htmlDoc2->put_onkeypress(varDisp);
		//htmlDoc2->put_onkeydown(varDisp);
		
	}
	/*
	if (htmlDoc2 != 0 && (varDisp.pdispVal = CreateWebEvtHandler(hwnd, htmlDoc2, 0, 3, (IUnknown *)htmlDoc2, 0)))
	{
		printf("connecting put_onkeydown\n");
		htmlDoc2->put_onkeydown(varDisp);
	}
	*/
	//htmlDoc2->Release();
#endif
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	RECT rcold;
	GetWindowRect(hwnd, &rcold);
	if(fullscreen)
		MakeFullScreen(hwnd,rcold,fullscreen,defmonitor,monitorpt);
	bool detach = false;
	// Step 3: The Message Loop
	while(GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		if(Msg.message == WM_COMMAND && Msg.hwnd == hwnd)
		{
			switch(Msg.lParam)
			{
			case IDC_MENU_REFRESH:
			{
			RECT rc;
			DoPageAction(hwnd,WEBPAGE_REFRESH);
			GetClientRect(hwnd,&rc);
			ResizeBrowser(hwnd, rc.right,rc.bottom);
			break;
			}
			case IDC_MENU_FULLSCREEN:
			{
			RECT rc;
			fullscreen =!fullscreen;
			MakeFullScreen(hwnd,rcold,fullscreen,defmonitor,monitorpt);
			GetClientRect(hwnd,&rc);
			ResizeBrowser(hwnd, rc.right,rc.bottom);
			}
			break;
			case IDC_MENU_DETACH:
				detach = true;
				PostQuitMessage(0);
				break;
			}
		}
		else if(Msg.message == WM_KEYDOWN && Msg.wParam == VK_F5)
		{
			RECT rc;
			DoPageAction(hwnd,WEBPAGE_REFRESH);
			GetClientRect(hwnd,&rc);
			ResizeBrowser(hwnd, rc.right,rc.bottom);
		}
		else if(Msg.message == WM_KEYDOWN && Msg.wParam == VK_F11)
		{
			RECT rc;
			fullscreen =!fullscreen;
			MakeFullScreen(hwnd,rcold,fullscreen,defmonitor,monitorpt);
			GetClientRect(hwnd,&rc);
			ResizeBrowser(hwnd, rc.right,rc.bottom);
		}
		else
		{
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	}
	return Msg.wParam;
}

//---------------------------------------------------------------------------------------
// Runner
//---------------------------------------------------------------------------------------

void fillmacros(std::map<std::string,std::string> & m,Configuration & c)
{
}

std::string lower(std::string myString)
{
	std::transform(myString.begin(), myString.end(), myString.begin(),
               (int(*)(int)) std::tolower);
	return myString;
}


std::string  applymacros(const std::map<std::string,std::string> & m, const std::string &in,Configuration & c)
{
	std::string r = "";
	int k0 = 0;
	while(true)
	{
		int k = in.find('$',k0);
		if(k < 0)
		{
			r += in.substr(k0);
			return r;
		}
		else
		{
			int k2;
			char sep = in[k+1] == '{' ? '}' : in[k+1] == '(' ? ')':' ';
			k+=2;
			k2 = in.find(sep,k);
			if(k2 < 0)
			{
				r += in.substr(k0);
				return r;
			}
			else
			{
				r += in.substr(k0,k-k0-2);
				std::string name = in.substr(k,k2-k);
				name = lower(name);
				k0 = sep == ' ' ? k2 : k2+1;
				std::string value;
				char buf[128];
				if(name == "pwd")
				{
					GetCurrentDirectory(sizeof(buf),buf);
					value = buf;
				}
				else if(name == "version")
				{
					value = c.version;
				}
				else if(name == "xvrrunpath")
				{
					value = exepath;
				}
				else if(name == "configpath")
				{
					value = c.configdir;
				}
				else if(name == "configname")
				{
					value = c.name;
				}
				else
					printf("unknown macro name %s\n",name.c_str());
				r += value;
			}
		}
	}
	return r;
}


/**
 * Run the given configuration with specific target file 
 *
 * \todo replace with just a configuration
 * \todo add HTML gen for IE/xvrglut
 * \todo add netrender
 */
bool RunConfiguration(Configuration & c,const char * target)
{
	std::map<std::string,std::string> macros;
	fillmacros(macros,c);
	std::string app = applymacros(macros,c.application,c);
	c.netconfig = applymacros(macros,c.netconfig,c);
	if(app.empty())
		return false;
		
	target = target[0] == 0 ? c.script.c_str() : target;
	printf("[%s] running <%s> with <%s>\n",c.name.c_str(),target,app.c_str());

	int x=CW_USEDEFAULT,y=CW_USEDEFAULT,width=CW_USEDEFAULT,height=CW_USEDEFAULT,rate=0,pixels=0;
	if(!c.shape.empty())
	{
		std::string sizes,srate,swidth,sheight;
		splitch(c.shape.c_str(),sizes,srate,'@');
		if(!sizes.empty())
		{			
			splitch(sizes.c_str(),swidth,sheight,'x');
			width = atoi(swidth.c_str());
			height = atoi(sheight.c_str());
		}
		if(!srate.empty())
		{
			rate = atoi(srate.c_str());
		}
		x = 0;
		y = 0;
		
	}
	printf("shape %s %dx%d:%d@%d +%d,%d\n",c.shape.c_str(),width,height,pixels,rate,x,y);
	
	if(app == "web" || app == "embed")
	{
		HMODULE h = LoadLibrary("kernel32.dll");
		HMODULE h2 = LoadLibrary("ntdll.dll");
		pLoadLibrary = (tLoadLibrary)GetProcAddress(h,"LoadLibraryA");
		pLoadLibraryExW = (tLoadLibraryExW)GetProcAddress(h,"LoadLibraryExW");
		pCreateFile = (tCreateFile)GetProcAddress(h,"CreateFileA");
		pCreateFileW = (tCreateFileW)GetProcAddress(h,"CreateFileW");
		pLdrLoadDll = (tLdrLoadDll)GetProcAddress(h2,"LdrLoadDll");
		bool doredirect = true; // c.netrenderon;
		CPatch patch_for_loadlibrary(pLoadLibrary, xLoadLibrary,true);
		CPatch patch_for_loadlibraryW(pLoadLibraryExW, xLoadLibraryExW,true);
		CPatch patch_for_create(pCreateFile, xCreateFile,false);
		CPatch patch_for_createW(pCreateFileW, xCreateFileW,false);
		//CPatch patch_for_ldr(pLdrLoadDll,xLdrLoadDll,true);
		
		if(doredirect)
		{
			// TODO: make it absolute!!! redirectopengl32 GetFullPathName
			static char buf[128];
			GetFullPathName(c.netrender.c_str(),sizeof(buf),buf,0);
			//redirectopengl32 = c.netrender; // disable the opengl
			//redirectopengl32 = "w:\\PERCRO\\code\\xvrrun\\xvrrun\\netrender\\opengl32.dll";
			//patch_for_loadlibrary.set_patch();
			//patch_for_loadlibraryW.set_patch();
			//patch_for_modulehandle.set_patch();
			patch_for_create.set_patch();
			patch_for_createW.set_patch();
		}
		
		// check for ...
		bool textmode = false;
		std::string text;
		if(endswith(target,".s3d.bin"))
		{
			char buf[128];
			GetFullPathName(target,sizeof(buf),buf,0);
			char * cp = strrchr(buf,'\\');
			if(cp != 0)
				cp[1] = 0;
			strcat(buf,"_xvrrun.htm");
			text += "<HTML><HEAD><TITLE>";
			text += target;			
			text += "</TITLE></HEAD><body style='background-color:#ffffff;margin:0px;padding:0px' scroll='no'><OBJECT ID='Canvas2D' WIDTH='100%' HEIGHT='100%' CLASSID='CLSID:5D589287-1496-4223-AE64-65FA078B5EAB' TYPE='application/x-oleobject' CODEBASE='http://client.vrmedia.it/XVRPlayer.cab#Version=1,0,0,900'>";
			text += "<PARAM NAME='ScriptName' VALUE='"; text += target; text += "'>";
			if(c.version.empty())
				c.version = "0150";
			text += "<PARAM NAME='EngineVersion' VALUE='"; text += c.version; text += "'>";
			std::string ep = c.engparam;
			if(c.stereo)
				ep += ";STEREO";
			text += "<PARAM NAME='EngineParam' VALUE='"; text += ep; 
			text += "<PARAM NAME='EngineParams' VALUE='"; text += ep; 
			text += "'>";
			text += "<PARAM NAME='UserParam' VALUE='"; text += c.userparam; text += "'>";
			text += "<PARAM NAME='BackgroundColor' VALUE='#ffffff'>";
			text += "</OBJECT></BODY></HTML>";
			//textmode = true;
			FILE * fp = fopen(buf,"wb");
			if(!fp)
			{
				printf("Cannot make %s\n",buf);
				return false;
			}
			printf("%s\n",buf);
			fputs(text.c_str(),fp);
			fclose(fp);
			target= buf;
		}
        // HINSTANCE hInstance
        // HINSTANCE hInstNULL
        // LPSTR lpszCmdLine
        // int nCmdShow
        // bool istext
        // const char * title
        // const char * html_or_url
        // bool fullscreen
        // int defmonitor
        // int monitorpt
        bool b = XDisplayWeb((HWND)c.hwnd,(HINSTANCE)GetModuleHandle (NULL),0,"",SW_SHOW,textmode,target,textmode ? text.c_str():makeabs(target).c_str(),c.fullscreen,c.fullscreenmonitor,c.monitorpt,
            x,y,width,height) !=0;

		if(doredirect)
		{
			//patch_for_loadlibrary.remove_patch();
			//patch_for_loadlibraryW.remove_patch();
			//patch_for_modulehandle.remove_patch();
			patch_for_create.remove_patch();
			patch_for_createW.remove_patch();
		}
		// fix net render
		return b;	
	}
	else
	{
		const char * args[20];
		int count = 0;
		std::string xapp = "\"";
		xapp += app;
		xapp += '\"';
		args[count++] = xapp.c_str();
		args[count++] = target;
		if(c.fullscreen)
			args[count++] = "-f";
		if(!c.userparam.empty())
		{
			args[count++] = "-Userparam";
			args[count++] = c.userparam.c_str();
		}
		if(!c.engparam.empty())
		{
			args[count++] = "-EngineParam";
			args[count++] = c.engparam.c_str();
		}
		if(c.stereo)
			args[count++] = "-s";
		// TODO: add -s for stereo
		// TODO: add -c for stencil
		// TODO: add -r for resolution
		// TODO: add -ms for multisample
		args[count++] = 0;
		printf("app is %s\n",app.c_str());
		for(int i = 0; i < count; i++)
			printf("%d <%s>\n",i,args[i]);
		_execve(app.c_str(),args,0);
		return true;
	}
}

//---------------------------------------------------------------------------------------
// Selection Dialog
//---------------------------------------------------------------------------------------

BOOL CALLBACK DlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{
		case WM_INITDIALOG:
			{
				Configurations::container_t::const_iterator it = configurations.confs.begin();
				Configurations::container_t::const_iterator ite = configurations.confs.end();
				for(; it != ite; ++it)
				{
					int index = SendDlgItemMessage(hwnd, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)it->second->name.c_str());
					SendDlgItemMessage(hwnd, IDC_LIST, LB_SETITEMDATA, (WPARAM)index, (LPARAM)it->second);	
					if(configurations.active == it->second)
						SendDlgItemMessage(hwnd,IDC_LIST, LB_SETSEL,1,(LPARAM)index);
				}
			}
			break;
		break;
		case WM_KEYDOWN:
			if(wParam >= '0' && wParam <= '9')
			{
				int index = wParam-'0';
				if(index < configurations.confs.size())
				{
					configurations.active = (Configuration*)SendDlgItemMessage(hwnd, IDC_LIST,LB_GETITEMDATA,index,0);
					printf("%p\n",configurations.active);
					EndDialog(hwnd, 1);
				}
			}
			else if(wParam == VK_ESCAPE)
			{
				EndDialog(hwnd,0);
			}
			else
				printf("%d %c\n",wParam,wParam);
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDC_OK:
				{
					HWND hList = GetDlgItem(hwnd, IDC_LIST);
					int index=SendMessage(hList, LB_GETCURSEL , (WPARAM)0, (LPARAM)0);
					if(index >= 0)
					{
						configurations.active = (Configuration*)SendMessage(hList, LB_GETITEMDATA,index,0);
						EndDialog(hwnd, 1);
					}
				}
				break;
				case IDC_CANCEL:
					EndDialog(hwnd, 0);
					break;
				case IDC_REFRESH:
					SendDlgItemMessage(hwnd, IDC_LIST, LB_RESETCONTENT, 0, 0);
					// TODO: reload
					break;
				case IDC_LIST:
					switch(HIWORD(wParam))
					{
						case LBN_SELCHANGE:
						{
							HWND hList = GetDlgItem(hwnd, IDC_LIST);
							UINT index=SendMessage(hList, LB_GETCURSEL , (WPARAM)0, (LPARAM)0);
							//SetDlgItemText(hwnd, IDC_SHOWTITLE, "");
							break;
						}
						case LBN_DBLCLK:
						{
							SendMessage(hwnd,WM_COMMAND,MAKEWPARAM(IDC_OK,0),0);
							break;
						}
						break;
					}
				break;
			}
		break;
		case WM_CLOSE:
			EndDialog(hwnd, 0);
		break;
		default:
			return FALSE;
	}
	return TRUE;
}

//---------------------------------------------------------------------------------------
// Entrypoint
//---------------------------------------------------------------------------------------

void usage()
{
	printf("xvrrun 1.1 by Emanuele Ruffaldi 2009\n"
	"Syntax: xvrrun [options] targetfile\n"
	"\ttargetfile can be a .s3d.bin file or an .htm file\n"
	"Options:\n"
	"\t-c configuration_name      If not specified it opens the Configuration selector\n"
	"\t-f configuration_file      Load the file after default configurations\n"
	"\t-l                         List configuration\n"
	"\t-n                         Disable default configurations (e.g. iexplore)\n"
	"\t-h                         This help\n"
	);
	exit(0);
}

int main(int argc, char * argv[])
{
	const char * activeconfig = "";
	const char * target = "";
	const char * localconfigfile = "";
	bool dumpmode = false;
	bool spawned = false;
	bool noauto = false;
	int idx = 0;
	unsigned long long hwnd = 0;
	GetModuleFileName(0,exepath,sizeof(exepath));
	char * xp = strrchr(exepath,'\\');
	if(xp != 0)
		*xp = 0;

	for(int i = 0; i < argc; i++)
		printf("%d: %s\n",i,argv[i]);

	for(int i = 1; i < argc; i++)
	{
		if(strcmp(argv[i],"-c") == 0)
		{
			activeconfig = argv[i+1];
			i++;
		}
		else if(strcmp(argv[i],"-n") == 0)
		{
			noauto = true;
		}
		else if(strcmp(argv[i],"-s") == 0)
			spawned = true;
		else if(strcmp(argv[i],"-f") == 0)
		{
			localconfigfile = argv[i+1];
			i++;
		}
		else if(strcmp(argv[i],"-w") == 0)
		{
			sscanf(argv[i+1],"%llu",&hwnd);
			i++;
		}
		else if(strcmp(argv[i],"-l") == 0)
			dumpmode = true;
		// last option
		else if(argv[i][0] == '-' || strcmp(argv[i],"-h") == 0 || strcmp(argv[i],"--help") == 0)
			usage();
		else if(idx == 0)
		{
			target = argv[i];
			idx++;
		}
		else
		{
			printf("unknown argument %s\n", argv[i]);
		}
	}
    printf("Configuration is %s\n",activeconfig);
	if(!noauto)
	{
		Configuration * c = new Configuration();
		c->application = "embed";
		c->name = "iexplore";
		configurations.confs["iexplore"] = c;
	
	}
	if(startswith(target,"xvr:"))
	{
		target += 4;
	}
	
	std::string p1 = makeabs(joinpath(splitpath1(argv[0]),"xvrrun.ini"));
	std::string p2 = makeabs("xvrrun.ini");
	std::string p3 = localconfigfile[0] != 0 ? makeabs(localconfigfile) : "";
	printf("Loading <%s> <%s> <%s>\n",p1.c_str(),p2.c_str(),p3.c_str());
	configurations.load(p1.c_str());
	if(p1 != p2)
		configurations.load(p2.c_str());
	if(!p3.empty() && p1 != p3 && p2 != p3)
		configurations.load(p3.c_str());
	if(dumpmode)
	{
		Configurations::container_t::const_iterator it = configurations.confs.begin();
		Configurations::container_t::const_iterator ite = configurations.confs.end();
		for(; it != ite; ++it)
		{
			std::cout << *it->second << std::endl;
		}
		return 0;
	}
	HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(0);
	if(activeconfig[0] == 0)
		DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DlgProc);
	else
		configurations.active = configurations.confs[activeconfig];


	if(configurations.active != 0)
	{
		configurations.active->hwnd = hwnd;
		Configuration & c = *configurations.active;
		if(c.netrenderon && !spawned)
		{
			// respawn all plus ... except if c is selected ...
			std::vector<const char *> nargv(argc+1+(activeconfig[0] == 0 ? 2 : 0)+1+2);
			std::string exe = argv[0];
            std::string xconfig;
			int k = exe.rfind('\\');
			if (k >= 0)
				exe = exe.substr(0,k) + "\\netrender" + exe.substr(k);
			else
				exe = "netrender\\" + exe;
			nargv[0] = exe.c_str();
			CopyFile(argv[0],nargv[0],FALSE);
			int base = 1;
			nargv[base++] = "-s";
            nargv[base++] = "-f";
            nargv[base++] = p1.c_str();
			if(activeconfig[0] == 0)
			{
				nargv[base++] = "-c";
				nargv[base++] = c.name.c_str();
			}
			for(int i = 1; i < argc; i++)
				nargv[base++] = argv[i];
			nargv[base] = 0;
            printf("spawning %s\n",exe.c_str());            
            for(int q = 0; q < 100; q++)            
                if(nargv[q] == 0)
                    break;
                else
                    printf("\t%s\n",nargv[q]);
			_spawnv(_P_WAIT,exe.c_str(),&nargv[0]);
			return 0;
		}
		else
			RunConfiguration(*configurations.active,target);
	}
	else
		printf("configuration not specified\n");
	return 0;
}

__declspec(dllexport) void xvrrun_close(void* h)
{
	
}

// for MATLAB
__declspec(dllexport) void* xvrrun_new(const char * target, const char * configuration)
{
	//XDisplayWeb(HINSTANCE hInstance, HINSTANCE hInstNULL, LPSTR lpszCmdLine, int nCmdShow, bool istext, const char * title,const char * html_or_url,bool fullscreen)
	//
	return (void*)0;
}
