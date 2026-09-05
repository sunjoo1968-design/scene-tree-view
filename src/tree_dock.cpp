// Copyright (C) 2026 rockbenben <rockbenben@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tree_dock.h"
#include "obs_bridge.h"
#include <QColor>
#include <QApplication>
#include <QClipboard>
#include <QInputDialog>
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QMessageBox>
#include <QPushButton>
#include <QColorDialog>
#include <QDockWidget>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QShortcut>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <climits>
#include <tuple>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.h>
#include <plugin-support.h>
#include <util/bmem.h>

static const char *kMime = "application/x-scene-anchor";

static QIcon anchorIcon(const char *file)
{
	char *path = obs_module_file(file);
	QIcon icon(QString::fromUtf8(path ? path : ""));
	bfree(path);
	return icon;
}

static QIcon anchorFolderIcon(bool dark)
{
	return anchorIcon(dark ? "icons/folder_dark.svg" : "icons/folder_light.svg");
}
static QIcon anchorSceneIcon(bool dark)
{
	return anchorIcon(dark ? "icons/scene_dark.svg" : "icons/scene_light.svg");
}
static QIcon anchorMinusIcon(bool dark)
{
	return anchorIcon(dark ? "icons/minus_dark.svg" : "icons/minus_light.svg");
}

static QIcon anchorRemoveFolderIcon(bool dark)
{
	QPixmap pixmap = anchorFolderIcon(dark).pixmap(20, 20);
	QPainter painter(&pixmap);
	painter.fillRect(9, 9, 11, 11, QColor(dark ? "#24262b" : "#ffffff"));
	painter.drawPixmap(9, 9, anchorMinusIcon(dark).pixmap(11, 11));
	painter.end();
	return QIcon(pixmap);
}

struct Opt {
	const char *key;
	bool def;
};
static constexpr Opt kOptIcons{"SceneIcons", true};
struct Preset {
	const char *hex;
	const char *key;
};
static const Preset kColors[8] = {
	{"#d13438", "Red"},  {"#ca5010", "Orange"}, {"#c19c00", "Yellow"}, {"#107c10", "Green"},
	{"#038387", "Teal"}, {"#0078d4", "Blue"},   {"#8764b8", "Purple"}, {"#881798", "Magenta"},
};

static bool presetHas(const QString &hex)
{
	for (const Preset &p : kColors)
		if (hex.compare(QString::fromUtf8(p.hex), Qt::CaseInsensitive) == 0)
			return true;
	return false;
}

static constexpr double kMinContrast = 3.0;

static double srgbLin(double c)
{
	return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
static double relLum(const QColor &c)
{
	return 0.2126 * srgbLin(c.redF()) + 0.7152 * srgbLin(c.greenF()) + 0.0722 * srgbLin(c.blueF());
}
static double contrastOf(double a, double b)
{
	return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

static QColor contrastAdjusted(const QColor &c, const QColor &bg)
{
	const double lb = relLum(bg);
	if (contrastOf(relLum(c), lb) >= kMinContrast)
		return c;
	const bool lighten = lb < 0.18;
	float h = 0, s = 0, l = 0, a = 1;
	c.getHslF(&h, &s, &l, &a);
	if (h < 0)
		h = 0;
	double lo = lighten ? l : 0.0, hi = lighten ? 1.0 : l;
	for (int i = 0; i < 24; ++i) {
		const double mid = (lo + hi) / 2;
		const bool ok = contrastOf(relLum(QColor::fromHslF(h, s, float(mid), a)), lb) >= kMinContrast;
		if (lighten)
			(ok ? hi : lo) = mid;
		else
			(ok ? lo : hi) = mid;
	}
	return QColor::fromHslF(h, s, float(lighten ? hi : lo), a);
}

static QPixmap colorSwatch(const QColor &raw, const QColor &bg, const QColor &fg, bool current)
{
	QPixmap px(16, 16);
	px.fill(Qt::transparent);
	QPainter p(&px);
	p.setRenderHint(QPainter::Antialiasing, false);
	p.fillRect(QRect(1, 1, 14, 14), contrastAdjusted(raw, bg));
	if (current) {
		p.setPen(QPen(fg, 2));
		p.drawRect(QRect(1, 1, 13, 13));
	}
	p.end();
	return px;
}

static QIcon tintedIcon(const QIcon &src, const QColor &c)
{
	QPixmap pm = src.pixmap(QSize(16, 16));
	if (pm.isNull())
		return src;
	QPainter p(&pm);
	p.setCompositionMode(QPainter::CompositionMode_SourceIn);
	p.fillRect(pm.rect(), c);
	p.end();
	return QIcon(pm);
}

QStringList AnchorModel::mimeTypes() const
{
	return {QString::fromUtf8(kMime)};
}
Qt::DropActions AnchorModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

QMimeData *AnchorModel::mimeData(const QModelIndexList &indexes) const
{
	QModelIndexList sorted = indexes;
	std::sort(sorted.begin(), sorted.end(), [](const QModelIndex &a, const QModelIndex &b) {
		QModelIndex x = a, y = b;
		std::vector<int> pa, pb;
		for (; x.isValid(); x = x.parent())
			pa.insert(pa.begin(), x.row());
		for (; y.isValid(); y = y.parent())
			pb.insert(pb.begin(), y.row());
		return pa < pb;
	});
	QJsonArray arr;
	for (const QModelIndex &idx : sorted) {
		if (idx.column() != 0)
			continue;
		QStandardItem *it = itemFromIndex(idx);
		const int kind = it->data(RoleKind).toInt();
		QJsonObject o;
		o["canvas"] = it->data(RoleCanvas).toString();
		o["placed"] = it->data(RolePlaced).toBool() || kind == RowPlan::Folder;
		o["uuid"] = it->data(RoleUuid).toString();
		QJsonArray p;
		for (const QVariant &v : it->data(RolePath).toList())
			p.append(v.toInt());
		o["path"] = p;
		arr.append(o);
	}
	auto *mime = new QMimeData();
	mime->setData(QString::fromUtf8(kMime), QJsonDocument(arr).toJson(QJsonDocument::Compact));
	return mime;
}

class AnchorTreeView : public QTreeView {
public:
	using QTreeView::QTreeView;

protected:
	void mousePressEvent(QMouseEvent *event) override
	{
		// Opening the context menu must not select or switch a scene.
		if (event->button() == Qt::RightButton) {
			event->accept();
			return;
		}
		QTreeView::mousePressEvent(event);
	}
	void mouseDoubleClickEvent(QMouseEvent *event) override
	{
		const QModelIndex index = indexAt(event->position().toPoint());
		if (event->button() == Qt::LeftButton && index.isValid() &&
		    index.data(RoleKind).toInt() == RowPlan::Folder)
			setExpanded(index, !isExpanded(index));
		event->accept();
	}

	void drawRow(QPainter *painter, const QStyleOptionViewItem &option,
		     const QModelIndex &index) const override
	{
		QStyleOptionViewItem activeOption(option);
		const bool program = index.data(RoleProgramScene).toBool() ||
			(index.data(RoleContainsProgram).toBool() && !isExpanded(index));
		if (program || index.data(RoleActiveScene).toBool()) {
			// Program takes priority when preview and program refer to the same scene.
			const QColor highlight(program ? "#8e4149" : "#2456c4");
			activeOption.state |= QStyle::State_Selected | QStyle::State_Active;
			activeOption.palette.setColor(QPalette::Highlight, highlight);
			activeOption.palette.setColor(QPalette::HighlightedText, Qt::white);
			painter->fillRect(option.rect, highlight);
		}
		QTreeView::drawRow(painter, activeOption, index);
	}

	void drawBranches(QPainter *p, const QRect &rect, const QModelIndex &index) const override
	{
		const int ind = indentation();
		if (ind <= 0 || rect.width() < ind)
			return;
		const int cells = rect.width() / ind;
		const QColor base = palette().color(QPalette::Text);

		p->save();
		p->setRenderHint(QPainter::Antialiasing, true);

		if (cells > 1) {
			QColor g = base;
			g.setAlpha(34);
			p->setPen(QPen(g, 1.0));
			for (int i = 0; i < cells - 1; ++i) {
				const qreal x = rect.left() + ind * (i + 0.5);
				p->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom() + 1));
			}
		}

		if (index.data(RoleKind).toInt() == RowPlan::Folder) {
			const QRectF cell(rect.right() + 1 - ind, rect.top(), ind, rect.height());
			const bool hasKids = index.data(RoleHasKids).toBool();
			QColor c = base;
			c.setAlpha(hasKids ? 190 : 110);
			const qreal s = ind * 0.20;
			const QPointF ctr = cell.center();
			QPainterPath path;
			if (isExpanded(index)) {
				path.moveTo(ctr.x() - s, ctr.y() - s * 0.5);
				path.lineTo(ctr.x(), ctr.y() + s * 0.5);
				path.lineTo(ctr.x() + s, ctr.y() - s * 0.5);
			} else {
				path.moveTo(ctr.x() - s * 0.5, ctr.y() - s);
				path.lineTo(ctr.x() + s * 0.5, ctr.y());
				path.lineTo(ctr.x() - s * 0.5, ctr.y() + s);
			}
			QPen pen(c, 1.4);
			pen.setCapStyle(Qt::RoundCap);
			pen.setJoinStyle(Qt::RoundJoin);
			p->setPen(pen);
			p->setBrush(Qt::NoBrush);
			p->drawPath(path);
		}
		p->restore();
	}
};

