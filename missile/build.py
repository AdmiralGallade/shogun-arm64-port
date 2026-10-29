#!/usr/bin/env python3
"""
build.py -- compile the M weapon and emit what both runtimes need to load it.

    python missile/build.py [--ndk <path>]

Outputs
  missile/out/missile.bin        the blob, linked at BLOB_BASE
  missile/out/missile.json       blob + patch list, for the desktop harness
  android/app/src/main/cpp/missile_blob.h
                                 the same, as C++ arrays, plus the M button

Everything the blob calls in the game is checked against the library's symbol
table, and every patch site against the bytes it expects to replace, so a
wrong address fails here rather than as a crash on a phone.
"""
import argparse
import glob
import json
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
LIB = os.path.join(ROOT, "runtime", "lib", "libHAL.Android.so")
OUT = os.path.join(HERE, "out")
HEADER = os.path.join(ROOT, "android", "app", "src", "main", "cpp",
                      "missile_blob.h")
# Button art, written beside the game's saves, where its '*' paths look.
# The P and shield art comes from make_upgrades.py.
ICONS = ["weapon_m.tga", "weapon_e_up.tga", "weapon_e_down.tga",
         "weapon_p_up.tga", "weapon_p_down.tga", "weapon_sh_up.tga", "weapon_sh_down.tga",
         "shield_bubble.tga", "plasma_sphere.tga", "next_mission.tga"]

BLOB_BASE = 0x10300000
BLOB_SIZE = 0x100000            # missile.ld: nothing else is mapped up to here (1 MB since 2026-10-12)
IMG = 0x10000000

