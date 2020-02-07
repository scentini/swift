#define SWIFT_NAME(X) __attribute__((swift_name(#X)))

#define SWIFT_ENUM(_type, _name) \
  enum _name : _type _name; enum __attribute__((enum_extensibility(open))) _name : _type

@interface Foo
- (instancetype)init;
@end

void acceptsClosure(id value, void (*fn)(void)) SWIFT_NAME(Foo.accepts(self:closure:));
void acceptsClosureStatic(void (*fn)(void)) SWIFT_NAME(Foo.accepts(closure:));

enum {
  // Note that there was specifically a crash when renaming onto an ObjC class,
  // not just a struct.
  AnonymousEnumConstantObjC SWIFT_NAME(Foo.anonymousEnumConstant)
};

