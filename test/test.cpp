#include <stdint.h>
#include <cstring>
#include <iostream>
#include <string>
#include <gtest/gtest.h>
#include "dataex_utility.h"
using namespace std;

typedef uint16_t len_t;
typedef uint8_t  tag_t;
static const tag_t kTagInvalid = 0;

/*----------------------------------------------------------------------------*/
template <class B, class D> struct IsBaseOf {
  typedef char (&Small)[1];
  typedef char (&Big)[2];
  
  static Small Test(B);
  static Big Test(...);
  static D MakeD();

  static const bool value = sizeof(Test(MakeD())) == sizeof(Small);  
};

#define __is_base(Base, ...) IsBaseOf<const Base*, const __VA_ARGS__ *>::value

/*----------------------------------------------------------------------------*/

typedef void (*EncodeFunc)(const void* value, void*& p);
typedef void (*DecodeFunc)(void* value, void*& p, size_t len);
typedef size_t (*GetEncodeSizeFunc)(const void* t);

struct FieldInfo {
  GetEncodeSizeFunc field_encoded_size_func_;
  uint16_t offset_;
  tag_t    tag_;
  EncodeFunc encode_func_;
  DecodeFunc decode_func_;
};

struct Serializable {
  //const FieldInfo *fields_infos_;
};

////////////////////////////////////////////////////////////////////////////////
template<typename T, bool isArray = false>
struct Elem {
  typedef T type;
};

template<typename T>
struct Elem<T, true> {
  typedef typename T::element_type type;
};

////////////////////////////////////////////////////////////////////////////////
template<typename T, bool isSerializable, bool isArray>
struct EncodeSizeGetter {
  static size_t size(const void* t) {
    return sizeof(T);
  }
};

template<typename T>
struct EncodeSizeGetter<T, true, false> {
  static size_t size(const void* t) {
    return __get_size(*((const T*)t));
  }
};

template<typename T>
struct EncodeSizeGetter<T, true, true> {
  static size_t size(const void* t) {
    size_t s = 0;
    const typename Elem<T, true>::type* pt = (const typename Elem<T, true>::type*)t;
    for (int i = 0; i < T::capacity; ++i) {
      s += 2; /*size of len_t*/
      s += EncodeSizeGetter<typename Elem<T, true>::type, true, false>::size(&pt[i]);
    }
    return s;
  }
};

template<bool isSerializable, bool isArray>
struct EncodeSizeGetter<string, isSerializable, isArray> {
  static size_t size(const void* t) {
    return ((string*)t)->size();
  }
};

/*----------------------------------------------------------------------------*/
void* __do_encode(const Serializable& d, FieldInfo* field_info, void* p);
void* __do_decode(Serializable& d, FieldInfo* field_info, void* p, size_t total_len);
size_t __do_get_size(const Serializable& d, FieldInfo* field_info);

#define __encode(d, p)  __do_encode(d, typeof(d)::kFieldsInfos, p)
#define __decode(d, p, total_len) __do_decode(d, typeof(d)::kFieldsInfos, p, total_len)
#define __get_size(d)  __do_get_size(d, typeof(d)::kFieldsInfos)

template<typename T, bool isSerializable, bool isArray>
struct Encoder {
  static void encode(const void* value, void*& p) {
    *(((T*)p)) = *(T*)value;
    p = ((T*)p)+1;
  }
};

template<typename T>
struct Encoder<T, true, false> {
  static void encode(const void* value, void*& p) {
    p = __encode(*(T*)value, p);
  }
};

template<typename T>
struct Encoder<T, true, true> {
  static void encode(const void* value, void*& p) {
    typename Elem<T, true>::type* pt = (typename Elem<T, true>::type*)value;
    for (int i = 0; i < T::capacity; ++i) {
      *((len_t*)p) = EncodeSizeGetter<typename Elem<T, true>::type, true, false>::size(&pt[i]);
      p = ((len_t*)p)+1;
      p = __encode(pt[i], p);
    }
  }
};

