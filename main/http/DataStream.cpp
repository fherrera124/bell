// #include "bell/http/DataStream.h"
// #include <cassert>
// #include "bell/Logger.h"
// #include "bell/http/Common.h"

// using namespace bell::http;

// bell::Result<> DataStream::open(http::Method method, const std::string& url,
//                                 const Headers& headers, const std::byte* body,
//                                 size_t length, int timeoutMs, bool secure) {
//   connection = std::make_unique<http::Connection>();

//   // Ensure the chunk size is valid
//   lastReadChunk.resize(chunkSize);
//   bytesInLastReadChunk = 0;
//   chunkStartPosition = 0;

//   auto res = connection->connect(url, timeoutMs, secure);
//   if (!res) {
//     return res;
//   }
//   this->httpHeaders = headers;
//   this->httpMethod = method;

//   Headers allHeaders = httpHeaders;

//   // Add range header to request
//   allHeaders.push_back(http::rangeHeader(0, chunkSize - 1));

//   auto writer = connection->sendRequest(method, headers, length);
//   if (!writer) {
//     return tl::make_unexpected(writer.error());
//   }

//   if (body && (length > 0)) {
//     auto writeRes =
//         writer->writeBodyRaw(reinterpret_cast<const char*>(body), length);
//     if (!writeRes) {
//       return tl::make_unexpected(writeRes.error());
//     }
//   }

//   auto reader = connection->getResponse();
//   if (!reader) {
//     return tl::make_unexpected(reader.error());
//   }

//   if (reader->getHeader("Content-Length").empty()) {
//     totalSize = std::nullopt;
//   } else {
//     totalSize = reader->getContentLength();
//   }

//   auto rangeHeader = reader->getHeader("Content-Range");
//   if (rangeHeader.empty()) {
//     isSeekableFlag = false;
//   } else {
//     // Parse the Content-Range header
//     // Example: "bytes 0-1023/2048"
//     auto totalSizeStr = rangeHeader.substr(rangeHeader.find('/') + 1);
//     if (totalSizeStr != rangeHeader.end()) {
//       try {
//         totalSize = std::stoll(totalSizeStr.data());
//         isSeekableFlag = true;
//       } catch (const std::invalid_argument& e) {
//         BELL_LOG(error, LOG_TAG, "Failed to parse Content-Range header: {}",
//                  e.what());
//         return bell::make_unexpected_errc<>(std::errc::bad_message);
//       }
//     } else {
//       isSeekableFlag = false;
//     }
//   }

//   size_t toRead = std::min(chunkSize, reader->getContentLength());
//   auto* stream = connection->getResponse()->getStream();
//   stream->read(reinterpret_cast<char*>(lastReadChunk.data()), toRead);
//   if (stream->fail() && !stream->eof()) {
//     return bell::make_unexpected_errc<>(std::errc::io_error);
//   }

//   bytesInLastReadChunk = stream->gcount();

//   return {};
// }

// bool DataStream::isOpen() const {
//   return connection != nullptr;
// }

// bool DataStream::isSeekable() const {
//   return isSeekableFlag;
// }

// bool DataStream::isInfinite() const {
//   return !totalSize.has_value();
// }

// std::optional<size_t> DataStream::size() const {
//   return totalSize;
// }

// size_t DataStream::position() const {
//   return currentPosition;
// }

// bell::Result<> DataStream::seek(size_t offset) {
//   if (!isSeekable()) {
//     return bell::make_unexpected_errc<>(std::errc::invalid_argument);
//   }

//   if (offset >= totalSize.value_or(0)) {
//     return bell::make_unexpected_errc<>(std::errc::invalid_seek);
//   }

//   currentPosition = offset;
//   bytesInLastReadChunk = 0;
//   chunkStartPosition = 0;

//   return {};
// }

// bell::Result<size_t> DataStream::read(std::byte* outputBuffer,
//                                       size_t outputBufferLen) {
//   if (!isOpen()) {
//     return bell::make_unexpected_errc<size_t>(std::errc::bad_file_descriptor);
//   }

//   size_t totalCopied = 0;
//   size_t toRead = outputBufferLen;

//   while (toRead > 0) {
//     // Copy remaining bytes from current chunk
//     size_t availableInChunk = 0;
//     if (bytesInLastReadChunk > chunkStartPosition) {
//       availableInChunk = bytesInLastReadChunk - chunkStartPosition;
//     }

//     if (availableInChunk > 0) {
//       size_t toCopy = std::min(toRead, availableInChunk);
//       std::copy(lastReadChunk.data() + chunkStartPosition,
//                 lastReadChunk.data() + chunkStartPosition + toCopy,
//                 outputBuffer + totalCopied);
//       chunkStartPosition += toCopy;
//       currentPosition += toCopy;
//       totalCopied += toCopy;
//       toRead -= toCopy;

//       // If we've satisfied the request, return
//       if (toRead == 0) {
//         break;
//       }
//     }

//     // No more data in current chunk, request next
//     if (isSeekable()) {
//       auto res = requestNextRange();
//       if (!res) {
//         return bell::make_unexpected_errc<size_t>(std::errc::io_error);
//       }
//     } else {
//       // For non-seekable streams, just read next chunk from the same connection
//       auto* stream = connection->getResponse()->getStream();
//       stream->read(reinterpret_cast<char*>(lastReadChunk.data()), chunkSize);
//       if (stream->fail() && !stream->eof()) {
//         return bell::make_unexpected_errc<size_t>(std::errc::io_error);
//       }
//       bytesInLastReadChunk = static_cast<size_t>(stream->gcount());
//       chunkStartPosition = 0;

//       if (bytesInLastReadChunk == 0) {
//         // EOF for finite streams
//         break;
//       }
//     }

//     // If after fetching, no bytes were read, break (EOF for finite streams)
//     if (bytesInLastReadChunk == 0) {
//       break;
//     }

//     // Reset chunk position for next read iteration
//     chunkStartPosition = 0;
//   }

//   return totalCopied;
// }

// bell::Result<> DataStream::requestNextRange() {
//   if (!connection) {
//     return bell::make_unexpected_errc<>(std::errc::bad_file_descriptor);
//   }

//   Headers allHeaders = httpHeaders;
//   size_t chunkReadSize =
//       std::min(chunkSize, totalSize.value_or(SIZE_MAX) - currentPosition);

//   // Range starts exactly at currentPosition
//   allHeaders.push_back(
//       http::rangeHeader(currentPosition, currentPosition + chunkReadSize - 1));

//   auto writer = connection->sendRequest(this->httpMethod, allHeaders, 0);
//   if (!writer) {
//     return tl::make_unexpected(writer.error());
//   }

//   auto reader = connection->getResponse();
//   if (!reader) {
//     return tl::make_unexpected(reader.error());
//   }

//   size_t toRead = reader->getContentLength();
//   auto* stream = reader->getStream();
//   assert(toRead <= lastReadChunk.size());

//   stream->read(reinterpret_cast<char*>(lastReadChunk.data()), toRead);
//   if (stream->fail() && !stream->eof()) {
//     return bell::make_unexpected_errc<>(std::errc::io_error);
//   }

//   bytesInLastReadChunk = static_cast<size_t>(stream->gcount());
//   chunkStartPosition = 0;

//   return {};
// }
