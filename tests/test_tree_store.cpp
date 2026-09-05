// Copyright (C) 2026 rockbenben <rockbenben@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../src/tree_store.h"
#include "../src/projection.h"
#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #x); } } while (0)

static const TreeNode &N(const TreeNode *p)
{
	static const TreeNode kEmpty{};
	return p ? *p : kEmpty;
}

static const RowPlan &R(const std::vector<RowPlan> &v, size_t i)
{
	static const RowPlan kEmpty{};
	return i < v.size() ? v[i] : kEmpty;
}

static const std::vector<std::unique_ptr<TreeNode>> &C(const std::vector<std::unique_ptr<TreeNode>> *p)
{
	static const std::vector<std::unique_ptr<TreeNode>> kEmpty{};
	return p ? *p : kEmpty;
}

static void test_roundtrip()
{
	TreeStore s;
	CHECK(s.insertFolder("cv1", {}, 0, "Opening"));
	CHECK(s.placeScene("cv1", "uuid-a", {0}, 0));
	CHECK(s.placeScene("cv1", "uuid-b", {}, 1));
	CHECK(s.setColor("cv1", {0}, "#d13438"));
	CHECK(s.setExpanded("cv1", {0}, false));

	const QString json = s.toJson();
	TreeStore t;
	CHECK(t.fromJson(json));
	CHECK(t.toJson() == json);
	const TreeNode *f = t.nodeAt("cv1", {0});
	CHECK(f && f->type == TreeNode::Folder && f->name == QStringLiteral("Opening"));
	CHECK(N(f).color == QStringLiteral("#d13438") && N(f).expanded == false);
	const TreeNode *sc = t.nodeAt("cv1", {0, 0});
	CHECK(N(sc).type == TreeNode::Scene && N(sc).uuid == QStringLiteral("uuid-a"));
}

static void test_bad_json()
{
	TreeStore s;
	CHECK(!s.fromJson("not json"));
	CHECK(s.canvasRoot("cv1") == nullptr);
	CHECK(!s.fromJson("[1,2,3]"));
	CHECK(s.fromJson("{}"));
}

static void test_foreign_version()
{
	const QString v9 = QStringLiteral("{\"version\":9,\"future\":true}");
	TreeStore s;
	CHECK(s.fromJson(v9));
	CHECK(s.isForeign());
	CHECK(s.toJson() == v9);
	CHECK(!s.insertFolder("cv1", {}, 0, "x"));
	CHECK(!s.placeScene("cv1", "u", {}, 0));
	s.resolveAndPrune({});
	CHECK(s.toJson() == v9);
}

static void test_failed_op_leaves_store_untouched()
{
	TreeStore s;
	const QString before = s.toJson();
	CHECK(!s.insertFolder("cv1", NodePath{99}, 0, "x"));
	CHECK(!s.setColor("cv2", NodePath{5}, "#fff"));
	CHECK(!s.setExpanded("cv3", NodePath{0}, true));
	CHECK(!s.placeScene("cv4", "u", NodePath{7}, 0));
	CHECK(s.canvasRoot("cv1") == nullptr);
	CHECK(s.canvasRoot("cv2") == nullptr);
	CHECK(s.canvasRoot("cv3") == nullptr);
	CHECK(s.canvasRoot("cv4") == nullptr);
	CHECK(s.toJson() == before);

	CHECK(s.insertFolder("cv1", {}, 0, "A"));
	CHECK(s.canvasRoot("cv1") != nullptr);
	CHECK(s.toJson() != before);
}

static TreeStore makeFixture()
{
	// cv1: [F"A"[S(a), F"B"[S(b)]], S(c)]
	TreeStore s;
	s.insertFolder("cv1", {}, 0, "A");
	s.placeScene("cv1", "a", {0}, 0);
	s.insertFolder("cv1", {0}, 1, "B");
	s.placeScene("cv1", "b", {0, 1}, 0);
	s.placeScene("cv1", "c", {}, 1);
	return s;
}

