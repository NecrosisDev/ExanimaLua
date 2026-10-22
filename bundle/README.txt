EXANIMA MODS BY NECROSISDEV
===========================

Everything needed to play these mods on another PC with your own copy of Exanima (Steam).
No game files are included.

WHAT'S INSIDE
{MODS}
  EMTK              the Exanima Modding Toolkit launcher, with the patched framework the Lua mods need
  profile           an EMTK profile called "Necrosis Mods" with everything enabled in the right order

INSTALL
  1. Close Exanima.
  2. Double-click Install.cmd. It finds your Exanima folder (or asks for it), backs up your saves to
     Documents\Exanima save backups, then copies the mods, EMTK and the profile into the game folder.
     Nothing is deleted. Every copied file is checked against SHA256SUMS.txt.
  3. Start the game with  <Exanima folder>\EMTK\Launch Exanima (modded).vbs
     Or open EMTK\emtk_launcher.exe, pick the "Necrosis Mods" profile and launch from there.

  If you already use EMTK, you can keep your own launcher: put EMTK\emtk_framework.dll beside it
  (back up the old one first) and enable the mods in your profile, Exanima Lua Runtime first.

NOTES
  - Endless is installed but switched off: it needs template files built from your own game install,
    which can't be shared. Its README (mods\com.necrosis.endless\README.md) explains how to make them.
  - Each mod's options are in game under Settings > Mod Settings.
  - Keep a mod enabled for any save that has its items in it, or go back to the backed-up saves.
  - Problems? Read mods\com.necrosis.script-runtime\ScriptRuntime.log in your Exanima folder.

UNINSTALL
  Pick another EMTK profile, or delete the mods\com.necrosis.* folders, the EMTK folder and
  .emtk\profiles\Necrosis Mods from your Exanima folder. Restore your saves from the backup if needed.

LICENSES
  EMTK is MIT / Apache-2.0 (EMTK\LICENSE-MIT, EMTK\LICENSE-APACHE); EMTK\emtk-startup.patch is the
  change made to upstream EMTK (github.com/ExanimaModding/Toolkit). Each mod's own licence, where it has
  one, is in its folder.
