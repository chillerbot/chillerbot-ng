
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times

// chiller
MACRO_CONFIG_INT(ClChillerDir, cl_chiller_dir, 0, -1, 1, CFGFLAG_CLIENT, "walk this direction")
MACRO_CONFIG_INT(ClChillerJmp, cl_chiller_jmp, 0, 0, 1000, CFGFLAG_CLIENT, "jump x ticks")
MACRO_CONFIG_INT(ClChillerRender, cl_chiller_render, 0, 0, 1, CFGFLAG_CLIENT, "render map in ascii")
MACRO_CONFIG_INT(ClChillerInput, cl_chiller_inp, 1, 0, 1, CFGFLAG_CLIENT, "user keyboard input like chat and keypresses (set to 0 if the client seems broken)")
MACRO_CONFIG_INT(ClPenTest, cl_pentest, 0, 0, 1, CFGFLAG_CLIENT, "penetrate servers to test stability")
MACRO_CONFIG_STR(ClPenTestFile, cl_pentest_file, 128, "pentest.txt", CFGFLAG_CLIENT, "chat messages file for pentestmode")
MACRO_CONFIG_STR(ClShutdownSrv, cl_shutdown_srv, 128, "", CFGFLAG_CLIENT, "provide password of rcon to shutdown server after x seconds")

