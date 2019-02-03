#include <iostream>
#include <fstream>
#include <map>

#include "json.hpp"

using namespace nlohmann;

int main(int argc, char **argv) {
    std::string decode_func = 
        "template <typename V>\n"
        "boost::optional<const USSEMatcher<V> &> DecodeUSSE(uint64_t instruction) {\n"
        "   static const std::vector<USSEMatcher<V>> table {\n"
        "        // clang-format off\n"
        "#define INST(fn, name, bitstring) shader::decoder::detail::detail<USSEMatcher<V>>::GetMatcher(fn, name, bitstring)\n";

    std::string handler_decl;

    if (argc == 1) {
        std::cerr << "No input json!\n";
        return -1;
    }

    const char *rules_file = argv[1];

    // Load rules
    std::ifstream rules_stream(rules_file);
    if (!rules_stream) {
        std::cerr << "Can't open rules file!\n";
        return -1;
    }
    
    json rules;

    try {
        rules = json::parse(rules_stream);
    } catch (...) {
        std::cerr << "JSON load failed, lint your JSON\n";
        return -1;
    }
    
    for (auto &instruction: rules.items()) {
        std::string display_name = instruction.key();
        std::string handler_name = display_name;

        if (instruction.value().find("handler") != instruction.value().end()) {
            handler_name = instruction.value()["handler"].get<std::string>();
        }

        handler_decl += "bool " + handler_name + "(";

        std::string comments = "        /*\n";
        std::string inst_decl = "\n        INST(&v::" + handler_name + ", \"" + display_name + " ()\",     \"";

        bool occupied[120];

        auto allocate_bitstring_entry = [&](char &c) -> bool {
            for (auto i = 0; i < 120; i++) {
                if (!occupied[i]) {
                    occupied[i] = true;
                    c = 'a' + i;
                    return true;
                }
            }

            return false;
        };

        // Start with letter 'a' to 'z'
        std::fill(occupied, occupied + 120, false);

        int secid = 0;
        std::map<std::size_t, std::string, std::greater<std::size_t>> args;

        int total_sec = instruction.value()["defs"].size();

        for (auto &sec: instruction.value()["defs"].items()) {
            std::string bitarray;

            for (auto &field: sec.value().items()) {
                // Output field name
                std::string field_name = field.key();
                std::string handler_arg_type = "Imm";

                int c = 0;
                std::string bn;

                auto node = field.value().find("bitname");

                // Iter and get: Bit name in decoder and bit count
                if (node == field.value().end()) {
                    // Auto allocate
                    char bitstring_entry;
                    bool result = allocate_bitstring_entry(bitstring_entry);

                    if (!result) {
                        std::cerr << "Can't allocate new character for bitstring of instruction !" << display_name << "\n";
                        return -1;
                    }

                    bn = bitstring_entry;
                } else {
                    bn = node.value().get<std::string>();
                }

                try {
                    c = field.value()["count"].get<int>();
                } catch (...) {
                    std::cerr << "Bitcount for \"" << field_name << "\" of \"" << display_name << "\" not presents\n";
                }
                
                node = field.value().find("argtype");

                if (node != field.value().end()) {
                    // Custom type provided
                    handler_arg_type = node.value().get<std::string>();
                } else {
                    handler_arg_type += std::to_string(c);
                }

                node = field.value().find("offset");

                std::size_t offset;

                if (node != field.value().end()) {
                    offset = node.value().get<uint32_t>();
                } else {
                    std::cerr << "Offset not available for " << field_name << " in " << display_name << "!\n";
                    return -1;
                }
                
                // Process things
                if (offset < bitarray.size()) {
                    int tchar_to_delete =  std::min((int)(bitarray.size() - offset), c);
                    bitarray.erase(bitarray.length() - offset - tchar_to_delete, tchar_to_delete);
                    bitarray.insert(bitarray.length() - offset, c, bn[0]);
                } else if (offset > bitarray.size()) {
                    // Offset is bigger, install some -------
                    bitarray.insert(bitarray.begin(), offset - bitarray.size(), '-');
                    bitarray.insert(bitarray.begin(), c, bn[0]);
                } else {
                    bitarray.insert(bitarray.begin(), c, bn[0]);
                }

                comments += "           * " + bn + " = " + field_name + "\n";
                args.emplace(offset + (total_sec - 1 - secid) * 32, handler_arg_type + " " + field_name);
            }

            if (bitarray.size() != 32) {
                std::cerr << "WARNING: Section \"" << sec.key() << "\" defintion has total of bits not equal to 32, adding more!\n";
                std::cerr << "Bit array generated: " << bitarray << "\n";

                if (bitarray.size() < 32)
                    bitarray.insert(bitarray.begin(), 32 - bitarray.size(), '-');
            }

            secid++;
            inst_decl += bitarray;
        }

        for (auto &arg: args) {
            handler_decl += arg.second + ", ";
        }

        // Erase a lewd comma at the end, and extra spaces
        handler_decl.erase(handler_decl.length() - 2, 2);
        handler_decl += ") {\n\n}\n\n";

        inst_decl += "\"),\n";
        comments += "        */";
        decode_func += comments;
        decode_func += inst_decl;
    }

    // Erase extra comma
    decode_func.erase(decode_func.length() - 1);
    decode_func += "\n";

    // Finalize
    decode_func +=
        "        // clang-format on\n"
        "   };\n"
        "#undef INST\n"
        "   const auto matches_instruction = [instruction](const auto &matcher) { return matcher.Matches(instruction); };\n"
        "   auto iter = std::find_if(table.begin(), table.end(), matches_instruction);\n"
        "   return iter != table.end() ? boost::optional<const USSEMatcher<V> &>(*iter) : boost::none;\n"
        "}";

    std::ofstream decode_cpp_file("parse_gxp_usse.cpp");
    decode_cpp_file << handler_decl << "\n";
    decode_cpp_file << decode_func << "\n";

    return 0;
}