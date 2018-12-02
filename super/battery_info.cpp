#include "battery_info.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

void UnrecoverableErrorImplementation(std::ostream& stream) {}

template <typename FirstArg, typename ...Args>
void UnrecoverableErrorImplementation(std::ostream& stream,
                                      FirstArg&& first_arg, Args&& ...args) {
  stream << std::forward<FirstArg>(first_arg);
  UnrecoverableErrorImplementation(stream, std::forward<Args>(args)...);
}

template <typename ...Args>
void PrintUnrecoverableErrorAndAbort(Args&& ...args) {
  UnrecoverableErrorImplementation(std::cerr, std::forward<Args>(args)...);
  std::cerr << std::endl;
  std::abort();
}

#define UnrecoverableError(...) \
    PrintUnrecoverableErrorAndAbort(__LINE__, " ", __VA_ARGS__)

bool GetFastUpdateIntervalAndSetToFalse();
bool GetError(std::string& error);
void SetError(const std::string& new_error);
void SetFastUpdateIntervalToTrue();
void Update();
void UpdateLoop();

struct SingleBatteryInfoInternal {
  std::string id;
  std::string name;
  double charge;
  int energy_now;
  int energy_full;
  int power_now;
  BatteryStatus status;
};

struct BatteryInfoInternal {
  std::string error;
  std::vector<SingleBatteryInfoInternal> sbiis;
  int minutes_left;
  std::vector<SingleBatteryInfo> sbis;
  BatteryInfo bi;
};

std::regex tlp_output_regex(
    // Battery header.
    // \1 - battery id (eg. BAT0)
    // \2 - battery name (eg. Internal)
    R"(\+\+\+ ThinkPad Battery Status:[[:space:]]+([[:alnum:]]+)[[:space:]]+)"
        R"(\(.*[[:space:]]+([[:alnum:]]+)\)\n)"
    // Manufacturer - ignored.
    R"(/sys/class/power_supply/\1/manufacturer[[:space:]]*=.*\n)"
    // Model name - ignored.
    R"(/sys/class/power_supply/\1/model_name[[:space:]]*=.*\n)"
    // Cycle count - ignored.
    R"(/sys/class/power_supply/\1/cycle_count[[:space:]]*=.*\n)"
    // Energy full design - ignored.
    R"(/sys/class/power_supply/\1/energy_full_design[[:space:]]*=.*\n)"
    // Energy full.
    // \3 - energy full (eg. 24610) [mWh]
    R"(/sys/class/power_supply/\1/energy_full[[:space:]]*=)"
        R"([[:space:]]*([[:digit:]]{1,8})[[:space:]]\[mWh\]\n)"
    // Energy now.
    // \4 - energy now (eg. 14790) [mWh]
    R"(/sys/class/power_supply/\1/energy_now[[:space:]]*=)"
        R"([[:space:]]*([[:digit:]]{1,8})[[:space:]]\[mWh\]\n)"
    // Power now.
    // \5 - power now (eg. 4416) [mW]
    R"(/sys/class/power_supply/\1/power_now[[:space:]]*=)"
        R"([[:space:]]*([[:digit:]]{1,8})[[:space:]]\[mW\]\n)"
    // Status.
    // \6 - status (eg. Full/Charging/Discharging)
    R"(/sys/class/power_supply/\1/status[[:space:]]*=)"
        R"([[:space:]]*([[:alnum:]].*)\n)"
    // Empty line.
    R"(\n)"
    // Start threshold - ignored.
    R"(tpacpi-bat.\1.startThreshold[[:space:]]*=.*\n)"
    // Stop threshold - ignored.
    R"(tpacpi-bat.\1.stopThreshold[[:space:]]*=.*\n)"
    // Force discharge - ignored.
    R"(tpacpi-bat.\1.forceDischarge[[:space:]]*=.*\n)",
    std::regex::optimize);