bool AnchorModel::canDropMimeData(const QMimeData *data, Qt::DropAction, int, int, const QModelIndex &parent) const
{
	if (!ObsBridge::get() || ObsBridge::get()->option("LayoutLocked", false))
		return false;
	if (!data->hasFormat(QString::fromUtf8(kMime)))
		return false;
	if (parent.isValid()) {
		QStandardItem *p = itemFromIndex(parent);
		const int k = p->data(RoleKind).toInt();
		if (k == RowPlan::Scene)
			return false;
		const auto arr = QJsonDocument::fromJson(data->data(QString::fromUtf8(kMime))).array();
		for (const auto v : arr)
			if (v.toObject()["canvas"].toString() != p->data(RoleCanvas).toString())
				return false;
	}
	return true;
}

bool AnchorModel::dropMimeData(const QMimeData *data, Qt::DropAction, int row, int, const QModelIndex &parent)
{
	if (!ObsBridge::get() || ObsBridge::get()->option("LayoutLocked", false) ||
	    !canDropMimeData(data, Qt::MoveAction, row, 0, parent))
		return false;
	const auto arr = QJsonDocument::fromJson(data->data(QString::fromUtf8(kMime))).array();
	if (arr.isEmpty())
		return false;
	auto *b = ObsBridge::get();

	QString canvas;
	NodePath destFolder;
	QStandardItem *container = parent.isValid() ? itemFromIndex(parent) : nullptr;
	if (parent.isValid()) {
		canvas = container->data(RoleCanvas).toString();
		if (container->data(RoleKind).toInt() == RowPlan::Folder)
			for (const QVariant &v : container->data(RolePath).toList())
				destFolder.push_back(v.toInt());
	} else {
		canvas = arr.first().toObject()["canvas"].toString();
	}

	auto containerRowCount = [&] {
		return container ? container->rowCount() : rowCount();
	};
	auto containerChild = [&](int i) {
		return container ? container->child(i) : item(i);
	};
	auto isPlacedRow = [](QStandardItem *c) {
		return c->data(RoleKind).toInt() == RowPlan::Folder || c->data(RolePlaced).toBool();
	};
	const int rowCnt = containerRowCount();
	int destIndex;
	if (row >= 0 && row < rowCnt && isPlacedRow(containerChild(row))) {
		const QVariantList pl = containerChild(row)->data(RolePath).toList();
		destIndex = pl.isEmpty() ? 0 : pl.last().toInt();
	} else {
		int lastPlaced = -1;
		for (int i = 0; i < rowCnt; ++i)
			if (isPlacedRow(containerChild(i)))
				lastPlaced = i;
		destIndex = (lastPlaced < 0) ? 0
					     : containerChild(lastPlaced)->data(RolePath).toList().last().toInt() + 1;
	}

	b->applyTreeOp(obs_module_text("SceneAnchor.Undo.Move"), [&]() {
		bool any = false;
		std::vector<NodePath> paths;
		std::vector<QString> unplaced;
		for (const auto v : arr) {
			const QJsonObject o = v.toObject();
			if (o["canvas"].toString() != canvas)
				continue;
			if (o["placed"].toBool()) {
				NodePath p;
				for (const auto pv : o["path"].toArray())
					p.push_back(pv.toInt());
				paths.push_back(std::move(p));
			} else {
				unplaced.push_back(o["uuid"].toString());
			}
		}
		int base = destIndex, moved = 0;
		if (!paths.empty()) {
			int at = destIndex, n = 0;
			if (b->store.moveNodes(canvas, paths, destFolder, destIndex, &at, &n)) {
				any = true;
				base = at;
				moved = n;
			}
		}
		int off = moved;
		for (const QString &u : unplaced)
			any |= b->store.placeScene(canvas, u, destFolder, base + off++);
		return any;
	});
	return false;
}

