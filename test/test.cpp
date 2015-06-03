#include <stdint.h>
#include <cstring>
#include <iostream>
#include <string>
#include <gtest/gtest.h>
#include "dataex_utility.h"
using namespace std;

/*----------------------------------------------------------------------------*/
typedef uint16_t len_t;
typedef uint8_t  tag_t;
static const tag_t kTagInvalid = 0;

/*----------------------------------------------------------------------------*/
typedef void (*EncodeFunc)(const void* instance, size_t field_offset, void*& p);
typedef void (*DecodeFunc)(void* instance, size_t field_offset, void*& p, size_t len);
typedef size_t (*GetEncodeSizeFunc)(const void* t);

struct FieldInfo {
  GetEncodeSizeFunc field_encoded_size_func_;
  uint16_t offset_;
  tag_t    tag_;
  EncodeFunc encode_func_;
  DecodeFunc decode_func_;
};

struct Serializable {
  const FieldInfo *fields_infos_;
};

template<typename T, class Enable = void>
struct EncodeSizeGetter {
  static size_t size(const void* t) {
    return sizeof(T);
  }
};

template<typename T>
struct EncodeSizeGetter<T, typename enable_if<__is_base_of(Serializable, T)>::type> {
  static size_t size(const void* t) {
    return T::size(t);
  }
};

template<>
struct EncodeSizeGetter<string> {
  static size_t size(const void* t) {
    return ((string*)t)->size();
  }
};

/*----------------------------------------------------------------------------*/
void* __encode(const Serializable& d, void* p);
void* __decode(Serializable& d, void* p, size_t total_len);

template<typename T, class Enable = void>
struct Encoder {
  static void encode(const void* instance, size_t field_offset, void*& p) {
    *((T*)p) = *(T*)( ((uint8_t*)instance) + field_offset );
    p = ((T*)p)+1;
  }
};

template<typename T>
struct Encoder<T, typename enable_if<__is_base_of(Serializable, T)>::type> {
  static void encode(const void* instance, size_t field_offset, void*& p) {
    Serializable& nested = *(Serializable*)( ((uint8_t*)instance) + field_offset );
    p = __encode(nested, p);
  }
};

template<>
struct Encoder<string> {
  static void encode(const void* instance, size_t field_offset, void*& p) {
    string& str = *(string*)( ((uint8_t*)instance) + field_offset );
    memcpy(p, str.c_str(), str.size());
    p = ((uint8_t*)p) + str.size();
  }
};

/*----------------------------------------------------------------------------*/
template<typename T, class Enable = void>
struct Decoder {
  static void decode(void* instance, size_t field_offset, void*& p, size_t len) {
    *(T*)(((uint8_t*)instance)+field_offset) = *((T*)p);
    p = ((T*)p)+1;
  }
};

template<typename T>
struct Decoder<T, typename enable_if<__is_base_of(Serializable, T)>::type> {
  static void decode(void* instance, size_t field_offset, void*& p, size_t len) {
    Serializable& nested = *(Serializable*)( ((uint8_t*)instance) + field_offset );
    p = __decode(nested, p, len);
  }
};

template<>
struct Decoder<string> {
  static void decode(void* instance, size_t field_offset, void*& p, size_t len) {
    string& str = *(string*)( ((uint8_t*)instance) + field_offset );
    str = string((const char*)p, len);
  }
};

/*----------------------------------------------------------------------------*/
void* __encode(const Serializable& d, void* p) {
  for (const FieldInfo* fi = &d.fields_infos_[0]; fi->tag_ != kTagInvalid; ++fi) {
    *((tag_t*)p) = fi->tag_;
    p = ((tag_t*)p)+1;

    void* value = (void*)(((uint8_t*)&d)+fi->offset_);
    *((len_t*)p) = fi->field_encoded_size_func_(value);
    p = ((len_t*)p)+1;

    (*fi->encode_func_)(&d, fi->offset_, p);
  }
  
  return p;
}

//TODO: return NULL when decode failed
void* __decode(Serializable& d, void* p, size_t total_len) {
  size_t field_count = 0;
  for (const FieldInfo* fi = &d.fields_infos_[0]; fi->tag_ != kTagInvalid; ++fi) {
    field_count++;
  }
  
  void* end = (void*)(((uint8_t*)p) + total_len);
  while (p < end) {
    tag_t t = *(tag_t*)p;   p = ((tag_t*)p)+1;
    len_t len = *(len_t*)p; p = ((len_t*)p)+1;

    if ( t > 0 && t <= field_count ) {
      const FieldInfo* fi = &d.fields_infos_[t - 1];
      (*fi->decode_func_)(&d, fi->offset_, p, len);
    } else {
      p = ((uint8_t*)p) + len;
    }
  }
  return p;
}

