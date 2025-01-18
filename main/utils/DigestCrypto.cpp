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

void utils::DigestCrypto::update(const uint8_t* bytes, size_t length) {
  // Update the context with the specified bytes
  auto result = mbedtls_md_update(&ctx, bytes, length);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to update digest context, mbedtls error: {}", result));
  }
}

void utils::DigestCrypto::updateString(std::string_view str) {
  // Update the context with the specified string
  update(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

void utils::DigestCrypto::hmac(const uint8_t* key, size_t keyLength) {
  // Initialize the HMAC context
  auto result = mbedtls_md_hmac_starts(&ctx, key, keyLength);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to initialize HMAC context, mbedtls error: {}", result));
  }

  hmacInitialized = true;
}

void utils::DigestCrypto::hmacUpdate(const uint8_t* bytes, size_t length) {
  // Update the HMAC context with the specified bytes
  auto result = mbedtls_md_hmac_update(&ctx, bytes, length);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to update HMAC context, mbedtls error: {}", result));
  }
}

void utils::DigestCrypto::hmacUpdateString(const std::string_view& key) {
  // Update the HMAC context with the specified string
  hmacUpdate(reinterpret_cast<const uint8_t*>(key.data()), key.size());
}

void utils::DigestCrypto::finish(uint8_t* output) {
  // Finalize the context and store the result in the output array
  auto result = mbedtls_md_finish(&ctx, output);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to finish digest computation, mbedtls error: {}", result));
  }
}

void utils::DigestCrypto::hmacFinish(uint8_t* output) {
  // Finalize the HMAC context and store the result in the output array
  auto result = mbedtls_md_hmac_finish(&ctx, output);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to finish HMAC computation, mbedtls error: {}", result));
  }
}

size_t utils::DigestCrypto::getDigestSize() {
  // Get the length of the digest
  return mbedtls_md_get_size(mbedtls_md_info_from_type(digestType));
}

void utils::DigestCrypto::getDigest(const uint8_t* bytes, size_t length,
                                    uint8_t* output) {
  hmacInitialized = false;
  reset();
  update(bytes, length);
  finish(output);
}

void utils::DigestCrypto::getHmac(const uint8_t* key, size_t keyLength,
                                  const uint8_t* message, size_t messageLength,
                                  uint8_t* output) {
  hmacInitialized = true;
  hmac(key, keyLength);
  hmacUpdate(message, messageLength);
  hmacFinish(output);
}