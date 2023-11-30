#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <format>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#define MessageBoxA(...) // g3t r3kt winblows

using u64   = std::uint64_t;
using i64   = std::int64_t;
using u32   = std::uint32_t;
using usize = std::size_t;

u32
lodword(u64 _num)
{
  return (u32)(_num & 0x00000000ffffffffull);
}

u32
hidword(u64 _num)
{
  return (u32)((_num & 0xffffffff00000000ull) >> 32);
}

usize
bits_to_represent(u32 num)
{
  usize out = 0;
  while (num)
  {
    num >>= 1;
    ++out;
  }
  return out;
}

template <typename TCharArr, std::size_t TSize>
struct ct_string
{
  consteval ct_string(const TCharArr (&arr)[TSize]) : data{std::to_array(arr)} {}
  consteval ct_string(const std::array<TCharArr, TSize> arr) : data{arr} {}

  [[nodiscard]] consteval std::string_view
  to_view() const noexcept
  {
    return {data.data() /* propably UB idk */};
  }
  std::array<TCharArr, TSize> data;
  constexpr static usize size = TSize;
};

template <ct_string THexStr>
constexpr std::string_view
remove_leading_zeros()
{
  if constexpr (THexStr.size == 0)
  {
    return std::string_view{};
  }
  else
  {
    usize counter = 0;
    while (counter != THexStr.to_view().size())
    {
      if (THexStr.data.at(counter) != '0')
      {
        break;
      }
      else
      {
        ++counter;
      }
    }
    return THexStr.to_view().substr(counter - 1, THexStr.size - counter);
  }
  return std::string_view{};
}

template <ct_string TStr>
consteval auto
hex_str_to_int_array()
{
  constexpr std::string_view _str = remove_leading_zeros<TStr>();
  // hex string to unsigned long
  constexpr auto strtoul_cxpr = [](const std::string_view str) constexpr
  {
    // hex digit to byte/uint
    constexpr auto ctob_cxpr = [](const char chr) constexpr -> u32
    {
      if (chr <= '9' && chr >= '0')
      {
        return ((u32)chr) - (u32)'0';
      }
      else if (chr <= 'F' && chr >= 'A')
      {
        return ((u32)chr) - (u32)'A' + 10;
      }
      else if (chr <= 'f' && chr >= 'a')
      {
        return ((u32)chr) - (u32)'a' + 10;
      }
      return (u32)-1;
    };

    u32 out{0};
    usize counter{0};

    for (usize i = str.size() - 1; i != (usize)-1; --i)
    {
      auto current_char = str.at(i);
      out |= (ctob_cxpr(current_char) << (4 * counter));
      ++counter;
    }
    return out;
  };

  if constexpr (_str.size() == 0)
  {
    return std::array<u32, 0>{};
  }
  else
  {
    constexpr usize real_length = (_str.size() - 1); // excluding stupid null terminator
    if constexpr (real_length == 0)
    {
      return std::array<u32, 0>{};
    }
    else
    {
      std::array<u32, (real_length / 8u) + ((((real_length / 8u)) * 8u) < real_length ? 1 : 0)> out{0};

      const usize blocks_count = out.size();
      for (usize i = blocks_count; i != 0; --i)
      {
        usize end_index   = real_length - (blocks_count - i) * 8;
        usize begin_index = std::max((i64)0, (i64)end_index - 8);
        auto &current     = out.at(blocks_count - i);
        current           = strtoul_cxpr(_str.substr(begin_index, end_index - begin_index));
      }
      return out;
    }
  }
}

template <ct_string THexString = "0">
struct vnum_t
{
  static constexpr u64 max64_val = 0xffffffffffffffffull;
  static constexpr u64 max32_val = 0xffffffffull;

  constexpr vnum_t()
  {
    constexpr auto num_array = hex_str_to_int_array<THexString>();
    if (num_array.size() == 0)
    {
      return;
    }
    for (const auto ite : num_array)
    {
      blocks.push_back(ite);
    }
  }

  vnum_t(const auto &other) : blocks(other.blocks), sign_bit(other.sign_bit){};

  decltype(auto)
  operator=(const auto &other)
  {
    blocks = other.blocks;
  }

  void
  resize_to_other(auto &other)
  {
    if (blocks.size() < other.blocks.size())
    {
      blocks.resize(other.blocks.size());
    }
    else if (blocks.size() > other.blocks.size())
    {
      other.blocks.resize(blocks.size());
    }
  }