template<bool isSerializable, bool isArray>
struct Encoder<string, isSerializable, isArray> {
  static void encode(const void* value, void*& p) {
    string& str = *(string*)( value );
    memcpy(p, str.c_str(), str.size());
    p = ((uint8_t*)p) + str.size();
  }
};


/*----------------------------------------------------------------------------*/

template<typename T, bool isSerializable, bool isArray>
struct Decoder {
  static void decode(void* value, void*& p, size_t len) {
    *(T*)value = *((T*)p);
    p = ((T*)p)+1;
  }
};

template<typename T>
struct Decoder<T, true, true> {
  static void decode(void* value, void*& p, size_t len) {
    typename Elem<T, true>::type* dest = (typename Elem<T, true>::type*)value;
    for (int i = 0; i < T::capacity; ++i) {
      //TODO: check dest[i] not exceeds total_len.
      len_t len  = *(len_t*)p; p = ((len_t*)p)+1;
      p = __decode(dest[i], p, len); 
    }
  }
};

template<typename T>
struct Decoder<T, true, false> {
  static void decode(void* value, void*& p, size_t len) {
    p = __decode(*(Serializable*)value, p, len);
  }
};

template<bool isSerializable, bool isArray>
struct Decoder<string, isSerializable, isArray> {
  static void decode(void* value, void*& p, size_t len) {
    *(string*)value = string((const char*)p, len);
    p = ((uint8_t*)p)+len;
  }
};

/*----------------------------------------------------------------------------*/
void* __do_encode(const Serializable& d, FieldInfo* field_infos, void* p) {
  for (const FieldInfo* fi = fields_infos[0]; fi->tag_ != kTagInvalid; ++fi) {
    *((tag_t*)p) = fi->tag_;
    p = ((tag_t*)p)+1;

    void* value = (void*)(((uint8_t*)&d)+fi->offset_);
    *((len_t*)p) = fi->field_encoded_size_func_(value);
    p = ((len_t*)p)+1;

    (*fi->encode_func_)(value, p);
  }
  
  return p;
}

//TODO: return NULL when decode failed
void* __do_decode(Serializable& d, FieldInfo* field_infos, void* p, size_t total_len) {
  size_t field_count = 0;
  for (const FieldInfo* fi = fields_infos[0]; fi->tag_ != kTagInvalid; ++fi) {
    field_count++;
  }

  void* end = (void*)(((uint8_t*)p) + total_len);
  while (p < end) {
    tag_t t = *(tag_t*)p;   p = ((tag_t*)p)+1;
    len_t len = *(len_t*)p; p = ((len_t*)p)+1;
    
    if ( t > 0 && t <= field_count ) {
      const FieldInfo* fi = fields_infos[t - 1];
      void* buf = (void*)(((uint8_t*)&d)+fi->offset_);
      (*fi->decode_func_)(buf, p, len);
    } else {
      p = ((uint8_t*)p) + len;
    }
  }
  return p;
}

size_t __do_get_size(const Serializable& d, FieldInfo* field_infos) {
  size_t s = 0;
  for (const FieldInfo* fi = fields_infos[0]; fi->tag_ != kTagInvalid; ++fi) {
    s += sizeof(tag_t) + sizeof(len_t);
    void* value = (void*)(((uint8_t*)&d)+fi->offset_);
    s += fi->field_encoded_size_func_(value);
  }
  return s;
}

/*----------------------------------------------------------------------------*/
#define FuncSelector(Struct, Func, ...) Struct< __VA_ARGS__                                                                 \
                                              , __is_base( Serializable                                                     \
                                                         , typename Elem< __VA_ARGS__                                       \
                                                                        , __is_base(dataex::ArrayBase, __VA_ARGS__)>::type  \
                                                         )                                                                  \
                                              , __is_base(dataex::ArrayBase, __VA_ARGS__)>::Func

