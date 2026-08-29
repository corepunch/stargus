/*
 Stargus launcher - start Stratagus with a chosen data directory.
 Copyright (C) 2010-2012 Pali Rohár <pali.rohar@gmail.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.
*/

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static void PrintUsage(const char *argv0)
{
	std::cout << "Usage: " << argv0 << " [-data=/path/to/data] [-map=path/to/map] [stratagus args...]\n"
	          << "  -data=PATH  Path to the Stargus data directory.\n"
	          << "              Defaults to ./data/ if omitted.\n"
	          << "  -map=PATH   Start directly on the specified map.\n"
	          << "              Without it, start at the main menu.\n";
}

static bool StartsWith(const char *value, const char *prefix)
{
	return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

static std::string FindStratagusBinary(const char *argv0)
{
#ifdef _WIN32
	const char *binary_name = "stratagus.exe";
#else
	const char *binary_name = "stratagus";
#endif

	fs::path launcher_path(argv0);
	if (launcher_path.has_parent_path()) {
		fs::path sibling = launcher_path.parent_path() / binary_name;
		if (fs::is_regular_file(sibling)) {
			return sibling.string();
		}
		fs::path make_build = launcher_path.parent_path() / "stratagus" / "build-make" / "bin" / binary_name;
		if (fs::is_regular_file(make_build)) {
			return make_build.string();
		}
	}

	fs::path cwd_candidate = fs::current_path() / binary_name;
	if (fs::is_regular_file(cwd_candidate)) {
		return cwd_candidate.string();
	}

	fs::path make_build = fs::current_path() / "stratagus" / "build-make" / "bin" / binary_name;
	if (fs::is_regular_file(make_build)) {
		return make_build.string();
	}

	return binary_name;
}

static int LaunchStratagus(const std::string &binary, const std::string &data_dir,
	const std::vector<std::string> &forwarded_args)
{
	std::vector<std::string> args;
	args.reserve(forwarded_args.size() + 3);
	args.emplace_back(binary);
	args.emplace_back("-d");
	args.emplace_back(data_dir);

	for (const std::string &arg : forwarded_args) {
		args.emplace_back(arg);
	}

	std::vector<char *> cargs;
	cargs.reserve(args.size() + 1);
	for (std::string &arg : args) {
		cargs.push_back(arg.data());
	}
	cargs.push_back(nullptr);

#ifdef _WIN32
	int ret = _spawnvp(_P_WAIT, binary.c_str(), cargs.data());
	if (ret == -1) {
		return errno != 0 ? errno : ENOENT;
	}
	return ret;
#else
	pid_t pid = fork();
	if (pid == 0) {
		execvp(binary.c_str(), cargs.data());
		_exit(ENOENT);
	}
	if (pid < 0) {
		return errno;
	}

	int status = 0;
	if (waitpid(pid, &status, 0) < 0) {
		return errno;
	}
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	if (WIFSIGNALED(status)) {
		return 128 + WTERMSIG(status);
	}
	return 1;
#endif
}

int main(int argc, char *argv[])
{
	std::string data_dir = "data/";
	std::vector<std::string> stratagus_args;
	stratagus_args.reserve(static_cast<size_t>(argc));

	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (StartsWith(arg, "-data=")) {
			data_dir = arg + 6;
			continue;
		}
		if (!std::strcmp(arg, "-data") || !std::strcmp(arg, "--data")) {
			std::cerr << "Error: use -data=/path/to/data\n\n";
			PrintUsage(argv[0]);
			return 1;
		}
		if (!std::strcmp(arg, "-h") || !std::strcmp(arg, "--help")) {
			PrintUsage(argv[0]);
			return 0;
		}
		if (StartsWith(arg, "-map=")) {
			const char *map = arg + 5;
			if (*map == '\0') {
				std::cerr << "Error: -map= requires a map path\n\n";
				PrintUsage(argv[0]);
				return 1;
			}
			// Stratagus loads a map supplied as its positional argument.
			stratagus_args.emplace_back(map);
			continue;
		}
		stratagus_args.emplace_back(arg);
	}

	const std::string stratagus_binary = FindStratagusBinary(argv[0]);
	const int ret = LaunchStratagus(stratagus_binary, data_dir, stratagus_args);
	if (ret == ENOENT) {
		std::cerr << "Failed to launch Stratagus: " << stratagus_binary << "\n";
		return 1;
	}
	return ret;
}
