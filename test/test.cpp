#include <stdint.h>
#include <cstring>
#include <iostream>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits.hpp>
USING_MOCKCPP_NS;
using namespace std;

/*------------------------------------------------------------------------------
- test `char arr[10]' field.
- test `const int *' field.
- too much duplicaate code
- enum {__field_count = 2};
  uint8_t fields_presence_[__field_count / 8 + 1];
------------------------------------------------------------------------------*/
template<typename T, class Enable = void>
struct EncodeSizeGetter {
  enum {encode_size = sizeof(T)};
};

template<typename T>
struct EncodeSizeGetter<T, typename boost::enable_if_c<T::encode_size>::type> {
  enum {encode_size = T::encode_size};
};

/*----------------------------------------------------------------------------*/
typedef uint16_t len_t;
typedef uint8_t  tag_t;
static const tag_t kTagInvalid = 0;

#define ENCODE_SIZE_T (sizeof(tag_t))
#define ENCODE_SIZE_TL (ENCODE_SIZE_T + sizeof(len_t))
#define ENCODE_SIZE_TLV(type) (ENCODE_SIZE_TL + EncodeSizeGetter<type>::encode_size)

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
struct Encoder<T, typename boost::enable_if_c<T::encode_size>::type> {
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
struct Decoder<T, typename boost::enable_if_c<T::encode_size>::type> {
  static void decode(void* instance, size_t field_offset, void*& p, size_t len) {
    Serializable& nested = *(Serializable*)( ((uint8_t*)instance) + field_offset );
    p = __decode(nested, p, len);
  }
};

// struct DataX : Serializable {
//   DataX() {
//     fields_infos_ = &kFieldsInfos[0];      
//     a = int();
//     b = int();
//   }
//   
//   int a; enum {__tag_a = 1};  
//   int b; enum {__tag_b = 2};
//   enum {__field_count = 2};
//   uint8_t fields_presence_[__field_count / 8 + 1];
//   enum { encode_size = ENCODE_SIZE_TLV(int) + ENCODE_SIZE_TLV(int) };
//   
//   static const FieldInfo kFieldsInfos[];
// };
// 
// const FieldInfo DataX::kFieldsInfos[] = {
//     { EncodeSizeGetter<int>::encode_size, offsetof(DataX, a), __tag_a, &Encoder<int>::encode, &Decoder<int>::decode }
//   , { EncodeSizeGetter<int>::encode_size, offsetof(DataX, b), __tag_b, &Encoder<int>::encode, &Decoder<int>::decode }
//   , { 0, kTagInvalid }
// };

#define DEF_DATA(_NAME)                  \
struct _NAME : Serializable {            \
  _NAME() {                              \
    fields_infos_ = &kFieldsInfos[0]; 

#define EXPAND_FIELDS(_, _NAME) \
  _(int, a, 1, _NAME)                  \
  _(int, b, 2, _NAME)

#define __INIT_FIELD_IN_CONSTRUCTOR(_TYPE, _FIELD_NAME, _TAG, _NAME) \
  _FIELD_NAME = _TYPE(); 

#define __DECLARE_FIELD(_TYPE, _FIELD_NAME, _TAG, _NAME) \
  _TYPE _FIELD_NAME; \
  enum {__tag_##_FIELD_NAME = _TAG};

#define __FIELD_ENCODE_SIZE(_TYPE, _FIELD_NAME, _TAG, _NAME) \
  + sizeof(tag_t) + sizeof(len_t) + EncodeSizeGetter<_TYPE>::encode_size

#define __DATAEX_CLASS_END               \
  static const FieldInfo kFieldsInfos[]; \
  }; /*class end*/

#define __DEFINE_FIELD_INFO(_TYPE, _FIELD_NAME, _TAG, _NAME) \
    { EncodeSizeGetter<_TYPE>::encode_size, offsetof(_NAME, _FIELD_NAME), _TAG, &Encoder<_TYPE>::encode, &Decoder<_TYPE>::decode },

//struct DataX : Serializable {
//  DataX() {
//    fields_infos_ = &kFieldsInfos[0];
DEF_DATA(DataX)
    //  a = int();
    //  b = int();
  EXPAND_FIELDS(__INIT_FIELD_IN_CONSTRUCTOR, DataX)
  }
  
