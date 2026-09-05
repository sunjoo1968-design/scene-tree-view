// Copyright (C) 2026 rockbenben <rockbenben@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tree_store.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <algorithm>

static QJsonObject nodeToJson(const TreeNode &n)
{
	QJsonObject o;
	if (n.type == TreeNode::Folder) {
		o["t"] = "folder";
		o["name"] = n.name;
		o["expanded"] = n.expanded;
		QJsonArray kids;
		for (const auto &c : n.children)
			kids.append(nodeToJson(*c));
		o["children"] = kids;
	} else {
		o["t"] = "scene";
		o["uuid"] = n.uuid;
		if (!n.alias.isEmpty())
			o["alias"] = n.alias;
		if (!n.name.isEmpty())
			o["name"] = n.name;
	}
	if (!n.color.isEmpty())
		o["color"] = n.color;
	return o;
}

static std::unique_ptr<TreeNode> nodeFromJson(const QJsonObject &o)
{
	auto n = std::make_unique<TreeNode>();
	const QString t = o["t"].toString();
	n->color = o["color"].toString();
	if (t == QLatin1String("folder")) {
		n->type = TreeNode::Folder;
		n->name = o["name"].toString();
		n->expanded = o["expanded"].toBool(true);
		for (const auto v : o["children"].toArray())
			if (v.isObject())
				if (auto c = nodeFromJson(v.toObject()))
					n->children.push_back(std::move(c));
	} else if (t == QLatin1String("scene")) {
		n->type = TreeNode::Scene;
		n->uuid = o["uuid"].toString();
		n->alias = o["alias"].toString();
		if (n->uuid.isEmpty())
			return nullptr;
		n->name = o["name"].toString();
	} else {
		return nullptr;
	}
	return n;
}

TreeNode *TreeStore::mutableNodeAt(const QString &canvas, const NodePath &path)
{
	if (foreign_)
		return nullptr;
	auto it = roots_.find(canvas);
	if (it == roots_.end()) {
		if (!path.empty())
			return nullptr;
		it = roots_.emplace(canvas, TreeNode{}).first;
	}
	TreeNode *n = &it->second;
	for (int idx : path) {
		if (n->type != TreeNode::Folder || idx < 0 || idx >= (int)n->children.size())
			return nullptr;
		n = n->children[idx].get();
	}
	return n;
}

const TreeNode *TreeStore::nodeAt(const QString &canvas, const NodePath &path) const
{
	auto it = roots_.find(canvas);
	if (it == roots_.end())
		return nullptr;
	const TreeNode *n = &it->second;
	for (int idx : path) {
		if (n->type != TreeNode::Folder || idx < 0 || idx >= (int)n->children.size())
			return nullptr;
		n = n->children[idx].get();
	}
	return n;
}

const std::vector<std::unique_ptr<TreeNode>> *TreeStore::canvasRoot(const QString &canvas) const
{
	auto it = roots_.find(canvas);
	return it == roots_.end() ? nullptr : &it->second.children;
}

QString TreeStore::nextFolderName(const QString &canvas, const QString &base) const
{
	QSet<QString> names;
	std::vector<const TreeNode *> pending;
	if (const auto *root = nodeAt(canvas, {}))
		pending.push_back(root);
	while (!pending.empty()) {
		const TreeNode *folder = pending.back();
		pending.pop_back();
		for (const auto &child : folder->children) {
			if (child->type == TreeNode::Folder) {
				names.insert(child->name);
				pending.push_back(child.get());
			}
		}
	}
	for (qulonglong number = 1;; ++number) {
		const QString candidate = base + QString::number(number);
		if (!names.contains(candidate))
			return candidate;
	}
}

bool TreeStore::insertFolder(const QString &canvas, const NodePath &parent, int index, const QString &name)
{
	TreeNode *p = mutableNodeAt(canvas, parent);
	if (!p || p->type != TreeNode::Folder)
		return false;
	auto n = std::make_unique<TreeNode>();
	n->type = TreeNode::Folder;
	n->name = name;
	const int i = std::clamp(index, 0, (int)p->children.size());
	p->children.insert(p->children.begin() + i, std::move(n));
	return true;
}

