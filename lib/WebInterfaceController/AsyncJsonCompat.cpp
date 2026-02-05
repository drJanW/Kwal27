/**
 * @file AsyncJsonCompat.cpp
 * @brief JSON handling compatibility layer
 * @version 260202A
 * @date 2026-02-02
 */
#include <AsyncJson.h>
#include <cstring>
#include <cstdlib>

#if ASYNC_JSON_SUPPORT == 1

#if ARDUINOJSON_VERSION_MAJOR == 6
AsyncCallbackJsonWebHandler::AsyncCallbackJsonWebHandler(const String &uri, ArJsonRequestHandlerFunction onRequest, size_t maxJsonBufferSize)
    : _uri(uri), _method(HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH), _onRequest(onRequest), maxJsonBufferSize(maxJsonBufferSize), _maxContentLength(16384) {}
#else
AsyncCallbackJsonWebHandler::AsyncCallbackJsonWebHandler(const String &uri, ArJsonRequestHandlerFunction onRequest)
    : _uri(uri), _method(HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH), _onRequest(onRequest), _maxContentLength(16384) {}
#endif

bool AsyncCallbackJsonWebHandler::canHandle(AsyncWebServerRequest *request) const {
  if (!_onRequest || !request->isHTTP() || !(_method & request->method())) {
    return false;
  }

  if (_uri.length() && (_uri != request->url() && !request->url().startsWith(_uri + "/"))) {
    return false;
  }

  if (request->method() != HTTP_GET && !request->contentType().equalsIgnoreCase(asyncsrv::T_application_json)) {
    return false;
  }

  return true;
}

void AsyncCallbackJsonWebHandler::handleRequest(AsyncWebServerRequest *request) {
  if (!_onRequest) {
    request->send(500);
    return;
  }

  if (request->method() == HTTP_GET) {
    JsonVariant json;
    _onRequest(request, json);
    return;
  }

  if (request->contentLength() > _maxContentLength) {
    request->send(413);
    return;
  }

  if (request->_tempObject == NULL) {
    request->send(400);
    return;
  }

#if ARDUINOJSON_VERSION_MAJOR == 5
  DynamicJsonBuffer jsonBuffer;
  JsonVariant json = jsonBuffer.parse((const char *)request->_tempObject);
  if (json.success()) {
#elif ARDUINOJSON_VERSION_MAJOR == 6
  DynamicJsonDocument jsonBuffer(this->maxJsonBufferSize);
  DeserializationError error = deserializeJson(jsonBuffer, (const char *)request->_tempObject);
  if (!error) {
    JsonVariant json = jsonBuffer.as<JsonVariant>();
#else
  JsonDocument jsonBuffer;
  DeserializationError error = deserializeJson(jsonBuffer, (const char *)request->_tempObject);
  if (!error) {
    JsonVariant json = jsonBuffer.as<JsonVariant>();
#endif
    _onRequest(request, json);
    return;
  }

  request->send(400);
}

void AsyncCallbackJsonWebHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (!_onRequest) {
    return;
  }

  if (total > _maxContentLength) {
    return;
  }

  if (index == 0 && request->_tempObject == NULL) {
    request->_tempObject = calloc(total + 1, sizeof(uint8_t));
    if (request->_tempObject == NULL) {
      return;
    }
  }

  if (request->_tempObject != NULL) {
    uint8_t *buffer = static_cast<uint8_t *>(request->_tempObject);
    memcpy(buffer + index, data, len);
  }
}

#endif
