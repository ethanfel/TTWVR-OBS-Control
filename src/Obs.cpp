#include "Control.h"
#include "Preview.h"
#include <windows.h>
#include <winhttp.h>
#include "Trace.h"
#include <bcrypt.h>
#include <wincrypt.h>
#include <nlohmann/json.hpp>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace studio {
namespace {
using Json = nlohmann::json;
struct InternetClose { void operator()(void* h) const { WinHttpCloseHandle(h); } };
using Handle = std::unique_ptr<void, InternetClose>;
struct Fault { Link reason; };
constexpr DWORD freshMs = 2500;
SRWLOCK lock = SRWLOCK_INIT;
struct Guard {
    Guard() { AcquireSRWLockExclusive(&lock); }
    ~Guard() { ReleaseSRWLockExclusive(&lock); }
};
Status state;
ULONGLONG lastVisit, sampled, requested;
const char* queued = nullptr;
HANDLE worker, wake, previewWorker, previewWake;
Picture lastPicture;
ULONGLONG pictureTime;
bool quitting = false;

std::wstring env(const wchar_t* name) {
    DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (!n) return {};
    std::wstring v(n, L'\0');
    DWORD count = GetEnvironmentVariableW(name, v.data(), n);
    if (!count || count >= n) return {};
    v.resize(count);
    return v;
}
struct Settings { int port = 4455; std::string password; };
Settings settings() {
    auto path = std::filesystem::path(env(L"APPDATA")) / L"obs-studio/plugin_config/obs-websocket/config.json";
    // Proton users may point this at native OBS's config through Wine's Z: drive.
    auto ini = std::filesystem::absolute(L"Data/NVSE/Plugins/TTWVROBS.ini").wstring();
    wchar_t custom[1024]{};
    GetPrivateProfileStringW(L"OBS", L"ConfigFile", L"", custom, 1024, ini.c_str());
    if (*custom) path = custom;
    Settings out;
    std::ifstream f(path, std::ios::binary);
    if (f) {
        Json j = Json::parse(f, nullptr, false);
        if (!j.is_object()) throw Fault{Link::Settings};
        out.port = j.value("server_port", 4455);
        out.password = j.value("server_password", std::string{});
    } else if (*custom) throw Fault{Link::Settings};
    if (out.port < 1 || out.port > 65535) throw Fault{Link::Settings};
    return out;
}
std::string hash64(const std::string& bytes) {
    BCRYPT_ALG_HANDLE alg{};
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw Fault{Link::Authentication};
    std::array<BYTE,32> digest{};
    BCRYPT_HASH_HANDLE hash{};
    auto ok = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
    if (ok >= 0) ok = BCryptHashData(hash, (PUCHAR)bytes.data(), static_cast<ULONG>(bytes.size()), 0);
    if (ok >= 0) ok = BCryptFinishHash(hash, digest.data(), digest.size(), 0);
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    DWORD n = 48;
    char encoded[48]{};
    if (ok < 0 || !CryptBinaryToStringA(digest.data(), digest.size(),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded, &n))
        throw Fault{Link::Authentication};
    return encoded;
}
struct Socket {
    Handle session, connection, websocket;
    unsigned seq = 0;
    ~Socket() {
        if (websocket) {
            DWORD timeout = 350;
            WinHttpSetOption(websocket.get(), WINHTTP_OPTION_WEB_SOCKET_CLOSE_TIMEOUT, &timeout, sizeof(timeout));
            WinHttpWebSocketClose(websocket.get(), 1000, nullptr, 0);
        }
    }
    void send(const Json& j) {
        auto data = j.dump();
        if (WinHttpWebSocketSend(websocket.get(), WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                 data.data(), static_cast<DWORD>(data.size())))
            throw Fault{Link::Failed};
    }
    Json receive(std::size_t limit=65536) {
        std::string full;
        for (;;) {
            char bytes[4096];
            DWORD n = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE kind;
            if (WinHttpWebSocketReceive(websocket.get(), bytes, sizeof(bytes), &n, &kind))
                throw Fault{Link::Failed};
            if (kind == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
                USHORT code = 0; DWORD used = 0; char reason[124]{};
                WinHttpWebSocketQueryCloseStatus(websocket.get(), &code, reason, sizeof(reason), &used);
                throw Fault{code == 4009 ? Link::Authentication : Link::Failed};
            }
            if (kind != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE &&
                kind != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE) throw Fault{Link::Failed};
            full.append(bytes, n);
            if (full.size() > limit) throw Fault{Link::Failed};
            if (kind == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) return Json::parse(full);
        }
    }
    void open() {
        auto config = settings();
        session.reset(WinHttpOpen(L"TTWVR OBS Control/0.2.1", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                  nullptr, nullptr, 0));
        if (!session || !WinHttpSetTimeouts(session.get(), 1500, 1500, 2000, 2500))
            throw Fault{Link::Offline};
        connection.reset(WinHttpConnect(session.get(), L"127.0.0.1", config.port, 0));
        Handle req(WinHttpOpenRequest(connection.get(), L"GET", L"/", nullptr, nullptr, nullptr, 0));
        DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!req || !WinHttpSetOption(req.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)) ||
            !WinHttpSetOption(req.get(), WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0) ||
            !WinHttpSendRequest(req.get(), L"Sec-WebSocket-Protocol: obswebsocket.json\r\n", DWORD(-1),
                                nullptr, 0, 0, 0) || !WinHttpReceiveResponse(req.get(), nullptr))
            throw Fault{Link::Offline};
        DWORD status = 0, len = sizeof(status);
        if (!WinHttpQueryHeaders(req.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 nullptr, &status, &len, nullptr) || status != 101) throw Fault{Link::Offline};
        websocket.reset(WinHttpWebSocketCompleteUpgrade(req.get(), 0));
        if (!websocket) throw Fault{Link::Failed};
        auto hello = receive();
        if (hello.at("op") != 0 || hello.at("d").at("rpcVersion").get<int>() < 1) throw Fault{Link::Failed};
        Json ident{{"rpcVersion", 1}, {"eventSubscriptions", 0}};
        if (hello["d"].contains("authentication")) {
            auto& auth = hello["d"]["authentication"];
            ident["authentication"] = hash64(hash64(config.password + auth.at("salt").get<std::string>()) +
                                             auth.at("challenge").get<std::string>());
        }
        send({{"op", 1}, {"d", ident}});
        if (receive().at("op") != 2) throw Fault{Link::Authentication};
    }
    Json request(const char* type, bool& ok, const Json& params=nullptr, std::size_t limit=65536) {
        auto id = std::to_string(++seq);
        Json dreq={{"requestType",type},{"requestId",id}};
        if (!params.is_null()) dreq["requestData"]=params;
        send({{"op",6},{"d",std::move(dreq)}});
        auto res = receive(limit);
        auto& d = res.at("d");
        if (res.at("op") != 7 || d.at("requestId") != id || d.at("requestType") != type)
            throw Fault{Link::Failed};
        ok = d.at("requestStatus").at("result").get<bool>();
        return d.value("responseData", Json::object());
    }
    Status poll() {
        bool recOk, liveOk;
        auto rec = request("GetRecordStatus", recOk);
        auto live = request("GetStreamStatus", liveOk);
        if (!recOk || !liveOk) throw Fault{Link::Failed};
        Status s;
        s.link = Link::Ready;
        s.recording = rec.at("outputActive");
        s.paused = rec.value("outputPaused", false);
        s.live = live.at("outputActive");
        s.reconnecting = live.value("outputReconnecting", false);
        return s;
    }
};
DWORD WINAPI runPreview(void*) {
    const char* previous=nullptr;
    std::unique_ptr<Socket> socket;
    for (;;) {
        bool visible, ready;
        {
            Guard g;
            if (quitting) break;
            visible=lastVisit && GetTickCount64()-lastVisit<650;
            ready=state.link==Link::Ready;
        }
        Picture picture;
        const char* result="Preview idle";
        if (visible && ready) try {
            if (!socket) { socket=std::make_unique<Socket>(); socket->open(); }
            bool ok=false;
            result="Preview Program scene unavailable";
            auto scene=socket->request("GetCurrentProgramScene",ok);
            if (ok && scene.contains("currentProgramSceneName")) {
                Json params={{"sourceName",scene.at("currentProgramSceneName")},{"imageFormat","bmp"},
                             {"imageWidth",320},{"imageHeight",180}};
                result="Preview screenshot unavailable";
                auto shot=socket->request("GetSourceScreenshot",ok,params,384*1024);
                if (ok && shot.contains("imageData")) {
                    picture=decodePreview(shot.at("imageData").get_ref<const std::string&>());
                    result=picture ? "Preview Program frame received" : "Preview bitmap invalid";
                }
            }
        } catch (const Fault&) { socket.reset(); result="Preview socket error"; }
          catch (const std::exception&) { socket.reset(); result="Preview response invalid"; }
        else socket.reset();
        if (result!=previous) { obsTrace(result); previous=result; }
        const bool gotPicture=bool(picture);
        {
            Guard g;
            if (quitting) break;
            if (GetTickCount64()-lastVisit<650 && state.link==Link::Ready) {
                if (picture) { lastPicture=std::move(picture); pictureTime=GetTickCount64(); }
            } else lastPicture.reset();
        }
        WaitForSingleObject(previewWake,!visible ? INFINITE : ready && !gotPicture ? 1600 : 450);
    }
    socket.reset();
    return 0;
}

