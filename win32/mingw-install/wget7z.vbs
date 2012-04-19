' THIS IS THE HORROR!!!!!!!'
' incvoke with cscript setup.vbs c:\WHATEVER

WGETURL = "http://osspack32.googlecode.com/files/wget.exe"
SZIPURL = "http://www.mirrorservice.org/sites/download.sourceforge.net/pub/sourceforge/s/project/se/sevenzip/7-Zip/9.20/7za920.zip"

TARGET = WScript.Arguments.Item(0)

'download files
SaveWebBinary WGETURL, TARGET & "\wget.exe"
SaveWebBinary SZIPURL, TARGET & "\7za920.zip"

'unzip 7z
Unzip TARGET & "\7za920.zip", TARGET

'delete leftovers
Set objFSO = CreateObject("Scripting.FileSystemObject")
objFSO.DeleteFile(TARGET & "\7za920.zip")
objFSO.DeleteFile(TARGET & "\7-zip.chm")
objFSO.DeleteFile(TARGET & "\*.txt")
Set objFSO = Nothing




Function Unzip(strFileName,strFolderName)
Dim objshell
Dim objfso
' Create Shell.Application so we can use the CopyHere method
Set objshell = CreateObject("Shell.Application")
' Create FileSystemObject so we can use FolderExists and CreateFolder if necessary
Set objfso = CreateObject("Scripting.FileSystemObject")
' Create folder to receive files if it doesn' t already exist
If Not objfso.FolderExists(strFolderName) Then objfso.CreateFolder strFolderName
' Use CopyHere to extract files
objshell.NameSpace(strFolderName).CopyHere objshell.NameSpace(strFileName).Items
Set objfso = Nothing
Set objshell = Nothing
End Function



Function SaveWebBinary(strUrl, strFile) 'As Boolean
Const adTypeBinary = 1
Const adSaveCreateOverWrite = 2
Const ForWriting = 2
Dim web, varByteArray, strData, strBuffer, lngCounter, ado
    On Error Resume Next
    'Download the file with any available object
    Err.Clear
    Set web = Nothing
    Set web = CreateObject("WinHttp.WinHttpRequest.5.1")
    If web Is Nothing Then Set web = CreateObject("WinHttp.WinHttpRequest")
    If web Is Nothing Then Set web = CreateObject("MSXML2.ServerXMLHTTP")
    If web Is Nothing Then Set web = CreateObject("Microsoft.XMLHTTP")
    web.Open "GET", strURL, False
    web.Send
    If Err.Number <> 0 Then
        SaveWebBinary = False
        Set web = Nothing
        Exit Function
    End If
    If web.Status <> "200" Then
        SaveWebBinary = False
        Set web = Nothing
        Exit Function
    End If
    varByteArray = web.ResponseBody
    Set web = Nothing
    'Now save the file with any available method
    On Error Resume Next
    Set ado = Nothing
    Set ado = CreateObject("ADODB.Stream")
    If ado Is Nothing Then
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set ts = fs.OpenTextFile(strFile, ForWriting, True)
        strData = ""
        strBuffer = ""
        For lngCounter = 0 to UBound(varByteArray)
            ts.Write Chr(255 And Ascb(Midb(varByteArray,lngCounter + 1, 1)))
        Next
        ts.Close
    Else
        ado.Type = adTypeBinary
        ado.Open
        ado.Write varByteArray
        ado.SaveToFile strFile, adSaveCreateOverWrite
        ado.Close
    End If
    SaveWebBinary = True
End Function