TreeDock::TreeDock()
{
	auto *lay = new QVBoxLayout(this);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(2);

	search_ = new QLineEdit(this);
	search_->setPlaceholderText(QString::fromUtf8(obs_module_text("SceneAnchor.Search")));
	search_->setClearButtonEnabled(true);
	lay->addWidget(search_);

	connect(search_, &QLineEdit::returnPressed, this, [this] {
		if (search_->text().isEmpty())
			return;
		auto *b = ObsBridge::get();
		for (QModelIndex i = proxy_->index(0, 0, QModelIndex()); i.isValid(); i = view_->indexBelow(i)) {
			QStandardItem *it = itemAtSourceIndex(i);
			if (!it || it->data(RoleKind).toInt() != RowPlan::Scene)
				continue;
			b->switchToScene(it->data(RoleUuid).toString());
			return;
		}
	});
	auto *escClear = new QShortcut(QKeySequence(Qt::Key_Escape), search_);
	escClear->setContext(Qt::WidgetShortcut);
	connect(escClear, &QShortcut::activated, search_, &QLineEdit::clear);
	connect(ObsBridge::get(), &ObsBridge::focusSearchRequested, this, [this] {
		if (auto *dw = qobject_cast<QDockWidget *>(parentWidget())) {
			dw->show();
			dw->raise();
		}
		search_->setFocus(Qt::ShortcutFocusReason);
		search_->selectAll();
	});

	view_ = new AnchorTreeView(this);
	model_ = new AnchorModel(this);
	proxy_ = new QSortFilterProxyModel(this);
	proxy_->setSourceModel(model_);
	proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
	proxy_->setRecursiveFilteringEnabled(true);
	view_->setModel(proxy_);
	view_->setHeaderHidden(true);
	view_->setUniformRowHeights(true);
	view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
	view_->setIndentation(16);
	view_->setIconSize(QSize(16, 16));
	view_->setEditTriggers(QAbstractItemView::EditKeyPressed);
	view_->setContextMenuPolicy(Qt::CustomContextMenu);
	view_->setDragDropMode(QAbstractItemView::InternalMove);
	view_->setDefaultDropAction(Qt::MoveAction);
	view_->setDropIndicatorShown(true);
	lay->addWidget(view_, 1);

	hint_ = new QLabel(QString::fromUtf8(obs_module_text("SceneAnchor.EmptyHint")), this);
	hint_->setWordWrap(true);
	hint_->setAlignment(Qt::AlignCenter);
	hint_->setForegroundRole(QPalette::PlaceholderText);
	hint_->setContentsMargins(10, 6, 10, 6);
	updateHintCap();
	hint_->setVisible(false);
	lay->addWidget(hint_);

	auto *btnRow = new QHBoxLayout();
	btnRow->setContentsMargins(4, 0, 4, 4);
	btnAddFolder_ = new QToolButton(this);
	btnAddFolder_->setToolTip(QString::fromUtf8(obs_module_text("SceneAnchor.AddFolder")));
	btnRemove_ = new QToolButton(this);
	btnRemove_->setToolTip(QString::fromUtf8(obs_module_text("SceneAnchor.Menu.RemoveFolder")));
	btnRow->addWidget(btnAddFolder_);
	btnRow->addWidget(btnRemove_);
	btnUp_ = new QToolButton(this);
	btnUp_->setArrowType(Qt::UpArrow);
	btnUp_->setToolTip(QString::fromUtf8(obs_module_text("SceneAnchor.MoveUp")));
	btnDown_ = new QToolButton(this);
	btnDown_->setArrowType(Qt::DownArrow);
	btnDown_->setToolTip(QString::fromUtf8(obs_module_text("SceneAnchor.MoveDown")));
	btnRow->addWidget(btnUp_);
	btnRow->addWidget(btnDown_);
	connect(btnUp_, &QToolButton::clicked, this, [this] { moveSelection(-1); });
	connect(btnDown_, &QToolButton::clicked, this, [this] { moveSelection(1); });
	btnRow->addStretch();
	btnFindProgram_ = new QToolButton(this);
	btnFindProgram_->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
	btnFindProgram_->setToolTip(QString::fromUtf8(obs_module_text("SceneAnchor.FindProgram")));
	btnRow->addWidget(btnFindProgram_);
	connect(btnFindProgram_, &QToolButton::clicked, this, &TreeDock::findProgramScene);
	btnBackup_ = new QToolButton(this);
	btnBackup_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
	btnBackup_->setToolTip(QString::fromUtf8(obs_module_text("SceneAnchor.BackupLayout")));
	btnRow->addWidget(btnBackup_);
	connect(btnBackup_, &QToolButton::clicked, this, &TreeDock::backupLayout);
	btnRestore_ = new QToolButton(this);
	btnRestore_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
	btnRestore_->setToolTip(QString::fromUtf8(obs_module_text("SceneAnchor.RestoreLayout")));
	btnRow->addWidget(btnRestore_);
	connect(btnRestore_, &QToolButton::clicked, this, &TreeDock::restoreLayout);
	btnReset_ = new QToolButton(this);
	btnReset_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
	btnReset_->setToolTip(QString::fromUtf8(obs_module_text("SceneAnchor.Reset")));
	btnRow->addWidget(btnReset_);
	connect(btnReset_, &QToolButton::clicked, this, [this] {
		auto *bridge = ObsBridge::get();
		if (bridge->option("LayoutLocked", false))
			return;
		const QString snapshot = bridge->store.toJson();
		if (QMessageBox::question(this, QString::fromUtf8(obs_module_text("SceneAnchor.Reset")),
			QString::fromUtf8(obs_module_text("SceneAnchor.ResetConfirm")),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;
		if (snapshot != bridge->store.toJson())
			return;
		const auto live = bridge->liveCanvases();
		bridge->applyTreeOp(obs_module_text("SceneAnchor.Reset"), [&] {
			return bridge->store.resetToLive(live);
		});
		search_->clear();
	});
	btnLayoutLock_ = new QToolButton(this);
	btnLayoutLock_->setCheckable(true);
	btnLayoutLock_->setChecked(ObsBridge::get()->option("LayoutLocked", false));
	btnRow->addWidget(btnLayoutLock_);
	connect(btnLayoutLock_, &QToolButton::toggled, this, [this](bool locked) {
		ObsBridge::get()->setOption("LayoutLocked", locked);
		updateLayoutLock();
	});
	updateLayoutLock();
	lay->addLayout(btnRow);

	auto *b = ObsBridge::get();
	connect(b, &ObsBridge::needsRebuild, this, &TreeDock::rebuild, Qt::QueuedConnection);
	connect(b, &ObsBridge::sceneStateChanged, this, &TreeDock::onSceneStateChanged);
	connect(view_, &QTreeView::customContextMenuRequested, this, &TreeDock::onContextMenu);

	connect(search_, &QLineEdit::textChanged, this, [this](const QString &t) {
		proxy_->setFilterFixedString(t);
		updateLayoutLock();
		if (!t.isEmpty()) {
			view_->expandAll();
			view_->setDragDropMode(QAbstractItemView::NoDragDrop);
		} else {
			updateLayoutLock();
			rebuild();
		}
	});

	connect(model_, &QStandardItemModel::itemChanged, this, [this](QStandardItem *it) {
		if (rebuilding_)
			return;
		auto *bb = ObsBridge::get();
		const int kind = it->data(RoleKind).toInt();
		if (kind == RowPlan::Scene) {
			const QString cv = it->data(RoleCanvas).toString();
			const NodePath path = pathOfItem(it);
			const QString alias = it->text().trimmed();
			bb->applyTreeOp(obs_module_text("SceneAnchor.RenameAlias"), [&] {
				return bb->store.setSceneAlias(cv, path, alias);
			});
			return;
		}
		if (kind == RowPlan::Folder) {
			const QString cv = it->data(RoleCanvas).toString();
			const NodePath p = pathOfItem(it);
			const QString name = it->text().trimmed();
			if (name.isEmpty()) {
				bb->needsRebuild();
				return;
			}
			if (const TreeNode *cur = bb->store.nodeAt(cv, p); cur && cur->name == name)
				return;
			pendingRenameCanvas_ = cv;
			pendingRenamePath_ = p;
			pendingRenameName_ = name;
			hasPendingRename_ = true;
			bb->applyTreeOp(obs_module_text("SceneAnchor.Undo.RenameFolder"),
					[&] { return bb->store.renameFolder(cv, p, name); });
		}
	});

	auto expandWrite = [this](const QModelIndex &pi, bool exp) {
		if (rebuilding_ || !search_->text().isEmpty())
			return;
		QStandardItem *it = itemAtSourceIndex(pi);
		if (!it)
			return;
		const QString cv = it->data(RoleCanvas).toString();
		const int kind = it->data(RoleKind).toInt();
		if (kind != RowPlan::Folder)
			return;
		const NodePath p = pathOfItem(it);
		ObsBridge::get()->silentTreeOp([cv, p, exp] { ObsBridge::get()->store.setExpanded(cv, p, exp); });
	};
	connect(view_, &QTreeView::expanded, this, [expandWrite](const QModelIndex &i) { expandWrite(i, true); });
	connect(view_, &QTreeView::collapsed, this, [expandWrite](const QModelIndex &i) { expandWrite(i, false); });
	connect(view_, &QTreeView::expanded, view_->viewport(), qOverload<>(&QWidget::update));
	connect(view_, &QTreeView::collapsed, view_->viewport(), qOverload<>(&QWidget::update));

	connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
		[this](const QModelIndex &cur, const QModelIndex &) {
			if (rebuilding_ || !cur.isValid())
				return;
			if (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
				return;
			if (view_->selectionModel()->selectedRows().size() > 1)
				return;
			QStandardItem *it = itemAtSourceIndex(cur);
			if (it && it->data(RoleKind).toInt() == RowPlan::Scene)
				ObsBridge::get()->switchToScene(it->data(RoleUuid).toString());
		});

	view_->setExpandsOnDoubleClick(false);

	connect(btnAddFolder_, &QToolButton::clicked, this, [this] {
		auto *bb = ObsBridge::get();
		QString canvas = bb->liveCanvases().empty() ? QString() : bb->liveCanvases().front().uuid;
		NodePath parent;
		int index = INT_MAX;
		if (view_->currentIndex().isValid()) {
			QStandardItem *it = itemAtSourceIndex(view_->currentIndex());
			canvas = it->data(RoleCanvas).toString();
			const NodePath p = pathOfItem(it);
			if (it->data(RoleKind).toInt() == RowPlan::Folder) {
				parent = p;
			} else if (it->data(RolePlaced).toBool() && !p.empty()) {
				parent.assign(p.begin(), p.end() - 1);
				index = p.back() + 1;
			}
		}
		if (canvas.isEmpty())
			return;
		bb->applyTreeOp(obs_module_text("SceneAnchor.Undo.AddFolder"), [&] {
			return bb->store.insertFolder(canvas, parent, index,
				bb->store.nextFolderName(canvas, QString::fromUtf8(obs_module_text("SceneAnchor.NewFolder"))));
		});
	});

	connect(btnRemove_, &QToolButton::clicked, this, [this] { removeFolders(); });
	auto *removeShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), view_);
	removeShortcut->setContext(Qt::WidgetShortcut);
	connect(removeShortcut, &QShortcut::activated, this, [this] { removeFolders(); });

	obs_log(LOG_INFO, "dock constructed");
}

