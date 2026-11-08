// SPDX-License-Identifier: GPL-2.0-or-later
// Native Common Item Dialog in a short-lived child process of this executable.
// Shell extensions must not run under the emulator's fail-fast exception handler.
// SDL keeps pumping messages; selection callbacks run on the UI thread.
#include "clarity.h"
#include "ui/gui_util.h"
#include "oslib/storage.h"
#include <windows.h>
#include <shobjidl.h>
#include <nowide/convert.hpp>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

HWND getNativeHwnd();

namespace clarity {
namespace {
struct Result { bool cancelled = true; std::string path, error; };
struct Request {
    HWND owner;
    std::string title, folder, extension;
    bool file;
    StringCallback callback;
};
std::future<Result> pending;
Request active{};
std::string lastFolder;
bool retry = false;
std::mutex helperMutex;
HANDLE helperJob = nullptr; // borrowed while launchPicker owns the job
bool shuttingDown = false;
template<class T> struct Release { void operator()(T* p) const { if (p) p->Release(); } };

Result choose(const Request& request) {
    Result result;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) { result.error = "Windows could not initialize the file dialog."; return result; }
    struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
    IFileOpenDialog* raw = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&raw));
    std::unique_ptr<IFileOpenDialog, Release<IFileOpenDialog>> dialog(raw);
    if (SUCCEEDED(hr)) {
        DWORD options = 0;
        hr = dialog->GetOptions(&options);
        if (SUCCEEDED(hr)) hr = dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST
            | FOS_NOCHANGEDIR | (request.file ? FOS_FILEMUSTEXIST : FOS_PICKFOLDERS));
        dialog->SetTitle(nowide::widen(request.title).c_str());
        if (!request.folder.empty()) {
            IShellItem* folder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(nowide::widen(request.folder).c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }
        std::wstring pattern = request.extension.empty() ? L"*.*" : L"*." + nowide::widen(request.extension);
        if (request.file) {
            COMDLG_FILTERSPEC filter{pattern.c_str(), pattern.c_str()};
            if (SUCCEEDED(hr)) hr = dialog->SetFileTypes(1, &filter);
        }
        // Do not attach a shell dialog's input queue to the DXGI window across
        // processes. The UI thread disables/re-enables the emulator window.
        if (SUCCEEDED(hr)) hr = dialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* item = nullptr;
            hr = dialog->GetResult(&item);
            if (SUCCEEDED(hr)) {
                PWSTR path = nullptr;
                hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
                if (SUCCEEDED(hr)) { result.path = nowide::narrow(path); result.cancelled = false; CoTaskMemFree(path); }
                item->Release();
            }
        }
    }
    if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        wchar_t* message = nullptr;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, hr, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
        result.error = message ? nowide::narrow(message) : "Windows could not open the file dialog.";
        if (message) LocalFree(message);
    }
    return result;
}

struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    void close() { if (value) CloseHandle(value); value = nullptr; }
};
// Windows CRT command-line quoting, including trailing slashes and quotes.
std::wstring quote(const std::wstring& value) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') { ++slashes; continue; }
        result.append(slashes * (ch == L'"' ? 2 : 1), L'\\');
        slashes = 0;
        if (ch == L'"') result += L'\\';
        result += ch;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
