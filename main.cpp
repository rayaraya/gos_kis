#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>
#include <map>
#include <set>
#include <iterator>
#include <algorithm>
#include <limits>
#include <sstream>
#include <unistd.h>


struct CommonSubSequenceBlock {
    uint32_t first_start;
    uint32_t second_start;
    uint16_t length;
};


template <typename T>
class Patch {
public:
    std::vector<std::pair<uint32_t, uint16_t>> common;
    std::vector<std::vector<T>> insertions;

    Patch() = default;

private:
    template <typename T1, typename STR>
    void write_type(const T1& data, STR& s) {
        s.write(reinterpret_cast<const char*>(&data), sizeof(T1));
    }

    template <typename Tout, typename TS>
    Tout read_type(TS& s) {
        Tout buffer;
        s.read(reinterpret_cast<char*>(&buffer), sizeof(Tout));
        return buffer;
    }

public:
    template <typename STR>
    void serialize(STR& s) {
        uint32_t common_len = common.size();
        write_type(common_len, s);

        for (const auto& [position, size]: common) {
            write_type(position, s);
            write_type(size, s);
        }

        uint32_t insertions_size = insertions.size();
        write_type(insertions_size, s);
        for (const auto& ins: insertions) {
            uint32_t size = ins.size();
            write_type(size, s);
            for (const auto& elem: ins) {
                write_type(elem, s);
            }
        }
    }

    template <typename STR>
    void deserialize(STR& s) {
        common.clear();
        uint32_t common_len = read_type<uint32_t>(s);
        for (size_t i = 0; i < common_len; ++i) {
            uint32_t position = read_type<uint32_t>(s);
            uint16_t len = read_type<uint16_t>(s);
            common.emplace_back(position, len);
        }

        insertions.clear();
        uint32_t insertions_size = read_type<uint32_t>(s);
        for (size_t i = 0; i < insertions_size; ++i) {
            uint32_t sz = read_type<uint32_t>(s);
            std::vector<T> data(sz);
            for (auto& elem: data) {
                elem = read_type<T>(s);
            }
            insertions.push_back(data);
        }
    }
};

template <typename T1, typename T2>
std::ostream& operator<<(std::ostream& s, const std::pair<T1, T2>& data) {
    s << "{" << data.first << "," << data.second << "}";
    return s;
}


template <typename T>
std::ostream& operator<<(std::ostream& s, const Patch<T>& data) {
    // s << "Patch: " << std::endl;
    // s << "    Common: " << data.common << std::endl;
    // s << "    Insertions: ";
    for (const auto ins: data.insertions) {
        for (const auto elem: ins) {
            s << elem;
        }
        s << ", ";
    }
    s << std::endl;
    return s;
}


std::vector<CommonSubSequenceBlock> SqueezeBlocks(const std::vector<CommonSubSequenceBlock> &data) {
    if (data.size() <= 1) {
        return data;
    }

    std::vector<CommonSubSequenceBlock> result;
    CommonSubSequenceBlock last_block{0, 0, 0};
    for (const auto &block: data) {
        if ((last_block.first_start + last_block.length == block.first_start) &&
            (last_block.second_start + last_block.length == block.second_start)) {
            last_block.length += block.length;
        } else {
            if (last_block.length > 0) {
                result.push_back(last_block);
            }
            last_block = block;
        }
    }
    return result;
}


std::ostream &operator<<(std::ostream &ostr, const CommonSubSequenceBlock &block) {
    ostr << "(" << block.first_start << " " << block.second_start << " " << block.length << ")";
    return ostr;
}


template<typename T>
std::ostream &operator<<(std::ostream &ostr, const std::vector<T> &data) {
    for (const auto &value: data) {
        ostr << value << " ";
    }
    return ostr;
}


class BlockCommonSubstring {
public:
    explicit BlockCommonSubstring(uint32_t memory_size, uint32_t min_block_size = 16)
            :
            memory_((memory_size + 1) * (memory_size + 1), 0),
            memory_size_(memory_size),
            min_block_size_(min_block_size) {
    }

