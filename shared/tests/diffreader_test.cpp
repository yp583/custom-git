#include <gtest/gtest.h>
#include <sstream>
#include "diffreader.hpp"

class DiffReaderTest : public ::testing::Test {
protected:
    const std::string simple_diff = R"(diff --git a/foo.cpp b/foo.cpp
index 1234567..abcdefg 100644
--- a/foo.cpp
+++ b/foo.cpp
@@ -1,3 +1,4 @@
 #include <iostream>
+#include <string>
 int main() {
     return 0;
 }
)";

    const std::string multi_file_diff = R"(diff --git a/foo.cpp b/foo.cpp
index 1234567..abcdefg 100644
--- a/foo.cpp
+++ b/foo.cpp
@@ -1,2 +1,3 @@
 line1
+added_line
 line2
diff --git a/bar.cpp b/bar.cpp
index 1234567..abcdefg 100644
--- a/bar.cpp
+++ b/bar.cpp
@@ -1,2 +1,2 @@
-old_line
+new_line
 unchanged
)";

    const std::string deletion_diff = R"(diff --git a/test.cpp b/test.cpp
--- a/test.cpp
+++ b/test.cpp
@@ -1,4 +1,2 @@
 keep
-remove1
-remove2
 keep2
)";
};

TEST_F(DiffReaderTest, ParsesSimpleDiff) {
    std::istringstream input(simple_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    ASSERT_EQ(files.size(), 1);
    EXPECT_EQ(files[0].filepath, "foo.cpp");
}

TEST_F(DiffReaderTest, ParsesMultiFileDiff) {
    std::istringstream input(multi_file_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    ASSERT_EQ(files.size(), 2);
    EXPECT_EQ(files[0].filepath, "foo.cpp");
    EXPECT_EQ(files[1].filepath, "bar.cpp");
}

TEST_F(DiffReaderTest, DetectsInsertions) {
    std::istringstream input(simple_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    ASSERT_EQ(files.size(), 1);

    int insertion_count = 0;
    for (const auto& line : files[0].lines) {
        if (line.mode == INSERTION) {
            insertion_count++;
            EXPECT_EQ(line.content, "#include <string>");
        }
    }
    EXPECT_EQ(insertion_count, 1);
}

TEST_F(DiffReaderTest, DetectsDeletions) {
    std::istringstream input(deletion_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    ASSERT_EQ(files.size(), 1);

    int deletion_count = 0;
    for (const auto& line : files[0].lines) {
        if (line.mode == DELETION) {
            deletion_count++;
        }
    }
    EXPECT_EQ(deletion_count, 2);
}

TEST_F(DiffReaderTest, DetectsContextLines) {
    std::istringstream input(simple_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    ASSERT_EQ(files.size(), 1);

    int eq_count = 0;
    for (const auto& line : files[0].lines) {
        if (line.mode == EQ) {
            eq_count++;
        }
    }
    EXPECT_GE(eq_count, 1);
}

TEST_F(DiffReaderTest, EmptyDiff) {
    std::istringstream input("");
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    EXPECT_EQ(files.size(), 0);
}

// Tests for getDiffContent
TEST_F(DiffReaderTest, GetDiffContentFiltersInsertions) {
    std::istringstream input(multi_file_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    ASSERT_GE(files.size(), 1);

    DiffChunk insertions = getDiffContent(files[0], {INSERTION});
    EXPECT_EQ(insertions.filepath, "foo.cpp");

    for (const auto& line : insertions.lines) {
        EXPECT_EQ(line.mode, INSERTION);
    }
}

TEST_F(DiffReaderTest, GetDiffContentFiltersDeletions) {
    std::istringstream input(deletion_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    ASSERT_EQ(files.size(), 1);

    DiffChunk deletions = getDiffContent(files[0], {DELETION});

    for (const auto& line : deletions.lines) {
        EXPECT_EQ(line.mode, DELETION);
    }
    EXPECT_EQ(deletions.lines.size(), 2);
}

TEST_F(DiffReaderTest, GetDiffContentMultipleTypes) {
    std::istringstream input(multi_file_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    DiffChunk both = getDiffContent(files[1], {INSERTION, DELETION});

    EXPECT_EQ(both.lines.size(), 2);
}

TEST_F(DiffReaderTest, GetDiffContentEmptyTypesReturnsAll) {
    std::istringstream input(simple_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    DiffChunk all = getDiffContent(files[0], {});

    EXPECT_EQ(all.lines.size(), files[0].lines.size());
}

// Tests for combineContent
TEST_F(DiffReaderTest, CombineContentJoinsLines) {
    DiffChunk chunk;
    chunk.filepath = "test.cpp";
    chunk.lines = {
        {EQ, "line1", 0},
        {INSERTION, "line2", 1},
        {EQ, "line3", 2}
    };

    std::string combined = combineContent(chunk);
    EXPECT_EQ(combined, "line1\nline2\nline3\n");
}

TEST_F(DiffReaderTest, CombineContentEmptyChunk) {
    DiffChunk chunk;
    chunk.filepath = "test.cpp";

    std::string combined = combineContent(chunk);
    EXPECT_EQ(combined, "");
}

// Tests for hunk header parsing
TEST_F(DiffReaderTest, ParsesHunkHeaderLineNumbers) {
    std::istringstream input(simple_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    ASSERT_EQ(files.size(), 1);
    EXPECT_EQ(files[0].old_start, 1);
    EXPECT_EQ(files[0].new_start, 1);
}

TEST_F(DiffReaderTest, ParsesHunkHeaderNonOneStart) {
    const std::string diff_at_line_10 = R"(diff --git a/foo.cpp b/foo.cpp
--- a/foo.cpp
+++ b/foo.cpp
@@ -10,3 +15,4 @@
 context
+added
 context
 context
)";
    std::istringstream input(diff_at_line_10);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    ASSERT_EQ(files.size(), 1);
    EXPECT_EQ(files[0].old_start, 10);
    EXPECT_EQ(files[0].new_start, 15);
}

TEST_F(DiffReaderTest, GetDiffContentCopiesStartPositions) {
    const std::string diff_at_line_10 = R"(diff --git a/foo.cpp b/foo.cpp
--- a/foo.cpp
+++ b/foo.cpp
@@ -10,3 +15,4 @@
 context
+added
 context
 context
)";
    std::istringstream input(diff_at_line_10);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    DiffChunk chunk = getDiffContent(files[0], {});

    EXPECT_EQ(chunk.old_start, 10);
    EXPECT_EQ(chunk.new_start, 15);
}

// Tests for createPatch
TEST_F(DiffReaderTest, CreatePatchBasicFormat) {
    std::istringstream input(simple_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    DiffChunk chunk = getDiffContent(files[0], {});

    std::string patch = createPatch(chunk);

    // Check header
    EXPECT_NE(patch.find("--- a/foo.cpp"), std::string::npos);
    EXPECT_NE(patch.find("+++ b/foo.cpp"), std::string::npos);

    // Check hunk header exists
    EXPECT_NE(patch.find("@@ -"), std::string::npos);

    // Check line prefixes
    EXPECT_NE(patch.find("+#include <string>"), std::string::npos);
    EXPECT_NE(patch.find(" #include <iostream>"), std::string::npos);
}

TEST_F(DiffReaderTest, CreatePatchCorrectCounts) {
    std::istringstream input(deletion_diff);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    DiffChunk chunk = getDiffContent(files[0], {});

    std::string patch = createPatch(chunk);

    // deletion_diff: 2 context (EQ), 2 deletions
    // old_count = 4 (2 EQ + 2 DEL), new_count = 2 (2 EQ)
    EXPECT_NE(patch.find("@@ -1,4 +1,2 @@"), std::string::npos);
}

TEST_F(DiffReaderTest, CreatePatchRoundTrip) {
    // Parse a diff, create a patch, verify it looks like a valid patch
    const std::string original = R"(diff --git a/test.cpp b/test.cpp
--- a/test.cpp
+++ b/test.cpp
@@ -5,4 +5,5 @@
 line1
 line2
+new_line
 line3
 line4
)";
    std::istringstream input(original);
    DiffReader dr(input);
    dr.ingestDiff();

    std::vector<DiffFile> files = dr.getFiles();
    DiffChunk chunk = getDiffContent(files[0], {});

    std::string patch = createPatch(chunk);

    // Verify structure
    EXPECT_NE(patch.find("--- a/test.cpp"), std::string::npos);
    EXPECT_NE(patch.find("+++ b/test.cpp"), std::string::npos);
    EXPECT_NE(patch.find("@@ -5,4 +5,5 @@"), std::string::npos);
    EXPECT_NE(patch.find(" line1"), std::string::npos);
    EXPECT_NE(patch.find("+new_line"), std::string::npos);
}
