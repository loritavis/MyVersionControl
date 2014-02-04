function varargout = verctrl(varargin)                                          
%VERCTRL Version control operations on Windows platforms 2
%   List = VERCTRL('all_systems') returns a list of all of the version control 
%   systems installed in the current machine.
%  
%   fileChange = VERCTRL(COMMAND,FILENAMES,HANDLE) performs the version control
%   operation specified by COMMAND on FILENAMES, which is a cell array of files.
%   HANDLE is a window handle; use a value of 0.  
%   These commands return a logical 1 to the workspace if the file has
%   changed on disk or a logical 0 to the workspace if the file has not
%   changed on disk.   
%   Available values for COMMAND that can be used with the FILENAMES argument:
% 
%       'get'           Retrieves a file or files for viewing and compiling, but 
%                       not editing. The file or files will be tagged read-only. 
%                       The list of files should contain either files or directories 
%                       but not both.
%   
%       'checkout'      Retrieves a file or files for editing.
%                                             
%       'checkin'       Checks a file or files into the version control system,
%                       storing the changes and creating a new version.                        
%
%       'uncheckout'    Cancels a previous check-out operation and restores the 
%                       contents of the selected file or files to the precheckout version.
%                       All changes made to the file since the checkout are lost.
%                       
%       'add' 		    Adds a file or files into the version control system.                  
%
%       'history'       Displays the history of a file or files. 
%
%   VERCTRL(COMMAND,FILENAMES,HANDLE) performs the version control
%   operation specified by COMMAND on FILENAMES, which is a cell array of files.
%   HANDLE is a window handle; use a value of 0.
%   Available values for COMMAND that can be used with the FILENAMES argument:
%
%       'remove'        Removes a file or files from the version control system. 
%                       It does not delete the file from the local hard drive, 
%                       only from the version control system.
% 
%   fileChange = VERCTRL(COMMAND,FILE,HANDLE) performs the version control operation
%   specified by COMMAND on FILE, which is a single file. HANDLE is a window handle;  
%   use a value of 0.  These commands return a logical 
%   1 to the workspace if the file has changed on  disk or a logical 0 to
%   the workspace if the file has not changed on disk.   
%   Available values for COMMAND that can be used with the FILENAMES argument:
% 
%       'isdiff'        Compares a file with the latest checked in version 
%                       of the file in the version control system. Returns 
%                       1 if the files are different and it returns 0 if the 
%                       files are identical.    
%
%   VERCTRL(COMMAND,FILE,HANDLE) performs the version control operation
%   specified by COMMAND on FILE, which is a single file. HANDLE is a window handle;  
%   use a value of 0.  
%   Available values for COMMAND that can be used with the FILENAMES argument:
%                      
%       'showdiff'      Displays the differences between a file and the latest checked in 
%                       version of the file in the version control system. 
%
%       'properties'    Displays the properties of a file. 
%                      
%   Examples:  
%       Return a list in the command window of all version control systems 
%       installed in the machine.
%       List = verctrl('all_systems')
%       List =     
%               'Microsoft Visual SourceSafe'
%               'Jalindi Igloo'
%               'PVCS Source Control'
%               'ComponentSoftware RCS'   
%      
%       Check out D:\file1.ext from the version control system. This command
%       opens 'checkout' window and returns a logical 1 to the workspace if the 
%       file has changed on disk or a logical 0 to the workspace if 
%       the file has not changed on disk.
%       fileChange = verctrl('checkout',{'D:\file1.ext'},0)
%     
%       Add D:\file1.ext and D:\file2.ext to the version control system.
%       This command opens 'add' window and returns a logical 1 to the workspace if the 
%       file has changed on disk or a logical 0 to the workspace if 
%       the file has not changed on disk.
%       fileChange = verctrl('add',{'D:\file1.ext','D:\file2.ext'},0)
%     
%       Display the properties of D:\file1.ext. This command opens 'properties'
%       window.
%       verctrl('properties','D:\file1.ext',0)
%   
%   See also CHECKIN, CHECKOUT, UNDOCHECKOUT, CMOPTS 
% making changes   
%   Copyright 1998-2004 The MathWorks, Inc.
%