# Engine addresses used by missile.c, and the symbol each must be. The value
# carries the Thumb bit exactly as the symbol table does.
ENGINE = {
    "UE_GetGameDataPtr": "UE_GetGameDataPtr",
    "UE_GetHandlePtr": "UE_GetHandlePtr",
    "FAdd": "__aeabi_fadd",
    "UE_DeleteBagEntry": "UE_DeleteBagEntry",
    "WR_GetHash": "UE_GetHashFromString",
    "WR_LookupHashKey": "UE_LookupHashKey",
    "WR_DeleteHashKey": "UE_DeleteHashKey",
    "WR_DeleteBagEntry": "UE_DeleteBagEntry",
    "WR_GetEntryFromBagIndex": "UE_GetEntryFromBagIndex",
    "WR_GetHandlePtr": "UE_GetHandlePtr",
    "WR_Log": "UE_Log",
    "WR_SetRemoveBadGuyCb": "BH_SetRemoveBadGuyCallback",
    "WR_SetRemoveGenericCb": "BH_SetRemoveGenericEntityCallback",
    "WR_ClearEntities": "BH_ClearEntities",
    "StrikeBadguy": "_Z12StrikeBadguyPvi",
    "AddShotImpact": "_Z13AddShotImpactP15WEAPONSHOTMODELjii",
    "UE_PlaySound": "UE_PlaySound",
    "UE_LoadSprites": "UE_LoadSprites",
    "UE_LoadSound": "UE_LoadSound",
    "UE_PopBlendMode": "UE_PopBlendMode",
    "UE_Sin": "UE_Sin",
    "UE_Cos": "UE_Cos",
    "UE_GetAngle": "UE_GetAngle",
    "UE_GetAngleDelta": "UE_GetAngleDelta",
    "UE_MulFP": "UE_MulFP",
    "UE_GetFirstBagEntry": "UE_GetFirstBagEntry",
    "UE_GetNextBagEntry": "UE_GetNextBagEntry",
    "BH_GetFirstBadGuy": "BH_GetFirstBadGuy",
    "BH_GetNextBadGuy": "BH_GetNextBadGuy",
    "BH_GetBadGuyPosition": "BH_GetBadGuyPosition",
    "BH_isBadGuyInvincible": "BH_isBadGuyInvincible",
    "BH_GetBadGuyHealth": "BH_GetBadGuyHealth",
    "UpdateSpread": "_Z12UpdateSpreadP6PLAYER",
    "UpdateSatellites": "_Z16UpdateSatellitesP6PLAYER",
    "RenderSatellites": "_Z16RenderSatellitesP6PLAYERiii",
    "RenderPlayer": "_Z12RenderPlayerP6SHOGUNiii",
    "RenderPlayerShadow": "_Z18RenderPlayerShadowP6SHOGUNii",
    "PlayerOutro": "_Z22UpdatePlayerStateOutroP6PLAYERP5STATE",
    "PlayerPreOutro": "_Z25UpdatePlayerStatePreOutroP6PLAYERjj",
    "LoadSatellites": "_Z14LoadSatellitesP6WEAPON",
    "UpdateShotWeapon": "_Z16UpdateShotWeaponP6PLAYERjPP15WEAPONSHOTMODELPA4_S2_",
    "WeaponBulletCancel": "_Z18WeaponBulletCancelP6PLAYER",
    "GetEntryCoord": "_Z13GetEntryCoordiPiS_",
    "isNarrowScreen": "_Z14isNarrowScreenv",
    "TestWeaponSelection": "_Z19TestWeaponSelectionP10WEAPONMENUjii",
    "SelectBTWeaponMenu": "_Z26SelectBulletTimeWeaponMenuP10WEAPONMENUP6PLAYERj",
    "DecideBTWeaponMenu": "_Z26DecideBulletTimeWeaponMenuP10WEAPONMENUP6PLAYERj",
    "DeactivateBTWeaponMenu": "_Z30DeactivateBulletTimeWeaponMenuP10WEAPONMENU",
    "SetActionWeaponSet": "_Z18SetActionWeaponSetP6WEAPONj",
    "EnableWeapon": "_Z12EnableWeaponP6SHOGUN",
    "RenderBTWeaponMenuSprite": "_Z32RenderBulletTimeWeaponMenuSpriteP6SHOGUNjiii",
    "MS_GetBadGuyActor": "BH_GetBadGuyActor",
    "MS_SetActorPosition": "AM_SetActorPosition",
    "MS_StrikePlayer": "_Z12StrikePlayerP6PLAYERPvP3HUD",
    # beach.c
    "BH_LoadBadGuy": "BH_LoadBadGuy",
    "BH_LoadBullet": "BH_LoadBullet",
    "BH_LoadGenericEntity": "BH_LoadGenericEntity",
    # worlds.c
    "MPEG_GetTexture": "MPEG_GetTexture",
    "LoadWorldFile": "_Z13LoadWorldFileP6SHOGUNPc",
    "InitWorldFile": "_Z13InitWorldFileP6SHOGUNPc",
    "LoadWorld1": "_Z10LoadWorld1P6SHOGUN",
    "LoadBossMusic": "_Z13LoadBossMusicv",
    "OceanBossInit": "_Z13OceanBossInitP4BOSS",
    "UE_LoadSprites5": "UE_LoadSprites",
    "UE_LoadTexture": "UE_LoadTexture",
    "UE_DrawSprite": "UE_DrawSprite",
    "UE_SetEntityTexture": "UE_SetEntityTexture",
    "MPEG_Open": "MPEG_Open",
    "MPEG_Start": "MPEG_Start",
    "SL_Mpeg2Close": "_Z11mpeg2_closeP10mpeg2dec_s",
    "SL_OSFree": "UE_OSFree",
    "SL_GetHandlePtr": "UE_GetHandlePtr",
    "UE_PopMemoryContext": "UE_PopMemoryContext",
    "MPEG_DecodeFrame": "MPEG_DecodeFrame",
    "UE_GetGameDataPtr5": "UE_GetGameDataPtr",
    "UE_SetConstraint": "UE_SetConstraint",
    "GetCurrentLeaderboardLevel": "_Z26GetCurrentLeaderboardLevelP6SHOGUN",
    "DB_Load": "_Z14LoadDebriefingP6SHOGUN",
    "DB_Update": "_Z16UpdateDebriefingP6SHOGUN",
    "DB_Render": "_Z16RenderDebriefingP6SHOGUN",
    "DB_UIScaled": "_Z8UIScaledij",
    "DB_UIDisplayFactor": "_Z18GetUIDisplayFactori",
    "DB_HighQuality": "UE_EnableHighQualitySprites",
    "DB_LowQuality": "UE_DisableHighQualitySprites",
    "DB_Filtering": "UE_EnableSpriteFiltering",
    "DB_NoFiltering": "UE_DisableSpriteFiltering",
    "DB_CreateIcon": "_Z27MENU_CreateSimpleIconWidgetjj",
    "DB_UpdateWidget": "UE_UpdateWidget",
    "DB_RenderWidget": "UE_RenderWidget",
    "DB_IconClicked": "_Z24MENU_isSimpleIconClickedj",
    "DB_PushAlpha": "UE_PushAlpha",
    "DB_PopAlpha": "UE_PopAlpha",
    "DB_ScreenWidth": "UE_GetScreenWidth",
    "DB_ScreenHeight": "UE_GetScreenHeight",
    "DB_GetWidth": "UE_GetWidth",
    "DB_SetStateFade": "_Z21SetStateWithCrossfadeP6SHOGUNjPc",
    # starfish.c
    "SF_GetGameDataPtr": "UE_GetGameDataPtr",
    "SF_ClearBytes": "UE_ClearBytes",
    "BH_PausePartition": "BH_PausePartition",
    "ShowNextSpeechSection": "_Z21ShowNextSpeechSectionP6SHOGUN",
    "HideDialog": "_Z10HideDialogP6SHOGUN",
    "ShakeWorld": "_Z10ShakeWorldv",
    "BH_SpawnBadGuy": "BH_SpawnBadGuy",
    "BH_GetBadGuyActorSF": "BH_GetBadGuyActor",
    "AM_SetActorPosition": "AM_SetActorPosition",
    "AM_SetActorOrientation": "AM_SetActorOrientation",
    "AM_SetActorRegisterValue": "AM_SetActorRegisterValue",
    "AM_GetActorRegisterValue": "AM_GetActorRegisterValue",
    "BH_SetBadGuyInvincibility": "BH_SetBadGuyInvincibility",
    "BH_GetBadGuyHealthSF": "BH_GetBadGuyHealth",
    "BH_SpawnGenericEntity": "BH_SpawnGenericEntity",
    "BH_SpawnExplosion": "BH_SpawnExplosion",
    "SF_Sin": "UE_Sin",
    "SF_Cos": "UE_Cos",
    "SF_MulFP": "UE_MulFP",
    "SF_GetFirstBadGuy": "BH_GetFirstBadGuy",
    "SF_GetNextBadGuy": "BH_GetNextBadGuy",
    "SF_RangedRand": "UE_RangedRand",
    "SF_GetAngle": "UE_GetAngle",
    "SF_GetFrameCount": "UE_GetFrameCount",
    "SF_GetGenericActor": "BH_GetGenericEntityActor",
    "SF_GetActorPosition": "AM_GetActorPosition",
    "StrikePlayer": "_Z12StrikePlayerP6PLAYERPvP3HUD",
    "isDialogHidden": "_Z14isDialogHiddenP6SHOGUN",
    "OceanMidBossInit": "_Z16OceanMidBossInitP4BOSS",
    # hermit.c
    "HB_GetGameDataPtr": "UE_GetGameDataPtr",
    "HB_ClearBytes": "UE_ClearBytes",
    "HB_PausePartition": "BH_PausePartition",
    "HB_ShakeWorld": "_Z10ShakeWorldv",
    "HB_SpawnBadGuy": "BH_SpawnBadGuy",
    "HB_GetBadGuyActor": "BH_GetBadGuyActor",
    "HB_SetActorPosition": "AM_SetActorPosition",
    "HB_SetActorRegister": "AM_SetActorRegisterValue",
    "HB_GetActorRegister": "AM_GetActorRegisterValue",
    "HB_SetInvincibility": "BH_SetBadGuyInvincibility",
    "HB_GetBadGuyHealth": "BH_GetBadGuyHealth",
    "HB_StrikeBadGuy": "BH_StrikeBadGuy",
    "HB_SpawnGeneric": "BH_SpawnGenericEntity",
    "HB_SpawnExplosion": "BH_SpawnExplosion",
    "HB_GetFirstBadGuy": "BH_GetFirstBadGuy",
    "HB_GetNextBadGuy": "BH_GetNextBadGuy",
    "HB_MulFP": "UE_MulFP",
    "HB_Sin": "UE_Sin",
    # seoul.c
    "SE_LoadBadGuy": "BH_LoadBadGuy",
    "SE_LoadBullet": "BH_LoadBullet",
    "SE_LoadGeneric": "BH_LoadGenericEntity",
    # edinburgh.c
    "ED_LoadBadGuy": "BH_LoadBadGuy",
    "ED_LoadBullet": "BH_LoadBullet",
    "ED_LoadGeneric": "BH_LoadGenericEntity",
    "ED_LoadSprites": "UE_LoadSprites",
    "ED_DrawSprite": "UE_DrawSprite",
    "ED_GetScreenWidth": "UE_GetScreenWidth",
    "ED_GetScreenHeight": "UE_GetScreenHeight",
    "ED_GetWidth": "UE_GetWidth",
    "ED_GetHeight": "UE_GetHeight",
    "ED_GetMouseX": "UE_GetMouseX",
    "ED_GetMouseY": "UE_GetMouseY",
    "ED_GetMouseState": "HAL_GetMouseState",
    "ED_RenderShieldArea": "_Z16RenderShieldAreaP3HUDP6PLAYER",
    "ED_isFileExist": "UE_isFileExist",
    "ED_LoadBinFile": "UE_LoadBinFile",
    "ED_SaveBinFile": "UE_SaveBinFile",
    "WR_isFileExist": "UE_isFileExist",          # worlds.c: the Cheats tab's hard mode flag (docs/HARD_MODE.md)
    "WR_LoadBinFile": "UE_LoadBinFile",
    "ED_GetPartitionTime": "BH_GetPartitionTime",
    "ED_DeactivateBTMenu": "_Z30DeactivateBulletTimeWeaponMenuP10WEAPONMENU",
    # andaman.c
    "AD_LoadBadGuy": "BH_LoadBadGuy",
    "AD_LoadBullet": "BH_LoadBullet",
    "AD_LoadGeneric": "BH_LoadGenericEntity",
    "AD_GetPartitionTime": "BH_GetPartitionTime",
    # antarctica.c
    "AN_LoadBadGuy": "BH_LoadBadGuy",
    "AN_LoadBullet": "BH_LoadBullet",
    "AN_LoadGeneric": "BH_LoadGenericEntity",
    # seoul_boss.h (magpie.c, twins.c, courier.c, chase.c)
    "SB_GetGameDataPtr": "UE_GetGameDataPtr",
    "SB_ClearBytes": "UE_ClearBytes",
    "SB_PausePartition": "BH_PausePartition",
    "SB_ShakeWorld": "_Z10ShakeWorldv",
    "SB_SpawnBadGuy": "BH_SpawnBadGuy",
    "SB_GetBadGuyActor": "BH_GetBadGuyActor",
    "SB_SetActorPosition": "AM_SetActorPosition",
    "SB_SetActorOrient": "AM_SetActorOrientation",
    "SB_SetActorRegister": "AM_SetActorRegisterValue",
    "SB_GetActorRegister": "AM_GetActorRegisterValue",
    "SB_GetActorPosition": "AM_GetActorPosition",
    "SB_SetInvincibility": "BH_SetBadGuyInvincibility",
    "SB_GetBadGuyHealth": "BH_GetBadGuyHealth",
    "SB_SetBadGuyHealth": "BH_SetBadGuyHealth",
    "SB_StrikeBadGuy": "BH_StrikeBadGuy",
    "SB_SpawnGeneric": "BH_SpawnGenericEntity",
    "SB_SpawnBonus": "BH_SpawnBonus",
    "SB_GetGenericActor": "BH_GetGenericEntityActor",
    "SB_SpawnExplosion": "BH_SpawnExplosion",
    "SB_GetFirstBadGuy": "BH_GetFirstBadGuy",
    "SB_GetNextBadGuy": "BH_GetNextBadGuy",
    "SB_MulFP": "UE_MulFP",
    "SB_Sin": "UE_Sin",
    "SB_Cos": "UE_Cos",
    "SB_RangedRand": "UE_RangedRand",
    "SB_GetAngle": "UE_GetAngle",
    "SB_StrikePlayer": "_Z12StrikePlayerP6PLAYERPvP3HUD",
    # worlds.c: the level select's entries past the game's four
    "StartNewGame": "_Z12StartNewGameP6SHOGUNj",
    # sunda.c, coelacanth.c (Mission 10)
    "SU_LoadBadGuy": "BH_LoadBadGuy",
    "SU_LoadBullet": "BH_LoadBullet",
    "SU_LoadGeneric": "BH_LoadGenericEntity",
    "SU_PausePartition": "BH_PausePartition",
    # abyss.c (Mission 11)
    "AB_LoadBadGuy": "BH_LoadBadGuy",
    "AB_LoadBullet": "BH_LoadBullet",
    "AB_LoadGeneric": "BH_LoadGenericEntity",
    "AB_GetPartitionTime": "BH_GetPartitionTime",
    "AB_LoadSprites": "UE_LoadSprites",
    "AB_LoadSound": "UE_LoadSound",
    "AB_PlaySound": "UE_PlaySound",
    "AB_SetChannelVolume": "UE_SetChannelVolume",
    "AB_DrawSprite": "UE_DrawSprite",
    "AB_DrawRotatedSprite": "UE_DrawRotatedSprite",
    "AB_GetAlpha": "UE_GetAlpha",
    "AB_GetWidth": "UE_GetWidth",
    "AB_GetHeight": "UE_GetHeight",
    "AB_InsertBagEntry": "UE_InsertBagEntry",
    "AB_GetFirstBagEntry": "UE_GetFirstBagEntry",
    "AB_GetNextBagEntry": "UE_GetNextBagEntry",
    "AB_StrikeBadguy": "_Z12StrikeBadguyPvi",
    "AB_RenderShip": "_Z10RenderShipP6SHOGUNii",
    "AB_GetBadGuyPosition": "BH_GetBadGuyPosition",
    "AB_SetEntityOpacity": "UE_SetEntityOpacity",
    "AB_ClearScreen": "UE_ClearScreen",
    "AB_GetFrameCount": "UE_GetFrameCount",
    "AB_GetGrazeCount": "BH_GetGrazeCount",
    "AB_IsInvincible": "BH_isBadGuyInvincible",
    # homeward.c, fortress.c, inferno.c (Mission 13)
    "HW_LoadBadGuy": "BH_LoadBadGuy",
    "HW_LoadBullet": "BH_LoadBullet",
    "HW_LoadGeneric": "BH_LoadGenericEntity",
    "HW_SetEntityOpacity": "UE_SetEntityOpacity",
    "HW_GetFrameCount": "UE_GetFrameCount",
    "FT_SetActorParent": "AM_SetActorParent",
    "IN_SetActorParent": "AM_SetActorParent",
    # atlantic.c (Mission 16)
    "AL_LoadBadGuy": "BH_LoadBadGuy",
    "AL_LoadBullet": "BH_LoadBullet",
    "AL_LoadGeneric": "BH_LoadGenericEntity",
    "AL_LoadSprites": "UE_LoadSprites",
    "AL_DrawSprite": "UE_DrawSprite",
    "AL_GetScreenWidth": "UE_GetScreenWidth",
    "AL_GetScreenHeight": "UE_GetScreenHeight",
    "AL_GetWidth": "UE_GetWidth",
    "AL_GetFrameCount": "UE_GetFrameCount",
    # paris.c (Mission 15)
    "PA_LoadBadGuy": "BH_LoadBadGuy",
    "PA_LoadGeneric": "BH_LoadGenericEntity",
    "PA_GetFrameCount": "UE_GetFrameCount",
    "PA_EnableEntity": "UE_EnableEntity",
    "PA_DisableEntity": "UE_DisableEntity",
    "PA_CurrentSprite": "_ZL16getCurrentSpriteP6ENTITY",
    # amaterasu.c, barge.c, raijin.c (Mission 14)
    "AT_LoadBadGuy": "BH_LoadBadGuy",
    "AT_LoadBullet": "BH_LoadBullet",
    "AT_LoadGeneric": "BH_LoadGenericEntity",
    "AT_SetEntityOpacity": "UE_SetEntityOpacity",
    "AT_GetFrameCount": "UE_GetFrameCount",
    "AT_LoadSprites": "UE_LoadSprites",
    "AT_DrawSprite": "UE_DrawSprite",
    "AT_GetScreenWidth": "UE_GetScreenWidth",
    "AT_GetScreenHeight": "UE_GetScreenHeight",
    "AT_GetWidth": "UE_GetWidth",
    "AT_GetHeight": "UE_GetHeight",
    "BG_SetActorParent": "AM_SetActorParent",
    "RJ_SetActorParent": "AM_SetActorParent",
    # lab.c (Mission 12)
    "LR_LoadBadGuy": "BH_LoadBadGuy",
    "LR_LoadBullet": "BH_LoadBullet",
    "LR_LoadGeneric": "BH_LoadGenericEntity",
    # dialog.c (every mission's dialog box)
    "DG_UpdateSpeech": "_Z12UpdateSpeechP6SPEECH",
    "DG_UpdateWeaponMenu": "_Z16UpdateWeaponMenuP6SHOGUNjii",
    "DG_isDialogHidden": "_Z14isDialogHiddenP6SHOGUN",
    "DG_GetGameDataPtr": "UE_GetGameDataPtr",
    "DG_GetFrameCount": "UE_GetFrameCount",
    "DG_GetScreenHeight": "UE_GetScreenHeight",
    "DG_GetMouseX": "UE_GetMouseX",
    "DG_GetMouseY": "UE_GetMouseY",
    "DG_GetMouseState": "HAL_GetMouseState",
    "DG_GetScreenWidth": "UE_GetScreenWidth",
    "DG_UIScaled": "_Z8UIScaledij",
    "DG_Clamp": "UE_Clamp",
    "DG_TestWeaponSelection": "_Z19TestWeaponSelectionP10WEAPONMENUjii",
    "DG_isMouseOverWidget": "_Z20UE_isMouseOverWidgetjii",
    "DG_GetHandlePtr": "UE_GetHandlePtr",
    # lore.c (the story so far, on the loading screen)
    "LO_GetState": "UE_GetState",
    "LO_UpdateCrossfade": "_Z15UpdateCrossfadeP6SHOGUN",
    "LO_RenderCrossfade": "_Z15RenderCrossfadeP6SHOGUN",
    "LO_GameData": "UE_GetGameDataPtr",
    "LO_PushAlpha": "UE_PushAlpha",
    "LO_PopAlpha": "UE_PopAlpha",
    "LO_ScreenWidth": "UE_GetScreenWidth",
    "LO_ScreenHeight": "UE_GetScreenHeight",
    "LO_SetFont": "UE_SetFont",
    "LO_GetTextWidth": "UE_GetTextWidth",
    "LO_GetTextHeight": "UE_GetTextHeight",
    "LO_DrawText": "UE_DrawText",
    "LO_DrawColumn": "_Z20DrawTextwithinColumnPcjjiijj",
    "LO_SetColor": "UE_SetColor",
    "LO_GetMouseState": "HAL_GetMouseState",
    "LO_MulFP": "UE_MulFP",
}
# Guest sources linked into the one blob.
SOURCES = ["missile.c", "beach.c", "worlds.c", "starfish.c", "hermit.c",
           "seoul.c", "magpie.c", "twins.c", "courier.c", "antarctica.c", "chase.c",
           "edinburgh.c", "subagent.c", "shogungpt.c", "andaman.c", "battery.c", "ironreef.c",
           "sunda.c", "coelacanth.c", "abyss.c", "abyss_boss.c", "lab.c", "lab_boss.c", "homeward.c", "fortress.c",
           "inferno.c", "amaterasu.c", "barge.c", "raijin.c", "paris.c", "tank.c", "flagship.c", "atlantic.c", "limits.c", "dialog.c", "lore.c"]