  //int a; enum {__tag_a = 1};  
  //int b; enum {__tag_b = 2};
  EXPAND_FIELDS(__DECLARE_FIELD, DataX)

  enum { encode_size = 0
           //+ sizeof(tag_t) + sizeof(len_t) + EncodeSizeGetter<int>::encode_size
           //+ sizeof(tag_t) + sizeof(len_t) + EncodeSizeGetter<int>::encode_size
           EXPAND_FIELDS(__FIELD_ENCODE_SIZE, DataX)
  };

__DATAEX_CLASS_END
const FieldInfo DataX::kFieldsInfos[] = {
    //{ EncodeSizeGetter<int>::encode_size, offsetof(DataX, a), __tag_a, &Encoder<int>::encode, &Decoder<int>::decode },
    //{ EncodeSizeGetter<int>::encode_size, offsetof(DataX, b), __tag_b, &Encoder<int>::encode, &Decoder<int>::decode },
    EXPAND_FIELDS(__DEFINE_FIELD_INFO, DataX)
    { 0, kTagInvalid }
};


// //struct DataX : Serializable {
// //  DataX() {
// //    fields_infos_ = &kFieldsInfos[0];
// DEF_DATA(DataX)
//     //  a = int();
//     //  b = int();
//   EXPAND_FIELDS(__INIT_FIELD_IN_CONSTRUCTOR, DataX)
//   }
//   
//   //int a; enum {__tag_a = 1};  
//   //int b; enum {__tag_b = 2};
//   EXPAND_FIELDS(__DECLARE_FIELD, DataX)
// 
//   enum { encode_size = 0
//            //+ sizeof(tag_t) + sizeof(len_t) + EncodeSizeGetter<int>::encode_size
//            //+ sizeof(tag_t) + sizeof(len_t) + EncodeSizeGetter<int>::encode_size
//            EXPAND_FIELDS(__FIELD_ENCODE_SIZE, DataX)
//   };
// 
// __DATAEX_CLASS_END
// const FieldInfo DataX::kFieldsInfos[] = {
//     //{ EncodeSizeGetter<int>::encode_size, offsetof(DataX, a), __tag_a, &Encoder<int>::encode, &Decoder<int>::decode },
//     //{ EncodeSizeGetter<int>::encode_size, offsetof(DataX, b), __tag_b, &Encoder<int>::encode, &Decoder<int>::decode },
//     EXPAND_FIELDS(__DEFINE_FIELD_INFO, DataX)
//     { 0, kTagInvalid }
// };
// 

struct DataXN : Serializable {
  DataXN() {
    fields_infos_ = &kFieldsInfos[0];      
    a = int();
    x = DataX();
    b = int();    
  }

  int   a; enum {__tag_a = 1};
  DataX x; enum {__tag_x = 2};
  int   b; enum {__tag_b = 3};
  enum { encode_size = 0 + ENCODE_SIZE_TLV(int) + ENCODE_SIZE_TLV(DataX) + ENCODE_SIZE_TLV(int) };

