#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "Aquila/Platform/Filesystem/Filesystem.h"
#include "Aquila/Platform/Filesystem/NativeFileSystem.h"

#include <fstream>

using namespace Aquila::Platform::Filesystem;

static std::string make_temp_root(const std::string &name) {
	std::string path = path_join(dir_get_current(), "__test_" + name);
	dir_create(path);
	return path;
}

static void cleanup_temp_root(const std::string &path) {
	dir_remove(path);
}

TEST_SUITE("Filesystem::Path") {
	TEST_CASE("path_join") {
		CHECK(path_join("/foo", "bar") == "/foo/bar");
		CHECK(path_join("/foo/", "bar") == "/foo/bar");
		CHECK(path_join("", "bar") == "bar");
		CHECK(path_join("/foo", "") == "/foo");
	}

	TEST_CASE("path_normalize") {
		CHECK(path_normalize("/foo/../bar") == "/bar");
		CHECK(path_normalize("/foo/./bar") == "/foo/bar");
		CHECK(path_normalize("//foo//bar") == "/foo/bar");
		CHECK(path_normalize("foo\\bar") == "foo/bar");
	}

	TEST_CASE("path_is_absolute") {
#ifdef AQUILA_PLATFORM_WINDOWS
		CHECK(path_is_absolute("C:\\foo") == true);
		CHECK(path_is_absolute("foo") == false);
#else
		CHECK(path_is_absolute("/foo") == true);
		CHECK(path_is_absolute("foo") == false);
		CHECK(path_is_absolute("") == false);
#endif
	}

	TEST_CASE("path_extension") {
		CHECK(path_extension("file.txt") == ".txt");
		CHECK(path_extension("file.tar.gz") == ".gz");
		CHECK(path_extension("file") == "");
		CHECK(path_extension(".hidden") == "");
	}
}

TEST_SUITE("Filesystem::Dir") {
	TEST_CASE("dir_create / dir_remove / FileStat") {
		const std::string root = make_temp_root("dircreate");
		const std::string sub = path_join(root, "subdir");

		CHECK(dir_create(sub) == true);
		const auto stat = file_stat(sub);
		CHECK(stat.exists == true);
		CHECK(stat.is_directory == true);

		CHECK(dir_remove(sub) == true);
		CHECK(file_stat(sub).exists == false);

		cleanup_temp_root(root);
	}

	TEST_CASE("dir_remove_all removes a folder with its contents but never the root") {
		const std::string root = make_temp_root("removeall");
		NativeFileSystem fs(root);
		CHECK(fs.dir_create("a"));
		CHECK(fs.dir_create("a/b"));
		std::ofstream(path_join(root, "a/b/file.txt")) << "x";

		CHECK(fs.dir_remove_all("a"));
		CHECK_FALSE(fs.dir_exists("a"));
		CHECK_FALSE(fs.dir_remove_all(""));
		CHECK_FALSE(fs.dir_remove_all("/"));
		CHECK_FALSE(fs.dir_remove_all("."));
		CHECK_FALSE(fs.dir_remove_all("x/.."));
		CHECK_FALSE(fs.dir_remove_all("../removeall"));
		CHECK(file_stat(root).exists);

		cleanup_temp_root(root);
	}

	TEST_CASE("dir_list") {
		const std::string root = make_temp_root("dirlist");

		// Create two subdirs and one file
		dir_create(path_join(root, "a"));
		dir_create(path_join(root, "b"));
		// write a tiny file
		FILE *f = fopen(path_join(root, "file.txt").c_str(), "w");
		if (f) {
			fputs("x", f);
			fclose(f);
		}

		auto entries = dir_list(root);
		CHECK(entries.size() == 3);

		cleanup_temp_root(root);
	}
}

TEST_SUITE("Filesystem::File") {
	TEST_CASE("file_move") {
		const std::string root = make_temp_root("filemove");
		const std::string src = path_join(root, "src.txt");
		const std::string dst = path_join(root, "dst.txt");

		FILE *f = fopen(src.c_str(), "w");
		REQUIRE(f != nullptr);
		fputs("data", f);
		fclose(f);

		CHECK(file_move(src, dst) == true);
		CHECK(file_exists(src) == false);
		CHECK(file_exists(dst) == true);

		cleanup_temp_root(root);
	}

	TEST_CASE("file_exists / file_remove") {
		const std::string root = make_temp_root("fileexists");
		const std::string file = path_join(root, "test.txt");

		CHECK(file_exists(file) == false);

		FILE *f = fopen(file.c_str(), "w");
		REQUIRE(f != nullptr);
		fputs("hello", f);
		fclose(f);

		CHECK(file_exists(file) == true);
		CHECK(file_remove(file) == true);
		CHECK(file_exists(file) == false);

		cleanup_temp_root(root);
	}
}

TEST_SUITE("NativeFileSystem") {
	TEST_CASE("dir_create / DirExists / dir_remove") {
		const std::string root = make_temp_root("nfs_dir");
		NativeFileSystem nfs(root);

		CHECK(nfs.dir_create("sub") == true);
		CHECK(nfs.dir_exists("sub") == true);
		CHECK(nfs.dir_remove("sub") == true);
		CHECK(nfs.dir_exists("sub") == false);

		cleanup_temp_root(root);
	}

	TEST_CASE("Constructor creates root if missing") {
		const std::string root = path_join(dir_get_current(), "__test_nfs_root");
		CHECK(file_exists(root) == false);

		NativeFileSystem nfs(root);
		const auto stat = file_stat(root);
		CHECK(stat.is_directory == true);

		dir_remove(root);
	}

	TEST_CASE("FileOpen / file_exists / file_remove") {
		const std::string root = make_temp_root("nfs_open");
		NativeFileSystem nfs(root);

		auto file = nfs.file_open("hello.txt", AccessMode::Write, OpenMode::Text);
		REQUIRE(file != nullptr);
		file.reset(); // close it

		CHECK(nfs.file_exists("hello.txt") == true);
		CHECK(nfs.file_remove("hello.txt") == true);
		CHECK(nfs.file_exists("hello.txt") == false);

		cleanup_temp_root(root);
	}

	TEST_CASE("FileGetSize") {
		const std::string root = make_temp_root("nfs_size");
		NativeFileSystem nfs(root);

		auto file = nfs.file_open("size.txt", AccessMode::Write, OpenMode::Binary);
		REQUIRE(file != nullptr);
		const char *data = "hello";
		file->write(data, 5);
		file.reset();

		CHECK(nfs.file_get_size("size.txt") == 5);

		cleanup_temp_root(root);
	}

	TEST_CASE("file_move renames within root") {
		const std::string root = make_temp_root("nfs_move");
		NativeFileSystem nfs(root);

		auto f = nfs.file_open("old.txt", AccessMode::Write, OpenMode::Text);
		REQUIRE(f != nullptr);
		f.reset();

		CHECK(nfs.file_move("old.txt", "new.txt") == true);
		CHECK(nfs.file_exists("old.txt") == false);
		CHECK(nfs.file_exists("new.txt") == true);

		cleanup_temp_root(root);
	}

	TEST_CASE("IsReadOnly returns false") {
		const std::string root = make_temp_root("nfs_ro");
		NativeFileSystem nfs(root);
		CHECK(nfs.is_read_only() == false);
		cleanup_temp_root(root);
	}
}