# (site, what the game has there, blob symbol). Every site is Thumb; each
# becomes a 4-byte `bl` into the blob. `expect` is checked before patching.
#   call:<symbol>  -- an existing `bl` to that function
#   raw:<hex>      -- these exact bytes (instructions that are not a call)
PATCHES = [
    # weapon logic
    (0x42aa4, "call:_Z12UpdateSpreadP6PLAYER", "hook_update_spread"),
    (0x42a9e, "call:_Z16UpdateSatellitesP6PLAYER", "hook_update_satellites"),
    (0x3bc64, "call:_Z16RenderSatellitesP6PLAYERiii", "hook_render_satellites"),
    (0x3bd1c, "call:_Z16RenderSatellitesP6PLAYERiii", "hook_render_satellites"),
    # Mission 10's ending: the ship hidden once the pilot is aboard the Valhalla
    (0x460fa, "call:_Z12RenderPlayerP6SHOGUNiii", "hook_render_player"),
    (0x46124, "call:_Z12RenderPlayerP6SHOGUNiii", "hook_render_player"),
    (0x3c202, "call:_Z12RenderPlayerP6SHOGUNiii", "hook_render_player"),
    (0x3746c, "call:_Z18RenderPlayerShadowP6SHOGUNii", "hook_render_player_shadow"),
    # ... and no warp at the level's end: she went under the sea
    (0x45258, "call:_Z25UpdatePlayerStatePreOutroP6PLAYERjj", "hook_player_preoutro"),
    (0x4522a, "call:_Z22UpdatePlayerStateOutroP6PLAYERP5STATE", "hook_player_outro"),
    (0x42b68, "raw:052b23d0", "shim_action"),         # cmp r3,#5 ; beq
    (0x2d772, "call:_Z14LoadSatellitesP6WEAPON", "hook_load"),
    # UpdateShots' hit handling, for the coilgun's pass-through
    (0x3c2ec, "call:_Z12StrikeBadguyPvi", "shim_strike"),
    (0x3c2f8, "call:_Z13AddShotImpactP15WEAPONSHOTMODELjii", "shim_impact"),
    # SetActionWeaponSwitch: `ldr r3,[r0,#0x2c] ; cmp r3,#0` -> bl, then the
    # next halfword becomes `pop {pc}` (see EXTRA below)
    (0x28b6e, "raw:c36a002b", "hook_switch"),
    # the radial menu
    (0x38240, "call:UE_PopBlendMode", "shim_render_menu"),
    (0x3802e, "raw:4e4aae18", "shim_sprite_id"),      # ldr r2,=0x762 ; adds
    (0x3802a, "call:_Z13GetEntryCoordiPiS_", "hook_entry_coord"),
    (0x337c6, "call:_Z13GetEntryCoordiPiS_", "hook_entry_coord"),
    (0x37c6a, "call:_Z19TestWeaponSelectionP10WEAPONMENUjii", "hook_test_last"),
    (0x37c74, "call:_Z26SelectBulletTimeWeaponMenuP10WEAPONMENUP6PLAYERj",
     "hook_select"),
    # worlds 5-8, the Beach, Seoul, the chase and Edinburgh (worlds.c): level select...
    (0x4674a, "raw:67f07cee", "hook_select_assets"),   # blx MPEG_DecodeFrame
    # ... its preview clips remembered as they open, and freed on leaving
    # (nothing closed them: ~30 MB a visit)
    (0x466dc, "raw:57f058e9", "hook_mpeg_open"),       # blx MPEG_Open
    (0x466ee, "raw:57f050e9", "hook_mpeg_open"),
    (0x46700, "raw:57f046e9", "hook_mpeg_open"),
    (0x46712, "raw:57f03ee9", "hook_mpeg_open"),
    (0x291e6, "call:UE_PopMemoryContext", "hook_select_exit"),
    # Edinburgh's skip-tutorial button, drawn with the HUD (edinburgh.c)
    (0x36712, "call:_Z16RenderShieldAreaP3HUDP6PLAYER", "hook_render_shield_area"),
    (0x3675a, "call:_Z16RenderShieldAreaP3HUDP6PLAYER", "hook_render_shield_area"),
    # the preview's MPEG calls on 0x69a + entry (0x69f, entry 5, is the
    # globe's map texture)
    (0x42eaa, "raw:6bf0ccea", "hook_mpeg_decode"),     # blx MPEG_DecodeFrame
    (0x42ebc, "raw:5af0e4ea", "hook_mpeg_start"),      # blx MPEG_Start
    (0x42ec8, "raw:6bf0bcea", "hook_mpeg_decode"),
    (0x43064, "raw:5af032ea", "hook_mpeg_texture"),    # blx MPEG_GetTexture
    (0x4307a, "raw:5af006ea", "hook_mpeg_start"),
    (0x46758, "raw:56f0b8ee", "hook_mpeg_texture"),
    (0x45ccc, "call:UE_DrawSprite", "hook_draw_title"),
    (0x45dc8, "call:UE_SetEntityTexture", "hook_globe_texture"),
    (0x42d9e, "raw:2ff048ec", "hook_globe_spin"),      # blx UE_SetConstraint
    (0x42e7a, "raw:2ff0daeb", "hook_globe_spin"),
    # ...and the world itself
    (0x47bd8, "raw:04291ed8", "shim_load_world"),      # cmp r1,#4 ; bhi
    (0x44e8c, "raw:042a0ad8", "shim_init_world"),      # cmp r2,#4 ; bhi
    (0x315b8, "raw:283be358", "shim_init_boss"),       # subs r3,#0x28 ; ldr
    (0x3fdb4, "call:_Z13OceanBossInitP4BOSS", "hook_final_boss_init"),
    (0x315ca, "call:_Z16OceanMidBossInitP4BOSS", "hook_mid_boss_init"),
    (0x3231c, "call:_Z26GetCurrentLeaderboardLevelP6SHOGUN", "hook_leaderboard_level"),
    # ... one mission to the next: worlds 5-11 go on to the next world, not
    # the game's ending; the ending (after Mission 4) gets a Next Mission
    # button under Continue to Hell Mode (worlds.c)
    (0x4801e, "raw:032900d9", "shim_next_world"),     # cmp r1,#3 ; bls
    (0x2e8b2, "call:_Z14LoadDebriefingP6SHOGUN", "hook_load_debriefing"),
    (0x47e2e, "call:_Z16UpdateDebriefingP6SHOGUN", "hook_update_debriefing"),
    (0x460d2, "call:_Z16RenderDebriefingP6SHOGUN", "hook_render_debriefing"),
    (0x393c6, "call:_Z8UIScaledij", "hook_debrief_raise"),   # the hell button, update
    (0x415ec, "call:_Z8UIScaledij", "hook_debrief_raise"),   # ... render
    (0x41608, "call:_Z8UIScaledij", "hook_debrief_raise"),   # the banner
    (0x44c82, "call:_Z26GetCurrentLeaderboardLevelP6SHOGUN", "hook_leaderboard_level"),
    # the environment's placements, 512 -> 2048, in a table of ours (limits.c)
    (0x47362, "raw:c300b518", "shim_env_copy"),      # lsls r3,r0,#3 ; adds r5,r6,r2
    (0x477d6, "raw:d9006318", "shim_env_convert"),   # lsls r1,r3,#3 ; adds r3,r4,r1
    (0x4782e, "raw:a5199a46", "shim_env_fixup"),     # adds r5,r4,r6 ; mov sl,r3
    (0x4774c, "raw:16197318", "shim_env_length"),    # adds r6,r2,r4 ; adds r3,r6,r1
    (0x35294, "raw:1d19aa46", "shim_dyn_entry"),     # adds r5,r3,r4 ; mov sl,r5
    (0x28e2c, "raw:da003618", "shim_ocean_path"),    # lsls r2,r3,#3 ; adds r6,r6,r0
    (0x28e46, "raw:be003418", "shim_ocean_path2"),   # lsls r6,r7,#2 ; adds r4,r6,r0
    # the level select: an entry past the game's four starts its row's world
    # (worlds.c: world 10 is left for a level of its own)
    (0x42efa, "call:_Z12StartNewGameP6SHOGUNj", "hook_start_new_game"),
    (0x42f42, "call:_Z12StartNewGameP6SHOGUNj", "hook_start_new_game"),
    # Mission 11 (abyss.c): the ship drawn as a submersible; armour that only
    # the laser burns
    (0x3bcd6, "call:_Z10RenderShipP6SHOGUNii", "hook_render_ship"),
    (0x3b614, "call:_Z12StrikeBadguyPvi", "hook_laser_strike"),
    # ... and the water round its floor is blue, deepening, not black
    (0x45e9c, "call:UE_ClearScreen", "hook_clear_screen"),
    (0x45f4c, "call:UE_ClearScreen", "hook_clear_screen"),
    (0x45fde, "call:UE_ClearScreen", "hook_clear_screen"),
    # Mission 12 (abyss.c): the props' clip plane let down below the ground,
    # for the chasm's walls
    (0x59be2, "raw:c7f762ec", "hook_clip_plane"),      # blx glClipPlanef
    (0x59b4e, "raw:c7f7b2ec", "hook_props_ortho"),     # blx glOrthox (its far plane)
    # every world: the generic entities' renderer (T.292, ARM code) draws each
    # with UE_DrawRotatedSprite, turned or not -- unturned ones go plain (abyss.c)
    (0xc0f34, "arm:39ae00eb", "hook_generic_draw"),   # bl UE_DrawRotatedSprite (veneer)
    # ... and BH_RenderBullets the same, for every enemy bullet
    (0xc0e0c, "arm:83ae00eb", "hook_generic_draw"),   # bl UE_DrawRotatedSprite (veneer)
    # ... and no Entropy Engine: ControlPlayer's graze count
    (0x3878e, "raw:82f0f6eb", "shim_graze"),       # blx BH_GetGrazeCount
    # every mission's dialog box: tap to show the whole sentence, tap again
    # to move on (dialog.c) -- the touch, read before the weapon wheel sees
    # it, and the tap applied to the sentence
    (0x4471a, "call:_Z16UpdateWeaponMenuP6SHOGUNjii", "hook_update_weapon_menu"),
    (0x36d38, "call:_Z12UpdateSpeechP6SPEECH", "hook_update_speech"),
    # every mission's loading screen: the story so far, held until a tap once
    # the world is loaded (lore.c) -- onUpdate's and onRender's state reads,
    # the fade's update and its drawing
    (0x47d74, "raw:2ff0cae8", "hook_update_state"),    # blx UE_GetState
    (0x45e8a, "raw:31f040e8", "hook_render_state"),    # blx UE_GetState
    (0x47dbe, "call:_Z15UpdateCrossfadeP6SHOGUN", "hook_update_crossfade"),
    (0x45ea2, "call:_Z15RenderCrossfadeP6SHOGUN", "hook_render_crossfade"),
]
# Plain rewrites, a Thumb halfword or an ARM word: (site, expect, replacement).
EXTRA = [
    (0x28b72, "06d1", "00bd"),     # bne -> pop {pc}
    # after a hit, `b delete` -> `b next`: shim_impact deletes non-slugs itself
    (0x3c2fc, "c5e7", "c9e7"),
    # InitSelectLevels: the highest entry index, 3 -> 13 (fourteen missions)
    (0x461bc, "0320", "0f20"),

    # renderGraphicShadow (ARM) projected every entity's shadow -- a project, an
    # unproject and a 2x2 solve in soft float, ~100 a frame in a busy scene --
    # and only then looked whether its graphic has a shadow sprite at all;
    # most don't. The same instructions, the look first:
    #   ldrh r4,[r4,#0xa] ; cmp r4,#0 ; beq out ; add r1,sp,#8 ; add r0,sp,#0xc ;
    #   mov lr,pc ; ldr pc,[sp,#0x24] ; ldrsh r2,[sp,#0xe] ; ldrsh r1,[sp,#0xa] ;
    #   str r2,[sp,#0xc] ; str r1,[sp,#8]   (and on as before)
    (0xbe448, "08108de2", "ba40d4e1"),
    (0xbe44c, "0c008de2", "000054e3"),
    (0xbe450, "0fe0a0e1", "1000000a"),     # beq 0xbe498 (the return)
    (0xbe454, "24f09de5", "08108de2"),
    (0xbe458, "ba40d4e1", "0c008de2"),
    (0xbe45c, "fe20dde1", "0fe0a0e1"),
    (0xbe460, "fa10dde1", "24f09de5"),
    (0xbe464, "000054e3", "fe20dde1"),
    (0xbe468, "0c208de5", "fa10dde1"),
    (0xbe46c, "08108de5", "0c208de5"),
    (0xbe470, "0800000a", "08108de5"),

    # ---- engine limits raised (docs/ENGINE_LIMITS.md) ----
    # onStartup: UE_InitCore's heap, `movs r0,#0xc0 ; lsls r0,#16` (12 MB)
    # -> `movs r0,#0x80 ; lsls r0,#18` (32 MB). Every file the engine loads
    # passes through a scratch buffer cut from the heap's free top, so this
    # is also the largest file (a 1024 x 1024 RGBA texture failed at 12 MB).
    # The guest heap that holds it is 256 MB in both runtimes.
    (0x48436, "c020", "8020"),
    (0x48440, "0004", "8004"),
    # BH_Init's definition bags and the hash tables that find them (ARM
    # `mov r1,#n`): badguys 128 -> 256, bullets 64 -> 256, generics
    # 128 -> 256. The game's own fill 101, 52 and 62 of them; Mission 9
    # used 124 badguys of 128. A full bag failed silently, and the first
    # spawn of the missing one crashed in the actor creator.
    (0xbc4bc, "8010a0e3", "011ca0e3"),   # bag  0x40a badguys
    (0xbc4fc, "4010a0e3", "011ca0e3"),   # bag  0x40b bullets
    (0xbc52c, "8010a0e3", "011ca0e3"),   # bag  0x40d generics
    (0xbc53c, "8010a0e3", "011ca0e3"),   # hash 0x40e badguys
    (0xbc54c, "4010a0e3", "011ca0e3"),   # hash 0x40f bullets
    (0xbc56c, "8010a0e3", "011ca0e3"),   # hash 0x411 generics
    # ... and the actor manager's compiled scripts, one per definition of
    # every kind (AM_Create's second count, `mov r2,#0x200`): 512 -> 1024.
    # Their code goes to the script pool, which worlds.c moves somewhere
    # bigger for the added worlds.
    (0xbc310, "022ca0e3", "012ba0e3"),
    # loadEnvironment: models stop at `cmp r7,#0x20` with r7 counting from
    # 1 (31 models). Their table is 16 bytes a model from SHOGUN+0x450 and
    # ran up to the placements' old table at SHOGUN+0x650 -- empty since
    # limits.c took the placements (0x4800 bytes). Only loadEnvironment and
    # AddEntityDyn (ENV+0x33c + model*16, unbounded) touch it, so it grows
    # into that room: `cmp r7,#0xff`, 254 models (to SHOGUN+0x1430).
    (0x4741c, "202f", "ff2f"),
    (0x474f6, "202f", "ff2f"),
    # loadEnvironment's clamp on placements, `lsls r3,r7,#2` (0x200) ->
    # `#4` (0x800): the table is limits.c's now
    (0x47348, "bb00", "3b01"),
    # UE_LoadConfigFileFromStream (ARM) sizes a loaded config's text at the
    # file's size + 8 (`add r7,r0,#8`), but writes each `#++` as "#%d": a
    # section numbered 100 or more comes out a byte longer than it went in.
    # Past ~120 numbered sections ([Model#++] + [Sprite#++]) the world's
    # text ran over the next heap chunk -- sprites lost their settings,
    # handles went missing, the heap's free list broke. 8 -> 4 KB of room.
    (0x96eb8, "087080e2", "017a80e2"),
]