static void test_mutators()
{
	{
		TreeStore s = makeFixture();
		CHECK(s.renameFolder("cv1", {0}, "A2"));
		CHECK(N(s.nodeAt("cv1", {0})).name == QStringLiteral("A2"));
		CHECK(!s.renameFolder("cv1", {0, 0}, "x"));
		CHECK(!s.renameFolder("cv1", {9}, "x"));
	}
	{
		TreeStore s = makeFixture();
		CHECK(s.dissolveFolder("cv1", {0, 1}));
		CHECK(N(s.nodeAt("cv1", {0, 1})).uuid == QStringLiteral("b"));
		CHECK(s.dissolveFolder("cv1", {0}));
		CHECK(N(s.nodeAt("cv1", {0})).uuid == QStringLiteral("a"));
		CHECK(N(s.nodeAt("cv1", {1})).uuid == QStringLiteral("b"));
		CHECK(N(s.nodeAt("cv1", {2})).uuid == QStringLiteral("c"));
	}
	{
		TreeStore s = makeFixture();
		CHECK(s.removeNode("cv1", {0}));
		CHECK(!s.findScene("cv1", "a"));
		CHECK(!s.findScene("cv1", "b"));
		CHECK(s.findScene("cv1", "c").value() == NodePath{0});
		CHECK(!s.removeNode("cv1", {}));
	}
}

static void test_move()
{
	{
		TreeStore s = makeFixture();
		CHECK(s.moveNodes("cv1", {{0, 0}, {1}}, {}, 0));
		CHECK(N(s.nodeAt("cv1", {0})).uuid == QStringLiteral("a"));
		CHECK(N(s.nodeAt("cv1", {1})).uuid == QStringLiteral("c"));
		CHECK(N(s.nodeAt("cv1", {2})).name == QStringLiteral("A"));
	}
	{
		TreeStore s = makeFixture();
		CHECK(s.moveNodes("cv1", {{0}, {0, 1}}, {}, 2));
		CHECK(N(s.nodeAt("cv1", {0})).uuid == QStringLiteral("c"));
		CHECK(N(s.nodeAt("cv1", {1})).name == QStringLiteral("A"));
		CHECK(N(s.nodeAt("cv1", {1, 1})).name == QStringLiteral("B"));
	}
	{
		TreeStore s = makeFixture();
		CHECK(!s.moveNodes("cv1", {{0}}, {0, 1}, 0));
	}
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "w", {}, 0));
		CHECK(s.placeScene("cv1", "x", {}, 1));
		CHECK(s.placeScene("cv1", "y", {}, 2));
		CHECK(s.moveNodes("cv1", {{0}, {1}}, {}, 2));
		CHECK(N(s.nodeAt("cv1", {0})).uuid == QStringLiteral("w"));
		CHECK(N(s.nodeAt("cv1", {1})).uuid == QStringLiteral("x"));
		CHECK(N(s.nodeAt("cv1", {2})).uuid == QStringLiteral("y"));
	}
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "w", {}, 0));
		CHECK(s.placeScene("cv1", "x", {}, 1));
		CHECK(s.insertFolder("cv1", {}, 2, "D"));
		CHECK(s.placeScene("cv1", "y", {}, 3));
		CHECK(s.placeScene("cv1", "z", {}, 4));
		CHECK(s.moveNodes("cv1", {{0}, {1}, {3}}, {}, 4));
		CHECK(N(s.nodeAt("cv1", {0})).name == QStringLiteral("D"));
		CHECK(N(s.nodeAt("cv1", {1})).uuid == QStringLiteral("w"));
		CHECK(N(s.nodeAt("cv1", {2})).uuid == QStringLiteral("x"));
		CHECK(N(s.nodeAt("cv1", {3})).uuid == QStringLiteral("y"));
		CHECK(N(s.nodeAt("cv1", {4})).uuid == QStringLiteral("z"));
	}
	{
		TreeStore s = makeFixture();
		CHECK(s.moveNodes("cv1", {{1}}, {0, 1}, 0));
		CHECK(N(s.nodeAt("cv1", {0, 1, 0})).uuid == QStringLiteral("c"));
	}
	{
		TreeStore s = makeFixture();
		s.setColor("cv1", {0, 0}, "#107c10");
		CHECK(s.placeScene("cv1", "a", {}, 2));
		CHECK(N(s.nodeAt("cv1", {2})).uuid == QStringLiteral("a"));
		CHECK(N(s.nodeAt("cv1", {2})).color == QStringLiteral("#107c10"));
		CHECK(s.findScene("cv1", "a") == std::optional<NodePath>{NodePath{2}});
		CHECK(N(s.nodeAt("cv1", {0})).children.size() == 1);
	}
	{
		TreeStore s;
		CHECK(s.insertFolder("cv1", {}, 0, "F"));
		CHECK(s.placeScene("cv1", "a", {0}, 0));
		CHECK(s.placeScene("cv1", "b", {0}, 1));
		int at = -1, n = -1;
		CHECK(s.moveNodes("cv1", {{0, 0}}, {0}, 1, &at, &n));
		CHECK(at == 0);
		CHECK(n == 1);
	}
	{
		TreeStore s;
		CHECK(s.insertFolder("cv1", {}, 0, "outer"));
		CHECK(s.insertFolder("cv1", {0}, 0, "inner"));
		CHECK(s.placeScene("cv1", "x", {0, 0}, 0));
		CHECK(s.insertFolder("cv1", {}, 1, "dest"));
		int at = -1, n = -1;
		CHECK(s.moveNodes("cv1", {{0}, {0, 0}}, {1}, 0, &at, &n));
		CHECK(n == 1);
	}
}