bool TreeStore::renameFolder(const QString &canvas, const NodePath &path, const QString &name)
{
	if (path.empty() || name.isEmpty())
		return false;
	TreeNode *n = mutableNodeAt(canvas, path);
	if (!n || n->type != TreeNode::Folder)
		return false;
	n->name = name;
	return true;
}

bool TreeStore::setSceneAlias(const QString &canvas, const NodePath &path, const QString &alias)
{
	if (path.empty())
		return false;
	TreeNode *n = mutableNodeAt(canvas, path);
	if (!n || n->type != TreeNode::Scene)
		return false;
	n->alias = alias.trimmed();
	return true;
}

bool TreeStore::resetToLive(const std::vector<LiveCanvas> &live)
{
	if (foreign_)
		return false;
	clear();
	placeMissingScenesAtRoot(live);
	return true;
}

bool TreeStore::moveSiblingNodes(const QString &canvas, std::vector<NodePath> sources, int direction)
{
	if (foreign_ || sources.empty() || (direction != -1 && direction != 1))
		return false;
	std::sort(sources.begin(), sources.end());
	sources.erase(std::unique(sources.begin(), sources.end()), sources.end());
	if (sources.front().empty())
		return false;
	const NodePath parent(sources.front().begin(), sources.front().end() - 1);
	for (const auto &path : sources) {
		if (path.empty() || NodePath(path.begin(), path.end() - 1) != parent || !nodeAt(canvas, path))
			return false;
	}
	TreeNode *p = mutableNodeAt(canvas, parent);
	if (!p || p->type != TreeNode::Folder)
		return false;
	if ((direction < 0 && sources.front().back() == 0) ||
	    (direction > 0 && sources.back().back() == (int)p->children.size() - 1))
		return false;
	if (direction < 0) {
		for (const auto &path : sources)
			std::swap(p->children[path.back()], p->children[path.back() - 1]);
	} else {
		for (auto it = sources.rbegin(); it != sources.rend(); ++it)
			std::swap(p->children[it->back()], p->children[it->back() + 1]);
	}
	return true;
}

bool TreeStore::dissolveFolder(const QString &canvas, const NodePath &path)
{
	if (path.empty())
		return false;
	TreeNode *n = mutableNodeAt(canvas, path);
	if (!n || n->type != TreeNode::Folder)
		return false;
	NodePath parentPath(path.begin(), path.end() - 1);
	TreeNode *p = mutableNodeAt(canvas, parentPath);
	if (!p)
		return false;
	const int idx = path.back();
	auto folder = std::move(p->children[idx]);
	p->children.erase(p->children.begin() + idx);
	for (size_t i = 0; i < folder->children.size(); ++i)
		p->children.insert(p->children.begin() + idx + i, std::move(folder->children[i]));
	return true;
}

bool TreeStore::removeNode(const QString &canvas, const NodePath &path)
{
	if (path.empty())
		return false;
	if (!mutableNodeAt(canvas, path))
		return false;
	NodePath parentPath(path.begin(), path.end() - 1);
	TreeNode *p = mutableNodeAt(canvas, parentPath);
	if (!p)
		return false;
	p->children.erase(p->children.begin() + path.back());
	return true;
}

static bool isPrefixOf(const NodePath &a, const NodePath &b)
{
	return a.size() <= b.size() && std::equal(a.begin(), a.end(), b.begin());
}

