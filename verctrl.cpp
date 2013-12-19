/*
*
* CONFIDENTIAL AND CONTAINING PROPRIETARY TRADE SECRETS
* Copyright 2001-2011 The MathWorks, Inc.
* The source code contained in this listing contains proprietary and
* confidential trade secrets of The MathWorks, Inc.  The use, modification,
* or development of derivative work based on the code or ideas obtained
* from the code is prohibited without the express written permission of The
* MathWorks, Inc. The disclosure of this code to any party not authorized
* by The MathWorks, Inc. is strictly forbidden.
* CONFIDENTIAL AND CONTAINING PROPRIETARY TRADE SECRETS
*/

#include "version.h"
#include <afx.h>
#include <windows.h>
#include <winreg.h>
#include <stdlib.h>
#include <memory>
#include <stdio.h>
#include <string>
#define snprintf _snprintf

#include "mex.h"
#include "scc.h"

#include "verctrl.h"
#include "verctrlUtil.h"
#include "resources/verctrl/verctrl.hpp"

#include "package.h"
#include "jmi.h"
#include "util.h"

#include "i18n/MessageCatalog.hpp"
#include "i18n/ustring.hpp"
#include "i18n/BaseMsgID.hpp"
#include "i18n/ustring_conversions.hpp"
#include "resources/MATLAB/sourceControl.hpp"


#define strcmpi _strcmpi

static char userName[SCC_USER_LEN + 1];
static void* context        = NULL;
static LONG capability      = 0x00000000L;
static LONG chkCommentLength;
static LONG cmtLen;
static HMODULE serverLib    = NULL;
static char currentWorkingFolder[_MAX_PATH];
// DLL to use instead of the Source Control Provider specified in the registry.
// For debugging purposes.
static char* gDebugDLL = NULL;

/* Since we can't store empty strings in Java hashtables, we
   need placeholder strings to represent empty project names
   and paths.  These are arbitrary, as long as they are
   never going to be valid project names or paths. */
static const char* empty_proj_placeholder = "##no_project_name##";
static const char* empty_path_placeholder = "##no_path##";

/* The name of the Source Control System in the Registry can
   be up to 2^31 bytes long, but more than about 128 would get
   difficult to display. */
#define REGISTRY_NAME_MAXLEN 128

static bool gVerboseMode = false;

/*
* Get number of SCC providers installed.
* This is found by looking at the registry entry under:
* HKEY_LOCAL_MACHINE\\Software\\SourceCodeControlProvider\\InstalledSCCProviders.
*/
static DWORD getNumberOfSCCSystems() {
    HKEY          hKey;
    long lResult  = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        "Software\\SourceCodeControlProvider\\InstalledSCCProviders",
        0, KEY_READ, &hKey);

    DWORD dwIndex = 0;            // Index in sub-key enumeration
    if (lResult == ERROR_SUCCESS) {
        while (lResult != ERROR_NO_MORE_ITEMS) {
            char tempVar[REGISTRY_NAME_MAXLEN];
            DWORD dwSizeName = REGISTRY_NAME_MAXLEN-1; // leave space for '\0'
            lResult = RegEnumValue(hKey, dwIndex, tempVar, &dwSizeName, NULL,
                                                          NULL, NULL, NULL);
            if (lResult == ERROR_NO_MORE_ITEMS)
                break;
            dwIndex++;
        }
    }
    return dwIndex;
}

/*
* Get all the SCC providers installed.
* This is found by looking at the registry entry under:
* HKEY_LOCAL_MACHINE\\Software\\SourceCodeControlProvider\\InstalledSCCProviders.
*/
static void getAllSCCSystems(char **sccProviders, int numberOfProviders) {
    HKEY          hKey;
    long lResult  = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        "Software\\SourceCodeControlProvider\\InstalledSCCProviders",
        0, KEY_READ, &hKey);

    if (lResult == ERROR_SUCCESS) {
        for (int i = 0; i < numberOfProviders; i++) {
            // Size of szName buffer/size of data returned.
            DWORD dwSizeName = REGISTRY_NAME_MAXLEN-1;
            lResult = RegEnumValue(hKey, i, sccProviders[i], &dwSizeName, NULL,
                                                           NULL, NULL, NULL);
            if (lResult == ERROR_NO_MORE_ITEMS) {
                // This would indicate that the registry had changed since
                // the call to getNumberOfSCCSystems.  Or that there was
                // a bug here somewhere.
                strcpy(sccProviders[i],"<name not found>");
            } else if (lResult == ERROR_MORE_DATA) {
                // Buffer too short
                strcpy(sccProviders[i],"<name too long>");
            }
        }
    }
}
// Clean up allmemory allocation from mxArrayToString
void cleanupScc(char *sccProviderName, char *sccRegKey)
{
    if (sccProviderName != NULL)
    {
        mxFree(sccProviderName);
        sccProviderName = NULL;
    }
    if (sccRegKey != NULL)
    {
        mxFree(sccRegKey);
        sccRegKey = NULL;
    }
}