/*----------------------------------------------------------------------------*/
#define DECLARE_DATA_CLASS_BEGIN(_NAME)  \
namespace __NS_##_NAME {                 \
struct _NAME : Serializable {            \
  _NAME() {                              \
    fields_infos_ = &kFieldsInfos[0]; 

#define __INIT_FIELD_IN_CONSTRUCTOR(_TAG, _FIELD_NAME, ...) /*_FIELD_NAME = __VA_ARGS__();*/

#define __DECLARE_FIELD(_TAG, _FIELD_NAME, ...) \
  __VA_ARGS__ _FIELD_NAME;                      \
  enum {__tag_##_FIELD_NAME = _TAG};

#define __FIELD_ENCODE_SIZE(_TAG, _FIELD_NAME, ...)  \
  + sizeof(tag_t) + sizeof(len_t) + EncodeSizeGetter<__VA_ARGS__>::size(&(instance->_FIELD_NAME))

#define __DEFINE_FIELD_INFO(_TAG, _FIELD_NAME, ...) \
    { &EncodeSizeGetter<__VA_ARGS__>::size          \
    , offsetof(DataType, _FIELD_NAME)               \
    , _TAG                                          \
    , &Encoder<__VA_ARGS__>::encode                 \
    , &Decoder<__VA_ARGS__>::decode }, 

#define DECLARE_DATA_CLASS(_NAME)                  \
DECLARE_DATA_CLASS_1( EXPAND_FIELDS_##_NAME        \
                , _NAME                            \
                , __INIT_FIELD_IN_CONSTRUCTOR      \
                , __DECLARE_FIELD                  \
                , __FIELD_ENCODE_SIZE);

#define DEFINE_DATA_CLASS(_NAME) DEFINE_DATA_CLASS_1(EXPAND_FIELDS_##_NAME, _NAME, __DEFINE_FIELD_INFO)
#define DEFINE_DATA_CLASS_1(EXPAND_FIELDS, _NAME, _M1)  \
namespace __NS_##_NAME {                                \
  const FieldInfo _NAME::kFieldsInfos[] = {             \
      EXPAND_FIELDS(_M1/*__DEFINE_FIELD_INFO*/)         \
      { 0, kTagInvalid }                                \
  };                                                    \
} /*namespace end*/

#define DECLARE_DATA_CLASS_1(EXPAND_FIELDS, _NAME, _M1, _M2, _M3) \
DECLARE_DATA_CLASS_BEGIN(_NAME)                                   \
  EXPAND_FIELDS(_M1/*__INIT_FIELD_IN_CONSTRUCTOR*/)               \
  }                                                               \
  EXPAND_FIELDS(_M2/*__DECLARE_FIELD*/)                           \
  static size_t size(const void* p) {                             \
     const _NAME* instance = (const _NAME*)p;                     \
     return 0 EXPAND_FIELDS(_M3/*__FIELD_ENCODE_SIZE*/);          \
  };                                                              \
  static const FieldInfo kFieldsInfos[];                          \
}; /*class end*/                                                  \
typedef _NAME DataType;                                           \
}  /*namespace end*/                                              \
using __NS_##_NAME::_NAME;

#define DEF_DATA(_NAME) DECLARE_DATA_CLASS(_NAME); DEFINE_DATA_CLASS(_NAME);

/*----------------------------------------------------------------------------*/
#define EXPAND_FIELDS_SingleFieldData(_)  \
  _(1, a, int)

DEF_DATA(SingleFieldData);

/*----------------------------------------------------------------------------*/
#define EXPAND_FIELDS_SingleStringData(_)  \
  _(1, a, string)

DEF_DATA(SingleStringData);

/*----------------------------------------------------------------------------*/
#define EXPAND_FIELDS_DataX(_)  \
  _(1, a, int)                  \
  _(2, b, int)

DEF_DATA(DataX);

/*----------------------------------------------------------------------------*/
#define EXPAND_FIELDS_DataWithNested(_)  \
  _(1,  a, int  )                        \
  _(2,  x, DataX)                        \
  _(3,  b, int  )                        \
  _(4,  c, char )                        \
  _(5,  d, dataex::array<char, 3>)       \
  _(6,  e, string)

DEF_DATA(DataWithNested);

/*----------------------------------------------------------------------------*/
    #define ENCODE_SIZE_T (sizeof(tag_t))
    #define ENCODE_SIZE_TL (ENCODE_SIZE_T + sizeof(len_t))
    #define ENCODE_SIZE_TLV(value, ...) (ENCODE_SIZE_TL + EncodeSizeGetter<__VA_ARGS__>::size(&value))
/*----------------------------------------------------------------------------*/
char __buf[32 * 1024]; char* g_buf = &__buf[0];
class t : public ::testing::Test {
  virtual void SetUp() {
     memset(g_buf, 0, sizeof(__buf));
  }
};

TEST_F(t, SingleFieldData__size_of_struct__should_be_total_of__TLVs) {
  SingleFieldData sfd;
  int i;

  EXPECT_EQ(ENCODE_SIZE_TLV(i, int), SingleFieldData::size(&sfd));
}

