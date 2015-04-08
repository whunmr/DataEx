#include <stdint.h>
#include <cstring>
#include <iostream>
#include <string>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits.hpp>
#include <boost/array.hpp>
USING_MOCKCPP_NS;
using namespace std;

/*----------------------------------------------------------------------------*/
typedef uint16_t len_t;
typedef uint8_t  tag_t;
static const tag_t kTagInvalid = 0;

/*----------------------------------------------------------------------------*/
typedef void (*EncodeFunc)(const void* instance, size_t field_offset, void*& p);
typedef void (*DecodeFunc)(void* instance, size_t field_offset, void*& p, size_t len);

struct FieldInfo {
  size_t   field_encoded_size_;
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
  enum {encode_size = sizeof(T)}; //TODO: delete
  static size_t size(const T& t) {
    return sizeof(T);
  }
};

template<typename T>
struct EncodeSizeGetter<T, typename boost::enable_if_c<boost::is_base_of<Serializable, T>::value>::type> {
  enum {encode_size = T::encode_size};
  static size_t size(const T& t) {
    return t.size();
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
struct Encoder<T, typename boost::enable_if_c<boost::is_base_of<Serializable, T>::value>::type> {
  static void encode(const void* instance, size_t field_offset, void*& p) {
    Serializable& nested = *(Serializable*)( ((uint8_t*)instance) + field_offset );
    p = __encode(nested, p);
  }
};

template<typename T, class Enable = void>
struct Decoder {
  static void decode(void* instance, size_t field_offset, void*& p, size_t len) {
    *(T*)(((uint8_t*)instance)+field_offset) = *((T*)p);
    p = ((T*)p)+1;
  }
};

template<typename T>
struct Decoder<T, typename boost::enable_if_c<boost::is_base_of<Serializable, T>::value>::type> {
  static void decode(void* instance, size_t field_offset, void*& p, size_t len) {
    Serializable& nested = *(Serializable*)( ((uint8_t*)instance) + field_offset );
    p = __decode(nested, p, len);
  }
};

/*----------------------------------------------------------------------------*/
void* __encode(const Serializable& d, void* p) {
  for (const FieldInfo* fi = &d.fields_infos_[0]; fi->tag_ != kTagInvalid; ++fi) {
    *((tag_t*)p) = fi->tag_;
    p = ((tag_t*)p)+1;

    *((len_t*)p) = fi->field_encoded_size_;
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

#define __FIELD_ENCODE_SIZE(_TAG, _FIELD_NAME, ...) \
  + sizeof(tag_t) + sizeof(len_t) + EncodeSizeGetter<__VA_ARGS__>::encode_size

#define __DEFINE_FIELD_INFO(_TAG, _FIELD_NAME, ...) \
    { EncodeSizeGetter<__VA_ARGS__>::encode_size    \
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
}  /*namespace end*/
  

#define DECLARE_DATA_CLASS_1(EXPAND_FIELDS, _NAME, _M1, _M2, _M3) \
DECLARE_DATA_CLASS_BEGIN(_NAME)                                   \
  EXPAND_FIELDS(_M1/*__INIT_FIELD_IN_CONSTRUCTOR*/)               \
  }                                                               \
  EXPAND_FIELDS(_M2/*__DECLARE_FIELD*/)                           \
  enum { encode_size = 0                                          \
     EXPAND_FIELDS(_M3/*__FIELD_ENCODE_SIZE*/)                    \
  };                                                              \
  static const FieldInfo kFieldsInfos[];                          \
}; /*class end*/                                                  \
typedef _NAME DataType;                                           \
}  /*namespace end*/                                              \
using __NS_##_NAME::_NAME;

#define DEF_DATA(_NAME)      \
  DECLARE_DATA_CLASS(_NAME); \
  DEFINE_DATA_CLASS(_NAME);

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
  _(5,  d, boost::array<char, 3>)

DEF_DATA(DataWithNested);

/*----------------------------------------------------------------------------*/
//namespace _DataX {
//  struct DataX : Serializable {
//    DataX() {
//      fields_infos_ = &kFieldsInfos[0];      
//      a = int();
//      b = int();
//    }
//    
//    int a; enum {__tag_a = 1};
//    int b; enum {__tag_b = 2};
//    enum {__field_count = 2};
//    uint8_t fields_presence_[__field_count / 8 + 1];
//    enum { encode_size = 0 + sizeof(tag_t) + sizeof(len_t) + EncodeSizeGetter<int>::encode_size
//                           + sizeof(tag_t) + sizeof(len_t) + EncodeSizeGetter<int>::encode_size };
//    static const FieldInfo kFieldsInfos[];
//  };
//  
//  typedef DataX DataType;
//  
//  const FieldInfo DataX::kFieldsInfos[] = {
//      { EncodeSizeGetter<int>::encode_size, offsetof(DataType, a), __tag_a, &Encoder<int>::encode, &Decoder<int>::decode }
//    , { EncodeSizeGetter<int>::encode_size, offsetof(DataType, b), __tag_b, &Encoder<int>::encode, &Decoder<int>::decode }
//    , { 0, kTagInvalid }
//  };
//}
//using _DataX::DataX;

/*----------------------------------------------------------------------------*/
//  struct DataWithNested : Serializable {
//    DataWithNested() {
//      fields_infos_ = &kFieldsInfos[0];      
//      a = int();
//      x = DataX();
//      b = int();    
//    }
//  
//    int   a; enum {__tag_a = 1};
//    DataX x; enum {__tag_x = 2};
//    int   b; enum {__tag_b = 3};
//    enum { encode_size = 0 + ENCODE_SIZE_TLV(int) + ENCODE_SIZE_TLV(DataX) + ENCODE_SIZE_TLV(int) };
//  
//    static const FieldInfo kFieldsInfos[];
//  };
//  
//  const FieldInfo DataWithNested::kFieldsInfos[] = {
//   { EncodeSizeGetter<int>::encode_size   , offsetof(DataWithNested, a), __tag_a, &Encoder<int>::encode   , &Decoder<int>::decode}
//  ,{ EncodeSizeGetter<DataX>::encode_size , offsetof(DataWithNested, x), __tag_x, &Encoder<DataX>::encode , &Decoder<DataX>::decode}
//  ,{ EncodeSizeGetter<int>::encode_size   , offsetof(DataWithNested, b), __tag_b, &Encoder<int>::encode   , &Decoder<int>::decode}
//  ,{ 0, 0, kTagInvalid, NULL }
//  };


/*----------------------------------------------------------------------------*/
    unsigned char g_buf[32 * 1024];