    template<typename T>
    std::vector<CommonSubSequenceBlock> processBlock(
            const T *old_memory, uint32_t old_size,
            const T *new_memory, uint32_t new_size
    ) {

        if ((new_size > memory_size_) || (old_size > memory_size_)) {
            throw std::runtime_error("Block is too big");
        }

        // Compute LCS
        for (size_t new_index = 0; new_index <= new_size; ++new_index) {
            for (size_t old_index = 0; old_index <= old_size; ++old_index) {
                uint32_t &current_elem = getMem(new_index, old_index);

                if ((new_index == 0) || (old_index == 0)) {
                    current_elem = 0;
                } else if (old_memory[old_index - 1] == new_memory[new_index - 1]) {
                    current_elem = getMem(new_index - 1, old_index - 1) + 1;
                } else {

                    uint32_t &right_elem = getMem(new_index, old_index - 1);
                    uint32_t &top_elem = getMem(new_index - 1, old_index);

                    current_elem = std::max(right_elem, top_elem);
                }
            }
        }

        // Recover LCS
        std::vector<CommonSubSequenceBlock> result;

        std::pair<uint32_t, uint32_t> end_position;
        uint16_t current_length_ = 0;

        for (int32_t new_index = new_size, old_index = old_size; (new_index > 0) && (old_index > 0);) {
            if (new_memory[new_index - 1] == old_memory[old_index - 1]) {
                if (current_length_ == 0) {
                    current_length_ = 1;
                    end_position = std::make_pair(old_index, new_index);
                } else {
                    ++current_length_;
                }
                --new_index;
                --old_index;
            } else {
                if (current_length_ >= min_block_size_) {
                    result.push_back({end_position.first - current_length_,
                                      end_position.second - current_length_,
                                      current_length_});
                }
                current_length_ = 0;

                uint32_t current_value = getMem(new_index, old_index);

                if ((new_index > 0) && (getMem(new_index - 1, old_index) == current_value)) {
                    --new_index;
                }

                if ((old_index > 0) && (getMem(new_index, old_index - 1) == current_value)) {
                    --old_index;
                }
            }
        }
        if (current_length_ >= min_block_size_) {
            result.push_back({end_position.first - current_length_,
                              end_position.second - current_length_,
                              current_length_});
        }
        std::reverse(result.begin(), result.end());

//        std::cout << result << std::endl;
//        std::cout << "-------------------------------------" << std::endl;

        return SqueezeBlocks(result);
    }

    template<typename T>
    std::vector<CommonSubSequenceBlock> operator()(
            const T& s1, const T& s2
    ) {
        std::vector<CommonSubSequenceBlock> result;

        uint32_t st1 = 0, st2 = 0;

        while (st1 < s1.size() && st2 < s2.size()) {

            auto tmp_result = processBlock(s1.data() + st1, std::min<uint32_t>(memory_size_, s1.size() - st1),
                                           s2.data() + st2, std::min<uint32_t>(memory_size_, s2.size() - st2));

            for (const auto &elem: tmp_result) {
                result.push_back({elem.first_start + st1,
                                  elem.second_start + st2,
                                  elem.length});
            }

            if (!tmp_result.empty()) {
                CommonSubSequenceBlock last_result = *(tmp_result.rbegin());
                st1 += last_result.first_start + last_result.length;
                st2 += last_result.second_start + last_result.length;
            } else {
                st1 += memory_size_;
                st2 += memory_size_;
            }
        }
        result = SqueezeBlocks(result);
        return result;
    }

private:
    inline uint32_t &getMem(uint32_t i, uint32_t j) {
        return memory_[i * memory_size_ + j];
    }

    std::vector<uint32_t> memory_;
    uint32_t memory_size_;
    uint32_t min_block_size_;
};


