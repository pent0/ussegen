#include <iostream>
#include <fstream>

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

    json rules = json::parse(rules_stream);
    
    for (auto &instruction: rules.items()) {
        std::string display_name = instruction.key();
        std::string handler_name = display_name;

        if (instruction.value().find("handler") != instruction.value().end()) {
            handler_name = instruction.value()["handler"].get<std::string>();
        }

        handler_decl += "bool " + handler_name + "(";

        std::string comments = "        /*\n";
        std::string bitarray = "\n        INST(&v::" + handler_name + ", \"" + display_name + " ()\",     \"";

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

        for (auto &sec: instruction.value()["defs"].items()) {
            int total_bits_count = 0;

            for (auto &field: sec.value().items()) {
                // Output field name
                std::string field_name = field.key();
                std::string handler_arg_type = "Imm";

                std::string bn;
                int idx = 0;

                // Iter and get: Bit name in decoder and bit count
                if (field.value()[0].type() != json::value_t::string) {
                    // Auto allocate
                    char bitstring_entry;
                    bool result = allocate_bitstring_entry(bitstring_entry);

                    if (!result) {
                        std::cerr << "Can't allocate new character for bitstring of instruction !" << display_name << "\n";
                        return -1;
                    }

                    bn = bitstring_entry;
                    idx -= 1;
                } else {
                    bn = field.value()[idx].get<std::string>();
                }

                auto c = field.value()[idx + 1].get<int>();

                total_bits_count += c;

                if (field.value().size() == 3 + idx) {
                    // Custom type provided
                    handler_arg_type = field.value()[idx + 2].get<std::string>();
                } else {
                    handler_arg_type += std::to_string(c);
                }

                for (int i = 0 ; i < c; i++) {
                    bitarray += bn[0];
                }

                comments += "           * " + bn + " = " + field_name + "\n";
                handler_decl += handler_arg_type + " " + field_name + ", ";
            }

            if (total_bits_count != 32) {
                std::cerr << "WARNING: Section \"" << sec.key() << "\" defintion has total of bits not equal to 32!\n";
            }
        }

        // Erase a lewd comma at the end, and extra spaces
        handler_decl.erase(handler_decl.length() - 2, 2);
        handler_decl += ") {\n\n}\n";

        bitarray += "\"),";
        comments += "        */";
        decode_func += comments;
        decode_func += bitarray;
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