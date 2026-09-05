// Copyright (C) 2026 rockbenben <rockbenben@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "projection.h"
#include "tree_store.h"
#include <QObject>
#include <QTimer>
#include <functional>
#include <obs-frontend-api.h>
#include <obs-hotkey.h>

class TreeDock;

class ObsBridge : public QObject {
	Q_OBJECT
public:
	static ObsBridge *get();
	static void create();
	static void destroy();

	TreeStore store;
	TreeDock *dock = nullptr;

	std::vector<LiveCanvas> liveCanvases() const;
	QString currentSceneUuid() const;   // program
	QString currentPreviewUuid() const;

	void applyTreeOp(const char *undoName, const std::function<bool()> &op);
	void silentTreeOp(const std::function<void()> &op);

	void switchToScene(const QString &uuid);
	void markDirty();

	bool option(const char *key, bool def) const;
	void setOption(const char *key, bool v);

signals:
	void needsRebuild();
	void sceneStateChanged();
	void focusSearchRequested();
private:
	ObsBridge();
	~ObsBridge() override;
	static void frontendEvent(enum obs_frontend_event event, void *ptr);
	static void frontendSaveLoad(obs_data_t *save_data, bool saving, void *ptr);
	static void sourceRenamed(void *ptr, calldata_t *cd);
	static void treeRestore(const char *data);
	bool isMainCanvasScene(const QString &uuid) const;
	void registerHotkeys();
	void unregisterHotkeys();
	void saveHotkeys() const;
	void loadHotkeys();
	static void focusSearchHotkeyCb(void *data, obs_hotkey_id id, obs_hotkey_t *key, bool pressed);
	obs_hotkey_id focusSearchHotkey_ = OBS_INVALID_HOTKEY_ID;
	QTimer saveTimer_;
};