#define DECLARE_DATA_CLASS_BEGIN(_NAME)  \
namespace __NS_##_NAME {                 \
struct _NAME : Serializable {            \
  _NAME() {
    /*fields_infos_ = &kFieldsInfos[0];*/

#define __INIT_FIELD_IN_CONSTRUCTOR(_TAG, _FIELD_NAME, ...) /*_FIELD_NAME = __VA_ARGS__();*/

#define __DECLARE_FIELD(_FIELD_NAME, ...)                    \
  __VA_ARGS__ _FIELD_NAME;                                   \
  enum {__tag_##_FIELD_NAME = __COUNTER__ - __counter_start};

#define __DEFINE_FIELD_INFO(_FIELD_NAME, ...)                \
  {                                                          \
     &FuncSelector(EncodeSizeGetter, size, __VA_ARGS__)      \
     , offsetof(DataType, _FIELD_NAME)                       \
     , __tag_##_FIELD_NAME                                   \
     , &FuncSelector(Encoder, encode, __VA_ARGS__)           \
     , &FuncSelector(Decoder, decode, __VA_ARGS__)           \
  }, 

#define DECLARE_DATA_CLASS(_NAME)                    \
DECLARE_DATA_CLASS_1( __FIELDS_OF_##_NAME            \
                , _NAME                              \
                , __INIT_FIELD_IN_CONSTRUCTOR        \
                , __DECLARE_FIELD);

#define DEFINE_DATA_CLASS(_NAME) DEFINE_DATA_CLASS_1(__FIELDS_OF_##_NAME, _NAME, __DEFINE_FIELD_INFO)
#define DEFINE_DATA_CLASS_1(EXPAND_FIELDS, _NAME, _M1)  \
namespace __NS_##_NAME {                                \
  const FieldInfo _NAME::kFieldsInfos[] = {             \
      EXPAND_FIELDS(_M1/*__DEFINE_FIELD_INFO*/)         \
      { 0, kTagInvalid }                                \
  };                                                    \
} /*namespace end*/

#define DECLARE_DATA_CLASS_1(EXPAND_FIELDS, _NAME, _M1, _M2)      \
DECLARE_DATA_CLASS_BEGIN(_NAME)                                   \
  EXPAND_FIELDS(_M1/*__INIT_FIELD_IN_CONSTRUCTOR*/)               \
  }                                                               \
  enum { __counter_start = __COUNTER__ };                         \
  EXPAND_FIELDS(_M2/*__DECLARE_FIELD*/)                           \
  static const FieldInfo kFieldsInfos[];                          \
}; /*class end*/                                                  \
typedef _NAME DataType;                                           \
}  /*namespace end*/                                              \
using __NS_##_NAME::_NAME;

#define DEF_DATA(_NAME) DECLARE_DATA_CLASS(_NAME); DEFINE_DATA_CLASS(_NAME);

/*----------------------------------------------------------------------------*/
#define __FIELDS_OF_SingleFieldData(_)  \
  _(a, int)

DEF_DATA(SingleFieldData);

/*----------------------------------------------------------------------------*/
#define __FIELDS_OF_SingleStringData(_)  \
  _(a, string)

DEF_DATA(SingleStringData);

/*----------------------------------------------------------------------------*/
#define __FIELDS_OF_DataX(_)  \
  _(a, int)                   \
  _(b, unsigned int)

DEF_DATA(DataX);

/*----------------------------------------------------------------------------*/
#define __FIELDS_OF_DataXArray(_)  \
  _(a, __array(DataX, 2))          \

DEF_DATA(DataXArray);

/*----------------------------------------------------------------------------*/
#define __FIELDS_OF_DataWithNested(_) \
  _(a, int  )                         \
  _(x, DataX)                         \
  _(b, int  )                         \
  _(c, char )                         \
  _(d, __array(char, 3))              \
  _(e, string)                        \
  _(f, bool)

DEF_DATA(DataWithNested);