TEST_F(t, SingleStringData__should_able_to_encode__string_field__in_TLV) {
  SingleStringData ssd;
  ssd.a = "abc";

  EXPECT_EQ(ENCODE_SIZE_TLV(ssd.a, string), SingleStringData::size(&ssd));
}

TEST_F(t, DataX__size_of_struct__should_be_total_of__TLVs) {
  DataX dx;
  int i;
  EXPECT_EQ(ENCODE_SIZE_TLV(i, int)/*a*/ + ENCODE_SIZE_TLV(i, int)/*b*/, DataX::size(&dx));
}

TEST_F(t, DataWithNested__size_of_struct_with_nested_struct) {
  DataWithNested dwn;
  DataX dx;
  int i;
  char c;
  dataex::array<char, 3> a3;
  string s;

  size_t expected = ENCODE_SIZE_TLV(i, int) /*a*/
                  + ENCODE_SIZE_TL /*TL of nested X*/ + DataX::size(&dx) /*encoded X*/
                  + ENCODE_SIZE_TLV(i, int)  /*b*/
                  + ENCODE_SIZE_TLV(c, char) /*c*/
                  + ENCODE_SIZE_TLV(a3, dataex::array<char, 3>)/*d*/
                  + ENCODE_SIZE_TLV(s, string)/*e*/;
  
  EXPECT_EQ(expected, DataWithNested::size(&dwn));
}

/*----------------------------------------------------------------------------*/
TEST_F(t, SingleFieldData__should_able_to_encode_SingleFieldData) {
  SingleFieldData x;
  x.a = 0x12345678;

  __encode(x, g_buf);

  char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12};
  EXPECT_TRUE(ArraysMatch(expected, g_buf));
}

TEST_F(t, SingleStringData_should_able_to_encode_data_with_string_field) {
  SingleStringData ssd;
  ssd.a = "abc";

  __encode(ssd, g_buf);

  char expected[] = { 0x01, 0x03, 0x00, 'a', 'b', 'c'};
  EXPECT_TRUE(ArraysMatch(expected, g_buf));
}

TEST_F(t, SingleStringData_should_able_to_encode__bytes_contains_zero__with_string_field) {
  SingleStringData ssd;
  char data_with_zero[] = {'a', '\0', 'b', '\0', 'c', '\0'};
  ssd.a = string(data_with_zero, sizeof(data_with_zero));

  __encode(ssd, g_buf);

  char expected[] = { 0x01, 0x06, 0x00, 'a', '\0', 'b', '\0', 'c', '\0'};
  EXPECT_TRUE(ArraysMatch(expected, g_buf));
}

TEST_F(t, DataX_should_able_to_encode_normal_struct) {
  DataX x;
  x.a = 0x12345678;
  x.b = 0x11223344;
  __encode(x, g_buf);

  char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11};
  EXPECT_TRUE(ArraysMatch(expected, g_buf));
}

TEST_F(t, DataWithNested_should_able_to_encode_struct_with_nested_struct) {
  DataWithNested xn;
  xn.a = 0xCAFEBABE;
  xn.x.a = 0x12345678;
  xn.x.b = 0x11223344;
  xn.b = 0xDEADBEEF;
  xn.c = 0x45;
  memcpy(xn.d.c_array(), "XYZ", strlen("XYZ"));
  xn.e = "hello";

  __encode(xn, g_buf);

  char expected[] = { 0x01, 0x04, 0x00, 0xBE, 0xBA, 0xFE, 0xCA
                             , 0x02, 0x0E, 0x00 /*T and L of nested X*/
                             , 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11
                             , 0x03, 0x04, 0x00, 0xEF, 0xBE, 0xAD, 0xDE
                             , 0x04, 0x01, 0x00, 0x45
                             , 0x05, 0x03, 0x00, 'X', 'Y', 'Z'
                             , 0x06, 0x05, 0x00, 'h', 'e', 'l', 'l', 'o'};

  EXPECT_TRUE(ArraysMatch(expected, g_buf));
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
                             , 0x06, 0x05, 0x00, 'h', 'e', 'l', 'l', 'o'};

  __decode(xn, expected, sizeof(expected));
  
  EXPECT_EQ(0xCAFEBABE, xn.a);
  EXPECT_EQ(0x12345678, xn.x.a);
  EXPECT_EQ(0x11223344, xn.x.b);
  EXPECT_EQ(0xDEADBEEF, xn.b);
  EXPECT_EQ(0x45,       xn.c);
  EXPECT_EQ('X',       xn.d[0]);
  EXPECT_EQ('Y',       xn.d[1]);
  EXPECT_EQ('Z',       xn.d[2]);
  EXPECT_STREQ(xn.e.c_str(), "hello");
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
- types
bool
string
int32
double
float
int64
uint32
uint64
------------------------------------------------------------------------------*/
