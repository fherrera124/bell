#include "bell/utils/DigestCrypto.h"
#include "fmt/format.h"

using namespace bell;

utils::DigestCrypto::DigestCrypto(mbedtls_md_type_t type, bool hmac) {
  if (type == MBEDTLS_MD_NONE) {
    throw std::invalid_argument("Invalid hash type");
  }

  digestType = type;

  // Initialize the context
  mbedtls_md_init(&ctx);
  auto result = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(type), hmac);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to setup digest context, mbedtls error: {}", result));
  }

  reset();
}

utils::DigestCrypto::~DigestCrypto() {
  // Free the context
  mbedtls_md_free(&ctx);
}

void utils::DigestCrypto::reset() {
  // Reset the context
  auto result = mbedtls_md_starts(&ctx);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to reset digest context, mbedtls error: {}", result));
  }

  if (hmacInitialized) {
    // Reset the HMAC context
    result = mbedtls_md_hmac_reset(&ctx);
    if (result != 0) {
      throw std::runtime_error(fmt::format(
          "Failed to reset HMAC context, mbedtls error: {}", result));
    }
  }
}

void utils::DigestCrypto::update(const std::byte* bytes, size_t length) {
  // Update the context with the specified bytes
  auto result =
      mbedtls_md_update(&ctx, reinterpret_cast<const uint8_t*>(bytes), length);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to update digest context, mbedtls error: {}", result));
  }
}

void utils::DigestCrypto::updateString(std::string_view str) {
  // Update the context with the specified string
  update(reinterpret_cast<const std::byte*>(str.data()), str.size());
}

void utils::DigestCrypto::hmac(const std::byte* key, size_t keyLength) {
  // Initialize the HMAC context
  auto result = mbedtls_md_hmac_starts(
      &ctx, reinterpret_cast<const uint8_t*>(key), keyLength);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to initialize HMAC context, mbedtls error: {}", result));
  }

  hmacInitialized = true;
}

void utils::DigestCrypto::hmacUpdate(const std::byte* bytes, size_t length) {
  // Update the HMAC context with the specified bytes
  auto result = mbedtls_md_hmac_update(
      &ctx, reinterpret_cast<const uint8_t*>(bytes), length);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to update HMAC context, mbedtls error: {}", result));
  }
}

void utils::DigestCrypto::hmacUpdateString(const std::string_view& key) {
  // Update the HMAC context with the specified string
  hmacUpdate(reinterpret_cast<const std::byte*>(key.data()), key.size());
}

void utils::DigestCrypto::finish(std::byte* output) {
  // Finalize the context and store the result in the output array
  auto result = mbedtls_md_finish(&ctx, reinterpret_cast<uint8_t*>(output));
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to finish digest computation, mbedtls error: {}", result));
  }
}

void utils::DigestCrypto::hmacFinish(std::byte* output) {
  // Finalize the HMAC context and store the result in the output array
  auto result =
      mbedtls_md_hmac_finish(&ctx, reinterpret_cast<uint8_t*>(output));
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to finish HMAC computation, mbedtls error: {}", result));
  }
}

size_t utils::DigestCrypto::getDigestSize() {
  // Get the length of the digest
  return mbedtls_md_get_size(mbedtls_md_info_from_type(digestType));
}

void utils::DigestCrypto::getDigest(const std::byte* bytes, size_t length,
                                    std::byte* output) {
  hmacInitialized = false;
  reset();
  update(bytes, length);
  finish(output);
}

void utils::DigestCrypto::getHmac(const std::byte* key, size_t keyLength,
                                  const std::byte* message,
                                  size_t messageLength, std::byte* output) {
  hmacInitialized = true;
  reset();
  hmac(key, keyLength);
  hmacUpdate(message, messageLength);
  hmacFinish(output);
}
