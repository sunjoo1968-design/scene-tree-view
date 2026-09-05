// Copyright (C) 2026 rockbenben <rockbenben@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "obs_bridge.h"
#include "tree_dock.h"
#include <vector>
#include <obs-module.h>
#include <obs.h>
#include <plugin-support.h>
#include <util/config-file.h>

static ObsBridge *g_bridge = nullptr;
ObsBridge *ObsBridge::get()
{
	return g_bridge;
}
void ObsBridge::create()
{
	if (!g_bridge)
		g_bridge = new ObsBridge();
}
void ObsBridge::destroy()
{
	delete g_bridge;
	g_bridge = nullptr;
}

namespace {
int countNodes(const QString &json)
{
	return json.count(QLatin1String("\"t\":"));
}
} // namespace

ObsBridge::ObsBridge()
{
	saveTimer_.setSingleShot(true);
	saveTimer_.setInterval(2000);
	connect(&saveTimer_, &QTimer::timeout, [] { obs_frontend_save(); });
	obs_frontend_add_event_callback(&ObsBridge::frontendEvent, this);
	obs_frontend_add_save_callback(&ObsBridge::frontendSaveLoad, this);
	signal_handler_connect(obs_get_signal_handler(), "source_rename", &ObsBridge::sourceRenamed, this);
	registerHotkeys();
}

ObsBridge::~ObsBridge()
{
	unregisterHotkeys();
	signal_handler_disconnect(obs_get_signal_handler(), "source_rename", &ObsBridge::sourceRenamed, this);
	obs_frontend_remove_save_callback(&ObsBridge::frontendSaveLoad, this);
	obs_frontend_remove_event_callback(&ObsBridge::frontendEvent, this);
}

void ObsBridge::markDirty()
{
	saveTimer_.start();
}

void ObsBridge::frontendSaveLoad(obs_data_t *save_data, bool saving, void *ptr)
{
	auto *b = static_cast<ObsBridge *>(ptr);
	if (saving) {
		std::map<QString, QString> uuidToName;
		for (const auto &cv : b->liveCanvases())
			for (const auto &sc : cv.scenes)
				uuidToName[sc.uuid] = sc.name;
		b->store.stampSceneNames(uuidToName);

		obs_data_set_string(save_data, "scene_anchor", b->store.toJson().toUtf8().constData());
		b->saveHotkeys();
	} else {
		const char *json = obs_data_get_string(save_data, "scene_anchor");
		if (json && *json)
			b->store.fromJson(QString::fromUtf8(json));
		else
			b->store.clear();
	}
}

static void pruneToLive(ObsBridge *b)
{
	b->store.resolveAndPrune(b->liveCanvases());
}

void ObsBridge::frontendEvent(enum obs_frontend_event event, void *ptr)
{
	auto *b = static_cast<ObsBridge *>(ptr);
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		b->loadHotkeys();
		[[fallthrough]];
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		pruneToLive(b);
		emit b->needsRebuild();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		b->store.clear();
		emit b->needsRebuild();
		break;
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		emit b->needsRebuild();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED: {
		emit b->sceneStateChanged();
		break;
	}
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		emit b->sceneStateChanged();
		break;
	case OBS_FRONTEND_EVENT_THEME_CHANGED:
		emit b->needsRebuild();
		break;
	default:
		break;
	}
}

void ObsBridge::sourceRenamed(void *ptr, calldata_t *cd)
{
	auto *b = static_cast<ObsBridge *>(ptr);
	obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!obs_scene_from_source(source))
		return;
	QMetaObject::invokeMethod(b, [b] { emit b->needsRebuild(); }, Qt::QueuedConnection);
}

std::vector<LiveCanvas> ObsBridge::liveCanvases() const
{
	std::vector<LiveCanvas> out;
	obs_canvas_t *main = obs_get_main_canvas();
	if (!main)
		return out;
	const QString mainUuid = QString::fromUtf8(obs_canvas_get_uuid(main));
	LiveCanvas lc;
	lc.uuid = mainUuid;
	struct obs_frontend_source_list sl = {};
	obs_frontend_get_scenes(&sl);
	for (size_t j = 0; j < sl.sources.num; j++) {
		obs_source_t *s = sl.sources.array[j];
		obs_canvas_t *sc = obs_source_get_canvas(s);
		const bool isMain = !sc || QString::fromUtf8(obs_canvas_get_uuid(sc)) == mainUuid;
		obs_canvas_release(sc);
		if (isMain)
			lc.scenes.push_back(
				{QString::fromUtf8(obs_source_get_uuid(s)), QString::fromUtf8(obs_source_get_name(s))});
	}
	obs_frontend_source_list_free(&sl);
	out.push_back(std::move(lc));
	obs_canvas_release(main);
	return out;
}

QString ObsBridge::currentSceneUuid() const
{
	obs_source_t *s = obs_frontend_get_current_scene();
	if (!s)
		return {};
	const QString u = QString::fromUtf8(obs_source_get_uuid(s));
	obs_source_release(s);
	return u;
}

QString ObsBridge::currentPreviewUuid() const
{
	if (!obs_frontend_preview_program_mode_active())
		return {};
	obs_source_t *s = obs_frontend_get_current_preview_scene();
	if (!s)
		return {};
	const QString u = QString::fromUtf8(obs_source_get_uuid(s));
	obs_source_release(s);
	return u;
}