bool TreeStore::moveNodes(const QString &canvas, std::vector<NodePath> sources, const NodePath &destFolder,
			  int destIndex, int *insertedAt, int *movedCount)
{
	if (foreign_ || sources.empty())
		return false;
	const TreeNode *destCheck = nodeAt(canvas, destFolder);
	if (!destCheck || destCheck->type != TreeNode::Folder)
		return false;
	std::sort(sources.begin(), sources.end());
	std::vector<NodePath> tops;
	for (const auto &s : sources) {
		if (s.empty())
			return false;
		if (!tops.empty() && isPrefixOf(tops.back(), s))
			continue;
		tops.push_back(s);
	}
	for (const auto &s : tops) {
		if (isPrefixOf(s, destFolder))
			return false;
		if (!nodeAt(canvas, s))
			return false;
		NodePath par(s.begin(), s.end() - 1);
		if (!nodeAt(canvas, par))
			return false;
	}
	TreeNode *dest = mutableNodeAt(canvas, destFolder);
	if (!dest)
		return false;
	int adjusted = std::clamp(destIndex, 0, (int)dest->children.size());
	const int threshold = adjusted;
	for (const auto &s : tops) {
		NodePath par(s.begin(), s.end() - 1);
		if (par == destFolder && s.back() < threshold)
			--adjusted;
	}
	std::vector<std::unique_ptr<TreeNode>> grabbed(tops.size());
	for (int i = (int)tops.size() - 1; i >= 0; --i) {
		const NodePath &s = tops[i];
		NodePath par(s.begin(), s.end() - 1);
		TreeNode *p = mutableNodeAt(canvas, par);
		grabbed[i] = std::move(p->children[s.back()]);
		p->children.erase(p->children.begin() + s.back());
	}
	for (size_t i = 0; i < grabbed.size(); ++i)
		dest->children.insert(dest->children.begin() + adjusted + i, std::move(grabbed[i]));
	if (insertedAt)
		*insertedAt = adjusted;
	if (movedCount)
		*movedCount = (int)grabbed.size();
	return true;
}

static bool dfsFind(const TreeNode &folder, const QString &uuid, NodePath &path)
{
	for (int i = 0; i < (int)folder.children.size(); ++i) {
		const TreeNode &n = *folder.children[i];
		path.push_back(i);
		if (n.type == TreeNode::Scene && n.uuid == uuid)
			return true;
		if (n.type == TreeNode::Folder && dfsFind(n, uuid, path))
			return true;
		path.pop_back();
	}
	return false;
}

std::optional<NodePath> TreeStore::findScene(const QString &canvas, const QString &sceneUuid) const
{
	auto it = roots_.find(canvas);
	if (it == roots_.end())
		return std::nullopt;
	NodePath p;
	if (dfsFind(it->second, sceneUuid, p))
		return p;
	return std::nullopt;
}

bool TreeStore::placeScene(const QString &canvas, const QString &sceneUuid, const NodePath &destFolder, int destIndex)
{
	if (foreign_ || sceneUuid.isEmpty())
		return false;
	if (auto existing = findScene(canvas, sceneUuid))
		return moveNodes(canvas, {*existing}, destFolder, destIndex);
	TreeNode *dest = mutableNodeAt(canvas, destFolder);
	if (!dest || dest->type != TreeNode::Folder)
		return false;
	auto n = std::make_unique<TreeNode>();
	n->type = TreeNode::Scene;
	n->uuid = sceneUuid;
	const int i = std::clamp(destIndex, 0, (int)dest->children.size());
	dest->children.insert(dest->children.begin() + i, std::move(n));
	return true;
}

bool TreeStore::setColor(const QString &canvas, const NodePath &path, const QString &color)
{
	if (path.empty())
		return false;
	TreeNode *n = mutableNodeAt(canvas, path);
	if (!n)
		return false;
	n->color = color;
	return true;
}

bool TreeStore::setExpanded(const QString &canvas, const NodePath &path, bool expanded)
{
	if (path.empty())
		return false;
	TreeNode *n = mutableNodeAt(canvas, path);
	if (!n || n->type != TreeNode::Folder)
		return false;
	n->expanded = expanded;
	return true;
}

static void pruneScenesRec(TreeNode &folder, const QSet<QString> &live)
{
	auto &v = folder.children;
	v.erase(std::remove_if(v.begin(), v.end(),
			       [&](const std::unique_ptr<TreeNode> &c) {
				       return c->type == TreeNode::Scene && !live.contains(c->uuid);
			       }),
		v.end());
	for (auto &c : v)
		if (c->type == TreeNode::Folder)
			pruneScenesRec(*c, live);
}