static void test_prune()
{
	TreeStore s = makeFixture();
	s.insertFolder("cv-gone", {}, 0, "X");
	s.resolveAndPrune({{QStringLiteral("cv1"),
			    QStringLiteral("Main"),
			    {{QStringLiteral("a"), QString()}, {QStringLiteral("c"), QString()}}}});
	CHECK(!s.findScene("cv1", "b"));
	CHECK(s.findScene("cv1", "a"));
	CHECK(N(s.nodeAt("cv1", {0, 1})).name == QStringLiteral("B"));
	CHECK(s.canvasRoot("cv-gone") == nullptr);
}

static void test_edge_cases()
{
	{
		TreeStore s = makeFixture();
		CHECK(!s.renameFolder("cv1", {}, "x"));
		CHECK(!s.dissolveFolder("cv1", {}));
	}
	{
		TreeStore s;
		CHECK(s.insertFolder("cv1", {}, 0, "empty"));
		const QString before = s.toJson();
		CHECK(s.dissolveFolder("cv1", {0}));
		CHECK(C(s.canvasRoot("cv1")).empty());
		CHECK(s.toJson() != before);
	}
	{
		// cv1: [S(x), F"M"[S(p), S(q), S(r)], S(y)]
		TreeStore s;
		CHECK(s.placeScene("cv1", "x", {}, 0));
		CHECK(s.insertFolder("cv1", {}, 1, "M"));
		CHECK(s.placeScene("cv1", "p", {1}, 0));
		CHECK(s.placeScene("cv1", "q", {1}, 1));
		CHECK(s.placeScene("cv1", "r", {1}, 2));
		CHECK(s.placeScene("cv1", "y", {}, 2));
		CHECK(s.dissolveFolder("cv1", {1}));
		CHECK(N(s.nodeAt("cv1", {0})).uuid == QStringLiteral("x"));
		CHECK(N(s.nodeAt("cv1", {1})).uuid == QStringLiteral("p"));
		CHECK(N(s.nodeAt("cv1", {2})).uuid == QStringLiteral("q"));
		CHECK(N(s.nodeAt("cv1", {3})).uuid == QStringLiteral("r"));
		CHECK(N(s.nodeAt("cv1", {4})).uuid == QStringLiteral("y"));
	}
	{
		TreeStore s;
		CHECK(s.insertFolder("cv1", {}, 0, "F"));
		CHECK(s.placeScene("cv1", "a", {0}, 0));
		CHECK(s.placeScene("cv1", "b", {0}, 1));
		CHECK(s.placeScene("cv1", "c", {0}, 2));
		CHECK(s.removeNode("cv1", {0, 1}));
		CHECK(N(s.nodeAt("cv1", {0, 0})).uuid == QStringLiteral("a"));
		CHECK(N(s.nodeAt("cv1", {0, 1})).uuid == QStringLiteral("c"));
		CHECK(N(s.nodeAt("cv1", {0})).children.size() == 2);
	}
	{
		TreeStore s;
		CHECK(s.insertFolder("cv1", {}, 0, "F"));
		CHECK(s.placeScene("cv1", "only", {0}, 0));
		CHECK(s.removeNode("cv1", {0, 0}));
		CHECK(N(s.nodeAt("cv1", {0})).type == TreeNode::Folder);
		CHECK(N(s.nodeAt("cv1", {0})).children.empty());
	}
	{
		TreeStore s;
		const QString before = s.toJson();
		CHECK(!s.renameFolder("nope", {0}, "x"));
		CHECK(!s.dissolveFolder("nope", {0}));
		CHECK(!s.removeNode("nope", {0}));
		CHECK(!s.moveNodes("nope", {{0}}, {}, 0));
		CHECK(s.canvasRoot("nope") == nullptr);
		CHECK(s.toJson() == before);
	}
	{
		TreeStore s = makeFixture();
		const QString before = s.toJson();
		CHECK(!s.removeNode("cv1", {2}));  // idx == size()
		CHECK(!s.removeNode("cv1", {99}));
		CHECK(s.toJson() == before);
	}
}