/*
* Identifies the source control provider which should be used, according
* to the Windows registry.
* The returned string should be freed using mxFree.
*/
static char* identifySCCSystem(SCCARGS *sccArgs) {
    char *sccProviderName   = NULL;
    char *sccRegKey         = NULL;
    char *libPath           = NULL;
    mxArray     *prhs[3] = {NULL, NULL, NULL};
    mxArray     *plhs[1] = {NULL};
    mxArray     *rhs[3]  = {NULL, NULL, NULL};
    mxArray     *lhs[1]  = {NULL};
	const fl::i18n::BaseMsgID& msgid  = MATLAB::sourceControl::none() ; 
	const fl::ustring Msg = fl::i18n::MessageCatalog::get_message(msgid);
	std::string sMsg = fl::i18n::to_string(Msg);

    if (gVerboseMode) mexPrintf("verctrl: identifySCCSystem\n");

    // Get the default source control system by calling matlab.
    mxArray *plhs1[1];
    mexSetTrapFlag(1);

    int status       = mexCallMATLAB(1, plhs1, 0, NULL, "cmopts");
    if (status != 0) 
    {
        if (gVerboseMode) mexPrintf("verctrl: error calling cmopts\n");
        cleanupScc(sccProviderName, sccRegKey);
		throwMatlabError(sccArgs,verctrl::verctrl::NoProvider());
    }
    
   sccProviderName = mxArrayToString(plhs1[0]);
   if (gVerboseMode) mexPrintf("verctrl: cmopts returned SCC provider name \"%s\"\n", sccProviderName);
   

   if (strcmpi(sccProviderName,sMsg.c_str()) == 0)
   {
        cleanupScc(sccProviderName, sccRegKey);
		throwMatlabError(sccArgs,verctrl::verctrl::ProviderNotSelected());
   }

   // Query the registry to get the location of the SCC Provider registry entry.
   prhs[0]          = mxCreateString("HKEY_LOCAL_MACHINE");
   prhs[1]          = mxCreateString("Software\\SourceCodeControlProvider\\InstalledSCCProviders");
   prhs[2]          = mxCreateString(sccProviderName);

   if (prhs[0] == NULL || prhs[1] == NULL || prhs[2] == NULL)
   {
        cleanupScc(sccProviderName, sccRegKey);
		throwMatlabError(sccArgs,verctrl::verctrl::MemoryError());
   }

   mexSetTrapFlag(1);
   status           = mexCallMATLAB(1, plhs, 3, prhs, "winqueryreg");
   if (status != 0)
   {
        if (gVerboseMode) mexPrintf("verctrl: error calling winqueryreg\n");
        cleanupScc(sccProviderName, sccRegKey);
        throwMatlabError(sccArgs,verctrl::verctrl::ProviderNotInstalled());
   }
  
    sccRegKey = mxArrayToString(plhs[0]);
   if (gVerboseMode) mexPrintf("verctrl: winqueryreg returned key name \"%s\"\n", sccRegKey);

    // Step 2: Query the registry again to find the location of the SCC provider dll.

    rhs[0]           = mxCreateString("HKEY_LOCAL_MACHINE");
    rhs[1]           = mxCreateString(sccRegKey);
    rhs[2]           = mxCreateString(STR_SCCPROVIDERPATH);

    if (rhs[0] == NULL || rhs[1] == NULL || rhs[2] == NULL)
    {
        cleanupScc(sccProviderName, sccRegKey);
		throwMatlabError(sccArgs,verctrl::verctrl::MemoryError());
    }
    // a value of 1 return control here to the mex dll on error, instead of default 0 which 
    mexSetTrapFlag(1);
    status = mexCallMATLAB(1, lhs, 3, rhs, "winqueryreg");
    if (status != 0)
    {
        if (gVerboseMode) mexPrintf("verctrl: error calling winqueryreg\n");
        cleanupScc(sccProviderName, sccRegKey);
		throwMatlabError(sccArgs,verctrl::verctrl::NoProvider());
    }
    libPath = mxArrayToString(lhs[0]);
	if (gVerboseMode) mexPrintf("verctrl: winqueryreg returned library path \"%s\"\n", libPath);
    return libPath;
}


