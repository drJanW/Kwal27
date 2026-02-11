/**
 * @file WebDirector.h
 * @brief Web request routing and response coordination
 * @version 260202A
 $12026-02-05
 */
#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <memory>
#include <vector>

class WebDirector {
public:
    static WebDirector &instance();

    void plan();

    bool submitSdList(AsyncWebServerRequest *request, const String &path);
    bool submitSdDelete(AsyncWebServerRequest *request, const String &path);
    bool submitLightPatternsDump(AsyncWebServerRequest *request);
    bool submitLightColorsDump(AsyncWebServerRequest *request);
    bool submitLightPatternUpdate(AsyncWebServerRequest *request, const String &payload);
    bool submitLightPatternDelete(AsyncWebServerRequest *request, const String &payload);
    bool submitLightColorUpdate(AsyncWebServerRequest *request, const String &payload);
    bool submitLightColorDelete(AsyncWebServerRequest *request, const String &payload);

    void cancelRequest(AsyncWebServerRequest *request);

private:
    WebDirector() = default;
    WebDirector(const WebDirector &) = delete;
    WebDirector &operator=(const WebDirector &) = delete;

    static void cb_webDirectorTick();
    void runJobs();

    struct Job;
    Job *acquireIdleSlot();
    void startJob(Job &job);
    void runSdListJob(Job &job);
    void runSdDeleteJob(Job &job);
    void runLightPatternsGetJob(Job &job);
    void runLightColorsGetJob(Job &job);
    void runLightPatternUpdateJob(Job &job);
    void runLightPatternDeleteJob(Job &job);
    void runLightColorUpdateJob(Job &job);
    void runLightColorDeleteJob(Job &job);
    void finalizeJob(Job &job);
    void failJob(Job &job, int statusCode, const String &message);
    void releaseJob(Job &job);
    Job *findJobByRequest(AsyncWebServerRequest *request);
    static bool tryAcquireBusyFlag(Job &job);
    static void releaseBusyFlag(Job &job);

    static constexpr size_t kMaxJobs = 4U;
    static constexpr size_t kSdEntriesPerSlice = 4U;
    static constexpr size_t kSdDeleteStepsPerSlice = 4U;

    struct Job {
        enum class Type {
            SdList,
            SdDelete,
            LightPatternsDump,
            LightColorsDump,
            LightPatternUpdate,
            LightPatternDelete,
            LightColorUpdate,
            LightColorDelete
        } type = Type::SdList;
        enum class State { Idle, Pending, Running, Finishing, Failed } state = State::Idle;

        AsyncWebServerRequest *request = nullptr;
        String path;
        String parentPath;
    String payloadBuffer;
    String entriesBuffer;
    String jsonPayload;
    bool useHeader = false;
    String headerName;
    String headerValue;
        size_t entryCount = 0;
    bool truncated = false;
    bool sdBusyClaimed = false;
        int statusCode = 200;
        String errorMessage;

        // SD iteration state
        File dirHandle;
        bool dirOpen = false;
        bool firstEntry = true;
        bool headerPrepared = false;

        struct SdDeleteEntry {
            String path;
            bool postRemoval = false;
        };
        std::vector<SdDeleteEntry> sdDeleteStack;

        void reset();
    } jobs_[kMaxJobs];

    bool timerStarted_ = false;
};
