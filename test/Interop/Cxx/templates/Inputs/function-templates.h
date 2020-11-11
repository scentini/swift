template <class T> T add(T a, T b) { return a + b; }

template <class A, class B> A addTwoTemplates(A a, B b) { return a + b; }

template <class T> T passThrough(T value) { return value; }

template <class T> const T passThroughConst(const T value) { return value; }

void takesString(const char *) {}
template <class T> void expectsString(T str) { takesString(str); }

template <long x> void integerTemplate() {}
template <long x = 0> void defaultIntegerTemplate() {}

// We cannot yet use this in swift but, make sure we don't crash when parsing
// it.
template <class R, class T, class U> R returns_template(T a, U b) {
  return a + b;
}

// Same here:
template <class T> void cannot_infer_template() {}

// TODO: We should support these types. Until then, make sure we don't crash when importing.
template<class... Ts>
void testPackExpansion(Ts...) { }

template<class T>
void testTypeOfExpr(T a, typeof(a + 1) b) { }

template<class T>
void testTypeOf(T a, typeof a b) { }

template<class T>
decltype(auto) testAuto(T arg) {
  return arg;
}

// TODO: Add tests for Decltype, UnaryTransform, and TemplateSpecialization with
// a dependent type once those are supported.

// TODO: Add test for DeducedTemplateSpecializationType once we support class templates.

// TODO(SR-13809): We don't yet support dependent types but we still shouldn't
// crash when importing one.
template <class T> struct Dep { using TT = T; };

template <class T> void useDependentType(typename Dep<T>::TT) {}

template <class T> void lvalueReference(T &ref) { ref = 42; }

template <class T> void constLvalueReference(const T &) {}

template <class T> void forwardingReference(T &&) {}

namespace Orbiters {

template<class T>
void galileo(T) { }

template<class T, class U>
void cassini(T, U) { }

template<class T>
void magellan(T&) { }

}