void startSCCSystem(SCCARGS* sccArgs, char* libPath) {

    if (gVerboseMode) mexPrintf("Attempting to load library \"%s\"\n", libPath);

    // Step 3: Load the DLL
    serverLib       = LoadLibrary(libPath);
    if (serverLib == NULL)
    {
        if (gVerboseMode) mexPrintf("Failed to load library \"%s\"\n", libPath);
        mxFree(libPath);
		throwMatlabError(sccArgs,verctrl::verctrl::ProviderFailedToLoad());
    }
    if (gVerboseMode) mexPrintf("Finished loading library \"%s\"\n", libPath);

    mxFree(libPath);

    // Step 4: Initialize the SCC provider.
    char axPath[SCC_PRJPATH_LEN + 1];
    char sccName[SCC_NAME_LEN + 1];
    char projName[SCC_PRJPATH_LEN + 1];

    userName[0]     = '\0';
    axPath[0]       = '\0';
    projName[0]     = '\0';
    sccName[0]      = '\0';
    currentWorkingFolder[0] = '\0';

    // get the user name from the environment, it's used when opening projects
    if (strlen(userName) == 0) {
        const char *usrNm = getenv("USER");
        if (usrNm != NULL) {
            if (strlen(usrNm) <= SCC_USER_LEN) {
                strcpy(userName, usrNm);
            }
        }
    }
    if (gVerboseMode) mexPrintf("Attempting to SccInitialize\n");

    SCCRTN rtn      = (*(SccInitialize_PROC) GetProcAddress(serverLib, "SccInitialize"))
        (&context, sccArgs->WindowHandle, "MATLAB", sccName, &capability, axPath,
        &chkCommentLength, &cmtLen);
    if (IS_SCC_ERROR(rtn)) {
		if (gVerboseMode) mexPrintf("verctrl: SCC provider failed to initialize: %s\n",
			errorCodeToString(rtn));
        FreeLibrary(serverLib);
        serverLib   = NULL;
		throwMatlabError(sccArgs,verctrl::verctrl::FailedToInitialize());
    }

    // clean up - do not free sccArgs in this case, it is not an error condition path
    if (gVerboseMode) mexPrintf("verctrl: SCC provider initialized successfully\n");
}


void loadSCCSystem(SCCARGS* sccArgs) {

    if (serverLib != NULL) {
        return;
    }

    if (gDebugDLL != NULL) {
        // There's no mxStrdup
        mxArray* temp = mxCreateString(gDebugDLL);
        char* copy = mxArrayToString(temp);
        startSCCSystem(sccArgs, copy);
    } else {
       char* libPath = identifySCCSystem(sccArgs);
       startSCCSystem(sccArgs,libPath); 
    }
}

/*
* Close the currently opened project if any.
*/
static void closeProject() {
    FARPROC proc    = GetProcAddress(serverLib, "SccCloseProject");
    if (gVerboseMode) mexPrintf("verctrl: closing current project\n");
    (*(SccCloseProject_PROC) proc) (context);
}

/*
* Unload the source control system library.
*/
static void unloadSCCSystem() {
    if (serverLib == NULL) {
        return;
    }
    else {
        if (gVerboseMode) mexPrintf("verctrl: Unloading SCC DLL\n");
        closeProject();
        FARPROC proc    = GetProcAddress(serverLib, "SccUninitialize");
        (*(SccUninitialize_PROC) proc) (context);
        FreeLibrary(serverLib);
        serverLib = NULL;
    }
}