    template<typename T, size_t size, size_t n>
    ::testing::AssertionResult ArraysMatch(const T (&expected)[size],
                                           const T (&actual)[n]){
        for (size_t i(0); i < size; ++i){
            if (expected[i] != actual[i]){
                return ::testing::AssertionFailure() << "array[" << i
                    << "] (" << actual[i] << ") != expected[" << i
                    << "] (" << expected[i] << ")";
            }
        }
        return ::testing::AssertionSuccess();
    }

    #define ENCODE_SIZE_T (sizeof(tag_t))
    #define ENCODE_SIZE_TL (ENCODE_SIZE_T + sizeof(len_t))
    #define ENCODE_SIZE_TLV(...) (ENCODE_SIZE_TL + EncodeSizeGetter<__VA_ARGS__>::encode_size)

/*----------------------------------------------------------------------------*/
TEST(SingleFieldData, size_of_struct__should_be_total_of__TLVs) {
  EXPECT_EQ(ENCODE_SIZE_TLV(int)/*a*/, SingleFieldData::encode_size);
}

TEST(SingleStringData, should_able_to_encode__string_field__in_TLV) {
  SingleStringData ssd;
  ssd.a = "abcdef";
  //TOOD:
  //EXPECT_EQ(ENCODE_SIZE_TLV(int)/*a*/, SingleFieldData::encode_size);
}

TEST(xxx, xxx) {
   string a = "abc";

   EXPECT_EQ(ENCODE_SIZE_TL + a.size(), EncodeSizeGetter<string>::size(a));
}

TEST(DataX, size_of_struct__should_be_total_of__TLVs) {
  EXPECT_EQ(ENCODE_SIZE_TLV(int)/*a*/+ ENCODE_SIZE_TLV(int)/*b*/, DataX::encode_size);
}

TEST(DataWithNested, size_of_struct_with_nested_struct) {
  size_t expected = ENCODE_SIZE_TLV(int) /*a*/
                  + ENCODE_SIZE_TL /*TL of nested X*/ + DataX::encode_size /*encoded X*/
                  + ENCODE_SIZE_TLV(int)  /*b*/
                  + ENCODE_SIZE_TLV(char) /*c*/
                  + ENCODE_SIZE_TLV(boost::array<char, 3>)  /*d*/;
  EXPECT_EQ(expected, DataWithNested::encode_size);
}

/*----------------------------------------------------------------------------*/
TEST(SingleFieldData, should_able_to_encode_SingleFieldData) {
  SingleFieldData x;
  x.a = 0x12345678;
  __encode(x, g_buf);

  unsigned char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12};

