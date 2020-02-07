// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk) -I %S/Inputs/ %s -typecheck -verify

import ExternIntX

x += 1