/*
* Open the given project. This will close any existing open projects.
*/
static SCCRTN openProjFromSavedInfo(SCCARGS *sccArgs, HWND hWnd) {
    SCCRTN rtn       = SCC_E_INITIALIZEFAILED;

    char localDir[_MAX_PATH];
    getParentPath(sccArgs->FileNames[0], localDir);
    if (strcmp(localDir, currentWorkingFolder) == 0) {
        if (gVerboseMode) mexPrintf("verctrl: (openProjFromSavedInfo) already in this folder\n");
        rtn = SCC_OK;
    }
    else {
        // Query matlab to get the projectName, lpAuxProjPath.
        mxArray *prhs[1] = {NULL};
        prhs[0]          = mxCreateString(localDir);
        if (prhs[0] == NULL) {
			throwMatlabError(sccArgs,verctrl::verctrl::MemoryError());
        }
        mxArray    *plhs[2] = {NULL, NULL};
        mexSetTrapFlag(1);
        int status       = mexCallMATLAB(2, plhs, 1, prhs, "getsccprj");
        if (status != 0 || plhs[0] == NULL || plhs[1] == NULL) {
            if (gVerboseMode) mexPrintf("verctrl: error calling getsccprj\n");
			throwMatlabError(sccArgs,verctrl::verctrl::NoProvider());
        }
        if (!mxIsEmpty(plhs[0])) { //Previously saved.
            char axPath[SCC_PRJPATH_LEN + 1];
            char projName[SCC_PRJPATH_LEN + 1];
            axPath[0]        = '\0';
            projName[0]      = '\0';

            closeProject();
            // update for 64 bit mxarrays, cast to int.  Never will have 64 bit project name
            int prjNmLth     = static_cast<int>(mxGetNumberOfElements(plhs[0])) + 1;
            mxGetString(plhs[0], projName, prjNmLth);
            if (strlen(projName) == 0) {
                strcpy(projName, const_cast<const char *>(localDir));
            }
            // update for 64 bit mxarrays, cast to int.  Never will have 64 bit aux path name 
            int axPthLth =  static_cast<int>(mxGetNumberOfElements(plhs[1])) + 1;
			mxGetString(plhs[1], axPath, axPthLth);

            if (!utStrcmp(projName,empty_proj_placeholder)) {
                /* The string matches our placeholder for an empty
                   project name.  Replace with an empty string. */
                projName[0] = '\0';
            }

            if (!utStrcmp(axPath,empty_path_placeholder)) {
                /* The string matches our placeholder for an empty
                   path.  Replace with an empty string. */
                axPath[0] = '\0';
            }

            rtn =  (*(SccOpenProject_PROC) GetProcAddress(serverLib, "SccOpenProject"))
                (context, hWnd, userName, projName, localDir,
                axPath, "", NULL, SCC_OP_SILENTOPEN & ~SCC_OP_CREATEIFNEW);
            if (IS_SCC_SUCCESS(rtn)) {
                if (gVerboseMode) mexPrintf("verctrl: (openProjFromSavedInfo) current working folder is now \"%s\"\n", localDir);
                strcpy(currentWorkingFolder, localDir);
            }
            else if (IS_SCC_ERROR(rtn)) {
                if (gVerboseMode) mexPrintf("verctrl: (openProjFromSavedInfo) error calling SccOpenProject\n", localDir);
                currentWorkingFolder[0] = '\0';
            }

            // Clean up
            mxDestroyArray(prhs[0]);
            mxDestroyArray(plhs[0]);
            mxDestroyArray(plhs[1]);
        } else if (gVerboseMode) {
            mexPrintf("verctrl: (openProjFromSavedInfo) No project name stored\n");
        }
    }

    return rtn;
}

/*
* Open project for the given folder based on the saved info or prompt to select a SCC project.
*/
static SCCRTN promptAndOpenProject(const char *localFile, HWND hWnd) {
    char axPath[SCC_PRJPATH_LEN + 1];
    char projName[SCC_PRJPATH_LEN + 1];
    char localDir[_MAX_PATH];
    axPath[0]        = '\0';
    projName[0]     = '\0';
    BOOL pbNew      = false;

    if (gVerboseMode) mexPrintf("verctrl: promptAndOpenProject\n");

    getParentPath(localFile, localDir);
    SCCRTN rtn      = (*(SccGetProjPath_PROC) GetProcAddress(serverLib, "SccGetProjPath"))
        (context, hWnd, userName, projName, localDir,
        axPath, false, &pbNew);
    if (IS_SCC_SUCCESS(rtn)) {
        if (strlen(projName)==0) {
            /* Since we can't handle empty project names, use the
               placeholder. */
            strcpy(projName,empty_proj_placeholder);
        }
        if (strlen(axPath)==0) {
            /* Since we can't handle empty paths, use the
               placeholder. */
            strcpy(axPath,empty_path_placeholder);
        }
        if (gVerboseMode) mexPrintf("verctrl:  SccGetProjPath succeeded.\n"
	 			"Project name \"%s\", AuxPath \"%s\"\n", projName, axPath);
        closeProject();
        rtn         = (*(SccOpenProject_PROC) GetProcAddress(serverLib, "SccOpenProject"))
            (context, hWnd, userName, projName, localDir,
            axPath, "", NULL, SCC_OP_SILENTOPEN & ~SCC_OP_CREATEIFNEW);
        if (IS_SCC_SUCCESS(rtn)) {// Save results back in matlab.
	        if (gVerboseMode) mexPrintf("verctrl:  SccOpenProject succeeded.\n"
				"Saving project info for dicrectory \"%s\"\n", localDir);
            mxArray *rhs[3] = {NULL, NULL, NULL};
            rhs[0]          = mxCreateString(localDir);
            rhs[1]          = mxCreateString(projName);
            rhs[2]          = mxCreateString(axPath);
            mexSetTrapFlag(1);
            mexCallMATLAB(0, NULL, 3, rhs, "savesccprj");

            // Clean up.
            mxDestroyArray(rhs[0]);
            mxDestroyArray(rhs[1]);
            mxDestroyArray(rhs[2]);
        }
    }

    if (IS_SCC_SUCCESS(rtn)) {
        if (gVerboseMode) mexPrintf("verctrl: (promptAndOpenProject) current working folder is now \"%s\"\n", localDir);
        strcpy(currentWorkingFolder, localDir);
    }
    return rtn;
}

