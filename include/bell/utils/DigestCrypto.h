#pragma once

// System includes
#include <string_view>

// Local includes
#include <mbedtls/md.h>

namespace bell::utils {
class DigestCrypto {
 public:
  /**
   * @brief Constructs an DigestCrypto object.
   *
   * Initializes the digest context with the specified hashing algorithm.
   * Optionally initializes for HMAC if specified.
   *
   * @param type The type of hash algorithm to use.
   * @param hmac Set to true to initialize for HMAC operations. Default is false.
   */
  DigestCrypto(mbedtls_md_type_t type, bool hmac = false);
  ~DigestCrypto();

  /**
   * @brief Updates the digest with a chunk of data.
   *
   * Feeds the specified bytes into the ongoing hash computation.
   *
   * @param bytes Pointer to the byte array to update the hash with.
   * @param length The number of bytes to process.
   */
  void update(const uint8_t* bytes, size_t length);

  /**
   * @brief Updates the digest with a string.
   *
   * Feeds the specified string into the ongoing hash computation.
   *
   * @param str The string to update the hash with.
   */
  void updateString(std::string_view str);

  /**
   * @brief Prepares the context for HMAC operations with a provided key.
   *
   * Initializes the internal state for HMAC processing with the given key.
   *
   * @param key Pointer to the key array.
   * @param keyLength Length of the key in bytes.
   */
  void hmac(const uint8_t* key, size_t keyLength);

  /**
   * @brief Updates the HMAC with a chunk of data.
   *
   * Feeds the specified bytes into the ongoing HMAC computation.
   *
   * @param bytes Pointer to the byte array to update the HMAC with.
   * @param length The number of bytes to process.
   */
  void hmacUpdate(const uint8_t* bytes, size_t length);

  /**
   * @brief Updates the HMAC with a string.
   *
   * Feeds the specified string into the ongoing HMAC computation.
   *
   * @param key The string to update the HMAC with.
   */
  void hmacUpdateString(const std::string_view& key);

  /**
   * @brief Finalizes the digest computation.
   *
   * Completes the hash computation and stores the result in the output array.
   *
   * @remark Size of the output array must be at least getDigestSize() bytes.
   * @param output Pointer to the output array where the digest result will be stored.
   */
  void finish(uint8_t* output);

  /**
   * @brief Finalizes the HMAC computation.
   *
   * Completes the HMAC computation and stores the result in the output array.
   *
   * @remark Size of the output array must be at least getDigestSize() bytes.
   * @param output Pointer to the output array where the HMAC result will be stored.
   */
  void hmacFinish(uint8_t* output);

  /**
   * @brief Gets the size of the digest output.
   *
   * Returns the size of the resulting digest in bytes for the selected algorithm.
   *
   * @return The digest size in bytes.
   */
  size_t getDigestSize();

  /**
   * @brief Resets the digest context.
   */
  void reset();

  /**
   * @brief Performs a one-shot hash computation, simplifying the process.
   *
   * @param bytes Pointer to the byte array to hash.
   * @param length The number of bytes to hash.
   * @param output Pointer to the output array where the digest result will be stored, must be at least getDigestSize() bytes.
   */
  void getDigest(const uint8_t* bytes, size_t length, uint8_t* output);

  /**
   * @brief Performs a one-shot HMAC computation, simplifying the process.
   *
   * @param key Pointer to the key array.
   * @param keyLength Length of the key in bytes.
   * @param message Pointer to the message array.
   * @param messageLength Length of the message in bytes.
   * @param output Pointer to the output array where the HMAC result will be stored, must be at least getDigestSize() bytes.
   */
  void getHmac(const uint8_t* key, size_t keyLength, const uint8_t* message,
               size_t messageLength, uint8_t* output);

 private:
  mbedtls_md_context_t ctx{};
  mbedtls_md_type_t digestType;

  // Flag indicating if the context is initialized for HMAC.
  bool hmacInitialized = false;
};
}  // namespace bell::utils

namespace bell {
using DigestCrypto = utils::DigestCrypto;
}
