#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>

#include "string_builder.h"
#include "decoder.h"
#include "util.h"

const char* getFileName(const char* path) {
	#ifdef __WIN32
		constexpr char slash = '\\';
	#else
		constexpr char slash = '/';
	#endif
	const auto parts = String_View(path).split(slash);
	return parts[parts.size()-1].items;
}

Slice<u8> readEntireFile(const char* path) {
	FILE* file = fopen(path, "rb");
	if (!file) {
		eprintf(LOG_ERROR_STRING": Failed to open file %s", path);
		exit(1);
	}
	defer(fclose(file));

	fseek(file, 0, SEEK_END);
	auto const size = ftell(file);
	fseek(file, 0, SEEK_SET);

	u8* buffer = static_cast<u8*>(malloc(size));
	assertTrue(buffer != nullptr);
	fread(buffer, size, 1, file);

	return PtrToSlice(buffer, size);
}

bool isValidFileNameCharacter(char c) {
	switch (c) {
	case '#': case '%': case '&': case '{': case '}': case '<': case '>':
	case '*': case '?': case '/': case ' ': case '$': case '!': case '\'':
	case '\"': case ':': case '@': case '+': case '`': case '|': case '=':
	case '\\':
		return false;
	default:
		return true;
	}
}

char* pathToValidFileName(const char* path, size_t const len) {
	char* result = static_cast<char*>(malloc(len + 1));
	assertTrue(result != nullptr);
	for (size_t i = 0; i < len; i++) {
		if (!isValidFileNameCharacter(path[i])) {
			result[i] = '_';
		} else {
			result[i] = path[i];
		}
	}
	result[len] = 0;
	return result;
}

bool isFileAsm(std::string const& filename) {
	auto const parts = String_View(filename).split('.');
	auto const& last = parts[parts.size()-1];
	return 0 == strncmp(last.items, "asm", Min(last.count, 3));
}

std::vector<std::string> getAllAsmFilesInDir(const char* path) {
	namespace fs = std::filesystem;
	std::vector<std::string> result = {};
	if (!fs::exists(path)) {
		eprintfln(LOG_ERROR_STRING": The folder '%s' does not exist.", path);
		exit(1);
	} else if (!fs::is_directory(path)) {
		eprintfln(LOG_ERROR_STRING": The entry '%s' is not a directory.", path);
		exit(1);
	}
	for (auto const& entry: fs::directory_iterator(path)) {
		auto const& filename = entry.path().lexically_normal().string();
		if (entry.is_regular_file() && isFileAsm(filename)) {
			result.push_back(filename);
		}
	}
	return result;
}

bool hasSubString(std::string const& str, const char* substr) {
	size_t const len = str.size();
	size_t const subLen = strlen(substr);
	for (int i = 0; i < len; i++) {
		if (0 == strncmp(str.c_str() + i, substr, Min(len - i, subLen))) {
			return true;
		}
	}
	return false;
}

std::string fuzzyMatch(const char* path, const char* substr) {
	std::vector<std::string> entries = getAllAsmFilesInDir(path);
	for (auto const& entry: entries) {
		if (hasSubString(entry, substr)) {
			return entry;
		}
	}
	eprintfln(LOG_ERROR_STRING": Could not find an assembly file in '%s' with the substring '%s'.", path, substr);
	exit(1);
}

void usage(FILE* out, const char* program) {
	const char* programName = getFileName(program);
	fprintfln(out, "Usage: %s [-exec] [-d <directory>] <substring of *.asm>", programName);
	exit(out == stderr ? 1 : 0);
}

struct Cmd_Args {
	const char* asmFolder = nullptr;
	const char* asmSubstr = nullptr;
	bool exec = false;
	bool test = false;
	bool dump = false;