def die(msg):
    sys.exit("build.py: " + msg)


def find_ndk(given):
    cands = [given] if given else []
    sdk = os.environ.get("ANDROID_SDK_ROOT") or os.path.expanduser(
        "~/Android/Sdk")
    cands += sorted(glob.glob(os.path.join(sdk, "ndk", "*")), reverse=True)
    for c in cands:
        if not c:
            continue
        b = glob.glob(os.path.join(c, "toolchains", "llvm", "prebuilt", "*",
                                   "bin"))
        if b:
            return b[0]
    die("no NDK found; pass --ndk")


def tool(bindir, name):
    for ext in ("", ".exe", ".cmd"):
        p = os.path.join(bindir, name + ext)
        if os.path.exists(p):
            return p
    die("missing " + name)


# ---- the library -----------------------------------------------------------
class Lib:
    def __init__(self, path):
        d = self.d = open(path, "rb").read()
        u32 = lambda o: struct.unpack_from("<I", d, o)[0]
        u16 = lambda o: struct.unpack_from("<H", d, o)[0]
        self.loads = []
        phoff, phnum = u32(28), u16(44)
        for i in range(phnum):
            t, off, va, _, fsz = struct.unpack_from("<5I", d, phoff + 32 * i)
            if t == 1:
                self.loads.append((va, off, fsz))
        shoff, shnum, shent, shstr = u32(32), u16(48), u16(46), u16(50)
        sec = lambda i: struct.unpack_from("<10I", d, shoff + i * shent)
        names = sec(shstr)[4]
        self.sym = {}
        secs = {}
        for i in range(shnum):
            s = sec(i)
            n = d[names + s[0]:d.index(b"\0", names + s[0])].decode()
            secs[n] = s
        sy, st = secs[".symtab"], secs[".strtab"][4]
        for o in range(sy[4], sy[4] + sy[5], 16):
            nmo = u32(o)
            if nmo and (d[o + 12] & 0xF) == 2:
                n = d[st + nmo:d.index(b"\0", st + nmo)].decode("utf-8",
                                                                  "replace")
                self.sym[n] = u32(o + 4)

    def at(self, va, n):
        for v, off, fsz in self.loads:
            if v <= va < v + fsz:
                return self.d[off + va - v:off + va - v + n]
        die("0x%x is not in the image" % va)


