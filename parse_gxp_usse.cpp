bool vmad(Imm19 test4, Imm12 test, Imm12 test3, dummy test2) {

}

bool vmov(Imm8 dmp4, Imm12 basd, Imm6 asdasd, Imm5 dmp1, Imm12 dmp6, dummy dmp5) {

}


template <typename V>
boost::optional<const USSEMatcher<V> &> DecodeUSSE(uint64_t instruction) {
   static const std::vector<USSEMatcher<V>> table {
        // clang-format off
#define INST(fn, name, bitstring) shader::decoder::detail::detail<USSEMatcher<V>>::GetMatcher(fn, name, bitstring)
        /*
           * 1 = test
           * a = test4
           * p = test2
           * 0 = test3
        */
        INST(&v::vmad, "vmad ()",     "aaaaaaaaaaaaaaaaaaa-111111111111000000000000----pppppppppppppppp"),
        /*
           * a = asdasd
           * b = basd
           * c = dmp1
           * d = dmp4
           * p = dmp5
           * 0 = dmp6
        */
        INST(&v::vmov, "vmov ()",     "dddddddd-bbbbbbbbbbbbaaaaaaccccc000000000000-pppppppppppppppp---"),
        // clang-format on
   };
#undef INST
   const auto matches_instruction = [instruction](const auto &matcher) { return matcher.Matches(instruction); };
   auto iter = std::find_if(table.begin(), table.end(), matches_instruction);
   return iter != table.end() ? boost::optional<const USSEMatcher<V> &>(*iter) : boost::none;
}