static void test_place_missing_preserves_tree()
{
	TreeStore s = makeFixture();
	CHECK(s.setColor("cv1", {0}, "#123456"));
	CHECK(s.setExpanded("cv1", {0}, false));
	CHECK(s.setColor("cv1", {0, 1, 0}, "#abcdef"));
	s.stampSceneNames({{"b", "Saved name"}});
	const TreeNode *nested = s.nodeAt("cv1", {0, 1, 0});
	const QString before = s.toJson();
	const std::vector<LiveCanvas> existing{{"cv1", "Main", {{"b", "Changed"}, {"a", "A"}}}};
	CHECK(!s.placeMissingScenesAtRoot(existing));
	CHECK(s.toJson() == before);
	CHECK(s.placeMissingScenesAtRoot({{"cv1", "Main", {{"b", "Changed"}, {"d", "D"}, {"e", "E"}}}}));
	CHECK(s.nodeAt("cv1", {0, 1, 0}) == nested);
	CHECK(N(nested).name == QStringLiteral("Saved name"));
	CHECK(N(nested).color == QStringLiteral("#abcdef"));
	CHECK(N(s.nodeAt("cv1", {0})).color == QStringLiteral("#123456"));
	CHECK(!N(s.nodeAt("cv1", {0})).expanded);
	CHECK(N(s.nodeAt("cv1", {1})).uuid == QStringLiteral("c"));
	CHECK(N(s.nodeAt("cv1", {2})).uuid == QStringLiteral("d"));
	CHECK(N(s.nodeAt("cv1", {3})).uuid == QStringLiteral("e"));
	CHECK(s.removeNode("cv1", {3}));
	CHECK(s.removeNode("cv1", {2}));
	CHECK(s.toJson() == before);
}

static void test_place_missing_duplicates_and_empty()
{
	TreeStore s;
	const QString before = s.toJson();
	CHECK(!s.placeMissingScenesAtRoot({}));
	CHECK(!s.placeMissingScenesAtRoot({{"empty", "", {}}, {"invalid", "", {{"", "Name"}}}}));
	CHECK(s.toJson() == before);
	const std::vector<LiveCanvas> live{
		{"cv1", "", {{"a", "A"}, {"a", "Again"}, {"", "Invalid"}, {"b", "B"}}},
		{"cv2", "", {{"a", "Other canvas"}}},
		{"cv1", "", {{"b", "Again"}, {"c", "C"}, {"a", "Again"}}}};
	CHECK(s.placeMissingScenesAtRoot(live));
	CHECK(C(s.canvasRoot("cv1")).size() == 3);
	CHECK(C(s.canvasRoot("cv2")).size() == 1);
	CHECK(N(s.nodeAt("cv1", {0})).uuid == QStringLiteral("a"));
	CHECK(N(s.nodeAt("cv1", {1})).uuid == QStringLiteral("b"));
	CHECK(N(s.nodeAt("cv1", {2})).uuid == QStringLiteral("c"));
	CHECK(N(s.nodeAt("cv2", {0})).uuid == QStringLiteral("a"));
	const QString placed = s.toJson();
	CHECK(!s.placeMissingScenesAtRoot(live));
	CHECK(s.toJson() == placed);
}

static void test_place_missing_foreign()
{
	for (const QString &version : {QStringLiteral("9"), QStringLiteral("2147483648")}) {
		const QString json = QStringLiteral("{ \"version\":%1, \"future\":{\"keep\":true} }").arg(version);
		TreeStore s;
		CHECK(s.fromJson(json));
		CHECK(s.isForeign());
		CHECK(!s.placeMissingScenesAtRoot({{"cv1", "", {{"a", "A"}}}}));
		CHECK(s.canvasRoot("cv1") == nullptr);
		CHECK(s.toJson() == json);
	}
}