  void
  truncate_zeros()
  {
    while (true)
    {
      if (blocks.size() == 0)
      {
        return;
      }
      usize lastindex = blocks.size() - 1;
      if (blocks[lastindex] == 0)
      {
        blocks.erase(blocks.begin() + lastindex);
      }
      else
      {
        break;
      }
    }
  }

  void
  add(u32 num)
  {
    vnum_t<""> temp;
    temp.blocks.push_back(num);
    add(temp);
  }

  void
  add(auto &other)
  {
    if (other.sign_bit)
    {
      if (other.is_greater(*this))
      {
        sign_bit = true;
      }
    }

    if (sign_bit && !other.sign_bit)
    {
      if (other.is_greater(*this))
      {
        sign_bit = false;
      }
    }

    resize_to_other(other);

    u64 overflow = 0;

    for (u64 i = 0; i != blocks.size(); ++i)
    {
      u64 addition_result = (u64)blocks[i] + (i < other.blocks.size() ? (u64)other.blocks[i] : 0);
      if (addition_result > max32_val)
      {
        u32 remainder = lodword(addition_result);
        blocks[i]     = remainder;

        overflow = hidword(addition_result);
        u64 j    = i + 1;
        // spill over the overflow
        while (true) // yay branch predictor is happy
        {
          if (j + 1 >= blocks.size())
          {
            blocks.push_back({0});
          }
          auto &current_next_block = blocks[j];

          if (max32_val - current_next_block >= overflow)
          {
            current_next_block += overflow;
            break;
          }
          else // time to spill the overflow on to the next block setting the
               // current block to
          {
            u64 n_overflow     = overflow - (max32_val - current_next_block);
            current_next_block = lodword(current_next_block + overflow);
            overflow           = n_overflow;
          }
          ++j;
        }
      }
      else
      {
        blocks[i] = addition_result;
      }
    }

    // truncate beginning 0s
    truncate_zeros();
  }

  void
  subtract(u32 num)
  {
    vnum_t<""> temp;
    temp.blocks.push_back(num);
    subtract(temp);
  }

  void
  subtract(auto &other)
  {
    // subtracting negative number from smaller negative number yields positive
    if (sign_bit && other.sign_bit)
    {
      if (other.is_greater(*this))
      {
        sign_bit = false;
      }
    }

    resize_to_other(other);

    vnum_t x = *this;
    vnum_t y = other;

    while (!y.is_empty()) // Iterate until carry becomes 0.
    {
      vnum_t borrow = (x._bnot(x))._band(y);
      x             = x._bxor(y);
      borrow.bsl(1);
      y = borrow;
    }

    blocks = x.blocks;

    // truncate beginning 0s
    truncate_zeros();

    if (other.is_greater(*this)) // remove sign bit cuz we aint about that shi
    {
      u32 &last_block = blocks.at(blocks.size() - 1);
      last_block &= (0xffffffff << (bits_to_represent(last_block) + 1));
      sign_bit = true;
    }
  }

  bool
  is_equal(const auto &other) const
  {
    return blocks == other.blocks;
  }

  bool
  is_greater(const auto &other) const
  {
    if (other.sign_bit && !sign_bit)
    {
      return true;
    }
    else if (sign_bit && !other.sign_bit)
    {
      return false;
    }
    /*
    if the both are negative the 2's complement
    is bigger for the lesser absolute value
    */
    else if (sign_bit && other.sign_bit)
    {
      return blocks < other.blocks;
    }
    return blocks > other.blocks;
  }

  bool
  is_empty()
  {
    truncate_zeros();
    return blocks.empty();
  }

  void
  band(auto &other)
  {
    resize_to_other(other);
    for (usize i = 0; i != blocks.size(); ++i)
    {
      blocks[i] &= other.blocks[i];
    }
    truncate_zeros();
    other.truncate_zeros();
  }

  vnum_t
  _band(auto &other)
  {
    vnum_t out{*this};
    out.band(other);
    return out;
  }

  void
  bxor(auto &other)
  {
    resize_to_other(other);
    for (usize i = 0; i != blocks.size(); ++i)
    {
      blocks[i] ^= other.blocks[i];
    }
    truncate_zeros();
    other.truncate_zeros();
  }

  vnum_t
  _bxor(auto &other)
  {
    vnum_t out{*this};
    out.bxor(other);
    return out;
  }

  void
  bor(auto &other)
  {
    resize_to_other(other);
    for (usize i = 0; i != blocks.size(); ++i)
    {
      blocks[i] |= other.blocks[i];
    }
    truncate_zeros();
    other.truncate_zeros();
  }

