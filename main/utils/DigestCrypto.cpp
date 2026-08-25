#ifndef BELL_DISABLE_MBEDTLS
#include "bell/utils/DigestCrypto.h"
#include "fmt/format.h"

#if MBEDTLS_VERSION_MAJOR >= 4
#include <mbedtls/psa_util.h>  // mbedtls_md_psa_alg_from_type
#endif

using namespace bell;

utils::DigestCrypto::DigestCrypto(mbedtls_md_type_t type, bool hmac) {
  if (type == MBEDTLS_MD_NONE) {
    throw std::invalid_argument("Invalid hash type");
  }

  digestType = type;

  // Initialize the context
  mbedtls_md_init(&ctx);
#if MBEDTLS_VERSION_MAJOR >= 4
  (void)hmac;
  auto result = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(type), 0);
#else
  auto result = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(type), hmac);
#endif
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to setup digest context, mbedtls error: {}", result));
  }

  reset();
}

utils::DigestCrypto::~DigestCrypto() {
  // Free the context
  mbedtls_md_free(&ctx);
#if MBEDTLS_VERSION_MAJOR >= 4
  // Tear down any in-flight PSA MAC operation and imported HMAC key.
  psa_mac_abort(&macOp);
  psa_destroy_key(hmacKey);
#endif
}

void utils::DigestCrypto::reset() {
  // Reset the context
  auto result = mbedtls_md_starts(&ctx);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to reset digest context, mbedtls error: {}", result));
  }

#if MBEDTLS_VERSION_MAJOR < 4
  if (hmacInitialized) {
    // Reset the HMAC context
    result = mbedtls_md_hmac_reset(&ctx);
    if (result != 0) {
      throw std::runtime_error(fmt::format(
          "Failed to reset HMAC context, mbedtls error: {}", result));
    }
  }
#endif
  // On Mbed TLS 4.0 the HMAC operation is restarted on next call in hmac()
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
#if MBEDTLS_VERSION_MAJOR >= 4
  // Restart from clean state
  psa_mac_abort(&macOp);
  psa_destroy_key(hmacKey);
  hmacKey = mbedtls_svc_key_id_t{};

  psa_algorithm_t alg = PSA_ALG_HMAC(mbedtls_md_psa_alg_from_type(digestType));

  psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
  psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
  psa_set_key_algorithm(&attr, alg);
  psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);

  psa_status_t status = psa_import_key(
      &attr, reinterpret_cast<const uint8_t*>(key), keyLength, &hmacKey);
  if (status != PSA_SUCCESS) {
    throw std::runtime_error(
        fmt::format("Failed to import HMAC key, PSA error: {}", status));
  }

  status = psa_mac_sign_setup(&macOp, hmacKey, alg);
  if (status != PSA_SUCCESS) {
    psa_destroy_key(hmacKey);
    hmacKey = mbedtls_svc_key_id_t{};
    throw std::runtime_error(
        fmt::format("Failed to start HMAC operation, PSA error: {}", status));
  }
#else
  // Initialize the HMAC context
  auto result = mbedtls_md_hmac_starts(
      &ctx, reinterpret_cast<const uint8_t*>(key), keyLength);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to initialize HMAC context, mbedtls error: {}", result));
  }
#endif

  hmacInitialized = true;
}

void utils::DigestCrypto::hmacUpdate(const std::byte* bytes, size_t length) {
#if MBEDTLS_VERSION_MAJOR >= 4
  psa_status_t status =
      psa_mac_update(&macOp, reinterpret_cast<const uint8_t*>(bytes), length);
  if (status != PSA_SUCCESS) {
    throw std::runtime_error(
        fmt::format("Failed to update HMAC operation, PSA error: {}", status));
  }
#else
  // Update the HMAC context with the specified bytes
  auto result = mbedtls_md_hmac_update(
      &ctx, reinterpret_cast<const uint8_t*>(bytes), length);
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to update HMAC context, mbedtls error: {}", result));
  }
#endif
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
#if MBEDTLS_VERSION_MAJOR >= 4
  size_t macLength = 0;
  psa_status_t status = psa_mac_sign_finish(
      &macOp, reinterpret_cast<uint8_t*>(output), getDigestSize(), &macLength);
  // The imported key is single-use per HMAC computation; drop it regardless of
  // the outcome (psa_mac_sign_finish already released the operation on success).
  psa_destroy_key(hmacKey);
  hmacKey = mbedtls_svc_key_id_t{};
  hmacInitialized = false;
  if (status != PSA_SUCCESS) {
    throw std::runtime_error(
        fmt::format("Failed to finish HMAC operation, PSA error: {}", status));
  }
#else
  // Finalize the HMAC context and store the result in the output array
  auto result =
      mbedtls_md_hmac_finish(&ctx, reinterpret_cast<uint8_t*>(output));
  if (result != 0) {
    throw std::runtime_error(fmt::format(
        "Failed to finish HMAC computation, mbedtls error: {}", result));
  }
#endif
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
#endif  // BELL_DISABLE_MBEDTLS