  EXPECT_TRUE(ArraysMatch(expected, g_buf));
}

TEST(DataX, should_able_to_encode_normal_struct) {
  DataX x;
  x.a = 0x12345678;
  x.b = 0x11223344;
  __encode(x, g_buf);

  unsigned char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11};
  EXPECT_TRUE(ArraysMatch(expected, g_buf));
}

TEST(DataWithNested, should_able_to_encode_struct_with_nested_struct) {
  DataWithNested xn;
  xn.a = 0xCAFEBABE;
  xn.x.a = 0x12345678;
  xn.x.b = 0x11223344;
  xn.b = 0xDEADBEEF;
  xn.c = 0x45;
  memcpy(xn.d.c_array(), "XYZ", strlen("XYZ"));

  __encode(xn, g_buf);

  unsigned char expected[] = { 0x01, 0x04, 0x00, 0xBE, 0xBA, 0xFE, 0xCA
                             , 0x02, 0x0E, 0x00 /*T and L of nested X*/
                             , 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11
                             , 0x03, 0x04, 0x00, 0xEF, 0xBE, 0xAD, 0xDE
                             , 0x04, 0x01, 0x00, 0x45
                             , 0x05, 0x03, 0x00, 'X', 'Y', 'Z' };

  EXPECT_TRUE(ArraysMatch(expected, g_buf));
}

/*----------------------------------------------------------------------------*/
TEST(SingleFieldData, should_able_to_decode_SingleFieldData) {
  SingleFieldData x;
  unsigned char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12};

  __decode(x, expected, sizeof(expected));

  EXPECT_EQ(0x12345678, x.a);
}

TEST(DataX, should_able_to_decode_normal_struct) {
  DataX x;
  unsigned char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11};

  __decode(x, expected, sizeof(expected));

  EXPECT_EQ(0x12345678, x.a);
  EXPECT_EQ(0x11223344, x.b);
}

TEST(DataWithNested, should_able_to_decode_struct_with_nested_struct) {
  DataWithNested xn;

  unsigned char expected[] = { 0x01, 0x04, 0x00, 0xBE, 0xBA, 0xFE, 0xCA
                             , 0x02, 0x0E, 0x00 /*T and L of nested X*/
                             , 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11
                             , 0x03, 0x04, 0x00, 0xEF, 0xBE, 0xAD, 0xDE
                             , 0x04, 0x01, 0x00, 0x45
                             , 0x05, 0x03, 0x00, 'X', 'Y', 'Z'};

  __decode(xn, expected, sizeof(expected));
  
  EXPECT_EQ(0xCAFEBABE, xn.a);
  EXPECT_EQ(0x12345678, xn.x.a);
  EXPECT_EQ(0x11223344, xn.x.b);
  EXPECT_EQ(0xDEADBEEF, xn.b);
  EXPECT_EQ(0x45,       xn.c);
  EXPECT_EQ('X',       xn.d[0]);
  EXPECT_EQ('Y',       xn.d[1]);
  EXPECT_EQ('Z',       xn.d[2]);
}

/*----------------------------------------------------------------------------*/
TEST(DataX, should_ignore_unknown_tag__WHEN___decode_normal_struct) {
  DataX x;
  unsigned char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x03, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12 /*unexpected tag 0x03*/
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11};

  __decode(x, expected, sizeof(expected));
  
  EXPECT_EQ(0x12345678, x.a);
  EXPECT_EQ(0x11223344, x.b);
}

TEST(DataWithNested, should_ignore_unknown_tag__WHEN__decode_struct_with_nested_struct) {
  DataWithNested xn;

  unsigned char expected[] = { 0x01, 0x04, 0x00, 0xBE, 0xBA, 0xFE, 0xCA
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
- support string
- support varible length data.  data[0]
------------------------------------------------------------------------------*/