Result launchPicker(const Request& request) {
    Result result;
    result.error = "Windows could not open the file dialog.";
    Handle job{CreateJobObjectW(nullptr, nullptr)};
    if (!job.value) return result;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) return result;
    Handle reader, writer;
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    if (!CreatePipe(&reader.value, &writer.value, &security, 0)) return result;
    if (!SetHandleInformation(reader.value, HANDLE_FLAG_INHERIT, 0)) return result;
    // Restrict inheritance to the result pipe. No emulator files or handles
    // are exposed to the child, and no temporary result files are needed.
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<unsigned char> attributes(bytes);
    auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &bytes)) return result;
    struct Attributes { LPPROC_THREAD_ATTRIBUTE_LIST list; ~Attributes() { DeleteProcThreadAttributeList(list); } } cleanup{list};
    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        &writer.value, sizeof(HANDLE), nullptr, nullptr)) return result;
    std::wstring exe(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, exe.data(), (DWORD)exe.size());
    if (!length || length == exe.size()) return result;
    exe.resize(length);
    std::wstring command = quote(exe) + L" --clarity-native-picker "
        + std::to_wstring(reinterpret_cast<uintptr_t>(request.owner)) + L" "
        + std::to_wstring(reinterpret_cast<uintptr_t>(writer.value))
        + (request.file ? L" 1 " : L" 0 ") + quote(nowide::widen(request.title)) + L" "
        + quote(nowide::widen(request.folder)) + L" " + quote(nowide::widen(request.extension));
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = list;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_SUSPENDED,
        nullptr, nullptr, &startup.StartupInfo, &process)) return result;
    Handle child{process.hProcess}, thread{process.hThread};
    if (!AssignProcessToJobObject(job.value, child.value)) {
        TerminateProcess(child.value, ERROR_CANCELLED);
        return result;
    }
    {
        std::lock_guard<std::mutex> lock(helperMutex);
        if (shuttingDown) return result; // job destruction ends the suspended helper
        helperJob = job.value;
    }
    struct Registration {
        ~Registration() { std::lock_guard<std::mutex> lock(helperMutex); helperJob = nullptr; }
    } registration;
    AllowSetForegroundWindow(process.dwProcessId);
    if (ResumeThread(thread.value) == DWORD(-1)) return result;
    writer.close();
    std::string response;
    char buffer[4096]; DWORD count = 0;
    while (ReadFile(reader.value, buffer, sizeof(buffer), &count, nullptr) && count) {
        if (response.size() + count <= 1024 * 1024) response.append(buffer, count);
    }
    WaitForSingleObject(child.value, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(child.value, &exitCode);
    if (exitCode || response.empty()) return result;
    result.error.clear();
    if (response[0] == 'S') { result.cancelled = false; result.path = response.substr(1); }
    else if (response[0] == 'E') result.error = response.substr(1);
    else if (response != "C") result.error = "Windows returned an invalid file dialog result.";
    return result;
}
}
// Called before Flycast initializes its engine, renderer, config or exception
// handler. The helper only hosts the Windows dialog and writes its UTF-8 result.
bool windowsPickerMain(int argc, char* argv[], int& exitCode) {
    if (argc < 2 || std::string(argv[1]) != "--clarity-native-picker") return false;
    exitCode = 1;
    if (argc != 8) return true;
    try {
        Request request{};
        request.owner = reinterpret_cast<HWND>(std::stoull(argv[2]));
        Handle pipe{reinterpret_cast<HANDLE>(std::stoull(argv[3]))};
        DWORD flags;
        if (!GetHandleInformation(pipe.value, &flags) || GetFileType(pipe.value) != FILE_TYPE_PIPE) return true;
        if (!IsWindow(request.owner)) return true;
        request.file = std::string(argv[4]) == "1";
        request.title = argv[5]; request.folder = argv[6]; request.extension = argv[7];
        Result result = choose(request);
        std::string response = !result.error.empty() ? "E" + result.error
            : result.cancelled ? "C" : "S" + result.path;
        DWORD written = 0;
        if (WriteFile(pipe.value, response.data(), (DWORD)response.size(), &written, nullptr)
            && written == response.size()) exitCode = 0;
    } catch (...) { }
    return true;
}
bool windowsPickerActive() { return pending.valid() || retry; }
void shutdownWindowsPicker() {
    {
        std::lock_guard<std::mutex> lock(helperMutex);
        shuttingDown = true;
        if (helperJob) TerminateJobObject(helperJob, ERROR_CANCELLED);
    }
    if (pending.valid()) { try { pending.get(); } catch (...) { } }
    retry = false;
}
void pollWindowsPicker() {
    // A rejected selection keeps the picker workflow open, just like the
    // shared picker. Let the caller's validation error be dismissed first.
    if (retry && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        retry = false;
        active.folder = lastFolder;
        EnableWindow(active.owner, FALSE);
        pending = std::async(std::launch::async, launchPicker, active);
    }
    if (!pending.valid() || pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    Result result;
    try { result = pending.get(); }
    catch (const std::exception& e) { result.error = e.what(); }
    EnableWindow(active.owner, TRUE);
    SetForegroundWindow(active.owner);
    if (!result.path.empty()) lastFolder = active.file ? hostfs::storage().getParentPath(result.path) : result.path;
    // Cancellation never mutates the caller's selected path.
    if (active.callback) {
        bool accepted = active.callback(result.cancelled, result.path);
        retry = !result.cancelled && !accepted;
    }
    if (!result.error.empty()) gui_error(tr(result.error.c_str()));
}
void windowsPicker(const char* prompt, StringCallback callback, bool file, const std::string& extension) {
    if (!ImGui::IsPopupOpen(prompt)) return;
    // Consume the existing shared popup request without drawing a custom picker.
    ImGui::ClosePopupToLevel(GImGui->BeginPopupStack.Size, false);
    if (windowsPickerActive()) return;
    std::string title = prompt;
    title.resize(title.find("##") == std::string::npos ? title.size() : title.find("##"));
    active = {getNativeHwnd(), tr(title.c_str()), lastFolder.empty() ? hostfs::storage().getDefaultDirectory() : lastFolder,
        extension, file, callback};
    EnableWindow(active.owner, FALSE);
    pending = std::async(std::launch::async, launchPicker, active);
}
}
