#include <cassert>
#include <chrono>
#include <cstdlib>
#include <thread>

#include "battery_info.h"

void Callback(const BatteryInfo* bi, void* data) {
  int* x = (int*) data;
  printf("x = %d\n", *x);
  printf("Callback {\n");
  printf("  error = ");
  if (bi->error == NULL) {
    printf("NULL\n");
  } else {
    printf("%s\n", bi->error);
  }
  printf("  number_of_batteries = %d\n", bi->number_of_batteries);
  for (int i = 0; i < bi->number_of_batteries; i++) {
    const SingleBatteryInfo& sbi = bi->sbis[i];
    printf("  Battery {\n");
    printf("    charge = %.2lf\n", sbi.charge);
    printf("    status = ");
    switch (sbi.status) {
      case kUnused:       printf("Unused\n"); break;
      case kDischarging:  printf("Discharging\n"); break;
      case kCharging:     printf("Charging\n"); break;
      case kFull:         printf("Full\n"); break;
      default:            printf("Error\n");
    }
    printf("  }\n");
  }
  printf("  minutes_left = %d (%02d:%02d)\n",
         bi->minutes_left, bi->minutes_left / 60, bi->minutes_left % 60);
  printf("}\n");
}

int main() {
  int x = 123;
  RegisterCallback(Callback, (void*) &x);
  int y = 124;
  RegisterCallback(Callback, (void*) &y);
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
  return EXIT_SUCCESS;
}
