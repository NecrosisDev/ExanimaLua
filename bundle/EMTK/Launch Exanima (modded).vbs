' Starts Exanima through EMTK with the "Necrosis Mods" profile. Lives in <Exanima folder>\EMTK.
' A profile name as the first argument picks another profile from <Exanima folder>\.emtk\profiles.
Option Explicit
Dim shell, fso, here, game, profile
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
here = fso.GetParentFolderName(WScript.ScriptFullName)
game = fso.GetParentFolderName(here)
profile = "Necrosis Mods"
If WScript.Arguments.Count > 0 Then profile = WScript.Arguments(0)
profile = game & "\.emtk\profiles\" & profile & "\load_order.toml"
If Not fso.FileExists(profile) Then
  MsgBox "No EMTK profile at " & profile, vbExclamation, "Exanima"
  WScript.Quit 1
End If

' A command-line launch does not pass the profile to the game the way EMTK's window does; without it EMTK stops
' the game at startup.
shell.Environment("Process")("EMTK_LOAD_ORDER_PATH") = profile
shell.CurrentDirectory = here
shell.Run """" & here & "\emtk_launcher.exe"" instance launch", 0, False
