#ifndef __RSA_H__
#define __RSA_H__

#include <stdint.h>

// This is the header file for the library librsaencrypt.a

// Change this line to the file you'd like to use as a source of primes.
// The format of the file should be one prime per line.



struct public_key_class{
  long long modulus;
  long long exponent;
};

struct private_key_class{
  long long modulus;
  long long exponent;
};

// Hàm này tạo khóa công khai và khóa riêng tư, sau đó lưu chúng vào các cấu trúc được chỉ định bằng con trỏ.
// Đối số thứ ba nên là văn bản PRIME_SOURCE_FILE 

void rsa_gen_keys(struct public_key_class *pub, struct private_key_class *priv, const char *PRIME_SOURCE_FILE);

// Hàm này sẽ mã hóa dữ liệu mà con trỏ message trỏ tới. Nó trả về một con trỏ đến một mảng trên heap
// chứa dữ liệu đã mã hóa, hoặc NULL nếu có lỗi. Con trỏ này cần được giải phóng khi bạn đã hoàn thành.
// Dữ liệu đã mã hóa sẽ có kích thước lớn gấp 8 lần so với dữ liệu ban đầu.

long long *rsa_encrypt(const char *message, const unsigned long message_size, const struct public_key_class *pub);

// Hàm này sẽ giải mã dữ liệu mà con trỏ message trỏ tới. Nó trả về một con trỏ đến một mảng trên heap
// chứa dữ liệu đã giải mã, hoặc NULL nếu có lỗi. Con trỏ này cần được giải phóng khi bạn đã hoàn thành.
// Biến message_size là kích thước trong byte của tin nhắn đã mã hóa. Dữ liệu đã giải mã sẽ có kích thước
// bằng 1/8 so với dữ liệu đã mã hóa.

char *rsa_decrypt(const long long *message, const unsigned long message_size, const struct private_key_class *pub);

#endif