QStandardItem *TreeDock::itemAtSourceIndex(const QModelIndex &proxyIdx) const
{
	return model_->itemFromIndex(proxy_->mapToSource(proxyIdx));
}

NodePath TreeDock::pathOfItem(const QStandardItem *it) const
{
	NodePath p;
	for (const QVariant &v : it->data(RolePath).toList())
		p.push_back(v.toInt());
	return p;
}

QModelIndex TreeDock::findFolderIndex(const QString &canvas, const NodePath &path) const
{
	for (QStandardItem *it : model_->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive))
		if (it->data(RoleKind).toInt() == RowPlan::Folder && it->data(RoleCanvas).toString() == canvas &&
		    pathOfItem(it) == path)
			return proxy_->mapFromSource(model_->indexFromItem(it));
	return {};
}

void TreeDock::rebuild()
{
	if (rebuilding_)
		return;
	rebuilding_ = true;

	const QColor viewBg = view_->palette().color(QPalette::Base);
	const bool darkIcons = viewBg.lightness() < 128;
	const QIcon folderIcon = anchorFolderIcon(darkIcons);
	const QIcon sceneIcon = anchorSceneIcon(darkIcons);
	const bool icons = ObsBridge::get()->option(kOptIcons.key, kOptIcons.def);
	btnAddFolder_->setIcon(folderIcon);
	btnRemove_->setIcon(anchorRemoveFolderIcon(darkIcons));

	QString selUuid, selFolderCanvas, selFolderName;
	NodePath selFolderPath;
	bool selIsFolder = false;
	if (view_->currentIndex().isValid())
		if (QStandardItem *it = itemAtSourceIndex(view_->currentIndex())) {
			selUuid = it->data(RoleUuid).toString();
			if (it->data(RoleKind).toInt() == RowPlan::Folder) {
				selIsFolder = true;
				selFolderCanvas = it->data(RoleCanvas).toString();
				selFolderPath = pathOfItem(it);
				selFolderName = it->text();
			}
		}
	if (selIsFolder && hasPendingRename_ && selFolderCanvas == pendingRenameCanvas_ &&
	    selFolderPath == pendingRenamePath_)
		selFolderName = pendingRenameName_;
	hasPendingRename_ = false;
	const int scroll = view_->verticalScrollBar()->value();
	QSet<QString> selectedScenes;
	std::vector<std::tuple<QString, NodePath, QString>> selectedFolders;
	for (const auto &index : view_->selectionModel()->selectedRows()) {
		if (auto *item = itemAtSourceIndex(index)) {
			if (item->data(RoleKind).toInt() == RowPlan::Scene)
				selectedScenes.insert(item->data(RoleUuid).toString());
			else if (item->data(RoleKind).toInt() == RowPlan::Folder)
				selectedFolders.emplace_back(item->data(RoleCanvas).toString(), pathOfItem(item), item->text());
		}
	}

	auto *b = ObsBridge::get();
	const auto live = b->liveCanvases();
	if (b->store.placeMissingScenesAtRoot(live))
		b->markDirty();
	const auto plan = planProjection(b->store, live);

	model_->removeRows(0, model_->rowCount());
	std::vector<QStandardItem *> parents{model_->invisibleRootItem()};
	std::vector<std::pair<QStandardItem *, bool>> expandStates;
	int folderRows = 0, sceneRows = 0, unfiledRows = 0;

	for (const auto &r : plan) {
		auto *item = new QStandardItem(r.name);
		item->setData(int(r.kind), RoleKind);
		item->setData(r.uuid, RoleUuid);
		QVariantList pl;
		for (int i : r.path)
			pl << i;
		item->setData(pl, RolePath);
		item->setData(r.canvas, RoleCanvas);
		item->setData(r.placed, RolePlaced);
		if (!r.color.isEmpty())
			item->setData(r.color, RoleColor);
		Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
		if (r.kind == RowPlan::Folder)
			f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
		else if (r.kind == RowPlan::Scene)
			f |= Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
		else
			f = Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
		item->setFlags(f);
		const QColor raw(r.color);
		const QColor tag = raw.isValid() ? contrastAdjusted(raw, viewBg) : QColor();
		if (icons) {
			const QIcon &base = r.kind == RowPlan::Folder ? folderIcon : sceneIcon;
			item->setIcon(tag.isValid() ? tintedIcon(base, tag) : base);
		} else if (tag.isValid()) {
			item->setForeground(tag);
		}
		parents[r.depth]->appendRow(item);
		parents.resize(r.depth + 1);
		parents.push_back(item);
		if (r.kind != RowPlan::Scene)
			expandStates.push_back({item, r.expanded});
		if (r.kind == RowPlan::Folder)
			++folderRows;
		else if (r.kind == RowPlan::Scene) {
			++sceneRows;
			if (!r.placed)
				++unfiledRows;
		}
	}

	obs_log(LOG_INFO, "rebuild: %d rows (%d folders, %d scenes, %d unfiled) across %zu canvas(es)",
		(int)plan.size(), folderRows, sceneRows, unfiledRows, live.size());

	for (const auto &[it, exp] : expandStates)
		it->setData(it->rowCount() > 0, RoleHasKids);
	for (const auto &[it, exp] : expandStates)
		view_->setExpanded(proxy_->mapFromSource(model_->indexFromItem(it)), exp);

	if (!search_->text().isEmpty())
		view_->expandAll();

	const QString target = (!selUuid.isEmpty() || selIsFolder) ? selUuid : b->currentSceneUuid();
	if (!target.isEmpty() || selIsFolder) {
		for (QStandardItem *it :
		     model_->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive)) {
			const int kind = it->data(RoleKind).toInt();
			const bool sceneMatch = !target.isEmpty() && kind == RowPlan::Scene &&
						it->data(RoleUuid).toString() == target;
			const bool folderMatch = selIsFolder && kind == RowPlan::Folder &&
						 it->data(RoleCanvas).toString() == selFolderCanvas &&
						 pathOfItem(it) == selFolderPath && it->text() == selFolderName;
			if (sceneMatch || folderMatch) {
				view_->setCurrentIndex(proxy_->mapFromSource(model_->indexFromItem(it)));
				break;
			}
		}
	}
	hintWanted_ = folderRows == 0;
	updateHintCap();
	bool rootFolder = false;
	for (const auto &r : plan)
		if (r.kind == RowPlan::Folder && r.depth == 0) {
			rootFolder = true;
			break;
		}
	view_->setRootIsDecorated(rootFolder);
	view_->verticalScrollBar()->setValue(scroll);
	if (selectedScenes.size() + selectedFolders.size() > 1) {
		view_->selectionModel()->clearSelection();
		for (auto *item : model_->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive)) {
			const bool sceneSelected = item->data(RoleKind).toInt() == RowPlan::Scene &&
				selectedScenes.contains(item->data(RoleUuid).toString());
			const bool folderSelected = item->data(RoleKind).toInt() == RowPlan::Folder &&
				std::find(selectedFolders.begin(), selectedFolders.end(),
					std::make_tuple(item->data(RoleCanvas).toString(), pathOfItem(item), item->text())) != selectedFolders.end();
			if (sceneSelected || folderSelected)
				view_->selectionModel()->select(proxy_->mapFromSource(model_->indexFromItem(item)),
					QItemSelectionModel::Select | QItemSelectionModel::Rows);
		}
	}
	rebuilding_ = false;
	onSceneStateChanged();
}