  static const FieldInfo kFieldsInfos[];
};

const FieldInfo DataXN::kFieldsInfos[] = {
 { EncodeSizeGetter<int>::encode_size   , offsetof(DataXN, a), __tag_a, &Encoder<int>::encode   , &Decoder<int>::decode}
,{ EncodeSizeGetter<DataX>::encode_size , offsetof(DataXN, x), __tag_x, &Encoder<DataX>::encode , &Decoder<DataX>::decode}
,{ EncodeSizeGetter<int>::encode_size   , offsetof(DataXN, b), __tag_b, &Encoder<int>::encode   , &Decoder<int>::decode}
,{ 0, 0, kTagInvalid, NULL }
};

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
/*----------------------------------------------------------------------------*/

TEST(DataX, size_of_struct__should_be_total_of__TLVs) {
  EXPECT_EQ(ENCODE_SIZE_TLV(int)/*a*/+ ENCODE_SIZE_TLV(int)/*b*/, DataX::encode_size);
}

TEST(DataXN, size_of_struct_with_nested_struct) {
  size_t expected = ENCODE_SIZE_TLV(int) /*a*/
                  + ENCODE_SIZE_TL /*TL of nested X*/ + DataX::encode_size /*encoded X*/
                  + ENCODE_SIZE_TLV(int) /*b*/;
  EXPECT_EQ(expected, DataXN::encode_size);
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

TEST(DataXN, should_able_to_encode_struct_with_nested_struct) {
  DataXN xn;
  xn.a = 0xCAFEBABE;
  xn.x.a = 0x12345678;
  xn.x.b = 0x11223344;
  xn.b = 0xDEADBEEF;

  __encode(xn, g_buf);

  unsigned char expected[] = { 0x01, 0x04, 0x00, 0xBE, 0xBA, 0xFE, 0xCA
                             , 0x02, 0x0E, 0x00 /*T and L of nested X*/
                             , 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11
                             , 0x03, 0x04, 0x00, 0xEF, 0xBE, 0xAD, 0xDE };

  EXPECT_TRUE(ArraysMatch(expected, g_buf));
}

TEST(DataX, should_able_to_decode_normal_struct) {
  DataX x;
  unsigned char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11};

  __decode(x, expected, sizeof(expected));

  EXPECT_EQ(0x12345678, x.a);
  EXPECT_EQ(0x11223344, x.b);
}

TEST(DataXN, should_able_to_decode_struct_with_nested_struct) {
  DataXN xn;

  unsigned char expected[] = { 0x01, 0x04, 0x00, 0xBE, 0xBA, 0xFE, 0xCA
                             , 0x02, 0x0E, 0x00 /*T and L of nested X */
                             , 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11
                             , 0x03, 0x04, 0x00, 0xEF, 0xBE, 0xAD, 0xDE };

  __decode(xn, expected, sizeof(expected));
  
  EXPECT_EQ(0xCAFEBABE, xn.a);
  EXPECT_EQ(0x12345678, xn.x.a);
  EXPECT_EQ(0x11223344, xn.x.b);
  EXPECT_EQ(0xDEADBEEF, xn.b);
}



TEST(DataX, should_ignore_unknown_tag__WHEN___decode_normal_struct) {
  DataX x;
  unsigned char expected[] = { 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x03, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12 /*unexpected tag 0x03*/
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11};

  __decode(x, expected, sizeof(expected));
  
  EXPECT_EQ(0x12345678, x.a);
  EXPECT_EQ(0x11223344, x.b);
}

TEST(DataXN, should_ignore_unknown_tag__WHEN__decode_struct_with_nested_struct) {
  DataXN xn;

  unsigned char expected[] = { 0x01, 0x04, 0x00, 0xBE, 0xBA, 0xFE, 0xCA
                             , 0x09, 0x03, 0x00, 0x01, 0x02, 0x03       /*unknown tag 0x09*/
                             , 0x02, 0x0E, 0x00 /*T and L of nested X */
                             , 0x01, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12
                             , 0x11, 0x01, 0x00, 0x01                   /*unknown tag 0x011*/
                             , 0x02, 0x04, 0x00, 0x44, 0x33, 0x22, 0x11
                             , 0x12, 0x05, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 /*unknown tag*/
                             , 0x03, 0x04, 0x00, 0xEF, 0xBE, 0xAD, 0xDE };

  __decode(xn, expected, sizeof(expected));
  
  EXPECT_EQ(0xCAFEBABE, xn.a);
  EXPECT_EQ(0x12345678, xn.x.a);
  EXPECT_EQ(0x11223344, xn.x.b);
  EXPECT_EQ(0xDEADBEEF, xn.b);
}