SingleBatteryInfoInternal AnalizeBatteryFromRegexMatch(
    const std::smatch& match) {
  SingleBatteryInfoInternal sbii;
  sbii.id = match[1].str();
  sbii.name = match[2].str();
  sbii.energy_full = std::atoi(match[3].str().c_str());
  sbii.energy_now = std::atoi(match[4].str().c_str());
  sbii.power_now = std::atoi(match[5].str().c_str());
  const std::string& status = match[6].str();
  if (status == "Full") {
    sbii.status = kFull;
  } else if (status == "Charging") {
    sbii.status = kCharging;
  } else if (status == "Discharging") {
    sbii.status = kDischarging;
  } else {
    sbii.status = kUnused;
  }
  if (sbii.energy_full == 0) {
    sbii.charge = 0;
  } else {
    sbii.charge = sbii.energy_now * 100.0 / sbii.energy_full;
  }
  return sbii;
}

SingleBatteryInfo MakeExternalSingleBatteryInfo(
    const SingleBatteryInfoInternal& sbii) {
  SingleBatteryInfo sbi;
  sbi.charge = sbii.charge;
  sbi.status = sbii.status;
  return sbi;
}

void MakeExternalBatteryInfo(BatteryInfoInternal& bii) {
  BatteryInfo& bi = bii.bi;
  if (bii.error.empty()) {
    bi.error = NULL;
  } else {
    bi.error = const_cast<char*>(bii.error.c_str());
  }
  std::sort(bii.sbiis.begin(), bii.sbiis.end(),
            [](const SingleBatteryInfoInternal& sbii_a,
               const SingleBatteryInfoInternal& sbii_b) -> bool {
              return sbii_a.id < sbii_b.id;
            });
  for (const SingleBatteryInfoInternal& sbii : bii.sbiis) {
    bii.sbis.push_back(MakeExternalSingleBatteryInfo(sbii));
  }
  bi.number_of_batteries = static_cast<int>(bii.sbis.size());
  bi.sbis = bii.sbis.data();
  // Time left.
  double power_now = 0;
  double energy_now = 0;
  double energy_full = 0;
  BatteryStatus status = kUnused;
  for (const SingleBatteryInfoInternal& sbii : bii.sbiis) {
    power_now += sbii.power_now;
    energy_now += sbii.energy_now;
    energy_full += sbii.energy_full;
    if (sbii.status == kDischarging) {
      status = kDischarging;
    } else if (sbii.status == kCharging) {
      status = kCharging;
    } else if (sbii.status == kFull) {
      if (status == kUnused) {
        status = kFull;
      }
    }
  }
  if (!(power_now > 0 and 0 <= energy_now and energy_now <= energy_full)) {
    status = kUnused;
    SetFastUpdateIntervalToTrue();
  }
  // Converts hours to minutes.
  energy_now *= 60;
  energy_full *= 60;
  switch (status) {
    case kDischarging: {
      bi.minutes_left = static_cast<int>(energy_now / power_now);
      break;
    }

    case kCharging: {
      bi.minutes_left =
          static_cast<int>((energy_full - energy_now) / power_now);
      break;
    }

    case kFull: {
      bi.minutes_left = 0;
    } break;

    default: {
      bi.minutes_left = -1;
      break;
    }
  }
}

BatteryInfoInternal MakeBatteryInfoInternal(const std::string& tlp_output) {
  BatteryInfoInternal bii;
  if (!GetError(bii.error)) {
    std::sregex_iterator it_begin = std::sregex_iterator(
        tlp_output.begin(), tlp_output.end(), tlp_output_regex);
    std::sregex_iterator it_end = std::sregex_iterator();
    for (auto it = it_begin; it != it_end; ++it) {
      const std::smatch match = *it;
      bii.sbiis.push_back(AnalizeBatteryFromRegexMatch(match));
    }
  }
  MakeExternalBatteryInfo(bii);
  return bii;
}

constexpr int ErrorWaitSeconds = 30;
constexpr int UpdateIntervalSeconds = 20;
constexpr int FastUpdateIntervalSeconds = 2;