static void stampRec(TreeNode &folder, const std::map<QString, QString> &uuidToName)
{
	for (auto &c : folder.children) {
		if (c->type == TreeNode::Scene) {
			if (auto it = uuidToName.find(c->uuid); it != uuidToName.end())
				c->name = it->second;
		} else {
			stampRec(*c, uuidToName);
		}
	}
}

void TreeStore::stampSceneNames(const std::map<QString, QString> &uuidToName)
{
	if (foreign_)
		return;
	for (auto &[uuid, root] : roots_)
		stampRec(root, uuidToName);
}

static void claimLiveRec(const TreeNode &folder, const QSet<QString> &liveScenes, QSet<QString> &claimed)
{
	for (const auto &c : folder.children) {
		if (c->type == TreeNode::Folder)
			claimLiveRec(*c, liveScenes, claimed);
		else if (liveScenes.contains(c->uuid))
			claimed.insert(c->uuid);
	}
}

static void resolveRec(TreeNode &folder, const QSet<QString> &liveScenes, const std::map<QString, QString> &liveNames,
		       QSet<QString> &claimed)
{
	for (auto &c : folder.children) {
		if (c->type == TreeNode::Folder) {
			resolveRec(*c, liveScenes, liveNames, claimed);
			continue;
		}
		if (liveScenes.contains(c->uuid))
			continue;
		if (c->name.isEmpty())
			continue;
		if (auto it = liveNames.find(c->name); it != liveNames.end() && !claimed.contains(it->second)) {
			c->uuid = it->second;
			claimed.insert(c->uuid);
		}
	}
}

void TreeStore::resolveAndPrune(const std::vector<LiveCanvas> &live)
{
	if (foreign_)
		return;
	if (live.empty())
		return;
	std::map<QString, QSet<QString>> scenesByCanvas;
	std::map<QString, std::map<QString, QString>> namesByCanvas;
	for (const auto &cv : live) {
		auto &ss = scenesByCanvas[cv.uuid];
		auto &nn = namesByCanvas[cv.uuid];
		for (const auto &sc : cv.scenes) {
			if (sc.uuid.isEmpty())
				continue;
			ss.insert(sc.uuid);
			if (!sc.name.isEmpty())
				nn[sc.name] = sc.uuid;
		}
	}
	for (auto it = roots_.begin(); it != roots_.end();) {
		auto sIt = scenesByCanvas.find(it->first);
		if (sIt == scenesByCanvas.end()) {
			it = roots_.erase(it);
			continue;
		}
		const QSet<QString> &liveScenes = sIt->second;
		QSet<QString> claimed;
		claimLiveRec(it->second, liveScenes, claimed);
		resolveRec(it->second, liveScenes, namesByCanvas[it->first], claimed);
		pruneScenesRec(it->second, liveScenes);
		++it;
	}
}

void TreeStore::clear()
{
	roots_.clear();
	foreign_ = false;
	rawForeign_.clear();
}

