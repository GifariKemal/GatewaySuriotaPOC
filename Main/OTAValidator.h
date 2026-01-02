/**
 * @file OTAValidator.h
 * @brief OTA Firmware Validation - Signature & Checksum Verification
 * @version 1.0.0
 * @date 2025-11-26
 *
 * Provides security validation for OTA firmware updates:
 * - ECDSA P-256 signature verification
 * - SHA-256 checksum validation
 * - Version anti-rollback protection
 * - Firmware header validation
 *
 * Security Layers (from OTA_UPDATE.md):
 * - Layer 4: Firmware Signature (ECDSA P-256)
 * - Layer 3: Integrity Check (SHA-256)
 * - Layer 2: Transport Security (HTTPS/BLE)
 * - Layer 1: Version Control (anti-rollback)
 */

#ifndef OTA_VALIDATOR_H
#define OTA_VALIDATOR_H

#include <Arduino.h>

#include "DebugConfig.h"  // MUST BE FIRST
#include "OTAConfig.h"

// mbedTLS includes for cryptographic operations
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"

// Forward declaration
class OTAValidator;

/**
 * @brief Validation result structure
 */
struct ValidationResult {
  bool valid;
  OTAError error;
  String message;

  // Firmware info (populated on successful validation)
  uint8_t majorVersion;
  uint8_t minorVersion;
  uint8_t patchVersion;
  uint32_t buildNumber;
  uint32_t firmwareSize;

  ValidationResult()
      : valid(false),
        error(OTAError::NONE),
        majorVersion(0),
        minorVersion(0),
        patchVersion(0),
        buildNumber(0),
        firmwareSize(0) {}
};

/**
 * @brief OTA Firmware Validator
 *
 * Singleton class providing firmware validation services:
 * - ECDSA P-256 signature verification
 * - SHA-256 streaming hash computation
 * - Header validation
 * - Version comparison
 */
class OTAValidator {
 private:
  static OTAValidator* instance;

  // mbedTLS contexts
  mbedtls_pk_context pkContext;
  mbedtls_sha256_context sha256Context;
  mbedtls_entropy_context entropyContext;
  mbedtls_ctr_drbg_context ctrDrbgContext;

  // State
  bool publicKeyLoaded;
  bool hashInitialized;
  size_t bytesHashed;
  uint8_t computedHash[OTA_HASH_SIZE];

  // Thread safety
  SemaphoreHandle_t validatorMutex;

  // Current firmware version (for anti-rollback)
  uint8_t currentMajor;
  uint8_t currentMinor;
  uint8_t currentPatch;
  uint32_t currentBuildNumber;

  // Private constructor (singleton)
  OTAValidator();
  ~OTAValidator();

  // Internal methods
  bool initializeCrypto();
  void cleanupCrypto();
  int loadPublicKeyFromPEM(const char* pemKey, size_t keyLen);
  int loadPublicKeyFromFile(const char* filePath);

 public:
  /**
   * @brief Get singleton instance
   */
  static OTAValidator* getInstance();

  /**
   * @brief Initialize validator
   * @return true if successful
   */
  bool begin();

  /**
   * @brief Stop and cleanup
   */
  void stop();

  // ============================================
  // PUBLIC KEY MANAGEMENT
  // ============================================

  /**
   * @brief Load public key from PEM string
   * @param pemKey PEM encoded public key
   * @return true if successful
   */
  bool loadPublicKey(const char* pemKey);

  /**
   * @brief Load public key from file
   * @param filePath Path to PEM file
   * @return true if successful
   */
  bool loadPublicKeyFile(const char* filePath);

  /**
   * @brief Check if public key is loaded
   */
  bool isPublicKeyLoaded() const { return publicKeyLoaded; }

  // ============================================
  // SIGNATURE VERIFICATION
  // ============================================

  /**
   * @brief Verify ECDSA signature over data
   * @param data Data that was signed
   * @param dataLen Length of data
   * @param signature ECDSA signature (64 bytes for P-256)
   * @param sigLen Signature length
   * @return true if signature is valid
   */
  bool verifySignature(const uint8_t* data, size_t dataLen,
                       const uint8_t* signature, size_t sigLen);

  /**
   * @brief Verify signature over pre-computed hash
   * @param hash SHA-256 hash (32 bytes)
   * @param signature ECDSA signature
   * @param sigLen Signature length
   * @return true if signature is valid
   */
  bool verifySignatureHash(const uint8_t* hash, const uint8_t* signature,
                           size_t sigLen);