std::mutex mutex;
std::condition_variable cv;
std::string error;
bool fast_update_interval = false;
std::set<std::pair<BatteryInfoCallback, void*>> callbacks;

bool GetFastUpdateIntervalAndSetToFalse() {
  std::lock_guard<std::mutex> lock(mutex);
  const bool result = fast_update_interval;
  fast_update_interval = false;
  return result;
}

void SetFastUpdateIntervalToTrue() {
  std::lock_guard<std::mutex> lock(mutex);
  fast_update_interval = true;
}

void SetError(const std::string& new_error) {
  std::lock_guard<std::mutex> lock(mutex);
  error = new_error;
}

bool GetError(std::string& tmp) {
  std::lock_guard<std::mutex> lock(mutex);
  tmp = error;
  return !tmp.empty();
}

void TryInit() {
  std::lock_guard<std::mutex> lock(mutex);
  static bool is_initialized = false;
  if (!is_initialized) {
    is_initialized = true;
    std::thread(UpdateLoop).detach();
  }
}

void WaitForNonEmptyCallbacks() {
  std::unique_lock<std::mutex> lock(mutex);
  while (callbacks.empty()) {
    cv.wait(lock);
  }
}

void UpdateLoop() {
  bool is_pipe_created = false;
  int upower_to_me_descriptor;
  constexpr int BufferSize = 128;
  char buffer[BufferSize];
  while (true) {
    WaitForNonEmptyCallbacks();
    if (!is_pipe_created) {
      int p[2];
      if (pipe(p) != 0) {
        SetError("Couldn't create a pipe: " + std::string(strerror(errno)));
        std::this_thread::sleep_for(std::chrono::seconds(ErrorWaitSeconds));
        Update();
        continue;
      }
      int pid = fork();
      if (pid == -1) {
        SetError("Couldn't fork: " + std::string(strerror(errno)));
        if (close(p[0]) == -1 or close(p[1]) == -1) {
          UnrecoverableError("Close failed: ", strerror(errno));
        }
        Update();
        std::this_thread::sleep_for(std::chrono::seconds(ErrorWaitSeconds));
        continue;
      }
      if (pid == 0) {
        // Child.
        if (dup2(p[1], 1 /* stdout */) == -1) {
          UnrecoverableError("Dup2 failed: ", strerror(errno));
        }
        if (close(p[0]) == -1) {
          UnrecoverableError("Close failed: ", strerror(errno));
        }
        char* const envp[] = {NULL};
        char* const argv[] = {"/usr/bin/upower", "--monitor", NULL};
        execve(argv[0], argv, envp);
        UnrecoverableError("Execve failed: ", strerror(errno));
      }
      // Parent.
      if (close(p[1]) == -1) {
        UnrecoverableError("Parent: close failed: ", strerror(errno));
      }
      is_pipe_created = true;
      upower_to_me_descriptor = p[0];
    }
    assert(is_pipe_created);
    fd_set set;
    timeval timeout;
    FD_ZERO(&set);
    FD_SET(upower_to_me_descriptor, &set);
    if (GetFastUpdateIntervalAndSetToFalse()) {
      timeout.tv_sec = FastUpdateIntervalSeconds;
    } else {
      timeout.tv_sec = UpdateIntervalSeconds;
    }
    timeout.tv_usec = 0;
    const int rv =
        select(upower_to_me_descriptor + 1, &set, NULL, NULL, &timeout);
    if (rv == -1) {
      // Error.
      SetError("Select failed: " + std::string(strerror(errno)));
      Update();
      std::this_thread::sleep_for(std::chrono::seconds(ErrorWaitSeconds));
    } else if (rv == 0) {
      // Timeout.
      Update();
    } else {
      // Can read.
      const int read_result = read(upower_to_me_descriptor, buffer, BufferSize);
      if (read_result == -1) {
        SetError("Read failed: " + std::string(strerror(errno)));
        Update();
        std::this_thread::sleep_for(std::chrono::seconds(ErrorWaitSeconds));
      } else if (read_result == 0) {
        // The child process should never terminate.
        UnrecoverableError("The child stopped sending data.");
      } else {
        Update();
      }
    }
  }
}