void TreeDock::onSceneStateChanged()
{
	const bool wasRebuilding = rebuilding_;
	rebuilding_ = true;
	auto *b = ObsBridge::get();
	const QString prog = b->currentSceneUuid();
	const QString prev = b->currentPreviewUuid();
	const QString active = prev.isEmpty() ? prog : prev;
	btnFindProgram_->setEnabled(!prog.isEmpty());
	const auto items = model_->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive);
	for (auto *it : items) {
		if (it->data(RoleKind).toInt() == RowPlan::Folder) {
			it->setData(false, RoleContainsProgram);
			it->setToolTip(QString());
		}
	}
	for (QStandardItem *it : items) {
		if (it->data(RoleKind).toInt() != RowPlan::Scene)
			continue;
		QFont f = it->font();
		const QString u = it->data(RoleUuid).toString();
		f.setBold(u == prog);
		f.setItalic(false);
		it->setData(u == active, RoleActiveScene);
		it->setData(u == prog, RoleProgramScene);
		if (!prog.isEmpty() && u == prog) {
			for (auto *parent = it->parent(); parent; parent = parent->parent()) {
				if (parent->data(RoleKind).toInt() != RowPlan::Folder)
					continue;
				parent->setData(true, RoleContainsProgram);
				parent->setToolTip(QString::fromUtf8(obs_module_text("SceneAnchor.ContainsProgram")).arg(it->text()));
			}
		}
		it->setFont(f);
		if (u == active && activeSceneUuid_ != active &&
		    view_->selectionModel()->selectedRows().size() <= 1) {
			const QModelIndex index = proxy_->mapFromSource(model_->indexFromItem(it));
			if (index.isValid()) {
				const QSignalBlocker blocker(view_->selectionModel());
				view_->selectionModel()->setCurrentIndex(
					index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
			}
		}
	}
	activeSceneUuid_ = active;
	rebuilding_ = wasRebuilding;
	view_->viewport()->update();
}

