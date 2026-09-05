// Copyright (C) 2026 rockbenben <rockbenben@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "projection.h"
#include <QStandardItemModel>
#include <QTreeView>
#include <QWidget>

class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QHBoxLayout;
class QToolButton;
class QMimeData;

enum AnchorRoles {
	RoleKind = Qt::UserRole + 1, // int(RowPlan::Kind)
	RoleUuid,                    // QString
	RolePath,                    // QVariantList<int>
	RoleCanvas,                  // QString
	RolePlaced,                  // bool
	RoleColor,
	RoleHasKids,
	RoleActiveScene,
	RoleProgramScene,
};

class AnchorModel : public QStandardItemModel {
	Q_OBJECT
public:
	using QStandardItemModel::QStandardItemModel;
	QStringList mimeTypes() const override;
	QMimeData *mimeData(const QModelIndexList &indexes) const override;
	bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
			     const QModelIndex &parent) const override;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
			  const QModelIndex &parent) override;
	Qt::DropActions supportedDropActions() const override;
};

class TreeDock : public QWidget {
	Q_OBJECT
public:
	TreeDock();
public slots:
	void rebuild();
private slots:
	void onSceneStateChanged();
	void onContextMenu(const QPoint &pos);

private:
	QStandardItem *itemAtSourceIndex(const QModelIndex &proxyIdx) const; // proxy → source item
	NodePath pathOfItem(const QStandardItem *it) const;
	void resizeEvent(QResizeEvent *e) override;
	void updateHintCap();
	void updateLayoutLock();
	bool hintWanted_ = false;
	QModelIndex findFolderIndex(const QString &canvas, const NodePath &path) const;
	QLineEdit *search_ = nullptr;
	QLabel *hint_ = nullptr;
	QTreeView *view_ = nullptr;
	AnchorModel *model_ = nullptr;
	QSortFilterProxyModel *proxy_ = nullptr;
	QToolButton *btnAddFolder_ = nullptr, *btnRemove_ = nullptr;
	QToolButton *btnLayoutLock_ = nullptr;
	bool rebuilding_ = false;
	QString activeSceneUuid_;
	QString pendingRenameCanvas_;
	NodePath pendingRenamePath_;
	QString pendingRenameName_;
	bool hasPendingRename_ = false;
};