DWORD WINAPI run(void*) {
    std::unique_ptr<Socket> socket;
    ULONGLONG errorUntil = 0, settleUntil = 0;
    const char* settling = nullptr;
    for (;;) {
        const char* action = nullptr;
        bool visible;
        {
            Guard g;
            if (quitting) break;
            auto now = GetTickCount64();
            visible = lastVisit && now-lastVisit < freshMs;
            if (visible && socket && now-requested < freshMs) action = queued;
            queued = nullptr;
        }
        if (!visible) {
            socket.reset(); settling = nullptr; errorUntil = 0;
            Guard g; state = {}; sampled = 0;
        } else try {
            if (!socket) { socket = std::make_unique<Socket>(); socket->open(); }
            if (action) {
                bool ok;
                socket->request(action, ok);
                errorUntil = ok ? 0 : GetTickCount64()+4000;
                settling = ok ? action : nullptr;
                settleUntil = GetTickCount64()+6000;
            }
            auto next = socket->poll();
            auto now = GetTickCount64();
            if (settling) {
                bool active = std::string_view(settling).find("Record") != std::string_view::npos
                            ? next.recording : next.live;
                if (active == (std::string_view(settling).rfind("Start",0)==0)) settling = nullptr;
                else if (now >= settleUntil) { settling = nullptr; errorUntil = now+4000; }
            }
            Guard g;
            next.rejected = now < errorUntil;
            next.busy = queued || settling;
            state = next; sampled = now;
        } catch (const Fault& e) {
            socket.reset(); settling = nullptr; Guard g; state = {e.reason}; sampled = 0; queued = nullptr;
        } catch (const std::exception&) {
            socket.reset(); settling = nullptr; Guard g; state = {Link::Failed}; sampled = 0; queued = nullptr;
        }
        WaitForSingleObject(wake, !visible ? INFINITE : socket ? 450 : 1300);
    }
    return 0;
}
}
void visit() {
    Guard g;
    if (quitting) return;
    auto now = GetTickCount64();
    bool restart = !lastVisit || now-lastVisit >= freshMs;
    bool reopenPreview=!lastVisit || now-lastVisit >= 650;
    lastVisit = now;
    if (!previewWorker) {
        if (!previewWake) previewWake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if (previewWake) previewWorker=CreateThread(nullptr,0,runPreview,nullptr,0,nullptr);
    }
    if (reopenPreview && previewWake) { lastPicture.reset(); SetEvent(previewWake); }
    if (!worker) {
        if (!wake) wake = CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if (wake) worker = CreateThread(nullptr,0,run,nullptr,0,nullptr);
    }
    if (restart && wake) SetEvent(wake);
}
Picture preview() {
    Guard g;
    return state.link==Link::Ready && GetTickCount64()-pictureTime<1800 ? lastPicture : Picture{};
}
Status read() {
    Guard g;
    if (state.link == Link::Ready && GetTickCount64()-sampled >= freshMs)
        return {Link::Waiting};
    return state;
}
void press(Action a, bool expectedActive) {
    Guard g;
    auto now = GetTickCount64();
    bool active = a == Action::Record ? state.recording : state.live;
    if (quitting || state.link != Link::Ready || state.busy || active != expectedActive ||
        now-sampled >= freshMs || now-lastVisit >= freshMs) return;
    queued = a == Action::Record ? (active ? "StopRecord" : "StartRecord")
                                 : (active ? "StopStream" : "StartStream");
    state.busy = true; requested = now; SetEvent(wake);
}
void stop() {
    Guard g; quitting = true; queued = nullptr; if (wake) SetEvent(wake); if (previewWake) SetEvent(previewWake);
}
}