void TreeDock::moveSelection(int direction)
{
	if (btnLayoutLock_->isChecked() || !search_->text().isEmpty())
		return;
	QString canvas;
	std::vector<NodePath> paths;
	for (const auto &index : view_->selectionModel()->selectedRows()) {
		auto *item = itemAtSourceIndex(index);
		if (!item)
			return;
		const QString cv = item->data(RoleCanvas).toString();
		if (!canvas.isEmpty() && canvas != cv)
			return;
		canvas = cv;
		paths.push_back(pathOfItem(item));
	}
	auto *bridge = ObsBridge::get();
	bool moved = false;
	bridge->applyTreeOp(obs_module_text("SceneAnchor.Undo.Move"), [&] {
		moved = bridge->store.moveSiblingNodes(canvas, paths, direction);
		return moved;
	});
	if (moved) {
		rebuild();
		const QSignalBlocker blocker(view_->selectionModel());
		view_->selectionModel()->clearSelection();
		for (auto path : paths) {
			path.back() += direction;
			for (auto *item : model_->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive))
				if (item->data(RoleCanvas).toString() == canvas && pathOfItem(item) == path) {
					const auto index = proxy_->mapFromSource(model_->indexFromItem(item));
					view_->selectionModel()->setCurrentIndex(index,
						QItemSelectionModel::Select | QItemSelectionModel::Rows);
				}
		}
	}
}

static QString sceneCollectionName()
{
	char *name = obs_frontend_get_current_scene_collection();
	const QString result = QString::fromUtf8(name ? name : "");
	bfree(name);
	return result;
}

void TreeDock::findProgramScene()
{
	const QString program = ObsBridge::get()->currentSceneUuid();
	if (program.isEmpty())
		return;
	// Never change currentIndex: that signal is connected to scene switching.
	const QSignalBlocker blocker(view_->selectionModel());
	search_->clear();
	for (auto *item : model_->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive)) {
		if (item->data(RoleKind).toInt() != RowPlan::Scene || item->data(RoleUuid).toString() != program)
			continue;
		const auto index = proxy_->mapFromSource(model_->indexFromItem(item));
		for (auto parent = index.parent(); parent.isValid(); parent = parent.parent())
			view_->setExpanded(parent, true);
		view_->scrollTo(index, QAbstractItemView::PositionAtCenter);
		return;
	}
}

void TreeDock::backupLayout()
{
	auto *bridge = ObsBridge::get();
	const QString title = QString::fromUtf8(obs_module_text("SceneAnchor.BackupLayout"));
	const QString collection = sceneCollectionName();
	TreeStore copy;
	if (bridge->store.isForeign() || !copy.fromJson(bridge->store.toJson()))
		return;
	const auto live = bridge->liveCanvases();
	copy.placeMissingScenesAtRoot(live);
	std::map<QString, QString> names;
	for (const auto &canvas : live)
		for (const auto &scene : canvas.scenes)
			names[scene.uuid] = scene.name;
	copy.stampSceneNames(names);
	QJsonObject backup;
	backup["format"] = "scene-tree-view-layout";
	backup["version"] = 1;
	backup["collection"] = collection;
	backup["layout"] = QJsonDocument::fromJson(copy.toJson().toUtf8()).object();
	const QByteArray data = QJsonDocument(backup).toJson();
	// Apply the same validation to exports so every written file can be restored.
	TreeStore validated;
	if (data.size() > 8 * 1024 * 1024 || !validated.restoreLayout(copy.toJson(), live)) {
		QMessageBox::warning(this, title, QString::fromUtf8(obs_module_text("SceneAnchor.BackupInvalid")));
		return;
	}
	const QString path = QFileDialog::getSaveFileName(this, title, "scene-tree-layout.json", "JSON (*.json)");
	if (path.isEmpty())
		return;
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
		QMessageBox::warning(this, title, QString::fromUtf8(obs_module_text("SceneAnchor.BackupWriteFailed")));
}