/*----------------------------------------------------------------------------*/
#define ENCODE_SIZE_T (sizeof(tag_t))
#define ENCODE_SIZE_TL (ENCODE_SIZE_T + sizeof(len_t))
#define ENCODE_SIZE_TLV(value, ...) \
(ENCODE_SIZE_TL                     \
   + EncodeSizeGetter< __VA_ARGS__, __is_base(Serializable, typename Elem<__VA_ARGS__, __is_base(dataex::ArrayBase, __VA_ARGS__)>::type) \
                     , __is_base(dataex::ArrayBase, __VA_ARGS__)>::size(&value))

/*----------------------------------------------------------------------------*/
struct t : public ::testing::Test {
  virtual void SetUp() {
     buf_ = &buf[0];
     memset(buf_, 0, sizeof(buf));
  }
  char buf[32 * 1024];
  char* buf_;
};

TEST_F(t, SingleFieldData__size_of_struct__should_be_total_of__TLVs) {
  SingleFieldData sfd;
  int i;
  
  EXPECT_EQ(ENCODE_SIZE_TLV(i, int), __get_size(sfd));
}

TEST_F(t, SingleStringData__should_able_to_encode__string_field__in_TLV) {
  SingleStringData ssd;
  ssd.a = "abc";

  EXPECT_EQ(ENCODE_SIZE_TLV(ssd.a, string), __get_size(ssd));
}

TEST_F(t, DataX__size_of_struct__should_be_total_of__TLVs) {
  DataX dx;
  int i;
  EXPECT_EQ(ENCODE_SIZE_TLV(i, int)/*a*/ + ENCODE_SIZE_TLV(i, int)/*b*/, __get_size(dx));
}

TEST_F(t, DataWithNested__size_of_struct_with_nested_struct) {
  DataWithNested dwn;
  DataX dx;
  int i;
  char c;
  dataex::array<char, 3> a3;
  string s;
  bool b;

  size_t expected = ENCODE_SIZE_TLV(i, int) /*a*/
                  + ENCODE_SIZE_TL /*TL of nested X*/ + __get_size(dx) /*encoded X*/
                  + ENCODE_SIZE_TLV(i, int)  /*b*/
                  + ENCODE_SIZE_TLV(c, char) /*c*/
                  + ENCODE_SIZE_TLV(a3, dataex::array<char, 3>)/*d*/
                  + ENCODE_SIZE_TLV(s, string)/*e*/
                  + ENCODE_SIZE_TLV(b, bool);
  
  EXPECT_EQ(expected, __get_size(dwn));
}

/*----------------------------------------------------------------------------*/
TEST_F(t, SingleFieldData__should_able_to_encode_SingleFieldData) {
  SingleFieldData x;
  x.a = 0x12345678;

  __encode(x, buf_);

  char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12};
  EXPECT_TRUE(ArraysMatch(expected, buf_));
}

TEST_F(t, SingleStringData_should_able_to_encode_data_with_string_field) {
  SingleStringData ssd;
  ssd.a = "abc";

  __encode(ssd, buf_);

  char expected[] = { 0x01, 0x03, 0x00, 'a', 'b', 'c'};
  EXPECT_TRUE(ArraysMatch(expected, buf_));
}

TEST_F(t, SingleStringData_should_able_to_encode__bytes_contains_zero__with_string_field) {
  SingleStringData ssd;
  char data_with_zero[] = {'a', '\0', 'b', '\0', 'c', '\0'};
  ssd.a = string(data_with_zero, sizeof(data_with_zero));

  __encode(ssd, buf_);

  char expected[] = { 0x01, 0x06, 0x00, 'a', '\0', 'b', '\0', 'c', '\0'};
  EXPECT_TRUE(ArraysMatch(expected, buf_));
}

TEST_F(t, DataX_should_able_to_encode_normal_struct) {
  DataX x;
  x.a = 0x12345678;
  x.b = 0x11223344;
  __encode(x, buf_);

  char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                    , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11};
  EXPECT_TRUE(ArraysMatch(expected, buf_));
}