/*
* Add a new file into the source code control system.
*/
static bool add(SCCARGS *sccArgs) {
    bool reload     = true;
    if (!sccArgs->Quiet) {
        reload      = showSCCUI(sccArgs, capability, cmtLen);
    }
    if (reload) {
        LONG *fOptions  = (LONG*)mxCalloc(sccArgs->NumberOfFiles, sizeof(LONG));
        for (int i = 0; i < sccArgs->NumberOfFiles; i++)
            fOptions[i] = sccArgs->KeepCheckout ? SCC_KEEP_CHECKEDOUT : 0;
        int rtn         = (*(SccAdd_PROC) GetProcAddress(serverLib, "SccAdd"))
            (context, sccArgs->WindowHandle, sccArgs->NumberOfFiles,
            const_cast<const char **>(sccArgs->FileNames),
            sccArgs->Comment, fOptions, NULL);

         // Throw error if necessary
        if (IS_SCC_ERROR(rtn))
           throwSccError(sccArgs,rtn);
    }
    return reload;
}

/*
* Retrive a copy of a file for viewing and compling, but not editing.
*/
static bool get(SCCARGS *sccArgs) {
    bool reload     = true;
    if (!sccArgs->Quiet) {
        reload      = showSCCUI(sccArgs, capability, cmtLen);
    }
    if (reload) {
        LONG fOptions = 0;
        int rtn         = (*(SccGet_PROC) GetProcAddress(serverLib, "SccGet"))
            (context, sccArgs->WindowHandle, sccArgs->NumberOfFiles,
            const_cast<const char **>(sccArgs->FileNames),
            fOptions, NULL);

        // Throw error if necessary
        if (IS_SCC_ERROR(rtn))
            throwSccError(sccArgs, rtn);
     }
    return reload;
}

/*
* Retrive a copy of the file for editing.
*/
static bool checkout(SCCARGS *sccArgs) {
    bool reload     = true;
    if (!sccArgs->Quiet) {
        reload = showSCCUI(sccArgs, capability, chkCommentLength);
    }
    if (reload) {
        LONG fOptions = 0;
        int rtn         = (*(SccCheckout_PROC) GetProcAddress(serverLib, "SccCheckout"))
            (context, sccArgs->WindowHandle, sccArgs->NumberOfFiles,
            const_cast<const char **>(sccArgs->FileNames),
            sccArgs->Comment, fOptions, NULL);

        // Throw error if necessary
        if (IS_SCC_ERROR(rtn))
            throwSccError(sccArgs, rtn);
    }
    return reload;
}

/*
* This operation checks checked-out files back into SCC system, storing the changes
* and creating a new version.
*/
static bool checkin(SCCARGS *sccArgs) {
    bool reload = true;
    if (!sccArgs->Quiet) {
        reload = showSCCUI(sccArgs, capability, cmtLen);
    }
    if (reload) {
        LONG fOptions = sccArgs->KeepCheckout ?  SCC_KEEP_CHECKEDOUT : 0;
        int rtn       = (*(SccCheckin_PROC) GetProcAddress(serverLib, "SccCheckin"))
            (context, sccArgs->WindowHandle, sccArgs->NumberOfFiles,
            const_cast<const char **>(sccArgs->FileNames),
            sccArgs->Comment, fOptions, NULL);

        if (IS_SCC_ERROR(rtn))
            throwSccError(sccArgs, rtn);
    }
    return reload;
}

/*
* Cancel a previous check-out operation restoring the contents of the selected file
* or files to the way they ere before the checkout. All changes made to the file
* since the checkout were lost.
*/
static bool uncheckout(SCCARGS *sccArgs) {
    bool reload     = true;
    if (!sccArgs->Quiet) {
        reload      = showSCCUI(sccArgs, capability, cmtLen);
    }
    if (reload) {
        LONG fOptions = 0;
        int rtn         = (*(SccUncheckout_PROC) GetProcAddress(serverLib, "SccUncheckout"))
            (context, sccArgs->WindowHandle, sccArgs->NumberOfFiles,
            const_cast<const char **>(sccArgs->FileNames),
            fOptions, NULL);

        // Throw error if necessary
        if (IS_SCC_ERROR(rtn))
            throwSccError(sccArgs, rtn);
    }
    return reload;
}

/*
* Remove files from source control system.
*/
static void remove(SCCARGS *sccArgs) {
    if (!sccArgs->Quiet) {
        bool approved = showSCCUI(sccArgs, capability, cmtLen);
        if (!approved)
            return;
    }

    LONG fOptions = 0;
    int rtn         = (*(SccRemove_PROC) GetProcAddress(serverLib, "SccRemove"))
        (context, sccArgs->WindowHandle, sccArgs->NumberOfFiles,
        const_cast<const char **>(sccArgs->FileNames),
        sccArgs->Comment, fOptions, NULL);

    // Throw error if necessary
    if (IS_SCC_ERROR(rtn))
       throwSccError(sccArgs, rtn);
}