void TreeDock::restoreLayout()
{
	auto *bridge = ObsBridge::get();
	if (bridge->option("LayoutLocked", false))
		return;
	const QString title = QString::fromUtf8(obs_module_text("SceneAnchor.RestoreLayout"));
	const QString collection = sceneCollectionName();
	const QString snapshot = bridge->store.toJson();
	const QString path = QFileDialog::getOpenFileName(this, title, QString(), "JSON (*.json)");
	if (path.isEmpty())
		return;
	auto unchanged = [&] {
		return !bridge->option("LayoutLocked", false) && collection == sceneCollectionName() &&
			snapshot == bridge->store.toJson();
	};
	if (!unchanged()) {
		QMessageBox::warning(this, title, QString::fromUtf8(obs_module_text("SceneAnchor.BackupChanged")));
		return;
	}
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) {
		QMessageBox::warning(this, title, QString::fromUtf8(obs_module_text("SceneAnchor.BackupInvalid")));
		return;
	}
	const auto bytes = file.read(8 * 1024 * 1024 + 1);
	QJsonParseError error;
	const auto document = QJsonDocument::fromJson(bytes, &error);
	const auto backup = document.object();
	if (file.error() != QFileDevice::NoError || bytes.size() > 8 * 1024 * 1024 ||
	    error.error != QJsonParseError::NoError || !document.isObject() ||
	    backup["format"].toString() != "scene-tree-view-layout" || backup["version"].toDouble() != 1 ||
	    !backup["layout"].isObject() || !backup["collection"].isString()) {
		QMessageBox::warning(this, title, QString::fromUtf8(obs_module_text("SceneAnchor.BackupInvalid")));
		return;
	}
	if (backup["collection"].toString() != collection) {
		QMessageBox::warning(this, title, QString::fromUtf8(obs_module_text("SceneAnchor.BackupWrongCollection")));
		return;
	}
	const QString layout = QString::fromUtf8(QJsonDocument(backup["layout"].toObject()).toJson(QJsonDocument::Compact));
	TreeStore candidate;
	if (!candidate.restoreLayout(layout, bridge->liveCanvases())) {
		QMessageBox::warning(this, title, QString::fromUtf8(obs_module_text("SceneAnchor.BackupInvalid")));
		return;
	}
	if (QMessageBox::question(this, title, QString::fromUtf8(obs_module_text("SceneAnchor.RestoreConfirm")),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		return;
	if (!unchanged()) {
		QMessageBox::warning(this, title, QString::fromUtf8(obs_module_text("SceneAnchor.BackupChanged")));
		return;
	}
	bool restored = false;
	bridge->applyTreeOp(obs_module_text("SceneAnchor.RestoreLayout"), [&] {
		restored = bridge->store.restoreLayout(layout, bridge->liveCanvases());
		return restored;
	});
	if (restored)
		search_->clear();
}

void TreeDock::removeFolders(const QModelIndex &contextIndex)
{
	auto *bridge = ObsBridge::get();
	if (bridge->option("LayoutLocked", false))
		return;
	QModelIndexList indexes = view_->selectionModel()->selectedRows();
	if (contextIndex.isValid() && !view_->selectionModel()->isSelected(contextIndex))
		indexes = {contextIndex};
	std::vector<std::pair<QString, NodePath>> targets;
	for (const auto &index : indexes) {
		const auto *item = itemAtSourceIndex(index);
		if (item && item->data(RoleKind).toInt() == RowPlan::Folder)
			targets.emplace_back(item->data(RoleCanvas).toString(), pathOfItem(item));
	}
	std::sort(targets.begin(), targets.end());
	// An ancestor already includes its descendants. Reverse deletion keeps sibling paths valid.
	std::vector<std::pair<QString, NodePath>> roots;
	for (const auto &target : targets) {
		if (!roots.empty() && roots.back().first == target.first &&
		    roots.back().second.size() <= target.second.size() &&
		    std::equal(roots.back().second.begin(), roots.back().second.end(), target.second.begin()))
			continue;
		roots.push_back(target);
	}
	if (roots.empty())
		return;
	const QString snapshot = bridge->store.toJson();
	QMessageBox dialog(QMessageBox::Question,
		QString::fromUtf8(obs_module_text("SceneAnchor.Menu.RemoveFolder")),
		QString::fromUtf8(obs_module_text("SceneAnchor.RemoveFoldersConfirm")).arg(roots.size()),
		QMessageBox::NoButton, this);
	auto *dissolve = dialog.addButton(QString::fromUtf8(obs_module_text("SceneAnchor.KeepContents")),
		QMessageBox::ActionRole);
	auto *remove = dialog.addButton(QString::fromUtf8(obs_module_text("SceneAnchor.RemoveContents")),
		QMessageBox::DestructiveRole);
	auto *cancel = dialog.addButton(QMessageBox::Cancel);
	dialog.setDefaultButton(cancel);
	dialog.setEscapeButton(cancel);
	dialog.exec();
	if ((dialog.clickedButton() != dissolve && dialog.clickedButton() != remove) ||
	    bridge->option("LayoutLocked", false) || snapshot != bridge->store.toJson())
		return;
	const bool keep = dialog.clickedButton() == dissolve;
	bridge->applyTreeOp(obs_module_text(keep ? "SceneAnchor.Undo.Dissolve" : "SceneAnchor.Undo.RemoveFolder"), [&] {
		bool changed = false;
		for (auto it = roots.rbegin(); it != roots.rend(); ++it)
			changed |= keep ? bridge->store.dissolveFolder(it->first, it->second)
					: bridge->store.removeNode(it->first, it->second);
		return changed;
	});
}

void TreeDock::updateLayoutLock()
{
	const bool locked = btnLayoutLock_->isChecked();
	btnLayoutLock_->setText(QString::fromUtf8(locked ? obs_module_text("SceneAnchor.LayoutLocked")
						      : obs_module_text("SceneAnchor.LayoutUnlocked")));
	btnLayoutLock_->setToolTip(btnLayoutLock_->text());
	btnAddFolder_->setEnabled(!locked);
	btnRemove_->setEnabled(!locked);
	btnUp_->setEnabled(!locked && search_->text().isEmpty());
	btnDown_->setEnabled(!locked && search_->text().isEmpty());
	btnReset_->setEnabled(!locked);
	btnRestore_->setEnabled(!locked);
	view_->setEditTriggers(locked ? QAbstractItemView::NoEditTriggers : QAbstractItemView::EditKeyPressed);
	view_->setDragDropMode(locked || !search_->text().isEmpty() ? QAbstractItemView::NoDragDrop
								 : QAbstractItemView::InternalMove);
}

void TreeDock::updateHintCap()
{
	const int cap = qMax(48, height() / 3);
	hint_->setMaximumHeight(cap);
	const bool fits = hint_->heightForWidth(qMax(1, width() - 20)) <= cap;
	hint_->setVisible(hintWanted_ && fits);
}

void TreeDock::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	updateHintCap();
}

