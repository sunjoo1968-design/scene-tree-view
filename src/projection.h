// Copyright (C) 2026 rockbenben <rockbenben@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "tree_store.h"

struct RowPlan {
	enum Kind { Folder, Scene };
	Kind kind;
	int depth;
	QString name;
	QString uuid;
	NodePath path;
	QString canvas;
	QString color;
	bool expanded;
	bool placed;
};

std::vector<RowPlan> planProjection(const TreeStore &store, const std::vector<LiveCanvas> &live);