static void test_place_missing_large_batch()
{
	TreeStore s;
	LiveCanvas canvas{"cv1", "", {}};
	for (int i = 0; i < 10000; ++i)
		canvas.scenes.push_back({QString::number(i), {}});
	CHECK(s.placeMissingScenesAtRoot({canvas}));
	CHECK(C(s.canvasRoot("cv1")).size() == canvas.scenes.size());
	for (int i = 0; i < 10000; ++i)
		CHECK(N(s.nodeAt("cv1", {i})).uuid == QString::number(i));
	CHECK(!s.placeMissingScenesAtRoot({canvas}));
}

static void test_projection()
{
	{
		TreeStore s;
		std::vector<LiveCanvas> live{{QStringLiteral("cv1"),
					      QStringLiteral("Main"),
					      {{QStringLiteral("a"), QStringLiteral("Scene A")},
					       {QStringLiteral("b"), QStringLiteral("Scene B")},
					       {QStringLiteral("c"), QStringLiteral("Scene C")}}}};
		CHECK(s.placeMissingScenesAtRoot(live));
		CHECK(!s.placeMissingScenesAtRoot(live));
		CHECK(s.moveNodes("cv1", {{2}}, {}, 0));
		auto plan = planProjection(s, live);
		CHECK(R(plan, 0).uuid == QStringLiteral("c") && R(plan, 0).placed);
		CHECK(R(plan, 1).uuid == QStringLiteral("a") && R(plan, 1).placed);
		CHECK(R(plan, 2).uuid == QStringLiteral("b") && R(plan, 2).placed);
		CHECK(live[0].scenes[0].uuid == QStringLiteral("a"));
		CHECK(live[0].scenes[1].uuid == QStringLiteral("b"));
		CHECK(live[0].scenes[2].uuid == QStringLiteral("c"));
	}
	{
		TreeStore s = makeFixture();
		s.placeScene("cv1", "zombie", {0}, 0);
		std::vector<LiveCanvas> live{{QStringLiteral("cv1"),
					      QStringLiteral("Main canvas"),
					      {{QStringLiteral("a"), QStringLiteral("Scene A")},
					       {QStringLiteral("b"), QStringLiteral("Scene B")},
					       {QStringLiteral("c"), QStringLiteral("Scene C")},
					       {QStringLiteral("free"), QStringLiteral("Free Scene")}}}};
		auto plan = planProjection(s, live);
		CHECK(plan.size() == 6);
		CHECK(R(plan, 0).kind == RowPlan::Folder && R(plan, 0).depth == 0 &&
		      R(plan, 0).name == QStringLiteral("A"));
		CHECK(R(plan, 1).kind == RowPlan::Scene && R(plan, 1).depth == 1 &&
		      R(plan, 1).name == QStringLiteral("Scene A") && R(plan, 1).placed);
		CHECK(R(plan, 2).kind == RowPlan::Folder && R(plan, 2).name == QStringLiteral("B"));
		CHECK(R(plan, 3).uuid == QStringLiteral("b") && R(plan, 3).depth == 2);
		CHECK(R(plan, 4).uuid == QStringLiteral("c") && R(plan, 4).depth == 0 && R(plan, 4).placed);
		CHECK(R(plan, 5).uuid == QStringLiteral("free") && !R(plan, 5).placed && R(plan, 5).path.empty());
	}
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "b", {}, 0));
		CHECK(s.placeScene("cv1", "a", {}, 1));
		std::vector<LiveCanvas> live{{QStringLiteral("cv1"),
					      QStringLiteral("Main"),
					      {{QStringLiteral("a"), QStringLiteral("Scene A")},
					       {QStringLiteral("b"), QStringLiteral("Scene B")},
					       {QStringLiteral("c"), QStringLiteral("Scene C")}}}};
		auto plan = planProjection(s, live);
		CHECK(R(plan, 0).uuid == QStringLiteral("b"));
		CHECK(R(plan, 1).uuid == QStringLiteral("a"));
		CHECK(R(plan, 2).uuid == QStringLiteral("c") && !R(plan, 2).placed);
		CHECK(live[0].scenes[0].uuid == QStringLiteral("a"));
		CHECK(live[0].scenes[1].uuid == QStringLiteral("b"));
		CHECK(live[0].scenes[2].uuid == QStringLiteral("c"));
	}
	{
		TreeStore s;
		CHECK(s.insertFolder("cv1", {}, 0, "A"));
		CHECK(s.placeScene("cv1", "m", {0}, 0));
		std::vector<LiveCanvas> live{
			{QStringLiteral("cv1"),
			 QStringLiteral("Main"),
			 {{QStringLiteral("m"), QStringLiteral("M")}, {QStringLiteral("free"), QStringLiteral("F")}}}};
		auto plan = planProjection(s, live);
		CHECK(plan.size() == 3);
		CHECK(R(plan, 0).kind == RowPlan::Folder && R(plan, 0).depth == 0);
		CHECK(R(plan, 1).uuid == QStringLiteral("m") && R(plan, 1).depth == 1);
		CHECK(R(plan, 2).uuid == QStringLiteral("free") && R(plan, 2).depth == 0);
	}
	{
		TreeStore s;
		s.fromJson(QStringLiteral("{\"version\":9}"));
		std::vector<LiveCanvas> live{
			{QStringLiteral("cv1"), QStringLiteral("Main"), {{QStringLiteral("a"), QStringLiteral("A")}}}};
		auto plan = planProjection(s, live);
		CHECK(plan.size() == 1 && !R(plan, 0).placed);
	}
	{
		TreeStore s;
		s.fromJson(QStringLiteral(
			"{\"version\":1,\"canvases\":{\"cv1\":{\"tree\":["
			"{\"t\":\"scene\",\"uuid\":\"dup\"},"
			"{\"t\":\"folder\",\"name\":\"F\",\"children\":[{\"t\":\"scene\",\"uuid\":\"dup\"}]}"
			"]}}}"));
		std::vector<LiveCanvas> live{{QStringLiteral("cv1"),
					      QStringLiteral("Main"),
					      {{QStringLiteral("dup"), QStringLiteral("Duplicate Scene")}}}};
		auto plan = planProjection(s, live);
		int sceneRows = 0;
		for (const auto &r : plan)
			if (r.kind == RowPlan::Scene)
				++sceneRows;
		CHECK(sceneRows == 1);
	}
}