/*----------------------------------------------------------------------------*/
char DataXArray_expected[] = { 0x01, 0x20, 0x00  /* T and L of field*/
                             , 0x0E, 0x00        /* L for a[0] */
                             , 0x01, 0x04, 0x00, 0x33, 0x22, 0x11, 0x00
                             , 0x02, 0x04, 0x00, 0x68, 0x57, 0x24, 0x13
                             , 0x0E, 0x00        /* L for a[1] */
                             , 0x01, 0x04, 0x00, 0xbe, 0xba, 0xfe, 0xca
                             , 0x02, 0x04, 0x00, 0xef, 0xbe, 0xad, 0xde };

TEST_F(t, DataXArray_should_able_to__encode__struct_with_nested_struct_array) {
  DataXArray dxa;
  dxa.a[0].a = 0x00112233;
  dxa.a[0].b = 0x13245768;
  dxa.a[1].a = 0xcafebabe;
  dxa.a[1].b = 0xdeadbeef;
                      
  __encode(dxa, buf_);
  EXPECT_TRUE(ArraysMatch(DataXArray_expected, buf_));
}

TEST_F(t, DataXArray_should_able_to__decode___struct_with_nested_struct_array) {
  DataXArray dxb;
  
  __decode(dxb, DataXArray_expected, sizeof(DataXArray_expected));
  
  EXPECT_EQ(0x00112233, dxb.a[0].a);
  EXPECT_EQ(0x13245768, dxb.a[0].b);
  EXPECT_EQ(0xcafebabe, dxb.a[1].a);
  EXPECT_EQ(0xdeadbeef, dxb.a[1].b);
}
/*----------------------------------------------------------------------------*/

TEST_F(t, DataWithNested_should_able_to_encode_struct_with_nested_struct) {
  DataWithNested xn;
  xn.a = 0xCAFEBABE;
  xn.x.a = 0x12345678;
  xn.x.b = 0x11223344;
  xn.b = 0xDEADBEEF;
  xn.c = 0x45;
  memcpy(&xn.d, "XYZ", strlen("XYZ"));

  char buf_with_zero[] = {0x11, 0x22, 0x00, 0x00, 0x33};
  xn.e = string(buf_with_zero, sizeof(buf_with_zero));
  xn.f = true;

  __encode(xn, buf_);

  char expected[] = { 0x01, 0x04, 0x00, 0xBE, 0xBA, 0xFE, 0xCA
                             , 0x02, 0x0E, 0x00 /*T and L of nested X*/
                             , 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11
                             , 0x03, 0x04, 0x00, 0xEF, 0xBE, 0xAD, 0xDE
                             , 0x04, 0x01, 0x00, 0x45
                             , 0x05, 0x03, 0x00, 'X', 'Y', 'Z'
                             , 0x06, 0x05, 0x00, 0x11, 0x22, 0x00, 0x00, 0x33
                             , 0x07, 0x01, 0x00, 0x01};
  
  EXPECT_TRUE(ArraysMatch(expected, buf_));
}

/*----------------------------------------------------------------------------*/
TEST_F(t, SingleFieldData__should_able_to_decode_SingleFieldData) {
  SingleFieldData x;
  char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12};

  __decode(x, expected, sizeof(expected));

  EXPECT_EQ(0x12345678, x.a);
}

TEST_F(t, SingleStringData__should_able_to_dncode_data_with_string_field) {
  SingleStringData ssd;
  char expected[] = { 0x01, 0x04, 0x00, 'a', 'b', 'c', '\0'};
  
  __decode(ssd, expected, sizeof(expected));
  EXPECT_STREQ(ssd.a.c_str(), "abc");
}

TEST_F(t, SingleStringData__should_able_to_decode__bytes_contains_zero__with_string_field) {
  SingleStringData ssd;
  char expected[] = { 0x01, 0x06, 0x00, 'a', '\0', 'b', '\0', 'c', '\0'};
  char data_with_zero[] = {'a', '\0', 'b', '\0', 'c', '\0'};

  __decode(ssd, expected, sizeof(expected));  
  EXPECT_TRUE(ArraysMatch(data_with_zero, ssd.a.c_str()));
}

