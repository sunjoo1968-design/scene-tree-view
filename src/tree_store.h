// Copyright (C) 2026 rockbenben <rockbenben@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <QString>
#include <map>
#include <memory>
#include <optional>
#include <vector>

struct TreeNode {
	enum Type { Folder, Scene };
	Type type = Folder;
	QString name;
	QString alias;
	QString uuid;
	QString color;
	bool expanded = true;
	std::vector<std::unique_ptr<TreeNode>> children;
};

using NodePath = std::vector<int>;
inline constexpr int kTreeStoreVersion = 1;

struct LiveScene {
	QString uuid;
	QString name;
};
struct LiveCanvas {
	QString uuid;
	QString name;
	std::vector<LiveScene> scenes;
};

class TreeStore {
public:
	QString nextFolderName(const QString &canvas, const QString &base) const;
	bool insertFolder(const QString &canvas, const NodePath &parent, int index, const QString &name);
	bool renameFolder(const QString &canvas, const NodePath &path, const QString &name);
	bool setSceneAlias(const QString &canvas, const NodePath &path, const QString &alias);
	bool moveSiblingNodes(const QString &canvas, std::vector<NodePath> sources, int direction);
	bool resetToLive(const std::vector<LiveCanvas> &live);
	bool dissolveFolder(const QString &canvas, const NodePath &path);
	bool removeNode(const QString &canvas, const NodePath &path);
	bool moveNodes(const QString &canvas, std::vector<NodePath> sources, const NodePath &destFolder, int destIndex,
		       int *insertedAt = nullptr, int *movedCount = nullptr);
	bool placeScene(const QString &canvas, const QString &sceneUuid, const NodePath &destFolder, int destIndex);
	bool placeMissingScenesAtRoot(const std::vector<LiveCanvas> &live);
	bool setColor(const QString &canvas, const NodePath &path, const QString &color);
	bool setExpanded(const QString &canvas, const NodePath &path, bool expanded);

	const TreeNode *nodeAt(const QString &canvas, const NodePath &path) const;
	std::optional<NodePath> findScene(const QString &canvas, const QString &sceneUuid) const;
	const std::vector<std::unique_ptr<TreeNode>> *canvasRoot(const QString &canvas) const;

	// Keep persisted scene names fresh so copied scene collections can recover changed UUIDs by name.
	void stampSceneNames(const std::map<QString, QString> &uuidToName);

	void resolveAndPrune(const std::vector<LiveCanvas> &live);
	QString toJson() const;
	bool fromJson(const QString &json);
	bool restoreLayout(const QString &json, const std::vector<LiveCanvas> &live);
	bool isForeign() const { return foreign_; }
	void clear();

private:
	TreeNode *
	mutableNodeAt(const QString &canvas, const NodePath &path);
	std::map<QString, TreeNode> roots_;
	bool foreign_ = false;
	QString rawForeign_;
};
