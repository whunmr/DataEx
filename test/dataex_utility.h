#ifndef __DATAEX_UTILITY_H__
#define __DATAEX_UTILITY_H__

template <bool> struct enable_if {};
template <>     struct enable_if<true> {
  typedef void type;
};

template <class B, class D> struct IsBaseOf {
  typedef char (&Small)[1];
  typedef char (&Big)[2];
  
  static Small Test(B);
  static Big Test(...);
  static D MakeD();

  static const bool value = sizeof(Test(MakeD())) == sizeof(Small);  
};

#define __is_base_of(Base, Derived) IsBaseOf<const Base*, const Derived*>::value


#endif //#ifndef __DATAEX_UTILITY_H__