  // ============================================
  // CHECKSUM (SHA-256) COMPUTATION
  // ============================================

  /**
   * @brief Start streaming hash computation
   */
  void hashBegin();

  /**
   * @brief Update hash with more data
   * @param data Data chunk
   * @param len Data length
   */
  void hashUpdate(const uint8_t* data, size_t len);

  /**
   * @brief Finalize hash computation
   * @param hashOut Output buffer (32 bytes)
   */
  void hashFinalize(uint8_t* hashOut);

  /**
   * @brief Compute SHA-256 hash of data in one call
   * @param data Input data
   * @param len Data length
   * @param hashOut Output buffer (32 bytes)
   */
  void computeHash(const uint8_t* data, size_t len, uint8_t* hashOut);

  /**
   * @brief Compare hash with expected value
   * @param hash Computed hash
   * @param expected Expected hash
   * @return true if hashes match
   */
  bool compareHash(const uint8_t* hash, const uint8_t* expected);

  /**
   * @brief Convert hash to hex string
   * @param hash Hash bytes (32 bytes)
   * @param hexOut Output buffer (65 bytes min)
   */
  void hashToHex(const uint8_t* hash, char* hexOut);

  /**
   * @brief Parse hex string to hash bytes
   * @param hex Hex string (64 chars)
   * @param hashOut Output buffer (32 bytes)
   * @return true if successful
   */
  bool hexToHash(const char* hex, uint8_t* hashOut);

  // ============================================
  // FIRMWARE HEADER VALIDATION
  // ============================================

  /**
   * @brief Validate firmware header
   * @param header Pointer to header
   * @param result Output validation result
   * @return true if header is valid
   */
  bool validateHeader(const OTAFirmwareHeader* header,
                      ValidationResult& result);

  /**
   * @brief Validate full package header (header + signature)
   * @param package Pointer to package header
   * @param firmwareData Pointer to firmware data (after header)
   * @param firmwareSize Size of firmware data
   * @param result Output validation result
   * @return true if package is valid
   */
  bool validatePackage(const OTAPackageHeader* package,
                       const uint8_t* firmwareData, size_t firmwareSize,
                       ValidationResult& result);

  // ============================================
  // VERSION CONTROL (ANTI-ROLLBACK)
  // ============================================

  /**
   * @brief Set current firmware version
   * @param major Major version
   * @param minor Minor version
   * @param patch Patch version
   * @param buildNumber Build number
   */
  void setCurrentVersion(uint8_t major, uint8_t minor, uint8_t patch,
                         uint32_t buildNumber);

  /**
   * @brief Check if version is newer (or equal)
   * @param major New major version
   * @param minor New minor version
   * @param patch New patch version
   * @param buildNumber New build number
   * @return true if new version is >= current
   */
  bool isVersionNewer(uint8_t major, uint8_t minor, uint8_t patch,
                      uint32_t buildNumber);

  /**
   * @brief Check if downgrade is allowed
   * @param major New major version
   * @param minor New minor version
   * @param patch New patch version
   * @return true if update is allowed
   */
  bool isUpdateAllowed(uint8_t major, uint8_t minor, uint8_t patch,
                       uint32_t buildNumber);

  // ============================================
  // FULL VALIDATION
  // ============================================

  /**
   * @brief Perform full firmware validation
   * @param firmwareData Complete firmware data (with header)
   * @param totalSize Total size including header
   * @param result Output validation result
   * @return true if firmware is valid
   */
  bool validateFirmware(const uint8_t* firmwareData, size_t totalSize,
                        ValidationResult& result);

  /**
   * @brief Validate firmware from streaming data
   * Call hashBegin(), hashUpdate() during download, then this to finalize
   * @param expectedHash Expected SHA-256 hash (hex string or bytes)
   * @param signature ECDSA signature
   * @param result Output validation result
   * @return true if valid
   */
  bool validateStreaming(const char* expectedHashHex, const uint8_t* signature,
                         size_t sigLen, ValidationResult& result);

  // Singleton - no copy
  OTAValidator(const OTAValidator&) = delete;
  OTAValidator& operator=(const OTAValidator&) = delete;
};

#endif  // OTA_VALIDATOR_H
