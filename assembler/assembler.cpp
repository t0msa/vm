#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "../vm/vm.h"

static std::unordered_map<std::string_view, u8> jmp_map{
	{"jmp", JMP},
	{"je", JE},
	{"jne", JNE},
	{"jl", JL},
	{"jg", JG}
};

constexpr u32 FNV1A32_BASIS = 0x811c9dc5;
constexpr u32 FNV1A32_PRIME = 0x1000193;

constexpr u32 fnv1a32(const char* inp, const u32 hash = FNV1A32_BASIS) {
	return !inp[0] ? hash : fnv1a32(&inp[1], (hash ^ inp[0]) * FNV1A32_PRIME);
}

u32 fnv1a32_rt(const char* inp, const u32 hash = FNV1A32_BASIS) {
	return !inp[0] ? hash : fnv1a32(&inp[1], (hash ^ inp[0]) * FNV1A32_PRIME);
}

std::string read_file_into_string(const char* file) {
	std::ifstream file_in(file);

	if (!file_in.good())
		return "";

	return { std::istreambuf_iterator<char>(file_in), std::istreambuf_iterator<char>() };
}

std::vector<std::string> split(const std::string& inp) {
	std::vector<std::string> contents;

	std::string buf(260, '\0');

	for (auto i = 0u, buf_i = 0u; i < inp.size(); ++i) {
		if (inp[i] == '"') {
			const std::size_t next_quote = inp.find('"', i + 1);
			contents.push_back(inp.substr(i + 1, next_quote - i - 1));
			i = next_quote + 1;

			std::memset(buf.data(), 0, buf.size());
			buf_i = 0;
		}

		if (inp[i] == '[' || inp[i] == ']')
			contents.push_back({ inp[i++] });

		while (!std::isspace(inp[i]) && inp[i] != ',' && inp[i] != ']' && inp[i] != '\0')
			buf[buf_i++] = inp[i++];

		if (buf_i > 0) {
			contents.push_back(buf.substr(0, buf.find('\0')));
			std::memset(buf.data(), 0, buf.size());
			buf_i = 0;
		}
	}

	return contents;
}

u8 as_byte(const std::string& inp) {
	static u8 val;
	std::from_chars(&inp[0], &inp[inp.size()], val);
	return val;
}

// Life is life.
bool assemble(const std::string_view input_file, const std::string_view output_file) {
	const std::string contents = read_file_into_string(input_file.data());

	if (contents.empty())
		return false;

	std::unordered_map<std::string_view, std::vector<u32>> labels;

	const auto split_content = split(contents);

	std::vector<u8> assembly;

	for (auto i = 0u, vip = 0u; i < split_content.size(); ++i) {
		const auto& cur = split_content[i];

		switch (fnv1a32_rt(cur.c_str())) {
			case fnv1a32("push"):
			{
				assembly.insert(assembly.end(), { PUSH, as_byte(split_content[i + 1]) });
				vip += 2;
				break;
			}
			case fnv1a32("cmp"):
			{
				assembly.insert(assembly.end(), { CMP, split_content[i + 1] == "v0" ? u8(V0) : u8(V1), as_byte(split_content[i + 2]) });
				vip += 3;
				break;
			}
			case fnv1a32("jmp"):
			case fnv1a32("je"):
			case fnv1a32("jne"):
			case fnv1a32("jl"):
			case fnv1a32("jg"):
			{
				assembly.insert(assembly.end(), { jmp_map[cur], u8(PLACEHOLDER) });

				if (labels.find(split_content[i + 1]) == labels.end())
					labels[split_content[i + 1]].push_back(assembly.size() - 1);

				vip += 2;
				break;
			}
			case fnv1a32("mov"):
			{
				if (split_content[i + 1] == "v0" || split_content[i + 1] == "v1") {
					if (split_content[i + 2] == "[") {
						assembly.insert(assembly.end(), { MOV_REG_ADDR, split_content[i + 1] == "v0" ? u8(V0) : u8(V1), u8(PLACEHOLDER) });
						labels[split_content[i + 3]].push_back(assembly.size() - 1);
					}
					else {
						assembly.insert(assembly.end(), { MOV, split_content[i + 1] == "v0" ? u8(V0) : u8(V1), as_byte(split_content[i + 2]) });
					}
				}
				else if (split_content[i + 1] == "[") {
					assembly.insert(assembly.end(), { MOV_ADDR_IMM, u8(PLACEHOLDER), as_byte(split_content[i + 3]) });
					labels[split_content[i + 2]].push_back(assembly.size() - 2);
				}

				vip += 3;
				break;
			}
			case fnv1a32("print"):
			{
				assembly.insert(assembly.end(), { PRINT, PLACEHOLDER });
				labels[split_content[i + 1]].push_back(assembly.size() - 1);
				vip += 2;
				break;
			}
			case fnv1a32("halt"):
			{
				assembly.push_back(HALT);
				vip++;
				break;
			}
			default: break;
		}

		if (cur.find(':') != std::string::npos
			&& split_content[i + 1].find("string") == std::string::npos
			&& split_content[i + 1].find("byte") == std::string::npos) {
			const std::string_view label{ cur.begin(), cur.end() - 1 };

			if (labels.find(label) == labels.end()) {
				labels[label].push_back(vip);
				continue;
			}

			assembly.at(labels[label].at(0)) = vip;
		}
	}

	// Add padding between code and data.
	assembly.push_back(0xCC);

	for (auto i = 0u; i < split_content.size(); ++i) {
		if (i == split_content.size() - 1)
			break;

		if (split_content[i].find(":") != std::string::npos) {

			const std::string_view label{ split_content[i].begin(), split_content[i].end() - 1 };

			if (split_content[i + 1] == "string") {
				const std::string_view val{ split_content[i + 2].begin(), split_content[i + 2].end() };

				for (auto c : val)
					assembly.push_back(c);

				assembly.push_back(0x00);

				for (auto c : labels[label])
					assembly.at(c) = static_cast<u8>(assembly.size() - val.size() - 1);
			}
			else if (split_content[i + 1] == "byte") {
				assembly.push_back(as_byte(split_content[i + 2]));

				for (auto c : labels[label])
					assembly.at(c) = static_cast<u8>(assembly.size() - 1);
			}
		}
	}

	std::ofstream file_out(output_file.data(), std::ios::binary);

	file_out.write((const char*)assembly.data(), assembly.size());

	return true;
}


int main(int argc, char** argv) {
	if (argc < 3) {
		printf_s("Usage: <file in> <file out>\n");
		return 0;
	}

	printf_s("Assembling %s...\n", argv[1]);

	if (!assemble(argv[1], argv[2])) {
		printf_s("Program couldn't be assembled correctly!\n");
		return 0;
	}

	printf_s("Program assembled to %s.\n", argv[2]);
}