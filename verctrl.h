/* 
 * $Author: Senthil Vaithilingam
 *
 * CONFIDENTIAL AND CONTAINING PROPRIETARY TRADE SECRETS
 * Copyright 2001-2002 The MathWorks, Inc. 

 * CONFIDENTIAL AND CONTAINING PROPRIETARY TRADE SECRETS
 */

// Type definitions of proc addresses.
typedef long (*SccInitialize_PROC) (LPVOID * ppContext,             HWND hWnd,
                                    LPCSTR lpCallerName,            LPSTR lpSccName,
                                    LPLONG lpSccCaps,               LPSTR lpAuxPathLabel,
                                    LPLONG pnCheckoutCommentLen,    LPLONG pnCommentLen);

typedef long (*SccUninitialize_PROC) (LPVOID pContext);

typedef long (*SccOpenProject_PROC) (LPVOID pvContext,				HWND hWnd,
									LPSTR lpUser,                   LPSTR lpProjName,
									LPCSTR lpLocalProjPath,         LPSTR lpAuxProjPath,
									LPCSTR lpComment,               LPTEXTOUTPROC lpTextOutProc,
									LONG dwFlags);

typedef long (*SccGetProjPath_PROC) (LPVOID pvContext,				HWND hWnd,
                                    LPSTR lpUser,                   LPSTR lpProjName,
                                    LPSTR lpLocalPath,              LPSTR lpAuxProjPath,
                                    BOOL bAllowChangePath,          BOOL *pbNew);

typedef long (*SccCloseProject_PROC)(LPVOID pvContext);

typedef long (*SccGet_PROC)	    (LPVOID pvcontext,               HWND hWnd,
                                    LONG nFiles,                    LPCSTR *lpFileNames,
                                    LONG fOptions,            LPCMDOPTS pvOptions);

typedef long (*SccCheckout_PROC)   (LPVOID pvcontext,	            HWND hWnd,
                                    LONG nFiles,                    LPCSTR *lpFileNames,
                                    LPCSTR lpComment,		    LONG fOptions,   
				    LPCMDOPTS pvOptions);

typedef long (*SccCheckin_PROC)		(LPVOID pContext,               HWND hWnd,
                                    LONG nFiles,                    LPCSTR *lpFileNames,
                                    LPCSTR lpComment,               LONG fOptions,
                                    LPCMDOPTS pvOptions);

typedef long (*SccUncheckout_PROC)  (LPVOID pContext,				HWND hWnd,
                                    LONG nFiles,                    LPCSTR* lpFileNames,
                                    LONG fOptions,                   LPCMDOPTS pvOptions);

typedef long (*SccAdd_PROC)			(LPVOID pContext,               HWND hWnd,
                                    LONG nFiles,                    LPCSTR* lpFileNames,
                                    LPCSTR lpComment,               LONG * pdwFlags,
                                    LPCMDOPTS pvOptions);

typedef long (*SccRemove_PROC)		(LPVOID pContext,               HWND hWnd,
									LONG nFiles,                    LPCSTR* lpFileNames,
									LPCSTR lpComment,               LONG dwFlags,
									LPCMDOPTS pvOptions);

typedef long (*SccRename_PROC)		(LPVOID pContext,               HWND hWnd,
	                                LPCSTR lpFileName,              LPCSTR lpNewName);

typedef long (*SccDiff_PROC)		(LPVOID pContext,               HWND hWnd,
									LPCSTR lpFileName,               LONG fOptions,
									LPCMDOPTS pvOptions);

typedef long (*SccHistory_PROC)		(LPVOID pContext,               HWND hWnd,
									LONG nFiles,                    LPCSTR *lpFileNames,
									LONG fOptions,                  LPCMDOPTS pvOptions);


typedef long (*SccProperties_PROC)	(LPVOID pContext,				HWND hWnd,
									LPCSTR lpFileName);

typedef long (*SccQueryInfo_PROC)	(LPVOID pContext,				LONG nFiles,
									LPCSTR* lpFileNames,            LPLONG lpStatus);

typedef long (*SccGetCommandOptions_PROC) (LPVOID pContext,			HWND hWnd,
									enum SCCCOMMAND nCommand,       LPCMDOPTS *ppvOptions);

typedef long (*SccRunScc_PROC)		(LPVOID pContext,               HWND hWnd,
									LONG nFiles,                    LPCSTR* lpFileNames);
