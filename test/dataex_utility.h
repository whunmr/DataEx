#ifndef __DATAEX_UTILITY_H__
#define __DATAEX_UTILITY_H__

template <bool>
struct enable_if {};

template <>
struct enable_if<true> {
  typedef void type;
};

/*----------------------------------------------------------------------------*/
template <class B, class D> struct IsBaseOf {
  typedef char (&Small)[1];
  typedef char (&Big)[2];
  
  static Small Test(B);
  static Big Test(...);
  static D MakeD();

  static const bool value = sizeof(Test(MakeD())) == sizeof(Small);  
};

#define __is_base_of(Base, Derived) IsBaseOf<const Base*, const Derived*>::value

/*----------------------------------------------------------------------------*/
namespace dataex {
template<typename T, size_t N>
  struct array{
    T elems[N];
    
    typedef T&             reference;
    typedef const T&       const_reference;
    
    reference operator[](size_t i) { 
        assert(("out of range", i < N));
        return elems[i];
    }
    
    const_reference operator[](size_t i) const {     
        assert(("out of range", i < N));
        return elems[i]; 
    }
  
    const T* data() const { return elems; }
    T* data() { return elems; }
    T* c_array() { return elems; }
  };
} //namespace dataex

/*----------------------------------------------------------------------------*/
template<typename T, size_t size>
::testing::AssertionResult ArraysMatch(const T (&expected)[size],
                                       const char* actual){
    for (size_t i(0); i < size; ++i){
        if (expected[i] != actual[i]){
            return ::testing::AssertionFailure() << "array[" << i
                << "] (" << actual[i]  << "[as int]: "<< (int)actual[i]
                << ") != expected[" << i
                << "] (" << expected[i] << "[as int]: " << (int)expected[i] << ")";
        }
    }
    return ::testing::AssertionSuccess();
}

/*----------------------------------------------------------------------------*/
#ifdef _MSC_VER
  #if defined(_M_IX86) && !defined(DATAEX_DISABLE_LITTLE_ENDIAN_OPT_FOR_TEST)
    #define DATAEX_LITTLE_ENDIAN 1
  #endif
#else
  #include <sys/param.h>   // __BYTE_ORDER
  #if ((defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)) || \
         (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN)) && \
      !defined(DATAEX_DISABLE_LITTLE_ENDIAN_OPT_FOR_TEST)
    #define DATAEX_LITTLE_ENDIAN 1
  #endif
#endif

#endif //#ifndef __DATAEX_UTILITY_H__
