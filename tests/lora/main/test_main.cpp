#include "unity.h"

extern "C" void app_main(void) {
  // unity_run_menu() launches an interactive serial menu that lets you run
  // individual test cases or all tests at once.
  // Send '!' over the serial monitor to run all tests non-interactively.
  unity_run_menu();
}