	explicit Cmd_Args(int const argc, char** argv) {
		std::vector<const char*> nonFlags = {};
		for (int i = 1; i < argc; i++) {
			const char* opt = argv[i];
			const char* arg = argv[i + 1];
			if (0 == strcmp(opt, "-h")) {
				usage(stdout, argv[0]);
			} else if (0 == strcmp(opt, "-exec")) {
				exec = true;
			} else if (0 == strcmp(opt, "-test")) {
				test = true;
			} else if (0 == strcmp(opt, "-dump")) {
				dump = true;
			} else if (0 == strcmp(opt, "-d")) {
				if (arg == nullptr) {
					usage(stderr, argv[0]);
				}
				asmFolder = arg;
				i++;
			} else {
				nonFlags.push_back(opt);
				asmSubstr = opt;
			}
		}
		size_t const nonFlagCount = nonFlags.size();
		if (nonFlagCount != 1) {
			eprintf(LOG_ERROR_STRING": Expected one argument to not be a flag, but got %llu (", nonFlagCount);
			for (int i = 0; i < nonFlagCount; i++) {
				if (i > 0) {
					eprintf(", ");
				}
				eprintf("'%s'", nonFlags[i]);
			}
			eprintfln(")");
			usage(stderr, argv[0]);
		}
		if (asmFolder == nullptr) {
			asmFolder = ".";
		}
		if (exec && test) {
			eprintfln(LOG_ERROR_STRING": Can not provide the flags '-exec' and '-test' at the same time.");
			usage(stderr, argv[0]);
		}
		if (dump && !exec) {
			eprintfln(LOG_ERROR_STRING": The flag `-dump` requires the flag `-exec`");
			usage(stderr, argv[0]);
		}
	}
};

bool runCommand(const char* command, bool const exitIfNotSuccess = true) {
	printfln(LOG_INFO_STRING": %s", command);
	int exitCode = system(command);
	if (exitCode != 0) {
		if (exitIfNotSuccess) {
			eprintfln(LOG_ERROR_STRING": Could not execute command '%s'", command);
			exit(1);
		} else {
			return false;
		}
	}
	return true;
}

void deleteFile(const char* path) {
	const int ok = remove(path);
	assertTrue(ok == 0);
	printfln(LOG_INFO_STRING": Deleted file '%s'.", path);
}

void compareFiles(const char* fileA, const char* fileB) {
	String_Builder command = string_builder_make();
#if _WIN32
	const char* compareCmd = "fc /b ";
#elif __APPLE__
	const char* compareCmd = "cmp -l ";
#else
	const char* compareCmd = "diff --binary ";
#endif
	command.append(compareCmd);
	command.append(fileA);
	command.append(" ");
	command.append(fileB);

	if (!runCommand(command.items, false)) {
		eprintfln(LOG_ERROR_STRING": The files are different!");
	}
	command.destroy();
}

void printProcessingHeader(const char* path) {
	String_Builder header = string_builder_make();
	defer(header.destroy());
	header.append(ASCII_COLOR_B_GREEN "Processing" ASCII_COLOR_END " file ");
	header.append(ASCII_COLOR_B_CYAN);
	header.append(path);
	header.append(ASCII_COLOR_END"\n");
	printf(header.items);
	size_t const actualHeaderLen = header.len - ASCII_COLOR_byteCount(header.items) - StrLen("\n");
	for (size_t i = 0; i < actualHeaderLen; i++) {
		putchar('-');
	}
	putchar('\n');
}