  vnum_t
  _bor(auto &other)
  {
    vnum_t out{*this};
    out.bor(other);
    return out;
  }

  void
  bnot(auto &other)
  {
    resize_to_other(other);
    for (usize i = 0; i != blocks.size(); ++i)
    {
      blocks[i] = ~other.blocks[i];
    }
    truncate_zeros();
    other.truncate_zeros();
  }

  vnum_t
  _bnot(auto &other)
  {
    vnum_t out{*this};
    out.bnot(other);
    return out;
  }

  void
  plus_zero()
  {
    return;
  }

  void
  minus_zero()
  {
    return;
  }

  void
  bsl(const usize count)
  {
    // TODO: what if we want to do more than 32 bits of shifting
    // answer: we can do a for loop and then use the remainder
    if (count >= sizeof(u32) * 8)
    {
      MessageBoxA(0, "bitshift left count to big", 0, 0);
    }

    // we go "left to right" which means msb to lsb
    // which means vector.end to vector.begin but still << for every single
    // vector
    usize index = blocks.size() - 1;
    u64 current_overflow{0};
    // these operations are a potential security risk
    // TODO: this loop ending condition is really ugly
    for (; index != (usize)-1; --index)
    {
      auto &block      = blocks[index];
      auto &next_block = blocks[index == blocks.size() - 1 ? index : index + 1];
      // the part that gets "cut off" by the bitshift operation
      current_overflow = (u64)hidword((u64)block << count);

      std::cout << "current_overflow: " << current_overflow << "\n";
      block <<= count;

      // currently doing the msb so the last element in the vector
      // TODO: move out of loop
      if (index == blocks.size() - 1) [[unlikely]]
      {
        std::cout << "adding: " << current_overflow << "\n";
        blocks.push_back(current_overflow);
      }
      else
      {
        // we take the current cut off bits and add them to the next aka more
        // significant vector
        // the fact that we are doing this in inverse order MSB to LSB accounts
        // for overwriting the bits that would've needed shifting potentially
        next_block = next_block | current_overflow;
      }
    }
  }

  void
  bsr(const usize count)
  {
    if (count >= sizeof(u32) * 8)
    {
      MessageBoxA(0, "bitshift right count too big", 0, 0);
    }

    // we go "left to right" which means msb to lsb
    // which means vector.end to vector.begin but still << for every single
    // vector
    usize index = 0;
    u64 current_overflow{0};
    // these operations are a potential security risk
    for (; index != blocks.size(); ++index)
    {
      auto &block      = blocks[index];
      auto &next_block = blocks[index == blocks.size() - 1 ? index : index + 1];
      // the part that gets "cut off" by the bitshift operation
      current_overflow = ((u64)lodword((u64)(((u64)block) << 32) >> count));

      block >>= count;
      next_block >>= count;
      block |= current_overflow;
      current_overflow = 0;
    }
  }

  // division involves splitting the divisor into powers of two to use bitshifts
  // to then subtract the rest of that operation from the number once for for
  // odd divisors

  // modulo is just number - (number / modulo)
  // mutiplication is bitshifts for powers of two and repeated addition for rest
  // powers are bitshifting + multiplication for rest

  void
  print()
  {
    vnum_t<THexString> print_num = *this;

    if (sign_bit) /* invert and add 1 for 2's compliment conversion */
    {
      print_num.truncate_zeros();
      print_num.bnot(print_num);
      print_num.subtract(1);
      std::cout << "-";
    }

    auto &print_blocks = print_num.blocks;

    if (print_blocks.size() == 0)
    {
      std::cout << "0x0\n";
      return;
    }
    std::cout << "0x";

    std::cout << std::hex << print_blocks.at(blocks.size() > 0 ? print_blocks.size() - 1 : 0);

    if (print_blocks.size() < 2)
    {
      std::cout << "\n";
      return;
    }

    for (usize i = print_blocks.size() - 2; i != (usize)-1; --i)
    {
      std::cout << std::setw(8) << std::setfill('0') << print_blocks.at(i);
    }
    std::cout << "\n";
  }

  std::vector<u32> blocks{};
  bool sign_bit;
};

int
main()
{
  vnum_t<"e58ef02f"> a1;
  vnum_t<"fffffffff"> a2;

  a1.print();
  a2.print();
  a1.subtract(a2);
  auto b = hex_str_to_int_array<"0000000000000001">();
  std::cout << remove_leading_zeros<"0000000000000001">();
  a1.print();
}