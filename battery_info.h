#ifndef BATTERY_INFO_H_
#define BATTERY_INFO_H_

#if __cplusplus
extern "C" {
#endif

typedef enum {
  kUnused = 0,
  kDischarging = 1,
  kCharging = 2,
  kFull = 3,
} BatteryStatus;

typedef struct {
  // The % of battery left: [0, 100].
  double charge;
  BatteryStatus status;
} SingleBatteryInfo;

typedef struct {
  // @error message, NULL if there was no error.
  char* error;

  int number_of_batteries;

  // An array of length @number_of_batteries.
  SingleBatteryInfo* sbis;

  // The number of minutes left, -1 if unknown.
  int minutes_left;
} BatteryInfo;

typedef void (*BatteryInfoCallback)(const BatteryInfo*, void*);

void RegisterCallback(BatteryInfoCallback callback, void* data);
void UnregisterCallback(BatteryInfoCallback callback, void* data);

#if __cplusplus
}  // extern "C"
#endif

#endif  // BATTERY_INFO_H_