static void test_name_fallback()
{
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "old", {}, 0));
		s.stampSceneNames({{"old", "Shared"}});
		s.resolveAndPrune({{"cv1", "Main", {{"", "Shared"}}}});
		CHECK(C(s.canvasRoot("cv1")).empty());
		CHECK(!s.findScene("cv1", ""));
		TreeStore restored;
		CHECK(restored.fromJson(s.toJson()));
		CHECK(restored.toJson() == s.toJson());
	}
	{
		TreeStore s;
		CHECK(s.insertFolder("cv1", {}, 0, "Opening"));
		CHECK(s.placeScene("cv1", "old-a", {0}, 0));
		CHECK(s.placeScene("cv1", "old-b", {}, 1));
		s.stampSceneNames({{"old-a", "Standby"}, {"old-b", "Ending"}});

		s.resolveAndPrune({{"cv1", "Main", {{"new-a", "Standby"}, {"new-b", "Ending"}}}});

		CHECK(N(s.nodeAt("cv1", {0})).name == QStringLiteral("Opening"));
		CHECK(N(s.nodeAt("cv1", {0, 0})).uuid == QStringLiteral("new-a"));
		CHECK(N(s.nodeAt("cv1", {1})).uuid == QStringLiteral("new-b"));
		CHECK(s.findScene("cv1", "old-a") == std::nullopt);
	}
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "u1", {}, 0));
		s.stampSceneNames({{"u1", "Old Name"}});
		s.resolveAndPrune({{"cv1", "Main", {{"u1", "New Name"}}}});
		CHECK(s.findScene("cv1", "u1").has_value());
	}
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "gone", {}, 0));
		s.stampSceneNames({{"gone", "Deleted"}});
		s.resolveAndPrune({{"cv1", "Main", {}}});
		CHECK(s.findScene("cv1", "gone") == std::nullopt);
	}
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "live", {}, 0));
		CHECK(s.placeScene("cv1", "dead", {}, 1));
		s.stampSceneNames({{"live", "A"}, {"dead", "A"}});
		s.resolveAndPrune({{"cv1", "Main", {{"live", "A"}}}});
		CHECK(s.findScene("cv1", "live").has_value());
		CHECK(C(s.canvasRoot("cv1")).size() == 1);
	}
	{
		TreeStore s;
		CHECK(s.insertFolder("cv1", {}, 0, "Folder"));
		CHECK(s.placeScene("cv1", "u", {0}, 0));
		s.stampSceneNames({{"u", "Scene Name"}});
		CHECK(N(s.nodeAt("cv1", {0})).name == QStringLiteral("Folder"));
		CHECK(N(s.nodeAt("cv1", {0, 0})).name == QStringLiteral("Scene Name"));
	}
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "u", {}, 0));
		s.stampSceneNames({{"u", "N"}});
		TreeStore t;
		CHECK(t.fromJson(s.toJson()));
		CHECK(N(t.nodeAt("cv1", {0})).name == QStringLiteral("N"));
		TreeStore o;
		CHECK(o.fromJson(QStringLiteral(
			"{\"version\":1,\"canvases\":{\"cv1\":{\"tree\":[{\"t\":\"scene\",\"uuid\":\"x\"}]}}}")));
		CHECK(o.findScene("cv1", "x").has_value());
		CHECK(N(o.nodeAt("cv1", {0})).name.isEmpty());
	}
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "stale-ghost", {}, 0));
		CHECK(s.placeScene("cv1", "genuine-live", {}, 1));
		s.stampSceneNames({{"stale-ghost", "Shared"}, {"genuine-live", "Shared"}});
		s.resolveAndPrune({{"cv1", "Main", {{"genuine-live", "Shared"}}}});
		CHECK(C(s.canvasRoot("cv1")).size() == 1);
		CHECK(N(s.nodeAt("cv1", {0})).uuid == QStringLiteral("genuine-live"));
	}
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "cv1-dead", {}, 0));
		CHECK(s.placeScene("cv2", "cv2-live", {}, 0));
		s.stampSceneNames({{"cv1-dead", "X"}, {"cv2-live", "X"}});
		s.resolveAndPrune({{"cv1", "Main", {}}, {"cv2", "Vertical", {{"cv2-live", "X"}}}});
		CHECK(C(s.canvasRoot("cv1")).empty());
		CHECK(N(s.nodeAt("cv2", {0})).uuid == QStringLiteral("cv2-live"));
	}
	{
		TreeStore s;
		CHECK(s.placeScene("cv1", "moved", {}, 0));
		s.stampSceneNames({{"moved", "M"}});
		s.resolveAndPrune({{"cv1", "Main", {}}, {"cv2", "Vertical", {{"moved", "M"}}}});
		CHECK(C(s.canvasRoot("cv1")).empty());
	}
	{
		TreeStore s;
		CHECK(s.fromJson(QStringLiteral(
			"{\"version\":1,\"canvases\":{\"cv1\":{\"tree\":[{\"t\":\"scene\",\"uuid\":\"dead\"}]}}}")));
		s.resolveAndPrune({{"cv1", "Main", {{"live", ""}}}});
		CHECK(C(s.canvasRoot("cv1")).empty());
	}
	{
		TreeStore s;
		CHECK(s.insertFolder("cv1", {}, 0, "F"));
		CHECK(s.placeScene("cv1", "u", {0}, 0));
		const QString before = s.toJson();
		s.resolveAndPrune({});
		CHECK(s.toJson() == before);
	}
	{
		TreeStore t;
		CHECK(t.fromJson(QStringLiteral("{\"version\":1,\"canvases\":{\"cv1\":{\"expanded\":false,"
						"\"tree\":[{\"t\":\"scene\",\"uuid\":\"u\"}]}},\"mru\":[]}")));
		CHECK(N(t.nodeAt("cv1", {0})).uuid == QStringLiteral("u"));
		CHECK(!t.toJson().contains(QStringLiteral("expanded\":false")));
		CHECK(!t.toJson().contains(QStringLiteral("\"mru\"")));
	}
}

int main()
{
	test_roundtrip();
	test_bad_json();
	test_foreign_version();
	test_failed_op_leaves_store_untouched();
	test_mutators();
	test_move();
	test_prune();
	test_edge_cases();
	test_name_fallback();
	test_projection();
	test_place_missing_preserves_tree();
	test_place_missing_duplicates_and_empty();
	test_place_missing_foreign();
	test_place_missing_large_batch();
	if (failures) {
		std::printf("%d FAILURES\n", failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