bool ObsBridge::isMainCanvasScene(const QString &uuid) const
{
	obs_source_t *s = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (!s)
		return false;
	obs_canvas_t *sc = obs_source_get_canvas(s);
	obs_source_release(s);
	if (!sc)
		return true;
	obs_canvas_t *main = obs_get_main_canvas();
	if (!main) {
		obs_canvas_release(sc);
		return false;
	}
	const bool isMain = QString::fromUtf8(obs_canvas_get_uuid(sc)) == QString::fromUtf8(obs_canvas_get_uuid(main));
	obs_canvas_release(sc);
	obs_canvas_release(main);
	return isMain;
}

void ObsBridge::switchToScene(const QString &uuid)
{
	if (!isMainCanvasScene(uuid)) {
		obs_log(LOG_WARNING, "switchToScene: refused non-main-canvas scene %s", uuid.toUtf8().constData());
		return;
	}
	obs_source_t *s = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (!s)
		return;
	if (obs_frontend_preview_program_mode_active())
		obs_frontend_set_current_preview_scene(s);
	else
		obs_frontend_set_current_scene(s);
	obs_source_release(s);
}


static const char *kHotkeyFocusSearch = "SceneAnchor.FocusSearch";
static const char *kHotkeyCfgKey = "FocusSearchHotkey";

void ObsBridge::focusSearchHotkeyCb(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *b = static_cast<ObsBridge *>(data);
	QMetaObject::invokeMethod(b, [b] { emit b->focusSearchRequested(); }, Qt::QueuedConnection);
}

void ObsBridge::registerHotkeys()
{
	if (focusSearchHotkey_ != OBS_INVALID_HOTKEY_ID)
		return;
	focusSearchHotkey_ = obs_hotkey_register_frontend(
		kHotkeyFocusSearch, obs_module_text("SceneAnchor.Hotkey.FocusSearch"), focusSearchHotkeyCb, this);
}

void ObsBridge::unregisterHotkeys()
{
	if (focusSearchHotkey_ == OBS_INVALID_HOTKEY_ID)
		return;
	obs_hotkey_unregister(focusSearchHotkey_);
	focusSearchHotkey_ = OBS_INVALID_HOTKEY_ID;
}

void ObsBridge::saveHotkeys() const
{
	if (focusSearchHotkey_ == OBS_INVALID_HOTKEY_ID)
		return;
	config_t *c = obs_frontend_get_user_config();
	if (!c)
		return;

	obs_data_array_t *arr = obs_hotkey_save(focusSearchHotkey_);
	if (!arr)
		return;

	obs_data_t *wrap = obs_data_create();
	obs_data_set_array(wrap, "bindings", arr);
	config_set_string(c, "SceneAnchor", kHotkeyCfgKey, obs_data_get_json(wrap));
	config_save(c);
	obs_data_release(wrap);
	obs_data_array_release(arr);
}

void ObsBridge::loadHotkeys()
{
	if (focusSearchHotkey_ == OBS_INVALID_HOTKEY_ID)
		return;
	config_t *c = obs_frontend_get_user_config();
	if (!c)
		return;
	const char *json = config_get_string(c, "SceneAnchor", kHotkeyCfgKey);
	if (!json || !*json)
		return;
	obs_data_t *wrap = obs_data_create_from_json(json);
	if (!wrap)
		return;
	obs_data_array_t *arr = obs_data_get_array(wrap, "bindings");
	if (arr) {
		obs_hotkey_load(focusSearchHotkey_, arr);
		obs_data_array_release(arr);
	}
	obs_data_release(wrap);
}


bool ObsBridge::option(const char *key, bool def) const
{
	config_t *c = obs_frontend_get_user_config();
	if (!c)
		return def;
	config_set_default_bool(c, "SceneAnchor", key, def);
	return config_get_bool(c, "SceneAnchor", key);
}

void ObsBridge::setOption(const char *key, bool v)
{
	config_t *c = obs_frontend_get_user_config();
	if (!c)
		return;
	config_set_bool(c, "SceneAnchor", key, v);
	config_save(c);
}

void ObsBridge::treeRestore(const char *data)
{
	auto *b = get();
	if (!b)
		return;
	const QString json = QString::fromUtf8(data);
	b->store.fromJson(json);
	obs_log(LOG_INFO, "treeRestore: tree now has %d node(s)", countNodes(json));
	b->markDirty();
	emit b->needsRebuild();
}

void ObsBridge::applyTreeOp(const char *undoName, const std::function<bool()> &op)
{
	if (option("LayoutLocked", false))
		return;
	const QString before = store.toJson();
	if (!op())
		return;
	const QString after = store.toJson();
	if (before == after)
		return;
	obs_frontend_add_undo_redo_action(undoName, &ObsBridge::treeRestore, &ObsBridge::treeRestore,
					  before.toUtf8().constData(), after.toUtf8().constData(), false);
	obs_log(LOG_INFO, "applyTreeOp[%s]: %d -> %d node(s)", undoName, countNodes(before), countNodes(after));
	markDirty();
	emit needsRebuild();
}

void ObsBridge::silentTreeOp(const std::function<void()> &op)
{
	op();
	markDirty();
}