QString TreeStore::toJson() const
{
	if (foreign_)
		return rawForeign_;
	QJsonObject o;
	o["version"] = kTreeStoreVersion;
	QJsonObject cs;
	for (const auto &[uuid, root] : roots_) {
		QJsonArray tree;
		for (const auto &c : root.children)
			tree.append(nodeToJson(*c));
		QJsonObject co;
		co["tree"] = tree;
		cs[uuid] = co;
	}
	o["canvases"] = cs;
	return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool TreeStore::fromJson(const QString &json)
{
	clear();
	QJsonParseError err{};
	const auto doc = QJsonDocument::fromJson(json.toUtf8(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return false;
	const QJsonObject o = doc.object();
	if (o["version"].toDouble(1) > kTreeStoreVersion) {
		foreign_ = true;
		rawForeign_ = json;
		return true;
	}
	const QJsonObject cs = o["canvases"].toObject();
	for (auto it = cs.begin(); it != cs.end(); ++it) {
		TreeNode root;
		for (const auto nv : it.value().toObject()["tree"].toArray())
			if (nv.isObject())
				if (auto n = nodeFromJson(nv.toObject()))
					root.children.push_back(std::move(n));
		roots_.emplace(it.key(), std::move(root));
	}
	return true;
}

bool TreeStore::restoreLayout(const QString &json, const std::vector<LiveCanvas> &live)
{
	constexpr int maxBytes = 8 * 1024 * 1024;
	if (foreign_ || live.empty() || json.size() > maxBytes)
		return false;
	const QByteArray bytes = json.toUtf8();
	if (bytes.size() > maxBytes)
		return false;
	QJsonParseError error{};
	const auto document = QJsonDocument::fromJson(bytes, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject())
		return false;
	const auto object = document.object();
	if (!object["version"].isDouble() || object["version"].toDouble() != kTreeStoreVersion ||
	    !object["canvases"].isObject())
		return false;
	QSet<QString> liveIds;
	for (const auto &canvas : live)
		liveIds.insert(canvas.uuid);
	std::vector<std::pair<QJsonArray, int>> pending;
	const auto canvases = object["canvases"].toObject();
	for (auto it = canvases.begin(); it != canvases.end(); ++it) {
		if (!liveIds.contains(it.key()) || !it.value().isObject())
			return false;
		const auto canvas = it.value().toObject();
		if (!canvas["tree"].isArray())
			return false;
		pending.emplace_back(canvas["tree"].toArray(), 1);
	}
	// Validate iteratively before handing bounded input to the recursive loader.
	int nodes = 0;
	while (!pending.empty()) {
		auto [children, depth] = std::move(pending.back());
		pending.pop_back();
		for (const auto value : children) {
			if (depth > 64 || ++nodes > 50000 || !value.isObject())
				return false;
			const auto node = value.toObject();
			for (const char *key : {"color", "name", "alias"})
				if (node.contains(key) && !node[key].isString())
					return false;
			if (node.contains("expanded") && !node["expanded"].isBool())
				return false;
			if (!node["t"].isString())
				return false;
			const QString type = node["t"].toString();
			if (type == QLatin1String("folder")) {
				if (!node["name"].isString() || !node["children"].isArray())
					return false;
				pending.emplace_back(node["children"].toArray(), depth + 1);
			} else if (type == QLatin1String("scene")) {
				if (!node["uuid"].isString() || node["uuid"].toString().isEmpty() || node.contains("children"))
					return false;
			} else {
				return false;
			}
		}
	}
	TreeStore restored;
	if (!restored.fromJson(json) || restored.isForeign())
		return false;
	restored.resolveAndPrune(live);
	restored.placeMissingScenesAtRoot(live);
	*this = std::move(restored);
	return true;
}

bool TreeStore::placeMissingScenesAtRoot(const std::vector<LiveCanvas> &live)
{
	if (foreign_)
		return false;

	bool changed = false;
	std::map<QString, QSet<QString>> placedByCanvas;
	for (const auto &canvas : live) {
		auto [it, inserted] = placedByCanvas.try_emplace(canvas.uuid);
		auto &placed = it->second;
		if (inserted) {
			std::vector<const TreeNode *> pending;
			if (const auto *root = nodeAt(canvas.uuid, {}))
				pending.push_back(root);
			while (!pending.empty()) {
				const TreeNode *node = pending.back();
				pending.pop_back();
				if (node->type == TreeNode::Scene)
					placed.insert(node->uuid);
				else
					for (const auto &child : node->children)
						pending.push_back(child.get());
			}
		}
		TreeNode *root = nullptr;
		for (const auto &scene : canvas.scenes) {
			if (scene.uuid.isEmpty() || placed.contains(scene.uuid))
				continue;
			if (!root)
				root = mutableNodeAt(canvas.uuid, {});
			auto node = std::make_unique<TreeNode>();
			node->type = TreeNode::Scene;
			node->uuid = scene.uuid;
			root->children.push_back(std::move(node));
			placed.insert(scene.uuid);
			changed = true;
		}
	}
	return changed;
}
