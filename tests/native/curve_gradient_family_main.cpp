#include "harness.h"
#include <cstring>
int main() {
  unsigned total=0,failed=0;
  for(const auto &test:ae::test::registry()) {
    if(std::strncmp(test.name,"script_curve_",13) &&
       std::strcmp(test.name,"script_gradient_format_validation_and_evaluation") &&
       std::strcmp(test.name,"curve_editor_adds_moves_edits_keys_and_applies_once") &&
       std::strcmp(test.name,"gradient_editor_adds_moves_removes_stops_and_applies_once"))continue;
    ++total;ae::test::currentTestName()=test.name;ae::test::currentTestFailed()=false;test.fn();
    failed+=ae::test::currentTestFailed();std::printf("%s %s\n",ae::test::currentTestFailed()?"FAIL":"PASS",test.name);
  }
  std::printf("%u/%u scenarios passed\n",total-failed,total);return failed || total!=5;
}