std::string RunTlpStat() {
  int p[2];
  if (pipe(p) != 0) {
    SetError("Pipe failed: " + std::string(strerror(errno)));
    return std::string();
  }
  const int pid = fork();
  if (pid == -1) {
    SetError("Fork failed: " + std::string(strerror(errno)));
    if (close(p[0]) == -1 or close(p[1]) == -1) {
      UnrecoverableError("Close failed: ", strerror(errno));
    }
    return std::string();
  }
  if (pid == 0) {
    // Child.
    if (dup2(p[1], 1 /* stdout */) == -1) {
      UnrecoverableError("Dup2 failed: ", strerror(errno));
    }
    if (close(p[0]) == -1) {
      UnrecoverableError("Child: close failed: ", strerror(errno));
    }
    char* const envp[] = {NULL};
    char* const argv[] = {"/usr/bin/sudo", "/usr/bin/tlp-stat", "-b", NULL};
    execve(argv[0], argv, envp);
    UnrecoverableError("Execve failed: ", strerror(errno));
  }
  // Parent.
  if (close(p[1]) == -1) {
    UnrecoverableError("Parent: close failed: ", strerror(errno));
  }
  std::string output;
  constexpr int BufferSize = 256;
  char buffer[BufferSize];
  while (true) {
    const int read_result = read(p[0], buffer, BufferSize);
    if (read_result == -1) {
      UnrecoverableError("Parent: read failed: ", strerror(errno));
    } else if (read_result == 0) {
      // EOF.
      if (close(p[0]) == -1) {
        UnrecoverableError("Parent: close failed: ", strerror(errno));
      }
      break;
    } else {
      output.insert(output.end(), buffer, buffer + read_result);
    }
  }
  if (waitpid(pid, NULL /* Status */, 0 /* Flags */) == -1) {
    UnrecoverableError("Waitpid failed: ", strerror(errno));
  }
  return output;
}

void Update() {
  std::string tmp_error;
  if (GetError(tmp_error)) {
    std::lock_guard<std::mutex> lock(mutex);
    std::cerr << "error = " << tmp_error << std::endl;
  } else {
    std::string output = RunTlpStat();
    if (GetError(tmp_error)) {
      std::lock_guard<std::mutex> lock(mutex);
      std::cerr << "tlp-stat error = " << tmp_error << std::endl;
    } else {
      BatteryInfoInternal bii = MakeBatteryInfoInternal(std::move(output));
      std::set<std::pair<BatteryInfoCallback, void*>> processed_callbacks;
      auto GetNextCallback = [&processed_callbacks]()
          -> std::pair<BatteryInfoCallback, void*> {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& callback_pair : callbacks) {
          if (processed_callbacks.find(callback_pair) == processed_callbacks.end()) {
            processed_callbacks.insert(callback_pair);
            return callback_pair;
          }
        }
        return std::make_pair(nullptr, nullptr);
      };
      while (true) {
        auto callback_pair = GetNextCallback();
        if (callback_pair.first == nullptr) {
          break;
        }
        callback_pair.first(&bii.bi, callback_pair.second);
      }
    }
  }
}

void InsertCallback(BatteryInfoCallback callback, void* data) {
  std::lock_guard<std::mutex> lock(mutex);
  callbacks.emplace(callback, data);
  cv.notify_one();
}

}  // namespace

void RegisterCallback(BatteryInfoCallback callback, void* data) {
  TryInit();
  InsertCallback(callback, data);
  SetFastUpdateIntervalToTrue();
}

void UnregisterCallback(BatteryInfoCallback callback, void* data) {
  std::lock_guard<std::mutex> lock(mutex);
  callbacks.erase(std::make_pair(callback, data));
}