/*
* Is there any differences between working copy and latest version of a file.
*/
static bool isDiff(char *fileName, HWND windowHandle) {
    int rtn         = (*(SccDiff_PROC) GetProcAddress(serverLib, "SccDiff"))
        (context, windowHandle, fileName, SCC_DIFF_QD_CHECKSUM, NULL);
    return (rtn == SCC_I_FILEDIFFERS);
}

static int isFileDiff(char *fileName, HWND windowHandle) {
    int rtn         = (*(SccDiff_PROC) GetProcAddress(serverLib, "SccDiff"))
        (context, windowHandle, fileName, SCC_DIFF_QD_CHECKSUM, NULL);
    return (rtn);
}
/*
* Display differences between the working copy and latest version of a file.
*/
static void showDiff(SCCARGS *sccArgs) {
    int rtn;
    rtn = isFileDiff(sccArgs->FileNames[0], sccArgs->WindowHandle);
    if (rtn == SCC_I_FILEDIFFERS) 
    {
         rtn = (*(SccDiff_PROC) GetProcAddress(serverLib, "SccDiff"))
            (context, sccArgs->WindowHandle, sccArgs->FileNames[0], SCC_DIFF_IGNORESPACE, NULL);
        if (IS_SCC_ERROR(rtn))
            throwSccError(sccArgs, rtn);
    }
    else {
        // note: we are not doing anything special with error returns SCC_E_FILENOTCONTROLLED, SCC_E_NOTAUTHORIZED, 
        // SCC_E_ACCESSFAILURE or SCC_E_NONSPECIFICERROR
        // from Gentile Giacomo [Giacomo.Gentile@bologna.marelli.it]:
        // finally R&D of Telelogic in Irvine (CA) provide us a
        // possible reasons for the observed behaviour.
        // When the ccm.ini file is set to use the graphical merge and
        // the file are equal SCCI Telelogic ScciDiff function return the value:
        // SCC_E_NONSPECIFICERROR
        // They do not want to see the following message when a graphical merge took place.
        if (rtn == SCC_OK)
			throwMatlabError(sccArgs, verctrl::verctrl::DiffError());
    }
}

/*
* Displays the histroy of the passed file or files.
* Return true if the file has changed and needs to be reloaded.
*/
static bool history(SCCARGS *sccArgs){
    int rtn            = (*(SccHistory_PROC) GetProcAddress(serverLib, "SccHistory"))
        (context, sccArgs->WindowHandle, sccArgs->NumberOfFiles, 
         const_cast<LPCSTR*>(sccArgs->FileNames), 0x0, NULL);
    if (rtn == SCC_I_RELOADFILE)
        return true;
    else if (IS_SCC_ERROR(rtn))
        throwSccError(sccArgs, rtn);
    return false;
}

/*
* Display version specific properties of the passed file.
* Return true if the file has changed and needs to be reloaded.
*/
static bool properties(SCCARGS *sccArgs) {
    int rtn         = (*(SccProperties_PROC) GetProcAddress(serverLib, "SccProperties"))
        (context, sccArgs->WindowHandle, sccArgs->FileNames[0]);
    if (rtn == SCC_I_RELOADFILE)
        return true;
    else if (IS_SCC_ERROR(rtn))
        throwSccError(sccArgs, rtn);
    return false;
}

/*
* Get the status of a file.
*/
static int fileStatus(char **fileNames, const int numberOfFiles, LPLONG fileStatus) {
    int ret = (*(SccQueryInfo_PROC) GetProcAddress(serverLib, "SccQueryInfo"))(context, numberOfFiles,
        const_cast<const char **>(fileNames), fileStatus);
	if (gVerboseMode) {
		mexPrintf("verctrl: fileStatus\n");
		for (int i=0; i<numberOfFiles; i++) {
			std::string x;
			statusToString(fileStatus[i],x);
			mexPrintf("  %s  :  %s\n", fileNames[i], x.c_str());
		}
	}
	return ret;
}

