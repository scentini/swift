// RUN: %target-run-simple-swift(-I %S/Inputs -Xfrontend -enable-cxx-interop -Xcc -std=c++17)
//
// REQUIRES: executable_test

import DeclWithDefinition
import StdlibUnittest

var TemplatesTestSuite = TestSuite("TemplatesTestSuite")

TemplatesTestSuite.test("has-partial-definition") {
  let magicNumber = MagicNumber()
  var wrappedMagicNumber = PartiallyDefinedWrappedMagicNumber(t: magicNumber)
  expectEqual(wrappedMagicNumber.callGetInt(), 29)
}

runAllTests()
