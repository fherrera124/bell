#include <doctest/doctest.h>

#include <array>
#include <cstdlib>
#include <memory>

extern "C" {
#include "../external/tremor/codebook.h"

ogg_int32_t* _book_unquantize(const static_codebook* book, int entries,
                             int* sparsemap, int* maxpoint);
}

TEST_CASE("Tremor preserves the binary point while unquantizing a codebook") {
  std::array<long, 4> lengths{2, 2, 2, 2};
  std::array<long, 8> quant{1, 2, 3, 4, 5, 6, 7, 8};
  static_codebook book{};
  book.dim = 2;
  book.entries = 4;
  book.lengthlist = lengths.data();
  book.maptype = 2;
  book.q_min = 0;
  book.q_delta = (788L << 21) | 1;  // Vorbis packed representation of 1.0.
  book.q_quant = 4;
  book.q_sequencep = 1;
  book.quantlist = quant.data();

  int point = 0;
  std::unique_ptr<ogg_int32_t, decltype(&std::free)> values(
      _book_unquantize(&book, 4, nullptr, &point), &std::free);
  REQUIRE(values);
  CHECK(point == -25);
  const std::array<ogg_int32_t, 8> expected{
      33554432, 100663296, 100663296, 234881024,
      167772160, 369098752, 234881024, 503316480};
  for (size_t i = 0; i < expected.size(); ++i) {
    CHECK(values.get()[i] == expected[i]);
  }
}
