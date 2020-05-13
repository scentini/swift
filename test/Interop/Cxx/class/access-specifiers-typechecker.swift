// Test that C++ access specifiers are honored, i.e. private members aren't
// imported.

// RUN: %target-typecheck-verify-swift -verify-ignore-unknown -I %S/Inputs -enable-cxx-interop

import AccessSpecifiers

var v = PublicPrivate()

// Can access all public members and types.

v.PublicMemberVar = 1
PublicPrivate.PublicStaticMemberVar = 1
v.publicMemberFunc()

var publicTypedefVar: PublicPrivate.PublicTypedef
var publicStructVar: PublicPrivate.PublicStruct
var publicEnumVar: PublicPrivate.PublicEnum
print(PublicPrivate.PublicEnumValue1)
print(PublicPrivate.PublicAnonymousEnumValue)
var publicClosedEnumVar: PublicPrivate.PublicClosedEnum
var publicOpenEnumVar: PublicPrivate.PublicOpenEnum
var publicFlagEnumVar: PublicPrivate.PublicFlagEnum

// Cannot access any private members and types.

v.PrivateMemberVar = 1 // expected-error {{value of type 'PublicPrivate' has no member 'PrivateMemberVar'}}
PublicPrivate.PrivateStaticMemberVar = 1 // expected-error {{'PublicPrivate' has no member 'PrivateStaticMemberVar'}}
v.privateMemberFunc() // expected-error {{value of type 'PublicPrivate' has no member 'privateMemberFunc'}}
var privateTypedefVar: PublicPrivate.PrivateTypedef // expected-error {{'PrivateTypedef' is not a member type of 'PublicPrivate'}}
var privateStructVar: PublicPrivate.PrivateStruct // expected-error {{'PrivateStruct' is not a member type of 'PublicPrivate'}}
var privateEnumVar: PublicPrivate.PrivateEnum // expected-error {{'PrivateEnum' is not a member type of 'PublicPrivate'}}
print(PublicPrivate.PrivateEnumValue1) // expected-error {{'PublicPrivate' has no member 'PrivateEnumValue1'}}
print(PublicPrivate.PrivateAnonymousEnumValue) // expected-error {{'PublicPrivate' has no member 'PrivateAnonymousEnumValue'}}
var privateClosedEnumVar: PublicPrivate.PrivateClosedEnum // expected-error {{'PrivateClosedEnum' is not a member type of 'PublicPrivate'}}
var privateOpenEnumVar: PublicPrivate.PrivateOpenEnum // expected-error {{'PrivateOpenEnum' is not a member type of 'PublicPrivate'}}
var privateFlagEnumVar: PublicPrivate.PrivateFlagEnum // expected-error {{'PrivateFlagEnum' is not a member type of 'PublicPrivate'}}
