#ifndef TEST_INTEROP_CXX_TEMPLATES_INPUTS_EXPLICIT_SPECIALIZATION_H
#define TEST_INTEROP_CXX_TEMPLATES_INPUTS_EXPLICIT_SPECIALIZATION_H

struct MagicNumber {
public:
  inline int getInt() const { return 26; }
};

template <class T> struct MagicWrapper {
public:
  T t;
  inline int callGetInt() const { return t.getInt() + 5; }
};

template <> struct MagicWrapper<MagicNumber> {
  MagicNumber t;
  int callGetInt() const { return t.getInt() + 10; }
};

typedef MagicWrapper<MagicNumber> MagicWrappedNumberWithExplicitSpecialization;

#endif // TEST_INTEROP_CXX_TEMPLATES_INPUTS_EXPLICIT_SPECIALIZATION_H
