#include <cstdlib>
#include <unistd.h>

int main() {
  setuid(0);
  system("/usr/bin/tlp-stat");
  return 0;
}