template<typename T>
std::vector<T> read_file(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);
    std::ifstream::pos_type pos = ifs.tellg();

    std::vector<T> result(pos / sizeof(T));
   // std::cout << result.size() << std::endl;
    ifs.seekg(0, std::ios::beg);
    ifs.read((char *) (&result[0]), pos);

    return result;
}

template <typename T>
void write_file(const std::string& filename, const T& data) {
    std::ofstream ofs(filename, std::ios::out | std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
    ofs.close();
}

template <typename T, typename T2>
Patch<T> CreatePatch(const T2& data_old, const T2& data_new, uint32_t memory_size=2048, uint32_t min_block_size=32) {
    BlockCommonSubstring bs(memory_size, min_block_size);
    auto result_common_substr = bs(data_old, data_new);

    Patch<T> result;

    std::vector<std::pair<uint32_t, uint16_t>> common;
    for (const auto& elem: result_common_substr) {
        common.emplace_back(elem.first_start, elem.length);
    }

    result.common = common;
    std::vector<std::vector<T>> insertions;
    insertions.reserve(result_common_substr.size() + 1);

    uint32_t start_pos = 0;
    auto new_iter = data_new.begin();
    for (size_t i = 0; i < result_common_substr.size(); ++i) {
        uint32_t end_pos = result_common_substr[i].second_start;
        insertions.emplace_back(new_iter + start_pos, new_iter + end_pos);
        start_pos = result_common_substr[i].second_start +  result_common_substr[i].length;
    }
    if ((start_pos != 0) || (result_common_substr.empty())) {
        insertions.emplace_back(new_iter + start_pos, new_iter + data_new.size());
    }
    result.insertions = insertions;

    return result;
}



template <typename Tres, typename T, typename T2>
Tres ApplyPatch(const T2& data_old, const Patch<T>& patch) {
    std::vector<T> result;
    for (size_t i = 0; i < patch.common.size(); ++i) {
        for (const auto& elem: patch.insertions[i]) {
            result.emplace_back(elem);
        }
        for (size_t j = patch.common[i].first; j < patch.common[i].first + patch.common[i].second; ++j) {
            result.emplace_back(data_old[j]);
        }
    }
    
        for (const auto& elem: patch.insertions[patch.common.size()]) {
            result.emplace_back(elem);
        }
    
    return {result.begin(), result.end()};
}


int use_input(int argc, char ***argv) {

    char *first_file = NULL;
    char *second_file = NULL;
    char type_of_operation = 'd';

    while(1)
    {

        switch(getopt(argc, *argv, "duf:s:"))
        {
            case 'd':
                type_of_operation = 'd';
                continue;

            case 'u':
                type_of_operation = 'u';
                continue;

            case 'f':
                first_file = optarg;
                continue;

            case 's':
                second_file = optarg;
                continue;

            default :
                printf("Use \"-d -f old_file -s new_file\" to find diff and \"-u -f file_name -s diff_file\" to update file\n");
                break;

            case -1:
                break;
        }



        break;
    }
    auto s1 = read_file<uint8_t>(first_file);
    auto s2 = read_file<uint8_t>(second_file);
    // std::cout << first_file  <<  " " << second_file << std::endl;
    // std::cout << "s1: " << s1 << std::endl;
    // std::cout << "s2: " << s2 << std::endl;

    if (type_of_operation == 'd') {

        // Create patch
        auto patch = CreatePatch<uint8_t>(s1, s2, 1024, 16);
        std::ofstream out("patch", std::ios::out | std::ios::binary);
        patch.serialize(out);
        out.close();

        // std::cout << patch << std::endl;
    }

    if (type_of_operation == 'u') {
        // Read and apply patch
        std::ifstream in("patch", std::ios::in | std::ios::binary);
        Patch<uint8_t> read_patch;
        read_patch.deserialize(in);
        in.close();

        auto result = ApplyPatch<std::vector<std::uint8_t>>(s1, read_patch);

        write_file("result", result);
    }


    return 0;
}


int main(int argc, char **argv) {
    return use_input(argc, &argv);
}