def thumb_bl(site, target):
    """Encode a Thumb-1 BL from `site` to `target` (both guest addresses)."""
    off = target - (site + 4)
    if not -(1 << 22) <= off < (1 << 22):
        die("bl from 0x%x to 0x%x is out of range" % (site, target))
    off >>= 1
    hi = 0xF000 | ((off >> 11) & 0x7FF)
    lo = 0xF800 | (off & 0x7FF)
    return struct.pack("<HH", hi, lo)


def arm_blx(site, target):
    """Encode an ARM `blx imm` from `site` (ARM code) to the Thumb `target`."""
    dest = target & ~1
    off = dest - (site + 8)
    if not -(1 << 25) <= off < (1 << 25):
        die("blx from 0x%x to 0x%x is out of range" % (site, target))
    h = (off >> 1) & 1
    return struct.pack("<I", 0xFA000000 | (h << 24) | ((off >> 2) & 0xFFFFFF))


def decode_bl(b):
    hi, lo = struct.unpack("<HH", b)
    if hi & 0xF800 != 0xF000 or lo & 0xE800 != 0xE800:
        return None
    off = ((hi & 0x7FF) << 11) | (lo & 0x7FF)
    if off & (1 << 21):
        off -= 1 << 22
    return off << 1, bool(lo & 0x1000)   # offset, still Thumb (bl, not blx)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ndk")
    args = ap.parse_args()
    bindir = find_ndk(args.ndk)
    os.makedirs(OUT, exist_ok=True)
    lib = Lib(LIB)

    # 1. every engine address in missile.c must be the symbol it claims
    src = "".join(open(os.path.join(HERE, n)).read()
                  for n in SOURCES + sorted(os.path.basename(h) for h in glob.glob(os.path.join(HERE, "*.h"))))
    import re
    # the address is FN's last argument; the type before it may hold commas
    for macro, addr in re.findall(
            r"#define (\w+)\s+FN\(.*,\s*(0x[0-9a-f]+)\)", src):
        want = ENGINE.get(macro)
        if not want:
            die("%s is not in build.py's ENGINE table" % macro)
        if lib.sym.get(want) != int(addr, 16):
            die("%s: missile.c says 0x%s, symbol %s is 0x%x"
                % (macro, addr[2:], want, lib.sym.get(want, 0)))

    # 2. compile and link
    clang, lld = tool(bindir, "clang"), tool(bindir, "ld.lld")
    objcopy, nm = tool(bindir, "llvm-objcopy"), tool(bindir, "llvm-nm")
    elf = os.path.join(OUT, "missile.elf")
    binf = os.path.join(OUT, "missile.bin")
    run = lambda *a: subprocess.run(a, check=True)
    objs = []
    for n in SOURCES:
        obj = os.path.join(OUT, n[:-2] + ".o")
        run(clang, "--target=armv5te-none-eabi", "-mthumb", "-mfloat-abi=soft",
            "-Os", "-ffreestanding", "-fno-builtin", "-nostdlib", "-fno-pic",
            "-fno-jump-tables", "-fno-unwind-tables",
            "-fno-asynchronous-unwind-tables", "-Wall", "-Werror", "-c",
            os.path.join(HERE, n), "-o", obj)
        objs.append(obj)
    run(lld, "-T", os.path.join(HERE, "missile.ld"), "--no-undefined",
        "-static", *objs, "-o", elf)
    run(objcopy, "-O", "binary", elf, binf)
    syms = {}
    for line in subprocess.run([nm, elf], check=True, capture_output=True,
                               text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)
    blob = open(binf, "rb").read()
    end = syms["__blob_end"]
    if end - BLOB_BASE > BLOB_SIZE:
        die("blob is 0x%x bytes, more than the 0x%x mapped"
            % (end - BLOB_BASE, BLOB_SIZE))

    # 3. the patch list, checked against the original bytes
    patches = []
    for site, expect, hook in PATCHES:
        old = lib.at(site, 4)
        kind, what = expect.split(":", 1)
        if kind == "call":
            bl = decode_bl(old)
            tgt = lib.sym.get(what)
            if not bl or not tgt or not bl[1] or site + 4 + bl[0] != tgt & ~1:
                die("0x%x is not `bl %s` (%s)" % (site, what, old.hex()))
        elif old != bytes.fromhex(what):
            die("0x%x holds %s, expected %s" % (site, old.hex(), what))
        if hook not in syms:
            die("blob has no " + hook)
        enc = arm_blx if kind == "arm" else thumb_bl     # "arm:<bytes>": a call in ARM code
        patches.append((IMG + site, old, enc(IMG + site, syms[hook])))
    for site, expect, new in EXTRA:
        if len(expect) != len(new) or len(expect) not in (4, 8):
            die("0x%x: a rewrite is 2 or 4 bytes, the same before and after" % site)
        old = lib.at(site, len(expect) // 2)
        if old != bytes.fromhex(expect):
            die("0x%x holds %s, expected %s" % (site, old.hex(), expect))
        patches.append((IMG + site, old, bytes.fromhex(new)))

    icons = [(n, open(os.path.join(HERE, n), "rb").read()) for n in ICONS]
    json.dump({
        "base": BLOB_BASE, "size": BLOB_SIZE, "blob": blob.hex(),
        "patches": [[a, o.hex(), n.hex()] for a, o, n in patches],
        "icons": {n: d.hex() for n, d in icons},
        "syms": {k: v for k, v in syms.items() if not k.startswith("$")},
    }, open(os.path.join(OUT, "missile.json"), "w"))

    # 4. the C++ header
    def arr(name, data):
        rows = ["  " + ", ".join("0x%02x" % b for b in data[i:i + 16]) + ","
                for i in range(0, len(data), 16)]
        return ("const uint8_t %s[%d] = {\n%s\n};\n"
                % (name, len(data), "\n".join(rows)))
    with open(HEADER, "w", newline="\n") as f:
        f.write("// Generated by missile/build.py -- do not edit.\n"
                "// The M and E weapons: a Thumb blob mapped at kMissileBase, the\n"
                "// `bl` rewrites that route the game into it, and the button\n"
                "// art it loads from the files directory.\n"
                "#pragma once\n#include <cstdint>\n\n"
                "namespace shogun {\nnamespace missile {\n\n")
        f.write("constexpr uint32_t kMissileBase = 0x%08x;\n" % BLOB_BASE)
        f.write("constexpr uint32_t kMissileSize = 0x%x;\n\n" % BLOB_SIZE)
        f.write("struct Patch { uint32_t addr; uint8_t len; uint8_t old[4];"
                " uint8_t now[4]; };\n")
        f.write("const Patch kPatches[] = {\n")
        for a, o, n in patches:
            hx = lambda b: ", ".join("0x%02x" % x for x in b.ljust(4, b"\0"))
            f.write("  {0x%08x, %d, {%s}, {%s}},\n" % (a, len(o), hx(o), hx(n)))
        f.write("};\n\n")
        f.write(arr("kBlob", blob) + "\n")
        for i, (n, d) in enumerate(icons):
            f.write(arr("kIcon%d" % i, d) + "\n")
        f.write("struct Icon { const char* name; const uint8_t* data; "
                "uint32_t size; };\n")
        f.write("const Icon kIcons[] = {\n")
        for i, (n, d) in enumerate(icons):
            f.write('  {"%s", kIcon%d, sizeof kIcon%d},\n' % (n, i, i))
        f.write("};\n\n")
        f.write("}  // namespace missile\n}  // namespace shogun\n")

    print("blob %d bytes (+bss to 0x%x), %d patches"
          % (len(blob), end, len(patches)))
    for k in sorted(syms, key=syms.get):
        if k.startswith(("hook_", "shim_", "c_")):
            print("  0x%08x %s" % (syms[k], k))


if __name__ == "__main__":
    main()