/*
* Invoke the source code control system.
*/
static int runScc(HWND windowHandle) {
    return (*(SccRunScc_PROC) GetProcAddress(serverLib, "SccRunScc"))
        (context, windowHandle, 0, NULL);
}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    if (!(jmiUseJVM() && jmiUseSwing() && jmiUseMWT())) { // Java not available fully
		throwMatlabError(NULL,verctrl::verctrl::NoJava());
    }
    SCCARGS * sccArgs = (SCCARGS *) mxCalloc(1, sizeof(SCCARGS));
    constructInputArgs(nrhs, prhs, sccArgs);

    if (serverLib == NULL) {
        mexAtExit(unloadSCCSystem);
    }

    if (gVerboseMode) mexPrintf("verctrl: %s\n", sccArgs->Command);

    if (strcmpi("ALL_SYSTEMS", sccArgs->Command) == 0) {
        DWORD numberOfProviders = getNumberOfSCCSystems();
        char **sccProviders   = (char **)mxCalloc(numberOfProviders, sizeof(char *));
        for (DWORD i = 0; i < numberOfProviders; i++) {
            char *sccProviderName = (char *)mxCalloc(REGISTRY_NAME_MAXLEN,
                           sizeof(char));
            sccProviders[i]   = sccProviderName;
        }
        getAllSCCSystems(sccProviders, numberOfProviders);
        mxArray *providers    = mxCreateCellMatrix(numberOfProviders, 1);
        if (providers == NULL)
			throwMatlabError(sccArgs,verctrl::verctrl::MemoryError());
        else {
            for (DWORD j = 0; j < numberOfProviders; j++) {
                mxArray *val = mxCreateString(sccProviders[j]);
                if (val == NULL)
					throwMatlabError(sccArgs, verctrl::verctrl::MemoryError());
                mxSetCell(providers, j, val);
            }
            plhs[0] = providers;
        }
    }
    else if (strcmpi("CAPABILITY", sccArgs->Command) == 0) { // For developing the UI menus.
        loadSCCSystem(sccArgs);
        mxArray *result = mxCreateDoubleScalar (capability);
        if (result == NULL) {
			throwMatlabError(sccArgs, verctrl::verctrl::MemoryError());
        }
        plhs[0]         = result;
    }
    else if (strcmpi("RUNSCC", sccArgs->Command) == 0) {
        // Error checking
		if (sccArgs->WindowHandle == NULL) {
            throwMatlabError(sccArgs, verctrl::verctrl::BadWindowHandle());
		}
        loadSCCSystem(sccArgs);
        runScc(sccArgs->WindowHandle);
    }
    else if (strcmpi("REGISTER", sccArgs->Command) == 0) 
    {
        // Error checking
		if (sccArgs->WindowHandle == NULL) {
			throwMatlabError(sccArgs,verctrl::verctrl::InvalidHandle());
		} else if (sccArgs->FileNames == NULL) {
			throwMatlabError(sccArgs,verctrl::verctrl::NoDirectory());
		}
        loadSCCSystem(sccArgs);
        SCCRTN rtn = promptAndOpenProject(sccArgs->FileNames[0], sccArgs->WindowHandle);
        if (rtn == SCC_I_OPERATIONCANCELED) 
        {
			// No need to report an error to the user here
            if (gVerboseMode) mexPrintf("verctrl:  SCC_I_OPERATIONCANCELED\n");
        }
        else if (!IS_SCC_SUCCESS(rtn)) {
			if (gVerboseMode) mexPrintf("verctrl: %s\n", errorCodeToString(rtn));
            throwSccError(sccArgs, rtn);
        }
        // return a successful result
        if (nlhs >= 1) {
            mxArray *result = mxCreateLogicalScalar(false);
            if (result == NULL)
            {
				throwMatlabError(sccArgs,verctrl::verctrl::MemoryError());
            }
            plhs[0]     = result;
        }
        cleanupInputArgs(sccArgs);
        return;
    }
    else if (strcmpi("UNLOAD", sccArgs->Command) == 0) {
        unloadSCCSystem();
    }
    else if (strcmpi("STATUS", sccArgs->Command) == 0) {
        // Error checking
        if (sccArgs->FileNames == NULL) {
 			throwMatlabError(sccArgs, verctrl::verctrl::NoFiles(sccArgs->Command));
        }
        if (sccArgs->WindowHandle == NULL)
			throwMatlabError(sccArgs, verctrl::verctrl::InvalidHandle());

        LPLONG status   = (LPLONG)mxCalloc(sccArgs->NumberOfFiles, sizeof(LONG));
        if (status == NULL)
			throwMatlabError(sccArgs, verctrl::verctrl::MemoryError());

        loadSCCSystem(sccArgs);

        // see if we've opened this project before
        SCCRTN  rtn    = openProjFromSavedInfo(sccArgs, sccArgs->WindowHandle);
        if (IS_SCC_ERROR(rtn)) {
            for (int i = 0; i < sccArgs->NumberOfFiles; i++) {
                // SCC_STATUS_NO_MATLAB_PROJECT is TMW defined in scc.h.  Is an enum that extends microsoft supplied 
                // include file.  If we use a newer version of scc.h, then the build will break here and we'll need to add
                // it to the SccStatus enum.
                status[i] = SCC_STATUS_NO_MATLAB_PROJECT;
                if (gVerboseMode) mexPrintf("verctrl:  openProjFromSavedInfo failed (%s, %s)\n",
					sccArgs->FileNames[i], errorCodeToString(rtn));
            }
        }
        else {
            fileStatus(sccArgs->FileNames, sccArgs->NumberOfFiles, status);
        }
        mxArray *statusArray = mxCreateNumericMatrix(1, sccArgs->NumberOfFiles, mxUINT32_CLASS,  mxREAL);
        if (statusArray == NULL) {
			throwMatlabError(sccArgs,verctrl::verctrl::MemoryError());
        }
        unsigned int *arrayData = (unsigned int *)mxGetData(statusArray);
        for (int i = 0; i < sccArgs->NumberOfFiles; i++) {
            arrayData[i] = status[i];
        }
        plhs[0]     = statusArray;
    } else if (strcmpi("VERBOSE_ON", sccArgs->Command) == 0) {
        gVerboseMode = true;
        mexPrintf("verctrl: Verbose mode on\n");
    } else if (strcmpi("VERBOSE_OFF", sccArgs->Command) == 0) {
        mexPrintf("verctrl: Verbose mode off\n");
        gVerboseMode = false;
    } else if (strcmpi("SET_DLL", sccArgs->Command) == 0) {
		/* undocumented command used for interal troubleshooting, errors do not need translation*/
        unloadSCCSystem();
        char* dll = mxArrayToString(prhs[1]);
        if (gDebugDLL!=NULL) {
            mxFree(gDebugDLL);
        }
        if (dll==NULL || dll[0]=='\0') {
            mexPrintf("verctrl: Clearing Debug DLL name\n");
            gDebugDLL = NULL;
        } else {
            mexPrintf("verctrl: Using DLL \"%s\"\n", dll);
            gDebugDLL = dll;
        }
    } else {
        // Error checking
        if (sccArgs->FileNames == NULL) {
			throwMatlabError(sccArgs, verctrl::verctrl::NoFiles(sccArgs->Command));
        }
        if (sccArgs->WindowHandle == NULL)
			throwMatlabError(sccArgs,  verctrl::verctrl::BadWindowHandle());

        loadSCCSystem(sccArgs);

        SCCRTN rtn = openProjFromSavedInfo(sccArgs, sccArgs->WindowHandle);
        if (!IS_SCC_SUCCESS(rtn)) 
        {
            if (gVerboseMode) mexPrintf("verctrl:  openProjFromSavedInfo failed (%s)\n",
				sccArgs->FileNames[0], errorCodeToString(rtn));
            rtn = promptAndOpenProject(sccArgs->FileNames[0], sccArgs->WindowHandle);
            if (rtn == SCC_I_OPERATIONCANCELED) 
            {
                if (nlhs >= 1) 
                {
                    mxArray *result = mxCreateLogicalScalar(false);
                    if (result == NULL)
						throwMatlabError(sccArgs,verctrl::verctrl::MemoryError());
                    plhs[0]     = result;
                }
                cleanupInputArgs(sccArgs);
                return;
            }
            else if (!IS_SCC_SUCCESS(rtn)) 
            {
				if (gVerboseMode) mexPrintf("verctrl:  promptAndOpenProject failed (%s)\n",
					sccArgs->FileNames[0], errorCodeToString(rtn));
                throwSccError(sccArgs,rtn);
            }
        }

        bool reload = false;

        if (strcmpi(sccArgs->Command, "ADD") == 0) {
            reload = add(sccArgs);
        }
        else if (strcmpi(sccArgs->Command, "CHECKOUT") == 0) {
            reload = checkout(sccArgs);
        }
        else if (strcmpi(sccArgs->Command, "CHECKIN") == 0) {
            reload = checkin(sccArgs);
        }
        else if (strcmpi(sccArgs->Command, "GET") == 0) {
            reload = get(sccArgs);
        }
        else if (strcmpi(sccArgs->Command, "UNCHECKOUT") == 0) {
            reload = uncheckout(sccArgs);
        }
        else if (strcmpi(sccArgs->Command, "REMOVE") == 0) {
            remove(sccArgs);
        }
        else if (strcmpi(sccArgs->Command, "SHOWDIFF") == 0) {
            showDiff(sccArgs);
        }
        else if (strcmpi(sccArgs->Command, "ISDIFF") == 0) {
            reload       = isDiff(sccArgs->FileNames[0], sccArgs->WindowHandle);
        }
        else if (strcmpi(sccArgs->Command, "HISTORY") == 0) {
            reload     = history(sccArgs);
        }
        else if (strcmpi(sccArgs->Command, "PROPERTIES") == 0) {
            reload     = properties(sccArgs);
        }
        else {
            char warnTxt[128];
            sprintf(warnTxt, "Not a valid command: %s", sccArgs->Command);
            mexWarnMsgTxt(warnTxt);
        }
        if (nlhs >= 1) {
            mxArray *result = mxCreateLogicalScalar(reload);
            if (result == NULL)
				throwMatlabError(sccArgs,verctrl::verctrl::MemoryError());
            plhs[0]     = result;
        }
    }
    cleanupInputArgs(sccArgs);
 }
  

