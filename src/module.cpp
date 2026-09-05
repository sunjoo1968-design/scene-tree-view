// Copyright (C) 2026 rockbenben <rockbenben@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "obs_bridge.h"
#include "tree_dock.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")
OBS_MODULE_AUTHOR("Sunjoo; original work by rockbenben")

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("SceneAnchor.DockTitle");
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	ObsBridge::create();
	auto *dock = new TreeDock();
	ObsBridge::get()->dock = dock;
	obs_frontend_add_dock_by_id("scene_anchor_dock", obs_module_text("SceneAnchor.DockTitle"), dock);
	return true;
}

void obs_module_unload(void)
{
	ObsBridge::destroy();
}