void TreeDock::onContextMenu(const QPoint &pos)
{
	if (btnLayoutLock_->isChecked())
		return;
	auto *b = ObsBridge::get();
	QMenu menu(this);
	connect(b, &ObsBridge::needsRebuild, &menu, &QMenu::close);
	const QModelIndex pi = view_->indexAt(pos);
	QStandardItem *it = pi.isValid() ? itemAtSourceIndex(pi) : nullptr;
	const int kind = it ? it->data(RoleKind).toInt() : -1;

	auto addColorMenu = [&](QStandardItem *target) {
		QMenu *cm = menu.addMenu(QString::fromUtf8(obs_module_text("SceneAnchor.Menu.Color")));
		std::vector<std::pair<QString, NodePath>> targets;
		const auto targetIndex = proxy_->mapFromSource(model_->indexFromItem(target));
		const auto indexes = view_->selectionModel()->isSelected(targetIndex)
			? view_->selectionModel()->selectedRows() : QModelIndexList{targetIndex};
		for (const auto &index : indexes) {
			const auto *item = itemAtSourceIndex(index);
			if (item && item->data(RoleKind).toInt() == target->data(RoleKind).toInt())
				targets.emplace_back(item->data(RoleCanvas).toString(), pathOfItem(item));
		}
		const QString snapshot = b->store.toJson();
		auto apply = [b, targets, snapshot](const QString &color) {
			if (b->store.toJson() != snapshot)
				return;
			b->applyTreeOp(obs_module_text("SceneAnchor.Undo.Color"), [&] {
				bool changed = false;
				for (const auto &[canvas, path] : targets)
					changed |= b->store.setColor(canvas, path, color);
				return changed;
			});
		};
		const QColor menuBg = view_->palette().color(QPalette::Base);
		const QColor menuFg = view_->palette().color(QPalette::Text);
		const QString cur = target->data(RoleColor).toString();
		for (const Preset &pc : kColors) {
			QAction *a = cm->addAction(
				QString::fromUtf8(obs_module_text(QByteArray("SceneAnchor.Color.") + pc.key)));
			const bool isCur = cur.compare(QString::fromUtf8(pc.hex), Qt::CaseInsensitive) == 0;
			a->setIcon(QIcon(colorSwatch(QColor(QString::fromUtf8(pc.hex)), menuBg, menuFg, isCur)));
			const QByteArray hex(pc.hex);
			connect(a, &QAction::triggered, this, [apply, hex] { apply(QString::fromUtf8(hex)); });
		}
		cm->addSeparator();
		QAction *custom = cm->addAction(QString::fromUtf8(obs_module_text("SceneAnchor.Menu.CustomColor")));
		if (QColor(cur).isValid() && !presetHas(cur))
			custom->setIcon(QIcon(colorSwatch(QColor(cur), menuBg, menuFg, true)));
		connect(custom, &QAction::triggered, this, [this, apply, cur] {
			const QColor init = QColor(cur).isValid() ? QColor(cur) : QColor(Qt::white);
			const QColor picked = QColorDialog::getColor(
				init, this, QString::fromUtf8(obs_module_text("SceneAnchor.Menu.CustomColor")));
			if (picked.isValid())
				apply(picked.name(QColor::HexRgb));
		});
		QAction *clear = cm->addAction(QString::fromUtf8(obs_module_text("SceneAnchor.Menu.ClearColor")));
		connect(clear, &QAction::triggered, this, [apply] { apply(QString()); });
	};

	if (kind == RowPlan::Scene) {
		const QString uuid = it->data(RoleUuid).toString();
		const QString cv = it->data(RoleCanvas).toString();
		const QString label = it->text();
		QAction *copy = menu.addAction(QString::fromUtf8(obs_module_text("SceneAnchor.CopyName")));
		connect(copy, &QAction::triggered, this, [label] { QApplication::clipboard()->setText(label); });
		QAction *rename = menu.addAction(QString::fromUtf8(obs_module_text("SceneAnchor.RenameAlias")));
		connect(rename, &QAction::triggered, this, [this, b, uuid, cv, label] {
			const QString before = b->store.toJson();
			bool ok = false;
			const QString alias = QInputDialog::getText(this,
				QString::fromUtf8(obs_module_text("SceneAnchor.RenameAlias")),
				QString::fromUtf8(obs_module_text("SceneAnchor.AliasPrompt")),
				QLineEdit::Normal, label, &ok);
			if (!ok || before != b->store.toJson())
				return;
			const auto path = b->store.findScene(cv, uuid);
			if (path)
				b->applyTreeOp(obs_module_text("SceneAnchor.RenameAlias"), [&] {
					return b->store.setSceneAlias(cv, *path, alias);
				});
		});
		addColorMenu(it);
	} else if (kind == RowPlan::Folder) {
		const QString cv = it->data(RoleCanvas).toString();
		const NodePath p = pathOfItem(it);
		QAction *addSub = menu.addAction(QString::fromUtf8(obs_module_text("SceneAnchor.Menu.AddSubfolder")));
		connect(addSub, &QAction::triggered, this, [b, cv, p] {
			b->applyTreeOp(obs_module_text("SceneAnchor.Undo.AddFolder"), [&] {
				return b->store.insertFolder(
					cv, p, INT_MAX, b->store.nextFolderName(cv, QString::fromUtf8(obs_module_text("SceneAnchor.NewFolder"))));
			});
		});
		// Re-locate the folder at trigger time; the model may rebuild while the menu is open.
		QAction *ren = menu.addAction(QString::fromUtf8(obs_module_text("SceneAnchor.Menu.Rename")));
		connect(ren, &QAction::triggered, this, [this, cv, p] {
			const QModelIndex idx = findFolderIndex(cv, p);
			if (idx.isValid())
				view_->edit(idx);
		});
		menu.setToolTipsVisible(true);
		QAction *rm = menu.addAction(QString::fromUtf8(obs_module_text("SceneAnchor.Menu.RemoveFolder")));
		rm->setToolTip(QString::fromUtf8(obs_module_text("SceneAnchor.Menu.RemoveFolderTip")));
		connect(rm, &QAction::triggered, this, [this, cv, p] {
			const auto index = findFolderIndex(cv, p);
			if (index.isValid())
				removeFolders(index);
		});
		menu.addSeparator();
		addColorMenu(it);
	} else {
		QAction *addF = menu.addAction(QString::fromUtf8(obs_module_text("SceneAnchor.AddFolder")));
		connect(addF, &QAction::triggered, btnAddFolder_, &QToolButton::click);
		menu.addSeparator();


		QMenu *disp = menu.addMenu(QString::fromUtf8(obs_module_text("SceneAnchor.Menu.Display")));
		QAction *ico = disp->addAction(QString::fromUtf8(obs_module_text("SceneAnchor.Opt.SceneIcons")));
		ico->setCheckable(true);
		ico->setChecked(b->option(kOptIcons.key, kOptIcons.def));
		connect(ico, &QAction::triggered, this, [this, b](bool on) {
			b->setOption(kOptIcons.key, on);
			rebuild();
		});
	}
	menu.exec(view_->viewport()->mapToGlobal(pos));
}