// chillerbot (from ux)
MACRO_CONFIG_INT(ClSaveMapInfo, cl_save_map_info, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save map info ( off for tml compability on for everything else)")
MACRO_CONFIG_INT(ClFinishRename, cl_finish_rename, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Change name to cl_finish_name if finish is near.")
MACRO_CONFIG_STR(ClFinishName, cl_finish_name, 32, "chillerbot.png", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Change to this name if cl_finish_rename is active.")
MACRO_CONFIG_INT(ClCampHack, cl_camp_hack, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "0=off 1=mark with gun 2=auto walk")
MACRO_CONFIG_INT(ClAutoReply, cl_auto_reply, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Respond with cl_auto_reply_msg when pinged in chat.")
MACRO_CONFIG_STR(ClAutoReplyMsg, cl_auto_reply_msg, 32, "%n (chillerbot-ux auto reply)", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply when pinged in chat and cl_auto_reply is set to 1")

// warlist
MACRO_CONFIG_INT(ClWarList, cl_war_list, 1, 0, 1, CFGFLAG_CLIENT, "Display nameplate color based on chillerbot/warlist directory")

// remotecontrol
MACRO_CONFIG_INT(ClRemoteControl, cl_remote_control, 0, 0, 1, CFGFLAG_CLIENT, "Execute whisper in local console")
MACRO_CONFIG_STR(ClRemoteControlToken, cl_remote_control_token, 512, "spaces_do_not_work", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Remote control token (see cl_remote_control)")


// client
MACRO_CONFIG_INT(ClPredict, cl_predict, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Predict client movements")
MACRO_CONFIG_INT(ClAntiPingLimit, cl_antiping_limit, 0, 0, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Antiping limit (0 to disable)")
MACRO_CONFIG_INT(ClAntiPing, cl_antiping, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Enable antiping, i. e. more aggressive prediction.")
MACRO_CONFIG_INT(ClAntiPingPlayers, cl_antiping_players, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Predict other player's movement more aggressively (only enabled if cl_antiping is set to 1)")
MACRO_CONFIG_INT(ClAntiPingGrenade, cl_antiping_grenade, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Predict grenades (only enabled if cl_antiping is set to 1)")
MACRO_CONFIG_INT(ClAntiPingWeapons, cl_antiping_weapons, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Predict weapon projectiles (only enabled if cl_antiping is set to 1)")
MACRO_CONFIG_INT(ClNameplates, cl_nameplates, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show name plates")
MACRO_CONFIG_INT(ClNameplatesAlways, cl_nameplates_always, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Always show name plates disregarding of distance")
MACRO_CONFIG_INT(ClNameplatesTeamcolors, cl_nameplates_teamcolors, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use team colors for name plates")
MACRO_CONFIG_INT(ClNameplatesSize, cl_nameplates_size, 50, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Size of the name plates from 0 to 100%")
MACRO_CONFIG_INT(ClNameplatesClan, cl_nameplates_clan, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show clan in name plates")
MACRO_CONFIG_INT(ClNameplatesClanSize, cl_nameplates_clan_size, 30, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Size of the clan plates from 0 to 100%")
MACRO_CONFIG_INT(ClNameplatesOwn, cl_nameplates_own, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show own name plate (useful for demo recording)")
MACRO_CONFIG_INT(ClTextEntities, cl_text_entities, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Render textual entity data")
#if defined(__ANDROID__)
MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Auto switch weapon on pickup")
MACRO_CONFIG_INT(ClAutoswitchWeaponsOutOfAmmo, cl_autoswitch_weapons_out_of_ammo, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Auto switch weapon when out of ammo")
#else
MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Auto switch weapon on pickup")
MACRO_CONFIG_INT(ClAutoswitchWeaponsOutOfAmmo, cl_autoswitch_weapons_out_of_ammo, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Auto switch weapon when out of ammo")
#endif

MACRO_CONFIG_INT(ClShowhud, cl_showhud, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame HUD")
MACRO_CONFIG_INT(ClShowhudHealthAmmo, cl_showhud_healthammo, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame HUD (Health + Ammo)")
MACRO_CONFIG_INT(ClShowhudScore, cl_showhud_score, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame HUD (Score)")
MACRO_CONFIG_INT(ClShowRecord, cl_showrecord, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show old style DDRace client records")
MACRO_CONFIG_INT(ClShowNotifications, cl_shownotifications, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Make the client notify when someone highlights you")
MACRO_CONFIG_INT(ClShowEmotes, cl_showemotes, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show tee emotes")
MACRO_CONFIG_INT(ClShowChat, cl_showchat, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show chat")
MACRO_CONFIG_INT(ClShowChatFriends, cl_show_chat_friends, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show only chat messages from friends")
MACRO_CONFIG_INT(ClShowChatSystem, cl_show_chat_system, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show chat messages from the server")
MACRO_CONFIG_INT(ClShowKillMessages, cl_showkillmessages, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show kill messages")
MACRO_CONFIG_INT(ClShowVotesAfterVoting, cl_show_votes_after_voting, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show votes window after voting")
MACRO_CONFIG_INT(ClShowLocalTimeAlways, cl_show_local_time_always, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Always show local time")
MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame FPS counter")
MACRO_CONFIG_INT(ClShowpred, cl_showpred, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame prediction time in milliseconds")
MACRO_CONFIG_INT(ClEyeWheel, cl_eye_wheel, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show eye wheel along together with emotes")
MACRO_CONFIG_INT(ClEyeDuration, cl_eye_duration, 999999, 1, 999999, CFGFLAG_CLIENT|CFGFLAG_SAVE, "How long the eyes emotes last")

MACRO_CONFIG_INT(ClAirjumpindicator, cl_airjumpindicator, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClThreadsoundloading, cl_threadsoundloading, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Load sound files threaded")

MACRO_CONFIG_INT(ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Warn about team balance")

MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 0, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 0, 0, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMouseMaxDistance, cl_mouse_max_distance, 400, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(ClDyncam, cl_dyncam, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Enable dyncam")
MACRO_CONFIG_INT(ClDyncamMaxDistance, cl_dyncam_max_distance, 1000, 0, 2000, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Maximal dynamic camera distance")
MACRO_CONFIG_INT(ClDyncamMousesens, cl_dyncam_mousesens, 0, 0, 100000, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Mouse sens used when dyncam is toggled on")
MACRO_CONFIG_INT(ClDyncamDeadzone, cl_dyncam_deadzone, 300, 1, 1300, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Dynamic camera dead zone")
MACRO_CONFIG_INT(ClDyncamFollowFactor, cl_dyncam_follow_factor, 60, 0, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Dynamic camera follow factor")

MACRO_CONFIG_INT(EdZoomTarget, ed_zoom_target, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Zoom to the current mouse target")
MACRO_CONFIG_INT(EdShowkeys, ed_showkeys, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(ClShowWelcome, cl_show_welcome, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMotdTime, cl_motd_time, 10, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "How long to show the server message of the day")

MACRO_CONFIG_STR(ClDDNetMapDownloadUrl, cl_ddnet_map_download_url, 100, "https://maps.ddnet.tw", CFGFLAG_CLIENT|CFGFLAG_SAVE, "URL to use to download maps (can start with http:// or https://)")

MACRO_CONFIG_STR(ClLanguagefile, cl_languagefile, 255, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "What language file to use")
MACRO_CONFIG_INT(ClVanillaSkinsOnly, cl_vanilla_skins_only, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Only show skins available in Vanilla Teeworlds")
MACRO_CONFIG_INT(ClAutoStatboardScreenshot, cl_auto_statboard_screenshot, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Automatically take game over statboard screenshot")
MACRO_CONFIG_INT(ClAutoStatboardScreenshotMax, cl_auto_statboard_screenshot_max, 10, 0, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Maximum number of automatically created statboard screenshots (0 = no limit)")

MACRO_CONFIG_INT(ClDefaultZoom, cl_default_zoom, 10, 0, 20, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Default zoom level (10 default, min 0, max 20)")

MACRO_CONFIG_INT(ClPlayerUseCustomColor, player_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors")
MACRO_CONFIG_INT(ClPlayerColorBody, player_color_body, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player body color")
MACRO_CONFIG_INT(ClPlayerColorFeet, player_color_feet, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player feet color")
MACRO_CONFIG_STR(ClPlayerSkin, player_skin, 24, "default", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin")
MACRO_CONFIG_STR(ClSkinPrefix, cl_skin_prefix, 100, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Replace the skins by skins with this prefix (e.g. kitty, coala, santa)")
MACRO_CONFIG_INT(ClFatSkins, cl_fat_skins, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Enable fat skins")

MACRO_CONFIG_INT(UiPage, ui_page, 9, 0, 11, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface page")
MACRO_CONFIG_INT(UiToolboxPage, ui_toolbox_page, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toolbox page")
MACRO_CONFIG_STR(UiServerAddress, ui_server_address, 64, "localhost:8303", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface server address")
MACRO_CONFIG_INT(UiScale, ui_scale, 100, 50, 150, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface scale")
MACRO_CONFIG_INT(UiMousesens, ui_mousesens, 200, 1, 100000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Mouse sensitivity for menus/editor")

MACRO_CONFIG_INT(UiColorHue, ui_color_hue, 160, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color hue")
MACRO_CONFIG_INT(UiColorSat, ui_color_sat, 70, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color saturation")
MACRO_CONFIG_INT(UiColorLht, ui_color_lht, 175, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color lightness")
MACRO_CONFIG_INT(UiColorAlpha, ui_color_alpha, 228, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface alpha")

MACRO_CONFIG_INT(UiColorizePing, ui_colorize_ping, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Highlight ping")
MACRO_CONFIG_INT(UiColorizeGametype, ui_colorize_gametype, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Highlight gametype")

MACRO_CONFIG_STR(UiDemoSelected, ui_demo_selected, 256, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Selected demo file")
MACRO_CONFIG_INT(UiCloseWindowAfterChangingSetting, ui_close_window_after_changing_setting, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Close window after changing setting")

MACRO_CONFIG_INT(GfxNoclip, gfx_noclip, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Disable clipping")

// dummy
MACRO_CONFIG_STR(ClDummyName, dummy_name, 16, "brainless tee", CFGFLAG_SAVE|CFGFLAG_CLIENT, "Name of the Dummy")
MACRO_CONFIG_STR(ClDummyClan, dummy_clan, 12, "", CFGFLAG_SAVE|CFGFLAG_CLIENT, "Clan of the Dummy")
MACRO_CONFIG_INT(ClDummyCountry, dummy_country, -1, -1, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Country of the Dummy")
MACRO_CONFIG_INT(ClDummyUseCustomColor, dummy_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors")
MACRO_CONFIG_INT(ClDummyColorBody, dummy_color_body, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Dummy body color")
MACRO_CONFIG_INT(ClDummyColorFeet, dummy_color_feet, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Dummy feet color")
MACRO_CONFIG_STR(ClDummySkin, dummy_skin, 24, "default", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Dummy skin")
MACRO_CONFIG_INT(ClDummy, cl_dummy, 0, 0, 1, CFGFLAG_CLIENT, "0 - player / 1 - dummy")
MACRO_CONFIG_INT(ClDummyHammer, cl_dummy_hammer, 0, 0, 1, CFGFLAG_CLIENT, "Whether dummy is hammering for a hammerfly")
MACRO_CONFIG_INT(ClDummyResetOnSwitch, cl_dummy_resetonswitch, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Whether dummy should stop pressing keys when you switch")
MACRO_CONFIG_INT(ClDummyCopyMoves, cl_dummy_copy_moves, 0, 0, 1, CFGFLAG_CLIENT, "Whether dummy should copy your moves")

// more controlable dummy command
MACRO_CONFIG_INT(ClDummyControl, cl_dummy_control, 0, 0, 1, CFGFLAG_CLIENT, "Whether can you control dummy at the same time")
MACRO_CONFIG_INT(ClDummyJump, cl_dummy_jump, 0, 0, 1, CFGFLAG_CLIENT, "Whether dummy is jumping")
MACRO_CONFIG_INT(ClDummyFire, cl_dummy_fire, 0, 0, 1, CFGFLAG_CLIENT, "Whether dummy is firing")
MACRO_CONFIG_INT(ClDummyHook, cl_dummy_hook, 0, 0, 1, CFGFLAG_CLIENT, "Whether dummy is hooking")

// curl http download
MACRO_CONFIG_INT(ClHTTPConnectTimeoutMs, cl_http_connect_timeout_ms, 2000, 0, 100000, CFGFLAG_CLIENT|CFGFLAG_SAVE, "HTTP downloads: timeout for the connect phase in milliseconds (0 to disable)")
MACRO_CONFIG_INT(ClHTTPLowSpeedLimit, cl_http_low_speed_limit, 500, 0, 100000, CFGFLAG_CLIENT|CFGFLAG_SAVE, "HTTP downloads: Set low speed limit in bytes per second (0 to disable)")
MACRO_CONFIG_INT(ClHTTPLowSpeedTime, cl_http_low_speed_time, 5, 0, 100000, CFGFLAG_CLIENT|CFGFLAG_SAVE, "HTTP downloads: Set low speed limit time period (0 to disable)")

// server
MACRO_CONFIG_INT(SvWarmup, sv_warmup, 0, 0, 0, CFGFLAG_SERVER, "Number of seconds to do warmup before round starts")
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 1, CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SERVER, "Spam protection")

MACRO_CONFIG_INT(SvSpectatorSlots, sv_spectator_slots, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Number of slots to reserve for spectators")
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 0, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive players")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 0, 0, 2, CFGFLAG_SERVER, "How to deal with inactive players (0=move to spectator, 1=move to free spectator slot/kick, 2=kick)")

MACRO_CONFIG_INT(SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SERVER, "Restricts information in spectator mode")
MACRO_CONFIG_INT(SvVoteSpectate, sv_vote_spectate, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to move players to spectators")
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay, sv_vote_spectate_rejoindelay, 3, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before a player can rejoin after being moved to spectators by vote")
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to kick players")
MACRO_CONFIG_INT(SvVoteKickMin, sv_vote_kick_min, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Minimum number of players required to start a kick vote")
MACRO_CONFIG_INT(SvVoteKickBantime, sv_vote_kick_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time in seconds to ban a player if kicked by vote. 0 makes it just use kick")
MACRO_CONFIG_INT(SvJoinVoteDelay, sv_join_vote_delay, 300, 0, 1000, CFGFLAG_SERVER, "Add a delay before recently joined players can call any vote or participate in a kick/spec vote (in seconds)")
MACRO_CONFIG_INT(SvOldTeleportWeapons, sv_old_teleport_weapons, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Teleporting of all weapons (deprecated, use special entities instead)")
MACRO_CONFIG_INT(SvOldTeleportHook, sv_old_teleport_hook, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Hook through teleporter (deprecated, use special entities instead)")
MACRO_CONFIG_INT(SvTeleportHoldHook, sv_teleport_hold_hook, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Hold hook when teleported")
MACRO_CONFIG_INT(SvTeleportLoseWeapons, sv_teleport_lose_weapons, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Lose weapons when teleported (useful for some race maps)")
MACRO_CONFIG_INT(SvDeepfly, sv_deepfly, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Allow fire non auto weapons when deep")

MACRO_CONFIG_INT(SvMapUpdateRate, sv_mapupdaterate, 5, 1, 100, CFGFLAG_SERVER, "64 player id <-> vanilla id players map update rate")

MACRO_CONFIG_INT(SvSkinStealAction, sv_skinstealaction, 0, 0, 1, CFGFLAG_SERVER, "How to punish skin stealing (currently only 1 = force pinky)")

MACRO_CONFIG_STR(SvServerType, sv_server_type, 64, "none", CFGFLAG_SERVER, "Type of the server (novice, moderate, ...)")

MACRO_CONFIG_INT(SvSendVotesPerTick, sv_send_votes_per_tick, 5, 1, 15, CFGFLAG_SERVER, "Number of vote options being send per tick")

MACRO_CONFIG_INT(SvRescue, sv_rescue, 0, 0, 1, CFGFLAG_SERVER, "Allow /rescue command so players can teleport themselves out of freeze")
MACRO_CONFIG_INT(SvRescueDelay, sv_rescue_delay, 5, 0, 1000, CFGFLAG_SERVER, "Number of seconds between two rescues")

// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
	MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, 15, CFGFLAG_SERVER, "")
#endif

MACRO_CONFIG_INT(DbgFocus, dbg_focus, 0, 0, 1, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 1, CFGFLAG_CLIENT, "")
#endif
