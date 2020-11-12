// RUN: %target-swift-frontend -emit-module -o %t/FortyTwo.swiftmodule -I %S/Inputs -enable-cxx-interop  %s
import IncludeString

func getString() -> std.String {
  return returnString()
}