void processAsm(Cmd_Args const& cmdArgs, const char* inputAsmPath) {
	printProcessingHeader(inputAsmPath);
	FILE* inputAsm = fopen(inputAsmPath, "r");
	if (inputAsm == nullptr) {
		eprintfln("Could not load file '%s'", inputAsmPath);
		exit(1);
	}
	defer(fclose(inputAsm));

	char* validInputAsmName = pathToValidFileName(inputAsmPath, strlen(inputAsmPath)-StrLen(".asm"));
	defer(free(validInputAsmName));

	String_Builder tempFileName = string_builder_make();
	tempFileName.append(".Temp_Sim86_");
	tempFileName.append(validInputAsmName);
	defer(tempFileName.destroy());

	/* Compiling the input assembly file into a binary. */ {
		String_Builder command = string_builder_make();
		command.append("nasm -o ");
		command.append(tempFileName);
		command.append(' ');
		command.append(inputAsmPath);
		runCommand(command.items);
		command.destroy();
	}
	defer(deleteFile(tempFileName.items));

	FILE* tempFile = fopen(tempFileName.items, "rb");
	assertTrue(tempFile != nullptr);
	defer(fclose(tempFile));

	Slice<u8> inputBinary = readEntireFile(tempFileName.items);
	defer(free(inputBinary.ptr));

	const char* inputAsmFile = getFileName(inputAsmPath);
	if (cmdArgs.exec) {
		printfln("--- %s execution ---", inputAsmFile);
	} else {
		printfln(ASCII_COLOR_GREEN "; %s" ASCII_COLOR_END, inputAsmFile);
	}
	bool const couldDecode = decodeOrSimulate(stdout, inputBinary, cmdArgs.exec);
	putchar('\n');

	if (couldDecode && cmdArgs.dump) {
		assertTrue(cmdArgs.exec);
		size_t lastByteIndex = 0;
		for (size_t i = 0; i < std::size(gMemory); i++) {
			if (gMemory[i] != 0) {
				lastByteIndex = i;
			}
		}
		if (lastByteIndex > 0) {
			String_Builder dumpName = string_builder_make();
			dumpName.append("dump_");
			dumpName.append(validInputAsmName);
			defer(dumpName.destroy());
			FILE* dumpFile = fopen(dumpName.items, "wb");
			fwrite(gMemory, lastByteIndex, sizeof(u8), dumpFile);
			fclose(dumpFile);
			printfln(LOG_INFO_STRING": Created file '%s'", dumpName.items);
		}
	}

	if (couldDecode && cmdArgs.test) {
		assertTrue(cmdArgs.exec == false);
		String_Builder decodedAsmBinaryFileName = string_builder_make();
		decodedAsmBinaryFileName.append(".Temp_Sim86_Decoded_Output_");
		decodedAsmBinaryFileName.append(validInputAsmName);
		defer(decodedAsmBinaryFileName.destroy());

		String_Builder decodedAsmTextFileName = decodedAsmBinaryFileName.copy();
		decodedAsmTextFileName.append(".asm");
		defer(decodedAsmTextFileName.destroy());

		FILE* decodedAsmTextFile = fopen(decodedAsmTextFileName.items, "w");
		assertTrue(decodedAsmTextFile != nullptr);
		decodeOrSimulate(decodedAsmTextFile, inputBinary, false);
		fclose(decodedAsmTextFile);
		printfln(LOG_INFO_STRING": Created %s", decodedAsmTextFileName.items);
		defer(deleteFile(decodedAsmTextFileName.items));

		String_Builder command = string_builder_make();
		command.append("nasm -o ");
		command.append(decodedAsmBinaryFileName);
		command.append(' ');
		command.append(decodedAsmTextFileName);
		runCommand(command.items);
		command.destroy();
		defer(deleteFile(decodedAsmBinaryFileName.items));

		compareFiles(tempFileName.items, decodedAsmBinaryFileName.items);
	}
}

int main(int argc, char **argv) {
	#ifdef _DEBUG
		setvbuf(stdout, nullptr, _IONBF, 0);
	#endif
	Cmd_Args cmdArgs(argc, argv);

	if (0 == strcmp(cmdArgs.asmSubstr, ".all")) {
		auto const asmFiles = getAllAsmFilesInDir(cmdArgs.asmFolder);
		for (auto const& file: asmFiles) {
			processAsm(cmdArgs, file.c_str());
			printf("\n\n\n");
		}
	} else if (0 == strncmp(cmdArgs.asmSubstr, ".range", StrLen(".range"))) {
		String_View range(cmdArgs.asmSubstr + StrLen(".range"));
		if (range.count == 0 || range.items[0] != ':') {
			eprintfln(LOG_ERROR_STRING": The format is `.range:a:b` (inclusive) where a and b are 32-bit positive signed integers.");
		}
		range.advance(1);
		i32 const from = strtol(range.items, nullptr, 10);
		if (from < 0) {
			eprintfln(LOG_ERROR_STRING": The format is `.range:a:b` (inclusive) where a and b are 32-bit positive signed integers.");
		}
		range.advance(UnsignedDigitCount(from));
		if (range.count == 0 || range.items[0] != ':') {
			eprintfln(LOG_ERROR_STRING": The format is `.range:a:b` (inclusive) where a and b are 32-bit positive signed integers.");
		}
		range.advance(1);
		i32 const to = strtol(range.items, nullptr, 10);
		if (to < 0) {
			eprintfln(LOG_ERROR_STRING": The format is `.range:a:b` (inclusive) where a and b are 32-bit positive signed integers.");
		}
		range.advance(UnsignedDigitCount(to));
		if (range.count != 0) {
			eprintfln(LOG_ERROR_STRING": The format is `.range:a:b` (inclusive) where a and b are 32-bit positive signed integers.");
		}
		for (i32 i = from; i <= to; i++) {
			char it[I32_STR_SIZE_BASE10] = {0};
			snprintf(it, sizeof(it), "%" PRIi32, i);
            std::string const match = fuzzyMatch(cmdArgs.asmFolder, it);
            processAsm(cmdArgs, match.c_str());
		}
	} else {
		std::string const match = fuzzyMatch(cmdArgs.asmFolder, cmdArgs.asmSubstr);
		processAsm(cmdArgs, match.c_str());
	}

	return 0;
}