TEST_F(t, DataX__should_able_to_decode_normal_struct) {
  DataX x;
  char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11};

  __decode(x, expected, sizeof(expected));

  EXPECT_EQ(0x12345678, x.a);
  EXPECT_EQ(0x11223344, x.b);
}

TEST_F(t, DataWithNested__should_able_to_decode_struct_with_nested_struct) {
  DataWithNested xn;

  char expected[] = { 0x01, 0x04, 0x00, 0xBE, 0xBA, 0xFE, 0xCA
                             , 0x02, 0x0E, 0x00 /*T and L of nested X*/
                             , 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11
                             , 0x03, 0x04, 0x00, 0xEF, 0xBE, 0xAD, 0xDE
                             , 0x04, 0x01, 0x00, 0x45
                             , 0x05, 0x03, 0x00, 'X', 'Y', 'Z'
                             , 0x06, 0x05, 0x00, 'h', 'e', 'l', 'l', 'o'
                             , 0x07, 0x01, 0x00, 0x01};

  __decode(xn, expected, sizeof(expected));
  
  EXPECT_EQ(0xCAFEBABE, xn.a);
  EXPECT_EQ(0x12345678, xn.x.a);
  EXPECT_EQ(0x11223344, xn.x.b);
  EXPECT_EQ(0xDEADBEEF, xn.b);
  EXPECT_EQ(0x45,       xn.c);
  EXPECT_EQ('X',       xn.d[0]);
  EXPECT_EQ('Y',       xn.d[1]);
  EXPECT_EQ('Z',       xn.d[2]);
  EXPECT_STREQ("hello", xn.e.c_str());
  EXPECT_EQ(true,      xn.f);
}

/*----------------------------------------------------------------------------*/
TEST_F(t, DataX__should_ignore_unknown_tag__WHEN___decode_normal_struct) {
  DataX x;
  char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x03, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12 /*unexpected tag 0x03*/
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11};

  __decode(x, expected, sizeof(expected));
  
  EXPECT_EQ(0x12345678, x.a);
  EXPECT_EQ(0x11223344, x.b);
}

TEST_F(t, DataWithNested__should_ignore_unknown_tag__WHEN__decode_struct_with_nested_struct) {
  DataWithNested xn;

  char expected[] = { 0x01, 0x04, 0x00, 0xBE, 0xBA, 0xFE, 0xCA
                             , 0x09, 0x03, 0x00, 0x01, 0x02, 0x03       /*unknown tag 0x09*/
                             , 0x02, 0x0E, 0x00 /*T and L of nested X*/
                             , 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x11, 0x01, 0x00, 0x01                   /*unknown tag 0x011*/
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11
                             , 0x12, 0x05, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 /*unknown tag*/
                             , 0x03, 0x04, 0x00, 0xEF, 0xBE, 0xAD, 0xDE
                             , 0x04, 0x01, 0x00, 0x45};

  __decode(xn, expected, sizeof(expected));
  
  EXPECT_EQ(0xCAFEBABE, xn.a);
  EXPECT_EQ(0x12345678, xn.x.a);
  EXPECT_EQ(0x11223344, xn.x.b);
  EXPECT_EQ(0xDEADBEEF, xn.b);
  EXPECT_EQ(0x45, xn.c);
}

/*----------------------------------------------------------------------------
TODO:
- enum {__field_count = 2};
  uint8_t fields_presence_[__field_count / 8 + 1];
- array of nested serialize field
- types
-bool
-string
-int32
-uint32
double
float
int64
uint64

- all kinds of compatiblity tests. new old tests.
- 	@g++ $(CPPFLAGS) $(LDFLAGS) -g -o main $^ $(LDLIBS) && ./main --gtest_filter="*" && echo "" && echo "" && echo ""
- ./test/test.rb ./main | grep -v testing | grep -v typeinfo | grep -v TestBody | grep -v vtable | grep -v t_
-   static const FieldInfo *fields_infos_;  remove trailing _.
- test value of &d.fields_infos_[0] before access.
------------------------------------------------------------------------------*/
