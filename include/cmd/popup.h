	if(islike(param, "/popup.ps3"))
	{
		// /popup.ps3	- show info once
		// /popup.ps3@	- show overlay text using VshFpsCounter.sprx
		// /popup.ps3$	- show persistent info ON
		// /popup.ps3*	- show persistent info OFF
		// /popup.ps3?	- show webman version
		// /popup.ps3&snd=<id>
		// /popup.ps3?<msg>
		// /popup.ps3?<msg>&icon=<id>
		// /popup.ps3$<msg> - show persistent message
		// /popup.ps3*<msg> - show navigation message
		// /popup.ps3?<msg>&snd=<id>
		// /popup.ps3*<msg>&snd=<id>
		// /popup.ps3?<msg>&icon=<id>&snd=<id>
		// /popup.ps3?<msg>&icon=<rsc_icon> (e.g. /popup.ps3?Hello&icon=item_tex_cam_facebook)
		// /popup.ps3?<msg>&icon=<rsc_icon>&rco=<plugin_name> (e.g. /popup.ps3?Hello&icon=item_tex_NewAvc&rco=explore_plugin)
		// /popup.ps3?<rsc_msg>&rco=<plugin_name> (e.g. /popup.ps3?msg_psn&rco=explore_plugin)
		// /popup.ps3?<rsc_msg>&rco=<plugin_name>&icon=<id> (e.g. /popup.ps3?msg_error_hdd_full&rco=system_plugin&icon=5)
		// /popup.ps3=<msg> - return raw message (no html)

		if(param[10] == '\0')
			{show_info_popup = true;}						// show info once
		else if(param[10] == '$')
		{
			show_persistent_popup = PERSIST;
			if(param[11])
				{is_custom_popup = true; strncopy(custom_popup_msg, 200, &param[11]);}
			else
				{show_info_popup = true; BEEP1;}			// show persistent info ON
		}
		else if(param[10] == '*' && param[11] == '\0')
			{if(show_persistent_popup) BEEP2; is_custom_popup = false, show_persistent_popup = 0;}	// show persistent info OFF
		else if(param[10] == '?' && param[11] == '\0')
			{show_wm_version(param);}						// show webman version
		else
			#ifdef FIX_CLOCK
			{
				is_popup = 1;
				char *param2 = param + 11;
				if(islike(param2, "http://"))
				{
					// fork (#3): url used to be copied just past param2 inside param[2048]; a long URL overflowed
					// the stack. Keep it in its own bounded buffer and reject an over-long URL.
					char url[256];
					if(strlen(param2) < sizeof(url))
					{
						strcpy(url, param2); get_server_data(url, param2, HTML_RECV_SIZE - 11 - 1);
					}
				}
			}
			#else
			{is_popup = 1;}
			#endif

		goto html_response;
	}
